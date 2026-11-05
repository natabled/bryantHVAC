#pragma once

#include "esphome/core/component.h"
#include "esphome/components/bryant_framer/bryant_frame.h"
#include "esphome/components/uart/uart.h"
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
  std::vector<FrameListener *> listeners_;

  bool header_at_(size_t offset) const;
  bool find_header_(size_t *offset) const;
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
