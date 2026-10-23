# Lead and Accompaniment Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the lead an engine part that plays like its instrument, have the comping make room for it, and set the
keys/lead register by the keys' attack.

**Architecture:** A new `app/song/lead.c` holds the lead (the hybrid for percussive leads, the skeleton line for
sustained ones), lifted out of the spike in `song.c` with its state in `song_t`. `song_tick` voices the bar's chord
first (`harmony_prepare`), plans the lead's bar, then strikes the comping with a `busy` flag from the lead's density.
The keys' class picks the register rule: a floor above percussive keys, or a per-song pad placement.

**Tech Stack:** C99, float only, no libm, no malloc; host tests in `tests/host/` run by
`PATH=$PWD/.venv/bin:$PATH sh tests/run_tests.sh`; listening renders with `./new-song.sh [SEED]`.

**Spec:** `docs/superpowers/specs/2026-10-22-lead-accompaniment-design.md`

## Global Constraints

- C99, float only, no libm, no malloc; host build has `-Werror`.
- The song engine is a pure function of the roll and the plan: no file-scope mutable state; all lead state lives in
  `song_t`, cleared by `song_start`'s memset.
- New random choices use new seed streams (lead 140–149, pad placement 160) and add no roll fields.
- Music rules: no substitutions, rootless keys, one voicing per chord through the song, no semitone rubs anywhere
  (lead vs. sounding keys included), simplest key spelling.
- Lead notes stay in the chord-scale, never an avoid note, a chord tone on steps 0 and 8 (`tests/host/clash_test.c`).
- Every new rule has a numeric test over 300 songs (seeds chained as in `test_bass_and_roots`).
- Commits: author email `884872+pianomansam@users.noreply.github.com`; trailers
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>` and
  `Claude-Session: https://claude.ai/code/session_01ENYN74abncM7Wq3c4vd7md`. Never push.

## Review Focus

1. **A push across a chord change over the wrong chord.** A note pushed an 8th early sounds over the current chord:
   it must be legal over both (chord-scale, not avoid, no rub), else land on time. Test in Task 1.
2. **Pad "with"/"above" placements rubbing the lead.** Keys an octave up share the lead's range: the lead must skip
   any pitch a semitone from a sounding keys note. Covered by the clash test once its bounds move (Task 3).
3. **Floor with no room.** Percussive keys whose top is high leave few legal lead notes up to E6 (88): the note drops,
   never goes below the floor. Test in Task 3.
4. **Busy flag on the wrong bar.** The comping must thin in the bar the lead's onsets start in, with a 2-bar chord's
   second bar judged on its own. Test in Task 2.
5. **Two songs side by side.** `test_chain` runs two `song_t` at once; any leftover static in the lead breaks it.
   Covered by Task 1's move into `song_t` and the existing chain test.

---

### Task 1: The lead module, by class

**Files:**
- Create: `app/song/lead.h`, `app/song/lead.c`
- Modify: `app/song/library.h`, `app/song/library.c` (instrument class), `app/song/song.h`, `app/song/song.c`
  (remove the lead spike, call the lead), `app/app.c` (drop `VNL_LEADSPIKE`), `new-song.sh` (drop `LEAD`)
- Test: `tests/host/song_test.c`, `tests/host/clash_test.c` (lead bounds 62–86 become 60–88)

