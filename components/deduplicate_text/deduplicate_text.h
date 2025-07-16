#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/helpers.h"
#include <cstdint>

#ifdef ARDUINO_ARCH_ESP8266
#include <string_view>
#endif

namespace esphome {
namespace deduplicate_text {

class DeduplicateTextSensor : public text_sensor::TextSensor, public PollingComponent {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  
  void set_template(std::function<optional<std::string>()> &&f);
  void publish_state(const std::string &state);

#ifdef ARDUINO_ARCH_ESP8266
  void publish_state(std::string_view state);
#endif

 protected:
  uint32_t last_hash_ = 0;
  optional<std::function<optional<std::string>()>> f_;
  
  // FNV-1a hash function for string deduplication
#ifdef ARDUINO_ARCH_ESP8266
  uint32_t calculate_hash(std::string_view str);
#else
  uint32_t calculate_hash(const std::string &str);
#endif 
};

}  // namespace deduplicate_text
}  // namespace esphome 