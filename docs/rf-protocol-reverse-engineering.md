# RF Protocol Reverse Engineering with SDR

A practical guide to capturing and decoding unknown 433/868 MHz FSK protocols,
using the VEVOR diesel heater remote as the worked example.

All tools listed are **free and open source**.

---

## Hardware Required

| Item | Purpose | Notes |
|------|---------|-------|
| RTL-SDR v3 | Capture raw RF | ~$25, receive-only, covers 500 kHz–1766 MHz. Sufficient for this use case. |
| Antenna | Reception | Stock dipole works; a tuned 433 or 868 MHz whip improves range |
| Target device | Signal source | Heater remote / heater unit |

RTL-SDR is receive-only. For the TX verification step (Step 7) use the CC1101
module you already have — no additional hardware needed.

---

## Software Required

All tools below are free and open source.

| Tool | Platform | Purpose |
|------|---------|---------|
| [SDR++](https://github.com/AlexandreRouma/SDRPlusPlus) | Windows / Linux / macOS | Spectrum viewer and waterfall — use this for Step 1 and 2 |
| [GQRX](https://gqrx.dk/) | Linux / macOS | Alternative spectrum viewer |
| [URH](https://github.com/jopohl/urh) — Universal Radio Hacker | Windows / Linux / macOS | Capture, demodulate, decode, protocol analysis — the main tool |
| [Inspectrum](https://github.com/miek/inspectrum) | Linux / macOS / WSLg | Deep waveform inspection when URH auto-detection fails |
| [PulseView / sigrok](https://sigrok.org/) | All | Logic-level bit decoding (optional) |
| [rtl_433](https://github.com/merbanan/rtl_433) | All | Auto-identifies many known OOK/FSK protocols on 433/868 MHz — useful first check |

**On Windows / WSL:** RTL-SDR must be accessed from the Windows host, not from WSL
(USB device passthrough to WSL is unreliable for SDR). Run SDR++ and URH natively
on Windows. Install the RTL-SDR Windows driver using [Zadig](https://zadig.akeo.ie/)
(free) — replace the default driver with WinUSB or libusb-win32.

URH has a Windows installer. SDR++ has a Windows release binary. Both are portable
and require no admin rights after driver installation.

---

## Step 0 — Quick check with rtl_433

Before doing any manual work, run rtl_433 — it auto-identifies hundreds of known
433/868 MHz protocols and may decode your device immediately:

```bash
# Linux / WSL (pipe from Windows rtl_433.exe if needed)
rtl_433 -f 433920000 -A   # 433 MHz, auto-detect modulation
rtl_433 -f 868300000 -A   # 868 MHz
```

Press the remote button repeatedly. If rtl_433 recognises the protocol, it will
print the decoded fields. If not, proceed to Steps 1–7 manually.

---

## Step 1 — Find the Frequency

### Option A: Check the label

Look for a sticker on the remote or heater PCB: `433.92 MHz`, `868.3 MHz`,
or an FCC/CE ID number. In Europe, assume 868 MHz until proven otherwise.

### Option B: Spectrum scan with SDR++

1. Connect RTL-SDR, open SDR++.
2. Set sample rate: **2.4 MSPS**. Center on 433.92 MHz or 868.3 MHz.
3. Enable the **FFT** waterfall display.
4. Press a button on the remote repeatedly.
5. Watch for a burst of energy — a short bright horizontal stripe in the waterfall.
6. Note the exact frequency of the burst.

**Typical ISM frequencies:**

| Frequency | Region |
|-----------|--------|
| 433.92 MHz | Worldwide — most cheap Chinese devices |
| 868.3 MHz | Europe (ETSI EN 300 220) — EU-market devices |
| 315 MHz | North America — garage doors, older keyfobs |

---

## Step 2 — Identify Modulation

Zoom into the burst in the SDR++ waterfall:

| What you see | Modulation |
|-------------|-----------|
| Burst appears / disappears as a single spike | OOK / ASK |
| Burst shifts between two close frequencies | FSK |
| Two frequency shifts with smooth, rounded edges | GFSK |
| Four distinct frequency levels | 4-FSK |

The VEVOR heater uses **GFSK** (Gaussian FSK) — the same as Bluetooth LE and most
CC1101-based devices. In the waterfall it looks like a burst that moves between two
nearby frequencies rather than simply appearing and disappearing.

---

## Step 3 — Capture with URH

1. Open URH → **File → New Project**.
2. Select your RTL-SDR device.
3. Set:
   - **Frequency:** from Step 1
   - **Sample rate:** 2 MSPS minimum (4 MSPS gives more resolution)
   - **Bandwidth:** 500 kHz (enough for narrow-band FSK)
   - **Gain:** increase in 5 dB steps until the signal appears clearly in the waterfall
4. Click **Record**, press the remote button 5–10 times, stop recording.
5. You should see distinct short bursts in the waveform view.

---

## Step 4 — Demodulate

In URH's **Signal** tab:

1. Select a burst → right-click → **Demodulate**.
2. Choose **FSK** (or GFSK) demodulation.
3. URH shows a bit stream.

### Determine the baud rate

- Zoom into a single burst until individual bits are visible.
- Measure the width of the narrowest pulse in samples.
- `baud_rate = sample_rate / samples_per_bit`

Use **Edit → Auto-detect parameters** — URH will estimate baud rate automatically.
Common rates: 1200, 2400, 4800, 9600, **10000**, 38400, 115200 bps.

> **VEVOR heater (CC1101 registers MDMCFG4=0xF8, MDMCFG3=0x93):**
> `R = 26,000,000 × (256 + 147) / 2^20 ≈ 9,996 bps ≈ **10 kbps**`
> Note: some online sources incorrectly state 4800 bps for this protocol.

### Determine frequency deviation (FSK only)

Deviation = half the frequency gap between the two FSK tones.
In URH's spectrogram, measure the distance between the two frequency bands in the burst.
Divide by 2.

> **VEVOR heater (CC1101 register DEVIATN=0x26):**
> `f_dev = 26,000,000 / 2^17 × (8+6) × 2^2 ≈ **11.1 kHz**`

---

## Step 5 — Identify the Packet Structure

After demodulation you have a raw bit stream. CC1101-based FSK protocols follow
a fixed structure:

```plaintext
[Preamble] [Sync Word] [Length] [Payload] [CRC]
```

### Preamble

Alternating `10101010...` pattern used for clock synchronisation. Typically
4–8 bytes (32–64 bits). Discard everything up to and including this pattern.

### Sync Word

A fixed 2-byte (16-bit) pattern that marks the start of every packet.
**It is identical across all captures from the same device.**

How to find it:

1. Collect 5–10 button press captures.
2. In URH's **Analysis** tab, align the packets and find the first bit sequence
   that is **the same in every packet**.
3. That repeated prefix (immediately after the preamble) is the sync word.

> **VEVOR heater:** `0111 1110 0011 1100` = `0x7E3C`

### Length byte and Payload

In variable-length protocols (like CC1101 variable mode), the first byte after
the sync word is the payload length. Everything that follows is the payload.

### CRC

Usually the last 1–2 bytes of the payload. To confirm:

1. Take two packets with different payloads.
2. Check whether the last bytes change when the rest of the data changes.
3. In URH: **Analysis → CRC** — try CRC-16/MODBUS, CRC-16-CCITT, CRC-8, XOR.

> **VEVOR heater TX command packets:** CRC-16/MODBUS over bytes 0–6 (length, command, 4-byte address, sequence number), result placed at bytes 7–8.
> **VEVOR heater RX state packets:** validated by CC1101 hardware CRC (APPEND_STATUS byte 1, bit 7 = CRC_OK). The software CRC-16/MODBUS positions do not match this heater variant.

---

## Step 6 — Map the Payload Fields

Collect packets across different conditions:

- Multiple presses of each button (power, temperature up/down, mode)
- Heater running at different temperatures (state broadcasts)
- Heater idle vs active

Build a comparison table:

| Byte offset | Observed values | Hypothesis |
|-------------|----------------|-----------|
| 0 | `0x09` (TX cmd) / `0x17` (RX state) | Length byte (excl. self): 9 for command packets, 23 for state broadcasts |
| 1 | `0x2B`, `0x3C`, `0x3E`… | Command byte |
| 2–5 | identical across all packets | Device address (32-bit) |
| 6 | `0x00`–`0x08` | State machine state |
| 9 | `0x78`, `0x7A`… | Voltage × 10 (0x78 = 12.0V) |
| 10 | `0x14`, `0x15`… | Ambient temperature °C |
| 13 | `0x19`, `0x1A`… | Temperature setpoint °C |

Pattern rules:

- Bytes identical across all packets → device ID / address
- Byte changes only when one specific button is pressed → command field
- Byte increments/decrements gradually → numeric value (temperature, voltage)
- Byte has only two values → boolean flag or 2-value enum

---

## Step 7 — Verify by Transmitting

Use the **CC1101 module you already have** — it can transmit as well as receive.

Using the DieselHeaterRF library: if you have decoded the heater address from
Step 5–6, call `sendCommand()` with the known address. Observe whether the
heater responds (state change, LED, sound).

Alternatively, transmit a captured raw packet via `txBurst()` directly after
loading the bytes into the CC1101 FIFO.

> RTL-SDR is **receive-only** and cannot transmit. A transmit-capable SDR
> (e.g. HackRF, LimeSDR) is not required if you already have a CC1101 module.

---

## Worked Example: VEVOR Heater at 868 MHz

If your heater is the EU 868 MHz variant, all protocol parameters are **identical**
to the 433 MHz version — only the carrier frequency changes:

| Parameter | 433 MHz | 868 MHz |
|-----------|---------|---------|
| CC1101 FREQ2 | `0x10` | `0x21` |
| CC1101 FREQ1 | `0xB1` | `0x6E` |
| CC1101 FREQ0 | `0x3B` | `0x6A` |
| Carrier | 433.92 MHz | 868.30 MHz |
| Modulation | GFSK | GFSK |
| Baud rate | ~10 kbps | ~10 kbps |
| Deviation | ~11.1 kHz | ~11.1 kHz |
| Sync word | `0x7E3C` | `0x7E3C` (assumed — verify) |

To confirm the sync word on the 868 MHz variant: capture a few button presses
with URH at 868.3 MHz. After demodulation, look for the fixed 16-bit pattern
that repeats at the start of every packet after the preamble. If it is `0x7E3C`,
the existing library and config work as-is with `heater_frequency: "868"`.

---

## Troubleshooting

### No burst visible in the waterfall

- Try the other frequency band (433 vs 868 MHz)
- Increase gain in SDR++ in 5 dB steps — but stop before noise floor rises sharply
- Replace the remote battery

### Burst visible but URH cannot demodulate cleanly

- Try ASK/OOK instead of FSK — some remotes use simple OOK
- Adjust the demodulation centre threshold (the noise floor line in URH's signal view)
- Try Inspectrum for finer manual analysis: `inspectrum capture.cfile`

### Bit stream looks random or inconsistent across captures

- Baud rate is wrong — try halving or doubling the value
- Signal is Manchester-encoded — enable Manchester decoding in URH
- Sample rate too low — recapture at 4 MSPS

### Packets look structurally correct but CRC never matches

- Try other CRC variants: CRC-8, CRC-16-CCITT, CRC-32, plain XOR
- The CRC might cover a different byte range — try excluding the first byte (length)
- CRC might be initialised with a non-standard seed — check URH's CRC analyser options

### rtl_433 shows packets but protocol is unrecognised

- The device uses a proprietary protocol not in rtl_433's library (common for heaters)
- Proceed with manual URH analysis from Step 3

---

## Further Reading

- [URH documentation wiki](https://github.com/jopohl/urh/wiki)
- [SDR++ documentation](https://github.com/AlexandreRouma/SDRPlusPlus/wiki)
- [rtl_433 supported protocols list](https://github.com/merbanan/rtl_433/blob/master/README.md)
- [CC1101 datasheet (SWRS061I)](https://www.ti.com/product/CC1101) — register map, packet format, CRC — use this to cross-reference register values against protocol parameters
- [RTL-SDR Blog 433 MHz tutorials](https://www.rtl-sdr.com/tag/433mhz/) — many worked examples of 433 MHz protocol decoding using free tools
- Zadig (RTL-SDR Windows driver tool): https://zadig.akeo.ie/
