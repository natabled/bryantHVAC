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
class BryantWriter : public esphome::Component, public esphome::uart::UARTDevice {
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
  static constexpr size_t MAX_QUEUE = 4;
};
}  // namespace bryant_writer
