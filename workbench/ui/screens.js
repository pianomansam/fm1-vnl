// SPDX-License-Identifier: GPL-3.0-or-later
// The device's screens, as data: what each draws, what its knobs, buttons and encoders do, where they lead and what
// the screen reader says. Edit this file and press "Reload screens" under the mock (or reload the page): nothing is
// built. The workbench serves it straight from workbench/ui/. A plain JS module rather than JSON only so it can carry
// comments and trailing commas; keep it plain data (no functions), so it could become the firmware's tables later.
//
// The screen is 240 x 240, drawn as horizontal bands (gfx.c's cv_begin, cv_text, cv_blit). Each band:
//   { y, h, bg, draw: [items] }   bg a color (default BLACK); a band clips what is drawn in it
// An item:
//   { text: "{chord}", font: "B", x | right | after, y, color, when }
//     text   a template: {name} is a value below (or a line in `lines`)
//     font   T, XS, S, B, M or L (the device's, at their real metrics: ui/fonts.js)
//     x      the pen's left; right: the text ends there (measured); after: this many px past the previous item's end
//     color  BLACK, WHITE, LINE, DIM, GRAY, AMB, HI (gfx.c's palette, amber: APP_PALETTE)
//     when   a value that must be true (non-empty, non-zero), "!value" for false, or a list of them (all must hold)
//   { rect: [x, y, w, h], color, when }
//   { knobs: { x, dx, label: { y, font, color }, value: { y, font, color, when } } }: the screen's four knobs, K1 at x,
//     each next one dx to the right (their name, then their value)
// Values: APP (VNL-1), song (a song is playing), stopped, idle ("STOPPED" or "NEXT SONG"), key, mode, tempo, motion,
// mood (as the knobs write them), map, section, bar (from 1), bars, chord, pending ("TEMPO 85 IN 2 BARS", or empty),
// msg (a message is up: msg0, msg1, app_say's two parts).
//
// Controls: PLAY, FX, SEL, ENV, LFO, EDIT, GLO, HOME, SAVE, ARP, SEQ, REC, OCT-, OCT+ (plat.h's buttons) and the
// SELECT, ALGO, PRESET encoders (K1-K4 are `knobs`). Each maps to an action:
//   { go: "screen" }   open that screen (it goes on the breadcrumb); { go: "back" } the one before; { go: "home" }
//   { do: "play" }     play / stop
//   { do: "nudge", field, by }   a button: the field `by` steps (a request, landing as a knob's does)
//   { do: "turn", field, step }  an encoder: each detent `step` (a request)
// `defaults` apply on every screen unless the screen maps that control itself (null: the control does nothing there).
// A knob: { field, step, label } (label: the field's name if left out). A knob's or nudge's change is a request: it
// lands on the song's next 4-bar boundary, and the screen shows it waiting (`pending`), as app.c's requests do.
// `say`: what the screen reader says for the screen (app.c's speak()): parts, each a template or { text, when }.

