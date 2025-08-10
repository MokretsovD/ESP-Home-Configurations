#include "qrcode2_uart.h"
#include "esphome/core/log.h"

namespace esphome {
namespace qrcode2_uart {

static const char *const TAG = "qrcode2_uart";

// Define the command constants
constexpr uint8_t QRCode2UARTComponent::CMD_START_SCAN[];
constexpr uint8_t QRCode2UARTComponent::CMD_STOP_SCAN[];
constexpr uint8_t QRCode2UARTComponent::CMD_AUTO_SCAN_ON[];
constexpr uint8_t QRCode2UARTComponent::CMD_AUTO_SCAN_OFF[];
constexpr uint8_t QRCode2UARTComponent::CMD_TRIGGER_MODE[];
constexpr uint8_t QRCode2UARTComponent::CMD_RESET[];

void QRCode2UARTComponent::setup() {
  ESP_LOGI(TAG, "=== QRCode2 UART Component Setup Starting ===");
  ESP_LOGI(TAG, "Component pointer: %p", this);
  ESP_LOGI(TAG, "Button pin configured: %s", this->trigger_pin_ != nullptr ? "YES" : "NO");
  ESP_LOGI(TAG, "LED pin configured: %s", this->led_pin_ != nullptr ? "YES" : "NO");
  ESP_LOGI(TAG, "Scanner trigger pin configured: %s", this->scanner_trigger_pin_ != nullptr ? "YES" : "NO");
  
  // Configure pins
  if (this->trigger_pin_ != nullptr) {
    this->trigger_pin_->setup();
    this->trigger_pin_->pin_mode(gpio::FLAG_INPUT);
  }
  
  if (this->led_pin_ != nullptr) {
    this->led_pin_->setup();
    this->led_pin_->pin_mode(gpio::FLAG_OUTPUT);
    this->set_led_state(false);  // Start with scanner illumination LED off
  }
  
  if (this->scanner_trigger_pin_ != nullptr) {
    this->scanner_trigger_pin_->setup();
    this->scanner_trigger_pin_->pin_mode(gpio::FLAG_OUTPUT);
    this->scanner_trigger_pin_->digital_write(true);  // Start with trigger HIGH (scanner OFF)
    ESP_LOGI(TAG, "Scanner trigger pin initialized to HIGH (scanner OFF)");
  }
  
  // Small delay to let scanner boot up
  delay(500);
  
  // Configure the scanner
  this->configure_scanner();
}

void QRCode2UARTComponent::loop() {
  // Component heartbeat every 60 seconds (reduced frequency)
  static uint32_t last_heartbeat = 0;
  if (millis() - last_heartbeat > 60000) {
    ESP_LOGD(TAG, "QRCode2 component running, scanning: %s", this->scanning_ ? "YES" : "NO");
    last_heartbeat = millis();
  }
  
  // Check trigger button if configured
  if (this->trigger_pin_ != nullptr) {
    bool pin_state = this->trigger_pin_->digital_read();
    bool current_state = !pin_state;  // Button pressed when pin goes LOW (inverted logic)
    
    // Detect falling edge (button press) - from HIGH to LOW
    if (!pin_state && this->last_trigger_state_) {
      // Button pressed (falling edge detected)
      ESP_LOGI(TAG, "Button pressed, starting scan");
      this->start_scan();
    }
    
    this->last_trigger_state_ = pin_state;  // Store raw pin state for edge detection
  }
  
  // Process incoming UART data
  this->process_uart_data();
  
  // Check for scan timeout
  if (this->scanning_) {
    this->check_scan_timeout();
  }
}

void QRCode2UARTComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "QRCode2 UART Scanner:");
  ESP_LOGCONFIG(TAG, "  Scanning timeout: %u ms", this->scanning_timeout_);
  
  if (this->trigger_pin_ != nullptr) {
    LOG_PIN("  Trigger Pin: ", this->trigger_pin_);
  }
  
  if (this->led_pin_ != nullptr) {
    LOG_PIN("  LED Pin: ", this->led_pin_);
  }
  
  this->check_uart_settings(115200);
}

void QRCode2UARTComponent::configure_scanner() {
  ESP_LOGI(TAG, "Configuring QRCode2 scanner...");
  
  // Try sending a reset command first
  ESP_LOGI(TAG, "Sending reset command: 0x%02X 0x%02X 0x%02X", CMD_RESET[0], CMD_RESET[1], CMD_RESET[2]);
  this->write_array(CMD_RESET, sizeof(CMD_RESET));
  this->flush();
  delay(500);  // Wait longer after reset
  
  // Try enabling auto scan mode instead of trigger mode
  ESP_LOGI(TAG, "Enabling auto scan mode for testing");
  this->set_auto_scan_mode(true);
  this->flush();
  delay(200);
  
  ESP_LOGI(TAG, "QRCode2 scanner configuration complete");
}

