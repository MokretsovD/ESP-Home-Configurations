#include "deduplicate_text.h"
#include "esphome/core/log.h"

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

uint32_t DeduplicateTextSensor::calculate_hash(const std::string& str) {
  uint32_t hash = 2166136261U;  // FNV-1a 32-bit offset basis
  for (char c : str) {
    hash ^= static_cast<uint32_t>(c);
    hash *= 16777619U;  // FNV-1a 32-bit prime
  }
  return hash;
}

void DeduplicateTextSensor::publish_state(const std::string &state) {
  uint32_t hash = calculate_hash(state);
  
  // Check if this is a duplicate
  if (hash == this->last_hash_) {
    ESP_LOGVV(TAG, "'%s': Duplicate value detected, skipping publication", this->get_name().c_str());
    return;
  }
  
  // Update hash and publish
  this->last_hash_ = hash;
  ESP_LOGVV(TAG, "'%s': New unique value detected, publishing", this->get_name().c_str());
  
  // Call parent's publish_state to actually publish
  text_sensor::TextSensor::publish_state(state);
}

}  // namespace deduplicate_text
}  // namespace esphome 