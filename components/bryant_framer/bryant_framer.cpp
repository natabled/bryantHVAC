#include "bryant_framer.h"
#include <algorithm>
#include <cstdio>
#include <inttypes.h>

namespace bryant_framer {
static const char *const TAG = "bryant_framer";

void BryantFramer::setup() {
  if (ring_capacity_ > RING_STORAGE_SIZE) ring_capacity_ = RING_STORAGE_SIZE;
  if (ring_capacity_ < 256) ring_capacity_ = 256;
  if (drain_interval_ms_ == 0) drain_interval_ms_ = 7;
  ESP_LOGI(TAG, "Framer ready: drain=%" PRIu32 "ms ring=%u max_payload=%u", drain_interval_ms_,
           static_cast<unsigned>(ring_capacity_), max_payload_length_);
}

void BryantFramer::loop() {
  const uint32_t now = millis();
  if (static_cast<uint32_t>(now - last_drain_ms_) >= drain_interval_ms_) {
    last_drain_ms_ = now;
    drain_uart_();
    process_frames_();
  }
  if (diag_dirty_ || static_cast<uint32_t>(now - last_diag_ms_) >= 10000) publish_diagnostics_();
}

uint8_t BryantFramer::peek_(size_t offset) const {
  return ring_[(head_ + offset) % ring_capacity_];
}

void BryantFramer::pop_(size_t n) {
  if (n > count_) n = count_;
  head_ = (head_ + n) % ring_capacity_;
  count_ -= n;
}

void BryantFramer::push_(uint8_t byte) {
  if (count_ >= ring_capacity_) {
    // Retain newest stream bytes; overflow is counted and logged explicitly.
    pop_(1);
    lose_sync_("RX overflow");
    ++overflows_;
    ++discarded_bytes_;
    if (log_frame_errors_) ESP_LOGE(TAG, "RX ring overflow #%" PRIu32 "; discarded oldest byte", overflows_);
  }
  ring_[(head_ + count_) % ring_capacity_] = byte;
  ++count_;
}

void BryantFramer::drain_uart_() {
  uint8_t byte;
  size_t drained = 0;
  while (available() > 0) {
    if (!read_byte(&byte)) break;
    last_rx_ms_ = millis();
    push_(byte);
    ++drained;
  }
  if (drained && log_frame_errors_) {
    ESP_LOGV(TAG, "UART drain: %u byte(s), ring occupancy=%u",
             static_cast<unsigned>(drained), static_cast<unsigned>(count_));
  }
}

bool BryantFramer::header_at_(size_t offset) const {
  if (offset + 3 >= count_) return false;
  // Address values are intentionally unrestricted. Only the bus-address
  // structure is checked; F1/F1 is the documented broadcast exception.
  const uint8_t a = peek_(offset), b = peek_(offset + 1);
  const uint8_t c = peek_(offset + 2), d = peek_(offset + 3);
  return (b == 0x01 && d == 0x01) ||
         (a == 0xF1 && b == 0xF1 && d == 0x01);
}

void BryantFramer::lose_sync_(const char *reason) {
  if (synced_) {
    synced_ = false;
    ++sync_restarts_;
    diag_dirty_ = true;
    if (log_frame_errors_)
      ESP_LOGW(TAG, "Lost frame boundary: %s; restarts=%" PRIu32, reason, sync_restarts_);
  }
  candidate_start_ms_ = 0;
}

void BryantFramer::publish_diagnostics_() {
  if (sync_restarts_sensor_ != nullptr) sync_restarts_sensor_->publish_state(sync_restarts_);
  if (sync_acquisitions_sensor_ != nullptr) sync_acquisitions_sensor_->publish_state(sync_acquisitions_);
  if (sync_state_sensor_ != nullptr) sync_state_sensor_->publish_state(synced_ ? 1 : 0);
  last_diag_ms_ = millis();
  diag_dirty_ = false;
}

void BryantFramer::process_frames_() {
  // In SEARCH, slide one byte at a time until a structurally plausible,
  // CRC-valid complete frame is found. In SYNCED, consume each complete
  // frame at the known boundary, without an address whitelist or scan.
  while (count_ >= HEADER_SIZE) {
    if (!header_at_(0)) {
      lose_sync_("invalid header structure");
      ++discarded_bytes_;
      pop_(1);
      continue;
    }

    const uint8_t payload_length = peek_(4);
    if (payload_length > max_payload_length_) {
      lose_sync_("payload length out of bounds");
      ++discarded_bytes_;
      pop_(1);
      continue;
    }

    const size_t frame_size = HEADER_SIZE + payload_length + CRC_SIZE;
    if (count_ < frame_size) {
      // A partial candidate is normal across UART drains. If its declared
      // length never arrives, slide one byte and resume searching. This is
      // NOT an idle-bus sync timeout: no candidate means no timeout.
      const uint32_t now = millis();
      if (candidate_start_ms_ == 0) candidate_start_ms_ = now ? now : 1;
      if (static_cast<uint32_t>(now - candidate_start_ms_) < sync_timeout_ms_) return;
      lose_sync_("incomplete frame timeout");
      ++discarded_bytes_;
      pop_(1);
      continue;
    }
    candidate_start_ms_ = 0;

    std::array<uint8_t, MAX_FRAME_SIZE> frame{};
    for (size_t i = 0; i < frame_size; ++i) frame[i] = peek_(i);
    const uint16_t rx_crc = static_cast<uint16_t>(frame[frame_size - 2]) |
                            (static_cast<uint16_t>(frame[frame_size - 1]) << 8);
    const uint16_t calc_crc = crc16_carrier_(frame.data(), frame_size - CRC_SIZE);
    if (calc_crc != rx_crc) {
      ++crc_failures_;
      if (log_frame_errors_) {
        ESP_LOGW(TAG, "CRC FAIL #%" PRIu32 ": size=%u len=%u rx=%04X calc=%04X",
                 crc_failures_, static_cast<unsigned>(frame_size), payload_length, rx_crc, calc_crc);
        log_frame_(frame.data(), frame_size, 0, millis());
      }
      lose_sync_("CRC mismatch");
      ++discarded_bytes_;
      pop_(1);
      continue;
    }

    if (!synced_) {
      synced_ = true;
      ++sync_acquisitions_;
      diag_dirty_ = true;
      if (log_frame_errors_)
        ESP_LOGI(TAG, "Frame synchronization acquired (#%" PRIu32 ")", sync_acquisitions_);
    }
    const uint32_t fid = ++valid_frames_;
    const uint32_t rcv_ms = millis();
    if (log_valid_frames_ && should_log_frame_(frame.data(), frame_size))
      log_frame_(frame.data(), frame_size, fid, rcv_ms);
    if (!listeners_.empty()) {
      const FrameView view{fid, rcv_ms, frame[0], frame[1], frame[2], frame[3],
                           frame[4], frame[5], frame[6], frame[7],
                           frame.data() + HEADER_SIZE, frame_size};
      for (auto *listener : listeners_) listener->on_frame(view);
    }
    pop_(frame_size);
  }
  // No frame candidate: keep the sync state through arbitrarily long idle
  // gaps. Any trailing bytes remain buffered until the next UART drain.
}

uint8_t bitreverse8_(uint8_t x) {
  x = static_cast<uint8_t>((x & 0xF0) >> 4 | (x & 0x0F) << 4);
  x = static_cast<uint8_t>((x & 0xCC) >> 2 | (x & 0x33) << 2);
  x = static_cast<uint8_t>((x & 0xAA) >> 1 | (x & 0x55) << 1);
  return x;
}

uint16_t BryantFramer::crc16_carrier_(const uint8_t *data, size_t length) const {
  // Exact Carrier CRC variant from the proven rs485-hvac-v1-926.yaml:
  // init 0x0000, reflected input bits, polynomial 0x8005, final bit/byte reversal.
  uint16_t crc = 0x0000;
  for (size_t i = 0; i < length; ++i) {
    crc ^= static_cast<uint16_t>(bitreverse8_(data[i])) << 8;
    for (uint8_t bit = 0; bit < 8; ++bit)
      crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x8005) : static_cast<uint16_t>(crc << 1);
  }
  return static_cast<uint16_t>(bitreverse8_(crc & 0xFF) << 8 | bitreverse8_(crc >> 8));
}

