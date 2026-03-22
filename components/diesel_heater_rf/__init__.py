import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, text_sensor, binary_sensor
from esphome.const import (
    CONF_ID,
    CONF_UPDATE_INTERVAL,
    UNIT_CELSIUS,
    UNIT_VOLT,
    UNIT_DECIBEL_MILLIWATT,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    STATE_CLASS_MEASUREMENT,
)

DEPENDENCIES = ["api"]
AUTO_LOAD = ["sensor", "text_sensor", "binary_sensor"]

diesel_heater_rf_ns = cg.esphome_ns.namespace("diesel_heater_rf")
DieselHeaterRFComponent = diesel_heater_rf_ns.class_(
    "DieselHeaterRFComponent", cg.PollingComponent
)

CONF_HEATER_ADDRESS = "heater_address"
CONF_SCK_PIN = "sck_pin"
CONF_MISO_PIN = "miso_pin"
CONF_MOSI_PIN = "mosi_pin"
CONF_CS_PIN = "cs_pin"
CONF_GDO2_PIN = "gdo2_pin"
CONF_STATE_SENSOR = "state_sensor"
CONF_VOLTAGE_SENSOR = "voltage_sensor"
CONF_AMBIENT_TEMP_SENSOR = "ambient_temp_sensor"
CONF_CASE_TEMP_SENSOR = "case_temp_sensor"
CONF_SETPOINT_SENSOR = "setpoint_sensor"
CONF_HEAT_LEVEL_SENSOR = "heat_level_sensor"
CONF_PUMP_FREQ_SENSOR = "pump_freq_sensor"
CONF_RSSI_SENSOR = "rssi_sensor"
CONF_AUTO_MODE_SENSOR = "auto_mode_sensor"
CONF_FOUND_ADDRESS_SENSOR = "found_address_sensor"
CONF_TRANSCEIVER_STATUS_SENSOR = "transceiver_status_sensor"
CONF_FREQUENCY = "frequency"
CONF_FREQUENCY_OFFSET_HZ = "frequency_offset_hz"
CONF_CCA_MODE = "cca_mode"