void QRCode2UARTComponent::start_scan() {
  if (this->scanning_) {
    ESP_LOGW(TAG, "Already scanning, ignoring start_scan request");
    return;
  }
  
  ESP_LOGI(TAG, "Starting QR code scan");
  this->scanning_ = true;
  this->scan_start_time_ = millis();
  this->buffer_.clear();
  
  // Turn on scanner illumination LED for better scanning in dark conditions
  this->set_led_state(true);
  
  // Activate physical trigger pin to turn ON the red laser
  if (this->scanner_trigger_pin_ != nullptr) {
    ESP_LOGD(TAG, "Activating scanner trigger pin");
    this->scanner_trigger_pin_->digital_write(false);  // LOW = scanner ON
  }
  
  // Send start scan command multiple times
  ESP_LOGI(TAG, "Sending start scan command: 0x%02X 0x%02X 0x%02X", CMD_START_SCAN[0], CMD_START_SCAN[1], CMD_START_SCAN[2]);
  for (int i = 0; i < 3; i++) {
    this->write_array(CMD_START_SCAN, sizeof(CMD_START_SCAN));
    this->flush();
    delay(100);
  }
}

void QRCode2UARTComponent::stop_scan() {
  if (!this->scanning_) {
    return;
  }
  
  ESP_LOGD(TAG, "Stopping QR code scan");
  this->scanning_ = false;
  
  // Turn off scanner illumination LED
  this->set_led_state(false);
  
  // Deactivate physical trigger pin to turn OFF the red laser
  if (this->scanner_trigger_pin_ != nullptr) {
    ESP_LOGD(TAG, "Deactivating scanner trigger pin");
    this->scanner_trigger_pin_->digital_write(true);  // HIGH = scanner OFF
  }
  
  // Send stop scan command
  ESP_LOGI(TAG, "Sending stop scan command: 0x%02X 0x%02X 0x%02X", CMD_STOP_SCAN[0], CMD_STOP_SCAN[1], CMD_STOP_SCAN[2]);
  this->write_array(CMD_STOP_SCAN, sizeof(CMD_STOP_SCAN));
  this->flush();
}

void QRCode2UARTComponent::set_auto_scan_mode(bool enabled) {
  if (enabled) {
    this->write_array(CMD_AUTO_SCAN_ON, sizeof(CMD_AUTO_SCAN_ON));
  } else {
    this->write_array(CMD_AUTO_SCAN_OFF, sizeof(CMD_AUTO_SCAN_OFF));
  }
}

void QRCode2UARTComponent::set_trigger_mode() {
  this->write_array(CMD_TRIGGER_MODE, sizeof(CMD_TRIGGER_MODE));
}

void QRCode2UARTComponent::process_uart_data() {
  while (this->available()) {
    uint8_t data;
    this->read_byte(&data);
    
    // Log received data for debugging (reduced verbosity)
    ESP_LOGV(TAG, "Received UART byte: 0x%02X ('%c')", data, (data >= 0x20 && data <= 0x7E) ? data : '.');
    
    // Convert to char and add to buffer
    char c = static_cast<char>(data);
    
    // Handle different line endings
    if (c == '\r' || c == '\n') {
      if (!this->buffer_.empty()) {
        // Check if this looks like a command response (single char T or U)
        if (this->buffer_.length() == 1 && (this->buffer_ == "T" || this->buffer_ == "U")) {
          ESP_LOGV(TAG, "Ignoring command response: %s", this->buffer_.c_str());
        } else {
          ESP_LOGI(TAG, "QR Code scanned: %s", this->buffer_.c_str());
          this->handle_scan_result(this->buffer_);
        }
        this->buffer_.clear();
      }
    } else if (c >= 0x20 && c <= 0x7E) {  // Printable ASCII characters
      this->buffer_ += c;
    }
  }
}

void QRCode2UARTComponent::handle_scan_result(const std::string &result) {
  // Stop scanning after successful scan
  this->stop_scan();
  
  // Update all text sensors
  for (auto *sensor : this->text_sensors_) {
    sensor->update_scan_result(result);
  }
  
  // Trigger all registered scan triggers
  for (auto *trigger : this->scan_triggers_) {
    trigger->trigger(result);
  }
}

void QRCode2UARTComponent::set_led_state(bool state) {
  if (this->led_pin_ != nullptr) {
    ESP_LOGI(TAG, "Setting scanner LED to: %s", state ? "ON" : "OFF");
    this->led_pin_->digital_write(state);
  } else {
    ESP_LOGW(TAG, "LED pin not configured, cannot set LED state");
  }
}

void QRCode2UARTComponent::check_scan_timeout() {
  if (millis() - this->scan_start_time_ > this->scanning_timeout_) {
    ESP_LOGD(TAG, "Scan timeout reached, stopping scan");
    this->stop_scan();
  }
}

void QRCode2BinarySensor::loop() {
  if (this->parent_ != nullptr) {
    bool current_state = this->parent_->is_scanning();
    if (current_state != this->last_scanning_state_) {
      this->publish_state(current_state);
      this->last_scanning_state_ = current_state;
    }
  }
}

}  // namespace qrcode2_uart
}  // namespace esphome
