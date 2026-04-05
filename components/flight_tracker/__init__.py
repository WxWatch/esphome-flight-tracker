import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.http_request import CONF_HTTP_REQUEST_ID, HttpRequestComponent
from esphome.components.display import Display
from esphome.components.font import Font
from esphome.components.time import RealTimeClock
from esphome.components import color
from esphome.const import CONF_ID, CONF_DISPLAY_ID, CONF_TIME_ID, CONF_SHOW_UNITS, __version__ as ESPHOME_VERSION, Framework
from esphome.types import ConfigType

_MINIMUM_ESPHOME_VERSION = "2025.11.0"

DEPENDENCIES = ["network", "display", "font", "time"]
AUTO_LOAD = ["json", "watchdog"]

flight_tracker_ns = cg.esphome_ns.namespace("flight_tracker")
FlightTracker = flight_tracker_ns.class_("FlightTracker", cg.Component)

UnitDisplay = flight_tracker_ns.enum("UnitDisplay")
UNIT_DISPLAY_VALUES = {
    "long": UnitDisplay.UNIT_DISPLAY_LONG,
    "short": UnitDisplay.UNIT_DISPLAY_SHORT,
    "none": UnitDisplay.UNIT_DISPLAY_NONE,
}

CONF_ROUTES = "routes"
CONF_STOPS = "stops"
CONF_BASE_URL = "base_url"
CONF_FONT_ID = "font_id"
CONF_LIMIT = "limit"
CONF_ABBREVIATIONS = "abbreviations"
CONF_STYLES = "styles"
CONF_FEED_CODE = "feed_code"
CONF_DEFAULT_ROUTE_COLOR = "default_route_color"
CONF_REALTIME_COLOR = "realtime_color"
CONF_TIME_DISPLAY = "time_display"
CONF_LIST_MODE = "list_mode"
CONF_SCROLL_HEADSIGNS = "scroll_headsigns"


def validate_ws_url(value):
    url = cv.url(value)
    if not value.startswith("ws://") and not value.startswith("wss://"):
        raise cv.Invalid("URL must start with 'ws://' or 'wss://")

    return url


def validate_esphome_version(obj):
    if cv.Version.parse(ESPHOME_VERSION) < cv.Version.parse(_MINIMUM_ESPHOME_VERSION):
        raise cv.Invalid(
            "The flight_tracker component requires ESPHome version " +
            f"{_MINIMUM_ESPHOME_VERSION} or later."
        )
    return obj


COLOR_SCHEMA = cv.All(
    cv.requires_component("color"),
    cv.use_id(color.ColorStruct)
)


CONFIG_SCHEMA = cv.All(
    validate_esphome_version,
    cv.only_with_framework(frameworks=Framework.ARDUINO),
    cv.Schema(


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
            var.set_realtime_color(
                await cg.get_variable(config[CONF_REALTIME_COLOR])
            )
        )

    await cg.register_component(var, config)

    cg.add_library("WiFi", None)
