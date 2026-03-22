# Plan: Composable BLE Proxy Package

**Goal:** Enable Bluetooth proxy functionality to be added to any ESP32 device config
(e.g. `diesel-heater.yaml`) by including a single thin package, without touching the
existing dedicated `bluetooth-proxy-*.yaml` device configs.

---

## Background

The existing `packages/device-configs/bluetooth-proxy.yaml` is a **full self-contained
device package**. It declares every top-level YAML key (`esphome:`, `esp32:`, `api:`,
`ota:`, `logger:`, `packages:`, etc.) and is designed to be *the* primary package for a
dedicated Bluetooth proxy device.

Including it in a device that already declares those same keys (e.g. `diesel-heater.yaml`)
causes duplicate-key conflicts. Additionally, it hard-codes `framework: esp-idf`, which
conflicts with `diesel-heater.yaml`'s required `framework: arduino` (needed by the bundled
CC1101 SPI library).

The solution is to extract just the BLE-specific components into a new thin package that
has no framework or device-level opinions, then:
- Let `diesel-heater.yaml` (and any future composite device) include the thin package.
- Optionally refactor `bluetooth-proxy.yaml` to include the same thin package (eliminates
  duplication).
- Leave all existing `bluetooth-proxy-*.yaml` device configs **unchanged**.

---

## Files to Create or Modify

### 1. CREATE `packages/ble-proxy.yaml` — thin BLE-only package

Create the file with **only** the BLE tracker and proxy components. No framework, no
esphome block, no api, no ota, no wifi:

```yaml
---
# Composable BLE Proxy Package
# Adds Bluetooth proxy functionality to any ESP32 device.
# Include this package from a device config that already has esp32:, api:, wifi:, etc.
# Works with both framework: arduino and framework: esp-idf.
#
# No substitutions required.

esp32_ble_tracker:
  scan_parameters:
    active: true

bluetooth_proxy:
  active: true
```

### 2. MODIFY `diesel-heater.yaml` — uncomment and re-target the bt_proxy line

Current state (line 56):
```yaml
  # bt_proxy: !include packages/device-configs/bluetooth-proxy.yaml
```

Change to:
```yaml
  bt_proxy: !include packages/ble-proxy.yaml
```

No other changes to `diesel-heater.yaml` are needed. The existing `framework: arduino`
is compatible with `esp32_ble_tracker` and `bluetooth_proxy`.

### 3. OPTIONAL: Refactor `packages/device-configs/bluetooth-proxy.yaml`

This step eliminates the duplication of `esp32_ble_tracker:` and `bluetooth_proxy:`
between the new thin package and the full device package. It is **not required** for the
diesel-heater use case and does not affect dedicated proxy devices.

Remove these blocks from `packages/device-configs/bluetooth-proxy.yaml`:
```yaml
esp32_ble_tracker:
  scan_parameters:
    active: true

bluetooth_proxy:
  active: true
```

And replace with a package include inside the existing `packages:` block:
```yaml
packages:
  wifi: !include ../wifi.yaml
  wifi-signal-sensors: !include ../wifi-signal-sensors.yaml
  internaltemp-sensor: !include ../internal-temp-sensor-esp32.yaml
  ble: !include ../ble-proxy.yaml    # ← add this line
```

Existing dedicated proxy devices (`bluetooth-proxy-buro.yaml`,
`bluetooth-proxy-bedroom.yaml`, etc.) include `bluetooth-proxy.yaml` as their primary
package and require **no changes at all**.

---

## What Does NOT Need to Change

- `bluetooth-proxy-buro.yaml`
- `bluetooth-proxy-wr.yaml`
- `bluetooth-proxy-bedroom.yaml`
- `bluetooth-proxy-lr.yaml`
- `bluetooth-proxy-bm-2.yaml`
- `bluetooth-proxy-kitchen.yaml`

All of the above continue using `packages/device-configs/bluetooth-proxy.yaml` unchanged.

---

## Framework Compatibility Note

`esp32_ble_tracker` and `bluetooth_proxy` work with both `framework: arduino` and
`framework: esp-idf`. The dedicated proxy devices use `esp-idf` (better BLE efficiency
for a single-purpose device). The diesel-heater uses `arduino` (required by the bundled
CC1101 library). Both are valid — the thin package has no framework opinion.

---

## BLE + RF Coexistence Note

The CC1101 is an **external** 433 MHz transceiver communicating over SPI. It does not
use the ESP32's built-in radio, so there is no radio-level conflict with BLE.

The `getState()` polling loop in `DieselHeaterRF.cpp` already calls `yield()` inside its
GDO2 spin-wait, allowing the FreeRTOS BLE task to run during RF receive windows:

```cpp
// DieselHeaterRF.cpp — receivePacket()
while (!digitalRead(_pinGdo2)) {
  if (millis() - t > timeout) return false;
  yield();  // lets FreeRTOS schedule BLE and WiFi tasks
}
```

No further changes to the component are needed for BLE coexistence.

---

## Verification Steps

After implementing:

1. Run `esphome compile diesel-heater.yaml` — must succeed with no errors.
2. Confirm `bluetooth_proxy` and `esp32_ble_tracker` appear in the generated
   `.esphome/build/diesel-heater/src/main.cpp`.
3. If step 3 (optional refactor) was done, also compile one of the dedicated proxy
   devices to confirm it still works:
   ```bash
   esphome compile bluetooth-proxy-buro.yaml
   ```
4. Flash `diesel-heater.yaml` and confirm the device appears in Home Assistant as both
   a diesel heater controller and a Bluetooth proxy.
