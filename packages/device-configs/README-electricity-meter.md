# Electricity Meter Package

A comprehensive ESPHome package for reading OBIS-compliant smart electric meters via optical interface (IR head) or serial connection.

## Features

- **OBIS Protocol Support**: Reads standard OBIS codes for energy consumption, power, voltage, current, frequency, and phase angles
- **Data Validation**: Built-in validation to filter out corrupted readings and unrealistic values
- **Spike Protection**: Prevents sudden large changes in sensor values that could indicate data corruption
- **Communication Quality Monitoring**: Tracks frame reception rates and corruption statistics
- **Text Sensor Filtering**: Requires 3 consecutive identical readings for text sensors to prevent corruption
- **Comprehensive Sensors**: Supports energy consumption (total, daily, weekly, monthly, yearly), power, voltage, current, frequency, and phase angle measurements
- **Reset Controls**: Buttons to reset validation state and communication quality counters
- **Highly Configurable**: All OBIS codes and validation parameters configurable via substitutions

## Important Notes

⚠️ **Custom Component Required**: This package uses a custom `uart_line_reader` component. You must include the external components in your main configuration:

```yaml
external_components:
  - source:
      type: local
      path: components
```

⚠️ **Linter Warning**: ESPHome linters may show warnings about "Platform not found: 'text_sensor.uart_line_reader'" when checking the package file directly. This is expected and will resolve when the package is used in a configuration that includes the external component.

## Required Substitutions

### UART Configuration
```yaml
uart_rx_pin: "D2"          # GPIO pin for UART RX
uart_baud_rate: "9600"     # UART baud rate (1200, 2400, 4800, 9600)
uart_parity: "EVEN"        # UART parity (EVEN, ODD, NONE)
uart_data_bits: "7"        # UART data bits (7, 8)
uart_stop_bits: "1"        # UART stop bits (usually 1)
uart_rx_buffer_size: "1024" # UART RX buffer size
```

### OBIS Codes
All OBIS codes must be defined according to your meter's specification:

```yaml
# Energy consumption
obis_total_consumption: "1-0:1.8.0*255"
obis_consumption_1d: "1-0:1.8.0*96"
obis_consumption_7d: "1-0:1.8.0*97"
obis_consumption_30d: "1-0:1.8.0*98"
obis_consumption_365d: "1-0:1.8.0*99"
obis_consumption_t1: "1-0:1.8.1*255"
obis_consumption_t2: "1-0:1.8.2*255"
obis_consumption_since_reset: "1-0:1.8.0*100"

# Power and electrical parameters
obis_current_power: "1-0:16.7.0*255"
obis_voltage_l1: "1-0:32.7.0*255"
obis_voltage_l2: "1-0:52.7.0*255"
obis_voltage_l3: "1-0:72.7.0*255"
obis_current_l1: "1-0:31.7.0*255"
obis_current_l2: "1-0:51.7.0*255"
obis_current_l3: "1-0:71.7.0*255"
obis_frequency: "1-0:14.7.0*255"

# Phase angles
obis_phase_angle_ul2_ul1: "1-0:81.7.1*255"
obis_phase_angle_ul3_ul1: "1-0:81.7.2*255"
obis_phase_angle_il1_ul1: "1-0:81.7.4*255"
obis_phase_angle_il2_ul2: "1-0:81.7.15*255"
obis_phase_angle_il3_ul3: "1-0:81.7.26*255"

# Meter information
obis_serial_number: "1-0:96.1.0*255"
obis_firmware_version: "1-0:0.2.0*255"
obis_parameter_crc: "1-0:96.90.2*255"
obis_status_register: "1-0:97.97.0*255"
```

### Validation Constants
```yaml
# Spike protection limits
max_power_change: "3000.0"    # Maximum power change per reading (W)
max_current_change: "13.0"    # Maximum current change per reading (A)
max_energy_change: "1.0"      # Maximum energy change per reading (kWh)

# Absolute limits
max_energy_total: "100000.0"  # Maximum total energy (kWh)
max_absolute_power: "34500.0" # Maximum absolute power (W)
min_energy_value: "0.0"       # Minimum energy value (kWh)
min_power_value: "0.0"        # Minimum power value (W)

# Voltage limits
max_voltage: "280.0"          # Maximum voltage (V)
min_voltage: "0.0"            # Minimum voltage (V)

# Current limits
max_current: "30.0"           # Maximum current (A)
min_current: "0.0"            # Minimum current (A)

# Frequency limits
max_frequency: "52.0"         # Maximum frequency (Hz)
min_frequency: "48.0"         # Minimum frequency (Hz)

# Phase angle limits
max_phase_angle: "360.0"      # Maximum phase angle (degrees)
min_phase_angle: "-360.0"     # Minimum phase angle (degrees)
```

