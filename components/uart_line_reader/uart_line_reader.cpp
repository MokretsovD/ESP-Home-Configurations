#include "uart_line_reader.h"
#include "esphome/core/application.h"

namespace esphome {
namespace uart_line_reader {

static const char *const TAG = "uart_line_reader";

void UartLineReaderTextSensor::loop() {
  static std::string buffer;
  // Pre-allocate memory once to avoid heap fragmentation
  if (buffer.capacity() == 0) {
    buffer.reserve(128);
  }
    
  // Counter for periodic watchdog feeding
  static uint8_t char_count = 0;
    
  while (this->available()) {
    char c = this->read();
    
    // Feed watchdog periodically
    // Every 16 characters provides good balance of responsiveness vs performance
    if (++char_count >= 16) {
      char_count = 0;
      App.feed_wdt();
    }
    
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