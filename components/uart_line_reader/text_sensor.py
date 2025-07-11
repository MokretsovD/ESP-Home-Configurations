import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor, uart
from esphome.const import CONF_ID, CONF_UART_ID

uart_line_reader_ns = cg.esphome_ns.namespace('uart_line_reader')
UartLineReaderTextSensor = uart_line_reader_ns.class_('UartLineReaderTextSensor', text_sensor.TextSensor, cg.Component, uart.UARTDevice)

CONFIG_SCHEMA = text_sensor.text_sensor_schema(UartLineReaderTextSensor).extend(
    {
        cv.GenerateID(): cv.declare_id(UartLineReaderTextSensor),
        cv.Required(CONF_UART_ID): cv.use_id(uart.UARTComponent),
    }
)

async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
 