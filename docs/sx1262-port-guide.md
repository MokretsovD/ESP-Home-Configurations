# Porting DieselHeaterRF to SX1262 (LILYGO T3-S3 V1.3)

Reference guide for implementing `DieselHeaterSX1262.h/.cpp` — a drop-in replacement
for `DieselHeaterRF.h/.cpp` targeting the Semtech SX1262 transceiver instead of the
TI CC1101. Written after analysis session on 2026-03-18. Do not repeat this research
from scratch — read this document first.

---

## Target Hardware

### RF Chip
**Semtech SX1262** — Sub-GHz LoRa / FSK transceiver, 150–960 MHz, 22 dBm max TX power.
Supports LoRa and (G)FSK/OOK modulation. This port uses **GFSK mode only** — LoRa is
not used and not compatible with the heater protocol.

- Datasheet: https://semtech.com/products/wireless-rf/lora-connect/sx1262
- Key document: SX1262 Datasheet Rev 2.1 (DS.SX1262.W.APP, available from Semtech)
- SPI interface: 4-wire SPI + dedicated BUSY pin + DIO1 IRQ pin + RST

### Development Board
**LILYGO T3-S3 V1.3** (also written as LILYGO T-S3 V1.3 in listings)
- Full product name: LILYGO T3-S3 V1.3 LoRa Platine ESP32-S3 868MHz MeshCore IoT Modul
- MCU: **ESP32-S3** (not ESP32 or ESP32-C3)
- RF chip: SX1262 (confirmed from LILYGO GitHub source; verify PCB markings on receipt)
- Crystal: 32 MHz (both ESP32-S3 and SX1262 use 32 MHz XTAL)
- Available frequency variants: 868 MHz, 915 MHz (hardware identical, software differs)
- 433 MHz variant does NOT exist for this board

### LILYGO T3-S3 V1.3 GPIO Pin Assignments (SX1262)

| Signal   | GPIO | Notes |
|----------|------|-------|
| SCK      | 5    | SPI clock |
| MISO     | 3    | SPI data out from SX1262 |
| MOSI     | 6    | SPI data in to SX1262 |
| NSS (CS) | 7    | SPI chip select, active LOW |
| DIO1     | 1    | IRQ output: RxDone, TxDone, Timeout |
| RST      | 8    | Hardware reset, active LOW |
| BUSY     | 13   | Busy indicator, must be LOW before SPI |
| DIO2     | 48   | Optional (not needed for this port) |
| DIO3     | 2    | Optional (not needed for this port) |

Source: LilyGo-LoRa-Series GitHub repository, utilities.h
https://github.com/Xinyuan-LilyGO/LilyGo-LoRa-Series

**Verify these pins from the actual T3-S3 V1.3 schematic before implementing.**
The schematic is in the repository above under `schematic/`.

---

## Why SX1262 and Not Another CC1101

The original component (`DieselHeaterRF`) targets the **TI CC1101** transceiver.
The CC1101 is a 300–928 MHz FSK/OOK chip using a simple 2-byte register-write SPI
protocol.

The VEVOR heater remote is believed to operate at **868.3 MHz** (EU ISM band) based on:
1. User is in Europe
2. Transceiver tests at 433 MHz yielded zero heater packets despite correct CC1101 health
3. The AS07-M1101S module used for testing is 433 MHz — its matching network and PCB
   antenna are physically tuned for 433 MHz and cannot receive 868 MHz signals even if
   the CC1101 chip registers are reconfigured

A replacement 868 MHz module could be a CC1101-based one (e.g. Ebyte E07-868MS10,
direct drop-in, no code changes needed beyond `heater_frequency: "868"`). The SX1262
path is chosen for integration into a single ESP32-S3 board with no external module.

---

## Protocol Parameters Derived from CC1101 Registers

The following values were mathematically derived from the CC1101 register values
hardcoded in `DieselHeaterRF.cpp → initRadio()`. All formulas are from the
**CC1101 datasheet (SWRS061I)**, available at https://www.ti.com/product/CC1101.

CC1101 uses a **26 MHz** crystal (f_XOSC = 26,000,000 Hz).

### Data Rate

