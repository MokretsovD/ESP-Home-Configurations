#include "diesel_heater_rf.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace diesel_heater_rf {

static const char *const TAG = "diesel_heater_rf";

void DieselHeaterRFComponent::setup() {
  heater_ = new DieselHeaterRF(sck_pin_, miso_pin_, mosi_pin_, cs_pin_, gdo2_pin_);
  heater_->setFrequency(freq2_, freq1_, freq0_);
  heater_->setCcaMode(cca_mode_);
  heater_->setTxPower(tx_power_);
  delay(100); // CC1101 power-on settling before first SPI access
  heater_->begin(addr_);
  user_poll_interval_ms_ = get_update_interval();
  ESP_LOGI(TAG, "Initialized: address=0x%08X freq=0x%02X%02X%02X", addr_, freq2_, freq1_, freq0_);

  uint8_t partnum = heater_->getPartNum();
  uint8_t version = heater_->getVersion();
  uint8_t rf2, rf1, rf0;
  heater_->getFreqRegisters(&rf2, &rf1, &rf0);
  ESP_LOGI(TAG, "CC1101 FREQ regs readback: 0x%02X%02X%02X (written: 0x%02X%02X%02X)",
           rf2, rf1, rf0, freq2_, freq1_, freq0_);
  // Diagnostic: snapshot key registers immediately after initRadio to confirm init worked.
  uint8_t sync1_init = heater_->readConfigReg(0x04);
  uint8_t mdmcfg4_init = heater_->readConfigReg(0x10);
  uint8_t marcstate_init = heater_->getMarcstate();
  ESP_LOGI(TAG, "Post-init regs: SYNC1=0x%02X FREQ=0x%02X%02X%02X MDMCFG4=0x%02X MARCSTATE=0x%02X",
           sync1_init, rf2, rf1, rf0, mdmcfg4_init, marcstate_init);

  if (partnum == 0x00 && version == 0x14) {
    cc1101_ok_ = true;
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
  register_service(&DieselHeaterRFComponent::on_emergency_stop, "emergency_stop");
  register_service(&DieselHeaterRFComponent::on_get_status, "get_status");
  register_service(&DieselHeaterRFComponent::on_mode, "mode");
  register_service(&DieselHeaterRFComponent::on_temp_up, "temp_up");
  register_service(&DieselHeaterRFComponent::on_temp_down, "temp_down");
  register_service(&DieselHeaterRFComponent::on_set_value, "set_value", {"value"});
  register_service(&DieselHeaterRFComponent::on_find_address, "find_address");
  register_service(&DieselHeaterRFComponent::on_ping, "ping");

  if (!cc1101_ok_) {
    ESP_LOGE(TAG, "CC1101 init failed — RF polling disabled. Check SPI wiring.");
    return;
  }

  // Track WiFi activity to avoid overlapping RF with WiFi TX bursts.
  esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi_event_, this);
  // Mark WiFi as busy for the first 25 s — WiFi connects, DHCP, mDNS, API handshake
  // all cause sustained TX activity that can brownout-reset the CC1101.
  wifi_busy_until_ms_ = millis() + 25000;

  // Delay the first poll by 25 s to let WiFi fully settle before first SPI access.
  set_timeout(25000, [this]() { pending_cmds_.push_back(HEATER_CMD_GET_STATUS); });
}

void DieselHeaterRFComponent::update() {
  if (!cc1101_ok_) return;
  if (!initial_update_seen_) { initial_update_seen_ = true; return; }
  if (debug_mode_ || find_address_active_) return;

  // Never touch SPI during an active TX/RX cycle — any SPI transaction during
  // active RX risks bit-flipping the R/W bit (e.g. read 0x84 → write 0x04),
  // which would silently corrupt CC1101 registers and break packet reception.
  if (poll_phase_ != PollPhase::IDLE) return;

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

  if (offline_) {
    // In offline mode probing is timed by next_backoff_probe_ms_, not by update_interval.
    // update() still fires on the normal schedule (for the CC1101 health check above),
    // but only enqueues a probe when the backoff window has elapsed.
    if (millis() < next_backoff_probe_ms_) return;
  }

  // HEATER_CMD_GET_STATUS (0x23) is a status-poll: requests a state packet from the heater.
  // The heater responds to any valid command regardless of its WOR sleep state, so no
  // special wake sequence is needed — this is purely a periodic state refresh.
  pending_cmds_.push_back(HEATER_CMD_GET_STATUS);
}

