# Device Screens Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Put the workbench device mock's screens on the real FM-1 so that a change iterated in the mock moves to the device with a rebuild, not a rewrite.

**Architecture:** `workbench/ui/screens.js` stays the one source. A generator turns it into C tables. A small C runtime on the device interprets those tables with the same fixed vocabulary of draw items and actions that the mock's `device.js` implements. A manifest lists that vocabulary, and a parity check drives the mock and the host simulator through the same presses and compares their screens pixel for pixel. Choice data (names, statuses and fit rules) moves out of `rows.js` into shared data that both sides read.

**Tech Stack:**
- C99: the firmware and the host simulator (`./build.sh --host`), on fm1-base's gfx (`cv_begin`, `cv_rect`, `cv_text`, `cv_blit`, `text_w`) and plat (`plat_buttons`, `plat_keys`, `plat_enc`, `plat_leds`).
- Node with no npm dependencies: the exporter and the parity check.
- Python 3 (`python3 -I`): the C generators, in the manner of `tools/patterns/import.py`.

**Spec:** the Design section below. It records Sam's decisions of 2026-11-06 from the screens work: the B3 main screen, the part-screen template, Sections, Parts, Mixer, Settings, the key behavior, and the four portability rules.

## Global Constraints

**Budgets**
- Firmware image ≤ 409,600 B. Now 387,816 B, so about 21.7 KB of headroom. Report the size after each task that touches firmware.
- RAM ≤ 98,304 B. Now about 62 KB. Report each change.
- No audio block may be late with any screen up. Measure the worst redraw time per screen, in µs, with the docs/cost.md method.

