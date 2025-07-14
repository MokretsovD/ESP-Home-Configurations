#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/helpers.h"
#include <cstdint>

namespace esphome {
namespace deduplicate_text {

class DeduplicateTextSensor : public text_sensor::TextSensor, public PollingComponent {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  
  void set_template(std::function<optional<std::string>()> &&f);
  void publish_state(const std::string &state);

 protected:
  uint32_t last_hash_ = 0;
  optional<std::function<optional<std::string>()>> f_;
  
  // FNV-1a hash function for string deduplication
  uint32_t calculate_hash(const std::string& str);
};

}  // namespace deduplicate_text
}  // namespace esphome 