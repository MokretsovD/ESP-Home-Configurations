#include "qrcode2_uart.h"
#include "esphome/core/log.h"

namespace esphome {
namespace qrcode2_uart {

static const char *const TAG = "qrcode2_uart";



// Command constants are defined in the header file

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
    // Note: LED is controlled by scanner firmware, not by us
  }
  
  if (this->scanner_trigger_pin_ != nullptr) {
    this->scanner_trigger_pin_->setup();
    this->scanner_trigger_pin_->pin_mode(gpio::FLAG_OUTPUT);
    this->scanner_trigger_pin_->digital_write(true);  // Start with trigger HIGH (scanner OFF)
    ESP_LOGI(TAG, "Scanner trigger pin initialized to HIGH (scanner OFF)");
  }
  
  // Initialize button state properly after pin setup
  if (this->trigger_pin_ != nullptr) {
    bool initial_pin_state = this->trigger_pin_->digital_read();
    // With inverted: false in YAML, GPIO39 reads: HIGH = not pressed, LOW = pressed
    this->button_pressed_ = !initial_pin_state;  // LOW = pressed, HIGH = not pressed
    ESP_LOGI(TAG, "🔘 Button initialized - GPIO39 raw: %s, interpreted as button_pressed_: %s", 
             initial_pin_state ? "HIGH" : "LOW",
             this->button_pressed_ ? "TRUE (pressed)" : "FALSE (not pressed)");
  }
  
  // Small delay to let scanner boot up
  delay(500);
  
  // Configure the scanner
  this->configure_scanner();
  
  // Get device information at startup - COMMENTED OUT
  delay(1000);  // Give scanner time to fully initialize
  ESP_LOGI(TAG, "Scanner setup complete - device info requests disabled");
  // this->get_device_info();  // Function commented out - preserved for reference
}

