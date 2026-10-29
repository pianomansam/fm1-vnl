# Patterns out and in, piano-roll edits, the lead-doubling switch: Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Sam edits and adds the engine's data patterns as MIDI/MuseScore files and imports them back; edits rule-made
parts in the workbench's piano roll with a "why"; and can let the lead double other voices.

**Architecture:** A host C dump program prints the engine's pattern tables as JSON; Python (stdlib) writes them as
`.mid` + `.musicxml` under `patterns/<family>/`, and reads them back, validates them and generates the C tables the
engine compiles (`app/song/patterns_gen.c`). Entries carry a status: the firmware rolls approved entries only, the
workbench can pick candidates. Piano-roll edits are a default-off workbench event filter (delete/add) applied after
`song_tick`; the page keeps them per part roll. The doubling switch is a `song_ov_t` field read by the lead's rules.

**Tech Stack:** C (app/, -Werror host suite), Python 3 stdlib (tools/patterns/), plain HTML/JS + wasm (workbench/).

**Spec:** docs/superpowers/specs/2026-10-29-patterns-and-edits-design.md

## Global Constraints

- Local only; plain HTML/JS; Python stdlib only for tools; run Python on downloaded or user-edited files with `-I`.
- Engine hooks under app/ are default-off: with no hook and no override set, the firmware's songs are unchanged
  (pinned hashes b852d9cb / a662a137 and the voicer 0x7b61830d hold) — except where a task says it regenerates a table
  byte-identically.
- Approved options only in rolls; candidates are workbench-only until Sam approves them.
- US English; simplest key spelling (Bb not A#). Velocities 25-110.
- Never stop the server on 127.0.0.1:8732; use another port for browser checks. Don't write to workbench/feedback.jsonl
  in tests. Never delete anything in app/build/new.
- Commits: git email 884872+pianomansam@users.noreply.github.com, trailers
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>` and
  `Claude-Session: https://claude.ai/code/session_01ENYN74abncM7Wq3c4vd7md`. Never push or merge.
- Tests: engine `PATH=$PWD/.venv/bin:$PATH sh tests/run_tests.sh`; workbench `sh workbench/build.sh`,
  `node workbench/tools/smoke.mjs`, `python3 workbench/tools/test_server.py`; patterns `python3 -I tools/patterns/test_patterns.py`.

## Review Focus

1. A file MuseScore re-saves (different divisions, tied notes across the bar, a pickup/rest-only measure, extra
   instruments/staves) → imports as the same pattern, or names the file and the problem; never a silent wrong table.
2. An edited file that breaks a rule (no snare, a semitone rub chord, a substitution, an off-grid note) → nothing
   regenerated, the error names the file, the bar and the rule.
3. A piano-roll edit on a part, then a re-roll / ◀ ▶ / new seed / mute / section jump / loop → the edits come back
   exactly with their roll, never leak onto another roll, and what plays matches the roll drawn.
4. A new candidate pattern → the firmware never rolls it (hashes unchanged), the workbench can pick it, approving it
   changes the firmware's rolls only after re-import.
5. The doubling switch on → only the doubling rules are skipped; rubs, avoid notes and range still hold (clash tests).

---

### Task 1: The lead-doubling switch

**Files:** Modify `app/song/override.h`, the lead's rule sites (`app/song/lead.c`, `melody.c`, `phrase.c`, `song.c` as
found), `workbench/engine/wb.c`/`wb.h` (WB_OV_LEAD_DOUBLE), `workbench/rows.js`, `workbench/options.json`,
`workbench/tools/smoke.mjs`; Test `tests/host/song_test.c`.

**Interfaces:** Produces `int8_t lead_double` in `song_ov_t` (0 = rules on, default; 1 = rules off), override key
`WB_OV_LEAD_DOUBLE`, row `{ id: "leadDouble", group: VP, label: "Lead may double other voices", kind: "ov",
key: "LEAD_DOUBLE", source: "onOff" }`.

