---
name: Smart electric meter — verified hardware configuration
description: Verified working hardware setup for the Logarex LK13BE 803039 + Wemos D1 Mini reader, including pullup value and IR head modification.
type: project
originSessionId: e9d0e29d-d32e-4af5-8726-cb9260a46237
---
The smart-electric-meter device (Wemos D1 Mini, ESPHome, package
`packages/device-configs/electricity-meter.yaml`) reads a Logarex
LK13BE 803039 via a Stromleser serial IR head (micro-USB connector type)
on D2 (GPIO4) using software UART at 9600 7E1 (correct for this meter,
push-mode passive ASCII OBIS).

The verified working hardware configuration after the April 2026 weak-signal
debugging cycle is:

- **External 10 kΩ pullup** RX (D2) → 3.3 V — present since original build,
  never changed.
- **Photodiode load resistor on the IR head modified from stock ~13 kΩ
  (marked `12C`) to 33 kΩ (marked `333`).** This was the only change that
  resolved the recent weak-signal failure.
- **ESP8266 internal pullup OFF** (`mode: pullup: true` not set in YAML).
  Tested permutations: stock head + 47 kΩ external = no signal; stock
  head + no pullup = no signal and possible boot trouble.

**Why:** the meter is reported in the German simon42 forum thread
"Logarex LK13BE 803039 IR-Sendeschwach" to have a marginal IR transmitter.
The head load-resistor swap is the documented community fix. On this build
the same hardware ran reliably for years before degrading; root cause of
the degradation was not isolated (could be meter LED aging, head
degradation, or optical contamination).

**How to apply:** if the meter starts failing again, do not touch the
ESP-side wiring (10 kΩ external pullup is correct). Investigate optics
first (clean both surfaces, re-seat). If still failing on a stock head,
the resistor swap is the documented fix. Full procedure is in
`docs/electricity-meter-weak-ir-fixes.md`.

**Do NOT:** enable the YAML internal pullup while the external 10 kΩ is
fitted (untested combination), and do NOT remove the external pullup
(line floats, no signal, and the device may fail to boot cleanly).
