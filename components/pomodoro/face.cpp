#include "face.h"
#include <algorithm>

namespace esphome::pomodoro {

using tessera::Font;
using tessera::Tone;
namespace ui = tessera::ui;

static constexpr uint32_t ICON_SKIP = 0xF04AD, ICON_RESET = 0xF0709;

static int clamp(int v, int lo, int hi) { return std::max(lo, std::min(hi, v)); }

// The largest of the screen's text fonts that `text` fits in, across and high.
static Font fitting(const std::string &text, int width, int height) {
  for (Font f : {Font::VALUE, Font::HEADLINE, Font::TITLE, Font::BODY_LARGE})
    if (ui::text_width(text, f) <= width && ui::line_height(f) <= height) return f;
  return Font::BODY;
}

static void show(lv_obj_t *obj, bool shown) {
  if (!obj || lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN) != shown) return;
  if (shown) lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t *centred_label(lv_obj_t *parent, Font font, theme::Role role) {
  lv_obj_t *label = ui::label(parent, font, role);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  return label;
}

void Face::create(lv_obj_t *parent, int width, int height, const tessera::Plugin *plugin, bool card) {
  plugin_ = plugin;
  parent_ = parent;
  card_ = card;
  const int W = width, H = height;
  // The gap between keys, and the smallest key: the 7 mm a finger needs (Tessera's touch_min).
  const int g = ui::px(8), km = ui::mm(7);

  // The shape: the richest one whose keys all keep 7 mm in this room, with a ring of at least 12 to 14 mm.
  const int column_min = 2 * ui::mm(11) + g;
  if (W * 100 >= H * 115 && H >= 2 * km + g && W - column_min - 2 * g >= ui::mm(14))
    shape_ = Shape::SIDE;
  else if (W >= 3 * ui::mm(9) + 2 * g && H >= km + 2 * g + ui::mm(20))
    shape_ = Shape::STACK;
  else if (W >= 2 * km + g && H >= km + 2 * g + ui::mm(12))
    shape_ = Shape::COMPACT;
  else if (W >= H + ui::mm(10) + 2 * km + 2 * g)
    shape_ = Shape::STRIP;
  else
    shape_ = Shape::PLAIN;

  // The ring: where it stands and how large, per shape; the time goes inside it unless a shape puts it beside.
  int cx = W / 2, cy = H / 2, diameter = std::min(W, H);
  bool inside = true;
  int beside_x = 0, beside_w = 0;
  switch (shape_) {
    case Shape::SIDE: {
      const int column = std::max(std::min(W * 45 / 100, 2 * ui::mm(34) + g), column_min);
      const int ring_w = W - column - 2 * g;
      // On a large card the ring stops at 70 mm, and the ring and the keys stand together in the middle.
      diameter = std::min({ring_w, H, ui::mm(70)});
      const int gap = std::min(ring_w - diameter, 4 * g) + 2 * g;
      const int left = (W - (diameter + gap + column)) / 2;
      cx = left + diameter / 2;
      const int x0 = left + diameter + gap, kw = (column - g) / 2;
      const int kh = clamp((H - g) / 2, km, ui::mm(13));
      const int y0 = (H - (2 * kh + g)) / 2;
      add_button(Key::PRIMARY, x0, y0, column, kh, false, true);
      add_button(Key::SKIP, x0, y0 + kh + g, kw, kh, false, false);
      add_button(Key::RESET, x0 + kw + g, y0 + kh + g, kw, kh, false, false);
      break;
    }
    case Shape::STACK:
    case Shape::COMPACT: {
      const int kh = shape_ == Shape::COMPACT ? km : clamp(H * 12 / 100, km, ui::mm(13));
      const int aw = std::min(W, 3 * ui::mm(32) + 2 * g), x0 = (W - aw) / 2;
      const int ya = H - kh;
      if (shape_ == Shape::STACK) {
        const int kw = (aw - 2 * g) / 3;
        add_button(Key::RESET, x0, ya, kw, kh, false, false);
        add_button(Key::SKIP, x0 + kw + g, ya, kw, kh, false, false);
        add_button(Key::PRIMARY, x0 + 2 * (kw + g), ya, aw - 2 * (kw + g), kh, false, true);
      } else {
        const int kw = (aw - g) / 2;
        add_button(Key::SKIP, x0, ya, kw, kh, false, false);
        add_button(Key::PRIMARY, x0 + kw + g, ya, aw - kw - g, kh, false, true);
      }
      const int top = ya - 2 * g;
      diameter = std::min(W, top);
      cy = top / 2;
      break;
    }
    case Shape::STRIP: {
      // The ring gives the time room beside it: at most 45 % of what the keys leave.
      const int kd = clamp(H * 80 / 100, km, ui::mm(11));
      const int x2 = W - kd, x1 = x2 - g - kd, y = (H - kd) / 2;
      diameter = std::min(H, (x1 - 2 * g) * 45 / 100);
      cx = diameter / 2;
      add_button(Key::SKIP, x1, y, kd, kd, true, false);
      add_button(Key::PRIMARY, x2, y, kd, kd, true, true);
      inside = false;
      beside_x = diameter + g;
      beside_w = x1 - g - beside_x;
      break;
    }
    case Shape::PLAIN:
      if (W >= H * 16 / 10) {
        diameter = H;
        cx = H / 2;
        inside = false;
        beside_x = diameter + g;
        beside_w = W - beside_x;
      }
      break;
  }
  diameter = std::max(1, diameter);

  // A ring that runs clockwise from the top: its track in the ground's soft grey, its arc in the phase's tone.
  const int stroke = clamp(diameter * 9 / 100, ui::px(4), ui::mm(3));
  arc_ = lv_arc_create(parent);
  lv_obj_remove_style_all(arc_);
  lv_obj_remove_flag(arc_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(arc_, diameter, diameter);
  lv_obj_set_pos(arc_, cx - diameter / 2, cy - diameter / 2);
  lv_arc_set_rotation(arc_, 270);
  lv_arc_set_bg_angles(arc_, 0, 360);
  lv_arc_set_range(arc_, 0, 1000);
  lv_arc_set_value(arc_, 0);
  for (lv_part_t part : {LV_PART_MAIN, LV_PART_INDICATOR}) {
    lv_obj_set_style_arc_width(arc_, stroke, part);
    lv_obj_set_style_arc_rounded(arc_, true, part);
    lv_obj_set_style_arc_opa(arc_, LV_OPA_COVER, part);
  }

  if (inside) {
    const int inner = diameter - 2 * stroke;
    text_w_ = inner * 82 / 100;
    text_h_ = inner * 72 / 100;
    text_x_ = cx - text_w_ / 2;
    text_y_ = cy - text_h_ / 2;
  } else {
    text_x_ = beside_x;
    text_w_ = std::max(0, beside_w);
    text_y_ = 0;
    text_h_ = H;
  }
  time_ = centred_label(parent, Font::VALUE, theme::INK);
  sub_ = centred_label(parent, Font::BODY, theme::MUTED);
  // The rounds: a row of dots under the time, filled for every focus round of this cycle that is done.
  for (int i = 0; i < DOTS; ++i) {
    dot_[i] = ui::block(parent, card_ ? theme::CARD : theme::TRACK);
    lv_obj_set_style_radius(dot_[i], LV_RADIUS_CIRCLE, 0);
    show(dot_[i], false);
  }
  on_theme();
}

void Face::add_button(Key key, int x, int y, int w, int h, bool round, bool main) {
  if (count_ >= BUTTONS) return;
  Button &b = buttons_[count_++];
  b.key = key;
  b.x = x;
  b.y = y;
  b.w = w;
  b.h = h;
  b.round = round;
  b.main = main;
  b.box = ui::block(parent_, main ? theme::ACCENT : theme::BUTTON);
  lv_obj_set_size(b.box, w, h);
  lv_obj_set_pos(b.box, x, y);
  lv_obj_set_style_radius(b.box, LV_RADIUS_CIRCLE, 0);
  b.label = centred_label(b.box, Font::BODY, main ? theme::ON_ACCENT : theme::SLATE);
  lv_obj_set_width(b.label, w);
}

void Face::paint_button(Button &b, const char *words, uint32_t icon) {
  Font font;
  std::string text = words;
  // Words when they fit the key, else its icon: Skip and Reset are icons always, the key that acts says what it does.
  const bool fits = !text.empty() && ui::text_width(text, Font::BODY) <= b.w * 88 / 100;
  if (icon && (b.round || !fits)) {
    text = ui::icon(icon);
    font = ui::line_height(Font::ICON) <= b.h * 75 / 100 ? Font::ICON : Font::ICON_SMALL;
  } else {
    font = ui::text_width(text, Font::BODY_LARGE) <= b.w * 88 / 100 &&
                   ui::line_height(Font::BODY_LARGE) <= b.h * 80 / 100
               ? Font::BODY_LARGE
               : Font::BODY;
  }
  ui::set_font(b.label, font);
  ui::set_text(b.label, text);
  lv_obj_set_pos(b.label, 0, (b.h - ui::line_height(font)) / 2);
}

void Face::paint(const View &v) {
  for (int i = 0; i < count_; ++i) {
    Button &b = buttons_[i];
    switch (b.key) {
      case Key::PRIMARY: paint_button(b, v.primary, v.primary_icon); break;
      case Key::SKIP: paint_button(b, "", ICON_SKIP); break;
      case Key::RESET: paint_button(b, "", ICON_RESET); break;
      default: break;
    }
  }

  if (v.tone != tone_) {
    tone_ = v.tone;
    lv_obj_set_style_arc_color(arc_, ui::tone_color(tone_, theme::ACCENT), LV_PART_INDICATOR);
    finished_ = 255;   // the dots that are done take the new tone
  }
  if (v.progress != progress_) {
    progress_ = v.progress;
    lv_arc_set_value(arc_, clamp(progress_, 0, 1000));
  }

  // The time as large as its box takes, with a line and the dots under it, each only when it fits whole.
  const int line = ui::line_height(Font::BODY);
  const int n = clamp(v.rounds, 1, DOTS);
  const int dot = clamp(text_w_ / (n * 2), ui::px(4), ui::px(10)), dot_gap = std::max(2, dot * 7 / 10);
  const int dots_w = n * dot + (n - 1) * dot_gap, dots_h = dot + ui::px(4);
  const bool dots_fit = dots_w <= text_w_ && text_h_ >= dots_h + line;
  const int dh = dots_fit ? dots_h : 0;
  const bool sub_fits = !v.sub.empty() && ui::text_width(v.sub, Font::BODY) <= text_w_ && text_h_ - dh >= 2 * line;
  const int sub_h = sub_fits ? line : 0;
  const Font font = fitting(v.time, text_w_, std::max(line, text_h_ - sub_h - dh));
  const int time_h = ui::line_height(font);
  int y = text_y_ + std::max(0, (text_h_ - time_h - sub_h - dh) / 2);

  ui::set_font(time_, font);
  ui::set_text(time_, v.time);
  ui::set_tone(time_, v.done ? Tone::ALERT : Tone::NORMAL, theme::INK);
  lv_obj_set_size(time_, text_w_, time_h);
  lv_obj_set_pos(time_, text_x_, y);
  y += time_h;
  show(sub_, sub_fits);
  ui::set_text(sub_, v.sub);
  lv_obj_set_size(sub_, text_w_, line);
  lv_obj_set_pos(sub_, text_x_, y);
  y += sub_h;

  const int x0 = text_x_ + (text_w_ - dots_w) / 2, yd = y + ui::px(4);
  for (int i = 0; i < DOTS; ++i) {
    const bool shown = dots_fit && i < n;
    show(dot_[i], shown);
    if (!shown) continue;
    lv_obj_set_size(dot_[i], dot, dot);
    lv_obj_set_pos(dot_[i], x0 + i * (dot + dot_gap), yd);
  }
  if (v.finished != finished_ || v.rounds != rounds_) {
    finished_ = v.finished;
    rounds_ = v.rounds;
    paint_dots(v);
  }
}

void Face::paint_dots(const View &v) {
  const theme::Role empty = card_ ? theme::CARD : theme::TRACK;
  for (int i = 0; i < DOTS; ++i) ui::set_fill(dot_[i], i < v.finished ? tone_ : Tone::NORMAL, empty);
}

void Face::on_theme() {
  lv_obj_set_style_arc_color(arc_, ui::color(card_ ? theme::CARD : theme::TRACK), LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc_, ui::tone_color(tone_, theme::ACCENT), LV_PART_INDICATOR);
  if (time_) ui::set_tone(time_, Tone::NORMAL, theme::INK);
  if (sub_) ui::set_color(sub_, theme::MUTED);
  // Colours of the look that changed: the dots again on the next paint.
  for (int i = 0; i < DOTS; ++i)
    if (dot_[i]) lv_obj_set_style_bg_color(dot_[i], ui::color(card_ ? theme::CARD : theme::TRACK), 0);
  finished_ = 255;
  for (int i = 0; i < count_; ++i) {
    Button &b = buttons_[i];
    if (!b.box) continue;
    lv_obj_set_style_bg_color(b.box, ui::color(b.main ? theme::ACCENT : theme::BUTTON), 0);
    ui::set_color(b.label, b.main ? theme::ON_ACCENT : theme::SLATE);
  }
}

Key Face::hit(tessera::Point p) const {
  if (p.x < 0 || p.y < 0) return Key::OPEN;
  // A key takes a finger a millimetre beside it too: the gaps between the keys are 2 mm.
  const int slack = ui::mm(1);
  for (int i = 0; i < count_; ++i) {
    const Button &b = buttons_[i];
    if (p.x >= b.x - slack && p.x < b.x + b.w + slack && p.y >= b.y - slack && p.y < b.y + b.h + slack) return b.key;
  }
  return Key::OPEN;
}

}  // namespace esphome::pomodoro
