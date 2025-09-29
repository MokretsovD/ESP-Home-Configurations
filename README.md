# ESP Home Configurations

This repository contains a collection of ESPHome YAML configurations for various smart home sensors and devices. The goal is to provide ready-to-use, well-documented configurations for ESP8266 and ESP32-based devices, with a focus on Home Assistant integration.

## Table of Contents

- [Development Setup](#development-setup)
- [Custom Components](#custom-components)
  - [UART Line Reader](#uart-line-reader-uart_line_reader)
  - [Deduplicate Text Sensor](#deduplicate-text-sensor-deduplicate_text)
  - [QRCode2 UART Scanner](#qrcode2-uart-scanner-qrcode2_uart)
- [Shared Packages](#shared-packages)
- [Configurations](#configurations)
  - [Smart Electric Meter](#smart-electric-meter)
  - [Gas Meter (Gaszaehler)](#gas-meter-gaszaehler)
  - [Water Level Reservoir (wasserstand-regenreservoir)](#water-level-reservoir-wasserstand-regenreservoir)
  - [Air Quality Sensor 1](#air-quality-sensor-1)
  - [Bluetooth Proxy](#bluetooth-proxy)
  - [Info Screen](#info-screen)
  - [Soil Sensor](#soil-sensor)
  - [Garden Watering Controller](#garden-watering-controller)
  - [QRCode2 Scanner](#qrcode2-scanner)
  - [DFRobot SEN0610 mmWave Presence Sensor](#dfrobot-sen0610-mmwave-presence-sensor)
- [Secrets & Sensitive Data](#secrets--sensitive-data)
- [Credits & Inspirations](#credits--inspirations)
- [License](#license)

---

## Overview

This repository is a public collection of my ESPHome device configurations. It is intended for educational, reference, and practical use. All sensitive information (such as WiFi credentials and API keys) is stored in `secrets.yaml` and not included in the repository.

Some configurations are based on or inspired by awesome projects from the community (see [Credits & Inspirations](#credits--inspirations)).

---

## Development Setup

### IDE Setup

For the best development experience, I recommend using Visual Studio Code or Cursor with the following extensions:

1. [ESPHome](https://marketplace.visualstudio.com/items?itemName=ESPHome.esphome-vscode) - Provides YAML validation, syntax highlighting, and device management
2. [ESPHome Snippets](https://marketplace.visualstudio.com/items?itemName=ESPHome.esphome-snippets) - Adds useful code snippets for ESPHome development

### Local ESPHome CLI

To validate configurations and perform OTA updates locally, you'll need to install the ESPHome CLI. Here's how to set it up on Linux with installed Python:

1. Install ESPHome CLI using pip:
```bash
pip3 install esphome
```

2. Update ESPHome CLI when new versions are released:
```bash
pip3 install -U esphome
```

or

```bash
pip3 install esphome --upgrade
```

3. Validate a configuration:
```bash
esphome validate your-config.yaml
```

4. Perform an OTA update:
```bash
esphome run your-config.yaml
```

For more detailed installation and update instructions, refer to the official ESPHome documentation:
- [Getting Started Guide](https://esphome.io/guides/getting_started_command_line)
- [Installation Guide](https://esphome.io/guides/installing_esphome)

---

## Custom Components

This repository includes custom ESPHome components that extend the platform's capabilities beyond the built-in components.

### C++20 Support for ESP32

⚠️ **Important**: The `deduplicate_text` component and electricity meter package use C++17 features (like `std::string_view` and `constexpr` lambdas) and require C++17 or later support when compiling for ESP32. The `uart_line_reader` component works with standard C++11/C++14. ESP8266 has C++17 enabled by default.

**ESPHome 2025.7.0 and Later:**
Starting with ESPHome 2025.7.0, the ESP32 toolchain supports C++20 by default and no manual configuration is required. Components will work out of the box without any additional settings.

**ESPHome Versions Below 2025.7.0 or Standalone Components:**
For ESPHome versions below 2025.7.0, or when using components separately outside of ESPHome, you still need to manually enable C++17 support for ESP32. Add the following to your ESPHome configuration:

```yaml
esphome:
  name: your-device-name
  platformio_options:
    build_unflags:
      - -std=gnu++11
    build_flags:
      - -std=gnu++17
```

### UART Line Reader (`uart_line_reader`)

- **Location:** `components/uart_line_reader/`
- **Description:** Custom text sensor component that reads line-based data from UART interfaces, specifically designed for parsing structured data from smart meters and similar devices.
- **Features:**
  - Line-based UART data reading with configurable delimiters
  - Built-in filtering and validation capabilities
  - Optimized for OBIS protocol parsing from smart meters
  - Supports various UART configurations (baud rate, parity, data bits)
  - Memory-efficient streaming data processing
- **Requirements:** 
  - **ESP32**: Works out of the box (no C++17 required)
  - **ESP8266**: Works out of the box
- **Used by:** 
  - `packages/electricity-meter.yaml` - For reading OBIS data from smart electric meters
  - `smart-electric-meter.yaml` - Smart meter implementation
- **Usage:** Include in your configuration with:
  ```yaml
  external_components:
    - source:
        type: local
        path: components
  ```

### Deduplicate Text Sensor (`deduplicate_text`)

- **Location:** `components/deduplicate_text/`
- **Description:** Custom text sensor platform that provides efficient duplicate detection using hash-based comparison, eliminating the need for manual deduplication filters.
- **Features:**
  - Memory-efficient duplicate detection (only 4 bytes per sensor)
  - FNV-1a hash algorithm for fast, collision-resistant comparison
  - No heap fragmentation - uses stack-allocated hash values
  - Drop-in replacement for template text sensors with automatic deduplication
  - Supports all standard text sensor configuration options (lambda, update_interval, etc.)
  - Built-in logging and debugging capabilities
- **Requirements:** 
  - **ESP32**: C++17 or later support required (see above)
  - **ESP8266**: Works out of the box
- **Examples:** See `components/deduplicate_text/examples/` for complete usage examples
- **Used by:** 
  - `packages/device-configs/electricity-meter.yaml` - For meter information text sensors (serial number, firmware version, etc.)
- **Usage:** Replace `platform: template` with `platform: deduplicate_text`:
  ```yaml
  text_sensor:
    - platform: deduplicate_text
      name: "My Sensor"
      lambda: |-
        return std::string("Hello World");
      update_interval: 60s
  ```

### QRCode2 UART Scanner (`qrcode2_uart`)

- **Location:** `components/qrcode2_uart/`
- **Description:** Custom ESPHome component for interfacing with M5Stack QRCode2 scanners via UART communication. Provides comprehensive barcode and QR code scanning functionality with stock management features.
- **Features:**
  - UART-based communication with M5Stack QRCode2 scanners
  - Configurable scanning timeout (5-60 seconds)
  - Configurable long press duration for mode switching (1-10 seconds)
  - Button support for manual scan triggering and mode switching
  - Device information retrieval (firmware version, hardware model, etc.)
  - Binary sensor for scanning status and text sensor for scan results
  - Multiple automation triggers for scan events and button presses
  - RGB LED integration for visual status indication
  - Stock management modes (Add/Remove stock) with persistence
- **Requirements:** 
  - **ESP32**: Works out of the box (no special C++ requirements)
  - M5Stack QRCode2 scanner module
  - UART pins for communication (TX, RX)
  - GPIO pins for trigger control and button input
- **Documentation:** See `components/qrcode2_uart/README.md` for complete API reference
- **Used by:** 
  - `packages/device-configs/qrcode2-atom-lite.yaml` - M5Stack Atom Lite scanner device configuration
- **Usage:** Include in your configuration with:
  ```yaml
  external_components:
    - source:
        type: local
        path: components
  ```

---

## Shared Packages

The `packages/` directory contains reusable configuration components that can be included in multiple device configurations. These packages help maintain consistency and reduce code duplication across different ESPHome devices.

### WiFi Configuration (`wifi.yaml`)

- **File:** `packages/wifi.yaml`
- **Description:** Standardized WiFi configuration with static IP assignment and access point fallback.
- **Features:**
  - WiFi credentials from secrets
  - Static IP configuration
  - Access point fallback mode
  - Consistent network setup across devices

### WiFi Signal Monitoring (`wifi-signal-sensors.yaml`)

- **File:** `packages/wifi-signal-sensors.yaml`
- **Description:** Comprehensive WiFi signal strength monitoring with both dBm and percentage measurements.
- **Features:**
  - RSSI measurement in dBm
  - WiFi strength percentage calculation
  - Throttled updates (5-minute intervals)
  - Diagnostic entity category
  - Delta filtering for efficient updates

### Factory Reset Button (`factory-reset-button.yaml`)

- **File:** `packages/factory-reset-button.yaml`
- **Description:** Standardized factory reset button configuration for device recovery.
- **Features:**
  - Factory reset functionality
  - Diagnostic entity category
  - Consistent naming and behavior

### MQTT Configuration (`mqtt.yaml`)

- **File:** `packages/mqtt.yaml`
- **Description:** Standardized MQTT configuration with Last Will and Testament (LWT) and Home Assistant discovery.
- **Features:**
  - MQTT broker credentials from secrets
  - Configurable topic prefix
  - Last Will and Testament (LWT) setup for device availability
  - Home Assistant discovery support
  - Configurable LWT topics and payloads
  - Consistent connection setup across devices
- **Required Substitutions:**
  - `mqtt_broker`, `mqtt_username`, `mqtt_password`, `mqtt_topic_prefix`
- **Optional Substitutions:**
  - `mqtt_discovery`, `mqtt_lwt_topic_suffix`, `mqtt_lwt_online_payload`, `mqtt_lwt_offline_payload`

### MQTT Diagnostic Sensors (`mqtt-diagnostic-sensors.yaml`)

- **File:** `packages/mqtt-diagnostic-sensors.yaml`
- **Description:** MQTT connection monitoring and diagnostic information that complements the main MQTT package.
- **Features:**
  - MQTT connect count tracking
  - Connection quality monitoring
  - Diagnostic entity category
  - Disabled by default (can be enabled as needed)
- **Dependencies:** Designed to work alongside `mqtt.yaml` package

### Internal Temperature Sensor - ESP32 (`internal-temp-sensor-esp32.yaml`)

- **File:** `packages/internal-temp-sensor-esp32.yaml`
- **Description:** Internal temperature monitoring for ESP32-based devices.
- **Features:**
  - ESP32 internal temperature sensor
  - Diagnostic entity category
  - Disabled by default (can be enabled as needed)

### Device Configuration Packages

#### Electricity Meter Device Config (`device-configs/electricity-meter.yaml`)

- **File:** `packages/device-configs/electricity-meter.yaml`
- **Description:** Comprehensive device configuration package for reading OBIS-compliant smart electric meters via optical interface (IR head) or serial connection.
- **Features:**
  - OBIS protocol support for energy consumption, power, voltage, current, frequency, and phase angles
  - Data validation and spike protection to filter corrupted readings
  - Communication quality monitoring and corruption statistics
  - Efficient text sensor deduplication using custom `deduplicate_text` component
  - Text sensor corruption filtering (requires 3 consecutive identical readings)
  - Comprehensive energy sensors (total, daily, weekly, monthly, yearly consumption)
  - Reset controls for validation state and communication quality
  - Highly configurable via substitutions (OBIS codes, validation parameters, UART settings)
- **Board:** ESP8266 (tested), ESP32 (compiles but untested on hardware)
- **Requirements:** 
  - Custom `uart_line_reader` component (see [Custom Components](#custom-components))
  - Custom `deduplicate_text` component (see [Custom Components](#custom-components))
  - IR head or serial connection to smart meter
  - **ESP32**: C++17 or later support required for this package (see [C++20 Support](#c20-support-for-esp32))
- **Documentation:** See `packages/device-configs/README-electricity-meter.md` for detailed usage instructions
- **Example:** See `packages/device-configs/examples/example-electric-meter-usage.yaml`
- **Used in:** `smart-electric-meter.yaml` - Smart meter implementation

#### Bluetooth Proxy Device Config (`device-configs/bluetooth-proxy.yaml`)

- **File:** `packages/device-configs/bluetooth-proxy.yaml`
- **Description:** Shared configuration package for ESP32-based Bluetooth proxy devices. Provides consistent hardware setup, network configuration, and monitoring features.
- **Features:**
  - BLE tracking capabilities
  - Safe mode and factory reset buttons
  - OTA and HTTP update support
  - WiFi signal strength monitoring via shared package
  - Static IP and WiFi AP fallback
  - Home Assistant API integration
- **Board:** ESP32
- **Common Config:** Uses shared WiFi and WiFi signal monitoring configuration from `packages/wifi.yaml` and `packages/wifi-signal-sensors.yaml`
- **Used in:** Multiple `bluetooth-proxy-*.yaml` configurations for different locations

#### Soil Sensor D1 Mini Device Config (`device-configs/soil-sensor-d1-mini.yaml`)

- **File:** `packages/device-configs/soil-sensor-d1-mini.yaml`
- **Description:** Shared configuration package for ESP8266 D1 Mini-based soil moisture sensors. Provides consistent hardware setup, network configuration, and monitoring features.
- **Features:**
  - Soil moisture measurement via analog voltage
  - Calibration support (zero and full voltage)
  - Static IP and WiFi AP fallback
  - OTA updates and Home Assistant API integration
  - WiFi signal strength monitoring via shared package
- **Board:** ESP8266 D1 Mini
- **Common Config:** Uses shared WiFi and WiFi signal monitoring configuration from `packages/wifi.yaml` and `packages/wifi-signal-sensors.yaml`
- **Used in:**
  - `soil-sensor-tuyas.yaml` - Soil moisture monitoring implementation

#### QRCode2 Atom Lite Device Config (`device-configs/qrcode2-atom-lite.yaml`)

- **File:** `packages/device-configs/qrcode2-atom-lite.yaml`
- **Description:** Comprehensive device configuration package for M5Stack Atom Lite with QRCode2 Base scanner, designed for inventory management and barcode scanning applications with Home Assistant integration.
- **Features:**
  - Barcode/QR code scanning with configurable timeout (5-60 seconds)
  - Dual stock management modes (Add/Remove stock) with persistence across restarts
  - Button controls: short press to scan, configurable long press (1-10s) to switch modes
  - Comprehensive RGB LED status indication (12+ different states)
  - Home Assistant integration with sensors, controls, and automation triggers
  - Device information retrieval and diagnostic monitoring
  - Scan counting and duplicate detection with timestamps
  - Visual feedback for all device states and user interactions
- **Board:** M5Stack Atom Lite (ESP32-based)
- **Requirements:** 
  - Custom `qrcode2_uart` component (see [Custom Components](#custom-components))
  - M5Stack QRCode2 Base scanner module
  - Home Assistant for full feature utilization
- **Hardware:** Built-in SK6812 RGB LED (GPIO 27), Button (GPIO 39), UART pins (TX 22, RX 19)
- **Documentation:** See `packages/device-configs/qrcode2-atom-lite.md` for detailed hardware setup, pin connections, and usage instructions
- **Used in:** QRCode2 scanner device configurations for inventory management

#### DFRobot SEN0610 mmWave Presence Sensor Device Config (`device-configs/dfrobot-sen0610-presence.yaml`)

- **File:** `packages/device-configs/dfrobot-sen0610-presence.yaml`
- **Description:** Comprehensive device configuration package for the DFRobot SEN0610 mmWave presence sensor, enabling advanced occupancy detection, adjustable range and sensitivity, and Home Assistant integration.
- **Features:**
  - Real-time occupancy detection using mmWave radar
  - Adjustable detection range (min, max, trigger)
  - Sensitivity and latency controls
  - UART communication and raw message debugging
  - Home Assistant integration with sensors, numbers, switches, and buttons
  - Factory reset, restart, and configuration query buttons
- **Board:** ESP32 (tested), ESP8266 (should work)
- **Documentation:** See `packages/device-configs/dfrobot-sen0610-presence.md` for detailed usage instructions
- **Example:** See `packages/device-configs/examples/example-dfrobot-sen0610-usage.yaml`

#### RTTTL Buzzer Device Config (`device-configs/rtttl-buzzer.yaml`)

- **File:** `packages/device-configs/rtttl-buzzer.yaml`
- **Description:** Comprehensive device configuration package for RTTTL (Ring Tone Text Transfer Language) buzzer functionality, enabling musical notifications, alerts, and sound effects with Home Assistant integration.
- **Features:**
  - RTTTL playback with runtime volume control (0-100%)
  - API actions for dynamic song playback from Home Assistant
  - Predefined notification sounds (startup, error, success, doorbell, notification)
  - Flexible GPIO pin configuration for buzzer output
  - Cross-platform PWM output for ESP32 and ESP8266
  - Home Assistant buttons for testing predefined sounds
  - Support for custom RTTTL melodies and ringtones
- **Board:** ESP32 and ESP8266 (requires platform-specific substitution)
- **Hardware:** Passive buzzer (required for RTTTL functionality) or speaker with driver/amplifier
- **Documentation:** See `packages/device-configs/rtttl-buzzer.md` for detailed usage instructions
- **Example:** See `packages/device-configs/examples/example-rtttl-buzzer-usage.yaml`

---

## Configurations

### Smart Electric Meter

- **File:** `smart-electric-meter.yaml`
- **Description:** ESP8266-based smart electric meter reader using optical interface (IR head) to read OBIS-compliant electric meters.
- **Features:**
  - OBIS protocol support for comprehensive meter readings
  - Energy consumption tracking (total, daily, weekly, monthly, yearly)
  - Real-time power, voltage, current, and frequency monitoring
  - Phase angle measurements for three-phase systems
  - Data validation and corruption filtering
  - Communication quality monitoring
  - MQTT integration for Home Assistant
  - WiFi signal strength monitoring via shared package
- **Board:** ESP8266 D1 Mini (tested), ESP32 (compiles but untested on hardware)
- **Hardware:** Requires IR head for optical interface communication
- **Dependencies:** Uses custom `uart_line_reader` and `deduplicate_text` components (see [Custom Components](#custom-components))
- **Common Config:** Uses electricity meter device config from `packages/device-configs/electricity-meter.yaml`, MQTT configuration from `packages/mqtt.yaml` and `packages/mqtt-diagnostic-sensors.yaml`, and WiFi configuration from `packages/wifi.yaml`

### Gas Meter (Gaszaehler)

- **File:** `gaszahler.yaml`
- **Description:** ESP8266-based gas meter pulse counter using a reed switch. Tracks gas consumption and exposes it to Home Assistant.
- **Features:** 
  - Pulse counting via GPIO
  - Total consumption calculation
  - Optional LED pulse indicator
  - Scheduled weekly restart
  - Home Assistant API integration
  - WiFi signal strength monitoring via shared package
- **Based on:** [be-jo.net Gaszähler mit ESPHome](https://be-jo.net/2022/02/home-assistant-gaszaehler-mit-esphome-auslesen-flashen-unter-wsl/)
- **Common Config:** Uses shared WiFi and WiFi signal monitoring configuration from `packages/wifi.yaml` and `packages/wifi-signal-sensors.yaml`

### Water Level Reservoir (wasserstand-regenreservoir)

- **File:** `wasserstand-regenreservoir.yaml`
- **Description:** Monitors water level in a reservoir using an ADS1115 ADC and pressure sensor, with volume calculation and Home Assistant integration.
- **Features:**
  - Analog water level measurement
  - Volume calculation via calibration curve
  - I2C and optional Dallas temperature sensor support
  - WiFi signal strength monitoring via shared package
- **Based on:** [nachbelichtet.com Water Level in Cisterns](https://nachbelichtet.com/en/measure-water-level-in-cisterns-and-tanks-with-homeassistant-esphome-and-tl-136-pressure-sensor-update-2)
- **Common Config:** Uses shared WiFi and WiFi signal monitoring configuration from `packages/wifi.yaml` and `packages/wifi-signal-sensors.yaml`

### Air Quality Sensor 1

- **File:** `air-quality-sensor-1.yaml`
- **Description:** Advanced air quality monitor using ESP32-S2, supporting multiple sensors (CO2, VOC, PM, temperature, humidity, etc.) and a display.
- **Features:**
  - Multiple air quality sensors (SGP30, SGP41, PMS, etc.)
  - Display with dimming and status indicators
  - Home Assistant integration
  - WiFi signal strength monitoring via shared package
- **Based on:** [Tom's 3D SBR1 Sensor Box](https://go.toms3d.org/sbr1)
- **Common Config:** Uses shared WiFi and WiFi signal monitoring configuration from `packages/wifi.yaml` and `packages/wifi-signal-sensors.yaml`

### Bluetooth Proxy

- **Files:** `bluetooth-proxy-*.yaml`
- **Description:** ESP32-based Bluetooth proxies for extending Bluetooth coverage in Home Assistant.
- **Features:**
  - BLE tracking
  - Safe mode and factory reset buttons
  - OTA and HTTP update support
  - WiFi signal strength monitoring via shared package
- **Common Config:** Uses shared Bluetooth proxy device configuration from `packages/device-configs/bluetooth-proxy.yaml`

### Info Screen

- **File:** `lilygos3-info-screen.yaml`
- **Description:** ESP32-S3-based information display for Home Assistant data, including temperature, humidity, waste collection, and more.
- **Features:**
  - Custom display layouts
  - Battery and WiFi status
  - Home Assistant API integration
  - WiFi signal strength monitoring via shared package
- **Common Config:** Uses shared WiFi and WiFi signal monitoring configuration from `packages/wifi.yaml` and `packages/wifi-signal-sensors.yaml`

### Soil Sensor

- **File:** `soil-sensor-tuyas.yaml`
- **Description:** Soil moisture monitoring implementation using ESP8266 D1 Mini.
- **Features:**
  - Soil moisture measurement via analog voltage
  - Calibration support (zero and full voltage)
  - Static IP and WiFi AP fallback
  - OTA updates and Home Assistant API integration
  - WiFi signal strength monitoring via shared package
- **Common Config:** Uses shared soil sensor D1 Mini device configuration from `packages/device-configs/soil-sensor-d1-mini.yaml`

### Garden Watering Controller

- **File:** `garden-watering-controller.yaml`
- **Description:** ESP32-based garden watering controller (Work in Progress). Currently implements basic pump control with plans to expand using ESPHome's Sprinkler Controller component for full zone management.
- **Features:**
  - Direct main pump control via physical switch
  - 8 GPIO outputs via XL9535 I/O expander (prepared for future zone control)
  - Internal temperature monitoring
  - WiFi signal strength monitoring via shared package
  - Static IP and WiFi AP fallback
  - OTA updates and Home Assistant API integration
- **Board:** ESP32 DevKit
- **Common Config:** Uses shared WiFi and WiFi signal monitoring configuration from `packages/wifi.yaml` and `packages/wifi-signal-sensors.yaml`
- **Future Plans:** Will be enhanced using [ESPHome's Sprinkler Controller component](https://esphome.io/components/sprinkler.html) for advanced zone management, scheduling, and automation features.

### QRCode2 Scanner

- **Files:** `qrcode2-atom-lite-*.yaml`
- **Description:** M5Stack Atom Lite-based barcode and QR code scanners designed for inventory management with comprehensive Home Assistant integration.
- **Features:**
  - Barcode and QR code scanning with M5Stack QRCode2 Base
  - Dual stock management modes (Add Stock / Remove Stock)
  - Configurable scanning timeout (5-60 seconds) and button long press duration (1-10 seconds)
  - Comprehensive RGB LED status indication for all device states
  - Button controls: short press to scan, long press to switch modes
  - Scan counting and duplicate detection with real timestamps
  - Device information display and diagnostic monitoring
  - Home Assistant sensors, controls, and automation triggers
  - Visual feedback for WiFi/HA connectivity, scanning status, mode changes, and errors
- **Board:** M5Stack Atom Lite (ESP32-based)
- **Hardware Requirements:** M5Stack QRCode2 Base scanner module
- **Dependencies:** Uses custom `qrcode2_uart` component (see [Custom Components](#custom-components))
- **Common Config:** Uses shared QRCode2 Atom Lite device configuration from `packages/device-configs/qrcode2-atom-lite.yaml`
- **Documentation:** See `packages/device-configs/qrcode2-atom-lite.md` for complete setup and usage guide

### DFRobot SEN0610 mmWave Presence Sensor

- **File:** `mmwave-presence-1.yaml` (example)
- **Description:** ESP32-based mmWave presence sensor using the DFRobot SEN0610 module for advanced occupancy detection and automation.
- **Features:**
  - Real-time occupancy detection
  - Adjustable detection range and sensitivity
  - Home Assistant integration with sensors, numbers, switches, and buttons
  - Factory reset, restart, and configuration query
- **Board:** ESP32 (tested), ESP8266 (should work)
- **Hardware Requirements:** DFRobot SEN0610 mmWave sensor, UART wiring
- **Dependencies:** Uses shared device config from `packages/device-configs/dfrobot-sen0610-presence.yaml`
- **Documentation:** See `packages/device-configs/dfrobot-sen0610-presence.md` for complete setup and usage guide
- **Example:** See `packages/device-configs/examples/example-dfrobot-sen0610-usage.yaml`

---

## Secrets & Sensitive Data

All sensitive information (WiFi credentials, API keys, etc.) is stored in `secrets.yaml`, which is **not** included in this repository. You must create your own `secrets.yaml` file with the required values to use these configurations.

---

## Credits & Inspirations

Some configurations are based on or inspired by the following awesome community projects:

- **Gaszaehler:** [be-jo.net Gaszähler mit ESPHome](https://be-jo.net/2022/02/home-assistant-gaszaehler-mit-esphome-auslesen-flashen-unter-wsl/)
- **wasserstand-regenreservoir:** [nachbelichtet.com Water Level in Cisterns](https://nachbelichtet.com/en/measure-water-level-in-cisterns-and-tanks-with-homeassistant-esphome-and-tl-136-pressure-sensor-update-2)
- **air-quality-sensor-1:** [Tom's 3D SBR1 Sensor Box](https://go.toms3d.org/sbr1)

---

## License

See [LICENSE](LICENSE) for details.
