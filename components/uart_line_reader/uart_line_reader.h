#pragma once
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace uart_line_reader {

class UartLineReaderTextSensor : public text_sensor::TextSensor, public Component, public uart::UARTDevice {
 public:
  void setup() override {}
  void loop() override;
  void dump_config() override;
};

}  // namespace uart_line_reader
}  // namespace esphome 