#include "deduplicate_text.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace deduplicate_text {

static const char *const TAG = "deduplicate_text";

void DeduplicateTextSensor::setup() {
  // Nothing to do in setup
}

void DeduplicateTextSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Deduplicate Text Sensor '%s'", this->get_name().c_str());
  LOG_UPDATE_INTERVAL(this);
}

void DeduplicateTextSensor::update() {
  if (!this->f_.has_value())
    return;

  auto val = (*this->f_)();
  if (val.has_value()) {
    this->publish_state(val.value());
  }
}

void DeduplicateTextSensor::set_template(std::function<optional<std::string>()> &&f) {
  this->f_ = f;
}

#ifdef ARDUINO_ARCH_ESP8266
uint32_t DeduplicateTextSensor::calculate_hash(std::string_view str) {
  static uint8_t char_count = 0;

  uint32_t hash = 2166136261U;  // FNV-1a 32-bit offset basis
  for (char c : str) {
    hash ^= static_cast<uint32_t>(c);
    hash *= 16777619U;  // FNV-1a 32-bit prime

    // Feed watchdog periodically
    if (++char_count >= 100) {
      char_count = 0;
      App.feed_wdt();
    }
  }
  return hash;
}

void DeduplicateTextSensor::publish_state(std::string_view state) {
  uint32_t hash = calculate_hash(state);
  
  if (hash == this->last_hash_) {
    return;
  }
  
  this->last_hash_ = hash;
  text_sensor::TextSensor::publish_state(std::string(state));
}

void DeduplicateTextSensor::publish_state(const std::string &state) {
  publish_state(std::string_view(state));
}

#else // ESP32 and other platforms

uint32_t DeduplicateTextSensor::calculate_hash(const std::string &str) {
  static uint8_t char_count = 0;

  uint32_t hash = 2166136261U;  // FNV-1a 32-bit offset basis
  for (char c : str) {
    hash ^= static_cast<uint32_t>(c);
    hash *= 16777619U;  // FNV-1a 32-bit prime

    // Feed watchdog periodically
    if (++char_count >= 100) {
      char_count = 0;
      App.feed_wdt();
    }
  }
  return hash;
}

void DeduplicateTextSensor::publish_state(const std::string &state) {
  uint32_t hash = calculate_hash(state);
  
  if (hash == this->last_hash_) {
    return;
  }
  
  this->last_hash_ = hash;
  text_sensor::TextSensor::publish_state(state);
}

#endif

}  // namespace deduplicate_text
}  // namespace esphome 