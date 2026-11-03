import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]

bryant_framer_ns = cg.esphome_ns.namespace("bryant_framer")
BryantFramer = bryant_framer_ns.class_("BryantFramer", cg.Component, uart.UARTDevice)

CONF_UART_ID = "uart_id"
CONF_DRAIN_INTERVAL = "drain_interval"
CONF_RING_BUFFER_SIZE = "ring_buffer_size"
CONF_MAX_PAYLOAD_LENGTH = "max_payload_length"
CONF_LOG_VALID_FRAMES = "log_valid_frames"
CONF_LOG_FRAME_ERRORS = "log_frame_errors"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BryantFramer),
        cv.Required(CONF_UART_ID): cv.use_id(uart.UARTComponent),
        cv.Optional(CONF_DRAIN_INTERVAL, default="7ms"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_RING_BUFFER_SIZE, default=2048): cv.int_range(min=256, max=2048),
        cv.Optional(CONF_MAX_PAYLOAD_LENGTH, default=200): cv.int_range(min=1, max=255),
        cv.Optional(CONF_LOG_VALID_FRAMES, default=True): cv.boolean,
        cv.Optional(CONF_LOG_FRAME_ERRORS, default=True): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_drain_interval_ms(config[CONF_DRAIN_INTERVAL].total_milliseconds))
    cg.add(var.set_ring_buffer_size(config[CONF_RING_BUFFER_SIZE]))
    cg.add(var.set_max_payload_length(config[CONF_MAX_PAYLOAD_LENGTH]))
    cg.add(var.set_log_valid_frames(config[CONF_LOG_VALID_FRAMES]))
    cg.add(var.set_log_frame_errors(config[CONF_LOG_FRAME_ERRORS]))
