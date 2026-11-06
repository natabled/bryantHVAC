#pragma once
#include "esphome/core/component.h"
#include "esphome/components/bryant_framer/bryant_framer.h"
#include "esphome/components/bryant_writer/bryant_writer.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include <cstdint>
#include <string>
#include <vector>

namespace bryant_set {
class BryantSet : public esphome::Component, public bryant_framer::FrameListener,
                  public bryant_writer::TxListener {
 public:
  void set_framer(bryant_framer::BryantFramer *f) { framer_ = f; }
  void set_writer(bryant_writer::BryantWriter *w) { writer_ = w; }
  void set_status_sensor(esphome::text_sensor::TextSensor *s) { status_sensor_ = s; }
  void set_timeout_ms(uint32_t v) { timeout_ms_ = v; }
  void set_verify_delay_ms(uint32_t v) { verify_delay_ms_ = v; }
  void set_thermostat_address(uint8_t v) { thermostat_ = v; }
  void set_sam_address(uint8_t v) { sam_ = v; }
  bool set_heat(uint8_t zone, uint8_t fahrenheit);
  bool set_hold(uint8_t zone, bool enabled);
  void setup() override;
  void loop() override;
  void on_frame(const bryant_framer::FrameView &f) override;
  void on_tx_sent(const std::string &name, uint32_t ms) override;
  float get_setup_priority() const override { return esphome::setup_priority::DATA; }
 protected:
  enum class Operation : uint8_t { HEAT, HOLD_ON, HOLD_OFF };
  enum class State : uint8_t { IDLE, READ_QUEUED, WAIT_READ, WRITE_QUEUED, WAIT_WRITE, VERIFY_QUEUED, WAIT_VERIFY };
  void status_(const char *message);
  void finish_(bool success, const char *reason);
  bool queue_read_(const std::string &name);
  bool build_write_(const bryant_framer::FrameView &f);
  bryant_framer::BryantFramer *framer_{nullptr};
  bryant_writer::BryantWriter *writer_{nullptr};
  esphome::text_sensor::TextSensor *status_sensor_{nullptr};
  uint8_t thermostat_{0x20};
  uint8_t sam_{0x92};
  uint8_t zone_{1};
  uint8_t target_{66};
  Operation operation_{Operation::HEAT};
  bool initial_hold_{false};
  uint32_t timeout_ms_{15000};
  uint32_t verify_delay_ms_{500};
  uint32_t started_ms_{0};
  uint32_t last_tx_ms_{0};
  uint32_t verify_due_ms_{0};
  uint32_t verify_poll_ms_{0};
  uint32_t next_verify_poll_ms_{0};
  uint32_t sent_count_{0};
  uint32_t confirmed_count_{0};
  uint32_t failed_count_{0};
  State state_{State::IDLE};
};
}  // namespace bryant_set
