#include "bryant_writer.h"
#include <inttypes.h>
#include <esp_system.h>

namespace bryant_writer {
static const char *const TAG = "bryant_writer";

static uint8_t reverse8(uint8_t x) {
  x = static_cast<uint8_t>((x >> 4) | (x << 4));
  x = static_cast<uint8_t>(((x & 0xCC) >> 2) | ((x & 0x33) << 2));
  x = static_cast<uint8_t>(((x & 0xAA) >> 1) | ((x & 0x55) << 1));
  return x;
}
uint16_t BryantWriter::crc16_(const uint8_t *bytes, size_t len) {
  uint16_t crc = 0;
  for (size_t i = 0; i < len; ++i) {
    crc ^= static_cast<uint16_t>(reverse8(bytes[i])) << 8;
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x8005) : static_cast<uint16_t>(crc << 1);
    }
  }
  return static_cast<uint16_t>((static_cast<uint16_t>(reverse8(crc & 0xFF)) << 8) | reverse8(crc >> 8));
}
bool BryantWriter::parse_hex_(const std::string &hex, std::vector<uint8_t> *out) {
  out->clear();
  int high = -1;
  for (const char c : hex) {
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ':' || c == '-' || c == ',') continue;
    int value = -1;
    if (c >= '0' && c <= '9') value = c - '0';
    else if (c >= 'a' && c <= 'f') value = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') value = c - 'A' + 10;
    if (value < 0) return false;
    if (high == -1) high = value;
    else {
      out->push_back(static_cast<uint8_t>((high << 4) | value));
      high = -1;
    }
    if (out->size() > 265) return false;
  }
  return high == -1 && !out->empty();
}
bool BryantWriter::validate_(const std::vector<uint8_t> &frame) {
  if (frame.size() < 10 || frame.size() > 265) return false;
  if (frame.size() != static_cast<size_t>(frame[4]) + 10U) return false;
  const size_t n = frame.size();
  const uint16_t received = static_cast<uint16_t>(frame[n-2]) | (static_cast<uint16_t>(frame[n-1]) << 8);
  return crc16_(frame.data(), n-2) == received;
}
void BryantWriter::add_poll(const std::string &name, const std::string &hex, uint32_t offset_ms) {
  Poll p;
  p.name = name;
  p.offset_ms = offset_ms;
  p.valid = parse_hex_(hex, &p.frame) && validate_(p.frame);
  polls_.push_back(std::move(p));
}
void BryantWriter::setup() {
  const uint32_t now = millis();
  for (auto &p : polls_) {
    if (!p.valid) {
      ESP_LOGE(TAG, "Poll '%s' has invalid length or CRC; disabled", p.name.c_str());
      continue;
    }
    p.next_ms = now + 10000U + p.offset_ms;
  }
  ESP_LOGI(TAG, "Writer ready: polls=%u interval=%" PRIu32 "ms idle=%" PRIu32 "ms (best effort, no arbitration)",
           static_cast<unsigned>(polls_.size()), poll_interval_ms_, idle_ms_);
}
bool BryantWriter::queue_(const std::string &name, const std::vector<uint8_t> &frame, bool manual) {
  if (!validate_(frame)) {
    ESP_LOGW(TAG, "Rejected '%s': invalid frame length or CRC", name.c_str());
    return false;
  }
  for (const auto &item : pending_frames_) {
    if (!manual && item.name == name) return true;  // coalesce stale poll requests
  }
  if (pending_frames_.size() >= MAX_QUEUE) {
    ESP_LOGW(TAG, "TX queue full; dropped '%s'", name.c_str());
    return false;
  }
  pending_frames_.push_back({name, frame, manual});
  if (log_tx_) {
    ESP_LOGD(TAG, "Queued '%s' (%u bytes)", name.c_str(), static_cast<unsigned>(frame.size()));
  }
  return true;
}
bool BryantWriter::enqueue_body(const std::string &name, const std::vector<uint8_t> &body) {
  if (body.size() < 8 || body.size() > 263 || body.size() != static_cast<size_t>(body[4]) + 8U) {
    ESP_LOGW(TAG, "Rejected '%s': invalid frame body length", name.c_str());
    return false;
  }
  std::vector<uint8_t> full = body;
  const uint16_t crc = crc16_(full.data(), full.size());
  full.push_back(static_cast<uint8_t>(crc & 0xFF));
  full.push_back(static_cast<uint8_t>(crc >> 8));
  return queue_(name, full, true);
}
bool BryantWriter::enqueue_named_hex(const std::string &name, const std::string &hex) {
  std::vector<uint8_t> frame;
  if (!parse_hex_(hex, &frame)) return false;
  return queue_(name, frame, true);
}
bool BryantWriter::cancel(const std::string &name) {
  bool removed = false;
  for (auto it = pending_frames_.begin(); it != pending_frames_.end();) {
    if (it->name == name) { it = pending_frames_.erase(it); removed = true; }
    else ++it;
  }
  return removed;
}
bool BryantWriter::enqueue_hex(const std::string &hex) {
  std::vector<uint8_t> frame;
  if (!parse_hex_(hex, &frame)) {
    ESP_LOGW(TAG, "Manual TX rejected: invalid hex");
    return false;
  }
  return queue_("manual", frame, true);
}
void BryantWriter::schedule_polls_(uint32_t now) {
  for (auto &p : polls_) {
    if (!p.valid) continue;
    if (static_cast<int32_t>(now - p.next_ms) >= 0) {
      queue_(p.name, p.frame, false);
      // Skip missed intervals rather than burst stale polls.
      p.next_ms = now + poll_interval_ms_;
    }
  }
}
void BryantWriter::loop() {
  const uint32_t now = millis();
  schedule_polls_(now);
  if (pending_frames_.empty() || framer_ == nullptr) return;
  // Keep the UART framer as the sole RX consumer. If any bytes are buffered or
  // any frame is partially assembled, do not transmit.
  if (framer_->rx_pending() || this->available() > 0) {
    quiet_started_ms_ = 0;
    return;
  }
  const uint32_t last_activity = framer_->last_rx_ms() > last_tx_ms_ ? framer_->last_rx_ms() : last_tx_ms_;
  if (static_cast<uint32_t>(now - last_activity) < idle_ms_) {
    quiet_started_ms_ = 0;
    return;
  }
  if (quiet_started_ms_ == 0) {
    quiet_started_ms_ = now;
    tx_backoff_ms_ = 20U + (esp_random() % 101U);
    return;
  }
  if (static_cast<uint32_t>(now - quiet_started_ms_) < tx_backoff_ms_) return;
  // Recheck just before TX. UART idle detection is NOT collision prevention.
  if (framer_->rx_pending() || this->available() > 0) {
    quiet_started_ms_ = 0;
    return;
  }
  const auto &next = pending_frames_.front();
  this->write_array(next.frame.data(), next.frame.size());
  this->flush();
  last_tx_ms_ = millis();
  if (log_tx_) {
    ESP_LOGI(TAG, "TX '%s' %u bytes (no bus arbitration)", next.name.c_str(),
             static_cast<unsigned>(next.frame.size()));
    // Full frame, including CRC, for byte-for-byte comparison with RX logs.
    // Use bounded chunks to avoid ESP logger line-length truncation.
    for (size_t offset = 0; offset < next.frame.size(); offset += 32) {
      char hex[32 * 3 + 1];
      size_t pos = 0;
      const size_t end = (offset + 32 < next.frame.size() ? offset + 32 : next.frame.size());
      for (size_t i = offset; i < end; ++i) {
        const int written = snprintf(hex + pos, sizeof(hex) - pos,
                                     i == offset ? "%02X" : " %02X", next.frame[i]);
        if (written <= 0) break;
        pos += static_cast<size_t>(written);
      }
      ESP_LOGD(TAG, "TX '%s' offset=%u: %s", next.name.c_str(),
               static_cast<unsigned>(offset), hex);
    }
  }
  const std::string sent_name = next.name;
  pending_frames_.pop_front();
  quiet_started_ms_ = 0;
  if (tx_listener_ != nullptr) tx_listener_->on_tx_sent(sent_name, last_tx_ms_);
}
}  // namespace bryant_writer
