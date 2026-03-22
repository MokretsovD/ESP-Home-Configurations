#pragma once

#include <vector>
#include <cmath>
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/api/custom_api_device.h"
#include "DieselHeaterRF.h"

namespace esphome {
namespace diesel_heater_rf {

class DieselHeaterRFComponent : public PollingComponent, public api::CustomAPIDevice {
 public:
  // Pseudo-command: never sent over RF; consumed by loop() to drive set_value logic
  static constexpr uint8_t CMD_SET_VALUE = 0xFE;

  void set_heater_address(uint32_t addr) { addr_ = addr; }
  void set_sck_pin(uint8_t pin) { sck_pin_ = pin; }
  void set_miso_pin(uint8_t pin) { miso_pin_ = pin; }
  void set_mosi_pin(uint8_t pin) { mosi_pin_ = pin; }
  void set_cs_pin(uint8_t pin) { cs_pin_ = pin; }
  void set_gdo2_pin(uint8_t pin) { gdo2_pin_ = pin; }

  void set_state_sensor(text_sensor::TextSensor *s) { state_sensor_ = s; }
  void set_voltage_sensor(sensor::Sensor *s) { voltage_sensor_ = s; }
  void set_ambient_temp_sensor(sensor::Sensor *s) { ambient_temp_sensor_ = s; }
  void set_case_temp_sensor(sensor::Sensor *s) { case_temp_sensor_ = s; }
  void set_setpoint_sensor(sensor::Sensor *s) { setpoint_sensor_ = s; }
  void set_heat_level_sensor(sensor::Sensor *s) { heat_level_sensor_ = s; }
  void set_pump_freq_sensor(sensor::Sensor *s) { pump_freq_sensor_ = s; }
  void set_rssi_sensor(sensor::Sensor *s) { rssi_sensor_ = s; }
  void set_auto_mode_sensor(binary_sensor::BinarySensor *s) { auto_mode_sensor_ = s; }
  void set_found_address_sensor(text_sensor::TextSensor *s) { found_address_sensor_ = s; }
  void set_transceiver_status_sensor(text_sensor::TextSensor *s) { transceiver_status_sensor_ = s; }
  void set_freq(uint8_t f2, uint8_t f1, uint8_t f0) { freq2_ = f2; freq1_ = f1; freq0_ = f0; }
  void set_debug_mode(bool v) { debug_mode_ = v; }
  bool is_debug_mode() const { return debug_mode_; }
  void set_poll_interval_seconds(float seconds) {
    set_update_interval((uint32_t)(seconds * 1000.0f));
    start_poller();
  }

  void setup() override;
  void loop() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // HA API services
  void on_power();
  void on_get_status();
  void on_mode();
  void on_temp_up();
  void on_temp_down();
  // set_value: in auto mode sets temperature (8–35°C); in manual mode sets pump frequency (1.7–5.5 Hz)
  void on_set_value(float value);
  void on_find_address();

 protected:
  DieselHeaterRF *heater_{nullptr};
  uint32_t addr_{0};
  uint8_t sck_pin_{HEATER_SCK_PIN};
  uint8_t miso_pin_{HEATER_MISO_PIN};
  uint8_t mosi_pin_{HEATER_MOSI_PIN};
  uint8_t cs_pin_{HEATER_SS_PIN};
  uint8_t gdo2_pin_{HEATER_GDO2_PIN};

  // Last known values from heater state — updated on every received status packet
  int8_t last_setpoint_{-127};
  float last_pump_freq_{0.0f};
  bool last_auto_mode_{false};

  // Command queue — all RF activity is driven from here; CMD_SET_VALUE is a pseudo-command
  std::vector<uint8_t> pending_cmds_;
  uint8_t current_cmd_{0xFF};
  uint8_t cmd_fail_count_{0};
  uint8_t reinit_check_counter_{0};  // throttles mid-TX SYNC1 check to every 4 retransmits

  // Target for CMD_SET_VALUE: temperature (°C, auto mode) or pump frequency (Hz, manual mode)
  float target_value_{0.0f};

  // Expected auto_mode value after a MODE toggle — used to confirm the toggle took effect
  bool mode_toggle_expected_{false};

  text_sensor::TextSensor *state_sensor_{nullptr};
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *ambient_temp_sensor_{nullptr};
  sensor::Sensor *case_temp_sensor_{nullptr};
  sensor::Sensor *setpoint_sensor_{nullptr};
  sensor::Sensor *heat_level_sensor_{nullptr};
  sensor::Sensor *pump_freq_sensor_{nullptr};
  sensor::Sensor *rssi_sensor_{nullptr};
  binary_sensor::BinarySensor *auto_mode_sensor_{nullptr};
  text_sensor::TextSensor *found_address_sensor_{nullptr};
  text_sensor::TextSensor *transceiver_status_sensor_{nullptr};

  bool find_address_active_{false};
  uint8_t find_address_attempts_{0};
  uint32_t find_address_last_ms_{0};

  enum class PollPhase { IDLE, RX };
  PollPhase poll_phase_{PollPhase::IDLE};
  uint32_t poll_rx_deadline_ms_{0};

  bool debug_mode_{false};
  uint32_t debug_last_ms_{0};
  uint32_t debug_reg_dump_ms_{0};  // tracks periodic register readback in debug mode

  uint8_t freq2_{0x10};
  uint8_t freq1_{0xB0};  // 433.938 MHz default
  uint8_t freq0_{0x9E};

  static const char *state_to_string(uint8_t state);
};

}  // namespace diesel_heater_rf
}  // namespace esphome