- [ ] **Step 1: Failing tests:** `test_lead_double_off` (no override: hashes unchanged); `test_lead_double_on` (over
  300 seeds with lead_double=1: some lead note equals or is an octave of a sounding keys/bass/pad note at the same step
  where none did with 0 — the report lists each rule site skipped); clash_test with lead_double=1 still passes (rubs,
  avoid notes, range). Smoke: the row exists, toggles, lands at the next bar, rides in snapshots; old snapshots
  restore with it off.
- [ ] **Step 2:** FAIL. **Step 3:** Implement: find every rule that steers the lead away from another voice's pitch or
  octave (e.g. lead.c `skel_near`'s `keys[3] + 12` cost) and gate it on `!song_ov()->lead_double`.
- [ ] **Step 4:** All suites green. **Step 5:** Commit `Engine: a switch letting the lead double other voices (Sam; default off)`.

### Task 2: Export

**Files:** Create `tools/patterns/dump.c` (host program printing every table as JSON), `tools/patterns/export.py`,
`tools/patterns/midi.py`, `tools/patterns/mxl.py`, `tools/patterns/test_patterns.py`, `patterns/README.md`,
`patterns/<family>/*.mid|.musicxml`.

**Interfaces:** Produces the JSON model (one object per family, entries keyed by id, each with `status`) that Task 3
reads back; files named `<family>/<id>-<slug>.mid|.musicxml`; `midi.write(path, tracks, ppq=480)`,
`mxl.write(path, parts, chords=None)`.

Families and conventions are the spec's table (§1) verbatim: drums (8, GM map, >60 hit, ≤60 ghost, pattern 5's
every-3rd shaker written out with `poly3` kept in metadata), kick-phrases (options.json, 5), progressions (44,
chord symbols in C, `kind/mood/mode` in metadata), comp-rhythms (held, half, stabs: the `SECOND` candidates as
alternates), strum (4 + ballad fill: one pitch per stroke char), lead-arp (RHY 6, PAT 6, landings AT/LEN/W 6, over a
reference Cmaj7 C4-E4-G4-B4 = tones 1-4), melody-cells (7). All quantized to 16ths, 4/4, tempo 85.

- [ ] **Step 1: Failing tests** (`test_patterns.py`): every table entry gets both files; a MIDI file parses with a
  minimal reader to the same hits; MusicXML is well-formed and MuseScore-shaped (`score-partwise` 4.0, divisions 4);
  `README.md` lists each family's convention.
- [ ] **Step 2:** FAIL. **Step 3:** Implement; `tools/patterns/export.sh` builds `dump` with the host compiler and
  writes `patterns/`. **Step 4:** Pass. **Step 5:** Commit `Patterns: export every data table to MIDI and MuseScore`
  (including the generated files).

### Task 3: Import and generated tables

**Files:** Create `tools/patterns/import.py`, `tools/patterns/import.sh`, `tools/patterns/validate.py`,
`app/song/patterns_gen.c` (generated, committed, header comment "generated by tools/patterns/import.py; edit
patterns/"), `app/song/patterns_gen.h`; Modify `app/song/library.c`, `strum.c`, `lead.c`, `melody.c`, `harmony.c`
(the tables and the comp-rhythm constants move to the generated file); `workbench/options.json` (kickPhrases
regenerated).

**Interfaces:** Consumes Task 2's file conventions. Produces `import.sh [--check]` (exit 0 and regenerate; `--check`
validates only); each generated table keeps its current C name and type, plus a parallel `status` (1 approved,
0 candidate) per entry.

- [ ] **Step 1: Failing tests:** round trip — export then import with no edits regenerates `patterns_gen.c`
  byte-identically, and the engine suite's hashes hold; MusicXML and MIDI of one pattern import identically;
  **Review Focus 1:** MuseScore-style variants (divisions 480, a tie across beats, an extra empty staff, rests-only
  bar) import the same; **Review Focus 2:** each rule (drum without snare; progression with a substitution or a
  dominant below the 9th; off-grid note; strum up-stroke off an "and"; a chord-tone index > 4) is refused with
  file + bar + rule and leaves `patterns_gen.c` untouched; when both `.mid` and `.musicxml` exist, the newer wins.
