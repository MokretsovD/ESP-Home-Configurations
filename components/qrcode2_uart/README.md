# QRCode2 UART Component

A custom ESPHome component for interfacing with M5Stack QRCode2 scanners via UART communication.

## Overview

This component provides a thin wrapper around the M5Stack QRCode2 scanner's UART protocol, enabling barcode and QR code scanning functionality in ESPHome projects. It supports both automatic and manual scanning modes, device configuration, and comprehensive status reporting.

## Features

- **UART-based communication** with M5Stack QRCode2 scanners
- **Configurable scanning timeout** (5-60 seconds)
- **Configurable long press duration** for mode switching (1-10 seconds)
- **Button support** for manual scan triggering and mode switching
- **Device information retrieval** (firmware version, hardware model, etc.)
- **Binary sensor** for scanning status
- **Text sensor** for scan results
- **Multiple automation triggers** for scan events, button presses
- **RGB LED integration** for visual status indication
- **Stock management modes** (Add/Remove stock)

## Hardware Requirements

- M5Stack QRCode2 scanner module
- ESP32-based controller (tested with M5Stack Atom Lite)
- UART pins for communication (TX, RX)
- GPIO pins for trigger control and button input
- Optional: RGB LED for status indication

## Configuration

### Basic Configuration

```yaml
# UART configuration
uart:
  id: qrcode_uart
  baud_rate: 115200
  tx_pin: 22
  rx_pin: 19
  parity: NONE

# QRCode2 scanner component
qrcode2_uart:
  id: qrcode_scanner
  uart_id: qrcode_uart
  scanning_timeout: 20  # seconds
  scan_trigger_pin: 
    number: 39
    mode:
      input: true
    inverted: false
  scanner_trigger_pin:
    number: 23
    mode:
      output: true
  led_pin:
    number: 33
    mode:
      output: true
    inverted: true
```

### Configuration Variables

- **uart_id** (**Required**, string): The ID of the UART component to use for communication.
- **scanning_timeout** (*Optional*, int): Maximum time to wait for a scan result in seconds. Defaults to `20`.
- **scan_trigger_pin** (*Optional*, GPIO Pin): GPIO pin connected to the device button for manual scan triggering.
  - **inverted** (*Optional*, boolean): Whether the button pin logic is inverted. Defaults to `false`.
- **scanner_trigger_pin** (*Optional*, GPIO Pin): GPIO pin connected to the scanner's trigger input.
- **led_pin** (*Optional*, GPIO Pin): GPIO pin connected to the scanner's LED control (note: LED is firmware-controlled).
  - **inverted** (*Optional*, boolean): Whether the LED pin logic is inverted. Defaults to `true`.

### Automation Triggers

#### on_scan

Triggered when a barcode/QR code is successfully scanned.

```yaml
qrcode2_uart:
  # ... other config
  on_scan:
    then:
      - logger.log: 
          format: "Scanned: %s"
          args: ['x.c_str()']
```

#### on_long_press

Triggered when the button is held for the configured duration (default 5 seconds).

```yaml
qrcode2_uart:
  # ... other config
  on_long_press:
    then:
      - logger.log: "Long press detected - switching mode"
```

#### on_short_press

Triggered when the button is pressed briefly.

```yaml
qrcode2_uart:
  # ... other config
  on_short_press:
    then:
      - logger.log: "Short press detected"
```

#### on_start_scan

Triggered when scanning begins.

```yaml
qrcode2_uart:
  # ... other config
  on_start_scan:
    then:
      - logger.log: "Scanning started"
```

#### on_stop_scan

Triggered when scanning stops (timeout or successful scan).

```yaml
qrcode2_uart:
  # ... other config
  on_stop_scan:
    then:
      - logger.log: "Scanning stopped"
```

## Sensors

### Text Sensor

Displays the last scanned barcode/QR code value.

```yaml
text_sensor:
  - platform: qrcode2_uart
    qrcode2_uart_id: qrcode_scanner
    name: "Scanned Code"
    id: qr_code_result
```

### Binary Sensor

Indicates whether the scanner is currently active.

```yaml
binary_sensor:
  - platform: qrcode2_uart
    qrcode2_uart_id: qrcode_scanner
    name: "Scanner Active"
    id: scanner_active
```

## Actions

### qrcode2_uart.start_scan

Manually starts a scanning operation.

```yaml
- qrcode2_uart.start_scan:
    id: qrcode_scanner
```

### qrcode2_uart.stop_scan

Manually stops the current scanning operation.

```yaml
- qrcode2_uart.stop_scan:
    id: qrcode_scanner
```

### qrcode2_uart.get_device_info

Retrieves and logs device information from the scanner.

```yaml
- qrcode2_uart.get_device_info:
    id: qrcode_scanner
```

### qrcode2_uart.reset_scanner

Sends a reset command to the scanner.

```yaml
- qrcode2_uart.reset_scanner:
    id: qrcode_scanner
```

### qrcode2_uart.update_long_press_duration

Updates the long press duration threshold.

```yaml
- qrcode2_uart.update_long_press_duration:
    id: qrcode_scanner
    duration_ms: 3000  # 3 seconds
```

## Protocol Details

The component implements the M5Stack QRCode2 4-part UART protocol:

- **TYPE**: Command type (0x32 for control commands, 0x44 for status responses)
- **PID**: Product ID 
- **FID**: Function ID
- **PARAM**: Parameter value

### Supported Commands

- **Start Scan**: `{0x32, 0x75, 0x01}`
- **Stop Scan**: `{0x32, 0x75, 0x02}`
- **Get Software Version**: `{0x32, 0x76, 0x03}`
- **Get Firmware Version**: `{0x32, 0x76, 0x04}`
- **Get Hardware Model**: `{0x32, 0x76, 0x05}`
- **Get Serial Number**: `{0x32, 0x76, 0x06}`
- **Reset Scanner**: `{0x32, 0x76, 0x07}`

## Troubleshooting

### Scanner Not Responding

1. **Check UART connections**: Ensure TX/RX pins are not swapped
2. **Verify baud rate**: Must be 115200 for M5Stack QRCode2
3. **Check power**: Ensure scanner module is properly powered
4. **Button configuration**: Verify button pin configuration and inversion settings

### Button Issues

1. **Inverted logic**: Try toggling the `inverted` setting on the button pin
2. **Pin conflicts**: Ensure GPIO39 is not used by other components
3. **Pullup/pulldown**: GPIO39 doesn't support internal pullups - use external if needed

### LED Not Working

The scanner's illumination LED is controlled by firmware and activates automatically during scanning.

### Scan Results Not Appearing

1. **Check text sensor configuration**: Ensure sensor is properly linked to component
2. **Verify automation**: Check that `on_scan` automations are configured correctly
3. **Button functionality**: Test manual scanning with button press

## Example Integration

See the complete M5Stack Atom Lite QRCode2 configuration package for a full working example with RGB status LED, Home Assistant integration, and stock management features.

## Hardware Compatibility

- **Tested**: M5Stack Atom Lite + QRCode2 Base
- **Pins**: Any ESP32 UART-capable pins
- **Voltage**: 3.3V logic level
- **Communication**: 115200 baud UART

## License

This component follows ESPHome's open-source license model.
