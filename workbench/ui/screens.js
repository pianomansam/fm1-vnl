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
//     color  BLACK, WHITE, LINE, DIM, GRAY, AMB, HI (gfx.c's palette for app.h's APP_PALETTE: ESPRESSO)
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
//   { do: "turn", byCursor: [field | null, ...], step }  the field of the highlighted menu line (null: nothing)
//   { do: "lock", part }   the part locked, or unlocked (a new song keeps a locked part: app.c's locks; values
//                          lockDrums ... lockTexture say which are)
//   long: { ... }      on a key or a button, the action when held (a key 500 ms, a button 600 ms); let go sooner,
//                      the action itself
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
    // a change made but not live yet: its value at once, "*" in front, in this color (Sam, 2026-11-06)
    PENDING: [250, 226, 90],
    // the record's disc on the splash and the now-playing screen (Sam's splash C1: near-black, warm;
    // darkened with the background, 2026-11-11)
    VINYL: [8, 5, 3],
  },

  // 1-bit icons, 9 px wide, rows of "#" (lit) and "." (the main screen's): one per part, drawn in its color
  icons: {
    song: [   // two eighth notes, beamed, the second higher (Sam, 2026-11-06)
      "......###",
      "...###..#",
      "...#....#",
      "...#....#",
      "...#..###",
      ".###.####",
      "####..##.",
      ".##......",
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
    lock: [   // a padlock: a part locked (SEQ held on its screen), on its tile and by its name
      "...###...",
      "..#...#..",
      "..#...#..",
      ".#######.",
      ".###.###.",
      ".###.###.",
      ".#######.",
    ],
  },

  // the song values the controls change: the workbench row they set, the device's step rule (app.c's field_step),
  // how a value is written (key: KEY_NAME in the song's mode; mood: MOOD_NAME; mode: MODE_NAME; else the number)
  fields: {
    key: { row: "key", name: "KEY", wrap: 12, format: "key" },
    tempo: { row: "tempo", name: "TEMPO", min: 70, max: 90 },
    motion: { row: "motion", name: "MOTION", min: 0, max: 100 },
    mood: { row: "mood", name: "MOOD", min: 0, max: 3, format: "mood" },
    mode: { row: "mode", name: "SCALE", wrap: 6, format: "mode" },
    // a part's settings: `options: true` steps through its workbench row's choices (named as the row names them);
    // "variant" a re-roll (0 as rolled)
    compIns: { row: "keys", name: "INSTRUMENT", options: true },
    compRhythm: { row: "rhythm", name: "RHYTHM", options: true },
    compVoicing: { row: "voicings", name: "VOICINGS", format: "variant", min: 0, max: 99 },
    compReg: { row: "keysOct", name: "REGISTER", options: true },
    compFill: { row: "strumFill", name: "STRUM FILL", options: true },
    compThin: { row: "busyThin", name: "BUSY LEAD THINS", options: true },
    compWith: { row: "keysWith", name: "OVER A LOW LEAD", options: true },
    songSwing: { row: "swing", name: "SWING", options: true },
    songMap: { row: "map", name: "LAYOUT", options: true },
    songProg: { row: "prog", name: "CHORDS", options: true },
    songColor: { row: "color", name: "CHORD COLOR", options: true },
    drumKit: { row: "kit", name: "KIT", options: true },
    drumPattern: { row: "pattern", name: "PATTERN", options: true },
    drumKick: { row: "kick", name: "KICK", options: true },
    drumRide: { row: "ride", name: "RIDE", options: true },
    drumVar: { row: "drumsVar", name: "VARIATION", format: "variant", min: 0, max: 99 },
    bassIns: { row: "bass", name: "INSTRUMENT", options: true },
    bassLine: { row: "bassLine", name: "LINE", format: "variant", min: 0, max: 99 },
    padIns: { row: "pad", name: "INSTRUMENT", options: true },
    padOnSet: { row: "padOn", name: "ON/OFF", options: true },
    padPlaceSet: { row: "padPlace", name: "PLACE", options: true },
    padNotes: { row: "padNotes", name: "NOTES", format: "variant", min: 0, max: 99 },
    leadIns: { row: "lead", name: "INSTRUMENT", options: true },
    leadStyleSet: { row: "leadStyle", name: "STYLE", options: true },
    leadPhrase: { row: "phrase", name: "PHRASE", format: "variant", min: 0, max: 99 },
    leadReg: { row: "leadOct", name: "REGISTER", options: true },
    leadLoopSet: { row: "leadLoop", name: "LOOP", options: true },
    texDust: { row: "dust", name: "DUST", options: true },
    texNoise: { row: "noise", name: "NOISE", options: true },
    texAmb: { row: "ambience", name: "AMBIENCE", options: true },
    // the section playing repeats instead of moving on (the workbench's Loop section); acts at once
    loop: { row: "loop", name: "LOOP", options: true, now: true, wrap: 2 },   // turning either way flips it (Sam)
    // never end (Sam, 2026-11-06): where the outro would start, back to the section after the intro; no next song
    endless: { row: "endless", name: "STAY ON SONG", options: true, now: true, wrap: 2 },   // off by default (Sam)
  },

  // the parts, in ALGO's order (Sam, 2026-11-07: drums, bass, comp, lead, pad, texture, everywhere parts are listed) (on a part's screen ALGO moves to the next one, at its first page)
  parts: ["pSong", "pDrums", "pBass", "pComp", "pLead", "pPad", "pTexture"],

  // lines more than one place uses (app.c's line_key and line_place)
  lines: {
    keyline: "{key} {mode}  {tempo} BPM",
    place: "{map}  {section}  {bar}/{bars}",
  },

  defaults: {
    PLAY: { do: "play" },
    HOME: { go: "home" },
    SEQ: { do: "reroll" },   // a new song (Sam, 2026-11-06); the firmware's B_SEQ
    SEL: { go: "sections" },   // the song map, from any screen (Sam, 2026-11-06); on Sections, SEL goes back
    ARP: { go: "mixer" },      // the Parts roll, from any screen; on it, ARP goes back (Sam, 2026-11-06)
    FX: { go: "levelsKnobs" }, // the Mixer (its knobs layout, Sam 2026-11-08), from any screen; on it, FX goes back
    GLO: { go: "prefs" },      // the Settings, from any screen (the FM-1 convention: GLO = global); on it, GLO goes back
    // the lowest white keys, a part each (F drums, G bass, A comp, B lead, C pad), on every screen but Sections (its
    // keys jump): press mutes, a long press solos; again: off (Sam, 2026-11-06)
    KEY0: { do: "mute", part: "drums", long: { do: "solo", part: "drums" } },
    KEY2: { do: "mute", part: "bass", long: { do: "solo", part: "bass" } },
    KEY4: { do: "mute", part: "keys", long: { do: "solo", part: "keys" } },
    KEY6: { do: "mute", part: "lead", long: { do: "solo", part: "lead" } },
    KEY7: { do: "mute", part: "pad", long: { do: "solo", part: "pad" } },
  },
  // the keys' lights on every screen but one with its own (Sections): a part's key lit while it plays, blinking soloed
  keyLeds: { lanes: [0, 2, 4, 6, 7], parts: ["drums", "bass", "keys", "lead", "pad"] },

  screens: {
    // The boot splash (Sam, 2026-11-11: design C1): half a record entering from the left, its label amber with a cream
    // dot, the wordmark, an amber rule, the tagline. app.c shows it at power-on, the record spinning up from still,
    // slowing and settling (about 1.3 s, the engine and USB already running), then the home; the now-playing screen
    // shares its record, so the splash flows into it with only the text changing. Not on any control.
    splash: {
      title: "Splash",
      fade: true,   // its text fades out into now playing's (app.c), the record staying
      bands: [
        { y: 0, h: 64, draw: [{ record: [0, 120, 104], value: "spin", color: "AMB", back: "VINYL", past: "LINE" }] },
        { y: 64, h: 120, draw: [
          { record: [0, 56, 104], value: "spin", color: "AMB", back: "VINYL", past: "LINE" },
          { text: "vnl-1", font: "MARK", x: 118, y: 21, color: "WHITE" },     // its baseline at y 118
          { rect: [120, 64, 90, 2], color: "AMB" },
          { text: "LO-FI GENERATOR", font: "XS", x: 120, y: 75, color: "GRAY" },
        ] },
        { y: 184, h: 56, draw: [{ record: [0, -64, 104], value: "spin", color: "AMB", back: "VINYL", past: "LINE" }] },
      ],
      say: ["{APP}"],
    },

    // Now playing (Sam, 2026-11-11): the simple home, the splash's record with the song beside it. HOME on the main
    // screen opens it and HOME here goes back; whichever was used last is the home (HOME's, a new song's) and the
    // device remembers it, so boot lands there (the splash flows into it: the record stays, the text changes). The
    // song's name (its map's, big; a smaller face when too wide), its key, the chord, where it is, its map, the
    // tempo; the record turns while the song plays (33 1/3) and eases to a stop when it stops. Everything else as
    // everywhere.
    now: {
      title: "Now playing",
      fade: true,   // its text fades in from the splash's (app.c)
      bands: [
        { y: 0, h: 56, draw: [{ record: [0, 120, 104], value: "spin", color: "AMB", back: "VINYL", past: "LINE" }] },
        // the song beside it, centred on the record (rows 61-180): the name, key and mode, the chord playing (Sam,
        // 2026-11-11: "I still want the current chord to show"; the longest the engine names, A#maj#11, 67 px in B),
        // where it is, its map, the tempo
        { y: 56, h: 128, draw: [
          { record: [0, 64, 104], value: "spin", color: "AMB", back: "VINYL", past: "LINE" },
          { when: "known", text: "{map}", font: "M", fit: [122, "B"], x: 112, y: 5, color: "WHITE" },
          { when: "known", text: "{key} {mode}", font: "B", x: 112, y: 31, color: "HI" },
          { when: "song", text: "{chord}", font: "B", x: 112, y: 49, color: "WHITE" },
          { when: "song", text: "{section} {bar}/{bars}", font: "XS", x: 112, y: 69, color: "GRAY" },
          { when: "!song", text: "{idle}", font: "XS", x: 112, y: 69, color: "GRAY" },
          { sections: [112, 89, 122, 3], color: "AMB", back: "LINE", past: "DIM" },
          { when: "known", text: "{tempo}", font: "M", x: 112, y: 100, color: "HI" },
          { when: "known", text: "BPM", font: "XS", after: 4, y: 108, color: "GRAY" },
        ] },
        { y: 184, h: 56, draw: [{ record: [0, -64, 104], value: "spin", color: "AMB", back: "VINYL", past: "LINE" }] },
      ],
      controls: { HOME: { go: "main", home: true } },
      say: [{ when: "known", text: "{map}. {key} {mode}. " }, { when: "song", text: "CHORD {chord}. {section}, BAR {bar} OF {bars}" },
            { when: "!song", text: "{idle}" }, { when: "known", text: ", {tempo} BPM" }],
    },

    // The main screen (Sam, 2026-11-06: mockup B3, chosen over A-C; the FM-1 conventions from
    // docs/research/fm1-screens-2026-11-06.md): a thin status bar (where the song is, tempo top right), the song as
    // the hero (the chord, the key, a progress bar through the song, a pill for a change on its way), six part tiles
    // in their colors with their icons, and the 4-knob strip with fill bars. ALGO picks the part (the convention:
    // ALGO = track), EDIT opens it; the highlighted tile gets its part's color down its left edge.
    main: {
      title: "Main",
      menu: ["pSong", "pDrums", "pBass", "pComp", "pLead", "pPad", "pTexture"],
      bands: [
        { y: 0, h: 16, draw: [
          { when: "song", text: "{section}  {bar}/{bars}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: ["song", "looping"], text: "LOOP", font: "T", after: 8, y: 2, color: "HI" },
          { when: "!song", text: "{idle}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "known", text: "{tempo} BPM", font: "T", right: 232, y: 2, color: "HI" },
          // the firmware's name, as the splash spells it, small and quiet in the bar's free middle (Sam, 2026-11-12)
          { text: "vnl-1", font: "XS", x: 108, y: 0, color: "DIM" },
        ] },
        { y: 18, h: 46, draw: [
          { when: "sel0", rect: [2, 0, 236, 46], color: "LINE" },
          { when: "sel0", rect: [2, 0, 2, 46], color: "SONG" },
          { when: "song", text: "{chord}", font: "B", x: 8, y: 2, color: "WHITE" },
          { when: "song", text: "{key} {mode}", font: "XS", right: 232, y: 2, color: "HI" },
          { when: "song", icon: "song", x: 8, y: 26, color: "SONG" },
          { when: "song", text: "{map}", font: "T", x: 21, y: 24, color: "GRAY" },
          { when: "pending", text: "{pending}", font: "T", right: 232, y: 24, color: "HI" },   // as on a part's screen (Sam)
          // stopped with a song ready: its key (Sam, 2026-11-06), STOPPED only once (the status bar's)
          { when: "ready", text: "{key} {mode}", font: "B", x: 8, y: 2, color: "WHITE" },
          { when: "ready", icon: "song", x: 8, y: 26, color: "SONG" },
          { when: "ready", text: "{map}", font: "T", x: 21, y: 24, color: "GRAY" },
          { sections: [8, 40, 224, 3], color: "AMB", back: "LINE", past: "DIM" },   // the song's map, a segment a section
        ] },
        { y: 66, h: 144, draw: [
          { when: "sel1", rect: [2, 0, 116, 46], color: "LINE" },
          { when: "sel1", rect: [2, 0, 2, 46], color: "DRUMS" },
          { icon: "drums", x: 8, y: 5, color: "DRUMS" },
          { text: "DRUMS", font: "T", x: 21, y: 3, color: "DRUMS" },
          { when: "lockDrums", icon: "lock", x: 106, y: 4, color: "HI" },   // locked (Sam, 2026-11-12)
          { text: "{kit}", font: "XS", x: 8, y: 15, color: "WHITE" },
          { text: "{pattern}", font: "T", x: 8, y: 32, color: "GRAY" },
          { when: "sel2", rect: [122, 0, 116, 46], color: "LINE" },
          { when: "sel2", rect: [122, 0, 2, 46], color: "BASS" },
          { icon: "bass", x: 128, y: 5, color: "BASS" },
          { text: "BASS", font: "T", x: 141, y: 3, color: "BASS" },
          { when: "lockBass", icon: "lock", x: 226, y: 4, color: "HI" },   // locked (Sam, 2026-11-12)
          { text: "{bass}", font: "XS", x: 128, y: 15, color: "WHITE" },
          { when: "sel3", rect: [2, 48, 116, 46], color: "LINE" },
          { when: "sel3", rect: [2, 48, 2, 46], color: "COMP" },
          { icon: "comp", x: 8, y: 53, color: "COMP" },
          { text: "COMP", font: "T", x: 21, y: 51, color: "COMP" },
          { when: "lockComp", icon: "lock", x: 106, y: 52, color: "HI" },   // locked (Sam, 2026-11-12)
          { text: "{keys}", font: "XS", x: 8, y: 63, color: "WHITE" },
          { text: "{rhythm}", font: "T", x: 8, y: 80, color: "GRAY" },
          { when: "sel4", rect: [122, 48, 116, 46], color: "LINE" },
          { when: "sel4", rect: [122, 48, 2, 46], color: "LEAD" },
          { icon: "lead", x: 128, y: 53, color: "LEAD" },
          { text: "LEAD", font: "T", x: 141, y: 51, color: "LEAD" },
          { when: "lockLead", icon: "lock", x: 226, y: 52, color: "HI" },   // locked (Sam, 2026-11-12)
          { text: "{lead}", font: "XS", x: 128, y: 63, color: "WHITE" },
          { text: "{leadStyle}", font: "T", x: 128, y: 80, color: "GRAY" },
          { when: "sel5", rect: [2, 96, 116, 46], color: "LINE" },
          { when: "sel5", rect: [2, 96, 2, 46], color: "PAD" },
          { icon: "pad", x: 8, y: 101, color: "PAD" },
          { text: "PAD", font: "T", x: 21, y: 99, color: "PAD" },
          { when: "lockPad", icon: "lock", x: 106, y: 100, color: "HI" },   // locked (Sam, 2026-11-12)
          { text: "{pad}", font: "XS", x: 8, y: 111, color: "WHITE" },
          { when: "sel6", rect: [122, 96, 116, 46], color: "LINE" },
          { when: "sel6", rect: [122, 96, 2, 46], color: "TEXTURE" },
          { icon: "texture", x: 128, y: 101, color: "TEXTURE" },
          { text: "TEXTURE", font: "T", x: 141, y: 99, color: "TEXTURE" },
          { when: "lockTexture", icon: "lock", x: 226, y: 100, color: "HI" },   // locked (Sam, 2026-11-12)
          { text: "DUST {dust}", font: "XS", x: 128, y: 111, color: "WHITE" },
          { text: "NOISE {noise}  ROOM {ambience}", font: "T", x: 128, y: 128, color: "GRAY" },
        ] },
        { y: 212, h: 28, draw: [
          { knobs: { x: 8, dx: 58, label: { y: 0, font: "T", color: "GRAY" },
                     value: { y: 11, font: "XS", color: "HI", when: "known" },
                     bar: { y: 26, w: 50, h: 2, color: "AMB", back: "LINE" } } },
        ] },
      ],
      knobs: [{ field: "key", step: 1 }, { field: "tempo", step: 1 }, { field: "motion", step: 5 }, { field: "mood", step: 1 }],
      // ALGO picks the part (the convention); SELECT too, the main screen having no pages (Sam, 2026-11-06)
      controls: {
        ALGO: { do: "cursor" }, SELECT: { do: "cursor" }, EDIT: { go: "cursor" },
        // the highlighted part's instrument (or kit), a request as a knob's (Sam, 2026-11-06): a field a menu line
        // (null: nothing there)
        PRESET: { do: "turn", byCursor: [null, "drumKit", "bassIns", "compIns", "leadIns", "padIns", null], step: 1 },
        // OCT-/OCT+: the highlighted part's register, as on its own screen (Sam, 2026-11-06): the comp's and the lead's
        "OCT-": { do: "nudge", byCursor: [null, null, null, "compReg", "leadReg", null, null], by: -1 },
        "OCT+": { do: "nudge", byCursor: [null, null, null, "compReg", "leadReg", null, null], by: 1 },
        HOME: { go: "now", home: true },   // the now-playing screen, the home from then on (Sam, 2026-11-11)
      },
      say: [{ when: "song", text: "CHORD {chord}. {section}, BAR {bar} OF {bars}" }, { when: "!song", text: "{idle}" },
            { when: "ready", text: ". {key} {mode}, {tempo} BPM" },
            { when: "pending", text: ". {pending}" }],
    },

    // The parts over the song (Sam, 2026-11-06: the workbench's roll on the device): ARP, from any screen (notes laid
    // out over time, as an arpeggiator's are). A lane a part: its icon and name, then its notes
    // as the workbench's piano roll draws them, 8 bars across, scrolling with the song (played notes in its color, the
    // rest gray), the playhead. ALGO or SELECT picks
    // a lane, K1 mutes it, K2 solos it (again: off); listening only, at once. Levels later (the mixer item).
    mixer: {
      title: "Parts",
      lanes: ["drums", "bass", "keys", "lead", "pad"],
      bands: [
        { y: 0, h: 16, draw: [
          { when: "song", text: "{section}  {bar}/{bars}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: ["song", "looping"], text: "LOOP", font: "T", after: 8, y: 2, color: "HI" },
          { when: "!song", text: "{idle}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "known", text: "{tempo} BPM", font: "T", right: 232, y: 2, color: "HI" },
        ] },
        { y: 18, h: 28, draw: [
          { rect: [2, 0, 2, 26], color: "WHITE" },
          { text: "PARTS", font: "B", x: 10, y: 4, color: "WHITE" },
          { when: ["song", "!pending"], text: "{chord}", font: "XS", right: 232, y: 5, color: "WHITE" },
          { when: "pending", text: "{pending}", font: "T", right: 232, y: 8, color: "HI" },
        ] },
        { y: 50, h: 132, draw: [
          { lanes: [8, 0, 224, 132], parts: ["drums", "bass", "keys", "lead", "pad"], label: 46, rowH: 26, window: 8, keys: ["F", "G", "A", "B", "C"] },
        ] },
        { y: 196, h: 44, draw: [
          { rect: [8, 0, 224, 1], color: "LINE" },
          { text: "KEY MUTE   HOLD KEY SOLO   ALGO PICK", font: "T", x: 8, y: 8, color: "DIM" },
          { text: "ARP BACK   FX MIXER   HOME MAIN", font: "T", x: 8, y: 24, color: "DIM" },
        ] },
      ],
      controls: {
        ALGO: { do: "cursor" }, SELECT: { do: "cursor" }, K1: { do: "mute" }, K2: { do: "solo" }, ARP: { go: "back" },
      },
      say: ["PARTS. ", { when: "song", text: "{section}, BAR {bar} OF {bars}" }],
    },

    // The mixer (FX, from any screen): a fader a part, its level -12..+6 dB (kept across songs), M / S under
    // it. ALGO picks, K1 sets the level, K2 mutes, K3 solos; the keys mute and solo as everywhere.
    levels: {
      title: "Mixer",
      lanes: ["drums", "bass", "keys", "lead", "pad", "texture"],
      bands: [
        { y: 0, h: 16, draw: [
          { when: "song", text: "{section}  {bar}/{bars}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: ["song", "looping"], text: "LOOP", font: "T", after: 8, y: 2, color: "HI" },
          { when: "!song", text: "{idle}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "known", text: "{tempo} BPM", font: "T", right: 232, y: 2, color: "HI" },
        ] },
        { y: 18, h: 28, draw: [
          { rect: [2, 0, 2, 26], color: "WHITE" },
          { text: "MIXER", font: "B", x: 10, y: 4, color: "WHITE" },
          { when: ["song", "!pending"], text: "{chord}", font: "XS", right: 232, y: 5, color: "WHITE" },
          { when: "pending", text: "{pending}", font: "T", right: 232, y: 8, color: "HI" },
        ] },
        { y: 50, h: 140, draw: [
          { faders: [6, 0, 228, 140], parts: ["drums", "bass", "keys", "lead", "pad", "texture"], labels: ["DRUMS", "BASS", "COMP", "LEAD", "PAD", "TXTR"] },
        ] },
        { y: 196, h: 44, draw: [
          { rect: [8, 0, 224, 1], color: "LINE" },
          { text: "ALGO PICK   K1 LEVEL   K2 MUTE   K3 SOLO", font: "T", x: 8, y: 8, color: "DIM" },
          { text: "FX BACK   ARP PARTS   HOME MAIN", font: "T", x: 8, y: 24, color: "DIM" },
        ] },
      ],
      controls: {
        ALGO: { do: "cursor" }, SELECT: { do: "cursor" }, K1: { do: "level" }, K2: { do: "mute" }, K3: { do: "solo" }, FX: { go: "back" },
      },
      say: ["MIXER. ", { when: "song", text: "{section}, BAR {bar} OF {bars}" }],
    },

    // The mixer, laid out for the knobs (Sam, 2026-11-07; the device's since 2026-11-08, the mock's "Mixer" switch
    // under the screen picks the faders layout above to compare): the same faders, K1-K4 one part's level each, two pages
    // (SELECT or ALGO flips them: drums, bass, comp, lead; then pad, texture), the knob under each fader it drives.
    levelsKnobs: {
      title: "Mixer: knobs",
      pages: ["levelsKnobs", "levelsKnobs2"],
      bands: [
        { y: 0, h: 16, draw: [
          { when: "song", text: "{section}  {bar}/{bars}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: ["song", "looping"], text: "LOOP", font: "T", after: 8, y: 2, color: "HI" },
          { when: "!song", text: "{idle}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "known", text: "{tempo} BPM", font: "T", right: 232, y: 2, color: "HI" },
        ] },
        { y: 18, h: 28, draw: [
          { rect: [2, 0, 2, 26], color: "WHITE" },
          { text: "MIXER", font: "B", x: 10, y: 4, color: "WHITE" },
          { text: "{page}", font: "T", after: 8, y: 8, color: "GRAY" },
          { when: ["song", "!pending"], text: "{chord}", font: "XS", right: 232, y: 5, color: "WHITE" },
          { when: "pending", text: "{pending}", font: "T", right: 232, y: 8, color: "HI" },
        ] },
        { y: 50, h: 140, draw: [
          { faders: [6, 0, 228, 140], parts: ["drums", "bass", "keys", "lead", "pad", "texture"], labels: ["DRUMS", "BASS", "COMP", "LEAD", "PAD", "TXTR"],
            tags: ["K1", "K2", "K3", "K4", "", ""] },
        ] },
        { y: 196, h: 44, draw: [
          { rect: [8, 0, 224, 1], color: "LINE" },
          { text: "K1-K4 LEVEL   SELECT PAD/TXTR   KEY MUTE/SOLO", font: "T", x: 8, y: 8, color: "DIM" },
          { text: "FX BACK   ARP PARTS   HOME MAIN", font: "T", x: 8, y: 24, color: "DIM" },
        ] },
      ],
      // K1-K4 a part's level each (this page's: the tags under the faders say which); SELECT or ALGO the other page;
      // the keys mute (press) and solo (hold) as everywhere
      controls: {
        K1: { do: "level", part: "drums" }, K2: { do: "level", part: "bass" }, K3: { do: "level", part: "keys" }, K4: { do: "level", part: "lead" },
        ALGO: { do: "page" }, SELECT: { do: "page" }, FX: { go: "back" }, HOME: { go: "home" },
      },
      say: ["MIXER, PAGE {page}. ", { when: "song", text: "{section}, BAR {bar} OF {bars}" }],
    },
    levelsKnobs2: {
      title: "Mixer: knobs",
      pages: ["levelsKnobs", "levelsKnobs2"],
      bands: [
        { y: 0, h: 16, draw: [
          { when: "song", text: "{section}  {bar}/{bars}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: ["song", "looping"], text: "LOOP", font: "T", after: 8, y: 2, color: "HI" },
          { when: "!song", text: "{idle}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "known", text: "{tempo} BPM", font: "T", right: 232, y: 2, color: "HI" },
        ] },
        { y: 18, h: 28, draw: [
          { rect: [2, 0, 2, 26], color: "WHITE" },
          { text: "MIXER", font: "B", x: 10, y: 4, color: "WHITE" },
          { text: "{page}", font: "T", after: 8, y: 8, color: "GRAY" },
          { when: ["song", "!pending"], text: "{chord}", font: "XS", right: 232, y: 5, color: "WHITE" },
          { when: "pending", text: "{pending}", font: "T", right: 232, y: 8, color: "HI" },
        ] },
        { y: 50, h: 140, draw: [
          { faders: [6, 0, 228, 140], parts: ["drums", "bass", "keys", "lead", "pad", "texture"], labels: ["DRUMS", "BASS", "COMP", "LEAD", "PAD", "TXTR"],
            tags: ["", "", "", "", "K1", "K2"] },
        ] },
        { y: 196, h: 44, draw: [
          { rect: [8, 0, 224, 1], color: "LINE" },
          { text: "K1-K4 LEVEL   SELECT DRUMS-LEAD   KEY MUTE/SOLO", font: "T", x: 8, y: 8, color: "DIM" },
          { text: "FX BACK   ARP PARTS   HOME MAIN", font: "T", x: 8, y: 24, color: "DIM" },
        ] },
      ],
      // K1-K4 a part's level each (this page's: the tags under the faders say which); SELECT or ALGO the other page;
      // the keys mute (press) and solo (hold) as everywhere
      controls: {
        K1: { do: "level", part: "pad" }, K2: { do: "level", part: "texture" },
        ALGO: { do: "page" }, SELECT: { do: "page" }, FX: { go: "back" }, HOME: { go: "home" },
      },
      say: ["MIXER, PAGE {page}. ", { when: "song", text: "{section}, BAR {bar} OF {bars}" }],
    },

    // PLACEHOLDER: the user's settings, GLO (Sam, 2026-11-06): what goes here (reduced movement, the
    // screen reader, the key lights, brightness, which main screen is the default, the Advanced rules) is listed, not
    // working yet.
    prefs: {
      title: "Settings (placeholder)",
      placeholder: true,
      bands: [
        { y: 0, h: 16, draw: [
          { when: "song", text: "{section}  {bar}/{bars}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "!song", text: "{idle}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "known", text: "{tempo} BPM", font: "T", right: 232, y: 2, color: "HI" },
        ] },
        { y: 18, h: 28, draw: [
          { rect: [2, 0, 2, 26], color: "WHITE" },
          { text: "SETTINGS", font: "B", x: 10, y: 4, color: "WHITE" },
          { text: "PLACEHOLDER", font: "T", right: 232, y: 8, color: "DIM" },
        ] },
        { y: 50, h: 140, draw: [
          { text: "MOVEMENT", font: "T", x: 8, y: 4, color: "GRAY" }, { text: "LESS (A BAR AT A TIME)", font: "XS", right: 232, y: 2, color: "DIM" },
          { text: "SCREEN READER", font: "T", x: 8, y: 24, color: "GRAY" }, { text: "ON", font: "XS", right: 232, y: 22, color: "DIM" },
          { text: "KEY LIGHTS", font: "T", x: 8, y: 44, color: "GRAY" }, { text: "ON", font: "XS", right: 232, y: 42, color: "DIM" },
          { text: "BRIGHTNESS", font: "T", x: 8, y: 64, color: "GRAY" }, { text: "HIGH", font: "XS", right: 232, y: 62, color: "DIM" },
          { text: "MAIN SCREEN", font: "T", x: 8, y: 84, color: "GRAY" }, { text: "EXPANDED", font: "XS", right: 232, y: 82, color: "DIM" },
          { text: "ADVANCED RULES", font: "T", x: 8, y: 104, color: "GRAY" }, { text: "...", font: "XS", right: 232, y: 102, color: "DIM" },
        ] },
        { y: 196, h: 44, draw: [
          { rect: [8, 0, 224, 1], color: "LINE" },
          { text: "NOT WORKING YET: WHAT GOES HERE", font: "T", x: 8, y: 8, color: "DIM" },
          { text: "GLO BACK   HOME MAIN", font: "T", x: 8, y: 24, color: "DIM" },
        ] },
      ],
      controls: { GLO: { go: "back" } },
      say: ["SETTINGS, A PLACEHOLDER"],
    },

    // The song's map (Sam, 2026-11-06: where we are in the map, jump to a section, loop the one playing): SEL from
    // the main screen. One line a section (the one playing marked), ALGO or SELECT moves the highlight, EDIT goes
    // there (at the next bar line), K1 loops the section playing; the strip under the header is the main screen's.
    sections: {
      title: "Sections",
      list: "sections",
      rows: 7,   // lines that fit; more sections scroll with the highlight (Sam, 2026-11-06)
      keyLeds: { sections: [0, 2, 4, 6, 7, 9, 11, 12, 14, 16] },   // the playing section's key lit, a jump waiting blinking
      bands: [
        { y: 0, h: 16, draw: [
          { when: "song", text: "{section}  {bar}/{bars}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: ["song", "looping"], text: "LOOP", font: "T", after: 8, y: 2, color: "HI" },
          { when: "!song", text: "{idle}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "known", text: "{tempo} BPM", font: "T", right: 232, y: 2, color: "HI" },
        ] },
        { y: 18, h: 36, draw: [
          { rect: [2, 0, 2, 26], color: "SONG" },
          { icon: "song", x: 10, y: 9, color: "SONG" },
          { text: "SECTIONS", font: "B", x: 23, y: 4, color: "SONG" },
          { when: ["song", "!pending"], text: "{map}", font: "XS", right: 232, y: 5, color: "WHITE" },
          { when: "pending", text: "{pending}", font: "T", right: 232, y: 8, color: "HI" },
          { sections: [8, 31, 224, 3], color: "AMB", back: "LINE", past: "DIM" },
        ] },
        { y: 56, h: 144, draw: [
          { when: "sel0", rect: [4, 0, 232, 17], color: "LINE" },
          { when: "sec0Now", rect: [4, 0, 2, 17], color: "SONG" },
          { when: "sec0", text: "{sec0Key}", font: "T", x: 12, y: 3, color: "DIM" },
          { when: "sec0Now", text: "{sec0Key}", font: "T", x: 12, y: 3, color: "HI" },   // the playing section's key, lit
          { when: "sec0", text: "{sec0}", font: "XS", x: 26, y: 1, color: "WHITE" },
          { when: "sec0Now", text: "PLAYING", font: "T", after: 8, y: 3, color: "AMB" },
          { when: "sec0", text: "{sec0Bars} BARS", font: "T", right: 232, y: 3, color: "GRAY" },
          { when: "sel1", rect: [4, 18, 232, 17], color: "LINE" },
          { when: "sec1Now", rect: [4, 18, 2, 17], color: "SONG" },
          { when: "sec1", text: "{sec1Key}", font: "T", x: 12, y: 21, color: "DIM" },
          { when: "sec1Now", text: "{sec1Key}", font: "T", x: 12, y: 21, color: "HI" },   // the playing section's key, lit
          { when: "sec1", text: "{sec1}", font: "XS", x: 26, y: 19, color: "WHITE" },
          { when: "sec1Now", text: "PLAYING", font: "T", after: 8, y: 21, color: "AMB" },
          { when: "sec1", text: "{sec1Bars} BARS", font: "T", right: 232, y: 21, color: "GRAY" },
          { when: "sel2", rect: [4, 36, 232, 17], color: "LINE" },
          { when: "sec2Now", rect: [4, 36, 2, 17], color: "SONG" },
          { when: "sec2", text: "{sec2Key}", font: "T", x: 12, y: 39, color: "DIM" },
          { when: "sec2Now", text: "{sec2Key}", font: "T", x: 12, y: 39, color: "HI" },   // the playing section's key, lit
          { when: "sec2", text: "{sec2}", font: "XS", x: 26, y: 37, color: "WHITE" },
          { when: "sec2Now", text: "PLAYING", font: "T", after: 8, y: 39, color: "AMB" },
          { when: "sec2", text: "{sec2Bars} BARS", font: "T", right: 232, y: 39, color: "GRAY" },
          { when: "sel3", rect: [4, 54, 232, 17], color: "LINE" },
          { when: "sec3Now", rect: [4, 54, 2, 17], color: "SONG" },
          { when: "sec3", text: "{sec3Key}", font: "T", x: 12, y: 57, color: "DIM" },
          { when: "sec3Now", text: "{sec3Key}", font: "T", x: 12, y: 57, color: "HI" },   // the playing section's key, lit
          { when: "sec3", text: "{sec3}", font: "XS", x: 26, y: 55, color: "WHITE" },
          { when: "sec3Now", text: "PLAYING", font: "T", after: 8, y: 57, color: "AMB" },
          { when: "sec3", text: "{sec3Bars} BARS", font: "T", right: 232, y: 57, color: "GRAY" },
          { when: "sel4", rect: [4, 72, 232, 17], color: "LINE" },
          { when: "sec4Now", rect: [4, 72, 2, 17], color: "SONG" },
          { when: "sec4", text: "{sec4Key}", font: "T", x: 12, y: 75, color: "DIM" },
          { when: "sec4Now", text: "{sec4Key}", font: "T", x: 12, y: 75, color: "HI" },   // the playing section's key, lit
          { when: "sec4", text: "{sec4}", font: "XS", x: 26, y: 73, color: "WHITE" },
          { when: "sec4Now", text: "PLAYING", font: "T", after: 8, y: 75, color: "AMB" },
          { when: "sec4", text: "{sec4Bars} BARS", font: "T", right: 232, y: 75, color: "GRAY" },
          { when: "sel5", rect: [4, 90, 232, 17], color: "LINE" },
          { when: "sec5Now", rect: [4, 90, 2, 17], color: "SONG" },
          { when: "sec5", text: "{sec5Key}", font: "T", x: 12, y: 93, color: "DIM" },
          { when: "sec5Now", text: "{sec5Key}", font: "T", x: 12, y: 93, color: "HI" },   // the playing section's key, lit
          { when: "sec5", text: "{sec5}", font: "XS", x: 26, y: 91, color: "WHITE" },
          { when: "sec5Now", text: "PLAYING", font: "T", after: 8, y: 93, color: "AMB" },
          { when: "sec5", text: "{sec5Bars} BARS", font: "T", right: 232, y: 93, color: "GRAY" },
          { when: "sel6", rect: [4, 108, 232, 17], color: "LINE" },
          { when: "sec6Now", rect: [4, 108, 2, 17], color: "SONG" },
          { when: "sec6", text: "{sec6Key}", font: "T", x: 12, y: 111, color: "DIM" },
          { when: "sec6Now", text: "{sec6Key}", font: "T", x: 12, y: 111, color: "HI" },   // the playing section's key, lit
          { when: "sec6", text: "{sec6}", font: "XS", x: 26, y: 109, color: "WHITE" },
          { when: "sec6Now", text: "PLAYING", font: "T", after: 8, y: 111, color: "AMB" },
          { when: "sec6", text: "{sec6Bars} BARS", font: "T", right: 232, y: 111, color: "GRAY" },
          { when: "secRange", text: "{secRange}", font: "T", x: 12, y: 130, color: "GRAY" },   // the pager, under the list, left (Sam)
        ] },
        { y: 202, h: 38, draw: [
          { knobs: { x: 8, dx: 58, label: { y: 0, font: "T", color: "GRAY" }, value: { y: 11, font: "XS", color: "HI", when: "song" } } },
          { text: "EDIT OR ITS KEY: GO THERE", font: "T", right: 232, y: 2, color: "DIM" },
          { text: "SEL BACK", font: "T", right: 232, y: 16, color: "DIM" },
        ] },
      ],
      knobs: [{ field: "loop", step: 1 }, { field: "endless", step: 1 }],
      controls: {
        ALGO: { do: "cursor" }, SELECT: { do: "cursor" }, EDIT: { do: "jump" }, SEL: { go: "back" },
        // the lowest white keys jump straight to a section, first key the first section (Sam, 2026-11-06); the dim
        // letter on each line names its key
        KEY0: { do: "jump", i: 0 }, KEY2: { do: "jump", i: 1 }, KEY4: { do: "jump", i: 2 }, KEY6: { do: "jump", i: 3 }, KEY7: { do: "jump", i: 4 }, KEY9: { do: "jump", i: 5 }, KEY11: { do: "jump", i: 6 }, KEY12: { do: "jump", i: 7 }, KEY14: { do: "jump", i: 8 }, KEY16: { do: "jump", i: 9 },
      },
      say: ["SECTIONS. {section}, BAR {bar} OF {bars}", { when: "looping", text: ", LOOPING" }],
    },

    pSong: {
      title: "Song",
      pages: ["pSong", "pSongMore", "pSong3"],
      bands: [
        { y: 0, h: 16, draw: [
          { when: "song", text: "{section}  {bar}/{bars}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "!song", text: "{idle}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "known", text: "{tempo} BPM", font: "T", right: 232, y: 2, color: "HI" },
        ] },
        { y: 18, h: 28, draw: [
          { rect: [2, 0, 2, 26], color: "SONG" },
          { icon: "song", x: 10, y: 9, color: "SONG" },
          { text: "SONG", font: "B", x: 23, y: 4, color: "SONG" },
          { text: "{page}", font: "T", after: 8, y: 8, color: "GRAY" },
          { when: ["song", "!pending"], text: "{chord}", font: "XS", right: 232, y: 5, color: "WHITE" },
          { when: "pending", text: "{pending}", font: "T", right: 232, y: 8, color: "HI" },
        ] },
        { y: 48, h: 144, draw: [
          { text: "K1  KEY", font: "T", x: 8, y: 3, color: "GRAY" },
          { text: "{key}", font: "B", x: 8, y: 14, color: "WHITE", arrows: true },
          { meter: [8, 33, 224, 2], value: "keyFrac", color: "SONG", back: "LINE" },
          { text: "K2  SCALE", font: "T", x: 8, y: 39, color: "GRAY" },
          { text: "{mode}", font: "B", x: 8, y: 50, color: "WHITE", arrows: true },
          { meter: [8, 69, 224, 2], value: "modeFrac", color: "SONG", back: "LINE" },
          { text: "K3  TEMPO", font: "T", x: 8, y: 75, color: "GRAY" },
          { text: "{tempo}", font: "B", x: 8, y: 86, color: "WHITE", arrows: true },
          { meter: [8, 105, 224, 2], value: "tempoFrac", color: "SONG", back: "LINE" },
          { text: "K4  MOOD", font: "T", x: 8, y: 111, color: "GRAY" },
          { text: "{mood}", font: "B", x: 8, y: 122, color: "WHITE", arrows: true },
          { meter: [8, 141, 224, 2], value: "moodFrac", color: "SONG", back: "LINE" },
        ] },
        { y: 200, h: 40, draw: [
          { rect: [8, 0, 224, 1], color: "LINE" },
          { text: "ALGO PART   SELECT PAGE", font: "T", x: 8, y: 8, color: "DIM" },
          { text: "EDIT BACK   HOME MAIN", font: "T", x: 8, y: 24, color: "DIM" },
        ] },
      ],
      knobs: [{ field: "key", step: 1 }, { field: "mode", step: 1 }, { field: "tempo", step: 1 }, { field: "mood", step: 1 }],
      controls: {
        ALGO: { do: "part" }, SELECT: { do: "page" }, EDIT: { go: "back" },
      },
      say: ["SONG, PAGE {page}. KEY {key}. SCALE {mode}. TEMPO {tempo}. MOOD {mood}."],
    },
    pSongMore: {
      title: "Song: more",
      pages: ["pSong", "pSongMore", "pSong3"],
      bands: [
        { y: 0, h: 16, draw: [
          { when: "song", text: "{section}  {bar}/{bars}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "!song", text: "{idle}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "known", text: "{tempo} BPM", font: "T", right: 232, y: 2, color: "HI" },
        ] },
        { y: 18, h: 28, draw: [
          { rect: [2, 0, 2, 26], color: "SONG" },
          { icon: "song", x: 10, y: 9, color: "SONG" },
          { text: "SONG", font: "B", x: 23, y: 4, color: "SONG" },
          { text: "{page}", font: "T", after: 8, y: 8, color: "GRAY" },
          { when: ["song", "!pending"], text: "{chord}", font: "XS", right: 232, y: 5, color: "WHITE" },
          { when: "pending", text: "{pending}", font: "T", right: 232, y: 8, color: "HI" },
        ] },
        { y: 48, h: 144, draw: [
          { text: "K1  MOTION", font: "T", x: 8, y: 3, color: "GRAY" },
          { text: "{motion}", font: "B", x: 8, y: 14, color: "WHITE", arrows: true },
          { meter: [8, 33, 224, 2], value: "motionFrac", color: "SONG", back: "LINE" },
          { text: "K2  SWING", font: "T", x: 8, y: 39, color: "GRAY" },
          { text: "{songSwing}", font: "B", x: 8, y: 50, color: "WHITE", arrows: true },
          { meter: [8, 69, 224, 2], value: "songSwingFrac", color: "SONG", back: "LINE" },
          { text: "K3  LAYOUT", font: "T", x: 8, y: 75, color: "GRAY" },
          { text: "{songMap}", font: "B", x: 8, y: 86, color: "WHITE", arrows: true },
          { meter: [8, 105, 224, 2], value: "songMapFrac", color: "SONG", back: "LINE" },
          { text: "K4  CHORDS", font: "T", x: 8, y: 111, color: "GRAY" },
          { text: "{songProg}", font: "B", x: 8, y: 122, color: "WHITE", arrows: true },
          { meter: [8, 141, 224, 2], value: "songProgFrac", color: "SONG", back: "LINE" },
        ] },
        { y: 200, h: 40, draw: [
          { rect: [8, 0, 224, 1], color: "LINE" },
          { text: "ALGO PART   SELECT PAGE", font: "T", x: 8, y: 8, color: "DIM" },
          { text: "EDIT BACK   HOME MAIN", font: "T", x: 8, y: 24, color: "DIM" },
        ] },
      ],
      knobs: [{ field: "motion", step: 1 }, { field: "songSwing", step: 1 }, { field: "songMap", step: 1 }, { field: "songProg", step: 1 }],
      controls: {
        ALGO: { do: "part" }, SELECT: { do: "page" }, EDIT: { go: "back" },
      },
      say: ["SONG, PAGE {page}. MOTION {motion}. SWING {songSwing}. LAYOUT {songMap}. CHORDS {songProg}."],
    },
    // the Song part's third page (Sam, 2026-11-06): the chord color. The Family row isn't a field: its values are the
    // page's own strings (acoustic, synth), not an engine field the device can step (choices_gen has no table for it).
    // The six modes bring back no setting of their own: the Scale knob (page 1) steps through all six.
    pSong3: {
      title: "Song: color",
      pages: ["pSong", "pSongMore", "pSong3"],
      bands: [
        { y: 0, h: 16, draw: [
          { when: "song", text: "{section}  {bar}/{bars}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "!song", text: "{idle}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "known", text: "{tempo} BPM", font: "T", right: 232, y: 2, color: "HI" },
        ] },
        { y: 18, h: 28, draw: [
          { rect: [2, 0, 2, 26], color: "SONG" },
          { icon: "song", x: 10, y: 9, color: "SONG" },
          { text: "SONG", font: "B", x: 23, y: 4, color: "SONG" },
          { text: "{page}", font: "T", after: 8, y: 8, color: "GRAY" },
          { when: ["song", "!pending"], text: "{chord}", font: "XS", right: 232, y: 5, color: "WHITE" },
          { when: "pending", text: "{pending}", font: "T", right: 232, y: 8, color: "HI" },
        ] },
        { y: 48, h: 144, draw: [
          { text: "K1  CHORD COLOR", font: "T", x: 8, y: 3, color: "GRAY" },
          { text: "{songColor}", font: "B", x: 8, y: 14, color: "WHITE", arrows: true },
          { meter: [8, 33, 224, 2], value: "songColorFrac", color: "SONG", back: "LINE" },
        ] },
        { y: 200, h: 40, draw: [
          { rect: [8, 0, 224, 1], color: "LINE" },
          { text: "ALGO PART   SELECT PAGE", font: "T", x: 8, y: 8, color: "DIM" },
          { text: "EDIT BACK   HOME MAIN", font: "T", x: 8, y: 24, color: "DIM" },
        ] },
      ],
      knobs: [{ field: "songColor", step: 1 }],
      controls: {
        ALGO: { do: "part" }, SELECT: { do: "page" }, EDIT: { go: "back" },
      },
      say: ["SONG, PAGE {page}. CHORD COLOR {songColor}."],
    },
    pDrums: {
      title: "Drums",
      pages: ["pDrums", "pDrumsMore"],
      bands: [
        { y: 0, h: 16, draw: [
          { when: "song", text: "{section}  {bar}/{bars}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "!song", text: "{idle}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "known", text: "{tempo} BPM", font: "T", right: 232, y: 2, color: "HI" },
        ] },
        { y: 18, h: 28, draw: [
          { rect: [2, 0, 2, 26], color: "DRUMS" },
          { icon: "drums", x: 10, y: 9, color: "DRUMS" },
          { text: "DRUMS", font: "B", x: 23, y: 4, color: "DRUMS" },
          { text: "{page}", font: "T", after: 8, y: 8, color: "GRAY" },
          { when: "lockDrums", icon: "lock", x: 93, y: 9, color: "HI" },   // locked: the padlock by its name
          { when: ["song", "!pending"], text: "{chord}", font: "XS", right: 232, y: 5, color: "WHITE" },
          { when: "pending", text: "{pending}", font: "T", right: 232, y: 8, color: "HI" },
        ] },
        { y: 48, h: 144, draw: [
          { text: "K1  KIT", font: "T", x: 8, y: 3, color: "GRAY" },
          { text: "{drumKit}", font: "B", x: 8, y: 14, color: "WHITE", arrows: true },
          { meter: [8, 33, 224, 2], value: "drumKitFrac", color: "DRUMS", back: "LINE" },
          { text: "K2  PATTERN", font: "T", x: 8, y: 39, color: "GRAY" },
          { text: "{drumPattern}", font: "B", x: 8, y: 50, color: "WHITE", arrows: true },
          { meter: [8, 69, 224, 2], value: "drumPatternFrac", color: "DRUMS", back: "LINE" },
          { text: "K3  KICK", font: "T", x: 8, y: 75, color: "GRAY" },
          { text: "{drumKick}", font: "B", x: 8, y: 86, color: "WHITE", arrows: true },
          { meter: [8, 105, 224, 2], value: "drumKickFrac", color: "DRUMS", back: "LINE" },
          { text: "K4  RIDE", font: "T", x: 8, y: 111, color: "GRAY" },
          { text: "{drumRide}", font: "B", x: 8, y: 122, color: "WHITE", arrows: true },
          { meter: [8, 141, 224, 2], value: "drumRideFrac", color: "DRUMS", back: "LINE" },
        ] },
        { y: 200, h: 40, draw: [
          { rect: [8, 0, 224, 1], color: "LINE" },
          { text: "ALGO PART   SELECT PAGE", font: "T", x: 8, y: 8, color: "DIM" },
          { text: "EDIT BACK   HOME MAIN   SEQ RE-ROLL   HOLD LOCK", font: "T", x: 8, y: 24, color: "DIM" },
        ] },
      ],
      knobs: [{ field: "drumKit", step: 1 }, { field: "drumPattern", step: 1 }, { field: "drumKick", step: 1 }, { field: "drumRide", step: 1 }],
      controls: {
        PRESET: { do: "turn", field: "drumKit", step: 1 },   // browse its instrument (Sam, 2026-11-06)
        SEQ: { do: "reroll", part: "drums", long: { do: "lock", part: "drums" } },   // a press: this part re-rolls (its 🎲), on
        // the 4-bar boundary (Sam, 2026-11-06); held: locked, a new song keeps it (Sam, 2026-11-12; again: unlocked)
        ALGO: { do: "part" }, SELECT: { do: "page" }, EDIT: { go: "back" },
      },
      say: ["DRUMS, PAGE {page}. KIT {drumKit}. PATTERN {drumPattern}. KICK {drumKick}. RIDE {drumRide}.", { when: "lockDrums", text: " LOCKED." }],
    },
    pDrumsMore: {
      title: "Drums: more",
      pages: ["pDrums", "pDrumsMore"],
      bands: [
        { y: 0, h: 16, draw: [
          { when: "song", text: "{section}  {bar}/{bars}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "!song", text: "{idle}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "known", text: "{tempo} BPM", font: "T", right: 232, y: 2, color: "HI" },
        ] },
        { y: 18, h: 28, draw: [
          { rect: [2, 0, 2, 26], color: "DRUMS" },
          { icon: "drums", x: 10, y: 9, color: "DRUMS" },
          { text: "DRUMS", font: "B", x: 23, y: 4, color: "DRUMS" },
          { text: "{page}", font: "T", after: 8, y: 8, color: "GRAY" },
          { when: "lockDrums", icon: "lock", x: 93, y: 9, color: "HI" },   // locked: the padlock by its name
          { when: ["song", "!pending"], text: "{chord}", font: "XS", right: 232, y: 5, color: "WHITE" },
          { when: "pending", text: "{pending}", font: "T", right: 232, y: 8, color: "HI" },
        ] },
        { y: 48, h: 144, draw: [
          { text: "K1  VARIATION", font: "T", x: 8, y: 3, color: "GRAY" },
          { text: "{drumVar}", font: "B", x: 8, y: 14, color: "WHITE", arrows: true },
          { meter: [8, 33, 224, 2], value: "drumVarFrac", color: "DRUMS", back: "LINE" },
        ] },
        { y: 200, h: 40, draw: [
          { rect: [8, 0, 224, 1], color: "LINE" },
          { text: "ALGO PART   SELECT PAGE", font: "T", x: 8, y: 8, color: "DIM" },
          { text: "EDIT BACK   HOME MAIN   SEQ RE-ROLL   HOLD LOCK", font: "T", x: 8, y: 24, color: "DIM" },
        ] },
      ],
      knobs: [{ field: "drumVar", step: 1 }],
      controls: {
        PRESET: { do: "turn", field: "drumKit", step: 1 },   // browse its instrument (Sam, 2026-11-06)
        SEQ: { do: "reroll", part: "drums", long: { do: "lock", part: "drums" } },   // a press: this part re-rolls (its 🎲), on
        // the 4-bar boundary (Sam, 2026-11-06); held: locked, a new song keeps it (Sam, 2026-11-12; again: unlocked)
        ALGO: { do: "part" }, SELECT: { do: "page" }, EDIT: { go: "back" },
      },
      say: ["DRUMS, PAGE {page}. VARIATION {drumVar}.", { when: "lockDrums", text: " LOCKED." }],
    },
    pBass: {
      title: "Bass",
      pages: ["pBass"],
      bands: [
        { y: 0, h: 16, draw: [
          { when: "song", text: "{section}  {bar}/{bars}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "!song", text: "{idle}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "known", text: "{tempo} BPM", font: "T", right: 232, y: 2, color: "HI" },
        ] },
        { y: 18, h: 28, draw: [
          { rect: [2, 0, 2, 26], color: "BASS" },
          { icon: "bass", x: 10, y: 9, color: "BASS" },
          { text: "BASS", font: "B", x: 23, y: 4, color: "BASS" },
          { text: "{page}", font: "T", after: 8, y: 8, color: "GRAY" },
          { when: "lockBass", icon: "lock", x: 63, y: 9, color: "HI" },   // locked: the padlock by its name
          { when: ["song", "!pending"], text: "{chord}", font: "XS", right: 232, y: 5, color: "WHITE" },
          { when: "pending", text: "{pending}", font: "T", right: 232, y: 8, color: "HI" },
        ] },
        { y: 48, h: 144, draw: [
          { text: "K1  INSTRUMENT", font: "T", x: 8, y: 3, color: "GRAY" },
          { text: "{bassIns}", font: "B", x: 8, y: 14, color: "WHITE", arrows: true },
          { meter: [8, 33, 224, 2], value: "bassInsFrac", color: "BASS", back: "LINE" },
          { text: "K2  LINE", font: "T", x: 8, y: 39, color: "GRAY" },
          { text: "{bassLine}", font: "B", x: 8, y: 50, color: "WHITE", arrows: true },
          { meter: [8, 69, 224, 2], value: "bassLineFrac", color: "BASS", back: "LINE" },
        ] },
        { y: 200, h: 40, draw: [
          { rect: [8, 0, 224, 1], color: "LINE" },
          { text: "ALGO PART", font: "T", x: 8, y: 8, color: "DIM" },
          { text: "EDIT BACK   HOME MAIN   SEQ RE-ROLL   HOLD LOCK", font: "T", x: 8, y: 24, color: "DIM" },
        ] },
      ],
      knobs: [{ field: "bassIns", step: 1 }, { field: "bassLine", step: 1 }],
      controls: {
        PRESET: { do: "turn", field: "bassIns", step: 1 },   // browse its instrument (Sam, 2026-11-06)
        SEQ: { do: "reroll", part: "bass", long: { do: "lock", part: "bass" } },   // a press: this part re-rolls (its 🎲), on
        // the 4-bar boundary (Sam, 2026-11-06); held: locked, a new song keeps it (Sam, 2026-11-12; again: unlocked)
        ALGO: { do: "part" }, EDIT: { go: "back" },
      },
      say: ["BASS. INSTRUMENT {bassIns}. LINE {bassLine}.", { when: "lockBass", text: " LOCKED." }],
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
          { when: "known", text: "{tempo} BPM", font: "T", right: 232, y: 2, color: "HI" },
        ] },
        { y: 18, h: 28, draw: [
          { rect: [2, 0, 2, 26], color: "COMP" },
          { icon: "comp", x: 10, y: 9, color: "COMP" },
          { text: "COMP", font: "B", x: 23, y: 4, color: "COMP" },
          { text: "{page}", font: "T", after: 8, y: 8, color: "GRAY" },
          { when: "lockComp", icon: "lock", x: 85, y: 9, color: "HI" },   // locked: the padlock by its name
          { when: ["song", "!pending"], text: "{chord}", font: "XS", right: 232, y: 5, color: "WHITE" },
          { when: "pending", text: "{pending}", font: "T", right: 232, y: 8, color: "HI" },
        ] },
        { y: 48, h: 144, draw: [
          { text: "K1  INSTRUMENT", font: "T", x: 8, y: 3, color: "GRAY" },
          { text: "{compIns}", font: "B", x: 8, y: 14, color: "WHITE", arrows: true },
          { meter: [8, 33, 224, 2], value: "compInsFrac", color: "COMP", back: "LINE" },
          { text: "K2  RHYTHM", font: "T", x: 8, y: 39, color: "GRAY" },
          { text: "{compRhythm}", font: "B", x: 8, y: 50, color: "WHITE", arrows: true },
          { meter: [8, 69, 224, 2], value: "compRhythmFrac", color: "COMP", back: "LINE" },
          { text: "K3  VOICINGS", font: "T", x: 8, y: 75, color: "GRAY" },
          { text: "{compVoicing}", font: "B", x: 8, y: 86, color: "WHITE", arrows: true },
          { meter: [8, 105, 224, 2], value: "compVoicingFrac", color: "COMP", back: "LINE" },
          { text: "K4  REGISTER", font: "T", x: 8, y: 111, color: "GRAY" },
          { text: "{compReg}", font: "B", x: 8, y: 122, color: "WHITE", arrows: true },
          { meter: [8, 141, 224, 2], value: "compRegFrac", color: "COMP", back: "LINE" },
        ] },
        { y: 200, h: 40, draw: [
          { rect: [8, 0, 224, 1], color: "LINE" },
          { text: "ALGO PART   SELECT PAGE   OCT REGISTER", font: "T", x: 8, y: 8, color: "DIM" },
          { text: "EDIT BACK   HOME MAIN   SEQ RE-ROLL   HOLD LOCK", font: "T", x: 8, y: 24, color: "DIM" },
        ] },
      ],
      knobs: [{ field: "compIns", step: 1 }, { field: "compRhythm", step: 1 }, { field: "compVoicing", step: 1 }, { field: "compReg", step: 1 }],
      controls: {
        PRESET: { do: "turn", field: "compIns", step: 1 },   // browse its instrument (Sam, 2026-11-06)
        SEQ: { do: "reroll", part: "keys", long: { do: "lock", part: "keys" } },   // a press: this part re-rolls (its 🎲), on
        // the 4-bar boundary (Sam, 2026-11-06); held: locked, a new song keeps it (Sam, 2026-11-12; again: unlocked)
        ALGO: { do: "part" }, SELECT: { do: "page" }, EDIT: { go: "back" },
        "OCT-": { do: "nudge", field: "compReg", by: -1 }, "OCT+": { do: "nudge", field: "compReg", by: 1 },
      },
      say: ["COMP, PAGE {page}. INSTRUMENT {compIns}. RHYTHM {compRhythm}. VOICINGS {compVoicing}. REGISTER {compReg}.", { when: "lockComp", text: " LOCKED." }],
    },
    pCompMore: {
      title: "Comp: more",
      pages: ["pComp", "pCompMore"],
      bands: [
        { y: 0, h: 16, draw: [
          { when: "song", text: "{section}  {bar}/{bars}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "!song", text: "{idle}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "known", text: "{tempo} BPM", font: "T", right: 232, y: 2, color: "HI" },
        ] },
        { y: 18, h: 28, draw: [
          { rect: [2, 0, 2, 26], color: "COMP" },
          { icon: "comp", x: 10, y: 9, color: "COMP" },
          { text: "COMP", font: "B", x: 23, y: 4, color: "COMP" },
          { text: "{page}", font: "T", after: 8, y: 8, color: "GRAY" },
          { when: "lockComp", icon: "lock", x: 85, y: 9, color: "HI" },   // locked: the padlock by its name
          { when: ["song", "!pending"], text: "{chord}", font: "XS", right: 232, y: 5, color: "WHITE" },
          { when: "pending", text: "{pending}", font: "T", right: 232, y: 8, color: "HI" },
        ] },
        { y: 48, h: 144, draw: [
          { text: "K1  STRUM FILL", font: "T", x: 8, y: 3, color: "GRAY" },
          { text: "{compFill}", font: "B", x: 8, y: 14, color: "WHITE", arrows: true },
          { meter: [8, 33, 224, 2], value: "compFillFrac", color: "COMP", back: "LINE" },
          { text: "K2  BUSY LEAD THINS", font: "T", x: 8, y: 39, color: "GRAY" },
          { text: "{compThin}", font: "B", x: 8, y: 50, color: "WHITE", arrows: true },
          { meter: [8, 69, 224, 2], value: "compThinFrac", color: "COMP", back: "LINE" },
          { text: "K3  OVER A LOW LEAD", font: "T", x: 8, y: 75, color: "GRAY" },
          { text: "{compWith}", font: "B", x: 8, y: 86, color: "WHITE", arrows: true },
          { meter: [8, 105, 224, 2], value: "compWithFrac", color: "COMP", back: "LINE" },
        ] },
        { y: 200, h: 40, draw: [
          { rect: [8, 0, 224, 1], color: "LINE" },
          { text: "ALGO PART   SELECT PAGE   OCT REGISTER", font: "T", x: 8, y: 8, color: "DIM" },
          { text: "EDIT BACK   HOME MAIN   SEQ RE-ROLL   HOLD LOCK", font: "T", x: 8, y: 24, color: "DIM" },
        ] },
      ],
      knobs: [{ field: "compFill", step: 1 }, { field: "compThin", step: 1 }, { field: "compWith", step: 1 }],
      controls: {
        PRESET: { do: "turn", field: "compIns", step: 1 },   // browse its instrument (Sam, 2026-11-06)
        SEQ: { do: "reroll", part: "keys", long: { do: "lock", part: "keys" } },   // a press: this part re-rolls (its 🎲), on
        // the 4-bar boundary (Sam, 2026-11-06); held: locked, a new song keeps it (Sam, 2026-11-12; again: unlocked)
        ALGO: { do: "part" }, SELECT: { do: "page" }, EDIT: { go: "back" },
        "OCT-": { do: "nudge", field: "compReg", by: -1 }, "OCT+": { do: "nudge", field: "compReg", by: 1 },
      },
      say: ["COMP, PAGE {page}. STRUM FILL {compFill}. BUSY LEAD THINS {compThin}. OVER A LOW LEAD {compWith}.", { when: "lockComp", text: " LOCKED." }],
    },
    pPad: {
      title: "Pad",
      pages: ["pPad"],
      bands: [
        { y: 0, h: 16, draw: [
          { when: "song", text: "{section}  {bar}/{bars}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "!song", text: "{idle}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "known", text: "{tempo} BPM", font: "T", right: 232, y: 2, color: "HI" },
        ] },
        { y: 18, h: 28, draw: [
          { rect: [2, 0, 2, 26], color: "PAD" },
          { icon: "pad", x: 10, y: 9, color: "PAD" },
          { text: "PAD", font: "B", x: 23, y: 4, color: "PAD" },
          { text: "{page}", font: "T", after: 8, y: 8, color: "GRAY" },
          { when: "lockPad", icon: "lock", x: 55, y: 9, color: "HI" },   // locked: the padlock by its name
          { when: ["song", "!pending"], text: "{chord}", font: "XS", right: 232, y: 5, color: "WHITE" },
          { when: "pending", text: "{pending}", font: "T", right: 232, y: 8, color: "HI" },
        ] },
        { y: 48, h: 144, draw: [
          { text: "K1  INSTRUMENT", font: "T", x: 8, y: 3, color: "GRAY" },
          { text: "{padIns}", font: "B", x: 8, y: 14, color: "WHITE", arrows: true },
          { meter: [8, 33, 224, 2], value: "padInsFrac", color: "PAD", back: "LINE" },
          { text: "K2  ON/OFF", font: "T", x: 8, y: 39, color: "GRAY" },
          { text: "{padOnSet}", font: "B", x: 8, y: 50, color: "WHITE", arrows: true },
          { meter: [8, 69, 224, 2], value: "padOnSetFrac", color: "PAD", back: "LINE" },
          { text: "K3  PLACE", font: "T", x: 8, y: 75, color: "GRAY" },
          { text: "{padPlaceSet}", font: "B", x: 8, y: 86, color: "WHITE", arrows: true },
          { meter: [8, 105, 224, 2], value: "padPlaceSetFrac", color: "PAD", back: "LINE" },
          { text: "K4  NOTES", font: "T", x: 8, y: 111, color: "GRAY" },
          { text: "{padNotes}", font: "B", x: 8, y: 122, color: "WHITE", arrows: true },
          { meter: [8, 141, 224, 2], value: "padNotesFrac", color: "PAD", back: "LINE" },
        ] },
        { y: 200, h: 40, draw: [
          { rect: [8, 0, 224, 1], color: "LINE" },
          { text: "ALGO PART", font: "T", x: 8, y: 8, color: "DIM" },
          { text: "EDIT BACK   HOME MAIN   SEQ RE-ROLL   HOLD LOCK", font: "T", x: 8, y: 24, color: "DIM" },
        ] },
      ],
      knobs: [{ field: "padIns", step: 1 }, { field: "padOnSet", step: 1 }, { field: "padPlaceSet", step: 1 }, { field: "padNotes", step: 1 }],
      controls: {
        PRESET: { do: "turn", field: "padIns", step: 1 },   // browse its instrument (Sam, 2026-11-06)
        SEQ: { do: "reroll", part: "pad", long: { do: "lock", part: "pad" } },   // a press: this part re-rolls (its 🎲), on
        // the 4-bar boundary (Sam, 2026-11-06); held: locked, a new song keeps it (Sam, 2026-11-12; again: unlocked)
        ALGO: { do: "part" }, EDIT: { go: "back" },
      },
      say: ["PAD. INSTRUMENT {padIns}. ON/OFF {padOnSet}. PLACE {padPlaceSet}. NOTES {padNotes}.", { when: "lockPad", text: " LOCKED." }],
    },
    pLead: {
      title: "Lead",
      pages: ["pLead", "pLeadMore"],
      bands: [
        { y: 0, h: 16, draw: [
          { when: "song", text: "{section}  {bar}/{bars}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "!song", text: "{idle}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "known", text: "{tempo} BPM", font: "T", right: 232, y: 2, color: "HI" },
        ] },
        { y: 18, h: 28, draw: [
          { rect: [2, 0, 2, 26], color: "LEAD" },
          { icon: "lead", x: 10, y: 9, color: "LEAD" },
          { text: "LEAD", font: "B", x: 23, y: 4, color: "LEAD" },
          { text: "{page}", font: "T", after: 8, y: 8, color: "GRAY" },
          { when: "lockLead", icon: "lock", x: 84, y: 9, color: "HI" },   // locked: the padlock by its name
          { when: ["song", "!pending"], text: "{chord}", font: "XS", right: 232, y: 5, color: "WHITE" },
          { when: "pending", text: "{pending}", font: "T", right: 232, y: 8, color: "HI" },
        ] },
        { y: 48, h: 144, draw: [
          { text: "K1  INSTRUMENT", font: "T", x: 8, y: 3, color: "GRAY" },
          { text: "{leadIns}", font: "B", x: 8, y: 14, color: "WHITE", arrows: true },
          { meter: [8, 33, 224, 2], value: "leadInsFrac", color: "LEAD", back: "LINE" },
          { text: "K2  STYLE", font: "T", x: 8, y: 39, color: "GRAY" },
          { text: "{leadStyleSet}", font: "B", x: 8, y: 50, color: "WHITE", arrows: true },
          { meter: [8, 69, 224, 2], value: "leadStyleSetFrac", color: "LEAD", back: "LINE" },
          { text: "K3  PHRASE", font: "T", x: 8, y: 75, color: "GRAY" },
          { text: "{leadPhrase}", font: "B", x: 8, y: 86, color: "WHITE", arrows: true },
          { meter: [8, 105, 224, 2], value: "leadPhraseFrac", color: "LEAD", back: "LINE" },
          { text: "K4  REGISTER", font: "T", x: 8, y: 111, color: "GRAY" },
          { text: "{leadReg}", font: "B", x: 8, y: 122, color: "WHITE", arrows: true },
          { meter: [8, 141, 224, 2], value: "leadRegFrac", color: "LEAD", back: "LINE" },
        ] },
        { y: 200, h: 40, draw: [
          { rect: [8, 0, 224, 1], color: "LINE" },
          { text: "ALGO PART   SELECT PAGE   OCT REGISTER", font: "T", x: 8, y: 8, color: "DIM" },
          { text: "EDIT BACK   HOME MAIN   SEQ RE-ROLL   HOLD LOCK", font: "T", x: 8, y: 24, color: "DIM" },
        ] },
      ],
      knobs: [{ field: "leadIns", step: 1 }, { field: "leadStyleSet", step: 1 }, { field: "leadPhrase", step: 1 }, { field: "leadReg", step: 1 }],
      controls: {
        PRESET: { do: "turn", field: "leadIns", step: 1 },   // browse its instrument (Sam, 2026-11-06)
        SEQ: { do: "reroll", part: "lead", long: { do: "lock", part: "lead" } },   // a press: this part re-rolls (its 🎲), on
        // the 4-bar boundary (Sam, 2026-11-06); held: locked, a new song keeps it (Sam, 2026-11-12; again: unlocked)
        ALGO: { do: "part" }, SELECT: { do: "page" }, EDIT: { go: "back" },
        "OCT-": { do: "nudge", field: "leadReg", by: -1 }, "OCT+": { do: "nudge", field: "leadReg", by: 1 },
      },
      say: ["LEAD, PAGE {page}. INSTRUMENT {leadIns}. STYLE {leadStyleSet}. PHRASE {leadPhrase}. REGISTER {leadReg}.", { when: "lockLead", text: " LOCKED." }],
    },
    pLeadMore: {
      title: "Lead: more",
      pages: ["pLead", "pLeadMore"],
      bands: [
        { y: 0, h: 16, draw: [
          { when: "song", text: "{section}  {bar}/{bars}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "!song", text: "{idle}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "known", text: "{tempo} BPM", font: "T", right: 232, y: 2, color: "HI" },
        ] },
        { y: 18, h: 28, draw: [
          { rect: [2, 0, 2, 26], color: "LEAD" },
          { icon: "lead", x: 10, y: 9, color: "LEAD" },
          { text: "LEAD", font: "B", x: 23, y: 4, color: "LEAD" },
          { text: "{page}", font: "T", after: 8, y: 8, color: "GRAY" },
          { when: "lockLead", icon: "lock", x: 84, y: 9, color: "HI" },   // locked: the padlock by its name
          { when: ["song", "!pending"], text: "{chord}", font: "XS", right: 232, y: 5, color: "WHITE" },
          { when: "pending", text: "{pending}", font: "T", right: 232, y: 8, color: "HI" },
        ] },
        { y: 48, h: 144, draw: [
          { text: "K1  LOOP", font: "T", x: 8, y: 3, color: "GRAY" },
          { text: "{leadLoopSet}", font: "B", x: 8, y: 14, color: "WHITE", arrows: true },
          { meter: [8, 33, 224, 2], value: "leadLoopSetFrac", color: "LEAD", back: "LINE" },
        ] },
        { y: 200, h: 40, draw: [
          { rect: [8, 0, 224, 1], color: "LINE" },
          { text: "ALGO PART   SELECT PAGE   OCT REGISTER", font: "T", x: 8, y: 8, color: "DIM" },
          { text: "EDIT BACK   HOME MAIN   SEQ RE-ROLL   HOLD LOCK", font: "T", x: 8, y: 24, color: "DIM" },
        ] },
      ],
      knobs: [{ field: "leadLoopSet", step: 1 }],
      controls: {
        PRESET: { do: "turn", field: "leadIns", step: 1 },   // browse its instrument (Sam, 2026-11-06)
        SEQ: { do: "reroll", part: "lead", long: { do: "lock", part: "lead" } },   // a press: this part re-rolls (its 🎲), on
        // the 4-bar boundary (Sam, 2026-11-06); held: locked, a new song keeps it (Sam, 2026-11-12; again: unlocked)
        ALGO: { do: "part" }, SELECT: { do: "page" }, EDIT: { go: "back" },
        "OCT-": { do: "nudge", field: "leadReg", by: -1 }, "OCT+": { do: "nudge", field: "leadReg", by: 1 },
      },
      say: ["LEAD, PAGE {page}. LOOP {leadLoopSet}.", { when: "lockLead", text: " LOCKED." }],
    },
    pTexture: {
      title: "Texture",
      pages: ["pTexture"],
      bands: [
        { y: 0, h: 16, draw: [
          { when: "song", text: "{section}  {bar}/{bars}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "!song", text: "{idle}", font: "T", x: 8, y: 2, color: "GRAY" },
          { when: "known", text: "{tempo} BPM", font: "T", right: 232, y: 2, color: "HI" },
        ] },
        { y: 18, h: 28, draw: [
          { rect: [2, 0, 2, 26], color: "TEXTURE" },
          { icon: "texture", x: 10, y: 9, color: "TEXTURE" },
          { text: "TEXTURE", font: "B", x: 23, y: 4, color: "TEXTURE" },
          { text: "{page}", font: "T", after: 8, y: 8, color: "GRAY" },
          { when: "lockTexture", icon: "lock", x: 86, y: 9, color: "HI" },   // locked: the padlock by its name
          { when: ["song", "!pending"], text: "{chord}", font: "XS", right: 232, y: 5, color: "WHITE" },
          { when: "pending", text: "{pending}", font: "T", right: 232, y: 8, color: "HI" },
        ] },
        { y: 48, h: 144, draw: [
          { text: "K1  DUST", font: "T", x: 8, y: 3, color: "GRAY" },
          { text: "{texDust}", font: "B", x: 8, y: 14, color: "WHITE", arrows: true },
          { meter: [8, 33, 224, 2], value: "texDustFrac", color: "TEXTURE", back: "LINE" },
          { text: "K2  NOISE", font: "T", x: 8, y: 39, color: "GRAY" },
          { text: "{texNoise}", font: "B", x: 8, y: 50, color: "WHITE", arrows: true },
          { meter: [8, 69, 224, 2], value: "texNoiseFrac", color: "TEXTURE", back: "LINE" },
          { text: "K3  AMBIENCE", font: "T", x: 8, y: 75, color: "GRAY" },
          { text: "{texAmb}", font: "B", x: 8, y: 86, color: "WHITE", arrows: true },
          { meter: [8, 105, 224, 2], value: "texAmbFrac", color: "TEXTURE", back: "LINE" },
        ] },
        { y: 200, h: 40, draw: [
          { rect: [8, 0, 224, 1], color: "LINE" },
          { text: "ALGO PART", font: "T", x: 8, y: 8, color: "DIM" },
          { text: "EDIT BACK   HOME MAIN   HOLD SEQ LOCK", font: "T", x: 8, y: 24, color: "DIM" },
        ] },
      ],
      knobs: [{ field: "texDust", step: 1 }, { field: "texNoise", step: 1 }, { field: "texAmb", step: 1 }],
      controls: {
        // a press: nothing to re-roll here (rather than a whole new song by surprise); held: the texture locked
        SEQ: { do: "reroll", part: "texture", long: { do: "lock", part: "texture" } },
        ALGO: { do: "part" }, EDIT: { go: "back" },
      },
      say: ["TEXTURE. DUST {texDust}. NOISE {texNoise}. AMBIENCE {texAmb}.", { when: "lockTexture", text: " LOCKED." }],
    },
  },
};
