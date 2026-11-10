#pragma once
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/smart_display/plugin_api.h"
#include "esphome/components/smart_display/plugin_sound.h"   // empty on a screen without a speaker
#include "face.h"

namespace esphome::pomodoro {

// The pomodoro of the screen, one for every tile and the card. Plain data, kept in flash as it is, against the clock of
// the day so a restart picks it up where it was.
struct State {
  Phase phase = Phase::FOCUS;
  Run run = Run::IDLE;
  uint8_t round = 1;         // the focus round of this cycle, 1 to the rounds before the long break
  uint8_t finished = 0;      // the focus rounds of this cycle that are done (the filled dots)
  uint32_t total_s = 0;      // how long this run of the phase is (the whole ring)
  int64_t mark_ms = 0;       // running: the phase's end
  int64_t held_ms = 0;       // paused: what the phase has left
};

class Pomodoro : public Component, public tessera::Plugin {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void on_interval(uint32_t now_ms) override;
  bool settings(tessera::SettingsPage &page) override;

  void set_focus(number::Number *n) { focus_ = n; }
  void set_short(number::Number *n) { short_ = n; }
  void set_long(number::Number *n) { long_ = n; }
  void set_rounds(number::Number *n) { rounds_ = n; }
  void set_autostart(switch_::Switch *s) { autostart_ = s; }
  void set_opens(switch_::Switch *s) { opens_ = s; }
  void set_phase_sensor(text_sensor::TextSensor *s) { phase_sensor_ = s; }
  void set_round_sensor(sensor::Sensor *s) { round_sensor_ = s; }
  void set_ended(binary_sensor::BinarySensor *s) { ended_ = s; }
#ifdef USE_SPEAKER
  // The part parts/sound.yaml, on a screen with a speaker: the end of a phase beeps.
  void set_speaker(speaker::Speaker *s) { sounds_.set_speaker(s); }
  void set_alarm(tessera::Sound s) { alarm_ = s; }
  void set_sound_on(switch_::Switch *s) { sound_on_ = s; }
  void loop() override { sounds_.loop(); }
  void before_update() override { sounds_.stop(); }
#endif

  // A key tapped on a face, on a tile, the card or the popup.
  void press(Key key);
  // What a face shows now.
  View view();
  const State &state() const { return state_; }
  // The words of a phase, and of the one that follows the phase that is over.
  const char *phase_name(Phase phase);
  Phase following() const;
  uint32_t minutes_of(Phase phase) const;
  // The popup forgets itself when it goes.
  void popup_gone() { popup_waits_ = false; }

 protected:
  int64_t now_ms();
  uint8_t rounds() const;
  void begin(int64_t now);
  void finish();
  void step();
  void changed();
  void ended(int64_t now);
  void show_popup(bool waits, Phase over);
  void publish();
  tessera::BarItem bar_item();

  State state_;
  ESPPreferenceObject saved_;
  bool dirty_ = false, popup_waits_ = false, popup_due_ = false;
  uint32_t saved_at_ = 0, pulse_until_ = 0;
  int64_t base_ms_ = 0;
  uint32_t base_millis_ = 0;
  int published_phase_ = -1, published_round_ = -1, published_ended_ = -1;
  number::Number *focus_{}, *short_{}, *long_{}, *rounds_{};
  switch_::Switch *autostart_{}, *opens_{};
  text_sensor::TextSensor *phase_sensor_{};
  sensor::Sensor *round_sensor_{};
  binary_sensor::BinarySensor *ended_{};
#ifdef USE_SPEAKER
  tessera::Sound alarm_;
  switch_::Switch *sound_on_{};
  tessera::SoundPlayer sounds_;
#endif
};

// "25:00" or "1:12:30".
std::string clock_of(uint32_t seconds);

}  // namespace esphome::pomodoro