void QRCode2UARTComponent::loop() {
  // Component heartbeat every 60 seconds (reduced frequency)
  static uint32_t last_heartbeat = 0;
  if (millis() - last_heartbeat > 60000) {
    ESP_LOGD(TAG, "QRCode2 component running, scanning: %s", this->scanning_ ? "YES" : "NO");
    last_heartbeat = millis();
  }
  
  // Check trigger button with proper state machine
  if (this->trigger_pin_ != nullptr) {
    bool pin_state = this->trigger_pin_->digital_read();  // Read raw GPIO state
    
    // Debug raw pin state every few seconds
    static uint32_t last_pin_debug = 0;
    if (millis() - last_pin_debug > 5000) {
      ESP_LOGD(TAG, "🔍 GPIO39 raw: %s (expect HIGH when not pressed, LOW when pressed)", 
               pin_state ? "HIGH" : "LOW");
      last_pin_debug = millis();
    }
    
    // With YAML inverted: false, GPIO39 reads: HIGH = not pressed, LOW = pressed
    bool button_currently_pressed = !pin_state;  // LOW = pressed, HIGH = not pressed
    
    // Detect button press (transition from HIGH to LOW on GPIO39)
    if (button_currently_pressed && !this->button_pressed_) {
      // Button just pressed (GPIO39 went LOW)
      this->button_pressed_ = true;
      this->button_press_start_time_ = millis();
      this->long_press_detected_ = false;
      ESP_LOGI(TAG, "🔘 Button PRESSED (GPIO39: HIGH→LOW) at %lu ms", this->button_press_start_time_);
      
      // Trigger short press automation for immediate feedback
      for (auto *trigger : this->short_press_triggers_) {
        trigger->trigger("short_press");
      }
    }
    
    // Check for long press while button is held (GPIO39 stays LOW)
    if (button_currently_pressed && this->button_pressed_ && !this->long_press_detected_) {
      uint32_t press_duration = millis() - this->button_press_start_time_;
      if (press_duration >= this->long_press_duration_) {  // Configurable duration
        this->long_press_detected_ = true;
        ESP_LOGI(TAG, "🔘 LONG PRESS detected after %lu ms (threshold: %lu ms) - triggering mode switch", 
                 press_duration, this->long_press_duration_);
        
        // Trigger long press automations
        for (auto *trigger : this->long_press_triggers_) {
          trigger->trigger("long_press");
        }
      }
    }
    
    // Detect button release (transition from LOW to HIGH on GPIO39)
    if (!button_currently_pressed && this->button_pressed_) {
      // Button just released (GPIO39 went HIGH)
      uint32_t press_duration = millis() - this->button_press_start_time_;
      ESP_LOGI(TAG, "🔘 Button RELEASED (GPIO39: LOW→HIGH) after %lu ms", press_duration);
      
      if (!this->long_press_detected_) {
        // Short press - toggle scanning state
        if (this->scanning_) {
          ESP_LOGI(TAG, "⏹️ SHORT PRESS - stopping scan (scanner was active)");
          this->stop_scan();
        } else {
          ESP_LOGI(TAG, "▶️ SHORT PRESS - starting scan (scanner was idle)");
          this->buffer_.clear();
          this->parsing_qr_code_ = false;
          this->start_scan();
        }
      } else {
        ESP_LOGI(TAG, "🔄 LONG PRESS completed - mode already switched");
      }
      
      // Reset button state
      this->button_pressed_ = false;
      this->long_press_detected_ = false;
    }
  }
  
  // Process incoming UART data
  this->process_uart_data();
  
  // Handle QR data timeout for Atomic QRCode2 Base when it sends ASCII without terminators
  // ONLY process data as QR codes when we're actually scanning
  if (this->parsing_qr_code_ && !this->buffer_.empty() && this->scanning_) {
    uint32_t time_since_last_data = millis() - this->last_qr_data_time_;
    if (time_since_last_data > 300) {
      // Only process as QR code if it's reasonable length and we're in scanning mode
      if (this->buffer_.length() >= 3) {
        ESP_LOGI(TAG, "QR Code scanned: %s", this->buffer_.c_str());
        this->handle_scan_result(this->buffer_);
      } else {
        ESP_LOGV(TAG, "Ignoring short buffer (likely protocol fragment): '%s'", this->buffer_.c_str());
      }
      // CRITICAL: Reset state immediately after processing to prevent multiple triggers
      this->buffer_.clear();
      this->parsing_qr_code_ = false;
      this->last_qr_data_time_ = 0;  // Reset timestamp to prevent re-triggering
    } else {
      // Debug: Show when we're waiting for timeout
      static uint32_t last_debug = 0;
      if (millis() - last_debug > 1000) {  // Debug every 1000ms (reduced frequency)
        ESP_LOGV(TAG, "⏱️ Waiting for timeout: buffer='%s', time_since_last=%lu ms", this->buffer_.c_str(), time_since_last_data);
        last_debug = millis();
      }
    }
  } else if (this->parsing_qr_code_ && !this->buffer_.empty() && !this->scanning_) {
    // If we're accumulating data but not scanning, clear it after a timeout
    uint32_t time_since_last_data = millis() - this->last_qr_data_time_;
    if (time_since_last_data > 300) {
      ESP_LOGD(TAG, "📋 Clearing non-scan data after timeout: '%s'", this->buffer_.c_str());
      this->buffer_.clear();
      this->parsing_qr_code_ = false;
      this->last_qr_data_time_ = 0;
    }
  }
  
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
    LOG_PIN("  LED Pin (firmware-controlled): ", this->led_pin_);
  }
  
  this->check_uart_settings(115200);
}

