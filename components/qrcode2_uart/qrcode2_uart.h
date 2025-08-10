#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/automation.h"
#include "esphome/core/log.h"

#include "esphome/components/uart/uart.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include <vector>
#include <string>

namespace esphome {
namespace qrcode2_uart {

// Forward declarations
class QRCode2TextSensor;
class QRCode2BinarySensor;

class ScanTrigger : public Trigger<std::string> {
 public:
  explicit ScanTrigger() {}
};

class QRCode2UARTComponent : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  
  void set_scanning_timeout(uint32_t timeout) { scanning_timeout_ = timeout; }
  void set_trigger_pin(GPIOPin *pin) { trigger_pin_ = pin; }
  void set_led_pin(GPIOPin *pin) { led_pin_ = pin; }
  void set_scanner_trigger_pin(GPIOPin *pin) { scanner_trigger_pin_ = pin; }
  
  void add_scan_trigger(ScanTrigger *trigger) { scan_triggers_.push_back(trigger); }
  void add_text_sensor(QRCode2TextSensor *sensor) { text_sensors_.push_back(sensor); }
  void add_binary_sensor(QRCode2BinarySensor *sensor) { binary_sensors_.push_back(sensor); }
  
  void start_scan();
  void stop_scan();
  bool is_scanning() const { return scanning_; }
  
  // LED control for external access
  void set_illumination_led(bool state) { set_led_state(state); }
  
  // Configuration commands for the scanner
  void configure_scanner();
  void set_auto_scan_mode(bool enabled);
  void set_trigger_mode();
  
 protected:
  void process_uart_data();
  void handle_scan_result(const std::string &result);
  void set_led_state(bool state);
  void check_scan_timeout();
  
  std::vector<ScanTrigger *> scan_triggers_;
  std::vector<QRCode2TextSensor *> text_sensors_;
  std::vector<QRCode2BinarySensor *> binary_sensors_;
  std::string buffer_;
  bool scanning_{false};
  uint32_t scan_start_time_{0};
  uint32_t scanning_timeout_{20000};  // 20 seconds default
  
  GPIOPin *trigger_pin_{nullptr};        // Button pin (GPIO39)
  GPIOPin *led_pin_{nullptr};           // LED pin (GPIO33) 
  GPIOPin *scanner_trigger_pin_{nullptr}; // Scanner trigger pin (GPIO23)
  
  bool last_trigger_state_{false};
  
  // Scanner command constants
  static constexpr uint8_t CMD_START_SCAN[] = {0x16, 0x54, 0x0D};
  static constexpr uint8_t CMD_STOP_SCAN[] = {0x16, 0x55, 0x0D};
  static constexpr uint8_t CMD_AUTO_SCAN_ON[] = {0x16, 0x56, 0x01, 0x0D};
  static constexpr uint8_t CMD_AUTO_SCAN_OFF[] = {0x16, 0x56, 0x00, 0x0D};
  static constexpr uint8_t CMD_TRIGGER_MODE[] = {0x16, 0x58, 0x01, 0x0D};
  static constexpr uint8_t CMD_RESET[] = {0x16, 0x00, 0x0D};
};

template<typename... Ts> class StartScanAction : public Action<Ts...> {
 public:
  StartScanAction(QRCode2UARTComponent *parent) : parent_(parent) {}
  
  void play(Ts... x) override { this->parent_->start_scan(); }
  
 protected:
  QRCode2UARTComponent *parent_;
};

template<typename... Ts> class StopScanAction : public Action<Ts...> {
 public:
  StopScanAction(QRCode2UARTComponent *parent) : parent_(parent) {}
  
  void play(Ts... x) override { this->parent_->stop_scan(); }
  
 protected:
  QRCode2UARTComponent *parent_;
};

template<typename... Ts> class SetLEDAction : public Action<Ts...> {
 public:
  SetLEDAction(QRCode2UARTComponent *parent) : parent_(parent) {}
  
  void play(Ts... x) override { 
    ESP_LOGI("qrcode2_uart", "SetLEDAction called with state: %s", this->state_ ? "true" : "false");
    this->parent_->set_illumination_led(this->state_); 
  }
  
  void set_state(bool state) { this->state_ = state; }
  
 protected:
  QRCode2UARTComponent *parent_;
  bool state_{false};
};

class QRCode2TextSensor : public text_sensor::TextSensor, public Component {
 public:
  void setup() override {}
  void loop() override {}
  void set_parent(QRCode2UARTComponent *parent) { parent_ = parent; }
  void update_scan_result(const std::string &result) { this->publish_state(result); }
  
 protected:
  QRCode2UARTComponent *parent_{nullptr};
};

class QRCode2BinarySensor : public binary_sensor::BinarySensor, public Component {
 public:
  void setup() override {}
  void loop() override;
  void set_parent(QRCode2UARTComponent *parent) { parent_ = parent; }
  
 protected:
  QRCode2UARTComponent *parent_{nullptr};
  bool last_scanning_state_{false};
};

}  // namespace qrcode2_uart
}  // namespace esphome