export default {
  home: "main",

  // the song values the controls change: the workbench row they set, the device's step rule (app.c's field_step),
  // how a value is written (key: KEY_NAME in the song's mode; mood: MOOD_NAME; mode: MODE_NAME; else the number)
  fields: {
    key: { row: "key", name: "KEY", wrap: 12, format: "key" },
    tempo: { row: "tempo", name: "TEMPO", min: 70, max: 90 },
    motion: { row: "motion", name: "MOTION", min: 0, max: 100 },
    mood: { row: "mood", name: "MOOD", min: 0, max: 3, format: "mood" },
    mode: { row: "mode", name: "SCALE", wrap: 2, format: "mode" },
  },

  // lines more than one place uses (app.c's line_key and line_place)
  lines: {
    keyline: "{key} {mode}  {tempo} BPM",
    place: "{map}  {section}  {bar}/{bars}",
  },

  defaults: {
    PLAY: { do: "play" },
    HOME: { go: "home" },
  },

  screens: {
    // Today's screen (app.c's draw()): the header, the song, the knob strip
    main: {
      title: "Main",
      bands: [
        { y: 0, h: 32, draw: [
          { when: "msg", text: "{msg0}", font: "B", x: 10, y: 6, color: "WHITE" },
          { when: "msg", text: "{msg1}", font: "B", after: 8, y: 6, color: "HI" },
          { when: "!msg", text: "{APP}", font: "B", x: 10, y: 6, color: "WHITE" },
          { when: ["!msg", "song"], text: "{keyline}", font: "XS", right: 230, y: 10, color: "HI" },
        ] },
        { y: 40, h: 120, draw: [
          { when: "song", text: "{place}", font: "XS", x: 10, y: 4, color: "GRAY" },
          { when: "song", text: "{chord}", font: "B", x: 10, y: 34, color: "WHITE" },
          { when: "song", text: "{pending}", font: "XS", x: 10, y: 74, color: "HI" },
          { when: "!song", text: "{idle}", font: "B", x: 10, y: 34, color: "GRAY" },
        ] },
        { y: 190, h: 40, draw: [
          { knobs: { x: 4, dx: 58, label: { y: 2, font: "XS", color: "GRAY" },
                     value: { y: 18, font: "XS", color: "HI", when: "song" } } },
        ] },
      ],
      knobs: [{ field: "key", step: 1 }, { field: "tempo", step: 1 }, { field: "motion", step: 5 }, { field: "mood", step: 1 }],
      controls: {
        EDIT: { go: "edit" },   // PLACEHOLDER: the device does nothing on EDIT today; this proves a transition
      },
      say: ["{APP}. ", { when: "!song", text: "{idle}" }, { when: "song", text: "{keyline}. {place}. CHORD {chord}" },
            { when: "pending", text: ". {pending}" }],
    },

    // PLACEHOLDER, not a design: a second screen only to prove a transition end to end (EDIT opens it, HOME or EDIT
    // returns), a different knob map, and OCT-/OCT+ stepping a field
    edit: {
      title: "Edit (placeholder)",
      placeholder: true,
      bands: [
        { y: 0, h: 32, draw: [
          { when: "msg", text: "{msg0}", font: "B", x: 10, y: 6, color: "WHITE" },
          { when: "msg", text: "{msg1}", font: "B", after: 8, y: 6, color: "HI" },
          { when: "!msg", text: "EDIT", font: "B", x: 10, y: 6, color: "WHITE" },
          { when: "!msg", text: "PLACEHOLDER", font: "XS", right: 230, y: 10, color: "DIM" },
        ] },
        { y: 40, h: 120, draw: [
          { rect: [10, 0, 220, 1], color: "LINE" },
          { text: "TEMPO", font: "XS", x: 10, y: 12, color: "GRAY" },
          { text: "{tempo}", font: "M", x: 10, y: 30, color: "HI" },
          { text: "OCT- / OCT+ STEP IT", font: "XS", x: 10, y: 62, color: "DIM" },
          { when: "pending", text: "{pending}", font: "XS", x: 10, y: 84, color: "HI" },
          { text: "HOME RETURNS", font: "XS", x: 10, y: 104, color: "DIM" },
        ] },
        { y: 190, h: 40, draw: [
          { knobs: { x: 4, dx: 58, label: { y: 2, font: "XS", color: "GRAY" },
                     value: { y: 18, font: "XS", color: "HI", when: "song" } } },
        ] },
      ],
      knobs: [{ field: "tempo", step: 1 }, { field: "mode", step: 1 }],
      controls: {
        "OCT-": { do: "nudge", field: "tempo", by: -1 },
        "OCT+": { do: "nudge", field: "tempo", by: 1 },
        EDIT: { go: "back" },
        SELECT: { do: "turn", field: "motion", step: 5 },
      },
      say: ["EDIT, PLACEHOLDER. TEMPO {tempo}", { when: "pending", text: ". {pending}" }, ". HOME RETURNS"],
    },
  },
};
