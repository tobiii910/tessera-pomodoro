#include "pomodoro.h"
#include <algorithm>

namespace esphome::pomodoro {

using tessera::Font;
using tessera::Tone;
namespace ui = tessera::ui;

static constexpr uint32_t ICON_RINGING = 0xF009E;

// A tile: a face in the tile's room. The pomodoro lives in the plugin, so every tile and the card show the same one,
// and a tile made again when its page comes back (a board without PSRAM) shows it at once.
class PomodoroTile : public tessera::Tile {
 public:
  explicit PomodoroTile(Pomodoro *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    tile_ = c.tile;
    face_.create(c.parent, c.width, c.height, plugin_, false);
  }
  void on_tick(uint32_t epoch) override { face_.paint(plugin_->view()); }
  void on_theme() override { face_.on_theme(); }

  void on_tap() override {
    const Key key = face_.hit(tessera::tap_point());
    if (key == Key::OPEN || key == Key::NONE) {
      tessera::open_card(plugin_->plugin_id(), "pomodoro", "", tile_);
      return;
    }
    plugin_->press(key);
    face_.paint(plugin_->view());
  }

 private:
  Pomodoro *plugin_;
  int tile_ = -1;
  Face face_;
};

// The card: the same face with every key, over the whole glass.
class PomodoroCard : public tessera::Card {
 public:
  explicit PomodoroCard(Pomodoro *plugin) : plugin_(plugin) {}

  void open(const tessera::CardContext &c) override {
    face_.create(c.parent, c.width, c.height, plugin_, true);
    face_.paint(plugin_->view());
  }
  void on_tick(uint32_t epoch) override { face_.paint(plugin_->view()); }
  void on_theme() override { face_.on_theme(); }

  void on_tap() override {
    const Key key = face_.hit(tessera::tap_point());
    if (key == Key::OPEN || key == Key::NONE) return;
    plugin_->press(key);
    face_.paint(plugin_->view());
  }

 private:
  Pomodoro *plugin_;
  Face face_;
};

// The popup at the end of a phase: a disc in the phase's colour beside "Focus done", what comes next, and one key. It
// waits for Start (the next phase starts), or with Autostart says the next phase runs, and its key closes it.
class PhasePopup : public tessera::Popup {
 public:
  PhasePopup(Pomodoro *plugin, bool waits, Phase over) : plugin_(plugin), waits_(waits), over_(over) {}

  void open(const tessera::PopupContext &c) override {
    const int g = ui::px(8);
    const int disc = c.key > 0 ? c.key : ui::mm(9);
    const Tone tone = over_ == Phase::FOCUS ? Tone::BUSY : over_ == Phase::SHORT_BREAK ? Tone::GOOD : Tone::ACCENT;
    disc_ = ui::block(c.parent, theme::ACCENT);
    lv_obj_set_size(disc_, disc, disc);
    lv_obj_set_pos(disc_, 0, 0);
    lv_obj_set_style_radius(disc_, LV_RADIUS_CIRCLE, 0);
    ui::set_fill(disc_, tone, theme::ACCENT);
    tone_ = tone;
    const Font icon_font = ui::line_height(Font::ICON) <= disc * 80 / 100 ? Font::ICON : Font::ICON_SMALL;
    icon_ = ui::label(disc_, icon_font, theme::ON_ACCENT);
    lv_obj_set_width(icon_, disc);
    lv_obj_set_style_text_align(icon_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_y(icon_, (disc - ui::line_height(icon_font)) / 2);
    ui::set_text(icon_, ui::icon(ICON_RINGING));

    // "Focus done" beside the disc, short of the close key, in the largest font it fits.
    const char *done = over_ == Phase::FOCUS         ? plugin_->text("focus_done")
                       : over_ == Phase::SHORT_BREAK ? plugin_->text("short_break_done")
                                                     : plugin_->text("long_break_done");
    const int title_x = disc + g, title_w = std::max(1, c.width - title_x - c.corner);
    Font face = Font::BODY;
    for (Font f : {Font::HEADLINE, Font::TITLE, Font::BODY_LARGE})
      if (ui::text_width(done, f) <= title_w) {
        face = f;
        break;
      }
    title_ = ui::label(c.parent, face, theme::INK);
    lv_obj_set_width(title_, title_w);
    lv_obj_set_pos(title_, title_x, (disc - ui::line_height(face)) / 2);
    ui::set_text(title_, done);

    // What comes next: "Next: Short break, 5 min", or with Autostart "Short break runs, 5 min".
    // With Autostart the pomodoro is in the next phase already.
    const Phase next = waits_ ? plugin_->following() : plugin_->state().phase;
    std::string line =
        tessera::fill(plugin_->text(waits_ ? "next_line" : "started_line"), "phase", plugin_->phase_name(next));
    line = tessera::format(line.c_str(), static_cast<long>(plugin_->minutes_of(next)));
    const int line_y = disc + 2 * g;
    line_ = ui::text(c.parent, Font::BODY_LARGE, theme::INK_SOFT);
    lv_obj_set_width(line_, c.width);
    lv_obj_set_pos(line_, 0, line_y);
    ui::set_text(line_, line);
    const int line_h = 2 * ui::line_height(Font::BODY_LARGE);
    lv_obj_set_height(line_, line_h);

    // The key: Start (the next phase runs) or OK (it runs already).
    key_y_ = line_y + line_h + 2 * g;
    key_h_ = ui::mm(10);
    key_ = ui::block(c.parent, theme::ACCENT);
    lv_obj_set_size(key_, c.width, key_h_);
    lv_obj_set_pos(key_, 0, key_y_);
    lv_obj_set_style_radius(key_, LV_RADIUS_CIRCLE, 0);
    key_label_ = ui::label(key_, Font::BODY_LARGE, theme::ON_ACCENT);
    lv_obj_set_width(key_label_, c.width);
    lv_obj_set_style_text_align(key_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_y(key_label_, (key_h_ - ui::line_height(Font::BODY_LARGE)) / 2);
    ui::set_text(key_label_, plugin_->text(waits_ ? "start" : "ok"));
  }

  void on_theme() override {
    ui::set_fill(disc_, tone_, theme::ACCENT);
    ui::set_color(icon_, theme::ON_ACCENT);
    ui::set_color(title_, theme::INK);
    ui::set_color(line_, theme::INK_SOFT);
    lv_obj_set_style_bg_color(key_, ui::color(theme::ACCENT), 0);
    ui::set_color(key_label_, theme::ON_ACCENT);
  }

  void on_tap() override {
    const tessera::Point p = tessera::tap_point();
    if (p.y < key_y_ - ui::mm(1) || p.y > key_y_ + key_h_ + ui::mm(1)) return;
    plugin_->popup_gone();
    if (waits_) plugin_->press(Key::PRIMARY);
    tessera::close_popup(plugin_);
  }

  void on_dismiss() override { plugin_->popup_gone(); }

 private:
  Pomodoro *plugin_;
  bool waits_;
  Phase over_;
  Tone tone_ = Tone::BUSY;
  lv_obj_t *disc_{}, *icon_{}, *title_{}, *line_{}, *key_{}, *key_label_{};
  int key_y_ = 0, key_h_ = 0;
};

tessera::Tile *make_tile(Pomodoro *plugin) { return new PomodoroTile(plugin); }
tessera::Card *make_card(Pomodoro *plugin) { return new PomodoroCard(plugin); }
tessera::Popup *make_popup(Pomodoro *plugin, bool waits, Phase over) { return new PhasePopup(plugin, waits, over); }

}  // namespace esphome::pomodoro