void QRCode2UARTComponent::configure_scanner() {
  ESP_LOGI(TAG, "Configuring QRCode2 scanner with correct protocol...");
  
  // Set button trigger mode (Configuration Command)
  ESP_LOGI(TAG, "Setting button trigger mode: 0x%02X 0x%02X 0x%02X 0x%02X", 
           CMD_SET_BUTTON_TRIGGER[0], CMD_SET_BUTTON_TRIGGER[1], CMD_SET_BUTTON_TRIGGER[2], CMD_SET_BUTTON_TRIGGER[3]);
  this->write_array(CMD_SET_BUTTON_TRIGGER, sizeof(CMD_SET_BUTTON_TRIGGER));
  this->flush();
  delay(200);
  
  // Enable all barcode types
  ESP_LOGI(TAG, "Enabling all barcode types: 0x%02X 0x%02X 0x%02X 0x%02X", 
           CMD_ENABLE_ALL_CODES[0], CMD_ENABLE_ALL_CODES[1], CMD_ENABLE_ALL_CODES[2], CMD_ENABLE_ALL_CODES[3]);
  this->write_array(CMD_ENABLE_ALL_CODES, sizeof(CMD_ENABLE_ALL_CODES));
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
  
  // Scanner illumination LED is automatically controlled by firmware
  
  // Activate physical trigger pin to turn ON the red laser
  if (this->scanner_trigger_pin_ != nullptr) {
    ESP_LOGD(TAG, "Activating scanner trigger pin");
    this->scanner_trigger_pin_->digital_write(false);  // LOW = scanner ON
  }
  
  // Send start scan command (Control Command)
  ESP_LOGI(TAG, "Sending start scan command: 0x%02X 0x%02X 0x%02X", CMD_START_SCAN[0], CMD_START_SCAN[1], CMD_START_SCAN[2]);
  this->write_array(CMD_START_SCAN, sizeof(CMD_START_SCAN));
  this->flush();
  delay(100);
  
  // Trigger start scan automations
  for (auto *trigger : this->start_scan_triggers_) {
    trigger->trigger("start_scan");
  }
}

void QRCode2UARTComponent::stop_scan() {
  if (!this->scanning_) {
    return;
  }
  
  ESP_LOGD(TAG, "Stopping QR code scan");
  this->scanning_ = false;
  
  // Scanner illumination LED is automatically controlled by firmware
  
  // Deactivate physical trigger pin to turn OFF the red laser
  if (this->scanner_trigger_pin_ != nullptr) {
    ESP_LOGD(TAG, "Deactivating scanner trigger pin");
    this->scanner_trigger_pin_->digital_write(true);  // HIGH = scanner OFF
  }
  
  // Trigger stop scan automations
  for (auto *trigger : this->stop_scan_triggers_) {
    trigger->trigger("stop_scan");
  }
  
  // Send stop scan command (Control Command)
  ESP_LOGI(TAG, "Sending stop scan command: 0x%02X 0x%02X 0x%02X", CMD_STOP_SCAN[0], CMD_STOP_SCAN[1], CMD_STOP_SCAN[2]);
  this->write_array(CMD_STOP_SCAN, sizeof(CMD_STOP_SCAN));
  this->flush();
}

void QRCode2UARTComponent::set_auto_scan_mode(bool enabled) {
  ESP_LOGW(TAG, "Auto scan mode setting not yet implemented with correct protocol");
  ESP_LOGI(TAG, "Auto scan mode %s requested", enabled ? "enabled" : "disabled");
  // TODO: Implement with correct PID/FID codes once found
}

void QRCode2UARTComponent::set_trigger_mode() {
  ESP_LOGW(TAG, "Old trigger mode method - using button trigger mode instead");
  // Use the button trigger configuration instead
  this->write_array(CMD_SET_BUTTON_TRIGGER, sizeof(CMD_SET_BUTTON_TRIGGER));
  this->flush();
  delay(100);
}



// DEVICE INFO FUNCTIONS - PRESERVED FOR FUTURE REFERENCE
// These functions were used to request device information via protocol
// Commented out to simplify code but preserved for future use if needed

// void QRCode2UARTComponent::get_firmware_version() {
//   ESP_LOGI(TAG, "Requesting firmware version: 0x%02X 0x%02X 0x%02X", 
//            CMD_GET_FIRMWARE_VER[0], CMD_GET_FIRMWARE_VER[1], CMD_GET_FIRMWARE_VER[2]);
//   this->write_array(CMD_GET_FIRMWARE_VER, sizeof(CMD_GET_FIRMWARE_VER));
//   this->flush();
//   delay(200);
// }