**Interfaces:**
- Produces:
  - `int lib_ins_sustained(int ins);` 1 for INS_PAD, INS_FLUTE, INS_SAX, INS_MUTED_TPT, INS_VOX_PAD; else 0.
  - `lead.h`: `typedef struct { ... } lead_state_t;` (every former `spike_*` static, plus a `uint32_t seed`).
  - `int lead_plan_bar(lead_state_t *ls, const song_roll_t *r, const song_plan_t *p, const plan_block_t *b, int bar, const int keys[4], const int *pcs, int np);`
    plans one bar; `keys` are the bar's sounding keys notes. It returns the number of lead onsets that start in this bar
    (0 where the lead doesn't play).
  - `int lead_step(lead_state_t *ls, int step, float step_samples, song_ev_t *out, int max);`
  - `song_t` gains `lead_state_t lead;`. `song_set_lead_spike` is removed.

- [ ] **Step 1: Write the failing tests** in `song_test.c`, over 300 chained songs through `song_tick`:
  - `test_lead_by_class`:
    - In songs whose lead instrument is sustained, count arpeggio runs. A run is 3 or more consecutive lead onsets,
      each at most 2 steps after the one before, all chord tones, with 3 or more distinct pitch classes. Assert 0 runs.
    - In songs whose lead instrument is percussive, assert at least 80% of keys bars in non-intro/outro blocks have a
      lead onset.
  - `test_lead_where`:
    - Lead onsets: 0 in intro and outro blocks, 0 in blocks without `L_KEYS`.
    - Candy onsets: 0 in blocks where the lead may play.
  - `test_lead_push_legal`: every lead note that sounds across a chord change is in the chord-scale of, and not an
    avoid note of, both chords (`th_is_avoid`).
  - Replace the asserts in the old melody-lead test (around line 1057, via `melody_step`) that no longer describe the
    song's lead with these tests. Keep `melody_step`'s own determinism checks.

- [ ] **Step 2: Run them to see them fail**
  - Run: `PATH=$PWD/.venv/bin:$PATH sh tests/run_tests.sh`
  - Expected: song FAIL. The melody lead arpeggiates in some sustained-lead songs, and candy plays in lead blocks.

- [ ] **Step 3: Implement**
  - Move spike modes 3 (line) and 5 (hybrid) from `song.c` into `lead.c`. Percussive leads use the hybrid, sustained
    leads the line, chosen by `lib_ins_sustained(set->lead)`.
  - Keep the spike's seed streams (140–142) and hashes, so percussive-lead songs keep the rhythms Sam heard.
  - Delete modes 1, 2 and 4.
  - Where the lead plays: every block with `L_KEYS` except intro and outro. `song_tick` drops `melody_step`'s
    V_LEAD and V_CANDY there.
  - A pushed note that isn't legal over the current chord lands on time instead.
  - Non-chord tones (passing and neighbor notes) never fall on steps 0 or 8.

- [ ] **Step 4: Run the tests; all pass**
  - Run: `PATH=$PWD/.venv/bin:$PATH sh tests/run_tests.sh`
  - Expected: `all tests passed`, including `test_chain` (two songs side by side).

- [ ] **Step 5: Commit:** `feat(song): the lead by instrument class: hybrid arpeggio or sung line`

### Task 2: The comping makes room

**Files:**
- Modify: `app/song/harmony.h`, `app/song/harmony.c`, `app/song/song.c`
- Test: `tests/host/song_test.c` (update the `harmony_step` callers at lines ~878, 941, 964)

**Interfaces:**
- Consumes: `lead_plan_bar` (Task 1).
- Produces:
  - `void harmony_prepare(const song_roll_t *r, int bar, int pass, harmony_voicing_t *vs, int out4[4]);` voices the
    bar's chord into `vs->last` once (the loop voicing when locked), writing the sounding notes.
  - `harmony_step(..., harmony_voicing_t *vs, int busy, uint32_t salt, song_ev_t *out, int max)`: a new `busy` argument
    before `salt`. It strikes `vs->last` and no longer voices.

- [ ] **Step 1: Write the failing test** `test_comping_makes_room`, over 300 songs:
  - Scope: songs with percussive keys and stabs, offbeats or arpeggio comping.
  - Measure the mean keys strikes per bar in bars where the lead has 4 or more onsets (busy) and in bars with 0 or 1
    (quiet). Assert busy ≤ ⅔ × quiet for each of the three styles.
  - Assert held-comping songs and sustained-keys songs strike exactly as with `busy` = 0.
  - Assert the voicing in every bar equals the voicing when no bar is busy.

- [ ] **Step 2: Run it to see it fail** (busy and quiet bars strike alike).

- [ ] **Step 3: Implement**
  - `song_tick` at step 0 runs `harmony_prepare`, then `lead_plan_bar`.
  - `busy` = the returned onsets ≥ 4, held for the bar.
  - While busy:
    - stabs strike on step 0 only
    - offbeats strike on steps 6 and 14 only
    - the arpeggio strikes on even steps only
  - Held comping and sustained keys ignore `busy`.

- [ ] **Step 4: Run all tests; all pass.**

- [ ] **Step 5: Commit:** `feat(song): the comping thins while the lead is busy`

### Task 3: Register by the keys' attack

**Files:**
- Modify: `app/song/harmony.h`, `app/song/harmony.c`, `app/song/lead.c`
- Test: `tests/host/song_test.c`, `tests/host/clash_test.c` (chord bounds 48–76 become up to 88 when the keys are
  sustained and placed with or above)

**Interfaces:**
- Produces:
  - `enum { PL_BELOW, PL_WITH, PL_ABOVE };`
  - `int harmony_pad_place(const song_roll_t *r);` PL_* from seed stream 160, equal weights, for sustained keys.
    Always PL_BELOW for percussive keys.
  - `harmony_prepare`'s `out4`, and every strike, are shifted +12 for PL_WITH and PL_ABOVE. `vs->last` stays unshifted.

- [ ] **Step 1: Write the failing tests:**
  - `test_register_floor`, percussive keys:
    - Every lead note is at least the sounding keys' top note + 5, checked against every chord the note sounds over.
      Assert 0 violations.
    - Lead notes ≤ 88.
    - The share of lead notes with the keys' top pitch class is ≤ 15% (today 25%: measure it, and record the
      measured value in the commit message).
  - `test_pad_places`, sustained keys:
    - Each placement appears in 33% ± 8% of those songs.
    - Under PL_ABOVE, lead notes ≤ 72.
  - Move the clash test's bounds as listed under Files.

