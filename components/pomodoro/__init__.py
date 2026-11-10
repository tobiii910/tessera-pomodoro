"""Pomodoro: a focus timer with short and long breaks on the glass.

The pomodoro runs in the component (pomodoro.cpp), one for the whole screen, and is kept in the screen's flash so a
restart picks it up where it was. Its lengths, how many rounds come before the long break and whether the next phase
starts by itself are ESPHome entities of plugin.yaml; it tells Home Assistant its phase, its round and when a phase is
over. face.cpp lays out and paints the ring, the time, the rounds and the keys for every size of tile and for the
card; it comes from tessera-timers by Heronimonimo (MIT). register_plugin() takes the id, version, tile memory and the
screen's texts from the manifest and translations/ beside this folder.

On a screen with a speaker the part parts/sound.yaml hands it over as `speaker`, with its switch `sound_on`: the end of
a phase then beeps with sounds/done.wav, built in by smart_display.sound() and played by the core's sound player.
"""
from pathlib import Path

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, number, sensor, smart_display, speaker, switch, text_sensor
from esphome.const import CONF_ID

DEPENDENCIES = ["smart_display", "binary_sensor", "number", "sensor", "switch", "text_sensor"]

CONF_FOCUS = "focus"
CONF_SHORT = "short_break"
CONF_LONG = "long_break"
CONF_ROUNDS = "rounds"
CONF_AUTOSTART = "autostart"
CONF_OPENS = "opens_when_done"
CONF_PHASE = "phase"
CONF_ROUND = "round"
CONF_ENDED = "ended"
CONF_SPEAKER = "speaker"
CONF_SOUND_ON = "sound_on"
DONE = Path(__file__).parent / "sounds" / "done.wav"

pomodoro_ns = cg.esphome_ns.namespace("pomodoro")
Pomodoro = pomodoro_ns.class_("Pomodoro", cg.Component)


def check_sound(config):
    if CONF_SPEAKER in config:
        smart_display.read_sound(DONE)
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Pomodoro),
            cv.Required(CONF_FOCUS): cv.use_id(number.Number),
            cv.Required(CONF_SHORT): cv.use_id(number.Number),
            cv.Required(CONF_LONG): cv.use_id(number.Number),
            cv.Required(CONF_ROUNDS): cv.use_id(number.Number),
            cv.Required(CONF_AUTOSTART): cv.use_id(switch.Switch),
            cv.Required(CONF_OPENS): cv.use_id(switch.Switch),
            cv.Required(CONF_PHASE): cv.use_id(text_sensor.TextSensor),
            cv.Required(CONF_ROUND): cv.use_id(sensor.Sensor),
            cv.Required(CONF_ENDED): cv.use_id(binary_sensor.BinarySensor),
            cv.Optional(CONF_SPEAKER): cv.use_id(speaker.Speaker),
            cv.Optional(CONF_SOUND_ON): cv.use_id(switch.Switch),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    check_sound,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await smart_display.register_plugin(var, __file__)
    cg.add(var.set_focus(await cg.get_variable(config[CONF_FOCUS])))
    cg.add(var.set_short(await cg.get_variable(config[CONF_SHORT])))
    cg.add(var.set_long(await cg.get_variable(config[CONF_LONG])))
    cg.add(var.set_rounds(await cg.get_variable(config[CONF_ROUNDS])))
    cg.add(var.set_autostart(await cg.get_variable(config[CONF_AUTOSTART])))
    cg.add(var.set_opens(await cg.get_variable(config[CONF_OPENS])))
    cg.add(var.set_phase_sensor(await cg.get_variable(config[CONF_PHASE])))
    cg.add(var.set_round_sensor(await cg.get_variable(config[CONF_ROUND])))
    cg.add(var.set_ended(await cg.get_variable(config[CONF_ENDED])))
    if CONF_SPEAKER in config:
        cg.add(var.set_speaker(await cg.get_variable(config[CONF_SPEAKER])))
        cg.add(var.set_alarm(smart_display.sound(config[CONF_ID], "done", DONE)))
        if CONF_SOUND_ON in config:
            cg.add(var.set_sound_on(await cg.get_variable(config[CONF_SOUND_ON])))