// ---------------------------------------------------------------------------
// Service handlers — enqueue commands; loop() drives all RF activity
// ---------------------------------------------------------------------------

// Reset backoff to step 0 when the user explicitly sends a command while offline,
// so probing becomes aggressive again while the user is actively trying to connect.
void DieselHeaterRFComponent::reset_backoff_if_offline_() {
  if (!offline_) return;
  backoff_step_ = 0;
  next_backoff_probe_ms_ = millis() + kBackoffMs[0];
  ESP_LOGI(TAG, "User command while offline — resetting backoff to %lus",
           (unsigned long)(kBackoffMs[0] / 1000));
}

void DieselHeaterRFComponent::on_power() {
  if (offline_) {
    ESP_LOGW(TAG, "Service: power ignored — heater offline");
    return;
  }
  uint8_t s = pending_state_.state;
  if (s != HEATER_STATE_OFF && s != HEATER_STATE_RUNNING) {
    ESP_LOGW(TAG, "Service: power blocked — heater in transitional state %s (0x%02X)", state_to_string(s), s);
    return;
  }
  power_target_on_ = (s == HEATER_STATE_OFF);
  ESP_LOGI(TAG, "Service: power %s", power_target_on_ ? "on" : "off");
  pending_cmds_.push_back(HEATER_CMD_POWER);
}

void DieselHeaterRFComponent::on_emergency_stop() {
  if (offline_) {
    ESP_LOGW(TAG, "Service: emergency stop ignored — heater offline");
    return;
  }
  uint8_t s = pending_state_.state;
  if (s == HEATER_STATE_OFF) {
    ESP_LOGI(TAG, "Service: emergency stop — heater already off");
    return;
  }
  if (s == HEATER_STATE_SHUTDOWN || s == HEATER_STATE_SHUTTING_DOWN || s == HEATER_STATE_COOLING) {
    ESP_LOGI(TAG, "Service: emergency stop — heater already shutting down (%s)", state_to_string(s));
    return;
  }
  power_target_on_ = false;
  ESP_LOGW(TAG, "Service: EMERGENCY STOP from state %s (0x%02X)", state_to_string(s), s);
  pending_cmds_.push_back(HEATER_CMD_POWER);
}

void DieselHeaterRFComponent::on_get_status() {
  ESP_LOGI(TAG, "Service: get_status");
  reset_backoff_if_offline_();
  pending_cmds_.push_back(HEATER_CMD_GET_STATUS);
}

void DieselHeaterRFComponent::on_mode() {
  if (offline_) {
    ESP_LOGW(TAG, "Service: mode ignored — heater offline");
    return;
  }
  mode_toggle_expected_ = !pending_state_.autoMode;
  ESP_LOGI(TAG, "Service: mode toggle (target=%s)", mode_toggle_expected_ ? "auto" : "manual");
  pending_cmds_.push_back(HEATER_CMD_MODE);
}

void DieselHeaterRFComponent::on_temp_up() {
  if (offline_) {
    ESP_LOGW(TAG, "Service: temp_up ignored — heater offline");
    return;
  }
  ESP_LOGI(TAG, "Service: temp/freq up");
  pending_cmds_.push_back(HEATER_CMD_UP);
}

void DieselHeaterRFComponent::on_temp_down() {
  if (offline_) {
    ESP_LOGW(TAG, "Service: temp_down ignored — heater offline");
    return;
  }
  ESP_LOGI(TAG, "Service: temp/freq down");
  pending_cmds_.push_back(HEATER_CMD_DOWN);
}