// void QRCode2UARTComponent::get_hardware_model() {
//   ESP_LOGI(TAG, "Requesting hardware model: 0x%02X 0x%02X 0x%02X", 
//            CMD_GET_HARDWARE_MODEL[0], CMD_GET_HARDWARE_MODEL[1], CMD_GET_HARDWARE_MODEL[2]);
//   this->write_array(CMD_GET_HARDWARE_MODEL, sizeof(CMD_GET_HARDWARE_MODEL));
//   this->flush();
//   delay(200);
// }

// Master function to get all device information - useful for diagnostics
// void QRCode2UARTComponent::get_device_info() {
//   ESP_LOGI(TAG, "Getting device information with correct protocol...");
//   
//   // Get software version
//   ESP_LOGI(TAG, "Requesting software version: 0x%02X 0x%02X 0x%02X", 
//            CMD_GET_SOFTWARE_VER[0], CMD_GET_SOFTWARE_VER[1], CMD_GET_SOFTWARE_VER[2]);
//   this->write_array(CMD_GET_SOFTWARE_VER, sizeof(CMD_GET_SOFTWARE_VER));
//   this->flush();
//   delay(500);
//   
//   // Get firmware version
//   // this->get_firmware_version();  // Function commented out - preserved for reference
//   delay(500);
//   
//   // Get hardware model
//   // this->get_hardware_model();   // Function commented out - preserved for reference
//   delay(500);
//   
//   // Get serial number
//   ESP_LOGI(TAG, "Requesting serial number: 0x%02X 0x%02X 0x%02X", 
//            CMD_GET_SERIAL_NUM[0], CMD_GET_SERIAL_NUM[1], CMD_GET_SERIAL_NUM[2]);
//   this->write_array(CMD_GET_SERIAL_NUM, sizeof(CMD_GET_SERIAL_NUM));
//   this->flush();
//   delay(500);
// }



void QRCode2UARTComponent::reset_scanner() {
  ESP_LOGI(TAG, "Resetting scanner: 0x%02X 0x%02X 0x%02X", 
           CMD_FACTORY_RESET[0], CMD_FACTORY_RESET[1], CMD_FACTORY_RESET[2]);
  this->write_array(CMD_FACTORY_RESET, sizeof(CMD_FACTORY_RESET));
  this->flush();
  delay(1000);  // Wait longer for reset to complete
}

