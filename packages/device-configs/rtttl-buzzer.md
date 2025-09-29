# RTTTL Buzzer Configuration Package

A comprehensive ESPHome configuration package for RTTTL (Ring Tone Text Transfer Language) buzzer functionality, enabling musical notifications, alerts, and sound effects with Home Assistant integration.

## Overview

This package provides a ready-to-use configuration for RTTTL buzzer functionality, supporting custom melodies, predefined notification sounds, and API actions for dynamic sound playback. It is ideal for smart home automation scenarios requiring audio feedback and notifications.

> **✅ Universal Compatibility**: This package supports both **ESP32** and **ESP8266** platforms through configurable substitutions. You specify the appropriate PWM platform (`ledc` for ESP32, `esp8266_pwm` for ESP8266) via the `buzzer_pwm_platform` substitution.

## Features

- **RTTTL Playback**: Play RTTTL-formatted melodies and ringtones
- **API Actions**: Dynamic song playback via Home Assistant actions
- **Volume Control**: Configurable gain/volume settings (should work well with a speaker plus driver/amp, not so much with a buzzer)
- **Predefined Sounds**: Built-in notification sounds (startup, error, success, doorbell, notification)
- **Home Assistant Integration**: Buttons for playing predefined sounds
- **Flexible Pin Configuration**: Use any GPIO pin for buzzer output
- **Cross-Platform PWM**: Supports both ESP32 and ESP8266 platforms

## Hardware Requirements

### Required Components
- **Passive Buzzer** (required - active buzzers will not work with RTTTL as they cannot change frequency)
- **ESP32 or ESP8266**

### Pin Connections

| Function         | ESP Pin      | Buzzer Pin |
|-----------------|--------------|------------|
| PWM Output      | Configurable | Positive   |
| Ground          | GND          | Negative   |


## Configuration Usage

### Basic Setup for ESP32

```yaml
substitutions:
  buzzer_pin: GPIO21  # Adjust to your wiring
  buzzer_pwm_platform: "ledc"  # ESP32 uses LEDC

packages:
  rtttl_buzzer: !include packages/device-configs/rtttl-buzzer.yaml
```

### Basic Setup for ESP8266

```yaml
substitutions:
  buzzer_pin: GPIO4  # Adjust to your wiring (D2 on NodeMCU)
  buzzer_pwm_platform: "esp8266_pwm"  # ESP8266 uses esp8266_pwm

packages:
  rtttl_buzzer: !include packages/device-configs/rtttl-buzzer.yaml
```

### Advanced Setup with Custom Volume

```yaml
substitutions:
  buzzer_pin: GPIO21
  buzzer_pwm_platform: "ledc"  # or "esp8266_pwm" for ESP8266
  buzzer_gain: "60%"  # Lower volume

packages:
  rtttl_buzzer: !include packages/device-configs/rtttl-buzzer.yaml
```

### Required Substitutions
- `buzzer_pin`: GPIO pin connected to the buzzer (e.g., `GPIO21`)
- `buzzer_pwm_platform`: PWM platform (`"ledc"` for ESP32, `"esp8266_pwm"` for ESP8266)

### Optional Substitutions
- `buzzer_gain`: Volume gain percentage (default: `"75%"`)

## Home Assistant Integration

### API Actions

#### `rtttl_play`
Play any RTTTL string:
```yaml
service: esphome.your_device_rtttl_play
data:
  song_str: "mario:d=4,o=5,b=100:16e6,16e6,32p,8e6,16c6,16e6,8g6,8p,8g,8p"
```

#### `rtttl_play_with_volume`
Play RTTTL with custom volume (0-100):
```yaml
service: esphome.your_device_rtttl_play_with_volume
data:
  song_str: "mario:d=4,o=5,b=100:16e6,16e6,32p,8e6,16c6,16e6,8g6,8p,8g,8p"
  volume: 50  # Volume percentage (0-100)
```

### Predefined Sound Scripts

The package includes several predefined sounds accessible via scripts:

- `play_startup_sound`: Device startup melody
- `play_notification_sound`: General notification tone
- `play_error_sound`: Error alert sound
- `play_success_sound`: Success confirmation
- `play_doorbell_sound`: Doorbell chime

All predefined sounds use the current volume setting from the "Buzzer Volume" control.

### Home Assistant Controls

#### Number Components
- **Buzzer Volume**: Adjustable volume control (0-100%) with persistent settings

