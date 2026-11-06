// SPDX-License-Identifier: GPL-3.0-or-later
// The device's main screen as the firmware draws it today (app.c's draw()), as screen data: screen_check.mjs adds it
// to ui/screens.js's screens and compares it with the host simulator pixel for pixel. Not one of the workbench's
// screens (Sam, 2026-11-06: it went from the mock); it moves on when the firmware draws the new screens.
export default {
  reference: true,   // draws as the device does today: a waiting change shows only in the pending line
  title: "Today's device screen (reference)",
  bands: [
    { y: 0, h: 32, draw: [
      { when: "msg", text: "{msg0}", font: "B", x: 10, y: 6, color: "WHITE" },
      { when: "msg", text: "{msg1}", font: "B", after: 8, y: 6, color: "HI" },
      { when: "!msg", text: "{APP}", font: "B", x: 10, y: 6, color: "WHITE" },
      { when: ["!msg", "song"], text: "{keyline}", font: "XS", right: 230, y: 10, color: "HI" },
      { when: ["!msg", "stopped"], text: "{keyline}", font: "XS", right: 230, y: 10, color: "HI" },   // the song ready
    ] },
    { y: 40, h: 120, draw: [
      { when: "song", text: "{place}", font: "XS", x: 10, y: 4, color: "GRAY" },
      { when: "song", text: "{chord}", font: "B", x: 10, y: 34, color: "WHITE" },
      { when: "song", text: "{pending}", font: "XS", x: 10, y: 74, color: "HI" },
      { when: ["stopped", "section"], text: "{place}", font: "XS", x: 10, y: 4, color: "GRAY" },   // held: where PLAY continues
      { when: "!song", text: "{idle}", font: "B", x: 10, y: 34, color: "GRAY" },
    ] },
    { y: 190, h: 40, draw: [
      { knobs: { x: 4, dx: 58, label: { y: 2, font: "XS", color: "GRAY" },
                 value: { y: 18, font: "XS", color: "HI", when: "song" } } },
    ] },
  ],
  knobs: [{ field: "key", step: 1 }, { field: "tempo", step: 1 }, { field: "motion", step: 5 }, { field: "mood", step: 1 }],
  say: ["{APP}. ", { when: "!song", text: "{idle}" }, { when: "song", text: "{keyline}. {place}. CHORD {chord}" },
        { when: "pending", text: ". {pending}" }],
};
