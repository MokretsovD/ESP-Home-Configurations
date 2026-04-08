# CC1101 + ESP32 RF Module Reference

Hardware: TI CC1101 transceiver module (e.g. Ebyte E07-M1101S) connected to
ESP32 via SPI, sharing the 3.3V rail with WiFi. This document captures confirmed
behaviours from extensive testing and debugging.

## Power Supply

The CC1101 module draws from the ESP32 3.3V regulator, which also powers WiFi.
This shared rail is the single biggest source of reliability problems.

**Confirmed behaviours:**

- WiFi TX causes 3.3V rail droops that can brownout-reset the CC1101. Observed
  as registers reverting to power-on defaults (SYNC1=0xD3, FREQ2=0x1E, IOCFG2=0x29).
- RX calibration (~721 us per datasheet) is the most vulnerable phase. During TX,
  tight SPI polling loops maintain steady current draw that stabilises the rail.
  During RX calibration, no SPI activity occurs — the rail is free to droop.
- WiFi roam scans, reconnects, DHCP, and API handshakes all cause sustained TX
  bursts on the WiFi side.
- ESPHome `fast_connect: true` does NOT prevent periodic WiFi roam scans.
- A 25-second settle period after boot (WiFi connect, DHCP, mDNS, API handshake)
  before first RF activity significantly improves initial reliability.
- Deferred sensor publishing (publish when RF is idle, with a settle delay before
  next RF operation) reduces WiFi/RF overlap.

**Software mitigations:**

- WiFi event tracking (`SCAN_DONE`, `STA_CONNECTED`, `STA_DISCONNECTED`) with
  settle delays before RF operations.
- Publish settle delay (~100 ms) after sensor state pushes before next RF.
- Entering RX from FSTXON (synthesiser already locked) instead of from IDLE
  (requires full recalibration). This eliminates the vulnerable calibration window.

**Hardware mitigations:**

- **Bulk decoupling capacitor (highest impact, lowest cost):** Add a 47-100 uF
  electrolytic or 22-47 uF ceramic (X5R/X7R) directly at the CC1101 module's
  VCC/GND pins. Cheap modules typically include the per-pin 100 nF ceramics
  required by the CC1101 datasheet reference design but omit the bulk capacitor.
- **Ferrite bead isolation:** A 600 ohm @ 100 MHz ferrite bead (DC resistance
  < 0.5 ohm, rated > 100 mA) in series with VCC between the ESP32 rail and the
  CC1101 module. Must be paired with a bulk capacitor on the module side — without
  it, the bead's impedance prevents the CC1101 from drawing current quickly,
  making things worse.
- **Dedicated LDO regulator (best long-term fix):** A separate low-noise 3.3V
  LDO fed from the 5V rail (not from the already-regulated 3.3V) to power the
  CC1101 independently. The CC1101 draws up to ~34 mA peak, so a 100-150 mA LDO
  is sufficient. Low output noise (< 50 uV RMS) and high PSRR (> 60 dB @ 1 MHz)
  are preferred for RF applications. Place 100 nF + 10-47 uF on the LDO output.
  This replaces the ferrite bead approach.
- **Short wiring:** Keep VCC/GND wires under 5 cm. Long wires act as inductors
  and increase susceptibility to rail noise. Use a dedicated GND return path from
  the CC1101 module — don't daisy-chain through other peripherals.
- **ESP32 side bulk capacitor:** If the ESP32 dev board lacks adequate bulk
  capacitance (common on cheap boards), add 100-470 uF electrolytic on the
  ESP32's 3.3V rail to reduce voltage dips during WiFi TX.

## SPI Configuration

**SPI speed: use 1 MHz, not 4 MHz.**

- CC1101 datasheet maximum SCLK: 10 MHz (single access with inter-byte delay),
  6.5 MHz (burst access without inter-byte delay).
- At 4 MHz on a shared 3.3V rail, TX reliability became sensitive to unrelated
  code changes (adding/removing log statements changed success rate from 0% to
  90%). At 1 MHz this sensitivity disappeared.
- ESPHome's official CC1101 component uses 1 MHz.
- 1 MHz adds ~3 ms to `reinitRadio()` (40+ register writes) and ~1.4 ms to a
  14-packet burst. Both are negligible given existing delays.

**SPI driver notes (ESP-IDF):**

- Hardware CS cannot be used because CC1101 requires chip-ready detection (MISO
  LOW after CS assertion) before the first byte. Use `spics_io_num = -1` with
  manual CS in `pre_cb`/`post_cb`.
