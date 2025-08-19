# DFRobot SEN0610 mmWave Presence Sensor Configuration

A comprehensive ESPHome configuration package for the DFRobot SEN0610 mmWave presence sensor, enabling advanced occupancy detection, range and sensitivity adjustment, and Home Assistant integration.

## Overview

This package provides a ready-to-use configuration for the DFRobot SEN0610 mmWave sensor, supporting real-time occupancy detection, adjustable detection parameters, and full Home Assistant integration. It is ideal for smart home automation scenarios requiring reliable presence sensing.

## Features

- **Occupancy Detection**: Detects human presence using mmWave radar technology
- **Adjustable Range**: Minimum, maximum, and trigger range sliders
- **Sensitivity Control**: Separate sliders for occupancy and movement sensitivity
- **Latency Adjustment**: Fine-tune detection and clearance delays
- **UART Communication**: Robust serial interface for sensor control and data
- **Home Assistant Integration**: Exposes sensors, switches, numbers, and buttons
- **Configurable via Home Assistant**: All key parameters adjustable from the UI
- **Factory Reset and Query**: Buttons for sensor reset and configuration query

## Hardware Requirements

### Required Components
- **DFRobot SEN0610 mmWave Presence Sensor**
- **ESP32 or ESP8266** (tested with ESP32)
- **Wiring for UART (TX/RX)**

### Pin Connections

| Function         | ESP Pin  | Sensor Pin |
|-----------------|----------|------------|
| UART TX         | GPIO17   | RX         |
| UART RX         | GPIO16   | TX         |
| 3.3V Power      | 3.3V     | VCC        |
| Ground          | GND      | GND        |

> **Note:** Adjust TX/RX pins as needed for your board.

## Configuration Usage

### Basic Setup

```yaml
packages:
  dfrobot-sen0610: !include packages/device-configs/dfrobot-sen0610-presence.yaml
```

- Include the package in your ESPHome device YAML.
- Adjust UART pins if needed.
- Upload to your device.

### Home Assistant Entities

#### Sensors
- **mmWave Occupancy** (`binary_sensor`): True if presence detected
- **mmWave Raw UART Message** (`text_sensor`): Raw UART data for debugging

#### Numbers (Sliders)
- **mmWave Range (Minimum/Maximum/Trigger)**: Set detection boundaries (meters)
- **mmWave Delay (Detection/Clearance)**: Set detection and clearance latency (seconds)
- **mmWave Sensitivity (Occupancy/Movement)**: Adjust detection sensitivity (0-9)

#### Switches
- **mmWave sensor**: Enable/disable the sensor (sends start/stop commands)

#### Buttons
- **Apply mmWave Config**: Send all current settings to the sensor
- **Restart mmWave Sensor**: Reboot the sensor
- **Factory Reset mmWave**: Restore sensor to factory defaults
- **Query mmWave Config**: Request current sensor settings

## Example Home Assistant Automation

You can use the `mmWave Occupancy` binary sensor in automations, e.g.:

```yaml
- alias: Room Occupied
  trigger:
    - platform: state
      entity_id: binary_sensor.mmwave_occupancy
      to: 'on'
  action:
    - service: light.turn_on
      target:
        entity_id: light.room_lights
```

## Troubleshooting

- **No Occupancy Detected**: Check UART wiring and baud rate (9600). Ensure sensor is powered.
- **Sensor Not Responding**: Try Factory Reset or Restart buttons.
- **Parameter Changes Not Applied**: Use the Apply mmWave Config button after adjusting sliders.
- **Debugging**: Use the Raw UART Message sensor to view incoming data.

## Advanced Notes

- The configuration parses UART messages starting with `$DFHPD` to extract presence count.
- All detection parameters are adjustable live via Home Assistant UI.
- The sensor can be started/stopped and reset via exposed switches and buttons.

## File Reference
- **Configuration**: `packages/device-configs/dfrobot-sen0610-presence.yaml`
- **Example Usage**: (add an example YAML in `packages/device-configs/examples/`)
