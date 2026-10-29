# The lead styles, built into the engine: Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The eight shortlisted lead styles become the engine's only lead generators, with room made by the keys, the
old engines and unwanted leads removed, flash back under budget, and every trace of the source names scrubbed.

**Architecture:** A small lead framework (app/song/leads/) holds the shared motif format, the per-style register
centers and the dispatch; each style is one file ported from its spike. The harmony gets a per-song keys window so the
lead has room. Removals, set changes, the style roll and the workbench follow; flash is measured; the scrub runs last.

**Tech Stack:** C (app/, -Werror host suite, firmware `./build.sh`), the wasm workbench (workbench/), Python stdlib.

**Spec:** docs/superpowers/specs/2026-10-29-lead-styles-design.md

## Global Constraints

- Lead rules hold in every style: no semitone rubs against keys, bass or pad; no avoid notes on strong beats;
  velocities 25-110; chords never changed (no substitutions).
- Ported styles keep their spike's rules as Sam heard them (Sam: "Don't go back and rework any motifs"); only the
  shared fixes change them (keys' room, register centers). Their quirks carry over and go to the ledger's To revisit.
- Spike sources (read with `git show <branch>:<path>`, never merged): Impressionism `worktree-agent-aaf267bc7c3b70bee`;
  Romanticism `worktree-agent-aeec8cac90861a25b`; '60s Jazz `worktree-agent-a4802cdbc40dd46b4`; Modal Jazz
  `worktree-agent-a8cba358d5d753da1`; Jazz-Hop `worktree-agent-acdb178f7a5283930`; Film Score
  `worktree-agent-a58dfa512ae37f56c`; Bossa Nova `worktree-agent-a8d4733e8156e2a1b`; Ambient
  `worktree-agent-aedeac7d79de88df6`. Their renders: app/build/new/{impressionism,lead-*}-2026-10-29/ (INDEX.md each).
- Names: only the genre names in code, comments, UI, docs, files and commits written by this plan: Impressionism,
  Romanticism, '60s Jazz, Modal Jazz, Jazz-Hop, Film Score, Bossa Nova, Ambient. Never the person.
- Lead instruments: flute, felt piano, sampled vibes, Knight. Never the same instrument on keys and lead.
- Families (Sam): the synth/acoustic divide applies to drums only.
- Pinned hashes move in this plan: each task that changes songs re-pins with a comment naming its task.
- Renders for Sam: approved parts only, approved kits only, WAV links with lead timestamps (m:ss spans; script in the
  session scratchpad, leadtimes.py, or the same arithmetic).
- Never stop the server on 127.0.0.1:8732; never delete in app/build/new except folders you create; never push or
  merge; history rewriting only in Task 9, after Sam confirms there.
- Commits: git email 884872+pianomansam@users.noreply.github.com, trailers
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>` and
  `Claude-Session: https://claude.ai/code/session_01ENYN74abncM7Wq3c4vd7md`.
- Tests: engine `PATH=$PWD/.venv/bin:$PATH sh tests/run_tests.sh`; workbench `sh workbench/build.sh`,
  `node workbench/tools/smoke.mjs`, `python3 workbench/tools/test_server.py`; patterns
  `python3 -I tools/patterns/test_patterns.py`; firmware size `./build.sh` (image ≤ 409600 B at the end).

## Review Focus

1. Chords with few legal notes (maj7 and Lydian chords, m9 over a walking bass) → every style still has 0 rubs and
   drops no more notes than its spike did.
2. A keys window that would push the keys below their instrument's range → the lead's band moves up instead; the keys
   never leave their range or break one-voicing-per-chord.
3. Old workbench snapshots naming lead_style 0/1 (phrase planner, hybrid) or a removed lead instrument → restore onto
   a stand-in style or instrument, with the message retired options use.
4. A style forced in the workbench onto an instrument it has fit 0 for → it still plays, legal; only the roll avoids
   it.
5. Flash still over budget after the removals → stop and bring Sam the measured trims; never cut silently.

---

### Task 1: The lead framework and the keys' room

**Files:** Create `app/song/leads/leads.h`, `app/song/leads/leads.c`; Modify `app/song/lead.c` (dispatch to the
framework for the new styles), `app/song/harmony.c`/`.h` (keys window), `app/song/override.h` (lead_style range);
Test `tests/host/song_test.c`.

**Interfaces:**
- Produces:
  - `enum { LS_IMPRESSIONISM, LS_ROMANTICISM, LS_JAZZ60S, LS_MODAL, LS_JAZZHOP, LS_FILMSCORE, LS_BOSSA, LS_AMBIENT, LS_N };`
  - `typedef struct { uint8_t n; uint8_t at[8], len[8]; int8_t step[8]; uint8_t contour; } lead_motif_t;` (on-grid
    16th onsets/lengths within its bars, scale steps from the first note, contour bits): every style keeps its motif
    in this form (the future hybrid handoff).
  - `int leads_center(int style, int ins)`: MIDI center per style × lead instrument, from each spike's measured median
    lead pitch in the five A/B songs (clamped to the instrument's range; the builder measures them from the spike
    renders' json and records the table with its source).
  - `void harmony_keys_window(int lo, int hi)` set once per song before voicing; percussive keys voice within it
    (voice-leading rules unchanged, one voicing per chord); sustained keys keep their placement path. If no legal
    voicing fits, the window is ignored and the lead's band moves up (Review Focus 2).
  - `void leads_plan_bar(...)` dispatch by style (signature mirrors `lead_plan_bar`), returning the same `lead_state_t`.
  - The lead's band = `leads_center ± 6`, replacing `c.lo = top + 5`.

- [ ] **Step 1: Failing tests:** `test_keys_window` (keys' top note ≤ window hi in every bar over 300 seeds with the
  lead playing; one voicing per chord holds; no rubs); `test_keys_window_fallback` (a window below the keys' range →
  keys unchanged, band moved up); `test_lead_motif_format` (a motif round-trips through lead_motif_t).
- [ ] **Step 2:** FAIL. **Step 3:** Implement. The old styles (phrase planner, hybrid) still run until Task 6; they use
  the band too. **Step 4:** Suite green; re-pin. **Step 5:** Commit `Lead framework: a shared motif, register centers,
  and the keys make room for the lead`.

### Tasks 2-5: The styles, two per task

Task 2: Impressionism + Romanticism. Task 3: '60s Jazz + Modal Jazz. Task 4: Jazz-Hop + Film Score. Task 5: Bossa
Nova + Ambient.

**Files (each):** Create `app/song/leads/<style>.c` (e.g. `impressionism.c`, `romanticism.c`, `jazz60s.c`,
`modal.c`, `jazzhop.c`, `filmscore.c`, `bossa.c`, `ambient.c`); register in `leads.c`; Test `tests/host/song_test.c`,
`tests/host/clash_test.c`.

**Interfaces:** Consumes Task 1. Produces each style's plan function in the dispatch, its motif in `lead_motif_t`, and
nothing else global (no debug counters or exports in firmware; workbench-only metrics go in workbench/engine/).

- [ ] **Step 1: Failing tests per style:** `test_<style>_rules` (0 rubs, strong beats legal, velocities 25-110 over
  300 seeds with the style forced, each lead instrument it fits); `test_<style>_motif` (phrase heads restating the
  motif's rhythm and contour at ≥ the spike's measured shares, from its research doc / INDEX.md); `test_<style>_shape`
  (notes per playing bar and range within its spike's measured ranges ±15%); **Review Focus 1:** a forced Lydian maj7
  and an m9-over-walking-bass song → 0 rubs, dropped notes ≤ the spike's.
- [ ] **Step 2:** FAIL. **Step 3:** Port from the spike (its `plan_*` and helpers), renamed to the genre name, debug
  arrays and counters removed. **Step 4:** Suite green, clash over all styles so far; re-pin. **Step 5:** Render the
  five A/B songs per style (same seeds/sets/kits as its spike INDEX.md) into
  `app/build/new/leads-build-<style>-<date>/` with INDEX.md and lead timestamps, for Sam's A/B against the spike.
  **Step 6:** Commit `Lead styles: <A> and <B>`.

### Task 6: Removals

**Files:** Delete the phrase planner and hybrid paths in `app/song/lead.c`/`phrase.c`, the struck-melody path
(`melody.c`/`.h` and its calls in `song.c`) if nothing else uses it, the lead-arp and melody-cells pattern families
(`patterns/lead-arp/`, `patterns/melody-cells/`, their tables in `patterns_gen.c`, export/import/validate support,
workbench rows `arpRhythm`/`arpPattern`), and the `lead_double` override and its row. Modify `override.h` (lead_style
-1 rolled or LS_*), `workbench/options.json`, `rows.js`, `desk.js` (snapshot restore mapping), tests.

- [ ] **Step 1: Failing tests:** nothing references the removed symbols (build); **Review Focus 3:** an old snapshot
  with lead_style 0/1 restores onto a rolled style with a "retired" message, one with lead_double restores ignoring it;
  pattern tests pass with the two families gone (import refuses files in removed families with a clear message).
- [ ] **Step 2:** FAIL. **Step 3:** Implement. **Step 4:** All suites green; re-pin. Record image bytes before/after in
  docs/cost.md. **Step 5:** Commit `Leads: the old engines, the melody path and the lead-arp patterns go`.

### Task 7: Sets, the style roll, families, the workbench

**Files:** Modify `app/song/library.c` (sets 5 → felt; 10 → flute; 13 → vibes; 17 → flute; 18 → felt; 19 → felt;
20 → felt; 28 → Knight; 29 → flute; 33 → vibes; src notes "(lead styles build: Sam, 2026-10-29)"), the lead-instrument
list (kalimba, Emilyguitar, saw lead and synth pluck retired as leads; Emilyguitar stays on keys), the style roll
(new stream), family logic (drums only), `workbench/options.json`, `rows.js`, `desk.js`; tests.

**Interfaces:** `int song_lead_style(const song_roll_t *r)` (rolled per song; override wins). Weights = instrument fit
× flavor weight, exactly the spec's table ("Decided by Claude": style weights and flavor weights).

- [ ] **Step 1: Failing tests:** over 2000 songs each style's share is within ±25% of its expected share from the
  weights; fit-0 styles never rolled for an instrument; no set has the same instrument on keys and lead; lead totals
  flute 10, felt 8, vibes 7, Knight 4 sets; families choose the kit only (keys/bass/pad/lead unchanged by a family
  change in the workbench); **Review Focus 4:** a workbench-forced fit-0 style plays and passes the rules. Smoke: the
  lead-style row lists the eight; re-roll of the lead part works; feedback replay restores the style.
- [ ] **Step 2:** FAIL. **Step 3:** Implement. **Step 4:** All suites green; re-pin. **Step 5:** Commit `Sets and the
  style roll: eight lead styles on four lead instruments; families for drums only (Sam)`.

### Task 8: Flash

**Files:** `docs/cost.md`; trims only as approved.

- [ ] **Step 1:** `./build.sh`; record the image size, and the bytes per style (map file).
- [ ] **Step 2:** If ≤ 409600 B: record and commit `Cost: the lead styles' flash`. **Review Focus 5:** if over, STOP
  and report to the controller a table of candidate trims with measured bytes (no changes).

### Task 9: The name scrub

**Files:** every file and commit on `workbench` (unpushed) mentioning a lead-style source person; the research docs
(docs/research/impressionism-lead-*, lead-*-2026-10-29.md) renamed to genre names and rewritten to describe traits without
the person or their pieces; the not-kept styles' docs (Neoclassical, Cool Jazz, Post-Bop) rewritten under genre names or
removed (Post-Bop's v3 rules stay in the ledger as "Post-Bop"); Sam's quotes in the ledger reworded; render folders
and files under app/build/new renamed; spike branches and worktrees deleted.

- [ ] **Step 1:** Build the name list (every lead-style source, kept or not) and a check script that greps the tree
  and `git log -p` of the unpushed range for them; it lists hits (not yet zero).
- [ ] **Step 2:** Rewrite the current files; commit `Scrub: lead styles by genre name only`; the tree check passes.
- [ ] **Step 3:** STOP: show Sam the plan for the history rewrite (git filter-repo over the unpushed commits of
  `workbench`, the replacement map, a backup ref, and that nothing pushed changes); proceed only on his OK.
- [ ] **Step 4:** Rewrite history; delete the spike branches and worktrees; the history check passes; the suites pass
  on the rewritten HEAD.
