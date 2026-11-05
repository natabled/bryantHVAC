import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.components.bryant_framer import BryantFramer
from esphome.components.bryant_writer import BryantWriter
from esphome.const import CONF_ID

DEPENDENCIES = ["bryant_framer", "bryant_writer"]
ns = cg.esphome_ns.namespace("bryant_set")
BryantSet = ns.class_("BryantSet", cg.Component)
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(BryantSet),
    cv.Required("framer_id"): cv.use_id(BryantFramer),
    cv.Required("writer_id"): cv.use_id(BryantWriter),
    cv.Required("status_text_id"): cv.use_id(text_sensor.TextSensor),
    cv.Optional("thermostat_address", default=0x20): cv.int_range(min=0, max=255),
    cv.Optional("sam_address", default=0x92): cv.int_range(min=0, max=255),
    cv.Optional("timeout", default="15s"): cv.positive_time_period_milliseconds,
    cv.Optional("verify_delay", default="500ms"): cv.positive_time_period_milliseconds,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_framer(await cg.get_variable(config["framer_id"])))
    cg.add(var.set_writer(await cg.get_variable(config["writer_id"])))
    cg.add(var.set_status_sensor(await cg.get_variable(config["status_text_id"])))
    cg.add(var.set_thermostat_address(config["thermostat_address"]))
    cg.add(var.set_sam_address(config["sam_address"]))
    cg.add(var.set_timeout_ms(config["timeout"].total_milliseconds))
    cg.add(var.set_verify_delay_ms(config["verify_delay"].total_milliseconds))