```
Register:  MDMCFG4 = 0xF8 → DRATE_E = bits[3:0] = 8
           MDMCFG3 = 0x93 → DRATE_M = 0x93 = 147

Formula:   R_data = f_XOSC × (256 + DRATE_M) / 2^(28 - DRATE_E)
         = 26,000,000 × (256 + 147) / 2^20
         = 26,000,000 × 403 / 1,048,576
         ≈ 9,992 bps (~10 kbps)
```

> **Note:** Some sources describe this heater protocol as "4800 bps". This is incorrect.
> The register values compute unambiguously to ~10 kbps. Verify with SDR capture.

### Frequency Deviation

```
Register:  DEVIATN = 0x26
           DEVIATION_E = bits[6:4] = 2
           DEVIATION_M = bits[2:0] = 6

Formula:   f_dev = f_XOSC / 2^17 × (8 + DEVIATION_M) × 2^DEVIATION_E
         = 26,000,000 / 131,072 × (8 + 6) × 4
         = 198.364 × 14 × 4
         ≈ 11,108 Hz (~11.1 kHz)
```

### Channel Bandwidth (RX filter)

```
Register:  MDMCFG4 = 0xF8
           CHANBW_E = bits[7:6] = 3
           CHANBW_M = bits[5:4] = 3

Formula:   BW = f_XOSC / (8 × (4 + CHANBW_M) × 2^CHANBW_E)
         = 26,000,000 / (8 × 7 × 8)
         = 26,000,000 / 448
         ≈ 58,036 Hz (~58 kHz)
```

### Modulation

```
Register:  MDMCFG2 = 0x13
           MOD_FORMAT = bits[6:4] = 001 = GFSK
           SYNC_MODE  = bits[2:0] = 011 = 30/32 sync bits required
           MANCHESTER_EN = bit[3] = 0 (disabled)
```

Gaussian shaping parameter (BT) is not directly encoded in CC1101 — it uses a fixed
internal BT=0.5 for GFSK. Use SX1262 PulseShape = BT_0_5.

### Carrier Frequency

```
Register:  FREQ2 = 0x21, FREQ1 = 0x6E, FREQ0 = 0x6A (for 868 MHz variant)

Formula:   f = (FREQ2 × 2^16 + FREQ1 × 2^8 + FREQ0) × f_XOSC / 2^16
         = (0x216E6A) × 26,000,000 / 65,536
         = 2,191,978 × 396.73...
         ≈ 868,300,000 Hz = 868.3 MHz
```

### Sync Word

```
Register:  SYNC1 = 0x7E, SYNC0 = 0x3C → combined: 0x7E3C (big-endian)
```

### Preamble Length

```
Register:  MDMCFG1 = 0x22 → NUM_PREAMBLE = bits[6:4] = 010 = 4 bytes
           4 bytes × 8 bits = 32 preamble bits (alternating 0xAA pattern)
```

### Packet Format

```
Register:  PKTCTRL0 = 0x05
           WHITE_DATA     = bit[6] = 0 → data whitening OFF
           PKT_FORMAT     = bits[5:4] = 00 → normal FIFO mode
           CRC_EN         = bit[2] = 1 → CC1101 hardware CRC enabled
           LENGTH_CONFIG  = bits[1:0] = 01 → variable length mode

Register:  PKTCTRL1 = 0x04
           CRC_AUTOFLUSH  = bit[3] = 0 → keep packet in FIFO on CRC fail
           APPEND_STATUS  = bit[2] = 1 → CC1101 appends 2 status bytes
           ADR_CHK        = bits[1:0] = 00 → no address filtering
```

> **Important:** CC1101 appends 2 status bytes (RSSI + CRC_OK|LQI) to every received
> packet in the FIFO. This is why `receivePacket()` expects 24 bytes total:
> 1 (length) + 19 (payload) + 2 (CC1101 hardware CRC) + 2 (appended status) = 24.
> SX1262 does NOT append status bytes. The received buffer will be 22 bytes (raw packet
> only). The RSSI must be read separately via `GetPacketStatus` command.

### CRC Algorithm

The library uses **software CRC** (function `crc16_2`), not CC1101 hardware CRC,
despite `CRC_EN=1` in PKTCTRL0. The heater embeds its own application-level CRC
at bytes [19:20] of the 22-byte packet. The `crc16_2` function implements
**CRC-16/MODBUS**: polynomial 0x8005, initial value 0xFFFF, input and output reflected.