FREQUENCY_PRESETS = {
    "433": (0x10, 0xB0, 0x9E),  # 433.938 MHz — FREQ_REG=1,093,790; nominal 433.92 but measured heater center is ~+19 kHz higher
    "868": (0x21, 0x65, 0x6F),  # 868.30 MHz — FREQ_REG=2,188,655 = 868,300 kHz
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DieselHeaterRFComponent),
        cv.Required(CONF_HEATER_ADDRESS): cv.hex_int,
        cv.Optional(CONF_SCK_PIN, default=18): cv.int_,
        cv.Optional(CONF_MISO_PIN, default=19): cv.int_,
        cv.Optional(CONF_MOSI_PIN, default=23): cv.int_,
        cv.Optional(CONF_CS_PIN, default=5): cv.int_,
        cv.Optional(CONF_GDO2_PIN, default=4): cv.int_,
        cv.Optional(CONF_FREQUENCY, default="433"): cv.one_of(*FREQUENCY_PRESETS, lower=True),
        cv.Optional(CONF_FREQUENCY_OFFSET_HZ, default=0): cv.int_,
        cv.Optional(CONF_CCA_MODE, default=0): cv.int_range(min=0, max=3),
        cv.Optional(CONF_STATE_SENSOR): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_VOLTAGE_SENSOR): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_AMBIENT_TEMP_SENSOR): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_CASE_TEMP_SENSOR): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_SETPOINT_SENSOR): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_HEAT_LEVEL_SENSOR): sensor.sensor_schema(
            unit_of_measurement="",
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_PUMP_FREQ_SENSOR): sensor.sensor_schema(
            unit_of_measurement="Hz",
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_RSSI_SENSOR): sensor.sensor_schema(
            unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_AUTO_MODE_SENSOR): binary_sensor.binary_sensor_schema(),
        cv.Optional(CONF_FOUND_ADDRESS_SENSOR): text_sensor.text_sensor_schema(
            entity_category="diagnostic",
            icon="mdi:identifier",
        ),
        cv.Optional(CONF_TRANSCEIVER_STATUS_SENSOR): text_sensor.text_sensor_schema(
            entity_category="diagnostic",
            icon="mdi:radio-tower",
        ),
    }
).extend(cv.polling_component_schema("60s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add_library("SPI", None)

    cg.add(var.set_heater_address(config[CONF_HEATER_ADDRESS]))
    cg.add(var.set_sck_pin(config[CONF_SCK_PIN]))
    cg.add(var.set_miso_pin(config[CONF_MISO_PIN]))
    cg.add(var.set_mosi_pin(config[CONF_MOSI_PIN]))
    cg.add(var.set_cs_pin(config[CONF_CS_PIN]))
    cg.add(var.set_gdo2_pin(config[CONF_GDO2_PIN]))

    f2, f1, f0 = FREQUENCY_PRESETS[config[CONF_FREQUENCY]]
    offset_hz = config[CONF_FREQUENCY_OFFSET_HZ]
    if offset_hz != 0:
        freq_reg = (f2 << 16) | (f1 << 8) | f0
        delta = round(offset_hz * 65536 / 26_000_000)
        freq_reg = max(0, min(0xFFFFFF, freq_reg + delta))
        f2 = (freq_reg >> 16) & 0xFF
        f1 = (freq_reg >> 8) & 0xFF
        f0 = freq_reg & 0xFF
    cg.add(var.set_freq(f2, f1, f0))
    cg.add(var.set_cca_mode(config[CONF_CCA_MODE]))

    if CONF_STATE_SENSOR in config:
        s = await text_sensor.new_text_sensor(config[CONF_STATE_SENSOR])
        cg.add(var.set_state_sensor(s))

    if CONF_VOLTAGE_SENSOR in config:
        s = await sensor.new_sensor(config[CONF_VOLTAGE_SENSOR])
        cg.add(var.set_voltage_sensor(s))

    if CONF_AMBIENT_TEMP_SENSOR in config:
        s = await sensor.new_sensor(config[CONF_AMBIENT_TEMP_SENSOR])
        cg.add(var.set_ambient_temp_sensor(s))

    if CONF_CASE_TEMP_SENSOR in config:
        s = await sensor.new_sensor(config[CONF_CASE_TEMP_SENSOR])
        cg.add(var.set_case_temp_sensor(s))

    if CONF_SETPOINT_SENSOR in config:
        s = await sensor.new_sensor(config[CONF_SETPOINT_SENSOR])
        cg.add(var.set_setpoint_sensor(s))

    if CONF_HEAT_LEVEL_SENSOR in config:
        s = await sensor.new_sensor(config[CONF_HEAT_LEVEL_SENSOR])
        cg.add(var.set_heat_level_sensor(s))

    if CONF_PUMP_FREQ_SENSOR in config:
        s = await sensor.new_sensor(config[CONF_PUMP_FREQ_SENSOR])
        cg.add(var.set_pump_freq_sensor(s))

    if CONF_RSSI_SENSOR in config:
        s = await sensor.new_sensor(config[CONF_RSSI_SENSOR])
        cg.add(var.set_rssi_sensor(s))

    if CONF_AUTO_MODE_SENSOR in config:
        s = await binary_sensor.new_binary_sensor(config[CONF_AUTO_MODE_SENSOR])
        cg.add(var.set_auto_mode_sensor(s))

    if CONF_FOUND_ADDRESS_SENSOR in config:
        s = await text_sensor.new_text_sensor(config[CONF_FOUND_ADDRESS_SENSOR])
        cg.add(var.set_found_address_sensor(s))

    if CONF_TRANSCEIVER_STATUS_SENSOR in config:
        s = await text_sensor.new_text_sensor(config[CONF_TRANSCEIVER_STATUS_SENSOR])
        cg.add(var.set_transceiver_status_sensor(s))
