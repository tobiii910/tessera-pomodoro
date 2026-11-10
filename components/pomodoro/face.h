#pragma once
#include "esphome/components/smart_display/plugin_api.h"
#include <string>

namespace esphome::pomodoro {

enum class Phase : uint8_t { FOCUS, SHORT_BREAK, LONG_BREAK };
// IDLE: the phase is ready and waits for Start; DONE: its time is up and it waits for a tap to go on.
enum class Run : uint8_t { IDLE, RUNNING, PAUSED, DONE };

// What a finger can tap on a face. OPEN is the rest of a tile: it opens the card with every key.
enum class Key : uint8_t { NONE, PRIMARY, SKIP, RESET, OPEN };

// What a face shows now, made by the plugin every second.
struct View {
  std::string time, sub;
  int progress = 0;                          // the ring, 0 to 1000
  tessera::Tone tone = tessera::Tone::BUSY;  // the phase's colour: the ring and the rounds that are done
  bool done = false;                         // the phase is over: the time in the alert's colour
  const char *primary = "";                  // the words on the key that acts (Start, Pause, Resume)
  uint32_t primary_icon = 0;                 // the same on a round key
  uint8_t rounds = 4, finished = 0;          // the dots: how many, and how many are filled
};

// The ring, the time, the rounds and the keys of the pomodoro, laid out for the room it gets: a tile of any size or a
// card. The shapes are those of tessera-timers (Heronimonimo, MIT), with three keys in place of the add keys; the shape
// follows the room in millimetres of glass, so every key stays at least 7 mm across:
//   SIDE    the ring beside the keys (a wide tile, a card lying down),
//   STACK   the ring above a row of Reset, Skip and the key that acts (a 2x2 tile, a card standing up),
//   COMPACT the ring above Skip and the key that acts (a 1x2 tile),
//   STRIP   the ring, the time and two round keys in a row (a 2x1 tile),
//   PLAIN   the ring and the time (a 1x1 tile): a tap opens the card.
enum class Shape : uint8_t { PLAIN, STRIP, COMPACT, STACK, SIDE };

class Face {
 public:
  static constexpr int DOTS = 8;

  // Make every part in `parent`, once (create() of a tile, open() of a card). `card`: the room is a card's, on the
  // page's ground.
  void create(lv_obj_t *parent, int width, int height, const tessera::Plugin *plugin, bool card);
  void paint(const View &view);
  void on_theme();
  // The key under a point of the room (tessera::tap_point()), OPEN for the rest.
  Key hit(tessera::Point point) const;

 private:
  struct Button {
    lv_obj_t *box = nullptr, *label = nullptr;
    Key key = Key::NONE;
    int x = 0, y = 0, w = 0, h = 0;
    bool round = false, main = false;
  };
  static constexpr int BUTTONS = 3;

  void add_button(Key key, int x, int y, int w, int h, bool round, bool main);
  void paint_button(Button &b, const char *words, uint32_t icon);
  void paint_dots(const View &v);

  const tessera::Plugin *plugin_ = nullptr;
  lv_obj_t *parent_ = nullptr;
  Shape shape_ = Shape::PLAIN;
  bool card_ = false;
  lv_obj_t *arc_ = nullptr, *time_ = nullptr, *sub_ = nullptr;
  lv_obj_t *dot_[DOTS] = {};
  Button buttons_[BUTTONS];
  int count_ = 0;
  // Where the time, the line under it and the dots go: a box of `text_w_` across at (text_x_, text_y_), `text_h_`
  // high, centred in it; inside the ring or beside it.
  int text_x_ = 0, text_y_ = 0, text_w_ = 0, text_h_ = 0;
  tessera::Tone tone_ = tessera::Tone::NORMAL;
  int progress_ = -1;
  uint8_t rounds_ = 0, finished_ = 255;
};

}  // namespace esphome::pomodoro
