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

    // ---- MOCKUPS (2026-11-06, to react to; not designs yet): three main screens that show the song and its rolls
    // and lead into each part. Click a card's title in the flow view to jump to one. SELECT moves the highlight,
    // EDIT opens the highlighted part (A, B); C's EDIT opens a parts list.

    // A: a parts list. The chord and where the song is up top; one line per part (what was rolled); SELECT moves
    // the highlight, EDIT opens that part. The knobs shrink to one dim line.
    mockA: {
      title: "Mockup A: parts list",
      menu: ["pSong", "pDrums", "pBass", "pComp", "pPad", "pLead", "pTexture"],
      bands: [
        { y: 0, h: 44, draw: [
          { when: "song", text: "{chord}", font: "B", x: 10, y: 4, color: "WHITE" },
          { when: "song", text: "{tempo} BPM", font: "XS", right: 230, y: 6, color: "HI" },
          { when: "song", text: "{place}", font: "XS", x: 10, y: 24, color: "GRAY" },
          { when: "!song", text: "{idle}", font: "B", x: 10, y: 4, color: "GRAY" },
        ] },
        { y: 46, h: 126, draw: [
          { when: "sel0", rect: [4, 0, 232, 20], color: "LINE" },
          { text: "SONG", font: "T", x: 10, y: 4, color: "GRAY" },
          { text: "{key} {mode}  {mood}  MOTION {motion}", font: "XS", x: 64, y: 2, color: "WHITE" },
          { when: "sel1", rect: [4, 21, 232, 20], color: "LINE" },
          { text: "DRUMS", font: "T", x: 10, y: 25, color: "GRAY" },
          { text: "{kit} / {pattern}", font: "XS", x: 64, y: 23, color: "WHITE" },
          { when: "sel2", rect: [4, 42, 232, 20], color: "LINE" },
          { text: "BASS", font: "T", x: 10, y: 46, color: "GRAY" },
          { text: "{bass}", font: "XS", x: 64, y: 44, color: "WHITE" },
          { when: "sel3", rect: [4, 63, 232, 20], color: "LINE" },
          { text: "COMP", font: "T", x: 10, y: 67, color: "GRAY" },
          { text: "{keys} / {rhythm}", font: "XS", x: 64, y: 65, color: "WHITE" },
          { when: "sel4", rect: [4, 84, 232, 20], color: "LINE" },
          { text: "PAD", font: "T", x: 10, y: 88, color: "GRAY" },
          { text: "{pad}", font: "XS", x: 64, y: 86, color: "WHITE" },
          { when: "sel5", rect: [4, 105, 232, 20], color: "LINE" },
          { text: "LEAD", font: "T", x: 10, y: 109, color: "GRAY" },
          { text: "{lead} / {leadStyle}", font: "XS", x: 64, y: 107, color: "WHITE" },
        ] },
        { y: 172, h: 22, draw: [
          { when: "sel6", rect: [4, 0, 232, 20], color: "LINE" },
          { text: "TEXTURE", font: "T", x: 10, y: 4, color: "GRAY" },
          { text: "DUST {dust}  NOISE {noise}  ROOM {ambience}", font: "XS", x: 64, y: 2, color: "WHITE" },
        ] },
        { y: 200, h: 40, draw: [
          { when: "pending", text: "{pending}", font: "XS", x: 10, y: 2, color: "HI" },
          { text: "K1 KEY  K2 TEMPO  K3 MOTION  K4 MOOD", font: "T", x: 10, y: 22, color: "DIM" },
        ] },
      ],
      knobs: [{ field: "key", step: 1 }, { field: "tempo", step: 1 }, { field: "motion", step: 5 }, { field: "mood", step: 1 }],
      controls: { SELECT: { do: "cursor" }, EDIT: { go: "cursor" } },
      say: [{ when: "song", text: "CHORD {chord}. {place}" }, { when: "!song", text: "{idle}" }],
    },

    // B: tiles. The song across the top; six part tiles (two columns), each its instrument and its rhythm or style;
    // SELECT moves the highlight over them, EDIT opens one. No knob strip at all.
    mockB: {
      title: "Mockup B: tiles",
      menu: ["pSong", "pDrums", "pBass", "pComp", "pPad", "pLead", "pTexture"],
      bands: [
        { y: 0, h: 60, draw: [
          { when: "sel0", rect: [0, 0, 240, 58], color: "LINE" },
          { when: "song", text: "{chord}", font: "B", x: 10, y: 8, color: "WHITE" },   // (M and L have no lower case: "maj")
          { when: "song", text: "{key} {mode}", font: "XS", right: 230, y: 4, color: "HI" },
          { when: "song", text: "{tempo} BPM  {mood}", font: "XS", right: 230, y: 20, color: "HI" },
          { when: "song", text: "{place}", font: "XS", x: 10, y: 38, color: "GRAY" },
          { when: "!song", text: "{idle}", font: "M", x: 10, y: 2, color: "GRAY" },
        ] },
        { y: 62, h: 116, draw: [
          { when: "sel1", rect: [2, 0, 116, 56], color: "LINE" },
          { text: "DRUMS", font: "T", x: 8, y: 4, color: "GRAY" },
          { text: "{kit}", font: "XS", x: 8, y: 18, color: "WHITE" },
          { text: "{pattern}", font: "T", x: 8, y: 38, color: "GRAY" },
          { when: "sel2", rect: [122, 0, 116, 56], color: "LINE" },
          { text: "BASS", font: "T", x: 128, y: 4, color: "GRAY" },
          { text: "{bass}", font: "XS", x: 128, y: 18, color: "WHITE" },
          { when: "sel3", rect: [2, 58, 116, 56], color: "LINE" },
          { text: "COMP", font: "T", x: 8, y: 62, color: "GRAY" },
          { text: "{keys}", font: "XS", x: 8, y: 76, color: "WHITE" },
          { text: "{rhythm}", font: "T", x: 8, y: 96, color: "GRAY" },
          { when: "sel4", rect: [122, 58, 116, 56], color: "LINE" },
          { text: "PAD", font: "T", x: 128, y: 62, color: "GRAY" },
          { text: "{pad}", font: "XS", x: 128, y: 76, color: "WHITE" },
        ] },
        { y: 180, h: 58, draw: [
          { when: "sel5", rect: [2, 0, 116, 56], color: "LINE" },
          { text: "LEAD", font: "T", x: 8, y: 4, color: "GRAY" },
          { text: "{lead}", font: "XS", x: 8, y: 18, color: "WHITE" },
          { text: "{leadStyle}", font: "T", x: 8, y: 38, color: "GRAY" },
          { when: "sel6", rect: [122, 0, 116, 56], color: "LINE" },
          { text: "TEXTURE", font: "T", x: 128, y: 4, color: "GRAY" },
          { text: "DUST {dust}", font: "XS", x: 128, y: 18, color: "WHITE" },
          { text: "NOISE {noise}  ROOM {ambience}", font: "T", x: 128, y: 38, color: "GRAY" },
        ] },
      ],
      knobs: [{ field: "key", step: 1 }, { field: "tempo", step: 1 }, { field: "motion", step: 5 }, { field: "mood", step: 1 }],
      controls: { SELECT: { do: "cursor" }, EDIT: { go: "cursor" } },
      say: [{ when: "song", text: "CHORD {chord}. {place}" }, { when: "!song", text: "{idle}" }],
    },

    // B2: B with the FM-1 conventions applied (docs/research/fm1-screens-2026-11-06.md): a thin status bar (where the
    // song is, tempo top right), the song as the hero (the chord, the key, a progress bar through the song, a pill
    // for a change on its way), six part tiles, and the 4-knob strip back at the bottom with fill bars (8 of the top
    // 10 firmwares keep one). ALGO picks the part (the convention: ALGO = track), EDIT opens it.
    mockB2: {
      title: "Mockup B2: tiles, FM-1 conventions",
      menu: ["pSong", "pDrums", "pBass", "pComp", "pPad", "pLead", "pTexture"],
      bands: [
        { y: 0, h: 16, draw: [
          { when: "song", text: "{section}  {bar}/{bars}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "!song", text: "{idle}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "song", text: "{tempo} BPM", font: "T", right: 232, y: 2, color: "HI" },
        ] },
        { y: 18, h: 46, draw: [
          { when: "sel0", rect: [2, 0, 236, 46], color: "LINE" },
          { when: "song", text: "{chord}", font: "B", x: 8, y: 2, color: "WHITE" },
          { when: "song", text: "{key} {mode}", font: "XS", right: 232, y: 2, color: "HI" },
          { when: ["song", "!pending"], text: "{map}", font: "T", x: 8, y: 24, color: "GRAY" },
          { when: "pending", rect: [6, 21, 228, 15], color: "DIM" },
          { when: "pending", text: "{pending}", font: "T", x: 10, y: 23, color: "BLACK" },
          { when: "!song", text: "{idle}", font: "B", x: 8, y: 2, color: "GRAY" },
          { meter: [8, 40, 224, 3], value: "progress", color: "AMB", back: "LINE" },
        ] },
        { y: 66, h: 144, draw: [
          { when: "sel1", rect: [2, 0, 116, 46], color: "LINE" },
          { text: "DRUMS", font: "T", x: 8, y: 3, color: "GRAY" },
          { text: "{kit}", font: "XS", x: 8, y: 15, color: "WHITE" },
          { text: "{pattern}", font: "T", x: 8, y: 32, color: "GRAY" },
          { when: "sel2", rect: [122, 0, 116, 46], color: "LINE" },
          { text: "BASS", font: "T", x: 128, y: 3, color: "GRAY" },
          { text: "{bass}", font: "XS", x: 128, y: 15, color: "WHITE" },
          { when: "sel3", rect: [2, 48, 116, 46], color: "LINE" },
          { text: "COMP", font: "T", x: 8, y: 51, color: "GRAY" },
          { text: "{keys}", font: "XS", x: 8, y: 63, color: "WHITE" },
          { text: "{rhythm}", font: "T", x: 8, y: 80, color: "GRAY" },
          { when: "sel4", rect: [122, 48, 116, 46], color: "LINE" },
          { text: "PAD", font: "T", x: 128, y: 51, color: "GRAY" },
          { text: "{pad}", font: "XS", x: 128, y: 63, color: "WHITE" },
          { when: "sel5", rect: [2, 96, 116, 46], color: "LINE" },
          { text: "LEAD", font: "T", x: 8, y: 99, color: "GRAY" },
          { text: "{lead}", font: "XS", x: 8, y: 111, color: "WHITE" },
          { text: "{leadStyle}", font: "T", x: 8, y: 128, color: "GRAY" },
          { when: "sel6", rect: [122, 96, 116, 46], color: "LINE" },
          { text: "TEXTURE", font: "T", x: 128, y: 99, color: "GRAY" },
          { text: "DUST {dust}", font: "XS", x: 128, y: 111, color: "WHITE" },
          { text: "NOISE {noise}  ROOM {ambience}", font: "T", x: 128, y: 128, color: "GRAY" },
        ] },
        { y: 212, h: 28, draw: [
          { knobs: { x: 8, dx: 58, label: { y: 0, font: "T", color: "GRAY" },
                     value: { y: 11, font: "XS", color: "HI", when: "song" },
                     bar: { y: 26, w: 50, h: 2, color: "AMB", back: "LINE" } } },
        ] },
      ],
      knobs: [{ field: "key", step: 1 }, { field: "tempo", step: 1 }, { field: "motion", step: 5 }, { field: "mood", step: 1 }],
      controls: { ALGO: { do: "cursor" }, EDIT: { go: "cursor" } },
      say: [{ when: "song", text: "CHORD {chord}. {section}, BAR {bar} OF {bars}" }, { when: "!song", text: "{idle}" },
            { when: "pending", text: ". {pending}" }],
    },

    // C: now playing. The song is the screen (a big chord, where it is, what's waiting); the rolls in small type
    // underneath, read only; EDIT opens a parts list to pick from (two steps to a part, a calmer main screen).
    mockC: {
      title: "Mockup C: now playing",
      bands: [
        { y: 0, h: 30, draw: [
          { text: "{APP}", font: "S", x: 10, y: 6, color: "WHITE" },
          { when: "song", text: "{key} {mode}  {tempo} BPM", font: "XS", right: 230, y: 8, color: "HI" },
        ] },
        { y: 32, h: 92, draw: [
          { when: "song", text: "{chord}", font: "B", x: 10, y: 14, color: "WHITE" },   // (M and L have no lower case: "maj")
          { when: "song", text: "{place}", font: "XS", x: 10, y: 46, color: "GRAY" },
          { when: "pending", text: "{pending}", font: "XS", x: 10, y: 66, color: "HI" },
          { when: "!song", text: "{idle}", font: "L", x: 10, y: 4, color: "GRAY" },
        ] },
        { y: 126, h: 114, draw: [
          { rect: [10, 0, 220, 1], color: "LINE" },
          { text: "DRUMS  {kit} / {pattern}", font: "T", x: 10, y: 8, color: "GRAY" },
          { text: "BASS  {bass}", font: "T", x: 10, y: 24, color: "GRAY" },
          { text: "COMP  {keys} / {rhythm}", font: "T", x: 10, y: 40, color: "GRAY" },
          { text: "PAD  {pad}", font: "T", x: 10, y: 56, color: "GRAY" },
          { text: "LEAD  {lead} / {leadStyle}", font: "T", x: 10, y: 72, color: "GRAY" },
          { text: "{mood}  MOTION {motion}  /  EDIT: PARTS", font: "T", x: 10, y: 94, color: "DIM" },
        ] },
      ],
      knobs: [{ field: "key", step: 1 }, { field: "tempo", step: 1 }, { field: "motion", step: 5 }, { field: "mood", step: 1 }],
      controls: { EDIT: { go: "partsC" } },
      say: [{ when: "song", text: "CHORD {chord}. {place}" }, { when: "!song", text: "{idle}" }],
    },
    partsC: {
      title: "Mockup C: parts list",
      menu: ["pSong", "pDrums", "pBass", "pComp", "pPad", "pLead", "pTexture"],
      bands: [
        { y: 0, h: 30, draw: [{ text: "PARTS", font: "B", x: 10, y: 6, color: "WHITE" }] },
        { y: 34, h: 144, draw: [
          { when: "sel0", rect: [4, 0, 232, 20], color: "LINE" }, { text: "SONG", font: "XS", x: 12, y: 2, color: "WHITE" },
          { when: "sel1", rect: [4, 20, 232, 20], color: "LINE" }, { text: "DRUMS", font: "XS", x: 12, y: 22, color: "WHITE" },
          { when: "sel2", rect: [4, 40, 232, 20], color: "LINE" }, { text: "BASS", font: "XS", x: 12, y: 42, color: "WHITE" },
          { when: "sel3", rect: [4, 60, 232, 20], color: "LINE" }, { text: "COMP", font: "XS", x: 12, y: 62, color: "WHITE" },
          { when: "sel4", rect: [4, 80, 232, 20], color: "LINE" }, { text: "PAD", font: "XS", x: 12, y: 82, color: "WHITE" },
          { when: "sel5", rect: [4, 100, 232, 20], color: "LINE" }, { text: "LEAD", font: "XS", x: 12, y: 102, color: "WHITE" },
          { when: "sel6", rect: [4, 120, 232, 20], color: "LINE" }, { text: "TEXTURE", font: "XS", x: 12, y: 122, color: "WHITE" },
        ] },
        { y: 200, h: 20, draw: [{ text: "SELECT: MOVE  EDIT: OPEN  HOME: BACK", font: "T", x: 10, y: 4, color: "DIM" }] },
      ],
      controls: { SELECT: { do: "cursor" }, EDIT: { go: "cursor" } },
      say: ["PARTS"],
    },
    pSong: {
      title: "Part: Song (placeholder)",
      placeholder: true,
      bands: [
        { y: 0, h: 30, draw: [{ text: "SONG", font: "B", x: 10, y: 6, color: "WHITE" }, { when: "song", text: "{chord}", font: "XS", right: 230, y: 10, color: "HI" }] },
        { y: 34, h: 144, draw: [
          { text: "KEY", font: "T", x: 10, y: 4, color: "GRAY" }, { text: "{key} {mode}", font: "XS", x: 92, y: 2, color: "WHITE" },
          { text: "TEMPO", font: "T", x: 10, y: 24, color: "GRAY" }, { text: "{tempo}", font: "XS", x: 92, y: 22, color: "WHITE" },
          { text: "MOOD", font: "T", x: 10, y: 44, color: "GRAY" }, { text: "{mood}", font: "XS", x: 92, y: 42, color: "WHITE" },
          { text: "MOTION", font: "T", x: 10, y: 64, color: "GRAY" }, { text: "{motion}", font: "XS", x: 92, y: 62, color: "WHITE" },
          { text: "SWING", font: "T", x: 10, y: 84, color: "GRAY" }, { text: "{swing}", font: "XS", x: 92, y: 82, color: "WHITE" },
          { text: "MAP", font: "T", x: 10, y: 104, color: "GRAY" }, { text: "{map}", font: "XS", x: 92, y: 102, color: "WHITE" },
          { text: "CHORDS", font: "T", x: 10, y: 124, color: "GRAY" }, { text: "{prog}", font: "XS", x: 92, y: 122, color: "WHITE" },
        ] },
        { y: 200, h: 20, draw: [{ text: "PLACEHOLDER: WHAT THIS PART HAS  /  HOME", font: "T", x: 10, y: 4, color: "DIM" }] },
      ],
      controls: { EDIT: { go: "back" } },
      say: ["SONG"],
    },
    pDrums: {
      title: "Part: Drums (placeholder)",
      placeholder: true,
      bands: [
        { y: 0, h: 30, draw: [{ text: "DRUMS", font: "B", x: 10, y: 6, color: "WHITE" }, { when: "song", text: "{chord}", font: "XS", right: 230, y: 10, color: "HI" }] },
        { y: 34, h: 144, draw: [
          { text: "KIT", font: "T", x: 10, y: 4, color: "GRAY" }, { text: "{kit}", font: "XS", x: 92, y: 2, color: "WHITE" },
          { text: "PATTERN", font: "T", x: 10, y: 24, color: "GRAY" }, { text: "{pattern}", font: "XS", x: 92, y: 22, color: "WHITE" },
          { text: "KICK", font: "T", x: 10, y: 44, color: "GRAY" }, { text: "{kick}", font: "XS", x: 92, y: 42, color: "WHITE" },
          { text: "RIDE", font: "T", x: 10, y: 64, color: "GRAY" }, { text: "{ride}", font: "XS", x: 92, y: 62, color: "WHITE" },
          { text: "VARIATION", font: "T", x: 10, y: 84, color: "GRAY" }, { text: "{drumsVar}", font: "XS", x: 92, y: 82, color: "WHITE" },
        ] },
        { y: 200, h: 20, draw: [{ text: "PLACEHOLDER: WHAT THIS PART HAS  /  HOME", font: "T", x: 10, y: 4, color: "DIM" }] },
      ],
      controls: { EDIT: { go: "back" } },
      say: ["DRUMS"],
    },
    pBass: {
      title: "Part: Bass (placeholder)",
      placeholder: true,
      bands: [
        { y: 0, h: 30, draw: [{ text: "BASS", font: "B", x: 10, y: 6, color: "WHITE" }, { when: "song", text: "{chord}", font: "XS", right: 230, y: 10, color: "HI" }] },
        { y: 34, h: 144, draw: [
          { text: "INSTRUMENT", font: "T", x: 10, y: 4, color: "GRAY" }, { text: "{bass}", font: "XS", x: 92, y: 2, color: "WHITE" },
          { text: "LINE", font: "T", x: 10, y: 24, color: "GRAY" }, { text: "{bassLine}", font: "XS", x: 92, y: 22, color: "WHITE" },
        ] },
        { y: 200, h: 20, draw: [{ text: "PLACEHOLDER: WHAT THIS PART HAS  /  HOME", font: "T", x: 10, y: 4, color: "DIM" }] },
      ],
      controls: { EDIT: { go: "back" } },
      say: ["BASS"],
    },
    pComp: {
      title: "Part: Comp (placeholder)",
      placeholder: true,
      bands: [
        { y: 0, h: 30, draw: [{ text: "COMP", font: "B", x: 10, y: 6, color: "WHITE" }, { when: "song", text: "{chord}", font: "XS", right: 230, y: 10, color: "HI" }] },
        { y: 34, h: 144, draw: [
          { text: "INSTRUMENT", font: "T", x: 10, y: 4, color: "GRAY" }, { text: "{keys}", font: "XS", x: 92, y: 2, color: "WHITE" },
          { text: "RHYTHM", font: "T", x: 10, y: 24, color: "GRAY" }, { text: "{rhythm}", font: "XS", x: 92, y: 22, color: "WHITE" },
          { text: "VOICINGS", font: "T", x: 10, y: 44, color: "GRAY" }, { text: "{voicings}", font: "XS", x: 92, y: 42, color: "WHITE" },
          { text: "REGISTER", font: "T", x: 10, y: 64, color: "GRAY" }, { text: "{keysOct}", font: "XS", x: 92, y: 62, color: "WHITE" },
        ] },
        { y: 200, h: 20, draw: [{ text: "PLACEHOLDER: WHAT THIS PART HAS  /  HOME", font: "T", x: 10, y: 4, color: "DIM" }] },
      ],
      controls: { EDIT: { go: "back" } },
      say: ["COMP"],
    },
    pPad: {
      title: "Part: Pad (placeholder)",
      placeholder: true,
      bands: [
        { y: 0, h: 30, draw: [{ text: "PAD", font: "B", x: 10, y: 6, color: "WHITE" }, { when: "song", text: "{chord}", font: "XS", right: 230, y: 10, color: "HI" }] },
        { y: 34, h: 144, draw: [
          { text: "INSTRUMENT", font: "T", x: 10, y: 4, color: "GRAY" }, { text: "{pad}", font: "XS", x: 92, y: 2, color: "WHITE" },
          { text: "ON/OFF", font: "T", x: 10, y: 24, color: "GRAY" }, { text: "{padOn}", font: "XS", x: 92, y: 22, color: "WHITE" },
          { text: "PLACE", font: "T", x: 10, y: 44, color: "GRAY" }, { text: "{padPlace}", font: "XS", x: 92, y: 42, color: "WHITE" },
          { text: "NOTES", font: "T", x: 10, y: 64, color: "GRAY" }, { text: "{padNotes}", font: "XS", x: 92, y: 62, color: "WHITE" },
        ] },
        { y: 200, h: 20, draw: [{ text: "PLACEHOLDER: WHAT THIS PART HAS  /  HOME", font: "T", x: 10, y: 4, color: "DIM" }] },
      ],
      controls: { EDIT: { go: "back" } },
      say: ["PAD"],
    },
    pLead: {
      title: "Part: Lead (placeholder)",
      placeholder: true,
      bands: [
        { y: 0, h: 30, draw: [{ text: "LEAD", font: "B", x: 10, y: 6, color: "WHITE" }, { when: "song", text: "{chord}", font: "XS", right: 230, y: 10, color: "HI" }] },
        { y: 34, h: 144, draw: [
          { text: "INSTRUMENT", font: "T", x: 10, y: 4, color: "GRAY" }, { text: "{lead}", font: "XS", x: 92, y: 2, color: "WHITE" },
          { text: "STYLE", font: "T", x: 10, y: 24, color: "GRAY" }, { text: "{leadStyle}", font: "XS", x: 92, y: 22, color: "WHITE" },
          { text: "PHRASE", font: "T", x: 10, y: 44, color: "GRAY" }, { text: "{phrase}", font: "XS", x: 92, y: 42, color: "WHITE" },
          { text: "LOOP", font: "T", x: 10, y: 64, color: "GRAY" }, { text: "{leadLoop}", font: "XS", x: 92, y: 62, color: "WHITE" },
          { text: "REGISTER", font: "T", x: 10, y: 84, color: "GRAY" }, { text: "{leadOct}", font: "XS", x: 92, y: 82, color: "WHITE" },
        ] },
        { y: 200, h: 20, draw: [{ text: "PLACEHOLDER: WHAT THIS PART HAS  /  HOME", font: "T", x: 10, y: 4, color: "DIM" }] },
      ],
      controls: { EDIT: { go: "back" } },
      say: ["LEAD"],
    },
    pTexture: {
      title: "Part: Texture (placeholder)",
      placeholder: true,
      bands: [
        { y: 0, h: 30, draw: [{ text: "TEXTURE", font: "B", x: 10, y: 6, color: "WHITE" }, { when: "song", text: "{chord}", font: "XS", right: 230, y: 10, color: "HI" }] },
        { y: 34, h: 144, draw: [
          { text: "DUST", font: "T", x: 10, y: 4, color: "GRAY" }, { text: "{dust}", font: "XS", x: 92, y: 2, color: "WHITE" },
          { text: "NOISE", font: "T", x: 10, y: 24, color: "GRAY" }, { text: "{noise}", font: "XS", x: 92, y: 22, color: "WHITE" },
          { text: "AMBIENCE", font: "T", x: 10, y: 44, color: "GRAY" }, { text: "{ambience}", font: "XS", x: 92, y: 42, color: "WHITE" },
        ] },
        { y: 200, h: 20, draw: [{ text: "PLACEHOLDER: WHAT THIS PART HAS  /  HOME", font: "T", x: 10, y: 4, color: "DIM" }] },
      ],
      controls: { EDIT: { go: "back" } },
      say: ["TEXTURE"],
    },
  },
};
