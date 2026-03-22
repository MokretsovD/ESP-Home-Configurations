#include "diesel_heater_rf.h"

namespace esphome {
namespace diesel_heater_rf {

static const char *const TAG = "diesel_heater_rf";

void DieselHeaterRFComponent::setup() {
  heater_ = new DieselHeaterRF(sck_pin_, miso_pin_, mosi_pin_, cs_pin_, gdo2_pin_);
  heater_->setFrequency(freq2_, freq1_, freq0_);
  heater_->begin(addr_);
  ESP_LOGI(TAG, "Initialized: address=0x%08X freq=0x%02X%02X%02X", addr_, freq2_, freq1_, freq0_);

  uint8_t partnum = heater_->getPartNum();
  uint8_t version = heater_->getVersion();
  uint8_t rf2, rf1, rf0;
  heater_->getFreqRegisters(&rf2, &rf1, &rf0);
  ESP_LOGI(TAG, "CC1101 FREQ regs readback: 0x%02X%02X%02X (written: 0x%02X%02X%02X)",
           rf2, rf1, rf0, freq2_, freq1_, freq0_);
  if (partnum == 0x00 && version == 0x14) {
    ESP_LOGI(TAG, "CC1101 OK: PARTNUM=0x%02X VERSION=0x%02X", partnum, version);
    if (transceiver_status_sensor_ != nullptr) {
      char buf[48];
      snprintf(buf, sizeof(buf), "OK freq=0x%02X%02X%02X", rf2, rf1, rf0);
      transceiver_status_sensor_->publish_state(buf);
    }
  } else {
    char buf[48];
    snprintf(buf, sizeof(buf), "ERROR PARTNUM=0x%02X VERSION=0x%02X", partnum, version);
    ESP_LOGE(TAG, "CC1101 unexpected response: %s (expected 0x00/0x14 — check SPI wiring)", buf);
    if (transceiver_status_sensor_ != nullptr)
      transceiver_status_sensor_->publish_state(buf);
  }

  if (found_address_sensor_ != nullptr) {
    if (addr_ != 0) {
      char buf[12];
      snprintf(buf, sizeof(buf), "0x%08X", addr_);
      found_address_sensor_->publish_state(buf);
    } else {
      found_address_sensor_->publish_state("Not configured — run Find RF Address");
    }
  }

  register_service(&DieselHeaterRFComponent::on_power, "power");
  register_service(&DieselHeaterRFComponent::on_get_status, "get_status");
  register_service(&DieselHeaterRFComponent::on_mode, "mode");
  register_service(&DieselHeaterRFComponent::on_temp_up, "temp_up");
  register_service(&DieselHeaterRFComponent::on_temp_down, "temp_down");
  register_service(&DieselHeaterRFComponent::on_set_value, "set_value", {"value"});
  register_service(&DieselHeaterRFComponent::on_find_address, "find_address");
}

void DieselHeaterRFComponent::update() {
  if (debug_mode_ || find_address_active_) return;

  // Verify CC1101 is still configured (SYNC1 should be 0x7E).
  // Only check when IDLE — reading registers while CC1101 is in RX/TX returns the STATUS byte
  // instead of the register value, producing false-alarm reinits (e.g. SYNC1=0xD3 = STATUS byte).
  uint8_t marcstate = heater_->getMarcstate();
  if (marcstate != 0x01) return;  // not IDLE — skip health check
  uint8_t sync1 = heater_->readConfigReg(0x04);
  if (sync1 != 0x7E) {
    ESP_LOGW(TAG, "CC1101 config lost (SYNC1=0x%02X) — reinitialising", sync1);
    heater_->reinitRadio();
    sync1 = heater_->readConfigReg(0x04);
    if (sync1 != 0x7E) {
      ESP_LOGE(TAG, "CC1101 reinit failed (SYNC1=0x%02X) — check VCC decoupling", sync1);
      if (transceiver_status_sensor_ != nullptr)
        transceiver_status_sensor_->publish_state("ERROR: CC1101 not responding after reinit");
      return;
    }
    ESP_LOGI(TAG, "CC1101 reinit OK");
  }

  // HEATER_CMD_WAKEUP (0x23) is a status-poll: requests a state packet from the heater.
  // The heater responds to any valid command regardless of its WOR sleep state, so no
  // special wake sequence is needed — this is purely a periodic state refresh.
  pending_cmds_.push_back(HEATER_CMD_WAKEUP);
}

// ---------------------------------------------------------------------------
// Service handlers — enqueue commands; loop() drives all RF activity
// ---------------------------------------------------------------------------

void DieselHeaterRFComponent::on_power() {
  ESP_LOGI(TAG, "Service: power toggle");
  pending_cmds_.push_back(HEATER_CMD_POWER);
}

void DieselHeaterRFComponent::on_get_status() {
  ESP_LOGI(TAG, "Service: get_status");
  pending_cmds_.push_back(HEATER_CMD_WAKEUP);
}

void DieselHeaterRFComponent::on_mode() {
  ESP_LOGI(TAG, "Service: mode toggle (auto/manual)");
  pending_cmds_.push_back(HEATER_CMD_MODE);
}

void DieselHeaterRFComponent::on_temp_up() {
  ESP_LOGI(TAG, "Service: temp/freq up");
  pending_cmds_.push_back(HEATER_CMD_UP);
}

void DieselHeaterRFComponent::on_temp_down() {
  ESP_LOGI(TAG, "Service: temp/freq down");
  pending_cmds_.push_back(HEATER_CMD_DOWN);
}

void DieselHeaterRFComponent::on_set_value(float value) {
  if (last_setpoint_ == -127) {
    ESP_LOGW(TAG, "Service: set_value ignored — no state received yet");
    return;
  }
  if (last_auto_mode_) {
    // Auto mode: clamp to valid setpoint range and round to integer
    if (value < 8.0f) value = 8.0f;
    if (value > 35.0f) value = 35.0f;
    int8_t target = static_cast<int8_t>(value);
    ESP_LOGI(TAG, "Service: set_value %d°C (auto mode, current=%d°C)", target, last_setpoint_);
    target_value_ = static_cast<float>(target);
  } else {
    // Manual mode: clamp to valid pump frequency range and round to 0.1 Hz
    if (value < 1.7f) value = 1.7f;
    if (value > 5.5f) value = 5.5f;
    float target = roundf(value * 10.0f) / 10.0f;
    ESP_LOGI(TAG, "Service: set_value %.1f Hz (manual mode, current=%.1f Hz)", target, last_pump_freq_);
    target_value_ = target;
  }
  pending_cmds_.push_back(CMD_SET_VALUE);
}

void DieselHeaterRFComponent::on_find_address() {
  if (find_address_active_) return;
  ESP_LOGI(TAG, "Service: find_address — scanning (up to 10 attempts of 1.5s each)");
  if (found_address_sensor_ != nullptr)
    found_address_sensor_->publish_state("Searching...");
  find_address_attempts_ = 0;
  find_address_last_ms_ = 0;
  find_address_active_ = true;
}

// ---------------------------------------------------------------------------
// Main loop — processes pending_cmds_ queue; one command per state-machine cycle
// ---------------------------------------------------------------------------

void DieselHeaterRFComponent::loop() {

  // ── RX phase: waiting for heater to acknowledge the current command ──────
  if (poll_phase_ == PollPhase::RX) {
    heater_state_t state;
    if (heater_->getState(&state, 400)) {
      // ACK received — update sensors and pop the completed command
      poll_phase_ = PollPhase::IDLE;
      cmd_fail_count_ = 0;
      last_setpoint_ = state.setpoint;
      last_pump_freq_ = state.pumpFreq;
      last_auto_mode_ = state.autoMode;

      if (state_sensor_ != nullptr)
        state_sensor_->publish_state(state_to_string(state.state));
      if (voltage_sensor_ != nullptr)
        voltage_sensor_->publish_state(state.voltage);
      if (ambient_temp_sensor_ != nullptr)
        ambient_temp_sensor_->publish_state(state.ambientTemp);
      if (case_temp_sensor_ != nullptr)
        case_temp_sensor_->publish_state(state.caseTemp);
      if (setpoint_sensor_ != nullptr)
        setpoint_sensor_->publish_state(state.setpoint);
      if (heat_level_sensor_ != nullptr)
        heat_level_sensor_->publish_state(state.power);
      if (pump_freq_sensor_ != nullptr)
        pump_freq_sensor_->publish_state(state.pumpFreq);
      if (rssi_sensor_ != nullptr)
        rssi_sensor_->publish_state(state.rssi);
      if (auto_mode_sensor_ != nullptr)
        auto_mode_sensor_->publish_state(state.autoMode);

      ESP_LOGD(TAG, "state=%s power=%d setpoint=%d°C pumpFreq=%.1fHz ambient=%d°C voltage=%.1fV",
               state_to_string(state.state), state.power, state.setpoint,
               state.pumpFreq, state.ambientTemp, state.voltage);

      // MODE is a single-packet toggle: only pop when the toggle is confirmed in the response.
      // If the heater acknowledged but auto_mode didn't change, stay in IDLE and retry.
      if (current_cmd_ == HEATER_CMD_MODE && state.autoMode != mode_toggle_expected_) {
        ESP_LOGD(TAG, "MODE: toggle not confirmed (auto_mode=%d, expected=%d) — retrying",
                 (int)state.autoMode, (int)mode_toggle_expected_);
        return;  // poll_phase_ already IDLE; don't pop
      }

      if (!pending_cmds_.empty())
        pending_cmds_.erase(pending_cmds_.begin());

    } else if (millis() >= poll_rx_deadline_ms_) {
      // Deadline expired — command failed
      poll_phase_ = PollPhase::IDLE;
      cmd_fail_count_++;
      ESP_LOGW(TAG, "Cmd 0x%02X timed out (fail_count=%d)", current_cmd_, cmd_fail_count_);
      if (cmd_fail_count_ >= 10) {
        ESP_LOGE(TAG, "10 consecutive command failures — heater unreachable, clearing queue");
        pending_cmds_.clear();
        cmd_fail_count_ = 0;
      }
    } else {
      // 400 ms window elapsed, deadline not yet reached — retransmit.
      // MODE/POWER are toggle commands: each unique-seq packet fires one toggle, so sending
      // N packets = N toggles. Use exactly 1 packet per retransmit to avoid double-toggling.
      ESP_LOGD(TAG, "No response — retransmitting cmd 0x%02X", current_cmd_);
      bool is_toggle = (current_cmd_ == HEATER_CMD_MODE || current_cmd_ == HEATER_CMD_POWER);
      heater_->sendCommand(current_cmd_, addr_, is_toggle ? 1 : 14);
      // Check CC1101 SYNC1 every 4 retransmits (~1.6 s). CC1101 is in IDLE after sendCommand
      // so no marcstate guard is needed. VCC noise during TX bursts can corrupt SYNC1 causing
      // heater state packets to go unrecognised even though TX still works.
      if ((++reinit_check_counter_ & 0x03) == 0) {
        uint8_t sync1 = heater_->readConfigReg(0x04);
        if (sync1 != 0x7E) {
          ESP_LOGW(TAG, "CC1101 config lost mid-TX (SYNC1=0x%02X) — reinitialising", sync1);
          heater_->reinitRadio();
        }
      }
    }
    return;
  }

  // ── Debug mode: raw packet capture ───────────────────────────────────────
  if (debug_mode_ && !find_address_active_) {
    uint32_t now = millis();

    // Every 10 s: read back key CC1101 registers; reinit only if in IDLE and SYNC1 is wrong.
    if (now - debug_reg_dump_ms_ >= 10000) {
      uint8_t marcstate = heater_->getMarcstate();
      uint8_t sync1 = heater_->readConfigReg(0x04);
      if (marcstate == 0x01 && sync1 != 0x7E) {
        ESP_LOGW(TAG, "CC1101 config lost in debug mode (SYNC1=0x%02X) — reinitialising", sync1);
        heater_->reinitRadio();
        sync1 = heater_->readConfigReg(0x04);
        marcstate = heater_->getMarcstate();
      }
      if (marcstate == 0x01) {
        uint8_t freq2     = heater_->readConfigReg(0x0D);
        uint8_t freq1_reg = heater_->readConfigReg(0x0E);
        uint8_t freq0     = heater_->readConfigReg(0x0F);
        uint8_t mdmcfg4   = heater_->readConfigReg(0x10);
        uint8_t mdmcfg3   = heater_->readConfigReg(0x11);
        uint8_t mdmcfg2   = heater_->readConfigReg(0x12);
        uint8_t sync0     = heater_->readConfigReg(0x05);
        ESP_LOGI(TAG, "CC1101 regs: FREQ=0x%02X%02X%02X (want 0x10B09E) MDMCFG4=0x%02X MDMCFG3=0x%02X (want 0xF8/0x93) MDMCFG2=0x%02X (want 0x13) SYNC=0x%02X%02X (want 0x7E3C) MARCSTATE=0x%02X",
                 freq2, freq1_reg, freq0, mdmcfg4, mdmcfg3, mdmcfg2, sync1, sync0, marcstate);
      } else {
        ESP_LOGD(TAG, "CC1101 reg dump skipped — MARCSTATE=0x%02X (not IDLE, reads unreliable)", marcstate);
      }
      debug_reg_dump_ms_ = now;
    }

    if (now - debug_last_ms_ < 200) return;

    char raw[64];
    uint8_t len = 0;
    heater_->receiveRaw(raw, &len, 1000);
    debug_last_ms_ = millis();

    if (len > 0) {
      char hex[196] = {};
      for (int i = 0; i < len; i++)
        snprintf(hex + i * 3, 4, "%02X ", (uint8_t) raw[i]);
      const char *label = (len == 26) ? ", heater state" : (len == 12) ? ", remote command" : ", unexpected length";
      ESP_LOGI(TAG, "RF RAW [%d bytes%s]: %s", len, label, hex);
    } else {
      ESP_LOGI(TAG, "RF RAW: no packet in 1s window");
    }
    return;
  }

  // ── Find address scan ─────────────────────────────────────────────────────
  if (find_address_active_) {
    uint32_t now = millis();
    if (now - find_address_last_ms_ < 200) return;

    uint32_t addr = heater_->findAddress(1500);
    find_address_last_ms_ = millis();
    find_address_attempts_++;

    if (addr != 0) {
      char buf[12];
      snprintf(buf, sizeof(buf), "0x%08X", addr);
      ESP_LOGI(TAG, "Found heater address: %s — update heater_address substitution and re-flash", buf);
      if (found_address_sensor_ != nullptr)
        found_address_sensor_->publish_state(buf);
      find_address_active_ = false;
    } else if (find_address_attempts_ >= 10) {
      ESP_LOGW(TAG, "No heater found after %d attempts", find_address_attempts_);
      if (found_address_sensor_ != nullptr)
        found_address_sensor_->publish_state("Not found");
      find_address_active_ = false;
    }
    return;
  }

  // ── IDLE: process next command from queue ─────────────────────────────────
  if (pending_cmds_.empty() || addr_ == 0) return;

  uint8_t cmd = pending_cmds_.front();

  // CMD_SET_VALUE is a pseudo-command — evaluate target vs current state and insert
  // the appropriate UP/DOWN step at the front; re-evaluated after each state response
  // until the target is reached.
  if (cmd == CMD_SET_VALUE) {
    if (last_auto_mode_) {
      int8_t target = static_cast<int8_t>(target_value_);
      if (last_setpoint_ == target) {
        pending_cmds_.erase(pending_cmds_.begin());
        ESP_LOGI(TAG, "set_value: target %d°C reached", target);
        return;
      }
      uint8_t step = (last_setpoint_ < target) ? HEATER_CMD_UP : HEATER_CMD_DOWN;
      ESP_LOGD(TAG, "set_value: setpoint %d→%d, queuing %s", last_setpoint_, target,
               step == HEATER_CMD_UP ? "UP" : "DOWN");
      pending_cmds_.insert(pending_cmds_.begin(), step);
    } else {
      float target = target_value_;
      if (fabsf(last_pump_freq_ - target) < 0.05f) {
        pending_cmds_.erase(pending_cmds_.begin());
        ESP_LOGI(TAG, "set_value: target %.1f Hz reached", target);
        return;
      }
      uint8_t step = (last_pump_freq_ < target) ? HEATER_CMD_UP : HEATER_CMD_DOWN;
      ESP_LOGD(TAG, "set_value: pumpFreq %.1f→%.1f, queuing %s", last_pump_freq_, target,
               step == HEATER_CMD_UP ? "UP" : "DOWN");
      pending_cmds_.insert(pending_cmds_.begin(), step);
    }
    return;
  }

  // For MODE: save expected auto_mode value so the RX handler can confirm the toggle
  if (cmd == HEATER_CMD_MODE)
    mode_toggle_expected_ = !last_auto_mode_;

  // Transmit — MODE/POWER are toggle commands (1 packet each); all others use 14-packet burst
  current_cmd_ = cmd;
  bool is_toggle = (cmd == HEATER_CMD_MODE || cmd == HEATER_CMD_POWER);
  ESP_LOGD(TAG, "TX cmd 0x%02X (queue depth=%d)", cmd, (int) pending_cmds_.size());
  heater_->sendCommand(cmd, addr_, is_toggle ? 1 : 14);
  poll_phase_ = PollPhase::RX;
  poll_rx_deadline_ms_ = millis() + 8000;
}

const char *DieselHeaterRFComponent::state_to_string(uint8_t state) {
  switch (state) {
    case HEATER_STATE_OFF:           return "Off";
    case HEATER_STATE_STARTUP:       return "Startup";
    case HEATER_STATE_WARMING:       return "Warming";
    case HEATER_STATE_WARMING_WAIT:  return "Warming Wait";
    case HEATER_STATE_PRE_RUN:       return "Pre-Run";
    case HEATER_STATE_RUNNING:       return "Running";
    case HEATER_STATE_SHUTDOWN:      return "Shutdown";
    case HEATER_STATE_SHUTTING_DOWN: return "Shutting Down";
    case HEATER_STATE_COOLING:       return "Cooling";
    default:                         return "Unknown";
  }
}

}  // namespace diesel_heater_rf
}  // namespace esphome
