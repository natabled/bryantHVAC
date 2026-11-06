#pragma once
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/bryant_framer/bryant_framer.h"
#include <array>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace bryant_writer {
class TxListener {
 public:
  virtual ~TxListener() = default;
  virtual void on_tx_sent(const std::string &name, uint32_t ms) = 0;
};
class BryantWriter : public esphome::Component, public esphome::uart::UARTDevice, public bryant_framer::FrameListener {
 public:
  void set_framer(bryant_framer::BryantFramer *framer) { framer_ = framer; }
  void set_idle_ms(uint32_t ms) { idle_ms_ = ms; }
  void set_poll_interval_ms(uint32_t ms) { poll_interval_ms_ = ms; }
  void set_log_tx(bool yes) { log_tx_ = yes; }
  void add_poll(const std::string &name, const std::string &hex, uint32_t offset_ms);
  void set_tx_listener(TxListener *listener) { tx_listener_ = listener; }
  bool enqueue_body(const std::string &name, const std::vector<uint8_t> &body);
  bool enqueue_named_hex(const std::string &name, const std::string &hex);
  bool cancel(const std::string &name);
  bool enqueue_hex(const std::string &hex);  // manual diagnostic, full frame including CRC
  void set_responder_enabled(bool v) { responder_enabled_ = v; }
  void set_responder_register(uint16_t v) { responder_register_ = v; }
  void set_responder_address(uint8_t v) { responder_address_ = v; }
  void set_responder_window_ms(uint32_t v) { responder_window_ms_ = v; }
  void arm_responder_window() { responder_armed_at_ = millis(); responder_armed_ = true; }
  void disarm_responder_window() { responder_armed_ = false; responder_pending_ = false; }
  void on_frame(const bryant_framer::FrameView &f) override;
  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return esphome::setup_priority::DATA; }
 protected:
  struct Poll {
    std::string name;
    std::vector<uint8_t> frame;
    uint32_t offset_ms;
    uint32_t next_ms{0};
    bool valid{false};
  };
  struct Pending {
    std::string name;
    std::vector<uint8_t> frame;
    bool manual{false};
  };
  static bool parse_hex_(const std::string &hex, std::vector<uint8_t> *out);
  static uint16_t crc16_(const uint8_t *bytes, size_t len);
  static bool validate_(const std::vector<uint8_t> &frame);
  bool queue_(const std::string &name, const std::vector<uint8_t> &frame, bool manual);
  void schedule_polls_(uint32_t now);
  bryant_framer::BryantFramer *framer_{nullptr};
  std::vector<Poll> polls_;
  std::deque<Pending> pending_frames_;
  uint32_t idle_ms_{50};
  uint32_t poll_interval_ms_{50000};
  uint32_t last_tx_ms_{0};
  uint32_t quiet_started_ms_{0};
  uint32_t tx_backoff_ms_{0};
  bool log_tx_{true};
  TxListener *tx_listener_{nullptr};
  bool responder_enabled_{false};
  bool responder_armed_{false};
  bool responder_pending_{false};
  uint8_t responder_address_{0x92};
  uint16_t responder_register_{0x3B0E};
  uint32_t responder_window_ms_{5000};
  uint32_t responder_armed_at_{0};
  uint32_t responder_rx_ms_{0};
  uint8_t responder_peer_{0x20};
  uint8_t responder_peer_sub_{0x01};
  uint8_t responder_local_sub_{0x01};
  uint32_t responder_acks_{0};
  uint32_t responder_expired_{0};
  static constexpr size_t MAX_QUEUE = 4;
};
}  // namespace bryant_writer
