# VNL-1 Workbench Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A local web page that plays real VNL-1 songs, lets Sam swap or re-roll any song-level decision and edit a
drum kit and two-bar pattern, and records feedback Claude can replay exactly.

**Architecture:** The song engine (app/song) and voices (app/engine) compile with Emscripten into `wb.wasm`, run in
an AudioWorklet like the existing emulator (base/web/emu). Small, default-off hooks in the engine let the workbench
supply a custom set, a custom two-bar pattern, per-song overrides and a drum source. Every change rebuilds the song
and runs it silently up to the next bar line ("catch-up"), so state is exactly as if the song always had that
decision. A Python standard-library server serves the page and writes feedback and saved kits/patterns to files.

**Tech Stack:** C99 (the engine; host cc for tests, emcc for wasm), plain HTML/CSS/JS (no framework, no bundler),
Node 22 for headless smoke tests and replay, Python 3 standard library for the server.

**Spec:** docs/superpowers/specs/2026-10-28-workbench-design.md

## Global Constraints

- Local only; not the FM-1; the workbench may exceed the device's CPU, flash and RAM budgets.
- Engine hooks are default-off: with no hook set, the firmware and every existing test behave exactly as before.
- Changes land at the next bar (catch-up); seed and arrangement-map changes restart from bar 1.
- 🎲 changes only its row; a row forced to change re-derives and is highlighted; locked rows never move; a roll that
  conflicts with a lock is blocked and says why.
- Dropdowns list current, approved options only; "show retired" (off by default) adds retired ones, marked.
- Removed sounds are never offered by default: upright bass, X0X 808, poketrack 909, Optimist BOOMBAP and vintage,
  our synth 808/Dusty/Soft, the mixed sampled kits (Warm, Dark, Rusty, Gentle, Clean), the arpeggio comping.
- Families: acoustic (Knight, Emilyguitar, nylon, felt, flute, sax, trumpet, strings, cello, kalimba, music box;
  drums: shipped sampled kits, Brushes), synth (DX, organ, saw lead, synth pluck, sub, FM bass, chip bass, warm pad,
  bells, string lead; drums: poketrack 808, CR-78, Optimist LO-FI), electric goes with either (Rhodes, Wurli,
  electric bass, vibes, guitar lick). Supporting roles (bass, pad, ear candy) and drums follow the family.
- Files: `workbench/` (page, server, options, tools, engine glue, kits, patterns, feedback); wasm build output
  `app/build/workbench/`. Feedback `workbench/feedback.jsonl`; saved kits `workbench/kits/*.json`; patterns
  `workbench/patterns/*.json`; options `workbench/options.json`.
- Licenses: VNL-1 files `SPDX-License-Identifier: GPL-3.0-or-later`; copied poketrack files keep zlib; copied
  Optimist files keep `GPL-3.0-only` and their copyright line; samples CC0. Credits in CREDITS.md. Workbench-only
  code never goes under `app/`.
- US English. Commits end with the session's Co-Authored-By and Claude-Session lines, use the noreply address, and
  are never pushed or merged without Sam asking.
- Never delete anything under app/build/new except folders the task created.
- Tests: `PATH=$PWD/.venv/bin:$PATH sh tests/run_tests.sh` must stay green (-Werror); workbench smoke tests run with
  `node workbench/tools/smoke.mjs`.

## Review Focus

1. A change made in the song's last bar (catch-up target past the end): the song restarts from bar 1 with the
   change, no crash or silence. Test in Task 5.
2. Many changes inside one bar (dice clicked repeatedly): only the latest state applies at the bar line, audio has
   no gap longer than the catch-up, nothing piles up. Test in Task 5.
3. A dice or family change that conflicts with a locked row: nothing changes and the page names the lock. Test in
   Task 6.
4. A sampled kit whose files are missing: the kit shows "unavailable" in the picker and can't be chosen; no silent
   lane. Test in Task 9.
5. Feedback sent while the server is down: the entry stays queued in the page (localStorage) and is retried; the
   page says it isn't saved yet. Test in Task 7.

---

### Task 1: A green baseline

The suite hasn't run since several Sam-approved changes (no guitar or keys arpeggio, 8th sampled hats at light
swing, the vibes on the phrase planner, the 1.5 s ride). Pinned hashes and checks that encode the old behavior fail.

**Files:**
- Modify: `tests/host/song_test.c` (only checks that pinned the old behavior)

