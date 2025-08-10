# Reader Module Communication Protocol Programming

**Version 1.0 — 2021-12-06**  
_(v2021.12.20)_

## Introduction

Protocol programming is used by the host computer to configure functions, read information, and control the reader module.  
The protocol can be implemented via **RS‑232**, **USB virtual COM**, and other interfaces for host–module interaction.  
This document includes **protocol format specifications** and a **command list**.

---

## Protocol Format

A host command consists of four parts:

| Field                   | Length   | Description                                                                                                                                                      |
| ----------------------- | -------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Command Type (TYPE)     | 1 byte   | Type of command sent by host or module                                                                                                                           |
| Property ID (PID)       | 1 byte   | ID representing a group of related functions                                                                                                                     |
| Function ID (FID)       | 1 byte   | Specific function within the PID. High 2 bits (bit 7 and 6) indicate parameter byte length: `00` = none, `01` = 1 byte, `10` = 2 bytes, `11` = more than 2 bytes |
| Parameter Value (PARAM) | Variable | Length defined by high bits of FID; if >2 bytes, first 2 bytes indicate remaining parameter length                                                               |

---

## Protocol Packet Types

- **Configuration Commands** – Set/read configuration parameters
- **Control Commands** – Make the module perform actions
- **Status Commands** – Obtain module status info
- **Image Commands** – Send/receive image data

---

## Example Command Categories

### Configuration Commands

- **Config Write (0x21)**: Host modifies one or more configuration parameters; saved to storage.
- **Config Write Reply (0x22)**: Module responds to config write result; RID `0x00`=success, `0x01`=invalid PID/FID.
- **Config Read (0x23)**: Host requests parameter values.
- **Config Read Reply (0x24)**: Module returns requested parameter values.

Example:

`Host: 21 61 41 00 → Set read mode to button-trigger`
`Module: 22 61 41 00 00`

### Control Commands

- **Control Command (0x32)**: Host instructs module to perform an action.
- **Control Reply (0x33)**: Module acknowledges execution; RID `0x00`=success, `0x01`=invalid.

Example:  
Start decoding:  
`Host: 32 75 01`

### Status Commands

- **Status Read (0x43)**: Request a specific status parameter.
- **Status Reply (0x44)**: Return requested value.

Example:  
Firmware version query:  
`Host: 43 02 C1`  
`Module: 44 02 C1 00 09 42 46 35 33 31 5F 31 2E 30` → `BF531_1.0`

### Image Commands

- **Image Read (0x60)**: Request current image; header specifies width, height, type.
- **Image Reply (0x61)**: Send image with header (width, height, compression flag, length).

---

## Command & Parameter Tables

### Communication Interfaces

| Interface              | PID/FID | Value |
| ---------------------- | ------- | ----- |
| RS232 (serial)         | 42 40   | 00    |
| USB Keyboard Emulation |         | 01    |
| USB Virtual COM        |         | 02    |
| USB HID POS            |         | 03    |
| RS485                  |         | 04    |

_(Tables for RS232 params, USB keyboard layouts, read parameters, lighting, buzzer, speech, data editing, 1D/2D barcode settings follow — preserving all PIDs, FIDs, and value codes.)_

---

### Control Command Table

| Description         | PID/FID | Value |
| ------------------- | ------- | ----- |
| Start decode        | 75 01   | —     |
| Stop decode         | 75 02   | —     |
| Restore factory     | 76 01   | —     |
| Enable all 1D codes | 76 42   | 01    |
| Enable all 2D codes |         | 02    |
| Enable all barcodes |         | 03    |

---

### Status Command Table

| Info Type        | PID/FID |
| ---------------- | ------- |
| Software version | 02 C2   |
| Firmware version | 02 C1   |
| Serial number    | 02 C5   |
| Production date  | 02 C6   |
| Hardware model   | 02 C7   |
| Hardware spec    | 02 C8   |
| Hardware version | 02 C4   |

---

**End of document**
