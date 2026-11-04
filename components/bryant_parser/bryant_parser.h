#pragma once
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/bryant_framer/bryant_frame.h"
#include <string>
#include <vector>
#include <utility>

namespace bryant_parser {
struct MessageDef { std::string name; uint8_t dst, src, function; uint16_t reg; uint8_t minimum; };
struct FieldDef {
  std::string name, message;
  esphome::sensor::Sensor *sensor;
  uint8_t offset, width;
  bool is_signed, little_endian;
  float multiplier, add;
  uint32_t mask;
  uint8_t shift;
  float default_value;
  std::vector<std::pair<uint32_t, float>> bit_rules;
};
class BryantParser : public esphome::Component, public bryant_framer::FrameListener {
 public:
  void set_log_matches(bool v) { log_matches_ = v; }
  void set_log_unmatched(bool v) { log_unmatched_ = v; }
  void set_datetime_sensor(esphome::text_sensor::TextSensor *v) { datetime_sensor_ = v; }
  void add_message(const std::string &name, uint8_t dst, uint8_t src, uint8_t func, uint16_t reg, uint8_t min_len);
  void add_field(const std::string &name, const std::string &message, esphome::sensor::Sensor *sensor,
                 uint8_t offset, uint8_t width, bool is_signed, bool little_endian,
                 float multiplier, float add, uint32_t mask, uint8_t shift, float default_value);
  void add_bit_rule(const std::string &field_name, uint32_t mask, float value);
  void on_frame(const bryant_framer::FrameView &f) override;
 protected:
  void publish_datetime_();
  std::vector<MessageDef> messages_;
  std::vector<FieldDef> fields_;
  esphome::text_sensor::TextSensor *datetime_sensor_{nullptr};
  bool log_matches_{true}, log_unmatched_{false};
  int year_{-1}, month_{-1}, day_{-1}, hour_{-1}, minute_{-1}, second_{-1};
  bool have_date_{false}, have_time_{false};
};
}