- [ ] **Step 1:** Run `PATH=$PWD/.venv/bin:$PATH sh tests/run_tests.sh`; list every failure from
  `app/build/tests/*.log`.
- [ ] **Step 2:** For each failure, decide from git log (commits since 4c80468's ancestors: cf0dca0, 7773f9d,
  a162c90, 3025034) whether it pins behavior Sam changed on purpose. Re-pin those (new hash or bound, with a comment
  naming the commit and Sam's reason). Anything else is a regression: fix the code, not the test.
- [ ] **Step 3:** Run the suite. Expected: `all tests passed`. Run `sh tools/cost-bench.sh --forced 142 16` and
  `sh tools/mem-check.sh`; record numbers in docs/cost.md if they moved.
- [ ] **Step 4:** Commit: `Tests: re-pin the checks Sam's recent changes moved (arpeggio off, 8th hats, vibes
  phrase, ride)`.

### Task 2: Engine hooks — custom set and two-bar pattern

**Files:**
- Modify: `app/song/library.h`, `app/song/library.c`, `app/song/groove.c`, `app/song/harmony.c`, `app/song/lead.c`
- Test: `tests/host/song_test.c`

**Interfaces:**
- Produces:
  - `enum { LIB_SET_WB = 255, LIB_PATTERN_WB = 255 };`
  - `void lib_set_wb(const lib_set_t *s);` — NULL clears; `lib_set_by_id(LIB_SET_WB)` returns it.
  - `void lib_pattern_wb(const lib_pattern_t *p);` — same for `lib_pattern_by_id(LIB_PATTERN_WB)`.
  - `lib_pattern_t` gains trailing fields `uint8_t two_bar; uint16_t hit_b[6]; uint16_t ghost_b[6];` (existing
    initializers stay valid; zero = one-bar).
  - `unsigned lib_lane_hits(const lib_pattern_t *p, int lane, int bar);` and `lib_lane_ghosts(...)` — bar B's masks
    on odd bars when `two_bar`, else bar A's. Every read of `p->hit[...]`/`p->ghost[...]` in groove.c, harmony.c and
    lead.c goes through them (stab placement and the lead's snare check use bar 0).

- [ ] **Step 1: Failing tests** in song_test.c:
  `test_wb_set`: a song rolled, then `r.v[RF_SET] = LIB_SET_WB` with a custom set copying set 21 but `lead =
  INS_SAX`; `lib_set_by_id(LIB_SET_WB)->lead == INS_SAX`; the song plays to the end with V_LEAD events; after
  `lib_set_wb(NULL)`, `lib_set_by_id(LIB_SET_WB) == NULL`.
  `test_wb_two_bar`: a custom pattern, `two_bar = 1`, kick bar A `0x0401` (steps 0, 10), bar B `0x0481` (0, 7, 10);
  over a whole song, every bar with the kick layer has kicks exactly at those steps (A on even bars, B on odd);
  with `two_bar = 0` both bars play bar A.
  `test_wb_off`: with both hooks NULL, the existing pinned song hashes are unchanged (call the existing hash helper).
- [ ] **Step 2:** Run `app/build/tests/song_test wb_`; expect FAIL (undefined symbols).
- [ ] **Step 3:** Implement the interfaces above.
- [ ] **Step 4:** Run the whole suite; expect `all tests passed`.
- [ ] **Step 5:** Commit: `Engine: workbench hooks for a custom set and a two-bar pattern (default off)`.

### Task 3: Engine hooks — per-song overrides

**Files:**
- Create: `app/song/override.h`, `app/song/override.c`
- Modify: `app/song/harmony.c`, `app/song/lead.c`, `app/song/melody.c` (motif/phrase seeds), `app/song/groove.c`
  (ride), `app/song/pads.c` or wherever `harmony_pad_place` lives
- Test: `tests/host/song_test.c`

**Interfaces:**
- Produces:
  ```c
  typedef struct {
      int8_t comp;        /* -1 rolled, else CS_* */
      int8_t lead_style;  /* -1 rolled, 0 phrase planner, 1 hybrid */
      int8_t pad_place;   /* -1 rolled, else PL_* */
      int8_t ride;        /* -1 rolled, else RIDE_NONE / RIDE_ALL / RIDE_SECTION */
      int8_t keys_oct;    /* -1, 0, +1: the keys' register, clamped to the instrument's range */
      int8_t lead_oct;    /* -1, 0, +1: the lead's register, clamped likewise */
      uint32_t voicing_salt;  /* 0: as rolled; else a different, still rule-abiding voicing path */
      uint32_t phrase_salt;   /* 0: as rolled; else a new motif, rhythm and contour for the lead and melody */
  } song_ov_t;
  void song_override(const song_ov_t *ov);   /* NULL: none (default) */
  const song_ov_t *song_ov(void);            /* never NULL: a static all-rolled struct when none */
  ```
- `voicing_salt` changes the voicer's start shape (the first chord's reference spread) among its legal candidates;
  every voicing rule still holds (the clash test's checks).
- `phrase_salt` is mixed into the lead's and melody's seed streams only.

- [ ] **Step 1: Failing tests:** `test_ov_comp` (comp = CS_HALF on a held song: harmony_style returns CS_HALF);
  `test_ov_lead_style` (hybrid forced on a flute set: the lead plans with plan_hybrid); `test_ov_ride` (RIDE_NONE on
  a Warm-ride song: no V_RIDE events); `test_ov_voicing_salt` (over 40 seeds, salt 1 vs 0 changes the voicings in at
  least 30, and every salted song passes the clash test's rule checks); `test_ov_phrase_salt` (lead notes differ in
  at least 30 of 40 seeds; keys and drums identical); `test_ov_oct` (keys_oct +1 raises the keys 12 semitones where
  the range allows, never past it); `test_ov_off` (NULL: pinned hashes unchanged).
- [ ] **Step 2:** Run `app/build/tests/song_test ov_`; expect FAIL.
- [ ] **Step 3:** Implement.
- [ ] **Step 4:** Whole suite green, including clash.
- [ ] **Step 5:** Commit: `Engine: per-song overrides for the workbench (comping, lead style, pad place, ride,
  registers, voicing and phrase re-rolls; default off)`.

### Task 4: Engine hook — a drum source

**Files:**
- Modify: `app/engine/drums.h`, `app/engine/drums.c`
- Test: `tests/host/song_test.c`

**Interfaces:**
- Produces:
  ```c
  typedef struct {
      void (*on)(const song_ev_t *e);        /* a drum event (voice V_KICK..V_RIDE) */
      void (*render)(float *x, uint32_t n);  /* adds into the drums' bus, as drums_render */
      int (*active)(void);
      float (*kick_env)(void);
      void (*release)(void);
  } drums_hook_t;
  void drums_hook(const drums_hook_t *h);    /* NULL: the built-in kits (default) */
  ```
  While a hook is set, drums_on/render/active/kick_env/release delegate to it; the bus, compressor, filters and
  ducking are unchanged, so hooked drums go through the song's real chain.

- [ ] **Step 1: Failing test** `test_drums_hook`: a stub hook that records events and renders a 1.0 impulse per
  event; a song's drum events all reach `on`; the mix is non-silent at those times; `drums_hook(NULL)` restores the
  pinned hashes.
- [ ] **Step 2:** Run; FAIL. **Step 3:** Implement. **Step 4:** Suite green.
- [ ] **Step 5:** Commit: `Engine: a drum-source hook for the workbench (default off)`.

### Task 5: The wasm build, the glue and a playing page

**Files:**
- Create: `workbench/build.sh`, `workbench/engine/wb.c`, `workbench/engine/wb.h`, `workbench/wb-worklet.js`,
  `workbench/index.html` (transport only for now), `workbench/wb.js`, `workbench/server.py`,
  `workbench/tools/smoke.mjs`, `workbench/README.md`

**Interfaces:**
- `workbench/build.sh`: `emcc -O2 -ffp-contract=off -std=gnu99 -Iapp/song -Iapp/engine -Ibase/include
  -Iworkbench/engine --no-entry -sSTANDALONE_WASM -sALLOW_MEMORY_GROWTH` over app/song/*.c app/engine/*.c
  workbench/engine/*.c → `app/build/workbench/wb.wasm`; copies the page files there.
- Exports from wb.c (all `__attribute__((used, visibility("default")))`):
  - `void wb_init(void)`; `float *wb_out_l(void)`, `float *wb_out_r(void)`; `void wb_render(uint32_t n)` (n ≤ 1024
    frames; silence when stopped).
  - `void wb_seed(uint32_t seed)` (roll a song, restart at bar 1); `void wb_play(int on)`.
  - `int wb_roll_get(int field)`; `int wb_roll_set(int field, int value)` (staged; returns the value applied);
    `int wb_roll_dice(int field)` (staged; uses `roll_dice`; returns the new value or -1).
  - `void wb_role_set(int role, int ins)`; `int wb_role_get(int role)` (role = V_CHORD, V_BASS, V_LEAD, V_CANDY,
    V_PAD; edits the custom set, which the song switches to on first use).
  - `void wb_ov_set(int key, int value)`; `int wb_ov_get(int key)` (keys in wb.h: WB_OV_COMP … WB_OV_PHRASE_SALT).
  - `void wb_apply(void)`: stages become live at the next bar line by catch-up: song_start with the staged state,
    tick silently (events discarded) to the target bar, `vx_release_all`, resume. If the target bar is past the
    end, restart at bar 1. Several stagings before the bar line coalesce into one catch-up.
  - `int wb_bar(void)`, `int wb_bars(void)`, `float wb_seconds(void)`.
- `server.py`: `python3 workbench/server.py [port]` (default 8732) serves `app/build/workbench/` and `workbench/`
  (kits, patterns, samples, options.json); POST endpoints come in Task 7.

- [ ] **Step 1: Failing smoke test** `workbench/tools/smoke.mjs` (Node, loads wb.wasm directly): seed 21; play;
  render 20 s; RMS > -45 dBFS; render the same twice from a fresh instance → identical samples (determinism);
  `wb_roll_set(RF_TEMPO, 85)` + `wb_apply` at bar 3 → bar 4 plays at 85 BPM and the song keeps going;
  **Review Focus 1:** a staged change in the last bar restarts at bar 1 with the change, non-silent within 2 s;
  **Review Focus 2:** 50 stagings within one bar → one catch-up, the output never has a run of zeros longer than
  one render quantum after the bar line.
- [ ] **Step 2:** `sh workbench/build.sh && node workbench/tools/smoke.mjs`; expect FAIL.
- [ ] **Step 3:** Implement wb.c, the worklet (pattern: base/web/emu/worklet.js, minus the device parts),
  a page with play/stop/position, and the server.
- [ ] **Step 4:** Smoke test passes; open `http://localhost:8732/` and hear a song (note in the report that a human
  must confirm audio in a browser).
- [ ] **Step 5:** Commit: `Workbench: the engine in the browser (wasm, worklet, transport, server)`.

### Task 6: Decision rows, dice, locks and families

**Files:**
- Create: `workbench/options.json`, `workbench/rows.js`
- Modify: `workbench/index.html`, `workbench/wb.js`, `workbench/tools/smoke.mjs`

**Interfaces:**
- `options.json`: `{ "instruments": [{ "id": <INS_*>, "name", "roles": [...], "family": "acoustic"|"synth"|
  "electric", "status": "approved"|"candidate"|"retired" }], "kits": [...same shape...], "comping": [...],
  "patterns": [...], "kickPhrases": [{ "id": "k2", "a": [0,10], "b": [0,10] }, … k3 [0,7,10]/[0,7,10], k4 [0,6,8]/
  [0,6,8], k5 [0,10]/[0,7,10], k6 [0,6]/[0,3,10]], "families": {...} }`. Seed it from the ledgers and the Global
  Constraints lists; library ids from app/song/library.c.
- `rows.js` exports `ROWS` (group, label, kind: roll|role|ov|reroll|drum, field/role/key, options source) and
  `pickFor(row, state, opts)` (the dice for role/drum rows: a random approved option of the song's family, not the
  current one), `conflicts(row, value, state)` (rows a value forces to change, and whether a lock blocks it).
- Rows (spec): Song (seed, key/mode, tempo, swing, mood, motion, map, progression, color); Instruments (family,
  keys, comping, lead, bass, pad, ear candy); Voicing and phrase (🎲 phrase, lead style, lead register, 🎲 voicings,
  keys register, pad place); Drums (kit, pattern, kick phrase, ride section, "Edit kit & pattern" button); Texture
  (dust, noise, ambience). Each: dropdown, 🎲, 🔒; "re-roll all unlocked"; "show retired" switch.

- [ ] **Step 1: Failing tests** in smoke.mjs (pure JS on rows.js and options.json): no retired option is offered
  with "show retired" off; `pickFor` never returns the current value nor a different family's option; family →
  synth on an acoustic song re-derives bass/pad/candy/kit to synth options and reports them as forced;
  **Review Focus 3:** a family change while the bass row is locked to an acoustic-only bass is blocked and the
  message names "Bass".
- [ ] **Step 2:** Run; FAIL. **Step 3:** Implement rows, page rendering (layout A from the spec), highlighting of
  forced rows, and wiring to the wb_* exports (every change: stage, then `wb_apply`).
- [ ] **Step 4:** Smoke passes; a manual pass in the browser: each row changes what's heard at the next bar.
- [ ] **Step 5:** Commit: `Workbench: decision rows, dice, locks, families`.

### Task 7: Feedback, snapshots and replay

**Files:**
- Modify: `workbench/server.py`, `workbench/wb.js`, `workbench/index.html`
- Create: `workbench/tools/replay.mjs`, `workbench/tools/test_server.py`

**Interfaces:**
- Snapshot: `{ "rows": {<row id>: value}, "locks": [...], "seed", "drums": {kit lanes, levels, pattern} }`.
- Feedback entry (one line of feedback.jsonl): `{ "time": ISO, "snapshot", "position": {"seconds","bar"},
  "about": "song"|<row id>, "rating": "up"|"down"|null, "note", "changes": [{"seconds","bar","row","from","to"}] }`.
- Server: `POST /feedback` appends a line; `POST /kit` and `POST /pattern` write `workbench/kits|patterns/<name>.json`
  (names `[a-z0-9-]{1,40}`, refuse others with 400); responses JSON `{ok:true}`.
- `node workbench/tools/replay.mjs <feedback line number | snapshot.json> out.wav [seconds]` renders the snapshot
  through wb.wasm (same code path as the page).

- [ ] **Step 1: Failing tests:** test_server.py (start on a free port; POST feedback twice → two lines; bad kit
  name → 400); smoke.mjs: a snapshot replayed twice → identical WAV bytes; **Review Focus 5:** with the server
  down, a feedback entry is kept in localStorage (simulate with a fake fetch) and resent when it returns, and the
  page shows "not saved yet".
- [ ] **Step 2:** Run; FAIL. **Step 3:** Implement 👍/👎 + note per song and per row, the change log, the queue.
- [ ] **Step 4:** Tests pass; a feedback entry written from the browser replays to a WAV that matches what played.
- [ ] **Step 5:** Commit: `Workbench: feedback with replayable snapshots`.

### Task 8: Synth drum sources (poketrack 808, CR-78; Optimist LO-FI) and the lane engine

**Files:**
- Create: `workbench/engine/wb_drums.c`, `workbench/engine/wb_drums.h`, `workbench/engine/poketrack/` (copied
  trommelsynthesizer.c/.h, gains.h; zlib headers kept), `workbench/engine/optimist/` (copied drum_synth.c and the
  generated kit table for LO-FI; GPL-3.0-only headers kept), `workbench/engine/LICENSES.md`
- Modify: `workbench/engine/wb.c`, `workbench/build.sh`, `CREDITS.md`

Sources: the spike's clones at `/private/tmp/claude-501/-Users-samueloltz-Documents-fm-1/dfd13d60-fb03-409f-86fe-
9daa1a019026/scratchpad/reuse/` (poketrack 2c5d4a6, optimist 915fd8a) and the spike drivers in
`.../scratchpad/drumspike2/` (drv_pt.c, drv_opt.c, kitdefs.py) show working host calls; re-clone if gone.

**Interfaces:**
- A lane source: `typedef struct { uint8_t kind; uint8_t a, b; float gain_db; } wb_src_t;` kind = WB_SRC_NONE,
  WB_SRC_PT (a = machine 808|CR78, b = poketrack voice), WB_SRC_OPT (a = kit, b = voice), WB_SRC_SAMPLE (a = sample
  set, b = piece; Task 9), WB_SRC_BUILTIN (a = KIT_BRUSHES, b = lane).
- Lanes: kick, snare, closed hat, open hat, shaker, rim, ride (V_KICK..V_RIDE order).
- Exports: `void wb_lane_src(int lane, int kind, int a, int b)`, `void wb_lane_db(int lane, float db)`,
  `void wb_audition(int lane, int vel)` (plays the lane's sound now, outside the song, through the same chain),
  `void wb_kit_preset(int id)` (the three synth kits as shipped: every lane from its own machine/kit; a lane the
  kit lacks plays its closed hat for ride, nothing for shaker).
- wb_drums installs itself with `drums_hook` while any lane source is set.

- [ ] **Step 1: Failing tests** (smoke.mjs): each preset renders a song with non-silent kick and snare lanes;
  preset → per-lane override (CR-78 snare in the 808 kit) changes only that lane's samples; audition is non-silent
  and doesn't advance the song.
- [ ] **Step 2:** FAIL. **Step 3:** Implement; levels matched so each preset's kick+snare sits within 2 dB of the
  Brushes kit's on the same song (measure in the test).
- [ ] **Step 4:** Pass. **Step 5:** Commit: `Workbench: poketrack 808/CR-78 and Optimist LO-FI drum sources`.

### Task 9: Shipped sampled kits (Gogodze; Big Rusty sticks and brushes)

**Files:**
- Create: `workbench/tools/prep_kits.py`, `workbench/samples/README.md` (how to fetch; the samples themselves are
  git-ignored), `workbench/samples/kits.json` (generated index)
- Modify: `.gitignore` (`workbench/samples/*.f32`), `workbench/engine/wb_drums.c`, `workbench/wb.js`, `CREDITS.md`

The spike's SFZ reader and mic mixes (in `.../scratchpad/drumspike2/`: insong.py, kitdefs.py, gg.paths, br.paths,
INDEX.md of app/build/new/drums-originals) are the reference: the same programs (Gogodze `Programs/Kit.sfz`; Big
Rusty `Programs/02-basic.sfz`, brushes via its brush maps and switches), mixed to mono per hit.

**Interfaces:**
- `python3 -I workbench/tools/prep_kits.py FETCH_DIR workbench/samples` fetches (sparse, shallow) and writes one
  float32 mono 44.1 kHz file per piece and velocity layer/round robin (at most 4 layers × 2 round robins per piece),
  and `kits.json` (pieces, layers, which lanes they serve; Gogodze ride → its closed hat, no shaker; Big Rusty
  brushes rim → brush snare at 45%).
- Page loads the needed `.f32` files on first use of a kit into wasm memory: `uint32_t wb_sample_alloc(uint32_t
  bytes)`, `void wb_sample_set(int set, int piece, int layer, uint32_t ptr, uint32_t frames)`.

- [ ] **Step 1: Failing tests:** prep_kits.py on a tiny fixture SFZ (in workbench/tools/fixtures/) writes the
  expected files and index; smoke.mjs: a sampled kit renders non-silent kick and snare; **Review Focus 4:** with a
  piece's file missing, the kit is reported unavailable and the picker can't select it.
- [ ] **Step 2:** FAIL. **Step 3:** Implement. **Step 4:** Pass; run prep_kits.py for real.
- [ ] **Step 5:** Commit: `Workbench: Gogodze and Big Rusty kits as shipped`.

### Task 10: The drum screen

**Files:**
- Create: `workbench/drums.js`
- Modify: `workbench/index.html`, `workbench/wb.js`, `workbench/engine/wb.c` (pattern exports),
  `workbench/tools/smoke.mjs`

**Interfaces:**
- Exports: `void wb_pat_step(int lane, int bar01, int step, int state)` (state 0 empty, 1 hit, 2 ghost; edits the
  custom two-bar pattern, which the song switches to on first edit; lands at the next bar), `int wb_pat_get(int
  lane, int bar01, int step)`, `void wb_pat_load(int pattern_id)` (copies a library pattern into the custom one).
- Screen (spec, drums.html mockup): kit / pattern / kick-phrase pickers with 🎲; per lane: sound picker (any kit's
  sounds, approved only unless "show retired"), 🎲, ▶ audition, level; the two-bar grid (click cycles empty → hit
  → ghost); "Save as new kit" / "Save as new pattern" (POST /kit, /pattern); kick-phrase presets (k2-k6) write the
  kick lane's bars A and B.

- [ ] **Step 1: Failing tests:** grid edit at bar 5 is heard from bar 6 (smoke: kick events at the edited step);
  k5 preset → kick lane A = [0,10], B = [0,7,10]; save-pattern round trip (POST then load) reproduces the grid.
- [ ] **Step 2:** FAIL. **Step 3:** Implement. **Step 4:** Pass; manual pass in the browser.
- [ ] **Step 5:** Commit: `Workbench: the drum screen (lane sounds, levels, two-bar grid, save)`.
