#include "pomodoro.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace esphome::pomodoro {

using tessera::Tone;

// The tile, the card and the popup (tiles.cpp).
tessera::Tile *make_tile(Pomodoro *plugin);
tessera::Card *make_card(Pomodoro *plugin);
tessera::Popup *make_popup(Pomodoro *plugin, bool waits, Phase over);

static constexpr uint32_t SAVED_VERSION = 1;
static constexpr uint32_t PULSE_MS = 5000;   // with Autostart, how long "Phase ended" is on at the end of a phase
static constexpr uint32_t ICON_PLAY = 0xF040A, ICON_PAUSE = 0xF03E4, ICON_TIMER = 0xF051B;

// What Home Assistant's text sensor Pomodoro Phase says: a word for automations, never translated.
static const char *const PHASE_WORDS[] = {"focus", "short_break", "long_break", "idle", "paused"};

struct Saved {
  uint32_t version;
  State state;
};

std::string clock_of(uint32_t seconds) {
  char text[16];
  const unsigned h = seconds / 3600, m = seconds / 60 % 60, s = seconds % 60;
  if (h) snprintf(text, sizeof text, "%u:%02u:%02u", h, m, s);
  else snprintf(text, sizeof text, "%u:%02u", m, s);
  return text;
}

// Whole seconds, rounded up: a phase shows 0:01 until its very end.
static uint32_t seconds_up(int64_t ms) { return ms <= 0 ? 0 : static_cast<uint32_t>((ms + 999) / 1000); }

// Focus in amber (Home Assistant's "works on something"), the short break in green, the long break in blue.
static Tone tone_of(Phase phase) {
  switch (phase) {
    case Phase::SHORT_BREAK: return Tone::GOOD;
    case Phase::LONG_BREAK: return Tone::ACCENT;
    default: return Tone::BUSY;
  }
}

static int value_of(number::Number *n, int fallback, int low, int high) {
  if (n == nullptr || std::isnan(n->state)) return fallback;
  return std::max(low, std::min(high, static_cast<int>(std::lround(n->state))));
}

static void set_number(number::Number *n, int value) {
  if (n == nullptr) return;
  auto call = n->make_call();
  call.set_value(value);
  call.perform();
}

void Pomodoro::setup() {
  add_tile("pomodoro", [this]() { return make_tile(this); });
  add_card("pomodoro", [this]() { return make_card(this); }, true);
  add_bar_item("running", [this]() { return bar_item(); });

  // What ran before a restart runs on: the state is kept against the clock of the day, not the chip's.
  saved_ = global_preferences->make_preference<Saved>(fnv1_hash("pomodoro.state"));
  Saved saved{};
  if (saved_.load(&saved) && saved.version == SAVED_VERSION) state_ = saved.state;
}

int64_t Pomodoro::now_ms() {
  const uint32_t epoch = tessera::epoch();
  if (!epoch) return 0;
  // The clock has whole seconds; millis() fills in between, and is set right when the two part by more than 1.5 s.
  const uint32_t ms = millis();
  if (!base_ms_) {
    base_ms_ = static_cast<int64_t>(epoch) * 1000;
    base_millis_ = ms;
  }
  int64_t now = base_ms_ + static_cast<uint32_t>(ms - base_millis_);
  const int64_t drift = now - static_cast<int64_t>(epoch) * 1000;
  if (drift < -1500 || drift > 1500) {
    base_ms_ = static_cast<int64_t>(epoch) * 1000;
    base_millis_ = ms;
    now = base_ms_;
  }
  return now;
}

uint8_t Pomodoro::rounds() const { return static_cast<uint8_t>(value_of(rounds_, 4, 2, 8)); }

uint32_t Pomodoro::minutes_of(Phase phase) const {
  switch (phase) {
    case Phase::SHORT_BREAK: return value_of(short_, 5, 1, 30);
    case Phase::LONG_BREAK: return value_of(long_, 15, 1, 60);
    default: return value_of(focus_, 25, 1, 90);
  }
}

const char *Pomodoro::phase_name(Phase phase) {
  switch (phase) {
    case Phase::SHORT_BREAK: return text("short_break");
    case Phase::LONG_BREAK: return text("long_break");
    default: return text("focus");
  }
}