- [ ] **Step 2: Run them to see them fail.**

- [ ] **Step 3: Implement**
  - **Floor (percussive keys):**
    - The lead places each tone's pitch class in the lowest octave ≥ floor, at most 88. If none fits, the note drops.
    - Floor = the highest sounding keys note, over every chord the note sounds over, + 5.
  - **Color lean:** the hybrid's four tones are the voicing's pitch classes. The top voice's pitch class is replaced by
    the chord's highest color tone (13th, 9th, 7th, in that order) missing from the voicing, if there is one.
  - **Sustained keys:**
    - The lead ranges 60–83, or 60–72 under PL_ABOVE.
    - The lead skips any pitch a semitone from a sounding keys note.

- [ ] **Step 4: Run all tests; all pass.**

- [ ] **Step 5: Commit:** `feat(song): lead and keys registers by the keys' attack`

### Task 4: The sung line wanders

**Files:**
- Modify: `app/song/lead.c`
- Test: `tests/host/song_test.c`

- [ ] **Step 1: Write the failing test** `test_line_wanders`, over 300 songs:
  - In sustained-lead songs, the skeleton notes (each chord's landing note) reach 4 or more distinct pitches in ≥ 80%
    of songs. Measure today's share first and put it in the commit message.
  - No skeleton step leaps more than a perfect 5th.

- [ ] **Step 2: Run it to see it fail** (song 1's line only alternated C5 and Db5).

- [ ] **Step 3: Implement**
  - The skeleton's reference pitch follows a per-phrase arc of +0, +3, +5, +2 semitones over the 4 bars, applied to
    the last note before choosing the nearest legal chord tone.
  - Scale-step passing notes fill between skeleton notes a 3rd or more apart, on weak steps only (not 0 or 8), under
    the no-rub rule.

- [ ] **Step 4: Run all tests; all pass.**

- [ ] **Step 5: Commit:** `feat(song): the sung line arcs over each phrase`

### Task 5: Never both silent; docs; renders for Sam

**Files:**
- Modify: `tests/host/song_test.c`, `docs/music/general.md` (the interaction rules, as general musicianship),
  `docs/superpowers/plans/2026-10-22-song-engine-ledger.md` (Leads: done items, and what to judge by ear)

- [ ] **Step 1: Write the test** `test_never_both_silent`, over 300 songs:
  - In every bar of a non-intro/outro block with `L_KEYS`, something sounds at some step: a keys note (a held chord
    counts) or a lead note. Assert 0 silent bars.

- [ ] **Step 2: Run it.** Expected PASS, since held chords sound through. If it fails, fix the cause in lead or
  comping, never the test.

- [ ] **Step 3: Write the docs.**
  - `docs/music/general.md`: the lead leads, the comping makes room, and the register rule set by the keys' attack.
  - The ledger: move the lead and accompaniment items to done, and add "judge by ear with real instruments in Plan 3".

- [ ] **Step 4: Render for Sam.** Run `./new-song.sh` for three seeds that cover one percussive-lead song, one
  sustained-lead song, and one pad song (find them by rolling seeds and reading the log header). List the three seeds
  and their folders in the report.

- [ ] **Step 5: Commit:** `docs: lead and accompaniment rules; test: never both silent`
