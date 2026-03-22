# Diesel Heater RF (`diesel_heater_rf`)

A custom ESPHome component for wireless control and monitoring of VEVOR and other Chinese diesel heaters via 433 MHz RF using a TI CC1101 transceiver module. Replicates the protocol of the four-button OLED remote controller without any physical modifications to the heater.

Based on [DieselHeaterRF by Jarno Kyttälä](https://github.com/jakkik/DieselHeaterRF).

## Features

- **Non-invasive**: Communicates via RF — no wiring to the heater's control unit required
- **Full remote parity**: All commands available from the physical remote are exposed as HA services
- **Rich telemetry**: State, temperatures, voltage, pump frequency, heat level, and RF signal strength
- **Address discovery**: Built-in pairing service to find the heater's RF address
- **Direct setpoint control**: `set_value` service drives temperature or pump frequency to a target automatically using a non-blocking command queue

## Hardware Required

- ESP32 (any variant)
- TI CC1101 433 MHz transceiver module (e.g. Ebyte E07-M1101S, ~$5 USD)

### Default Wiring (CC1101 → ESP32)

| CC1101 | ESP32  |
|--------|--------|
| GDO2   | GPIO4  |
| SCK    | GPIO18 |
| MOSI   | GPIO23 |
| MISO   | GPIO19 |
| CSn    | GPIO5  |
| VCC    | 3V3    |
| GND    | GND    |

All pins are configurable via substitutions.

## Configuration

### Minimal Example

```yaml
diesel_heater_rf:
  heater_address: "0x12AB34CD"
```

### Full Example (via device package)

```yaml
substitutions:
  heater_address: !secret diesel_heater_address
  heater_gdo2_pin: "4"
  heater_sck_pin: "18"
  heater_miso_pin: "19"
  heater_mosi_pin: "23"
  heater_cs_pin: "5"

packages:
  hardware: !include packages/device-configs/diesel-heater-rf.yaml
```

### All Options

```yaml
diesel_heater_rf:
  heater_address: "0x12AB34CD"   # required; use find_address service to discover
  sck_pin: 18                    # optional, default 18
  miso_pin: 19                   # optional, default 19
  mosi_pin: 23                   # optional, default 23
  cs_pin: 5                      # optional, default 5
  gdo2_pin: 4                    # optional, default 4
  update_interval: 60s           # optional, default 60s

  state_sensor:
    name: "Heater State"

  ambient_temp_sensor:
    name: "Heater Ambient Temperature"

  case_temp_sensor:
    name: "Heater Case Temperature"

  setpoint_sensor:
    name: "Heater Temperature Setpoint"

  heat_level_sensor:
    name: "Heater Heat Level"

  pump_freq_sensor:
    name: "Heater Pump Frequency"

  voltage_sensor:
    name: "Heater Supply Voltage"

  rssi_sensor:
    name: "Heater RF Signal Strength"

  auto_mode_sensor:
    name: "Heater Auto Mode"
```

## Sensors

| Sensor                      | Type          | Unit | Description                                                        |
|-----------------------------|---------------|------|--------------------------------------------------------------------|
| `state_sensor`              | Text sensor   | —    | Operational state: Off, Startup, Warming, Running, Cooling, etc.   |
| `ambient_temp_sensor`       | Sensor        | °C   | Ambient air temperature                                            |
| `case_temp_sensor`          | Sensor        | °C   | Heat exchanger / case temperature                                  |
| `setpoint_sensor`           | Sensor        | °C   | Current temperature setpoint                                       |
| `heat_level_sensor`         | Sensor        | —    | Heat output level (1–10)                                           |
| `pump_freq_sensor`          | Sensor        | Hz   | Fuel pump frequency (manual mode)                                  |
| `voltage_sensor`            | Sensor        | V    | Supply voltage                                                     |
| `rssi_sensor`               | Sensor        | dBm  | RF signal strength                                                 |
| `auto_mode_sensor`          | Binary sensor | —    | `true` = thermostat mode, `false` = manual Hz mode                 |
| `found_address_sensor`      | Text sensor   | —    | RF address found by the `find_address` scan                        |
| `transceiver_status_sensor` | Text sensor   | —    | CC1101 init result and any reinit errors                           |

## Home Assistant Services

All services are registered under `esphome.<device_name>_<service>`.

| Service | Parameters | Description |
| --- | --- | --- |
| `power` | — | Toggle heater on/off |
| `get_status` | — | Request a state packet from the heater immediately |
| `mode` | — | Toggle between auto (thermostat) and manual (pump Hz) mode |
| `temp_up` | — | Increase setpoint by 1°C |
| `temp_down` | — | Decrease setpoint by 1°C |
| `set_value` | `value` (float) | Auto mode: sets temperature target (8–35°C); manual mode: sets pump frequency target (1.7–5.5 Hz). Drives to target using UP/DOWN steps confirmed against heater state. |
| `find_address` | — | Listen for 10 s and log the heater's RF address |

## First-Time Setup: Finding the Heater Address

1. Set `heater_address: "0x00000000"` in `secrets.yaml` and flash the device
2. Make sure the physical remote is powered on (heater broadcasts continuously)
3. Call the `find_address` HA service (or trigger it from the ESPHome web UI)
4. Check the ESPHome device logs — the address is printed as `Found heater address: 0xXXXXXXXX`
5. Update `diesel_heater_address` in `secrets.yaml` and re-flash

## Requirements

- **Framework:** Arduino (required — the library uses `SPI.h` and Arduino GPIO functions directly)
- **Platform:** ESP32 only
- **ESPHome:** 2026.x (tested on 2026.2.4)
- **Dependencies:** `api` component with `custom_services: true` must be present for HA service registration:
  ```yaml
  api:
    encryption:
      key: !secret ...
    custom_services: true
  ```

## Notes

- **Non-blocking architecture**: all RF activity runs through a command queue in `loop()`. `update()` only enqueues a status poll and returns immediately. No operation blocks for more than 400 ms per loop cycle.
- **`set_value` is re-evaluated**: the pseudo-command stays in the queue and inserts one UP or DOWN step at a time, confirmed against the heater state response, until the target is reached. Interruptions (e.g. RF gaps) are handled automatically on the next cycle.
- **Toggle commands** (`mode`, `power`) use a 14-packet burst for the initial TX and each retransmit. All packets in a burst share the same sequence number, and retransmits use `resendLastCommand()` to keep the same sequence number across retransmit cycles. The heater de-duplicates by sequence number, so the entire burst — including retransmits — counts as exactly one toggle.
- **`HEATER_CMD_GET_STATUS` (0x23) is a status poll**, not a session-establishment wakeup. The heater responds to any valid command regardless of its WOR (Wake-On-Radio) sleep state.
- **CC1101 config recovery**: VCC noise during TX bursts can corrupt CC1101 registers (most visibly SYNC1), causing received packets to go unrecognised. The component checks SYNC1 every 4 retransmits (~1.6 s) and reinitialises if needed. Add 100 nF ceramic + 10 µF electrolytic decoupling capacitors close to the CC1101 VCC pin to reduce occurrence.
- The component is compatible with the original physical remote — both can coexist on the same RF network simultaneously.
