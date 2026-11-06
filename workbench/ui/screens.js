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

  // colors of the screens' own, over the theme's (BLACK WHITE LINE DIM GRAY AMB HI): a color per part (the main screen's;
  // the firmware's palette would need them too). [r, g, b]
  colors: {
    SONG: [255, 184, 70], DRUMS: [236, 104, 88], BASS: [158, 130, 236], COMP: [70, 196, 176],
    PAD: [96, 164, 240], LEAD: [236, 124, 184], TEXTURE: [186, 168, 138],
  },

  // 1-bit icons, 9 px wide, rows of "#" (lit) and "." (the main screen's): one per part, drawn in its color
  icons: {
    song: [   // a record (Sam)
      "..#####..",
      ".#.....#.",
      "#.......#",
      "#...#...#",
      "#.......#",
      ".#.....#.",
      "..#####..",
    ],
    drums: [   // a drum from the side: its head on top (Sam)
      ".#######.",
      "#.......#",
      "##.....##",
      "#.#####.#",
      "#.......#",
      "#.......#",
      ".#######.",
    ],
    bass: [   // a bass guitar: the body low right, the neck up to the left (Sam)
      "##.......",
      ".##......",
      "...#.#...",
      "....###..",
      "....#####",
      "....#####",
      ".....###",
    ],
    comp: [   // keys
      "#########",
      "#.#.#.#.#",
      "#.#.#.#.#",
      "#.......#",
      "#########",
      ".........",
      ".........",
    ],
    pad: [   // a long, slow wave
      ".........",
      ".##......",
      "#..#....#",
      "....#..#.",
      ".....##..",
      ".........",
      ".........",
    ],
    lead: [   // a note
      ".....#...",
      ".....##..",
      ".....#.#.",
      ".....#...",
      "..####...",
      ".#####...",
      "..###....",
    ],
    texture: [   // dust
      "#.....#..",
      "...#....#",
      ".#.......",
      ".....#...",
      "#.......#",
      "...#.#...",
      ".#.....#.",
    ],
  },

  // the song values the controls change: the workbench row they set, the device's step rule (app.c's field_step),
  // how a value is written (key: KEY_NAME in the song's mode; mood: MOOD_NAME; mode: MODE_NAME; else the number)
  fields: {
    key: { row: "key", name: "KEY", wrap: 12, format: "key" },
    tempo: { row: "tempo", name: "TEMPO", min: 70, max: 90 },
    motion: { row: "motion", name: "MOTION", min: 0, max: 100 },
    mood: { row: "mood", name: "MOOD", min: 0, max: 3, format: "mood" },
    mode: { row: "mode", name: "SCALE", wrap: 2, format: "mode" },
    // a part's settings: `options: true` steps through its workbench row's choices (named as the row names them);
    // "variant" a re-roll (0 as rolled)
    compIns: { row: "keys", name: "INSTRUMENT", options: true },
    compRhythm: { row: "rhythm", name: "RHYTHM", options: true },
    compVoicing: { row: "voicings", name: "VOICINGS", format: "variant", min: 0, max: 99 },
    compReg: { row: "keysOct", name: "REGISTER", options: true },
    compFill: { row: "strumFill", name: "STRUM FILL", options: true },
    compThin: { row: "busyThin", name: "BUSY LEAD THINS", options: true },
  },

  // the parts, in ALGO's order (on a part's screen ALGO moves to the next one, at its first page)
  parts: ["pSong", "pDrums", "pBass", "pComp", "pPad", "pLead", "pTexture"],

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
    // The main screen (Sam, 2026-11-06: mockup B3, chosen over A-C; the FM-1 conventions from
    // docs/research/fm1-screens-2026-11-06.md): a thin status bar (where the song is, tempo top right), the song as
    // the hero (the chord, the key, a progress bar through the song, a pill for a change on its way), six part tiles
    // in their colors with their icons, and the 4-knob strip with fill bars. ALGO picks the part (the convention:
    // ALGO = track), EDIT opens it; the highlighted tile gets its part's color down its left edge.
    main: {
      title: "Main",
      menu: ["pSong", "pDrums", "pBass", "pComp", "pPad", "pLead", "pTexture"],
      bands: [
        { y: 0, h: 16, draw: [
          { when: "song", text: "{section}  {bar}/{bars}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "!song", text: "{idle}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "song", text: "{tempo} BPM", font: "T", right: 232, y: 2, color: "HI" },
        ] },
        { y: 18, h: 46, draw: [
          { when: "sel0", rect: [2, 0, 236, 46], color: "LINE" },
          { when: "sel0", rect: [2, 0, 2, 46], color: "SONG" },
          { when: "song", text: "{chord}", font: "B", x: 8, y: 2, color: "WHITE" },
          { when: "song", text: "{key} {mode}", font: "XS", right: 232, y: 2, color: "HI" },
          { when: ["song", "!pending"], icon: "song", x: 8, y: 26, color: "SONG" },
          { when: ["song", "!pending"], text: "{map}", font: "T", x: 21, y: 24, color: "GRAY" },
          { when: "pending", rect: [6, 21, 228, 15], color: "DIM" },
          { when: "pending", text: "{pending}", font: "T", x: 10, y: 23, color: "BLACK" },
          { when: "!song", text: "{idle}", font: "B", x: 8, y: 2, color: "GRAY" },
          { meter: [8, 40, 224, 3], value: "progress", color: "AMB", back: "LINE" },
        ] },
        { y: 66, h: 144, draw: [
          { when: "sel1", rect: [2, 0, 116, 46], color: "LINE" },
          { when: "sel1", rect: [2, 0, 2, 46], color: "DRUMS" },
          { icon: "drums", x: 8, y: 5, color: "DRUMS" },
          { text: "DRUMS", font: "T", x: 21, y: 3, color: "DRUMS" },
          { text: "{kit}", font: "XS", x: 8, y: 15, color: "WHITE" },
          { text: "{pattern}", font: "T", x: 8, y: 32, color: "GRAY" },
          { when: "sel2", rect: [122, 0, 116, 46], color: "LINE" },
          { when: "sel2", rect: [122, 0, 2, 46], color: "BASS" },
          { icon: "bass", x: 128, y: 5, color: "BASS" },
          { text: "BASS", font: "T", x: 141, y: 3, color: "BASS" },
          { text: "{bass}", font: "XS", x: 128, y: 15, color: "WHITE" },
          { when: "sel3", rect: [2, 48, 116, 46], color: "LINE" },
          { when: "sel3", rect: [2, 48, 2, 46], color: "COMP" },
          { icon: "comp", x: 8, y: 53, color: "COMP" },
          { text: "COMP", font: "T", x: 21, y: 51, color: "COMP" },
          { text: "{keys}", font: "XS", x: 8, y: 63, color: "WHITE" },
          { text: "{rhythm}", font: "T", x: 8, y: 80, color: "GRAY" },
          { when: "sel4", rect: [122, 48, 116, 46], color: "LINE" },
          { when: "sel4", rect: [122, 48, 2, 46], color: "PAD" },
          { icon: "pad", x: 128, y: 53, color: "PAD" },
          { text: "PAD", font: "T", x: 141, y: 51, color: "PAD" },
          { text: "{pad}", font: "XS", x: 128, y: 63, color: "WHITE" },
          { when: "sel5", rect: [2, 96, 116, 46], color: "LINE" },
          { when: "sel5", rect: [2, 96, 2, 46], color: "LEAD" },
          { icon: "lead", x: 8, y: 101, color: "LEAD" },
          { text: "LEAD", font: "T", x: 21, y: 99, color: "LEAD" },
          { text: "{lead}", font: "XS", x: 8, y: 111, color: "WHITE" },
          { text: "{leadStyle}", font: "T", x: 8, y: 128, color: "GRAY" },
          { when: "sel6", rect: [122, 96, 116, 46], color: "LINE" },
          { when: "sel6", rect: [122, 96, 2, 46], color: "TEXTURE" },
          { icon: "texture", x: 128, y: 101, color: "TEXTURE" },
          { text: "TEXTURE", font: "T", x: 141, y: 99, color: "TEXTURE" },
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

    // The device's screen today (app.c's draw()), kept as the mock's reference: screen_check.mjs compares it with
    // the simulator pixel for pixel. Not reachable from the new main screen.
    today: {
      title: "Today's device screen (reference)",
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
      say: ["{APP}. ", { when: "!song", text: "{idle}" }, { when: "song", text: "{keyline}. {place}. CHORD {chord}" },
            { when: "pending", text: ". {pending}" }],
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
      controls: { EDIT: { go: "back" }, ALGO: { do: "part" } },
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
      controls: { EDIT: { go: "back" }, ALGO: { do: "part" } },
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
      controls: { EDIT: { go: "back" }, ALGO: { do: "part" } },
      say: ["BASS"],
    },
    // The Comp part (the part-screen template, 2026-11-06): the status bar as on the main screen, the part's name in
    // its color with its icon and the page (SELECT steps the pages), then one row per knob (K1-K4: its name, its
    // value large, a fill bar in the part's color), and a hint line naming the buttons. ALGO moves to the next part,
    // OCT-/OCT+ step the register (Sam: the octave buttons move register dropdowns), EDIT goes back.
    pComp: {
      title: "Comp",
      pages: ["pComp", "pCompMore"],
      bands: [
        { y: 0, h: 16, draw: [
          { when: "song", text: "{section}  {bar}/{bars}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "!song", text: "{idle}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "song", text: "{tempo} BPM", font: "T", right: 232, y: 2, color: "HI" },
        ] },
        { y: 18, h: 28, draw: [
          { rect: [2, 0, 2, 26], color: "COMP" },
          { icon: "comp", x: 10, y: 9, color: "COMP" },
          { text: "COMP", font: "B", x: 23, y: 4, color: "COMP" },
          { text: "{page}", font: "T", after: 8, y: 8, color: "GRAY" },
          { when: ["song", "!pending"], text: "{chord}", font: "XS", right: 232, y: 5, color: "WHITE" },
          { when: "pending", text: "{pending}", font: "T", right: 232, y: 8, color: "HI" },
        ] },
        { y: 48, h: 144, draw: [
          { text: "K1  INSTRUMENT", font: "T", x: 8, y: 3, color: "GRAY" },
          { text: "{compIns}", font: "B", x: 8, y: 14, color: "WHITE" },
          { meter: [8, 33, 224, 2], value: "compInsFrac", color: "COMP", back: "LINE" },
          { text: "K2  RHYTHM", font: "T", x: 8, y: 39, color: "GRAY" },
          { text: "{compRhythm}", font: "B", x: 8, y: 50, color: "WHITE" },
          { meter: [8, 69, 224, 2], value: "compRhythmFrac", color: "COMP", back: "LINE" },
          { text: "K3  VOICINGS", font: "T", x: 8, y: 75, color: "GRAY" },
          { text: "{compVoicing}", font: "B", x: 8, y: 86, color: "WHITE" },
          { meter: [8, 105, 224, 2], value: "compVoicingFrac", color: "COMP", back: "LINE" },
          { text: "K4  REGISTER", font: "T", x: 8, y: 111, color: "GRAY" },
          { text: "{compReg}", font: "B", x: 8, y: 122, color: "WHITE" },
          { meter: [8, 141, 224, 2], value: "compRegFrac", color: "COMP", back: "LINE" },
        ] },
        { y: 200, h: 40, draw: [
          { rect: [8, 0, 224, 1], color: "LINE" },
          { text: "ALGO PART   SELECT PAGE   OCT REGISTER", font: "T", x: 8, y: 8, color: "DIM" },
          { text: "EDIT BACK   HOME MAIN", font: "T", x: 8, y: 24, color: "DIM" },
        ] },
      ],
      knobs: [{ field: "compIns", step: 1 }, { field: "compRhythm", step: 1 }, { field: "compVoicing", step: 1 }, { field: "compReg", step: 1 }],
      controls: {
        ALGO: { do: "part" }, SELECT: { do: "page" }, EDIT: { go: "back" },
        "OCT-": { do: "nudge", field: "compReg", by: -1 }, "OCT+": { do: "nudge", field: "compReg", by: 1 },
      },
      say: ["COMP, PAGE {page}. INSTRUMENT {compIns}. RHYTHM {compRhythm}. VOICINGS {compVoicing}. REGISTER {compReg}."],
    },
    pCompMore: {
      title: "Comp: more",
      pages: ["pComp", "pCompMore"],
      bands: [
        { y: 0, h: 16, draw: [
          { when: "song", text: "{section}  {bar}/{bars}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "!song", text: "{idle}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "song", text: "{tempo} BPM", font: "T", right: 232, y: 2, color: "HI" },
        ] },
        { y: 18, h: 28, draw: [
          { rect: [2, 0, 2, 26], color: "COMP" },
          { icon: "comp", x: 10, y: 9, color: "COMP" },
          { text: "COMP", font: "B", x: 23, y: 4, color: "COMP" },
          { text: "{page}", font: "T", after: 8, y: 8, color: "GRAY" },
          { when: ["song", "!pending"], text: "{chord}", font: "XS", right: 232, y: 5, color: "WHITE" },
          { when: "pending", text: "{pending}", font: "T", right: 232, y: 8, color: "HI" },
        ] },
        { y: 48, h: 144, draw: [
          { text: "K1  STRUM FILL", font: "T", x: 8, y: 3, color: "GRAY" },
          { text: "{compFill}", font: "B", x: 8, y: 14, color: "WHITE" },
          { meter: [8, 33, 224, 2], value: "compFillFrac", color: "COMP", back: "LINE" },
          { text: "K2  BUSY LEAD THINS", font: "T", x: 8, y: 39, color: "GRAY" },
          { text: "{compThin}", font: "B", x: 8, y: 50, color: "WHITE" },
          { meter: [8, 69, 224, 2], value: "compThinFrac", color: "COMP", back: "LINE" },
        ] },
        { y: 200, h: 40, draw: [
          { rect: [8, 0, 224, 1], color: "LINE" },
          { text: "ALGO PART   SELECT PAGE   OCT REGISTER", font: "T", x: 8, y: 8, color: "DIM" },
          { text: "EDIT BACK   HOME MAIN", font: "T", x: 8, y: 24, color: "DIM" },
        ] },
      ],
      knobs: [{ field: "compFill", step: 1 }, { field: "compThin", step: 1 }],
      controls: {
        ALGO: { do: "part" }, SELECT: { do: "page" }, EDIT: { go: "back" },
        "OCT-": { do: "nudge", field: "compReg", by: -1 }, "OCT+": { do: "nudge", field: "compReg", by: 1 },
      },
      say: ["COMP, PAGE {page}. STRUM FILL {compFill}. BUSY LEAD THINS {compThin}."],
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
      controls: { EDIT: { go: "back" }, ALGO: { do: "part" } },
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
      controls: { EDIT: { go: "back" }, ALGO: { do: "part" } },
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
      controls: { EDIT: { go: "back" }, ALGO: { do: "part" } },
      say: ["TEXTURE"],
    },
  },
};
