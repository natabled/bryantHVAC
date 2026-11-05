#include "bryant_set.h"
#include <inttypes.h>

namespace bryant_set {
static const char *const TAG = "bryant_set";
static const char *const READ = "set_read_3b03";
static const char *const WRITE = "set_write_3b03";
static const char *const VERIFY = "set_verify_3b03";
static constexpr uint8_t REG_LEN = 150;
static constexpr uint8_t HEAT_OFFSET = 12;
static constexpr uint8_t CHANGE_HEAT = 0x04;

void BryantSet::setup() {
  if (framer_ != nullptr) framer_->add_listener(this);
  if (writer_ != nullptr) writer_->set_tx_listener(this);
  status_("Ready");
}
void BryantSet::status_(const char *message) {
  ESP_LOGI(TAG, "%s", message);
  if (status_sensor_ != nullptr) status_sensor_->publish_state(message);
}
void BryantSet::finish_(bool success, const char *reason) {
  if (writer_ != nullptr) {
    writer_->cancel(READ);
    writer_->cancel(WRITE);
    writer_->cancel(VERIFY);
  }
  if (success) ++confirmed_count_; else ++failed_count_;
  state_ = State::IDLE;
  status_(reason);
}
bool BryantSet::queue_read_(const std::string &name) {
  // Header: destination, bus, source, bus, length, pid, extension, function;
  // payload: 00 3B 03. CRC is appended by bryant_writer.
  return writer_ != nullptr && writer_->enqueue_body(name, {
      thermostat_, 0x01, sam_, 0x01, 0x03, 0x00, 0x00, 0x0B, 0x00, 0x3B, 0x03});
}
bool BryantSet::set_heat(uint8_t zone, uint8_t fahrenheit) {
  if (state_ != State::IDLE) { status_("Rejected: transaction busy"); return false; }
  if (zone < 1 || zone > 8 || fahrenheit < 45 || fahrenheit > 95) {
    status_("Rejected: invalid zone or temperature"); return false;
  }
  zone_ = zone;
  target_ = fahrenheit;
  started_ms_ = millis();
  if (!queue_read_(READ)) { status_("Failed: read queue rejected"); return false; }
  state_ = State::READ_QUEUED;
  status_("Requesting fresh 3B03");
  return true;
}
bool BryantSet::build_write_(const bryant_framer::FrameView &f) {
  // Payload includes register prefix (00 3B 03) followed by 150 register bytes.
  if (f.length != REG_LEN + 3 || f.payload == nullptr) return false;
  std::vector<uint8_t> body;
  body.reserve(8 + f.length);
  body.insert(body.end(), {thermostat_, 0x01, sam_, 0x01, f.length, 0x00, 0x00, 0x0B,
                           0x00, 0x3B, 0x03});
  body.insert(body.end(), f.payload + 3, f.payload + f.length);
  // Register data bytes 0..2 are zone index, reserved, change flags.
  body[11] = static_cast<uint8_t>(zone_ - 1);
  body[12] = 0x00;
  body[13] = CHANGE_HEAT;
  body[11 + HEAT_OFFSET + zone_ - 1] = target_;
  return writer_->enqueue_body(WRITE, body);
}
void BryantSet::on_tx_sent(const std::string &name, uint32_t ms) {
  if (name == READ && state_ == State::READ_QUEUED) {
    last_tx_ms_ = ms;
    state_ = State::WAIT_READ;
    status_("Waiting for fresh 3B03");
  } else if (name == WRITE && state_ == State::WRITE_QUEUED) {
    last_tx_ms_ = ms;
    ++sent_count_;
    state_ = State::WAIT_WRITE;
    verify_due_ms_ = ms + verify_delay_ms_;
    status_("Write sent; awaiting verification poll");
  } else if (name == VERIFY && state_ == State::VERIFY_QUEUED) {
    verify_poll_ms_ = ms;
    last_tx_ms_ = ms;
    state_ = State::WAIT_VERIFY;
    next_verify_poll_ms_ = ms + 2000;
    status_("Verifying 3B03 setpoint");
  }
}
void BryantSet::on_frame(const bryant_framer::FrameView &f) {
  if (f.dest != sam_ || f.src != thermostat_ || f.function != 0x06 ||
      f.length != REG_LEN + 3 || f.payload == nullptr ||
      f.payload[0] != 0x00 || f.payload[1] != 0x3B || f.payload[2] != 0x03) return;
  // The only authoritative responses are ones received after our own poll TX.
  if (state_ == State::WAIT_READ && static_cast<int32_t>(f.rcv_ms - last_tx_ms_) >= 0) {
    if (!build_write_(f)) { finish_(false, "Failed: write queue rejected"); return; }
    state_ = State::WRITE_QUEUED;
    status_("Fresh 3B03 received; write queued");
  } else if (state_ == State::WAIT_VERIFY &&
             static_cast<int32_t>(f.rcv_ms - verify_poll_ms_) >= 0) {
    const uint8_t actual = f.payload[3 + HEAT_OFFSET + zone_ - 1];
    if (actual == target_) {
      finish_(true, "Confirmed: thermostat reports requested heat setpoint");
    } else {
      ESP_LOGW(TAG, "Verification mismatch zone=%u requested=%u reported=%u",
               static_cast<unsigned>(zone_), static_cast<unsigned>(target_), static_cast<unsigned>(actual));
      // Re-read only, never retry the write.
      state_ = State::WAIT_WRITE;
      verify_due_ms_ = millis() + 2000;
      status_("Setpoint not yet confirmed; will poll again");
    }
  }
}
void BryantSet::loop() {
  if (state_ == State::IDLE) return;
  const uint32_t now = millis();
  if (static_cast<uint32_t>(now - started_ms_) >= timeout_ms_) {
    finish_(false, "FAILED: setpoint transaction timed out");
    return;
  }
  if (state_ == State::WAIT_WRITE && static_cast<int32_t>(now - verify_due_ms_) >= 0) {
    if (queue_read_(VERIFY)) {
      state_ = State::VERIFY_QUEUED;
      status_("Verification poll queued");
    }
  }
}
}  // namespace bryant_set