void DieselHeaterRFComponent::on_set_value(float value) {
  if (offline_) {
    ESP_LOGW(TAG, "Service: set_value ignored — heater offline");
    return;
  }
  if (pending_state_.autoMode) {
    // Auto mode: clamp to valid setpoint range and round to integer
    if (value < 8.0f) value = 8.0f;
    if (value > 35.0f) value = 35.0f;
    int8_t target = static_cast<int8_t>(value);
    ESP_LOGI(TAG, "Service: set_value %d°C (auto mode, current=%d°C)", target, pending_state_.setpoint);
    target_value_ = static_cast<float>(target);
  } else {
    // Manual mode: clamp to valid pump frequency range and round to 0.1 Hz
    if (value < 1.7f) value = 1.7f;
    if (value > 5.5f) value = 5.5f;
    float target = roundf(value * 10.0f) / 10.0f;
    ESP_LOGI(TAG, "Service: set_value %.1f Hz (manual mode, current=%.1f Hz)", target, pending_state_.pumpFreq);
    target_value_ = target;
  }
  pending_cmds_.push_back(CMD_SET_VALUE);
}

void DieselHeaterRFComponent::on_ping() {
  ESP_LOGI(TAG, "Service: ping — immediate status poll");
  reset_backoff_if_offline_();
  pending_cmds_.insert(pending_cmds_.begin(), HEATER_CMD_GET_STATUS);
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
// Isolated TX path — noinline so changes elsewhere in loop() don't shift
// the compiled binary layout of delay/reinit/sendCommand/startRx.
// ---------------------------------------------------------------------------
void __attribute__((noinline)) DieselHeaterRFComponent::execute_tx_burst_(uint8_t cmd) {
  delay(50);
  heater_->reinitRadio();
  heater_->sendCommand(cmd, addr_, 14, current_seq_);
  // After sendCommand, CC1101 is in FSTXON with synth locked.
  // startRxFromFstxon() enters RX directly without recalibration — preserves
  // VCO tuning from the TX burst.  startRx() would SIDLE first, killing the
  // synth lock and forcing recalibration that can drift the RX frequency.
  heater_->startRxFromFstxon();
  rx_window_end_ms_ = millis() + 1000;
  next_rxb_check_ms_ = millis() + 200;
  poll_phase_ = PollPhase::RX_LISTEN;
}

// ---------------------------------------------------------------------------
// Main loop — processes pending_cmds_ queue; one command per state-machine cycle
// ---------------------------------------------------------------------------

void DieselHeaterRFComponent::loop() {
  if (!cc1101_ok_) return;

  // ── RX_LISTEN: non-blocking GDO2 poll ────────────────────────────────────
  if (poll_phase_ == PollPhase::RX_LISTEN) {
    // Hot path: GDO2 GPIO only — zero SPI, zero bus contention with active RX.
    bool gdo2 = heater_->isRxAvailable();
    // Warm path: check RXBYTES via SPI every ~200ms to catch overflow/stale packets.
    // Infrequent enough to avoid timing interference with incoming packets.
    if (!gdo2 && millis() > next_rxb_check_ms_) {
      next_rxb_check_ms_ = millis() + 200;
      uint8_t rxb = heater_->getRxBytes();
      if (rxb >= 26) {
        if (rxb >= 64) {
          heater_->startRx();
          return;
        }
        gdo2 = true;
      }
    }
    if (gdo2) {
      heater_state_t state;
      if (heater_->readPacket(&state)) {
        // ACK received — defer sensor publishing to a later loop() iteration
        // so WiFi TX from API state pushes doesn't overlap with RF activity.
        poll_phase_ = PollPhase::IDLE;
        cmd_fail_count_ = 0;

        if (offline_) {
          offline_ = false;
          backoff_step_ = 0;
          ESP_LOGI(TAG, "Heater back online — resuming normal polling");
        }
        // Save state for deferred publishing — don't publish now (WiFi TX during RF settle).
        pending_state_ = state;
        pending_publish_ = true;

        // MODE is a toggle: only pop when auto_mode actually flipped
        if (current_cmd_ == HEATER_CMD_MODE && state.autoMode != mode_toggle_expected_) {
          ESP_LOGD(TAG, "MODE: toggle not confirmed (auto_mode=%d, expected=%d) — retrying",
                   (int)state.autoMode, (int)mode_toggle_expected_);
          return;
        }

        if (!pending_cmds_.empty())
          pending_cmds_.erase(pending_cmds_.begin());
      } else {
        // Packet in FIFO but wrong address or bad CRC — restart RX, keep window
        heater_->startRx();
      }
      return;
    }

    if (millis() > rx_window_end_ms_) {
      if (heater_->isRxAvailable()) return;  // last-chance GDO2 check

      cmd_fail_count_++;
      if (cmd_fail_count_ >= 12) {
        cmd_fail_count_ = 0;
        heater_->endTxBurst();  // SIDLE

        if (current_cmd_ != HEATER_CMD_GET_STATUS) {
          // Action command exhausted 12 attempts — prepend GET_STATUS to verify heater state.
          pending_cmds_.insert(pending_cmds_.begin(), HEATER_CMD_GET_STATUS);
          poll_phase_ = PollPhase::IDLE;
          return;
        }

        // GET_STATUS exhausted 12 attempts — check registers, then go offline.
        pending_cmds_.clear();
        uint8_t sync1 = heater_->readConfigReg(0x04);
        uint8_t iocfg2 = heater_->readConfigReg(0x00);
        uint8_t freq2 = heater_->readConfigReg(0x0D);
        uint8_t freq1 = heater_->readConfigReg(0x0E);
        uint8_t mdmcfg4 = heater_->readConfigReg(0x10);
        uint8_t post_ms = heater_->getMarcstate();
        bool regs_ok = (sync1 == 0x7E) && (iocfg2 == 0x07) && (freq2 == freq2_) && (freq1 == freq1_) && (mdmcfg4 == 0xF8);
        if (!regs_ok) {
          ESP_LOGW(TAG, "12 failures + register corruption (SYNC1=0x%02X IOCFG2=0x%02X FREQ2=0x%02X FREQ1=0x%02X MDMCFG4=0x%02X MS=0x%02X) — reinitialising",
                   sync1, iocfg2, freq2, freq1, mdmcfg4, post_ms);
          heater_->reinitRadio();
          post_ms = heater_->getMarcstate();
          ESP_LOGI(TAG, "Post-reinit MARCSTATE=0x%02X (expect 0x01)", post_ms);
          poll_phase_ = PollPhase::IDLE;
          return;
        }
        ESP_LOGE(TAG, "12 failures, regs: SYNC1=0x%02X IOCFG2=0x%02X FREQ2=0x%02X FREQ1=0x%02X MDMCFG4=0x%02X MS=0x%02X — heater unreachable",
                 sync1, iocfg2, freq2, freq1, mdmcfg4, post_ms);
        poll_phase_ = PollPhase::IDLE;
        if (offline_) {
          if (backoff_step_ < kBackoffSteps - 1) backoff_step_++;  // disabled for debugging
          next_backoff_probe_ms_ = millis() + kBackoffMs[backoff_step_];
          ESP_LOGI(TAG, "Heater offline — next probe in %lus (step %d/%d)",
                   (unsigned long)(kBackoffMs[backoff_step_] / 1000), backoff_step_ + 1, (int)kBackoffSteps);
        } else {
          offline_ = true;
          backoff_step_ = 0;
          next_backoff_probe_ms_ = millis() + kBackoffMs[0];
          if (state_sensor_ != nullptr) state_sensor_->publish_state("Offline");
          ESP_LOGE(TAG, "Heater offline — first probe in %lus",
                   (unsigned long)(kBackoffMs[0] / 1000));
        }
        return;
      }

      // RX timeout, not yet exhausted — go IDLE so the unified TX path retransmits.
      // Command stays at front of pending_cmds_; cmd_fail_count_ > 0 signals retransmit.
      poll_phase_ = PollPhase::IDLE;
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

  // ── Deferred sensor publishing — fires once per RX success, when RF is idle.
  // publish_heater_state_() sets publish_settle_ms_ so WiFi TX from API pushes
  // has time to complete before the next RF operation.
  if (pending_publish_) {
    pending_publish_ = false;
    publish_heater_state_();
    return;  // yield to ESPHome event loop — let WiFi flush before next RF
  }

  // ── IDLE: process next command from queue ─────────────────────────────────
  if (pending_cmds_.empty() || addr_ == 0) return;

  // Wait for WiFi to settle before starting RF — WiFi TX causes 3.3V rail droops
  // that brownout-reset the CC1101. This covers WiFi scans, reconnects, and the
  // settle period after our own sensor publishes.
  if (!is_wifi_quiet_() && cmd_fail_count_ == 0) return;

  uint8_t cmd = pending_cmds_.front();

  // CMD_SET_VALUE is a pseudo-command — evaluate target vs current state and insert
  // the appropriate UP/DOWN step at the front; re-evaluated after each state response
  // until the target is reached.
  if (cmd == CMD_SET_VALUE) {
    if (pending_state_.autoMode) {
      int8_t target = static_cast<int8_t>(target_value_);
      if (pending_state_.setpoint == target) {
        pending_cmds_.erase(pending_cmds_.begin());
        ESP_LOGI(TAG, "set_value: target %d°C reached", target);
        return;
      }
      uint8_t step = (pending_state_.setpoint < target) ? HEATER_CMD_UP : HEATER_CMD_DOWN;
      ESP_LOGD(TAG, "set_value: setpoint %d→%d, queuing %s", pending_state_.setpoint, target,
               step == HEATER_CMD_UP ? "UP" : "DOWN");
      pending_cmds_.insert(pending_cmds_.begin(), step);
    } else {
      float target = target_value_;
      if (fabsf(pending_state_.pumpFreq - target) < 0.05f) {
        pending_cmds_.erase(pending_cmds_.begin());
        ESP_LOGI(TAG, "set_value: target %.1f Hz reached", target);
        return;
      }
      uint8_t step = (pending_state_.pumpFreq < target) ? HEATER_CMD_UP : HEATER_CMD_DOWN;
      ESP_LOGD(TAG, "set_value: pumpFreq %.1f→%.1f, queuing %s", pending_state_.pumpFreq, target,
               step == HEATER_CMD_UP ? "UP" : "DOWN");
      pending_cmds_.insert(pending_cmds_.begin(), step);
    }
    return;
  }

  // Pre-send idempotency checks — if GET_STATUS revealed the heater already
  // reached the expected state, skip the command instead of retrying with a new seq#.
  if (cmd == HEATER_CMD_POWER) {
    bool effectively_on = (pending_state_.state != HEATER_STATE_OFF &&
                           pending_state_.state != HEATER_STATE_SHUTDOWN &&
                           pending_state_.state != HEATER_STATE_SHUTTING_DOWN &&
                           pending_state_.state != HEATER_STATE_COOLING);
    if (effectively_on == power_target_on_) {
      ESP_LOGI(TAG, "POWER: heater already %s — skipping", effectively_on ? "on" : "off");
      pending_cmds_.erase(pending_cmds_.begin());
      return;
    }
  }
  if (cmd == HEATER_CMD_MODE && pending_state_.autoMode == mode_toggle_expected_) {
    ESP_LOGI(TAG, "MODE: already %s — skipping", pending_state_.autoMode ? "auto" : "manual");
    pending_cmds_.erase(pending_cmds_.begin());
    return;
  }

  // Unified TX path — handles both first burst and retransmit.
    // GET_STATUS: new seq# every 3 attempts (0,3,6), retransmit on others.
  // Action cmds: new seq# only on attempt 0, retransmit on 1-8.
  bool is_retransmit = cmd == HEATER_CMD_GET_STATUS ? (cmd_fail_count_ % 3) != 0 : cmd_fail_count_ > 0;
  if (!is_retransmit) {
    current_cmd_ = cmd;
    current_seq_ = heater_->nextSeq();
  } else {
  }

  execute_tx_burst_(cmd);
}

// ---------------------------------------------------------------------------
// WiFi event handler — extends wifi_busy_until_ms_ when WiFi does anything
// that causes sustained TX: scanning, (re)connecting, or receiving disconnect.
// Runs in the WiFi task context — only touch atomic/trivial members.
// ---------------------------------------------------------------------------
void DieselHeaterRFComponent::on_wifi_event_(void *arg, esp_event_base_t base, int32_t id, void *data) {
  auto *self = static_cast<DieselHeaterRFComponent *>(arg);
  uint32_t settle = 0;
  switch (id) {
    case WIFI_EVENT_SCAN_DONE:        settle = 200; break;  // scan burst just ended
    case WIFI_EVENT_STA_CONNECTED:    settle = 500; break;  // DHCP + API handshake coming
    case WIFI_EVENT_STA_DISCONNECTED: settle = 300; break;  // reconnect attempt imminent
    default: return;  // ignore routine events
  }
  uint32_t now = (uint32_t)(esp_timer_get_time() / 1000LL);
  uint32_t deadline = now + settle;
  if ((int32_t)(deadline - self->wifi_busy_until_ms_) > 0)
    self->wifi_busy_until_ms_ = deadline;
}

// ---------------------------------------------------------------------------
// Deferred sensor publishing with change detection.
// Called from loop() when IDLE + WiFi quiet. Only publishes values that
// actually changed, reducing WiFi TX to the minimum needed.
// ---------------------------------------------------------------------------
void DieselHeaterRFComponent::publish_heater_state_() {
  const heater_state_t &s = pending_state_;
  const char *state_str = state_to_string(s.state);

  // Publish only changed values — each publish_state() triggers an API message over WiFi.
  if (state_sensor_        && (!state_sensor_->has_state() || state_sensor_->state != state_str))
    state_sensor_->publish_state(state_str);
  if (voltage_sensor_      && (!voltage_sensor_->has_state() || voltage_sensor_->state != s.voltage))
    voltage_sensor_->publish_state(s.voltage);
  if (ambient_temp_sensor_ && (!ambient_temp_sensor_->has_state() || (int)ambient_temp_sensor_->state != s.ambientTemp))
    ambient_temp_sensor_->publish_state(s.ambientTemp);
  if (case_temp_sensor_    && (!case_temp_sensor_->has_state() || (int)case_temp_sensor_->state != s.caseTemp))
    case_temp_sensor_->publish_state(s.caseTemp);
  if (setpoint_sensor_     && (!setpoint_sensor_->has_state() || (int)setpoint_sensor_->state != s.setpoint))
    setpoint_sensor_->publish_state(s.setpoint);
  if (heat_level_sensor_   && (!heat_level_sensor_->has_state() || (int)heat_level_sensor_->state != s.power))
    heat_level_sensor_->publish_state(s.power);
  if (pump_freq_sensor_    && (!pump_freq_sensor_->has_state() || fabsf(pump_freq_sensor_->state - s.pumpFreq) >= 0.05f))
    pump_freq_sensor_->publish_state(s.pumpFreq);
  if (rssi_sensor_         && (!rssi_sensor_->has_state() || (int)rssi_sensor_->state != s.rssi))
    rssi_sensor_->publish_state(s.rssi);
  if (auto_mode_sensor_    && (!auto_mode_sensor_->has_state() || auto_mode_sensor_->state != (bool)s.autoMode))
    auto_mode_sensor_->publish_state(s.autoMode);
  const char *err_str = error_to_string(s.errorCode);
  if (error_sensor_        && (!error_sensor_->has_state() || error_sensor_->state != err_str))
    error_sensor_->publish_state(err_str);

  ESP_LOGD(TAG, "state=%s mode=%s power=%d setpoint=%d°C pumpFreq=%.1fHz ambient=%d°C voltage=%.1fV error=%s",
           state_str, s.autoMode ? "auto" : "manual",
           s.power, s.setpoint, s.pumpFreq, s.ambientTemp, s.voltage, err_str);

  // Publishing triggers WiFi TX — mark settle period before next RF.
  publish_settle_ms_ = millis() + kPublishSettleMs;
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

const char *DieselHeaterRFComponent::error_to_string(uint8_t error) {
  switch (error) {
    case 0x00: return "None";
    case 0x01: return "Overvoltage";
    case 0x02: return "Undervoltage";
    case 0x03: return "Glow Plug Failure";
    case 0x04: return "Pump Failure";
    case 0x05: return "Overheat";
    case 0x06: return "Motor Failure";
    case 0x07: return "Communication Error";
    case 0x08: return "Flame Out";
    default:   return "Unknown";
  }
}

}  // namespace diesel_heater_rf
}  // namespace esphome