// After focus a break, the long one when the rounds of the cycle are done; after a break focus again.
Phase Pomodoro::following() const {
  if (state_.phase != Phase::FOCUS) return Phase::FOCUS;
  return state_.finished >= rounds() ? Phase::LONG_BREAK : Phase::SHORT_BREAK;
}

void Pomodoro::changed() {
  dirty_ = true;
  tessera::refresh();
}

void Pomodoro::begin(int64_t now) {
  state_.total_s = minutes_of(state_.phase) * 60;
  state_.mark_ms = now + static_cast<int64_t>(state_.total_s) * 1000;
  state_.run = Run::RUNNING;
}

// The phase is over: a focus round fills its dot.
void Pomodoro::finish() {
  if (state_.phase == Phase::FOCUS) state_.finished = std::min<uint8_t>(state_.round, rounds());
}

// On to the following phase, ready to start.
void Pomodoro::step() {
  const Phase next = following();
  if (state_.phase == Phase::SHORT_BREAK) {
    state_.round = std::min<uint8_t>(state_.round + 1, rounds());
  } else if (state_.phase == Phase::LONG_BREAK) {
    state_.round = 1;
    state_.finished = 0;
  }
  state_.phase = next;
  state_.run = Run::IDLE;
  state_.total_s = 0;
}

void Pomodoro::press(Key key) {
  const int64_t now = now_ms();
  switch (key) {
    case Key::PRIMARY:
      if (!now) return;   // the clock is not set yet: nothing can run
      switch (state_.run) {
        case Run::IDLE: begin(now); break;
        case Run::RUNNING:
          state_.held_ms = std::max<int64_t>(0, state_.mark_ms - now);
          state_.run = Run::PAUSED;
          break;
        case Run::PAUSED:
          state_.mark_ms = now + state_.held_ms;
          state_.run = Run::RUNNING;
          break;
        case Run::DONE:
          step();
          begin(now);
          break;
      }
      break;
    case Key::SKIP:
      // The phase ends now, as if its time were up (a focus round counts), and the next one waits for Start.
      if (state_.run != Run::DONE) finish();
      step();
      break;
    case Key::RESET:
      state_ = State{};
      break;
    default:
      return;
  }
  if (popup_waits_ && state_.run != Run::DONE) {
    popup_waits_ = false;
    tessera::close_popup(this);
  }
  changed();
}

void Pomodoro::ended(int64_t now) {
  const Phase over = state_.phase;
  finish();
  state_.run = Run::DONE;
  const bool goes_on = autostart_ != nullptr && autostart_->state;
  if (goes_on) {
    step();
    begin(now);
    pulse_until_ = millis() + PULSE_MS;
  }
  changed();
  if (opens_ == nullptr || opens_->state) show_popup(!goes_on, over);
#ifdef USE_SPEAKER
  if (sound_on_ == nullptr || sound_on_->state) sounds_.play(alarm_);
#endif
}

void Pomodoro::show_popup(bool waits, Phase over) {
  popup_waits_ = waits;
  tessera::show_popup(this, [this, waits, over]() { return make_popup(this, waits, over); });
}

View Pomodoro::view() {
  View v;
  const int64_t now = now_ms();
  v.rounds = rounds();
  v.finished = std::min(state_.finished, v.rounds);
  v.tone = tone_of(state_.phase);
  uint32_t left = 0, total = 0;
  switch (state_.run) {
    case Run::IDLE:
      left = total = minutes_of(state_.phase) * 60;
      v.sub = phase_name(state_.phase);
      v.primary = text("start");
      v.primary_icon = ICON_PLAY;
      break;
    case Run::RUNNING:
      total = std::max<uint32_t>(state_.total_s, 1);
      left = now ? seconds_up(state_.mark_ms - now) : total;
      v.sub = now ? phase_name(state_.phase) : text("no_clock");
      v.primary = text("pause");
      v.primary_icon = ICON_PAUSE;
      break;
    case Run::PAUSED:
      total = std::max<uint32_t>(state_.total_s, 1);
      left = seconds_up(state_.held_ms);
      v.sub = text("paused");
      v.primary = text("resume");
      v.primary_icon = ICON_PLAY;
      break;
    case Run::DONE:
      v.done = true;
      v.sub = tessera::fill(text("next"), "phase", phase_name(following()));
      // A line too long for a small ring: the next phase's name alone.
      if (tessera::ui::text_width(v.sub, tessera::Font::BODY) > tessera::ui::mm(16)) v.sub = phase_name(following());
      v.primary = text("start");
      v.primary_icon = ICON_PLAY;
      break;
  }
  v.time = state_.run == Run::DONE ? std::string(text("done")) : clock_of(left);
  v.progress = state_.run == Run::DONE || !total ? 1000 : static_cast<int>(1000LL * std::min(left, total) / total);
  return v;
}

