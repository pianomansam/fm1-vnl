# VNL-1 Workbench Round 2 (sections, parts, piano roll) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** In the workbench, jump to any section, mute/solo parts, see every part's notes in a piano roll, re-roll one
part at a time with a history, and give feedback per part.

**Architecture:** Four new default-off part salts in the engine's per-song overrides; the wasm glue exports the
committed song's sections, a section jump (a catch-up to a chosen bar), a mute mask (bus-level), and the committed
song's events from the main-thread probe instance; the page adds a section strip, mute/solo, a canvas piano roll and
per-part 🎲 / ◀ ▶ / 🔒 / feedback.

**Tech Stack:** C99 (engine, emcc for wasm), plain HTML/CSS/JS, Node smoke tests, the existing Python server.

**Spec:** docs/superpowers/specs/2026-10-29-workbench-parts-design.md (builds on
docs/superpowers/specs/2026-10-28-workbench-design.md)

## Global Constraints

- Everything in docs/superpowers/plans/2026-10-28-workbench.md's Global Constraints still binds (local only, approved
  options only, default-off engine hooks, changes land at the next bar, licenses, commit rules, never delete under
  app/build/new, suites stay green).
- Part salts are 0 = as rolled; with every salt 0 the engine's pinned hashes (sf_songs a1a8e201 / 3910eb79) and every
  existing test are unchanged.
- The existing phrase re-roll (phrase_salt) keeps moving the lead and the melody/candy streams together (old feedback
  replays unchanged); the new candy salt moves only the candy.