#### Test Buttons
Test buttons are available in the diagnostic category:
- **Play Startup Sound**
- **Play Notification Sound** 
- **Play Error Sound**
- **Play Success Sound**
- **Play Doorbell Sound**

## Example Home Assistant Automations

### Doorbell Notification
```yaml
- alias: Doorbell Ring
  trigger:
    - platform: state
      entity_id: binary_sensor.front_door_button
      to: 'on'
  action:
    - service: esphome.your_device_rtttl_play
      data:
        song_str: "doorbell:d=4,o=5,b=125:8g,8a,8g,8a,8g,8a,8g,8a"
```

### Security Alert
```yaml
- alias: Security Alert Sound
  trigger:
    - platform: state
      entity_id: binary_sensor.motion_sensor
      to: 'on'
  condition:
    - condition: state
      entity_id: alarm_control_panel.home
      state: 'armed_away'
  action:
    - service: script.play_error_sound
```

### Success Notification
```yaml
- alias: Backup Complete Sound
  trigger:
    - platform: state
      entity_id: sensor.backup_status
      to: 'completed'
  action:
    - service: script.play_success_sound
```

## RTTTL Format

RTTTL (Ring Tone Text Transfer Language) format: `name:settings:notes`

### Settings
- `d=4`: Default note duration (4 = quarter note)
- `o=5`: Default octave
- `b=125`: Beats per minute (tempo)

### Notes
- Notes: `c`, `d`, `e`, `f`, `g`, `a`, `b`
- Durations: `1`, `2`, `4`, `8`, `16`, `32` (whole, half, quarter, eighth, sixteenth, thirty-second)
- Octaves: `4`, `5`, `6`, `7`
- Sharp notes: `c#`, `d#`, `f#`, `g#`, `a#`
- Pauses: `p`

### Example RTTTL Strings
```
"mario:d=4,o=5,b=100:16e6,16e6,32p,8e6,16c6,16e6,8g6,8p,8g,8p"
"tetris:d=4,o=5,b=160:e6,8b,8c6,8d6,16e6,16d6,8c6,8b,a,8a,8c6,e6,8d6,8c6"
"imperial:d=4,o=5,b=112:8g,8g,8g,8eb,8bb,g,8eb,8bb,g"
```

## Troubleshooting

- **No Sound**: Check buzzer wiring and ensure you're using a passive buzzer
- **Distorted Sound**: Try reducing `buzzer_gain` to lower PWM duty cycle
- **Volume Too Low**: Increase `buzzer_gain` or check power supply
- **RTTTL Not Playing**: Verify RTTTL string format and syntax
- **Buzzer Always On**: Ensure proper grounding and check for active vs passive buzzer

## Hardware Notes

### Volume Control
- **Runtime Control**: Use the "Buzzer Volume" number component in Home Assistant (0-100%)
- **API Control**: Use the `rtttl_play_with_volume` action with volume parameter
- **Default Volume**: Set via `buzzer_gain` substitution (used as initial value)
- Consider transistor amplifier for louder output if needed

## Technical Details

### Platform Configuration

The package uses a substitution variable to select the PWM platform:

```yaml
substitutions:
  buzzer_pwm_platform: "ledc"  # For ESP32, or "esp8266_pwm" for ESP8266

output:
  - platform: ${buzzer_pwm_platform}
    pin: ${buzzer_pin}
    id: buzzer_output
```

## Advanced Usage

### Custom RTTTL in Automations

#### Play with volume set on the device
```yaml
- action: esphome.your_device_rtttl_play
  data:
    song_str: "custom:d=4,o=5,b=140:8c,8d,8e,8f,8g,8a,8b,8c6"
```

#### Play with a specific volume
```yaml
- action: esphome.your_device_rtttl_play_with_volume
  data:
    song_str: "custom:d=4,o=5,b=140:8c,8d,8e,8f,8g,8a,8b,8c6"
    volume: 50
```

### Integration with Other Components
```yaml
# Play sound when motion detected
binary_sensor:
  - platform: gpio
    pin: GPIO2
    name: "Motion Sensor"
    on_press:
      - script.execute: play_notification_sound
```

## File Reference
- **Configuration**: `packages/device-configs/rtttl-buzzer.yaml`
- **Documentation**: `packages/device-configs/rtttl-buzzer.md`
- **Example Usage**: `packages/device-configs/examples/example-rtttl-buzzer-usage.yaml`