- MISO needs a pull-up (GPIO_PULLUP_ONLY) so it idles HIGH. Without it, MISO
  floats LOW and the chip-ready wait in `pre_cb` never triggers.
- After a strobe-only SPI transaction, the next register read may return invalid
  data if `SPI_TRANS_USE_RXDATA` was not set on the strobe transaction. Always
  set `SPI_TRANS_USE_RXDATA` on strobes to keep SPI_USR_MISO enabled.
- Similarly, burst writes should use a non-NULL `rx_buffer` (even if discarded)
  to keep MISO enabled for subsequent reads.

## CC1101 State Machine

### FSTXON State

After a TX burst with `TXOFF_MODE=FSTXON`, the synthesiser remains calibrated
and locked. From this state:

- **SRX** enters RX directly without calibration.
- **STX** enters TX directly without calibration.
- **Config register writes are forbidden.** The CC1101 allows config writes in
  IDLE, XOFF, or SLEEP states only. Writing registers (e.g. MCSM1) while in
  FSTXON corrupts the state machine. Observed: MARCSTATE=0x00 after SRX,
  degraded TX on subsequent bursts.
- **SIDLE** kills the synthesiser lock. Use only when full recalibration is
  acceptable (error recovery, health checks).

### MARCSTATE Reads

- Reading MARCSTATE immediately after a strobe (SRX, STX) returns stale data.
  The CC1101 state machine has not yet transitioned. A read within ~1 ms of SRX
  always returns 0x00 even on successful cycles.
- MARCSTATE reads during RX/TX return the STATUS byte, not the register value.
  Only read MARCSTATE (and other config registers) when the chip is in IDLE.
  Reading during non-IDLE states produces false positives (e.g. SYNC1=0xD3 which
  is actually a status byte, not register corruption).
- MARCSTATE register base address is 0x35. To read it via SPI, use 0xF5
  (0x35 | 0xC0 burst bit) because addresses 0x30-0x3D overlap between command
  strobes and status registers — the burst bit disambiguates.

### Calibration Behaviour

- From IDLE, SRX triggers calibration (~721 us per datasheet) before entering RX.
  MARCSTATE sequence: IDLE (0x01) → STARTCAL (0x08) → BWBOOST (0x09) →
  FS_LOCK (0x0A) → IFADCON (0x0B) → ENDCAL (0x0C) → RX (0x0D).
- From FSTXON, SRX goes directly to RX (no calibration).
- Manual calibration (SCAL strobe 0x33) works correctly but interferes with the
  auto-calibration that occurs during IDLE → TX transitions — inserting SCAL
  before a TX burst increased partial burst failures.
- VCO calibration failure manifests as TX at the wrong frequency (visible on
  waterfall as thin bursts shifted to 434.0-434.1 MHz instead of 433.92 MHz) or
  extremely wideband emissions (VCO sweeping without lock, 433.5-435.5 MHz).
  MARCSTATE polling reports normal TX/FSTXON transitions despite invalid RF output.

### Register Corruption Detection

- Periodic SYNC1 register read (expected 0x7E) while in IDLE detects brownout
  resets. Add IOCFG2 (expected 0x07), FREQ2, FREQ1, MDMCFG4 for completeness.
- Only perform register checks when MARCSTATE = 0x01 (IDLE). Non-IDLE reads
  return status bytes, causing false-positive corruption detection.
- On corruption: `reinitRadio()` (full register rewrite) restores operation.
  A 136 ms delay within `initRadio()` provides VCC settling time.

## TX Burst Protocol

### Packet Structure

- TX packet: 10 bytes — `[0x09 (len), cmd, addr[3:0], seq, crc_hi, crc_lo, 0x00]`
- RX response: 26 bytes — 23 data + 2 CC1101 APPEND_STATUS (RSSI, LQI|CRC_OK) + length byte
- CRC: CRC-16/MODBUS over first 7 bytes of TX packet

### Command Types

- **GET_STATUS (0x23)**: Status poll / keep-alive. Heater responds with state packet.
- **POWER (0x2B)**, **MODE (0x24)**: Toggle commands. Each unique sequence number
  fires exactly one toggle. Retransmitting the same seq does NOT re-toggle.
- **UP (0x3C)**, **DOWN (0x3E)**: Accumulator commands. Heater applies the most
  recent direction once per burst. 14-packet bursts improve hit probability
  against WOR sleep windows.

### Burst Behaviour

- 14 packets per burst provides >90% probability of hitting the heater's WOR
  listen window. Fewer packets significantly reduce success rate.
- The heater responds to any valid command regardless of WOR sleep state — no
  explicit wake sequence is required.
