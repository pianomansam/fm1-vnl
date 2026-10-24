# Sustained Lead Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the sustained lead's skeleton line with phrases: a per-song motif, an 8-bar question and answer, wind
phrasing, and motion by rank that holds notes longer.

**Architecture:**
- A new `app/song/phrase.c/.h` holds the pure functions:
  - the motif generator (seed → rhythm, contour and anchor/ornament tags)
  - the phrase planner (block, pass and bar → that bar's planned notes as scale-degree steps, with question,
    restatement, nudge, closing and arrival roles)
- `lead.c`'s `plan_line` is replaced by a thin adapter. It re-anchors the planned degrees to each chord through the
  existing `legal()` and register rules, applies motion by rank, and fills `lead_state_t.on/len`.

**Tech Stack:** C99, float only, no libm, no malloc; host tests in `tests/host/song_test.c`
(`PATH=$PWD/.venv/bin:$PATH sh tests/run_tests.sh`); renders with `./new-song.sh`.

**Spec:** `docs/superpowers/specs/2026-10-24-sustained-lead-design.md`, including its "Calls made for the builder".

## Global Constraints

- The song engine is a pure function of roll and plan. The motif and variations are pure functions of (song seed,
  lead block, pass number). Nothing new is stored beyond what `lead_state_t` holds.
- Music rules (from the clash test and the lead design): chord tones on steps 0 and 8; no semitone rubs with the
  keys or bass; no avoid notes; the lead's register (60–88; the floor over percussive keys; the pad placements);
  notes tied over a bar line legal over both chords.
- The lead-over-keys level rule holds (per-set mean 4–6.5 dB, median 4–6), and the keys duck stays.
- The struck/plucked hybrid (`plan_hybrid`) is unchanged.
- Commits: author `884872+pianomansam@users.noreply.github.com`; trailers
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>` and
  `Claude-Session: https://claude.ai/code/session_01ENYN74abncM7Wq3c4vd7md`. Never push.
- Sam's standing rule: when work completes, deliver clickable render links (with and without the bed).

## Review Focus

1. **A short lead block** (4 bars, or not a multiple of 4): the question alone, or the answer if it's the last lead
   block, must still end with an arrival or question and a breath, never mid-motif. Test in Task 2.
2. **Restatement over different chords:** bars 5–6 re-anchor the motif to chords that differ from bars 1–2. The rhythm
   must stay identical and the contour shape mostly identical, with no rub. Test in Task 2.
3. **A tie into a chord change with a key request landing:** a tied note crossing into a bar where a request lands
   must not ring into the new key (the existing `can_push` rule). Test in Task 2.
4. **Low motion with long notes:** held-longer notes must not run past the breath or into the next phrase. Test in
   Task 2.
5. **Determinism:** the same seed renders the same lead, alone or after another song (the existing carryover test
   covers it if a sustained-lead pair is included). Task 2.

---

### Task 1: The motif and the phrase planner (pure)

**Files:** Create `app/song/phrase.h`, `app/song/phrase.c`; Test: `tests/host/song_test.c`

**Interfaces:**
- Produces:
  - `typedef struct { uint8_t n; uint8_t at[8]; uint8_t len[8]; int8_t step[8]; uint8_t orn[8]; uint8_t bars; } motif_t2;`
    Onsets in 8ths from the motif's start, lengths in 8ths, scale steps from the previous note (step[0] is from the
    anchor), and ornament flags.
  - `void phrase_motif(uint32_t seed, motif_t2 *m);`
  - `typedef struct { uint8_t on; uint8_t at; uint8_t len; int8_t deg; uint8_t role; uint8_t orn; } phrase_note_t;`
    with `role` one of `PR_MOTIF`, `PR_VARIED`, `PR_QUESTION`, `PR_CLOSE`, `PR_ARRIVAL`.
  - `int phrase_bar(uint32_t seed, int block, int pass, int bar_in_phrase, int phrase_bars, int last_block, phrase_note_t *out, int max);`
    Returns the bar's planned notes. `deg` is a scale degree relative to the phrase's anchor; `at` and `len` are in
    16ths, and `len` may run past the bar.

- [ ] **Step 1: Write the failing tests** `test_phrase_plan`, over 500 seeds:
  - **Motif:** 3–6 onsets over 1–2 bars; lengths only from {2, 3, 4, 6, 8} 8ths; the first onset off beat 1 in 55–75%
    of seeds; at most one leap of ≥ 3 scale steps, followed by a step back in the other direction in ≥ 70% of seeds.
  - **8-bar plan:**
    - Bars 5–6 equal bars 1–2 (rhythm and degrees).
    - Bars 3 and 7 differ from 1 and 5 by exactly one nudge each.
    - Bar 4 ends on a `PR_QUESTION` note held ≥ 4 16ths, followed by ≥ 8 16ths of rest.
    - Bar 8 ends on `PR_ARRIVAL`, held ≥ 4 16ths, followed by ≥ 8 16ths of rest, approached by step.
  - **Wind density:** onsets per bar ≤ 5 everywhere, mean ≤ 3.5; beat 1 rests in ≥ 1 of every 2 bars.
  - **Short block (Review Focus 1):** `phrase_bars` 4 gives the question (or the answer when `last_block`), ending in
    a breath.
  - **Pure:** the same inputs give the same notes, called in any order.