For SX1262: **disable hardware CRC** (`CRCType = 0x00` in SetPacketParams). Keep the
existing `crc16_2` software CRC unchanged. This is the simplest approach and avoids
questions about SX1262 CRC seed/polynomial register compatibility with CRC-16/MODBUS.

---

## SX1262 Register / Command Values (Computed)

SX1262 uses a **32 MHz** crystal (f_XTAL = 32,000,000 Hz).
All frequency-related register values differ from CC1101 due to the different crystal.

### SetRfFrequency — 868.3 MHz

```
RFfreq = round(868,300,000 × 2^25 / 32,000,000)
       = round(868,300,000 × 1.048576)
       = round(910,541,107.2)
       = 0x364CA433  (verify: 0x364CA433 × 32e6 / 2^25 = 868.30 MHz ✓)
```

### SetModulationParams (FSK mode)

```
Param 1-3: BitRate    = round(32,000,000 / 9,992) = 3203 = 0x000C83
Param 4:   PulseShape = 0x09 (GFSK BT=0.5)
Param 5:   Bandwidth  = 0x12 (58.6 kHz — closest above 58 kHz)
Param 6-8: FreqDev    = round(11,108 × 2^25 / 32,000,000)
                      = round(11,108 × 1.048576)
                      = round(11,648.1)
                      = 11,648 = 0x002D80
```

SX1262 bandwidth register encoding (Param 5), for reference:

| Code | BW (kHz) |
|------|----------|
| 0x0C | 19.5 |
| 0x11 | 46.9 |
| **0x12** | **58.6** ← use this |
| 0x14 | 78.2 |
| 0x16 | 117.3 |

### SetPacketParams (FSK mode)

```
Param 1-2: PreambleLength  = 0x0020 (32 bits = 4 bytes)
Param 3:   PreambleDetect  = 0x00   (no minimum preamble detector threshold)
Param 4:   SyncWordLength  = 0x10   (2 bytes = 16 bits; SX1262 encodes as num_bits/8 - 1 = 1... verify from datasheet)
Param 5:   AddrComp        = 0x00   (no address filtering)
Param 6:   PacketType      = 0x01   (variable length)
Param 7:   PayloadLength   = 0xFF   (max; ignored in variable length mode)
Param 8:   CRCType         = 0x00   (no hardware CRC — use software CRC)
Param 9:   Whitening       = 0x00   (data whitening OFF)
```

> **SyncWordLength encoding must be verified from the SX1262 datasheet Table 13-52.**
> The encoding is non-obvious. Using RadioLib as a cross-reference is recommended.

### Sync Word Register

The sync word is written to SX1262 registers 0x06C0–0x06C7 (8-byte sync word buffer).
Write `0x7E 0x3C` to registers 0x06C0–0x06C1. Remaining bytes (0x06C2–0x06C7) unused.

```c
writeRegister(0x06C0, 0x7E);
writeRegister(0x06C1, 0x3C);
```

### SetDioIrqParams

Map DIO1 to RxDone + RxTimeout + TxDone:
```
IRQ mask:  0x0203 (TxDone=bit1, RxDone=bit2, Timeout=bit9 → 0x0203)
DIO1 mask: 0x0203
DIO2 mask: 0x0000
DIO3 mask: 0x0000
```

---

## Key Implementation Differences from CC1101

### 1. BUSY Pin — mandatory before every SPI transaction

```cpp
void spiStart() {
    while (digitalRead(_pinBusy));  // wait for SX1262 ready
    digitalWrite(_pinNss, LOW);
}
```

CC1101 uses MISO for chip-ready. SX1262 uses a dedicated BUSY pin. Sending any SPI
command while BUSY is HIGH causes silent failure. This is the single most common
implementation mistake with SX1262.

### 2. Hardware Reset on startup

```cpp
void reset() {
    digitalWrite(_pinRst, LOW);
    delay(2);
    digitalWrite(_pinRst, HIGH);
    delay(10);
    while (digitalRead(_pinBusy));
}
```

CC1101 uses a `SRES` strobe (0x30). SX1262 requires a physical pin reset.
A new `_pinRst` member is needed.

### 3. SPI command protocol

