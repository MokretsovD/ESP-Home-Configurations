#include "uart_line_reader.h"

namespace esphome {
namespace uart_line_reader {

static const char *const TAG = "uart_line_reader";

void UartLineReaderTextSensor::loop() {
  static std::string buffer;
  while (this->available()) {
    char c = this->read();
    if (c == '\n' || buffer.size() > 120) {
      if (!buffer.empty()) {
        this->publish_state(buffer);
        buffer.clear();
      }
    } else if (c != '\r') {
      buffer += c;
    }
  }
}

void UartLineReaderTextSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "UART Line Reader Text Sensor");
}

}  // namespace uart_line_reader
}  // namespace esphome 