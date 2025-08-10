import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID
from . import qrcode2_uart_ns, QRCode2UARTComponent

DEPENDENCIES = ["qrcode2_uart"]

QRCode2TextSensor = qrcode2_uart_ns.class_("QRCode2TextSensor", text_sensor.TextSensor, cg.Component)

CONF_QRCODE2_UART_ID = "qrcode2_uart_id"

CONFIG_SCHEMA = text_sensor.text_sensor_schema(QRCode2TextSensor).extend(
    {
        cv.GenerateID(): cv.declare_id(QRCode2TextSensor),
        cv.Required(CONF_QRCODE2_UART_ID): cv.use_id(QRCode2UARTComponent),
    }
)

async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)
    
    parent = await cg.get_variable(config[CONF_QRCODE2_UART_ID])
    cg.add(var.set_parent(parent))
    cg.add(parent.add_text_sensor(var))