- The heater may respond multiple times per burst (observed RXBYTES accumulating
  >26 bytes — multiple response packets).

### Retransmit Strategy

- `reinitRadio()` before each retransmit burst is required. Without it, VCO
  calibration fails on retransmits — RF emits at wrong frequencies or not at all,
  despite MARCSTATE showing normal TX transitions.
- A 50 ms delay before `reinitRadio()` provides VCC settling time.
- Excessive retransmits (10 x 14 = 140 packets) stress the 3.3V rail and degrade
  subsequent TX/RX reliability. Limit retransmit cascades and use exponential
  backoff for offline probing.

## RX Reception

### GDO2 vs RXBYTES

- IOCFG2=0x07 configures GDO2 to assert when a packet is received with CRC OK
  and valid address (de-asserts when first byte is read from RX FIFO).
- ~50% of valid responses arrive with GDO2=0 but RXBYTES=26. These are genuine
  hardware CRC failures — the packet data is in the FIFO but CRC_OK bit is not
  set. WiFi TX during the RX window likely corrupts bits.
- A periodic RXBYTES fallback check (~200 ms interval via SPI) catches packets
  that GDO2 misses. This doubled effective RX success rate.
- RXBYTES >= 64 indicates RXFIFO overflow — flush and restart RX.

### RXFIFO Management

- Flush RX FIFO before each TX burst (SFRX in IDLE) to prevent stale data from
  interfering.
- After RXFIFO overflow, the chip enters a state where subsequent TX from IDLE
  can succeed (chip transitions through FS_WAKEUP 0x02).
- Reading from an empty FIFO triggers RXFIFO underflow. Check GDO2 or RXBYTES
  before reading.

### Heater Response Timing

- Heater responds within ~100-200 ms of a TX burst (confirmed via RTL-433).
- A 1-second RX window provides adequate margin.
- Minimising the gap between TX burst completion and RX entry is critical. Even
  5-10 ms of dead time significantly reduces success rate.

## RX Response Packet Format

Byte offsets within the 26-byte RX FIFO data:

| Byte | Field | Notes |
|------|-------|-------|
| 0 | Length | 0x17 (23 data bytes) |
| 1 | Command echo | |
| 2-5 | Address | 4 bytes, big-endian |
| 6 | State | 0x00=Off, 0x01=Startup, 0x02=Warming, 0x03=Warming Wait, 0x04=Pre-Run, 0x05=Running, 0x06=Shutdown, 0x07=Shutting Down, 0x08=Cooling |
| 7 | Power level | Heat output level |
| 8 | Error code | 0x00=None, 0x01=Overvoltage, 0x02=Undervoltage, 0x03=Glow plug, 0x04=Pump, 0x05=Overheat, 0x06=Motor, 0x07=Comms, 0x08=Flame out |
| 9 | Voltage | Raw / 10.0 = volts |
| 10 | Ambient temp | Signed, degrees C |
| 11 | Unknown | |
| 12 | Case temp | Degrees C |
| 13 | Setpoint | Temperature target (auto mode) |
| 14 | Auto mode | 0x32 = auto (thermostat), other = manual |
| 15 | Pump frequency | Raw / 10.0 = Hz |
| 16-23 | Unknown | |
| 24 | RSSI | CC1101 APPEND_STATUS byte 1 |
| 25 | LQI / CRC_OK | CC1101 APPEND_STATUS byte 2: bit 7 = CRC_OK, bits [6:0] = LQI |

Note: Error codes sourced from community reports (jakkik Issue #12). The mapping
has not been verified against all real error conditions.

## CC1101 Power-On Reset Defaults

These register values indicate the chip has been brownout-reset:

| Register | Default | Configured |
|----------|---------|------------|
| SYNC1 (0x04) | 0xD3 | 0x7E |
| FREQ2 (0x0D) | 0x1E | 0x10 (433 MHz) |
| IOCFG2 (0x00) | 0x29 | 0x07 |

## ESP32 Integration Notes

### WiFi Interference Window

The WiFi quiet guard only protects command dispatch (before TX), not the RX
listen window. WiFi can burst freely during RX_LISTEN, causing bit corruption
in received packets (manifests as hardware CRC failures with valid RXBYTES count).

### Main Loop Stalls

The ESPHome main loop has been observed stalling for 2-10 seconds (cause
unknown — possibly higher-priority FreeRTOS tasks, flash operations, or logger
blocking). During retransmit cascades, this causes multiple RX windows to expire
instantly when the loop resumes — burning retransmit budget without any actual
RX listen time.