- [ ] **Step 2:** FAIL. **Step 3:** Implement; a new file (no id yet) gets the next free id and status candidate; the
  importer prints which existing approved entries changed and, if any did, that the pinned hashes will move (re-pin
  with a comment naming the edit).
- [ ] **Step 4:** All suites green. **Step 5:** Commit `Patterns: import from MIDI/MuseScore into generated engine tables`.

### Task 4: Candidates

**Files:** Modify the roll sites that draw each family (roll.c, strum.c, lead.c, melody.c, harmony.c), `override.h`
(forced picks for families without a row: `strum_pat`, `arp_rhy`, `arp_pat`, -1 rolled), `workbench/engine/wb.c`,
`workbench/rows.js`, `workbench/options.json` (generated `patterns`/`progs`/… entries with status), `workbench/server.py`
(POST /approve → writes `patterns/status.json`), `workbench/tools/test_server.py`, smoke.

**Interfaces:** Produces `patterns/status.json` (`{family: {id: "approved"|"candidate"|"retired"}}`, read by import.py);
rows `strumPat`, `arpRhythm`, `arpPattern` (ov); an "Approve" control beside a candidate in a row.

- [ ] **Step 1: Failing tests:** **Review Focus 4:** a candidate entry is never drawn by the firmware roll over 1000
  seeds and adding one leaves the hashes unchanged; the workbench can force it and it plays; POST /approve writes
  status.json (JSON-only, Host/Origin checks as the other POSTs) and re-import then makes it rollable; server tests.
- [ ] **Step 2:** FAIL. **Step 3:** Implement. **Step 4:** Green. **Step 5:** Commit `Patterns: candidates are
  workbench-only until approved`.

### Task 5: Edit glue

**Files:** Modify `workbench/engine/wb.c`, `wb.h`, smoke.

**Interfaces:** Produces `void wb_edits(const uint8_t *buf, int n)` taking packed records
`{u8 op (0 delete, 1 add); u8 role; u16 bar; u8 step; u8 note; u8 vel; u16 len_steps}` (bar 0-based, as wb_events),
`wb_edits_clear()`; edits land with the next landing like any change; `wb_events` reports the edited song (what plays).

- [ ] **Step 1: Failing tests:** delete removes exactly that event from both the heard events and wb_events; add plays
  a note with that length/velocity; an edit to a muted part stays silent and plays on unmute; edits survive a jump
  and loop; `wb_edits_clear` restores the engine's part exactly; default (no edits) leaves wb's output unchanged.
- [ ] **Step 2:** FAIL. **Step 3:** Implement as a filter after `song_tick` (no app/ change). **Step 4:** Green.
  **Step 5:** Commit `Workbench: note edits (delete/add) on the song heard`.

### Task 6: Piano-roll editing, why, history

**Files:** Modify `workbench/roll.js`, `workbench/index.html`, `workbench/desk.js`, `workbench/tools/replay.mjs`, smoke.

**Interfaces:** Consumes Task 5. Produces `desk.parts[p].history[i].edits` (list of records), roll interactions
(click add, select + Delete, drag move/resize snapped to 16ths, ↑↓ velocity ±8 within 25-110), a "Save edits" per
part that requires a why (non-empty) and posts feedback `{about: "edit:<part>", why, before, after, snapshot}`.

- [ ] **Step 1: Failing tests** (desk-level + pure roll.js hit-testing): **Review Focus 3:** edits ride their roll:
  ◀ ▶ restore them exactly, a re-roll of an edited part asks first (refuse without confirm) and the new roll has
  none, a new seed sets them aside unless locked, mute/jump/loop keep them; save without a why is refused; the feedback
  entry replays (replay.mjs) to the heard samples with the edits; old snapshots restore with no edits.
- [ ] **Step 2:** FAIL. **Step 3:** Implement; headless Chromium on another port: add, move, resize, delete a note,
  save with a why, 0 console errors. **Step 4:** Green. **Step 5:** Commit `Workbench: edit a part's notes in the
  piano roll, and say why`.
