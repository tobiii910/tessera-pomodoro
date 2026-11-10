# Pomodoro

A pomodoro timer on a Tessera screen: a round of focus, a short break, and after every fourth round a long break. A
ring shows how much of the phase is left, in amber while you focus, green in a short break and blue in the long one;
a row of dots under the time shows the rounds of the cycle that are done.

Based on [tessera-timers](https://github.com/Heronimonimo/tessera-timers) by Heronimonimo: the ring, the tile layouts,
the card, the top bar item, the way it keeps running through a restart and waits until Home Assistant has set the
clock all come from there.

## Set up

1. In Tessera, open Plugins, choose **Add with a link** and paste `https://github.com/tobiii910/tessera-pomodoro`.
2. Tick the screen and apply. The screen builds once.
3. In Layout, place the **Pomodoro** tile from the library's Plugins group, at any size from 1x1 to 3x3.
4. In the plugin's details on the screen's Plugins tab, under **Settings**, set the lengths and the rounds if 25, 5,
   15 minutes and 4 rounds do not suit you.

## What it shows

- **1x1**: the ring, the time, the phase and the dots. A tap opens the full-screen card with every key.
- **2x1**: the ring beside the time, with Skip and Start or Pause as round keys.
- **1x2**: the ring above Skip and Start or Pause.
- **2x2 and up, and the card**: the ring with Reset, Skip and Start, Pause or Resume.

The keys:

- **Start**, **Pause**, **Resume**: the phase that shows.
- **Skip**: the phase ends now (a focus round counts as done), and the next one waits for Start.
- **Reset**: back to focus, round 1.

When a phase ends its ring is full, the time says Done and the line under it what comes next. The pomodoro waits for
a tap on **Start**, unless **Start the next phase by itself** is on. A popup over whatever the screen shows says so
too (the setting **Show when a phase ends**, on by default). On a screen with a speaker it beeps three times.

## In the top bar

In Layout, open a page's top bar and add **Running pomodoro** from the plugins. It shows only while the pomodoro
runs: the phase and its minutes ("Focus 18 min"), the icon in the phase's colour.

## Settings

On the screen's Plugins tab, the plugin's details, **Settings**, also on the screen's own settings page and in Home
Assistant:

- **Focus length**: 1 to 90 minutes, 25 by default.
- **Short break**: 1 to 30 minutes, 5 by default.
- **Long break**: 1 to 60 minutes, 15 by default.
- **Rounds before the long break**: 2 to 8, 4 by default.
- **Start the next phase by itself**: off by default.
- **Show when a phase ends**: the popup, on by default.
- **Pomodoro sound**: only on a screen with a speaker (the part **Beep when a phase ends**, on by default).

A new length counts from the next phase on; the phase that runs keeps its own.

## In Home Assistant

The screen gets three entities:

- **Pomodoro Phase**: `focus`, `short_break` or `long_break` while one runs, `paused`, or `idle` while it waits.
- **Pomodoro Runde**: the focus round of the cycle, 1 to the rounds before the long break.
- **Pomodoro Phase beendet**: on from the end of a phase until the next one is started with a tap; with
  **Start the next phase by itself** on for five seconds at every end.

## Good to know

- There is one pomodoro per screen: every Pomodoro tile and the card show the same one.
- The screen counts on its own clock, which Home Assistant sets: until it is set (right after a start) the pomodoro
  waits.
- The Guition 4-inch has no speaker: there the end of a phase shows on the glass alone.
- Tried on Tessera's preview bench; not yet on a real screen.
