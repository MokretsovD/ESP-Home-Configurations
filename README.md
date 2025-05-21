# ESP Home Configurations

This repository contains a collection of ESPHome YAML configurations for various smart home sensors and devices. The goal is to provide ready-to-use, well-documented configurations for ESP8266 and ESP32-based devices, with a focus on Home Assistant integration.

## Table of Contents

- [Overview](#overview)
- [Development Setup](#development-setup)
- [Configurations](#configurations)
  - [Gas Meter (Gaszaehler)](#gas-meter-gaszaehler)
  - [Water Level Reservoir (wasserstand-regenreservoir)](#water-level-reservoir-wasserstand-regenreservoir)
  - [Air Quality Sensor 1](#air-quality-sensor-1)
  - [Bluetooth Proxy](#bluetooth-proxy)
  - [Info Screen](#info-screen)
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
- **Based on:** [be-jo.net Gaszähler mit ESPHome](https://be-jo.net/2022/02/home-assistant-gaszaehler-mit-esphome-auslesen-flashen-unter-wsl/)

### Water Level Reservoir (wasserstand-regenreservoir)

- **File:** `wasserstand-regenreservoir.yaml`
- **Description:** Monitors water level in a reservoir using an ADS1115 ADC and pressure sensor, with volume calculation and Home Assistant integration.
- **Features:**
  - Analog water level measurement
  - Volume calculation via calibration curve
  - I2C and optional Dallas temperature sensor support
- **Based on:** [nachbelichtet.com Water Level in Cisterns](https://nachbelichtet.com/en/measure-water-level-in-cisterns-and-tanks-with-homeassistant-esphome-and-tl-136-pressure-sensor-update-2)

### Air Quality Sensor 1

- **File:** `air-quality-sensor-1.yaml`
- **Description:** Advanced air quality monitor using ESP32-S2, supporting multiple sensors (CO2, VOC, PM, temperature, humidity, etc.) and a display.
- **Features:**
  - Multiple air quality sensors (SGP30, SGP41, PMS, etc.)
  - Display with dimming and status indicators
  - Home Assistant integration
- **Based on:** [Tom's 3D SBR1 Sensor Box](https://go.toms3d.org/sbr1)

### Bluetooth Proxy

- **Files:** `bluetooth-proxy-*.yaml`, `packages/device-configs/bluetooth-proxy.yaml`
- **Description:** ESP32-based Bluetooth proxies for extending Bluetooth coverage in Home Assistant.
- **Features:**
  - BLE tracking
  - Safe mode and factory reset buttons
  - OTA and HTTP update support

### Info Screen

- **File:** `lilygos3-info-screen.yaml`
- **Description:** ESP32-S3-based information display for Home Assistant data, including temperature, humidity, waste collection, and more.
- **Features:**
  - Custom display layouts
  - Battery and WiFi status
  - Home Assistant API integration

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
