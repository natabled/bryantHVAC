import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, text_sensor
from esphome.const import CONF_ID, CONF_NAME
from esphome.components.bryant_framer import BryantFramer

DEPENDENCIES = ["bryant_framer"]
AUTO_LOAD = ["sensor", "text_sensor"]
ns = cg.esphome_ns.namespace("bryant_parser")
BryantParser = ns.class_("BryantParser", cg.Component)

MATCH = cv.Schema({
    cv.Required("dst"): cv.int_range(min=0, max=255),
    cv.Required("src"): cv.int_range(min=0, max=255),
    cv.Required("function"): cv.int_range(min=0, max=255),
    cv.Required("register"): cv.int_range(min=0, max=65535),
})
MESSAGE = cv.Schema({
    cv.Required("name"): cv.string,
    cv.Required("match"): MATCH,
    cv.Optional("payload_min", default=3): cv.int_range(min=3, max=255),
})
BIT_RULE = cv.Schema({
    cv.Required("mask"): cv.int_range(min=1, max=0xFFFFFFFF),
    cv.Required("value"): cv.float_,
})
FIELD = cv.Schema({
    cv.Required("name"): cv.string,
    cv.Required("sensor_id"): cv.use_id(sensor.Sensor),
    cv.Required("message"): cv.string,
    cv.Required("offset"): cv.int_range(min=0, max=254),
    cv.Optional("width", default=8): cv.one_of(8, 16, 24, 32, int=True),
    cv.Optional("signed", default=False): cv.boolean,
    cv.Optional("endian", default="big"): cv.one_of("big", "little"),
    cv.Optional("multiplier", default=1.0): cv.float_,
    cv.Optional("add", default=0.0): cv.float_,
    cv.Optional("mask", default=0xFFFFFFFF): cv.int_range(min=0, max=0xFFFFFFFF),
    cv.Optional("shift", default=0): cv.int_range(min=0, max=31),
    cv.Optional("bit_rules", default=[]): cv.ensure_list(BIT_RULE),
    cv.Optional("default", default=0.0): cv.float_,
})

def validate_config(config):
    names = [m["name"] for m in config["messages"]]
    if len(names) != len(set(names)):
        raise cv.Invalid("Message names must be unique")
    for field in config["fields"]:
        if field["message"] not in names:
            raise cv.Invalid(f"Unknown message {field['message']} for {field['name']}")
        if field["offset"] + field["width"] // 8 > 255:
            raise cv.Invalid(f"Field {field['name']} exceeds max payload length")
    return config

CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_id(BryantParser),
    cv.Required("framer_id"): cv.use_id(BryantFramer),
    cv.Required("messages"): cv.ensure_list(MESSAGE),
    cv.Required("fields"): cv.ensure_list(FIELD),
    cv.Optional("datetime_text_id"): cv.use_id(text_sensor.TextSensor),
    cv.Optional("log_matches", default=True): cv.boolean,
    cv.Optional("log_unmatched", default=False): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA), validate_config)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    framer = await cg.get_variable(config["framer_id"])
    cg.add(framer.add_listener(var))
    cg.add(var.set_log_matches(config["log_matches"]))
    cg.add(var.set_log_unmatched(config["log_unmatched"]))
    if "datetime_text_id" in config:
        t = await cg.get_variable(config["datetime_text_id"])
        cg.add(var.set_datetime_sensor(t))
    for msg in config["messages"]:
        m = msg["match"]
        cg.add(var.add_message(msg["name"], m["dst"], m["src"], m["function"], m["register"], msg["payload_min"]))
    for field in config["fields"]:
        s = await cg.get_variable(field["sensor_id"])
        cg.add(var.add_field(field["name"], field["message"], s, field["offset"], field["width"],
                             field["signed"], field["endian"] == "little", field["multiplier"],
                             field["add"], field["mask"], field["shift"], field["default"]))
        for rule in field["bit_rules"]:
            cg.add(var.add_bit_rule(field["name"], rule["mask"], rule["value"]))
