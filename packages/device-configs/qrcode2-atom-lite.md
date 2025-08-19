# M5Stack Atom Lite QRCode2 Device Configuration

A comprehensive ESPHome device configuration package for the M5Stack Atom Lite with QRCode2 Base scanner, designed for inventory management and barcode scanning applications with Home Assistant integration.

## Overview

This package provides a complete, production-ready configuration for building a wireless barcode/QR code scanner with visual status indication, stock management modes, and comprehensive Home Assistant integration.

## Hardware Requirements

### Required Components

- **M5Stack Atom Lite** (ESP32-based controller)
- **M5Stack QRCode2 Base** (UART barcode scanner)

### Pin Connections

| Function | GPIO Pin | Description |
|----------|----------|-------------|
| UART TX | 22 | Serial transmission to scanner |
| UART RX | 19 | Serial reception from scanner |
| Scanner Trigger | 23 | Hardware trigger control |
| Scanner LED | 33 | Illumination LED (firmware controlled) |
| User Button | 39 | Manual trigger and mode switching |
| Status RGB LED | 27 | Built-in SK6812 status indicator |

## Features

### Core Functionality

- **Barcode/QR Code Scanning** with configurable 20-second timeout
- **Dual Stock Modes**: Add Stock (default) and Remove Stock
- **Button Controls**: Short press to toggle scan on/off, long press (5s) to switch modes
- **Visual Status Indication** via RGB LED with multiple states
- **Home Assistant Integration** with comprehensive sensor and control entities

### Stock Management

- **Add Stock Mode** (Green LED during scanning)
- **Remove Stock Mode** (Red LED during scanning)
- **Mode Persistence** across device restarts
- **Mode Switching** via long button press or Home Assistant

### LED Status Indicators

| Status | LED Behavior | Description |
|--------|--------------|-------------|
| Idle | Off | No scanning, no errors |
| WiFi Error | Orange blinking | No WiFi connection |
| HA Error | Purple blinking | No Home Assistant connection |
| Button Press | 1 blue blink | Button press acknowledgment (toggle scan/idle) |
| Mode Switch | 2 blinks of mode color | Mode change confirmation |
| Scanning (Add) | Solid green | Add stock scanning active |
| Scanning (Remove) | Solid red | Remove stock scanning active |
| New Code Scanned | Light blue blink | New barcode detected |
| Same Code Rescanned | 2 light blue blinks | Duplicate scan detected |
| Scan Timeout | Yellow blink | Scanning timed out |
| Settings Changed | Yellow blink | Configuration updated |
| Scanner Error | Rose blinking | Hardware communication error |

## Home Assistant Entities

### Sensors

#### Text Sensors
- **Scanned Code** - Raw barcode/QR code value
- **Last Scan Info** - Detailed scan information with timestamp, count, and mode
- **Stock Mode** - Current mode display ("Add Stock" / "Remove Stock")

#### Binary Sensors
- **Scanner Active** - Indicates if currently scanning

#### Numeric Sensors
- **Scan Counter** - Number of times same code has been scanned
- **Uptime** - Device uptime in minutes (diagnostic)

### Controls

#### Numbers (Sliders)
- **Scan Timeout** - Adjustable from 5-60 seconds (default: 20s)
- **Long Press Duration** - Button hold time from 1-10 seconds (default: 5s)
- **Grocy Location ID** - Location identifier for Grocy integration (1-100, default: 1)

#### Switches
- **Stock Mode** - Toggle between Add/Remove stock modes

#### Buttons
- **Start QR Scan** - Manual scan trigger (Home Assistant only)
- **Stop QR Scan** - Manual scan stop (Home Assistant only)
- **Reset Count** - Reset scan counter and last scanned code

**Note**: The physical button toggles between scan/idle states. Use HA buttons for specific actions.

#### Lights
- **Status LED** - Full RGB control with brightness, color, and effects

## Configuration Usage

### Basic Setup

```yaml
# Main device configuration file
substitutions:
  device_name: "qrcode2-atom-lite-1"
  device_friendly_name: "QR Scanner - 1"

packages:
  device_config: !include packages/device-configs/qrcode2-atom-lite.yaml

esphome:
  name: ${device_name}
  friendly_name: ${device_friendly_name}
```

### Customization Options

#### Pin Remapping

```yaml
# Override default pins if needed
substitutions:
  tx_pin: "21"      # Change TX pin
  rx_pin: "25"      # Change RX pin
  trigger_pin: "26" # Change trigger pin
  button_pin: "0"   # Change button pin
```

#### Timeout Adjustment

```yaml
# Modify scanning timeout
substitutions:
  scanning_timeout: "30"  # 30 seconds instead of default 20
```