void Pomodoro::publish() {
  const int phase = state_.run == Run::RUNNING ? static_cast<int>(state_.phase) : state_.run == Run::PAUSED ? 4 : 3;
  if (phase_sensor_ != nullptr && phase != published_phase_) {
    published_phase_ = phase;
    phase_sensor_->publish_state(PHASE_WORDS[phase]);
  }
  if (round_sensor_ != nullptr && state_.round != published_round_) {
    published_round_ = state_.round;
    round_sensor_->publish_state(state_.round);
  }
  const int ended = state_.run == Run::DONE || static_cast<int32_t>(pulse_until_ - millis()) > 0;
  if (ended_ != nullptr && ended != published_ended_) {
    published_ended_ = ended;
    ended_->publish_state(ended);
  }
}

void Pomodoro::on_interval(uint32_t now_ms_chip) {
  const int64_t now = now_ms();
  if (now && state_.run == Run::RUNNING && now >= state_.mark_ms) ended(now);
#ifdef USE_SPEAKER
  if (sound_on_ != nullptr && !sound_on_->state && sounds_.playing()) sounds_.stop();
#endif
  publish();
  // Kept at most once a second, so a run of taps writes the flash once.
  if (dirty_ && now_ms_chip - saved_at_ >= 1000) {
    Saved saved{};
    saved.version = SAVED_VERSION;
    saved.state = state_;
    saved_.save(&saved);
    dirty_ = false;
    saved_at_ = now_ms_chip;
  }
}

// The phase and its minutes while it runs ("Focus 12 min"): the bar asks every five seconds, so whole minutes, rounded
// up.
tessera::BarItem Pomodoro::bar_item() {
  tessera::BarItem item;
  const int64_t now = now_ms();
  if (!now || state_.run != Run::RUNNING) return item;
  const uint32_t seconds = seconds_up(state_.mark_ms - now);
  const std::string minutes = seconds < 60 ? std::string(text("under_minute"))
                                           : tessera::format(text("minutes"), static_cast<long>((seconds + 59) / 60));
  item.shown = true;
  item.icon = ICON_TIMER;
  item.tone = tone_of(state_.phase);
  item.text = tessera::fill(tessera::fill(text("bar"), "phase", phase_name(state_.phase)).c_str(), "time", minutes);
  return item;
}

bool Pomodoro::settings(tessera::SettingsPage &page) {
  page.icon = "\U000F051B";
  const char *unit = text("unit_min");
  page.number(text("focus_length"), 1, 90, 1, unit, [this] { return value_of(focus_, 25, 1, 90); },
              [this](int v) { set_number(focus_, v); });
  page.number(text("short_length"), 1, 30, 1, unit, [this] { return value_of(short_, 5, 1, 30); },
              [this](int v) { set_number(short_, v); });
  page.number(text("long_length"), 1, 60, 1, unit, [this] { return value_of(long_, 15, 1, 60); },
              [this](int v) { set_number(long_, v); });
  page.number(text("rounds"), 2, 8, 1, "", [this] { return static_cast<int>(rounds()); },
              [this](int v) { set_number(rounds_, v); });
  page.toggle(text("autostart"), [this] { return autostart_ != nullptr && autostart_->state; },
              [this](bool on) {
                if (autostart_ != nullptr) on ? autostart_->turn_on() : autostart_->turn_off();
              });
  page.toggle(text("opens_when_done"), [this] { return opens_ == nullptr || opens_->state; },
              [this](bool on) {
                if (opens_ != nullptr) on ? opens_->turn_on() : opens_->turn_off();
              });
#ifdef USE_SPEAKER
  if (sound_on_ != nullptr)
    page.toggle(text("sound"), [this] { return sound_on_->state; },
                [this](bool on) { on ? sound_on_->turn_on() : sound_on_->turn_off(); });
#endif
  return true;
}

}  // namespace esphome::pomodoro
