#include "uart_line_reader.h"
#include "esphome/core/application.h"

namespace esphome {
namespace uart_line_reader {

static const char *const TAG = "uart_line_reader";

void UartLineReaderTextSensor::loop() {
  static std::string buffer;
  // Pre-allocate memory once to avoid heap fragmentation
  if (buffer.capacity() == 0)
    buffer.reserve(128);
  while (this->available()) {
    // Feed the software watchdog so long bursts of UART data don't trigger a reset
    App.feed_wdt();
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