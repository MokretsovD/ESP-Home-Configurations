#pragma once

#include <vector>
#include <cmath>
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/api/custom_api_device.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_timer.h"
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
  void set_error_sensor(text_sensor::TextSensor *s) { error_sensor_ = s; }
  void set_found_address_sensor(text_sensor::TextSensor *s) { found_address_sensor_ = s; }
  void set_transceiver_status_sensor(text_sensor::TextSensor *s) { transceiver_status_sensor_ = s; }
  void set_freq(uint8_t f2, uint8_t f1, uint8_t f0) { freq2_ = f2; freq1_ = f1; freq0_ = f0; }
  void set_cca_mode(uint8_t mode) { cca_mode_ = mode; }
  void set_tx_power(uint8_t p) { tx_power_ = p; }
  void set_debug_mode(bool v) { debug_mode_ = v; }
  bool is_debug_mode() const { return debug_mode_; }
  void set_poll_interval_seconds(float seconds) {
    user_poll_interval_ms_ = (uint32_t)(seconds * 1000.0f);
    // Only restart poller if not in offline backoff — backoff is managed via
    // next_backoff_probe_ms_, not via update_interval, to avoid duplicate timers.
    if (!offline_) {
      set_update_interval(user_poll_interval_ms_);
      start_poller();
    }
  }

  void setup() override;
  void loop() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // HA API services
  void on_power();
  void on_emergency_stop();
  void on_get_status();
  void on_mode();
  void on_temp_up();
  void on_temp_down();
  // set_value: in auto mode sets temperature (8–35°C); in manual mode sets pump frequency (1.7–5.5 Hz)
  void on_set_value(float value);
  void on_find_address();
  void on_ping();

 protected:
  DieselHeaterRF *heater_{nullptr};
  uint32_t addr_{0};
  uint8_t sck_pin_{HEATER_SCK_PIN};
  uint8_t miso_pin_{HEATER_MISO_PIN};
  uint8_t mosi_pin_{HEATER_MOSI_PIN};
  uint8_t cs_pin_{HEATER_SS_PIN};
  uint8_t gdo2_pin_{HEATER_GDO2_PIN};

  // Command queue — all RF activity is driven from here; CMD_SET_VALUE is a pseudo-command
  std::vector<uint8_t> pending_cmds_;
  uint8_t current_cmd_{0xFF};
  uint8_t current_seq_{0};
  uint8_t cmd_fail_count_{0};
  uint32_t next_rxb_check_ms_{0};

  // Target for CMD_SET_VALUE: temperature (°C, auto mode) or pump frequency (Hz, manual mode)
  float target_value_{0.0f};

  // Expected state after toggle commands — set once when queued, checked before retry.
  // If GET_STATUS reveals the heater already reached the expected state, the command is skipped.
  bool mode_toggle_expected_{false};
  bool power_target_on_{false};

  text_sensor::TextSensor *state_sensor_{nullptr};
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *ambient_temp_sensor_{nullptr};
  sensor::Sensor *case_temp_sensor_{nullptr};
  sensor::Sensor *setpoint_sensor_{nullptr};
  sensor::Sensor *heat_level_sensor_{nullptr};
  sensor::Sensor *pump_freq_sensor_{nullptr};
  sensor::Sensor *rssi_sensor_{nullptr};
  binary_sensor::BinarySensor *auto_mode_sensor_{nullptr};
  text_sensor::TextSensor *error_sensor_{nullptr};
  text_sensor::TextSensor *found_address_sensor_{nullptr};
  text_sensor::TextSensor *transceiver_status_sensor_{nullptr};

  // Offline detection and backoff.
  // Backoff probing is driven by next_backoff_probe_ms_ checked in update(), NOT by
  // changing update_interval — start_poller() creates a new scheduler entry on every
  // call without cancelling the old one, so using it for backoff creates duplicate timers.
  // Backoff steps (ms): 30 s → 1 min → 2 min → 5 min → 15 min → 30 min → 1 h
  static constexpr uint32_t kBackoffMs[] = {30000, 60000, 120000, 300000, 900000, 1800000, 3600000};
  static constexpr uint8_t kBackoffSteps = sizeof(kBackoffMs) / sizeof(kBackoffMs[0]);
  bool offline_{false};
  uint8_t backoff_step_{0};
  uint32_t next_backoff_probe_ms_{0};  // millis() timestamp when the next offline probe is due
  uint32_t user_poll_interval_ms_{60000};

  bool find_address_active_{false};
  uint8_t find_address_attempts_{0};
  uint32_t find_address_last_ms_{0};

  enum class PollPhase { IDLE, RX_LISTEN };
  PollPhase poll_phase_{PollPhase::IDLE};
  uint32_t rx_window_end_ms_{0};

  bool initial_update_seen_{false};  // suppresses the immediate update() ESPHome fires at t=0
  bool cc1101_ok_{false};   // set true in setup() only if PARTNUM/VERSION match
  bool debug_mode_{false};
  uint32_t debug_last_ms_{0};
  uint32_t debug_reg_dump_ms_{0};  // tracks periodic register readback in debug mode

  uint8_t freq2_{0x10};
  uint8_t freq1_{0xB0};  // 433.938 MHz default
  uint8_t freq0_{0x9E};
  uint8_t cca_mode_{0};  // 0 = always TX, 3 = RSSI+no RX
  uint8_t tx_power_{7};  // PATABLE index 0-7; 7=+10dBm (default)

  // WiFi ↔ RF isolation: track WiFi activity to avoid overlapping RF operations
  // with WiFi TX bursts that cause 3.3V rail droops and CC1101 brownout-resets.
  uint32_t wifi_busy_until_ms_{0};   // don't start RF before this timestamp
  uint32_t publish_settle_ms_{0};    // after publishing sensors, wait before next RF
  static constexpr uint32_t kWifiSettleMs = 150;  // ms to wait after WiFi event before RF
  static constexpr uint32_t kPublishSettleMs = 100; // ms to wait after publishing before RF
  static void on_wifi_event_(void *arg, esp_event_base_t base, int32_t id, void *data);
  bool is_wifi_quiet_() const { uint32_t now = (uint32_t)(esp_timer_get_time() / 1000LL); return now > wifi_busy_until_ms_ && now > publish_settle_ms_; }

  // Deferred sensor publishing — publish only when RF is idle, with change detection.
  // This separates WiFi TX (API state pushes) from RF activity.
  bool pending_publish_{false};
  heater_state_t pending_state_{};
  void publish_heater_state_();

  static const char *state_to_string(uint8_t state);
  static const char *error_to_string(uint8_t error);
  void reset_backoff_if_offline_();
  void __attribute__((noinline)) execute_tx_burst_(uint8_t cmd);
};

}  // namespace diesel_heater_rf
}  // namespace esphome