- [ ] **Step 2: Run them to see them fail** (`phrase.h` missing).
- [ ] **Step 3: Implement `phrase.c`.**
  - Seeded with the project's `rng_t` streams. Use new stream numbers 180–189 (unused today; check `rng_seed`
    callers).
  - The nudges: ±1 step on one note, alter the last interval, invert the second half's contour, or displace one note
    by an 8th.
  - The arc: degrees rise toward bars 3–4 and settle to the arrival.
- [ ] **Step 4: Run all tests; all pass.**
- [ ] **Step 5: Commit:** `feat(song): a motif and an 8-bar question-and-answer phrase plan`

### Task 2: The sustained lead plays the phrase

**Files:** Modify `app/song/lead.c` (replace `plan_line`), `app/song/lead.h` if needed; Test:
`tests/host/song_test.c`, `tests/host/clash_test.c` (unchanged rules must keep passing)

**Interfaces:** Consumes `phrase_motif` and `phrase_bar` (Task 1). Produces no new public API; `lead_plan_bar`'s
signature is unchanged.

- [ ] **Step 1: Write the failing tests** over rolled songs pinned to the sustained-lead sets (sax, flute, trumpet,
  saw lead) via `roll_set`:
  - `test_sustained_phrases`:
    - In every lead block of ≥ 8 bars, the restatement's rhythm is identical to the motif's, and its contour is
      identical after snapping in ≥ 90% of restatements (Review Focus 2).
    - Bar 4 ends on the chord's 3rd or 5th, held ≥ a half note.
    - Bar 8 ends on a stable tone: the root or 3rd when the chord is the progression's first chord, otherwise the
      root.
    - A breath of ≥ half a bar follows bars 4 and 8, and no held note runs into it (Review Focus 4).
  - **Wind phrasing:**
    - mean onsets per bar ≤ 3.5
    - phrase starts off beat 1 in ≥ 55%
    - ≥ 70% of leaps of a 4th or more are followed by a step back
    - beat 1 rests at least every second bar
  - `test_sustained_motion`:
    - Same seed at motion 30 and motion 70: the low-motion onsets are a subset of the high-motion onsets.
    - Per phrase, low motion has fewer onsets and a sounding time within 10% of high motion's (Sam: fewer notes,
      held longer).
  - **Rules (existing tests must still pass):**
    - the clash test
    - `test_lead_push_legal`
    - `test_pushes`, including a tie into a bar where a request lands (Review Focus 3)
    - `test_lead_over_keys`
    - `test_song_carryover`, with a sustained-lead pair added (Review Focus 5)
- [ ] **Step 2: Run them to see them fail** against the old skeleton line.
- [ ] **Step 3: Implement**
  - Replace `plan_line`. Map each planned degree to a pitch: anchor the phrase on a chord tone near the lead's
    register center, step through the scale from it, snap strong-beat notes to the nearest chord tone, and keep
    weak-beat scale steps only when `legal()`; drop a note only when nothing is legal.
  - Notes tied over a bar line use the existing ring and pad mechanism and `lo_x`.
  - **Motion:** under 40, drop ornaments and extend the previous anchor through their time, never into a breath.
  - **Velocity:** an arc to the phrase's high point, softening into the arrival.
  - Keep the hybrid untouched.
- [ ] **Step 4: Run all tests; all pass.** Run `sh tools/cost-bench.sh --forced` with the standard list from
  docs/cost.md (expect no change; the lead is cheap) and `sh tools/mem-check.sh`.
- [ ] **Step 5: Render for Sam.** One song per sustained-lead set (Felt and trumpet, Nylon and sax, Neo-soul Wurli
  (sax), Felt and flute, Rhodes dusty (flute), Rhodes neo-soul (trumpet), and each synth set with the saw lead), with
  and without the bed. Use `SET=n` to pin, and a stub `open` on PATH. List them as clickable links, by set.
- [ ] **Step 6: Docs and commit.**
  - docs/music/lofi.md: the sustained lead's phrase form.
  - The song-engine ledger: done, plus "judge by ear" (motif length, nudges, breath length, arrival).
  - Commit: `feat(song): the sustained lead sings phrases: motif, question, answer, arrival`.