## Usage

1. **Include the package** in your ESPHome configuration:
```yaml
packages:
  electricity-meter: !include packages/electricity-meter.yaml
```

2. **Define all required substitutions** (see above sections)

3. **Configure UART hardware** according to your meter's specifications

4. **Connect the IR head** to the specified UART RX pin

## Hardware Setup

### IR Head Connection
- Connect the IR head's data output to the specified UART RX pin
- Connect power and ground as required by your IR head
- Position the IR head over the meter's optical interface

### Common UART Settings by Meter Type
- **German smart meters**: Often use 9600 baud, 7 data bits, even parity
- **Older meters**: May use 2400 or 4800 baud
- **Check your meter's documentation** for the correct settings

## Sensors Provided

### Energy Sensors
- `Total Consumption` - Total energy consumption (kWh)
- `Consumption 1 Day` - Energy consumption last 24 hours
- `Consumption 7 Days` - Energy consumption last 7 days
- `Consumption 30 Days` - Energy consumption last 30 days
- `Consumption 365 Days` - Energy consumption last 365 days
- `Consumption T1` - Energy consumption tariff 1
- `Consumption T2` - Energy consumption tariff 2
- `Consumption Since Reset` - Energy since last reset

### Power and Electrical Sensors
- `Current Power` - Current power consumption (W)
- `Voltage L1/L2/L3` - Line voltages (V)
- `Current L1/L2/L3` - Line currents (A)
- `Frequency` - Network frequency (Hz)
- `Phase Angle UL2:UL1` - Phase angle between L2 and L1 voltages
- `Phase Angle UL3:UL1` - Phase angle between L3 and L1 voltages
- `Phase Angle IL1:UL1` - Phase angle between L1 current and voltage
- `Phase Angle IL2:UL2` - Phase angle between L2 current and voltage
- `Phase Angle IL3:UL3` - Phase angle between L3 current and voltage

### Diagnostic Sensors
- `OBIS Valid Frame Rate` - Percentage of successfully processed frames
- `Serial Number` - Meter serial number
- `Firmware Version` - Meter firmware version
- `Parameter CRC` - Parameter checksum
- `Status Register` - Meter status information

### Control Buttons
- `Reset Validation State` - Resets all validation counters
- `Reset Communication Quality` - Resets communication quality counters

## Data Validation

The package includes several validation mechanisms:

1. **Energy Validation**: Ensures energy values are ever-growing and changes are reasonable
2. **Spike Protection**: Prevents sudden large changes in power and current readings
3. **Text Sensor Filtering**: Requires 3 consecutive identical readings for text sensors
4. **Bounds Checking**: Clamps all values to realistic ranges
5. **Corruption Detection**: Tracks and reports data corruption events

## Troubleshooting

### No Data Received
- Check UART wiring and settings
- Verify IR head is properly positioned
- Check baud rate and parity settings
- Enable DEBUG logging to see raw UART data

### Incorrect Values
- Verify OBIS codes match your meter's specification
- Check units and scaling factors
- Adjust validation constants if needed
- Review meter documentation for correct codes

### High Corruption Rate
- Check IR head positioning and cleanliness
- Verify UART settings match meter output
- Check for electrical interference
- Ensure stable power supply

## Example Configuration

See `packages/device-configs/examples/example-electric-meter-usage.yaml` for a complete example configuration.

## Compatibility

This package is designed to work with:
- OBIS-compliant smart meters
- IEC 62056-21 protocol
- ESPHome version 2024.6.0 or later
- ESP8266 and ESP32 platforms

## Contributing

When adding support for new meter types:
1. Add the specific OBIS codes to the documentation
2. Test with real hardware
3. Update validation constants as needed
4. Consider adding meter-specific examples 