bool BryantFramer::should_log_frame_(const uint8_t *frame, size_t size) const {
  if (size < HEADER_SIZE + 3 + CRC_SIZE) return true;
  const uint16_t reg = (static_cast<uint16_t>(frame[9]) << 8) | frame[10];
  for (const auto &rule : log_filters_) {
    if (frame[0] == rule.dst && frame[2] == rule.src && frame[7] == rule.function && reg == rule.reg)
      return rule.enabled;
  }
  return true;  // Unconfigured message types retain existing raw logging behavior.
}

void BryantFramer::log_frame_(const uint8_t *frame, size_t size, uint32_t fid, uint32_t rcv_ms) {
  // ESPHome/ESP-IDF log lines have a finite formatting buffer. Log in bounded chunks.
  ESP_LOGD(TAG, "FRAME fid=%" PRIu32 " rcv_ms=%" PRIu32 " dst=%02X/%02X src=%02X/%02X len=%u pid=%02X ext=%02X func=%02X size=%u",
           fid, rcv_ms, frame[0], frame[1], frame[2], frame[3], frame[4], frame[5], frame[6], frame[7],
           static_cast<unsigned>(size));
  for (size_t offset = 0; offset < size; offset += 32) {
    char hex[32 * 3 + 1];
    size_t pos = 0;
    const size_t end = (offset + 32 < size) ? offset + 32 : size;
    for (size_t i = offset; i < end; ++i)
      pos += static_cast<size_t>(snprintf(hex + pos, sizeof(hex) - pos, "%02X%s", frame[i], i + 1 == end ? "" : " "));
    ESP_LOGD(TAG, "FRAME fid=%" PRIu32 " offset=%u raw=[%s]", fid, static_cast<unsigned>(offset), hex);
  }
}

void BryantFramer::log_error_(const char *reason, size_t offset, size_t expected, size_t available) {
  ESP_LOGW(TAG, "FRAME ERROR: %s offset=%u expected=%u available=%u totals(valid=%" PRIu32 " crc_fail=%" PRIu32 " discarded=%" PRIu32 " overflow=%" PRIu32 ")",
           reason, static_cast<unsigned>(offset), static_cast<unsigned>(expected), static_cast<unsigned>(available),
           valid_frames_, crc_failures_, discarded_bytes_, overflows_);
}

}  // namespace bryant_framer
