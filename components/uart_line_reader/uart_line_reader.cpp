#include "uart_line_reader.h"
#include "esphome/core/application.h"

namespace esphome {
namespace uart_line_reader {

static const char *const TAG = "uart_line_reader";

void UartLineReaderTextSensor::loop() {
  // Fixed-size stack buffer - no heap allocation, no fragmentation possible
  static char buf[256];      // 256 bytes buffer
  static uint8_t idx = 0;    // current write position
  static bool overflow_warning_shown = false;

  // Reuse a static std::string to avoid repeated allocations.
  static std::string line_buf;
  if (line_buf.capacity() < sizeof(buf)) {
    line_buf.reserve(sizeof(buf));
  }

  while (this->available()) {
    // Feed the software watchdog so long bursts of UART data don't trigger a reset
    App.feed_wdt();

    char c = this->read();

    // End-of-line -> publish
    if (c == '\n') {
      if (idx > 0) {
        line_buf.assign(buf, idx);  // copy chars into std::string (no new alloc after reserve)
        this->publish_state(line_buf);
        idx = 0;  // reset for next line
        overflow_warning_shown = false;  // reset warning flag
      }
    } 
    // Buffer overflow protection - discard line and warn
    else if (idx >= sizeof(buf) - 1) {
      if (!overflow_warning_shown) {
        ESP_LOGW(TAG, "Line buffer overflow (%d bytes) - discarding line", sizeof(buf));
        overflow_warning_shown = true;
      }
      // Keep consuming characters until we hit newline to stay in sync
      if (c == '\n') {
        idx = 0;  // reset for next line
        overflow_warning_shown = false;
      }
      // Don't store character, just continue consuming
    } 
    else if (c != '\r') {  // ignore CR
      buf[idx++] = c;
    }
  }
}

void UartLineReaderTextSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "UART Line Reader Text Sensor");
  ESP_LOGCONFIG(TAG, "  Buffer size: %d bytes", 256);
}

}  // namespace uart_line_reader
}  // namespace esphome 