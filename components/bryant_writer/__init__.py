import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.components.bryant_framer import BryantFramer
from esphome.const import CONF_ID

DEPENDENCIES = ["uart", "bryant_framer"]
writer_ns = cg.esphome_ns.namespace("bryant_writer")
BryantWriter = writer_ns.class_("BryantWriter", cg.Component, uart.UARTDevice)

POLL = cv.Schema({
    cv.Required("name"): cv.string,
    cv.Required("frame"): cv.string,
    cv.Optional("offset_ms", default=0): cv.int_range(min=0, max=49999),
})
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(BryantWriter),
    cv.Required("uart_id"): cv.use_id(uart.UARTComponent),
    cv.Required("framer_id"): cv.use_id(BryantFramer),
    cv.Optional("idle_ms", default=50): cv.int_range(min=20, max=5000),
    cv.Optional("poll_interval", default="50s"): cv.positive_time_period_milliseconds,
    cv.Optional("polls", default=[]): cv.ensure_list(POLL),
    cv.Optional("log_tx", default=True): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    framer = await cg.get_variable(config["framer_id"])
    cg.add(var.set_framer(framer))
    cg.add(var.set_idle_ms(config["idle_ms"]))
    cg.add(var.set_poll_interval_ms(config["poll_interval"].total_milliseconds))
    cg.add(var.set_log_tx(config["log_tx"]))
    for poll in config["polls"]:
        cg.add(var.add_poll(poll["name"], poll["frame"], poll["offset_ms"]))
