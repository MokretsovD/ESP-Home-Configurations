import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_UPDATE_INTERVAL
from esphome.core import coroutine_with_priority

deduplicate_text_ns = cg.esphome_ns.namespace('deduplicate_text')
DeduplicateTextSensor = deduplicate_text_ns.class_('DeduplicateTextSensor', text_sensor.TextSensor, cg.Component)

CONFIG_SCHEMA = text_sensor.text_sensor_schema(DeduplicateTextSensor).extend({
    cv.GenerateID(): cv.declare_id(DeduplicateTextSensor),
    cv.Optional(CONF_LAMBDA): cv.returning_lambda,
    cv.Optional(CONF_UPDATE_INTERVAL): cv.positive_time_period_milliseconds,
}).extend(cv.COMPONENT_SCHEMA)

@coroutine_with_priority(40.0)
async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)
    
    if CONF_LAMBDA in config:
        template_ = await cg.process_lambda(config[CONF_LAMBDA], [], return_type=cg.optional.template(cg.std_string))
        cg.add(var.set_template(template_))
    
    if CONF_UPDATE_INTERVAL in config:
        cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL])) 