void QRCode2UARTComponent::process_uart_data() {
  static std::vector<uint8_t> protocol_buffer;
  
  while (this->available()) {
    uint8_t data;
    this->read_byte(&data);
    
    // Add to protocol buffer for status responses
    protocol_buffer.push_back(data);
    
    // Keep only last 100 bytes to avoid memory issues
    if (protocol_buffer.size() > 100) {
      protocol_buffer.erase(protocol_buffer.begin());
    }
    
    // Check for protocol status reply (0x44 = Status Reply or 0x33 for Atomic QRCode2)
    if (protocol_buffer.size() >= 3) {
      size_t pos = protocol_buffer.size() - 3;
      if ((protocol_buffer[pos] == 0x44 || protocol_buffer[pos] == 0x33) && 
          (protocol_buffer[pos+1] == 0x02 || protocol_buffer[pos+1] == 0x75)) {
        uint8_t fid = protocol_buffer[pos+2];
        ESP_LOGV(TAG, "Protocol response detected: 0x%02X 0x%02X 0x%02X", 
                 protocol_buffer[pos], protocol_buffer[pos+1], protocol_buffer[pos+2]);
        
        // Clear any ASCII buffer that might contain protocol data
        if (!this->buffer_.empty() && this->buffer_.length() <= 2) {
          ESP_LOGV(TAG, "Clearing buffer with potential protocol data: '%s'", this->buffer_.c_str());
          this->buffer_.clear();
          this->parsing_qr_code_ = false;
        }
        
        if (protocol_buffer.size() >= 4) {
          uint8_t status = protocol_buffer[pos+3];
          if (status == 0x00) {  // Success
            // Check if there's a length byte for data
            if (protocol_buffer.size() >= 5) {
              uint8_t length = protocol_buffer[pos+4];
              this->handle_status_response(fid, length, protocol_buffer, pos + 5);
            }
          }
        }
      }
    }
    
    // Handle QR code scanning (ASCII data ending with \r or \n OR timeout for Atomic QRCode2)
    char c = static_cast<char>(data);
    
    // Check if this looks like QR code data (printable ASCII)
    if (c >= 0x20 && c <= 0x7E) {
      this->buffer_ += c;
      this->parsing_qr_code_ = true;
      this->last_qr_data_time_ = millis();  // Update timestamp for timeout
      if (this->scanning_) {
        ESP_LOGV(TAG, "📝 QR char: '%c' (0x%02X), buffer now: '%s'", c, data, this->buffer_.c_str());
      } else {
        ESP_LOGV(TAG, "📄 Data char (not scanning): '%c' (0x%02X), buffer now: '%s'", c, data, this->buffer_.c_str());
      }
    } else if ((c == '\r' || c == '\n') && this->parsing_qr_code_) {
      if (!this->buffer_.empty() && this->scanning_) {
        if (this->buffer_.length() >= 3) {
          ESP_LOGI(TAG, "✅ QR Code scanned (terminated): '%s'", this->buffer_.c_str());
          this->handle_scan_result(this->buffer_);
        } else {
          ESP_LOGV(TAG, "Ignoring short terminated buffer: '%s'", this->buffer_.c_str());
        }
      } else if (!this->buffer_.empty()) {
        ESP_LOGD(TAG, "📋 Ignoring data received when not scanning: '%s'", this->buffer_.c_str());
      }
      this->buffer_.clear();
      this->parsing_qr_code_ = false;
      this->last_qr_data_time_ = 0;  // Reset timestamp
      ESP_LOGV(TAG, "Reset parsing due to terminator");
    } else if (c == '\r' || c == '\n') {
      // End of some data, reset QR buffer if it has content
      if (!this->buffer_.empty()) {
        ESP_LOGV(TAG, "Clearing buffer due to unexpected terminator: '%s'", this->buffer_.c_str());
        this->buffer_.clear();
      }
      this->parsing_qr_code_ = false;
    } else {
      // Log non-ASCII data
      ESP_LOGD(TAG, "🔍 Non-ASCII byte: 0x%02X (%d)", data, data);
    }
  }
  

}

void QRCode2UARTComponent::handle_status_response(uint8_t fid, uint8_t length, const std::vector<uint8_t> &buffer, size_t data_start) {
  // Just log protocol responses, don't use them for device info
  // Device info is handled via raw ASCII startup messages instead
  
  std::string response_data = "";
  if (length > 0 && data_start < buffer.size()) {
    for (size_t i = data_start; i < data_start + length && i < buffer.size(); i++) {
      uint8_t byte = buffer[i];
      if (byte >= 0x20 && byte <= 0x7E) {  // Printable ASCII
        response_data += static_cast<char>(byte);
      }
    }
  }
  
  ESP_LOGD(TAG, "📋 Protocol Response: FID=0x%02X, length=%d, data='%s'", fid, length, response_data.c_str());
}

void QRCode2UARTComponent::handle_scan_result(const std::string &result) {
  // Stop scanning after successful scan
  this->stop_scan();
  
  // Increment scan counter for uniqueness
  this->scan_counter_++;
  
  // Update all text sensors with just the raw result
  for (auto *sensor : this->text_sensors_) {
    sensor->update_scan_result(result);
  }
  
  // Trigger automations with original result (without formatting)
  for (auto *trigger : this->scan_triggers_) {
    trigger->trigger(result);
  }
  
  ESP_LOGI(TAG, "✅ Scan #%lu complete: %s", this->scan_counter_, result.c_str());
  ESP_LOGI(TAG, "📱 Updated sensor with raw code: %s", result.c_str());
}

void QRCode2UARTComponent::check_scan_timeout() {
  if (millis() - this->scan_start_time_ > this->scanning_timeout_) {
    ESP_LOGI(TAG, "⏰ Scan timeout reached after %lu ms, stopping scan", this->scanning_timeout_);
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
