#pragma once

#include "esphome/core/component.h"
#include "esphome/components/bryant_framer/bryant_frame.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace bryant_framer {

class BryantFramer : public esphome::Component, public esphome::uart::UARTDevice {
 public:
  void set_drain_interval_ms(uint32_t value) { drain_interval_ms_ = value; }
  void set_ring_buffer_size(size_t value) { ring_capacity_ = value; }
  void set_max_payload_length(uint16_t value) { max_payload_length_ = value; }
  void set_log_valid_frames(bool value) { log_valid_frames_ = value; }
  void set_log_frame_errors(bool value) { log_frame_errors_ = value; }
  void set_sync_timeout_ms(uint32_t value) { sync_timeout_ms_ = value; }
  void set_sync_restarts_sensor(esphome::sensor::Sensor *s) { sync_restarts_sensor_ = s; }
  void set_sync_acquisitions_sensor(esphome::sensor::Sensor *s) { sync_acquisitions_sensor_ = s; }
  void set_sync_state_sensor(esphome::sensor::Sensor *s) { sync_state_sensor_ = s; }

  // Per-message raw RX logging policy. First matching rule wins.
  void add_log_filter(uint8_t dst, uint8_t src, uint8_t function, uint16_t reg, bool enabled) {
    log_filters_.push_back({dst, src, function, reg, enabled});
  }
  void add_listener(FrameListener *listener) { if (listener != nullptr) listeners_.push_back(listener); }
  void set_listener(FrameListener *listener) { add_listener(listener); }
  uint32_t last_rx_ms() const { return last_rx_ms_; }
  bool rx_pending() { return count_ != 0 || this->available() > 0; }
  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return esphome::setup_priority::DATA; }

 protected:
  static constexpr size_t RING_STORAGE_SIZE = 2048;
  static constexpr size_t HEADER_SIZE = 8;
  static constexpr size_t CRC_SIZE = 2;
  static constexpr size_t MAX_FRAME_SIZE = HEADER_SIZE + 255 + CRC_SIZE;

  std::array<uint8_t, RING_STORAGE_SIZE> ring_{};
  size_t ring_capacity_{RING_STORAGE_SIZE};
  size_t head_{0};
  size_t count_{0};
  uint32_t drain_interval_ms_{7};
  uint32_t last_drain_ms_{0};
  uint32_t last_rx_ms_{0};
  uint16_t max_payload_length_{200};
  bool log_valid_frames_{true};
  bool log_frame_errors_{true};
  uint32_t valid_frames_{0};
  uint32_t crc_failures_{0};
  uint32_t discarded_bytes_{0};
  uint32_t overflows_{0};
  bool synced_{false};
  uint32_t sync_restarts_{0};
  uint32_t sync_acquisitions_{0};
  uint32_t sync_timeout_ms_{250};
  uint32_t candidate_start_ms_{0};
  uint32_t last_diag_ms_{0};
  bool diag_dirty_{true};
  esphome::sensor::Sensor *sync_restarts_sensor_{nullptr};
  esphome::sensor::Sensor *sync_acquisitions_sensor_{nullptr};
  esphome::sensor::Sensor *sync_state_sensor_{nullptr};
  void lose_sync_(const char *reason);
  void publish_diagnostics_();
  struct LogFilter { uint8_t dst, src, function; uint16_t reg; bool enabled; };
  std::vector<LogFilter> log_filters_;
  bool should_log_frame_(const uint8_t *frame, size_t size) const;
  std::vector<FrameListener *> listeners_;

  bool header_at_(size_t offset) const;
  uint8_t peek_(size_t offset) const;
  void pop_(size_t n);
  void push_(uint8_t byte);
  void drain_uart_();
  void process_frames_();
  uint16_t crc16_carrier_(const uint8_t *data, size_t length) const;
  void log_frame_(const uint8_t *frame, size_t size, uint32_t fid, uint32_t rcv_ms);
  void log_error_(const char *reason, size_t offset, size_t expected, size_t available);
};

}  // namespace bryant_framer
