# ESP Home Configurations

This repository contains a collection of ESPHome YAML configurations for various smart home sensors and devices. The goal is to provide ready-to-use, well-documented configurations for ESP8266 and ESP32-based devices, with a focus on Home Assistant integration.

## Table of Contents

- [Development Setup](#development-setup)
- [Shared Packages](#shared-packages)
- [Configurations](#configurations)
  - [Gas Meter (Gaszaehler)](#gas-meter-gaszaehler)
  - [Water Level Reservoir (wasserstand-regenreservoir)](#water-level-reservoir-wasserstand-regenreservoir)
  - [Air Quality Sensor 1](#air-quality-sensor-1)
  - [Bluetooth Proxy](#bluetooth-proxy)
  - [Info Screen](#info-screen)
  - [Soil Sensor](#soil-sensor)
  - [Garden Watering Controller](#garden-watering-controller)
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

### Internal Temperature Sensor - ESP32 (`internal-temp-sensor-esp32.yaml`)

- **File:** `packages/internal-temp-sensor-esp32.yaml`
- **Description:** Internal temperature monitoring for ESP32-based devices.
- **Features:**
  - ESP32 internal temperature sensor
  - Diagnostic entity category
  - Disabled by default (can be enabled as needed)

### Device Configuration Packages

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

---

## Configurations

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