CC1101: `[addr_byte][data_byte]` — simple 2-byte exchange.
SX1262: `[opcode][addr_high][addr_low][data...]` for registers, or `[opcode][params...]`
for commands. Key opcodes:

| Opcode | Command |
|--------|---------|
| 0x80 | SetStandby |
| 0x01 | SetPacketType (0x00=GFSK, 0x01=LoRa) |
| 0x86 | SetRfFrequency |
| 0x8B | SetModulationParams |
| 0x8C | SetPacketParams |
| 0x8F | SetBufferBaseAddress |
| 0x83 | SetTx |
| 0x82 | SetRx |
| 0x0D | WriteRegister |
| 0x1D | ReadRegister |
| 0x0E | WriteBuffer |
| 0x1E | ReadBuffer |
| 0x12 | GetIrqStatus |
| 0x02 | ClearIrqStatus |
| 0x13 | GetRxBufferStatus (returns payload length + buffer offset) |
| 0x14 | GetPacketStatus (returns RSSI, SNR) |
| 0x02 | SetDioIrqParams (same opcode as Clear — verify) |

> Always cross-reference opcodes against SX1262 datasheet Table 13-1 before
> implementing. Do not rely solely on this document for opcode values.

### 4. DIO1 replaces GDO2

```cpp
// CC1101 (original):
while (!digitalRead(_pinGdo2)) { ... }

// SX1262 port:
while (!digitalRead(_pinDio1)) { ... }
```

After reception: read IRQ status to confirm RxDone vs Timeout.

### 5. Packet buffer size changes

CC1101 packet in FIFO = 24 bytes (includes 2 appended status bytes).
SX1262 packet in buffer = 22 bytes (raw packet, no appended status).

The `receivePacket()` check must change:
```cpp
// CC1101 (original):
if (rxLen == 24) break;

// SX1262 port:
if (rxLen == 22) break;
```

And the RSSI value (currently read from `buf[22]`) must be obtained separately:
```cpp
// After ReadBuffer:
uint8_t rssiPkt, snrPkt, signalRssiPkt;
getPacketStatus(&rssiPkt, &snrPkt, &signalRssiPkt);
// SX1262 RSSI in dBm = -rssiPkt / 2
```

The caller (`DieselHeaterRFComponent::update()` in `diesel_heater_rf.cpp`) reads RSSI
from `state.rssi`. The `getState()` function signature is unchanged — just the internal
RSSI source changes.

### 6. CRC byte positions

With SX1262 (no appended status bytes), the packet layout shifts:
```
buf[0]     = length byte
buf[1..18] = 18 bytes of heater payload data
buf[19..20] = CRC-16/MODBUS (heater's software CRC) — verified by crc16_2()
buf[21]    = unknown / padding
```
Total = 22 bytes. `crc16_2(bytes, 19)` check and offset `buf[19..20]` remain the same.

---

## What Does NOT Change

- All public method signatures: `begin()`, `begin(uint32_t)`, `getState()`,
  `sendCommand()`, `findAddress()`, `getPartNum()`, `getVersion()`, `receiveRaw()`
- `crc16_2()` implementation (CRC-16/MODBUS, unchanged)
- `DieselHeaterRFComponent` ESPHome component — zero changes required
- All YAML configuration, substitutions, HA entities, buttons, switches
- The `heater_frequency: "868"` substitution already in `diesel-heater.yaml`

---

## ESPHome Config Changes Required

### board
```yaml
# Before (ESP32):
esp32:
  board: esp32dev
  framework:
    type: arduino

# After (ESP32-S3):
esp32:
  board: esp32s3box          # or lilygo-t3s3 if available in ESPHome
  framework:
    type: arduino
```

### New substitutions (replace heater_* SPI pins)
```yaml
substitutions:
  heater_sck_pin:  "5"
  heater_miso_pin: "3"
  heater_mosi_pin: "6"
  heater_cs_pin:   "7"
  heater_dio1_pin: "1"    # replaces heater_gdo2_pin
  heater_rst_pin:  "8"    # new
  heater_busy_pin: "13"   # new
  heater_frequency: "868"
```

### Component name
The new component should be named `diesel_heater_sx1262` to coexist with the
existing `diesel_heater_rf` component. The `__init__.py`, CONF_ constants, and
`packages/device-configs/diesel-heater-sx1262.yaml` should mirror the existing
CC1101 package structure.

