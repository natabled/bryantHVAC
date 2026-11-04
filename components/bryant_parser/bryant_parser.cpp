#include "bryant_parser.h"
#include <cstdio>
#include <inttypes.h>
namespace bryant_parser {
static const char *const TAG = "bryant_parser";
void BryantParser::add_message(const std::string &name, uint8_t dst, uint8_t src,
                               uint8_t func, uint16_t reg, uint8_t min_len) {
  messages_.push_back({name, dst, src, func, reg, min_len});
}
void BryantParser::add_field(const std::string &name, const std::string &message,
                             esphome::sensor::Sensor *sensor, uint8_t offset, uint8_t width,
                             bool is_signed, bool little_endian, float multiplier, float add,
                             uint32_t mask, uint8_t shift, float default_value) {
  fields_.push_back({name, message, sensor, offset, width, is_signed, little_endian,
                     multiplier, add, mask, shift, default_value, {}});
}
void BryantParser::add_bit_rule(const std::string &field_name, uint32_t mask, float value) {
  for (auto &f : fields_) if (f.name == field_name) { f.bit_rules.emplace_back(mask, value); return; }
}
void BryantParser::on_frame(const bryant_framer::FrameView &f) {
  if (f.length < 3) return;
  const uint16_t reg = static_cast<uint16_t>((static_cast<uint16_t>(f.payload[1]) << 8) | f.payload[2]);
  const MessageDef *message = nullptr;
  for (const auto &m : messages_) {
    if (m.dst == f.dst && m.src == f.src && m.function == f.function &&
        m.reg == reg && f.length >= m.minimum) { message = &m; break; }
  }
  if (message == nullptr) {
    if (log_unmatched_) ESP_LOGV(TAG, "unmatched frame=%" PRIu32 " dst=%02X src=%02X func=%02X reg=%04X len=%u",
                                f.id, f.dst, f.src, f.function, reg, static_cast<unsigned>(f.length));
    return;
  }
  if (log_matches_) ESP_LOGD(TAG, "matched frame=%" PRIu32 " message=%s reg=%04X len=%u",
                             f.id, message->name.c_str(), reg, static_cast<unsigned>(f.length));
  // Date and time are published as numeric fields and a combined local ISO date-time string.
  if (message->name == "thermostat_time" && f.length >= 6) {
    hour_ = f.payload[3]; minute_ = f.payload[4]; second_ = f.payload[5];
    have_time_ = true; publish_datetime_();
  } else if (message->name == "thermostat_date" && f.length >= 6) {
    day_ = f.payload[3]; month_ = f.payload[4]; year_ = f.payload[5];
    if (year_ < 100) year_ += 2000;
    have_date_ = true; publish_datetime_();
  }
  for (auto &field : fields_) {
    if (field.message != message->name) continue;
    const unsigned bytes = field.width / 8U;
    if (static_cast<unsigned>(field.offset) + bytes > f.length) {
      ESP_LOGW(TAG, "field=%s offset=%u width=%u beyond payload=%u", field.name.c_str(),
               static_cast<unsigned>(field.offset), static_cast<unsigned>(field.width),
               static_cast<unsigned>(f.length));
      continue;
    }
    uint32_t raw = 0;
    if (field.little_endian) {
      for (unsigned i = 0; i < bytes; ++i) raw |= static_cast<uint32_t>(f.payload[field.offset + i]) << (i * 8U);
    } else {
      for (unsigned i = 0; i < bytes; ++i) raw = (raw << 8U) | f.payload[field.offset + i];
    }
    float result;
    if (!field.bit_rules.empty()) {
      result = field.default_value;
      for (const auto &rule : field.bit_rules) {
        if ((raw & rule.first) == rule.first) { result = rule.second; break; }
      }
    } else {
      const uint32_t masked = (raw & field.mask) >> field.shift;
      int64_t signed_value = masked;
      if (field.is_signed && field.mask == 0xFFFFFFFFUL) {
        if (field.width < 32 && (raw & (uint32_t{1} << (field.width - 1))))
          signed_value = static_cast<int64_t>(raw) - (int64_t{1} << field.width);
        else if (field.width == 32) signed_value = static_cast<int32_t>(raw);
      }
      result = static_cast<float>(field.is_signed ? signed_value : static_cast<int64_t>(masked)) *
               field.multiplier + field.add;
    }
    field.sensor->publish_state(result);
    if (log_matches_) ESP_LOGD(TAG, "field=%s raw=0x%" PRIX32 " value=%.3f", field.name.c_str(),
                               raw, static_cast<double>(result));
  }
}
void BryantParser::publish_datetime_() {
  if (!have_date_ || !have_time_ || datetime_sensor_ == nullptr) return;
  if (year_ < 2000 || year_ > 2099 || month_ < 1 || month_ > 12 || day_ < 1 || day_ > 31 ||
      hour_ < 0 || hour_ > 23 || minute_ < 0 || minute_ > 59 || second_ < 0 || second_ > 59) {
    ESP_LOGW(TAG, "thermostat date/time out of range; not publishing");
    return;
  }
  char iso[24];
  snprintf(iso, sizeof(iso), "%04d-%02d-%02dT%02d:%02d:%02d", year_, month_, day_, hour_, minute_, second_);
  datetime_sensor_->publish_state(iso);
  if (log_matches_) ESP_LOGD(TAG, "thermostat datetime=%s (local, no timezone)", iso);
}
}