- Mute/solo is listening only: the song (voicings, the keys' duck, everything the engine decides) is unchanged; a
  muted part's sounding notes are cut at once and its new ones are silent.
- The piano roll shows the committed song as playing; muted parts greyed, not hidden.
- A section jump lands at the next bar line at the chosen section's first bar; while stopped it sets the start point.
- Part lifetimes: a seed or arrangement-map change resets part salts and histories unless the part is locked;
  "re-roll all" re-rolls unlocked parts; a family change keeps salts; mute/solo persist across regenerates.
- The drums 🎲 draws a kick phrase (k2-k6) plus a variation; it is disabled with a message while the grid holds hand
  edits; the kick-phrase row keeps its own 🎲.
- After a part re-roll lands, the page lists every other part whose events changed (a diff from the probe), no fixed
  list.
- Snapshot stays version 1, additive: `parts` (salts per part, locks) and `listen` (mute/solo).

## Review Focus

1. Jumping to a section while a change is still staged: the jump and the change land together at the next bar, no
   lost change, no double catch-up. Test in Task 2.
2. Mute toggled in the middle of a note: the note stops at once, and unmuting doesn't replay old notes. Test in
   Task 2.
3. A long song's piano roll (76 bars, all parts): drawn in under 200 ms after a landing on this Mac, no frozen page.
   Test in Task 4.
4. Stepping ◀ past the first roll or ▶ past the last: the button is disabled, nothing changes. Test in Task 5.
5. A part re-roll while that part is muted: it still re-rolls (the roll shows it greyed) and unmuting plays the new
   roll. Test in Task 5.

---

### Task 1: Engine — part salts

**Files:**
- Modify: `app/song/override.h`, `app/song/override.c`, `app/song/groove.c` (bass streams), `app/song/pads.c`,
  `app/song/melody.c` (candy), `app/song/groove.c` or `song.c` (drums variation), `workbench/engine/wb.c` (WB_OV keys)
- Test: `tests/host/song_test.c`

**Interfaces:**
- Produces: `song_ov_t` gains `uint32_t bass_salt, pad_salt, candy_salt, drums_salt;` (0 = as rolled); wb.h keys
  `WB_OV_BASS_SALT, WB_OV_PAD_SALT, WB_OV_CANDY_SALT, WB_OV_DRUMS_SALT` (in wb_consts).
- bass_salt reaches the bass style draw and walking window (groove.c streams 112/113 and the bass rhythm draw);
  pad_salt reaches pads.c stream 191 (and the pad's other song-long draws); candy_salt reaches only the candy's
  streams; drums_salt reaches the drum lanes' per-hit variation draws (velocity/ghost/lateness jitter salt) only —
  the kick phrase itself is chosen page-side (Task 5).

- [ ] **Step 1: Failing tests** `test_ov_part_salts`: over seeds 1-40, for each salt set to 1 (others 0): that
  part's events differ in ≥ 30 songs; drums' and every non-reacting part's events are identical; reactions allowed
  and listed in a comment (bass_salt may move nothing else; candy_salt nothing else; pad_salt nothing else;
  drums_salt nothing else); with all salts 0 the pinned hashes are unchanged. Clash test: songs 1-40 with each salt
  pass `run()`.
- [ ] **Step 2:** `app/build/tests/song_test ov_part` → FAIL.
- [ ] **Step 3:** Implement.
- [ ] **Step 4:** Full suite green.
- [ ] **Step 5:** Commit: `Engine: part salts for the workbench (bass, pad, candy, drums; default off)`.

### Task 1b: Engine — no ear candy; an optional pad, by song and by section

Added 2026-10-29 from Sam: "Remove the ear candy part completely. It doesn't make sense"; "Make it possible for the
pad part to not be in a song. And update the seeds to not always have it" (about two of three songs: "this looks
good"); "add the ability for the pad part to drop out of certain song sections" (rule approved: "yes").

**Files:**
- Modify: `app/song/melody.c`, `app/song/song.c`, `app/song/plan.c`, `app/song/library.c/.h` (the candy fields stay
  in the structs but are unused, or are removed with every reference — implementer's choice, say which),
  `app/song/pads.c`, `app/song/roll.c` (a pad stream), `app/engine/voices.c` (no candy voice assigned),
  `workbench/options.json`, `workbench/rows.js`, `workbench/desk.js`, `workbench/index.html` (no candy row/part)
- Test: `tests/host/song_test.c`, `workbench/tools/smoke.mjs`

**Interfaces:**
- No song produces V_CANDY events (V_CANDY stays in the enum; nothing emits it).
- `int song_pad_on(const song_roll_t *r)` — the song has a pad: about 2 of 3 songs (its own seed stream, so no other
  draw shifts); a workbench override `pad_on` (-1 rolled, 0 off, 1 on) in song_ov_t and WB_OV_PAD_ON.
- `int song_pad_in(const song_roll_t *r, const song_plan_t *p, int block)` — the pad plays in this section: about 2
  of 3 sections in a pad song; likelier out in intros and breaks, likelier in for lead sections and main grooves;
  never out for more than 2 sections in a row; the pad fades in/out over the section line (no hard cut).
- Every pinned hash and level test that moves is re-pinned with a comment naming this task and Sam's words; level
  tests' targets stay (the mix without candy/pad must still meet them, or the mix is fixed in code).

- [ ] **Step 1: Failing tests:** over 300 seeds no V_CANDY event; pad songs between 60% and 73%; in pad songs,
  sections with the pad between 55% and 78%, never 3 pad-less sections in a row, intro/break pad-less rate higher
  than lead/groove's; pad notes fade (no V_PAD note starts at full level on a section's first bar after a pad-less
  section… or the pad bus gain ramps — implementer picks the mechanism and tests it); pad_on override 0/1 works.
- [ ] **Step 2:** FAIL. **Step 3:** Implement; workbench: candy row and part gone, a "Pad" on/off row (dice and lock
  like others).
- [ ] **Step 4:** Engine suite, smoke, server tests green. **Step 5:** Commit:
  `Engine: no ear candy; the pad in about two of three songs and sections (Sam)`.

### Task 2: Glue — sections, jump, mute/solo, events

**Files:**
- Modify: `workbench/engine/wb.c`, `workbench/engine/wb.h`, `app/engine/voices.c`, `app/engine/voices.h`,
  `workbench/wb.js`, `workbench/tools/smoke.mjs`
- Test: `workbench/tools/smoke.mjs`, `tests/host/song_test.c` (the mute hook's default-off check)

**Interfaces:**
- Produces:
  - `const char *wb_sections(void)` — JSON `[{"start":bar,"bars":n,"name":"intro|groove|lead|break|outro…"}]` of
    the committed song's plan.
  - `void wb_jump(int bar)` — stages a jump: the next landing goes to `bar` (catch-up there); combines with any
    staged change into one landing; while stopped, sets where Play starts.
  - `void vx_mute(unsigned mask)` in voices.c (bit per bus: V_CHORD..V_PAD, and bit V_KICK for the drums' bus;
    default 0 = none; a newly muted bus's sounding voices are cut at once) and `void wb_mute(unsigned mask)`,
    `void wb_solo(int role)` (-1 none; solo = mute every other bus) in wb.c.
  - `int wb_events(uint32_t ptr_max_hint)` on the probe instance: runs the committed song silently from bar 1 and
    writes every event as packed records `{u16 bar; u8 step; u8 role; u8 note; u8 vel; u16 len_steps}` into a wasm
    buffer; `uint32_t wb_events_ptr(void)`, `int wb_events_count(void)`. Drum events use role V_KICK..V_RIDE and
    note = lane.
- Engine change ruling: mute needs a bus-level gate to cut sounding notes, which the glue can't do; `vx_mute` is a
  default-off hook (mask 0 = unchanged; test_vx_mute_off pins the hashes).

- [ ] **Step 1: Failing tests** (smoke, plus song_test for vx_mute off): sections JSON matches the plan's blocks
  and sums to the song's bars; jump to section 3 while playing lands at its first bar at the next bar line;
  **Review Focus 1:** a staged tempo change + a jump in the same bar → one landing, at the target bar, with the new
  tempo; jump while stopped → Play starts at that bar; mute lead → V_LEAD bus output silent within one quantum
  (render the bus via vx's solo/meter or compare full mixes) while every event still occurs (events identical);
  **Review Focus 2:** mute mid-note cuts it; unmute plays only notes struck after; solo keys = mute all others;
  wb_events count equals the events of a silent run of the same committed state, and round-trips (bar, step, role,
  note) against song_tick.
- [ ] **Step 2:** FAIL. **Step 3:** Implement. **Step 4:** Smoke and engine suite green.
- [ ] **Step 5:** Commit: `Workbench: sections, jump, mute/solo, the committed song's events`.

### Task 3: Page — section strip, mute/solo, listening state in feedback

**Files:**
- Modify: `workbench/index.html`, `workbench/desk.js`, `workbench/wb.js`, `workbench/tools/smoke.mjs`

**Interfaces:**
- Consumes Task 2's exports. Produces `desk.listen = {mute: Set<role>, solo: role|null}` (persisting across
  regenerates) and snapshot `listen`, plus feedback `position.section`.
- Section strip across the top: blocks scaled by bars, labeled, playhead moving, current section highlighted;
  click → `wb_jump(start)` through the run queue. Mute (M) and solo (S) buttons per part next to the part's rows.

- [ ] **Step 1: Failing tests** (smoke, desk-level): clicking a section stages a jump that lands at its first bar;
  mute/solo state survives a new seed; a feedback entry carries `position.section` and `listen`; replaying a
  snapshot restores mute/solo.
- [ ] **Step 2:** FAIL. **Step 3:** Implement; headless Chromium: strip renders, clicking jumps, 0 console errors.
- [ ] **Step 4:** Pass. **Step 5:** Commit: `Workbench: section strip and mute/solo`.

### Task 4: Piano roll (view only)

**Files:**
- Create: `workbench/roll.js`
- Modify: `workbench/index.html`, `workbench/wb.js`, `workbench/tools/smoke.mjs`

**Interfaces:**
- Consumes `wb_events*` (via the probe instance) and `wb_sections`. `roll.js` exports
  `layout(events, sections, bars) -> {lanes:[{role, notes:[{x,w,y,shade}]}]}` (pure, testable in Node) and
  `draw(canvas, layout, playhead, listen)`.
- One lane per part (keys, bass, lead, pad, candy: pitch vs time, length as width, velocity as shade; chord
  changes and section boundaries as lines), drums as a lane grid; scrolls to keep the playhead in view; redraws on
  every landing; muted parts greyed.

- [ ] **Step 1: Failing tests** (pure JS on `layout`): every event maps to one rectangle in its part's lane at the
  right bar/step; drums map to their lane rows; **Review Focus 3:** layout + draw of a 76-bar song with all parts
  takes < 200 ms in Node (time `layout`; time `draw` on a stub canvas context).
- [ ] **Step 2:** FAIL. **Step 3:** Implement; headless Chromium: roll renders, playhead moves, muted lane greyed,
  0 console errors.
- [ ] **Step 4:** Pass. **Step 5:** Commit: `Workbench: piano roll (view only)`.

### Task 5: Part re-rolls, history, locks, reactions, feedback per part

**Files:**
- Modify: `workbench/desk.js`, `workbench/rows.js`, `workbench/index.html`, `workbench/tools/smoke.mjs`,
  `workbench/tools/replay.mjs`

**Interfaces:**
- Consumes Task 1's salts (WB_OV_*_SALT; keys uses the existing voicing salt, lead the existing phrase salt),
  Task 2's events, Task 4's roll. Produces `desk.parts = {keys|bass|lead|pad|candy|drums: {salt, history: [...],
  at: index, locked: bool}}` and snapshot `parts`.
- Per part: 🎲 (new nonzero salt; drums: a kick phrase from options.json kickPhrases plus a drums salt), ◀ ▶ through
  history, 🔒, 👍/👎 + note (feedback `about: "part:<name>"`). After a re-roll lands, diff every other part's
  events (probe, before vs after) and show "<part> moved" lines. Lifetimes per the Global Constraints.

- [ ] **Step 1: Failing tests:** each part's 🎲 changes that part's events and lands at the next bar; ◀ returns the
  previous roll exactly (events identical); **Review Focus 4:** ◀ at the first roll and ▶ at the last are disabled
  and change nothing; a new seed resets unlocked parts' salts and histories, keeps locked ones; "re-roll all"
  re-rolls unlocked parts; the drums 🎲 is refused with a message while the grid has hand edits; a lead 🎲 reports
  "candy moved" (shared streams) and a keys-room reaction where it occurs; **Review Focus 5:** a re-roll of a muted
  part changes its events (roll greyed) and unmuting plays the new roll; per-part feedback replays to the heard
  audio (replay.mjs restores `parts`).
- [ ] **Step 2:** FAIL. **Step 3:** Implement; headless Chromium: each part's controls work, 0 console errors.
- [ ] **Step 4:** Smoke, server tests and engine suite green. **Step 5:** Commit:
  `Workbench: part re-rolls with history, locks, reactions and feedback`.