---

## Recommended Implementation Approach

### Phase 1 — Validate modulation parameters before writing full library

Before implementing the complete port, verify the derived modulation parameters
(data rate, deviation, bandwidth) against an actual SDR capture:

1. Capture heater remote button presses with RTL-SDR at 868.3 MHz using URH
2. Measure the actual baud rate from the bit timing in the demodulated waveform
3. Confirm deviation from the FSK frequency spread in the spectrogram
4. If values match the table above, proceed with implementation
5. If not, re-derive register values from the measured values

See `docs/rf-protocol-reverse-engineering.md` for the SDR capture procedure.

### Phase 2 — Implement DieselHeaterSX1262 class

Create:
- `components/diesel_heater_rf/DieselHeaterSX1262.h`
- `components/diesel_heater_rf/DieselHeaterSX1262.cpp`

These implement the same public interface as `DieselHeaterRF.h/.cpp` but target SX1262.

All tunable modulation constants should be `#define` at the top of the .cpp file
so they can be adjusted empirically without touching protocol logic:

```cpp
#define SX1262_FREQ_HZ       868300000UL
#define SX1262_BITRATE_REG   0x000C83   // ~9992 bps
#define SX1262_FREQDEV_REG   0x002D80   // ~11.1 kHz
#define SX1262_BW_REG        0x12       // 58.6 kHz
#define SX1262_PULSESHAPE    0x09       // GFSK BT=0.5
#define SX1262_PREAMBLE_LEN  32         // bits (4 bytes)
```

### Phase 3 — ESPHome component wiring

Add `gdo2_pin` → rename to `dio1_pin` in `__init__.py`, add `rst_pin` and `busy_pin`
as new `cv.Optional` schema entries. Keep `gdo2_pin` as a deprecated alias with
a warning to minimise breaking changes to existing CC1101 configs.

### Phase 4 — New device package

Create `packages/device-configs/diesel-heater-sx1262.yaml` mirroring
`diesel-heater-rf.yaml` but with the T3-S3 pin defaults and `heater_frequency: "868"`.

---

## Recommended Libraries for Reference

When implementing, cross-reference against:

1. **RadioLib** — https://github.com/jgromes/RadioLib
   The most complete SX1262 Arduino implementation. Study `SX126x.cpp` for command
   sequences, BUSY handling, and FSK mode setup. Do NOT copy-paste — the library has
   LoRa overhead not needed here, but the SX1262 command patterns are correct.

2. **arduino-LoRa** — https://github.com/sandeepmistry/arduino-LoRa
   Simpler, SX1276-focused but useful for comparison.

3. **LilyGo-LoRa-Series examples** — https://github.com/Xinyuan-LilyGO/LilyGo-LoRa-Series
   Board-specific SX1262 examples for the T3-S3. Pin assignments confirmed from here.

4. **SX1262 datasheet** — Rev 2.1 from Semtech, section 13 (command interface),
   section 6 (FSK modem), Table 13-52 (SetPacketParams encoding).

5. **CC1101 datasheet** — SWRS061I from TI. All CC1101 formulas used in this document
   are from this source. Keep it on hand for cross-referencing the original register
   values.

---

## Known Unknowns (Must Verify Before Finalising)

| Item | Uncertainty | How to resolve |
|------|-------------|----------------|
| Actual heater data rate | Register math gives ~10 kbps; some sources say 4800 bps | SDR capture + URH bit timing measurement |
| Sync word on 868 MHz variant | Assumed 0x7E3C same as 433 MHz; not confirmed | SDR capture, look for repeating 16-bit pattern after preamble |
| SyncWordLength SX1262 encoding | Non-obvious encoding in SetPacketParams param 4 | SX1262 datasheet Table 13-52; RadioLib source `SX126x.cpp` |
| Exact packet length (22 vs other) | Assumed 22 = 24 - 2 appended status bytes | Will be confirmed by receiveRaw() showing actual byte count |
| T3-S3 V1.3 exact GPIO for this variant | Confirmed from GitHub for the series, not V1.3 specifically | Check schematic PDF in LilyGo-LoRa-Series repository |
| Whether 868 MHz heater uses same protocol | No captured data yet | SDR capture in 868 MHz band |
