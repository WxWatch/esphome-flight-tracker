import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.display import Display
from esphome.components.font import Font
from esphome.components.time import RealTimeClock
from esphome.components import color
from esphome.const import (
    CONF_ID,
    CONF_DISPLAY_ID,
    CONF_TIME_ID,
    __version__ as ESPHOME_VERSION,
    Framework,
)
from esphome.types import ConfigType

_MINIMUM_ESPHOME_VERSION = "2025.11.0"

DEPENDENCIES = ["network", "display", "font", "time"]
AUTO_LOAD = ["json", "watchdog"]

flight_tracker_ns = cg.esphome_ns.namespace("flight_tracker")
FlightTracker = flight_tracker_ns.class_("FlightTracker", cg.Component)

CONF_HOST = "host"
CONF_PORT = "port"
CONF_FONT_ID = "font_id"
CONF_LIMIT = "limit"
CONF_REALTIME_COLOR = "realtime_color"
CONF_SCROLL_HEADSIGNS = "scroll_headsigns"


def validate_esphome_version(obj):
    if cv.Version.parse(ESPHOME_VERSION) < cv.Version.parse(_MINIMUM_ESPHOME_VERSION):
        raise cv.Invalid(
            "The flight_tracker component requires ESPHome version "
            + f"{_MINIMUM_ESPHOME_VERSION} or later."
        )
    return obj


COLOR_SCHEMA = cv.All(cv.requires_component("color"), cv.use_id(color.ColorStruct))


CONFIG_SCHEMA = cv.All(
    validate_esphome_version,
    cv.only_with_framework(frameworks=Framework.ARDUINO),
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(FlightTracker),
            cv.GenerateID(CONF_DISPLAY_ID): cv.use_id(Display),
            cv.GenerateID(CONF_FONT_ID): cv.use_id(Font),
            cv.GenerateID(CONF_TIME_ID): cv.use_id(RealTimeClock),
            cv.Optional(CONF_HOST): cv.string,
            cv.Optional(CONF_PORT, default=30003): cv.port,
            cv.Optional(CONF_LIMIT, default=3): cv.positive_int,
            cv.Optional(CONF_SCROLL_HEADSIGNS, default=False): cv.boolean,
            cv.Optional(CONF_REALTIME_COLOR): COLOR_SCHEMA,
        }
    ).extend(cv.COMPONENT_SCHEMA),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    drawing_display = await cg.get_variable(config[CONF_DISPLAY_ID])
    cg.add(var.set_display(drawing_display))

    font = await cg.get_variable(config[CONF_FONT_ID])
    cg.add(var.set_font(font))

    time = await cg.get_variable(config[CONF_TIME_ID])
    cg.add(var.set_rtc(time))

    if CONF_HOST in config:
        cg.add(var.set_host(config[CONF_HOST]))

    cg.add(var.set_port(config[CONF_PORT]))

    cg.add(var.set_scroll_headsigns(config[CONF_SCROLL_HEADSIGNS]))

    cg.add(var.set_limit(config[CONF_LIMIT]))

    if CONF_REALTIME_COLOR in config:
        cg.add(
            var.set_realtime_color(await cg.get_variable(config[CONF_REALTIME_COLOR]))
        )

    await cg.register_component(var, config)

    cg.add_library("WiFi", None)