**Fidelity**
- The parity check must match 100% of pixels on every compared state. `workbench/tools/screen_check.mjs` (today's screen) must keep passing until Task 8 retires it.

**Repository rules**
- Never modify, stage or commit anything in `base/` (Sam's uncommitted work). The base symlink blocks commits in worktrees: `rm base; mkdir base`, commit, then `rmdir base; ln -s …/base base`.
- Generated files (`*_gen.c`, `*_gen.h`, `*.json` exports) are committed. A freshness check fails the suite when one is stale.
- Commits use `git -c user.email=884872+pianomansam@users.noreply.github.com commit` with the Co-Authored-By and Claude-Session trailers. Never push.
- License GPL-3.0-or-later. US English. No artist names.
- `docs/cost.md` and the ledger have another session's uncommitted edits in the main checkout. Edit them only through a worktree branch merged with the stash dance.

**Checks that must pass after every task**
- `./build.sh --host`
- `sh tests/run_tests.sh`
- `sh workbench/build.sh && node workbench/tools/smoke.mjs`
- `./build.sh`

## Review Focus

1. **A string longer than its band** (a long instrument plus rhythm, "AS ROLLED (HELD ON 2 AND 4)"). The device must clip at the band exactly as the mock does, never draw outside it, and never overflow a buffer. Task 6 owns the test: the longest choice name of every row on every screen, with no overflow.
2. **A stale or missing choice** (an instrument retired since a song was rolled, an unknown override value). The device shows the value as its number, as the mock does, and doesn't crash or show garbage. Task 5 owns the test.
3. **A knob turned while stopped, between songs, or past the song's last 4-bar boundary.** Nothing waits, and the header still says the value, matching today's device. Task 7 owns the test.
4. **Fast input:** a spun encoder, two keys at once, a long press while turning a knob. Detents add up, and each key's long press is judged on its own. Task 7 owns the test.
5. **A screen whose data names something the device lacks** (a new draw item or action added only in the mock). The manifest check fails at build time and names it. The device never shows a blank or broken screen. Task 1 owns the test.

---

## Design

**Screens (as built in the mock, workbench/ui/screens.js):**
- the main screen (B3);
- the part screens (Song, Drums, Bass, Comp, Pad, Lead, Texture, with 2 pages where they have them);
- Sections (SEL);
- Parts, the piano roll (ARP);
- Mixer (FX);
- Settings, a placeholder (GLO).

**Conventions:**
- **Moving around:** HOME goes to the main screen. ALGO picks the part or highlight. SELECT steps the pages, or moves the highlight where a screen has no pages. EDIT opens and goes back. SEQ re-rolls: on the main screen a new song, on a part's screen that part. PRESET browses a part's instrument (or kit).
- **Changes:** a change waits for the 4-bar boundary, shown at once as `*VALUE` in the pending color with "… IN n BARS". A new song returns to the main screen.
- **Keys:** the five lowest white keys mute a part (press) or solo it (long press, 500 ms) on every screen except Sections, where the white keys jump to a section. A key's light shows its part playing (lit), muted (dark) or soloed (blinking). On Sections, the playing section's key is lit.

**Portability rules:**
1. **One source.** screens.js is the only place screens are defined. The firmware's tables are generated from it.
2. **One vocabulary, implemented twice.** It covers:
   - draw items: text, rect, icon, meter, knobs, sections, lanes, faders;
   - actions: go, play, nudge, turn, cursor, page, part, jump, reroll, mute, solo, level;
   - values: the names valuesOf produces.
   
   A manifest lists it. Both the mock and the generator refuse anything outside it.
3. **Parity check.** Scripted presses go through the mock and the simulator, and every screen is compared pixel for pixel.
4. **Shared choice data.** Row definitions, choice names, statuses and fit rules live in data that both rows.js and the device read.

**Settings:**
- A settings store on the device (flash, written only on change, safe against power loss).
- Settings as fields the interpreter shows and edits.
- A draw item may read a setting instead of a constant. Example: the roll's `live` reads the "movement" setting.
- Rule toggles get one engine flag each.

**Phases:**
1. The runtime with the main and part screens (this plan, Tasks 1–8).
2. Sections, with jump and loop on the device.
3. Mixer, Settings with the store, keys and lights.
4. The Parts roll.

Phases 2–4 are outlined at the end and get their own plans.

## File Structure

| File | Responsibility |
|---|---|
| `workbench/ui/vocab.json` (new) | The manifest: draw item kinds and their keys, action kinds and their keys, value names (fixed and patterned, e.g. `sel{0-9}`, `{field}Pend`). |
| `workbench/tools/export_screens.mjs` (new) | Evaluates screens.js and writes `workbench/ui/screens.json` (plain data). Fails on anything vocab.json lacks. |
| `workbench/rows.json` (new) | The row definitions (today's `ROWS` in rows.js) and the fit rules (`compFits`'s keys/notKeys, the rhythm-row rule), as data. rows.js imports it. |
| `tools/screens/gen.py` (new) | Reads screens.json, rows.json, options.json and vocab.json and writes `app/ui/screens_gen.c` and `.h` (screens, bands, items, strings, icons, colors, fields, controls, keys) and `app/ui/choices_gen.c` and `.h` (choice names a row, statuses, fits). `--check` reports staleness. |
| `app/song/rolled.c` and `.h` (new) | `song_rolled()`: what the "as rolled" overrides play (moved out of `wb_rolled` so the device and the workbench share it). |
| `app/song/song.c` (modify) | Requests on overrides and roles that land at the 4-bar boundary, like today's roll-field requests (`song_request`). |
| `app/ui/values.c` and `.h` (new) | The value bag from the main loop's snapshot: the C twin of `valuesOf()`. |
| `app/ui/render.c` (new) | The draw items: the C twin of `drawScreen()`. Runs in the unity build so it reaches gfx.c's static `cv_*`. |
| `app/ui/nav.c` (new) | The screen stack, cursor, pages, actions, requests, pending marks, key long press, key lights: the C twin of the `Device` class. |
| `app/app.c` (modify) | `input()`, `draw()` and `speak()` call the runtime. Today's hand-drawn screen and knob tables go. |
| `workbench/tools/parity.mjs` (new) | The parity check: one script of presses, run on the simulator (frames via `fbdump`, state via a new `uistate` command) and on the mock, compared pixel for pixel per step. Replaces screen_check.mjs in Task 8. |

---

### Task 1: The vocabulary manifest and the screens export

**Files:**
- Create: `workbench/ui/vocab.json`, `workbench/tools/export_screens.mjs`, `workbench/ui/screens.json` (generated)
- Modify: `workbench/ui/device.js` (`problems()` reads vocab.json), `workbench/tools/smoke.mjs`, `tests/run_tests.sh`

**Interfaces:**
- Produces:
  - `vocab.json`: `{ items: { text: [keys…], rect: […], icon: […], meter: […], knobs: […], sections: […], lanes: […], faders: […] }, actions: { go: ["go"], play: [], nudge: ["field","by"], turn: ["field","step"], cursor: [], page: [], part: [], jump: ["i"], reroll: ["part"], mute: ["part","long"], solo: ["part"], level: ["part"] }, values: { fixed: […], patterns: ["sel{0-9}","sec{0-9}","sec{0-9}Bars","sec{0-9}Now","sec{0-9}Pend","{field}","{field}Frac","{field}Pend","{field}First","{field}Last","{row}","{row}Pend"] } }`
  - `node workbench/tools/export_screens.mjs [--check]` writes or compares `workbench/ui/screens.json`. It exits 1 naming each item, action or value outside vocab.json.

- [ ] **Step 1: Write the failing test** (smoke). `vocab: every item, action and value screens.js uses is in vocab.json`. Also: a copy of the data with an item `{ spark: 1 }` is reported as `main.bands[0]: no draw item spark`, and one with `{ do: "warp" }` as `… unknown action warp`.
- [ ] **Step 2: Run it.** `node workbench/tools/smoke.mjs`. Expect: FAIL, vocab.json missing.
- [ ] **Step 3: Write vocab.json from what device.js implements today.** Make `problems()` check items, actions and template values against it instead of its built-in lists. Write `export_screens.mjs`. It imports screens.js, runs `problems()`, writes sorted-key JSON, and with `--check` exits 1 if the file differs.
- [ ] **Step 4: Run.** smoke passes. `node workbench/tools/export_screens.mjs --check` exits 0. Add `run screens-export node workbench/tools/export_screens.mjs --check` to `tests/run_tests.sh`.
- [ ] **Step 5: Commit.** "Screens: the vocabulary manifest and screens.json, checked against it"

### Task 2: Shared choice data (rows.json)

**Files:**
- Create: `workbench/rows.json`
- Modify: `workbench/rows.js` (ROWS, SECTIONS and the fit rules come from rows.json), `workbench/tools/smoke.mjs`

**Interfaces:**
- Produces:
  - `rows.json`: `{ rows: [ { id, label, short?, kind, field?, role?, key?, source, regenerates?, part? } … ], sections: […], fits: { comping: { keysOf: "keys", notKeysOf: "notKeys" }, rhythmRow: { strumSym: "CS_STRUM" } } }`. `rows.js` exports the same `ROWS` and `SECTIONS` as today.
  - The fit rules are data the C side can evaluate: a comp option fits instrument `i` iff (`!keys || keys ∋ i`) and `!(notKeys ∋ i)`.

- [ ] **Step 1: Write the failing test** (smoke). `rows.json: ROWS and SECTIONS deep-equal the values before the move (a snapshot taken now and committed as a test fixture), and compFits / rhythmRow give the same answers for every instrument × comping pair`.
- [ ] **Step 2: Run it.** Expect: FAIL, rows.json missing.
- [ ] **Step 3: Move ROWS, SECTIONS and the fit parameters into rows.json.** rows.js imports them (`import rows from "./rows.json" with { type: "json" }`). If the page's module loading can't take that, export a JS module generated alongside instead, and say which you chose. Add rows.json to the copy list in `workbench/build.sh`.
- [ ] **Step 4: Run** smoke and a real-browser load. Expect: PASS, and the page loads with no console errors.
- [ ] **Step 5: Commit.** "Rows: the row definitions and fit rules as data (rows.json), shared with the device"

### Task 3: The generator (screens and choices to C)

**Files:**
- Create: `tools/screens/gen.py`, `tools/screens/test_gen.py`, `app/ui/screens_gen.c`, `app/ui/screens_gen.h`, `app/ui/choices_gen.c`, `app/ui/choices_gen.h`
- Modify: `tests/run_tests.sh` (`run screens-gen python3 -I tools/screens/gen.py --check`)

**Interfaces:**
- Consumes: screens.json (Task 1), rows.json (Task 2), workbench/options.json, vocab.json.
- Produces:
  - `screens_gen.h`:
    - `enum ui_item_kind { UI_TEXT, UI_RECT, UI_ICON, UI_METER, UI_KNOBS, UI_SECTIONS, UI_LANES, UI_FADERS }` and `enum ui_action { UA_NONE, UA_GO, UA_PLAY, UA_NUDGE, UA_TURN, UA_CURSOR, UA_PAGE, UA_PART, UA_JUMP, UA_REROLL, UA_MUTE, UA_SOLO, UA_LEVEL }`, both generated from vocab.json, in its order.
    - `typedef struct { … } ui_item_t;`: kind, x, y, w, h, font (FONT_* index), color (palette index), right/after, a template string id, `when` as up to 3 condition ids with negation, and per-kind fields.
    - `ui_band_t { y, h, bg, first_item, n_items }`.
    - `ui_screen_t { title, bands, n_bands, knobs[4], controls (one action a control: 14 buttons, 3 encoders, 4 knobs, 27 keys), menu, list, lanes, pages, key_leds, say }`.
    - `extern const ui_screen_t UI_SCREENS[]; extern const int UI_N_SCREENS, UI_HOME;`
    - `UI_COLORS[]` (RGB565, theme plus data colors), `UI_ICONS[]` (1-bit rows), `UI_FIELDS[]` ({ row, name, kind: roll|options|variant, min, max, wrap, step, format, now }).
    - Value ids as an enum: `UV_*` for fixed values, plus helpers for the patterned ones.
  - `choices_gen.h`: `const char *ch_name(int row, int value)` (NULL when unknown); `int ch_count(int row)`; `int ch_value(int row, int index)`; `int ch_fits(int row, int value, int keys_ins)`; `int ch_is_retired(int row, int value)`. Rows are numbered by rows.json's order (`ROW_*` enum).
  - `python3 -I tools/screens/gen.py [--check]` writes or verifies all four files.

- [ ] **Step 1: Write the failing tests** (`tools/screens/test_gen.py`, run with `.venv/bin/python -I`):
  - `test_round_trip_main`: the generated main screen's band count, item kinds and template strings equal screens.json's.
  - `test_unknown_kind_refused`: a fixture with item `{ spark: 1 }` exits 1 naming it.
  - `test_choice_names`: `ch_name(ROW_KIT, 6)` matches options.json's kit 6 name upper-cased with its `short` when present. Exercised through a tiny C harness compiled in the test, as `tools/patterns/test_patterns.py` does.
  - `test_check_stale`: editing screens.json and running `--check` exits 1.
- [ ] **Step 2: Run them.** Expect: FAIL, gen.py missing.
- [ ] **Step 3: Implement gen.py.**
  - Strings are interned into one table.
  - Templates stay templates; the runtime fills them.
  - Colors convert with the mock's `RGB()`.
  - Icons pack 9-bit rows into `uint16_t`.
  - Report the generated tables' flash size and keep them `const`.
- [ ] **Step 4: Run** the tests and `gen.py --check`. Expect: PASS.
- [ ] **Step 5: Commit.** "Screens: the generator (screens.json, rows.json, options.json to C tables)"

### Task 4: The engine: requests on overrides and roles; song_rolled()

**Files:**
- Create: `app/song/rolled.c`, `app/song/rolled.h`
- Modify: `app/song/song.c`, `app/song/song.h`, `workbench/engine/wb.c` (`wb_rolled` calls `song_rolled`), `tests/host/song_test.c`

**Interfaces:**
- Produces:
  - `typedef struct { int comp, comp_pat, lead_style, pad_on, pad_place, strum_pat, strum_fill, lead_loop, bass_style, ride, keys_high, lead_lo, lead_hi; } song_rolled_t;` and `void song_rolled(const song_roll_t *r, song_rolled_t *out);`. Same values and meanings as today's `wb_rolled` JSON.
  - `void song_request_row(song_t *s, int row, int value);`: a request on any row the screens set (a role, an override, a salt, the kit, the pattern), landing on the next 4-bar boundary exactly like `song_request` for roll fields, and carried across a stop like them (`song_carry_requests`).

- [ ] **Step 1: Write the failing tests** (song_test.c):
  - `test_rolled_matches_wb`: for seeds 21, 72, 140 and 300, `song_rolled` equals the values `wb_rolled` reported before the move. Pin them from a run of the current code first.
  - `test_request_row_lands`: a comp instrument request at bar 2 changes nothing until bar 4, and from bar 4 plays the new instrument. Likewise a `keys_oct` +1 and a `bass_salt` re-roll.
- [ ] **Step 2: Run.** Expect: FAIL, undefined symbols.
- [ ] **Step 3: Implement.**
  - Move `wb_rolled`'s body into `song_rolled`.
  - `song_request_row` stores row and value pairs next to `req_set`/`req_val` and applies them at the boundary through the same paths the workbench uses: roles, `song_override`, and the kit/pattern setters.
- [ ] **Step 4: Run** all four checks. Expect: PASS, with no song hash moving.
- [ ] **Step 5: Commit.** "Song: requests on any row land on the 4-bar boundary; song_rolled shared with the workbench"

### Task 5: The device's value bag (values.c)

**Files:**
- Create: `app/ui/values.c`, `app/ui/values.h`
- Modify: `app/app.c` (`snap_t` carries what values need: rows' values, rolled, sections, pending), `tests/host/ui_test.c` (new, built by `tests/run_tests.sh` like `song_test`)

**Interfaces:**
- Consumes: `snap_t` (app.c), `song_rolled` (Task 4), `ch_*` (Task 3), `UI_FIELDS` (Task 3).
- Produces: `app/ui/nav.h` with the navigator's state type only, `ui_nav_t` (its fields as Task 7 lists them; Task 7 adds the functions), so the bag can read the stack, the cursor and the waiting requests; `typedef struct ui_bag ui_bag_t;`, `void ui_values(const snap_t *s, const ui_nav_t *nav, ui_bag_t *b);`, `const char *ui_value(const ui_bag_t *b, int value_id, int index, char *tmp);` (text; empty when unset), `int ui_true(const ui_bag_t *b, int cond_id);`. The rules are those of `device.js` `valuesOf()`:
  - `*` and the pending marks;
  - "AS ROLLED (X)" and ORIGINAL;
  - the First/Last arrows;
  - known/ready/song/idle/msg;
  - `progress`;
  - sections and `sel{i}`.

- [ ] **Step 1: Write the failing tests** (ui_test.c, against values the mock computes for the same state):
  - `test_values_main`: a fixed snapshot (seed 72, bar 13, key 8, tempo 84, kit 5, keys 0, comp as rolled → Charleston) gives `{kit}` = "CRUNCHY MACHINE", `{rhythm}` = "CHARLESTON", `{keyline}` as in the mock.
  - `test_values_pending`: a tempo request to 86 gives `tempo` = "*86" and `tempoPend` true, and `pending` = "TEMPO 86 IN 3 BARS".
  - `test_values_unknown_choice` (Review Focus 2): a kit value with no name gives its number.
  - The expected strings come from a fixture written by a small Node script from the mock's `valuesOf` for the same snapshot (`tests/host/ui_values_fixture.json`, generated by `workbench/tools/ui_fixture.mjs`, checked for freshness).
- [ ] **Step 2: Run.** Expect: FAIL.
- [ ] **Step 3: Implement `ui_values`/`ui_value`.** No heap. Fixed buffers sized from the longest generated string plus the longest choice name. Templates are filled by `ui_render` (Task 6), not stored.
- [ ] **Step 4: Run** all four checks. Expect: PASS. Report the RAM `ui_bag_t` takes.
- [ ] **Step 5: Commit.** "UI: the device's value bag, the mock's valuesOf in C"

### Task 6: The renderer (render.c) and the pixel parity of a still screen

**Files:**
- Create: `app/ui/render.c`, `app/ui/render.h`, `workbench/tools/parity.mjs`
- Modify: the simulator's script commands (app.c's host commands: add `uiscreen NAME` to show a screen and `uistate` to print the bag and the stack as JSON), `tests/run_tests.sh` (`run parity node workbench/tools/parity.mjs`)

**Interfaces:**
- Consumes: `UI_SCREENS` (Task 3), `ui_bag_t` (Task 5), gfx.c's `cv_begin`/`cv_rect`/`cv_text`/`cv_blit`/`text_w`.
- Produces: `void ui_draw(const ui_screen_t *sc, const ui_bag_t *b, uint32_t band_mask);` (draws the bands in the mask: all on a screen change, else those whose values changed); `parity.mjs` (steps: `{ press | turn | key | wait | screen }`; at each step it compares `fbdump` with the mock drawn from the same `uistate` bag).

- [ ] **Step 1: Write the failing parity cases:**
  - every screen in screens.json shown with `uiscreen` at seed 72, bar 13, playing, plus stopped and between-songs, compared with the mock: 100% of pixels;
  - `longest_names` (Review Focus 1): each row set to its longest choice name on its part screen, compared with the mock, with no write outside a band (the simulator's framebuffer checked outside each band's rows).
- [ ] **Step 2: Run** `node workbench/tools/parity.mjs`. Expect: FAIL, uiscreen unknown.
- [ ] **Step 3: Implement `ui_draw`.**
  - One function per draw item kind, mirroring `drawScreen` (text with right/after, the pending color for a lone waiting value, arrows).
  - Phase 1 must draw the kinds the main and part screens use: text, rect, icon, meter, knobs, sections. The others (lanes, faders) draw a "NOT YET" text in the band, and the parity check skips those screens until their phase.
- [ ] **Step 4: Run** all checks. Expect: PASS. Report the worst full-screen draw in µs and the image size.
- [ ] **Step 5: Commit.** "UI: the renderer, pixel for pixel with the mock on every phase-1 screen"

### Task 7: The navigator (nav.c): moving, changing, keys

**Files:**
- Create: `app/ui/nav.c`, `app/ui/nav.h`
- Modify: `workbench/tools/parity.mjs` (scripted presses), `tests/host/ui_test.c`

**Interfaces:**
- Consumes: `UI_SCREENS`, `UI_FIELDS` (Task 3), `song_request_row` and `song_request` (Task 4), `ui_values` (Task 5).
- Produces:
  - `typedef struct { uint8_t stack[8], depth; int8_t cursor[UI_MAX_SCREENS]; … held keys, the jump and roll requests … } ui_nav_t;`
  - `void ui_press(ui_nav_t *n, int control, int detents);`
  - `void ui_key(ui_nav_t *n, int key, int down, uint32_t ms);`
  - `void ui_tick(ui_nav_t *n, uint32_t ms);` (long press at 500 ms)
  - `void ui_new_song(ui_nav_t *n);` (back to the main screen)
  - `uint32_t ui_key_lights(const ui_nav_t *n, const snap_t *s, uint32_t *blink);`
  - The mock's `Device` semantics exactly: go/back/home, cursor wrap, page and part, turn and nudge as requests on the 4-bar boundary (dropped past the song's end), PRESET as a turn, SEQ reroll (a song on the main screen, a part's salt on its screen, waiting for the boundary), the `say` text.

- [ ] **Step 1: Write the failing parity scripts:**
  - `nav_main_to_comp`: ALGO 3, EDIT, K1 +1, OCT+, SELECT, ALGO 1, HOME. Compare at every step.
  - `nav_pending`: K2 +2 at bar 2, then waits to bar 4 and bar 5.
  - `nav_seq_part`: SEQ on Bass.
  - `nav_preset`: PRESET +1 on Drums.
  - `nav_new_song`: on Comp, the song ends, and the main screen shows.
  - `nav_stopped` (Review Focus 3): K2 while stopped and past the last boundary. Nothing waits; the header says it.
  - `nav_fast` (Review Focus 4): SELECT +12 in one turn; keys F and C down together; C held 600 ms while K1 turns.
  - ui_test.c covers `ui_tick`'s long press and `ui_key_lights` (bass muted → keys 0,4,6,7 lit; lead soloed → key 7 blinking).
- [ ] **Step 2: Run.** Expect: FAIL.
- [ ] **Step 3: Implement nav.c** to the mock's `Device` (device.js `press`, `keyDown`/`keyUp`/`tick`, `request`, `update`, `keyLights`). Where the mock calls `engine.*`, call the song requests and the existing mute/solo paths.
- [ ] **Step 4: Run** all checks. Expect: PASS.
- [ ] **Step 5: Commit.** "UI: the navigator, the mock's Device in C; parity over scripted presses"

### Task 8: The device runs the screens

**Files:**
- Modify: `app/app.c` (`input()` → `ui_press`/`ui_key`/`ui_tick`; `draw()` → `ui_values` + `ui_draw`; `speak()` → the screen's `say`; `plat_leds(btn, keys)` from `ui_key_lights` outside the browser remote; the old `KNOB_*` tables, the `line_*` helpers and `value_text` removed), `tests/scenarios/*.vnl` (the new screens' words), `workbench/README.md`, `app/README.md` or `docs/` notes
- Delete: `workbench/tools/screen_check.mjs`, `workbench/tools/today_screen.mjs` (parity.mjs covers the device now)

**Interfaces:**
- Consumes: everything above.

- [ ] **Step 1: Write the failing scenario.** `tests/scenarios/screens.vnl`: boot shows the main screen stopped with the ready song's key line; PLAY; ALGO 3; EDIT; the screen reader says "COMP, PAGE 1/2 …"; a key G press mutes the bass and its light goes dark; HOME.
- [ ] **Step 2: Run.** Expect: FAIL.
- [ ] **Step 3: Wire the runtime into app.c.**
  - The browser remote keeps working: in remote mode the device forwards and doesn't act, as today.
  - Measure:
    - the worst redraw per screen (µs);
    - the image size;
    - RAM;
    - one play-through of seeds 21, 72 and 140 with every screen visited, with no late audio block. Use the simulator's timing; on the device, the cost-ledger method.
  - Write the numbers into the commit message. Sam adds them to docs/cost.md.
- [ ] **Step 4: Run** all checks, then flash the FM-1 (Sam plugs it in) and check by eye on the device: the main screen, a part screen, a waiting change, key mute and solo with the lights.
- [ ] **Step 5: Commit.** "Device: the screens from the workbench, run by the UI runtime (phase 1)"

---

## Live state for Claude (Sam, 2026-11-06: "I will be asking you live questions")

Claude must be able to read what Sam is hearing and seeing, in the workbench or on a connected FM-1, without Sam
describing it. Build it alongside the phases; it shares their pieces.

- **One state shape for both:** `{ source: "workbench" | "device", at, seed, playing, bar, bars, section, chord,
  screen (id and title), stack, cursor, values (the screen's value bag: valuesOf / ui_values), rows (each row's value
  and its name, "as rolled" resolved), levels, mute, solo, loop, stay, pending (waiting changes, jump, re-roll) }`.
  The mock's `Device.values()` and the device's `ui_values` (Task 5) already produce most of it; one function each
  serializes it (`stateOf()` in device.js, `ui_state_json()` in app/ui).
- **The workbench:** the page posts its state to the local server about once a second while open (`POST /state`,
  JSON, under the server's existing Host/Origin/content-type checks; 127.0.0.1 only), and the server keeps the latest
  in `app/build/live/workbench.json` (git-ignored). Claude reads that file. No new listening ports, nothing leaves the
  machine.
- **The device:** a SysEx request on the browser-remote prefix (F0 7D 56 4E 04: "state") answered with the state as
  a chunked SysEx stream (7-bit text), and `base/tools/fm1_remote.py`-style tool of our own, `tools/fm1_state.py`
  (mido, read-only), that asks and prints the JSON. The simulator's `uistate` command (Task 6) is the same function,
  so the parity check covers it. While the browser remote is connected, the page can relay the device's answer to
  the server too, so one file holds whichever is live.
- **When:** the workbench half any time (small: a timer, an endpoint, a file); the device half after Task 8 (it needs
  the runtime's bag on the device). Tests: smoke checks the posted shape against `Device.values()`; a host scenario
  checks the device's answer against `uistate`.

## Later phases (their own plans)

- **Phase 2, Sections on the device:**
  - the engine jumps to a section and repeats one (`song_jump(bar)` and a loop flag, as the workbench's wb_jump and wb_advance do);
  - the Sections screen's drawing (already generated);
  - the white keys jump on Sections, waiting for the 4-bar boundary;
  - LOOP shown in the status bar.
- **Phase 3, Mixer, Settings, keys:**
  - the faders draw item;
  - `vx_level_set` from the Mixer;
  - a settings store in flash (two sectors, CRC, header last, written only when changed and only while audio is silent or at most once a minute);
  - settings as fields;
  - "movement" (the roll's `live`) as the first;
  - the Settings screen made real;
  - rule toggles, one engine flag each, as the Advanced inventory decides.
- **Phase 4, the Parts roll on the device:**
  - a design pass first. The device can't hold a whole song's notes, so choose between the bars played so far plus the sounding notes, and a short look-ahead computed off the audio path;
  - then the lanes draw item.
