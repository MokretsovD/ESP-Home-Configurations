# M5Stack Atom Lite QRCode2 Device Configuration

A comprehensive ESPHome device configuration package for the M5Stack Atom Lite with QRCode2 Base scanner, designed for inventory management and barcode scanning applications with Home Assistant integration.

## Overview

This package provides a complete, production-ready configuration for building a wireless barcode/QR code scanner with visual status indication, stock management modes, and comprehensive Home Assistant integration.

## Hardware Requirements

### Required Components

- **M5Stack Atom Lite** (ESP32-based controller)
- **M5Stack QRCode2 Base** (UART barcode scanner)
- **Built-in SK6812 RGB LED** (status indication)
- **Built-in button** (GPIO39 - scan trigger and mode switching)

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
- **Button Controls**: Short press to scan, long press (5s) to switch modes
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
| Button Press | 1 blue blink | Button press acknowledgment |
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
- **Scanner Info** - Device firmware and hardware information (diagnostic)

#### Binary Sensors
- **Scanner Active** - Indicates if currently scanning

#### Numeric Sensors
- **Scan Counter** - Number of times same code has been scanned
- **Uptime** - Device uptime in minutes (diagnostic)

### Controls

#### Numbers (Sliders)
- **Scan Timeout** - Adjustable from 5-60 seconds (default: 20s)
- **Long Press Duration** - Button hold time from 1-10 seconds (default: 5s)

#### Switches
- **Stock Mode** - Toggle between Add/Remove stock modes

#### Buttons
- **Start QR Scan** - Manual scan trigger
- **Reset Count** - Reset scan counter and last scanned code

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

#### Device Identification

```yaml
substitutions:
  device_name: "warehouse-scanner-01"
  device_friendly_name: "Warehouse Scanner #1"
  device_description: "QR code scanner for warehouse inventory"
```

## Automation Examples

### Basic Inventory Tracking

```yaml
# Home Assistant automation example
automation:
  - alias: "Process Inventory Scan"
    trigger:
      - platform: state
        entity_id: sensor.qr_scanner_scanned_code
    condition:
      - condition: template
        value_template: "{{ trigger.to_state.state != 'unknown' }}"
    action:
      - service: notify.mobile_app
        data:
          message: >
            {% if is_state('sensor.qr_scanner_stock_mode', 'Add Stock') %}
              Added item: {{ trigger.to_state.state }}
            {% else %}
              Removed item: {{ trigger.to_state.state }}
            {% endif %}
```

### Automated Mode Switching

```yaml
# Switch to remove mode during specific hours
automation:
  - alias: "Auto Remove Mode Evening"
    trigger:
      - platform: time
        at: "18:00:00"
    action:
      - service: switch.turn_off
        entity_id: switch.qr_scanner_stock_mode
```

### Error Notifications

```yaml
# Alert on scanner errors
automation:
  - alias: "Scanner Error Alert"
    trigger:
      - platform: state
        entity_id: binary_sensor.qr_scanner_scanner_active
        to: 'off'
        for: "00:01:00"
    condition:
      - condition: template
        value_template: "{{ now() - states.sensor.qr_scanner_scanned_code.last_changed > timedelta(minutes=5) }}"
    action:
      - service: notify.admin
        data:
          message: "QR Scanner may be malfunctioning - no activity detected"
```

## Troubleshooting

### Common Issues

#### Scanner Not Responding
1. Check UART wiring (TX/RX may be swapped)
2. Verify 115200 baud rate setting
3. Ensure scanner power connection
4. Check device logs for UART errors

#### Button Not Working
1. Verify GPIO39 configuration
2. Check button inversion setting (`inverted: false`)
3. Test button physically - should pull GPIO39 LOW when pressed
4. Ensure no pin conflicts with other components

#### LED Issues
1. **Scanner LED**: Controlled by firmware, only works during scanning
2. **Status RGB LED**: Check GPIO27 connection and SK6812 configuration
3. **Brightness**: Adjust via Home Assistant light entity

#### Home Assistant Integration
1. **Entities not appearing**: Check ESPHome logs for API connection
2. **States not updating**: Verify sensor configurations and triggers
3. **Controls not working**: Check action configurations and component IDs

### Diagnostic Steps

1. **Enable verbose logging**:
   ```yaml
   logger:
     level: VERBOSE
     logs:
       qrcode2_uart: VERBOSE
   ```

2. **Check UART communication**:
   - Look for "Device info:" logs during startup
   - Verify scanner responses to commands

3. **Monitor button state**:
   - Watch for GPIO39 state changes in logs
   - Verify press/release detection

4. **Test LED functions**:
   - Use Home Assistant to manually control status LED
   - Verify color and brightness changes

## Advanced Configuration

### Custom LED Effects

```yaml
# Add custom LED effects
light:
  - platform: esp32_rmt_led_strip
    # ... other config
    effects:
      - pulse:
          name: "Custom Pulse"
          transition_length: 2s
      - strobe:
          name: "Custom Strobe"
          colors:
            - state: true
              red: 100%
              green: 0%
              blue: 0%
              duration: 100ms
```

### Integration with External Systems

```yaml
# Send scans to external API
on_scan:
  then:
    - http_request.post:
        url: "https://inventory.company.com/api/scan"
        headers:
          Content-Type: "application/json"
        json:
          barcode: !lambda 'return x;'
          mode: !lambda 'return id(stock_mode) ? "add" : "remove";'
          timestamp: !lambda 'return id(homeassistant_time).now().timestamp;'
```

### Conditional Scanning

```yaml
# Only allow scanning during business hours
on_short_press:
  then:
    - if:
        condition:
          lambda: |-
            auto time = id(homeassistant_time).now();
            return time.hour >= 9 && time.hour < 17;
        then:
          - qrcode2_uart.start_scan:
              id: qrcode_scanner
        else:
          - logger.log: "Scanning disabled outside business hours"
```

## Performance Considerations

- **Scan Frequency**: Default 20-second timeout balances battery life and usability
- **LED Brightness**: Lower brightness extends battery life in battery-powered setups
- **WiFi Management**: Component includes automatic reconnection handling
- **Memory Usage**: Configuration uses ~15% RAM, ~65% Flash on ESP32

## Updates and Maintenance

### Firmware Updates
- Use ESPHome OTA updates for remote firmware deployment
- Monitor device logs during updates
- Test all functionality after updates

### Configuration Changes
- Most settings can be changed via Home Assistant without firmware updates
- Scan timeout and long press duration are runtime-configurable
- LED brightness and effects adjust immediately

### Hardware Maintenance
- Clean scanner lens periodically for optimal reading
- Check connections if scan reliability decreases
- Monitor device temperature in enclosed installations

## License

This configuration follows ESPHome's open-source license model and is provided as-is for educational and commercial use.
