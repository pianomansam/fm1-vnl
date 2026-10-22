# VNL-1 Song Engine Implementation Plan (Plan 1 of 5)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** VNL-1 plays an endless run of generated lo-fi songs (rolled choices, song maps, one chord loop, groove,
generated melody, novelty between songs) through placeholder voices, in the host simulator, the browser emulator and
on the FM-1.

**Architecture:** A pure-C song engine in `app/song/` (no platform calls, testable with plain `cc`) turns a seed into a
*roll* (every choice), the roll into a *plan* (blocks of bars with which layers play), and the plan into note events,
one 16th step at a time. Rolling happens on the main loop; stepping and rendering happen in the audio interrupt
(AMB-1's model: commands posted to the render, drained at the start of a block). `app/engine/` holds deliberately
simple placeholder voices; Plan 3 replaces them.

**Tech Stack:** C99, float only, fm1-base (`plat.h`, `fastmath.h`), fm1-base's host simulator and emcc build, POSIX
`sh` test runner.

**Spec:** `docs/superpowers/specs/2026-10-22-vnl-1-design.md`

**The other plans** (each written when its predecessor lands): 2 hardware bench (CPU per voice on the FM-1),
3 voices and lo-fi chain (Plaits/Rings, fallback rungs, dust, noise, ambience, automation arcs, swell), 4 curation
bench (browser tool; the hand-written tables of Task 3 move to data files + a generator), 5 playing it (screens D and
F, dice per choice, universal re-roller, saving, release).

## Global Constraints

- C99, `float` only (no `double`, no soft-double routines), no libm: math through `base/include/fastmath.h` (`fm_*`).
- No `malloc`; all state static, sized at compile time. Nothing in the render waits or loops unbounded.
- Every source file starts `/* SPDX-License-Identifier: MPL-2.0 */`. **No code from GPL projects** (DUB-1, LOF-1,
  HYP-1, MAKU, Tuna, Bunker Evolved, Jangada, Mini-Tone, TRIBE): ideas only. AMB-1 / fm1-base code (MPL) may be adapted.
- Deterministic: the same seed and the same requests give the same events, bit for bit, on host and device.
- `app/song/` includes only `<stdint.h>`, `<string.h>` and its own headers (no `plat.h`).
- Host test flags: `cc -O2 -std=c99 -ffp-contract=off -Wall -Wextra -Wdouble-promotion -Werror`.
- US English in names, comments and screen text.
- Spec values (copy exactly): tempo 70–90 BPM; song length 120–210 s; bars a multiple of 4; swing 52–62% (median
  ~55%); snare 10–30 ms late; hats up to 70 ms late, about half kick/snare velocity; chords 4 notes, bottom note
  near E3 (MIDI 52), within C3–E4 (48–64) except colors above; changes land on the next 4-bar boundary; silence
  between songs 2–4 s; drum break 4–8 bars, starting 43–65% through; lead phrases ~50% exact restatement, ~15%
  variant, ~35% new; progression mix ~40% vamp / 45% loop / 15% jazzy, 2–4 chords; novelty: at least 5 of the 9 main
  choices differ from the previous song (map, progression, instrument set, pattern family, mode, dust, surface noise,
  ambience, tempo by ≥ 6 BPM); the next key is related to the last (circle-of-fifths neighbor or relative).

## Review Focus

1. **A change requested late** (in the last step before a boundary, in the outro, or during the silent gap) lands on
   the next 4-bar boundary or is dropped with the song, never mid-bar, never carried into the next song. → Task 9.
2. **Tempo changed mid-song**: the song keeps its bar count, its sections stay aligned to bars, and it still ends. → Task 9.
3. **Extreme rolls**: 70 BPM with the longest map stays ≤ 210 s; 90 BPM with the shortest stays ≥ 120 s. → Task 4.
4. **A dice re-roll with few legal values** (novelty or mood excludes most) terminates and returns a different value
   whenever one exists. → Task 4.
5. **Stop mid-song, or a key change while notes sound**: no stuck notes; every voice releases. → Tasks 9, 10.

---

## File Structure

| Path | Responsibility |
| --- | --- |
| `app/song/rng.h`, `rng.c` | Seeded xorshift streams, one per choice (so changing one choice never shifts the others) |
| `app/song/theory.h`, `theory.c` | Modes, chords from scale degrees with colors, voicing, voice leading, chord/guide tones, avoid notes |
| `app/song/library.h`, `library.c` | The seed library (hand-written tables): progressions, instrument sets, drum pattern families, song maps, mood table |
| `app/song/roll.h`, `roll.c` | `song_roll_t`: every choice; rolling with novelty, key flow and length rules; dice per choice |
| `app/song/plan.h`, `plan.c` | A roll + its map → blocks of bars with layer masks, sections and events (break, fills, drops) |
| `app/song/groove.h`, `groove.c` | Drums and bass for one 16th step: pattern, swing, lateness, velocity, ghosts, bass styles |
| `app/song/harmony.h`, `harmony.c` | The chord for a bar (with reharmonize-on-repeat, dark intro/outro voicing) and the chord part's notes per step |
| `app/song/melody.h`, `melody.c` | The motif, phrase plan (head–solo–head, restate/variant/new), lead and ear-candy notes, turn taking |
| `app/song/song.h`, `song.c` | The sequencer: steps the plan, emits events, applies requests at boundaries, ends songs, the gap |
| `app/engine/voices.h`, `voices.c` | Placeholder voices and the mix (Plan 3 replaces) |
| `app/app.c`, `app/app.h`, `app/fm1.toml` | Wiring to the platform: rolling on the main loop, render, minimal screen, knobs, PLAY, host expects |
| `tests/host/song_test.c` | Unit tests for `app/song/` |
| `tests/host/clash_test.c` | Many rolled songs checked for clashes, register, length, novelty, determinism |
| `tests/scenarios/*.vnl` | Host simulator scripts |
| `tests/run_tests.sh` | Runs all of the above and fm1-base's build |

Shared types live in the header of the module that owns them; the Interfaces blocks below name them exactly.

---

### Task 1: Test runner and seeded random streams

**Files:**
- Create: `app/song/rng.h`, `app/song/rng.c`, `tests/host/song_test.c`, `tests/run_tests.sh`

**Interfaces:**
- Produces:
  - `typedef struct { uint32_t s; } rng_t;`
  - `void rng_seed(rng_t *r, uint32_t seed, uint32_t stream);` — distinct, nonzero states per (seed, stream)
  - `uint32_t rng_next(rng_t *r);` — xorshift32 (13, 17, 5)
  - `float rng_float(rng_t *r);` — [0, 1)
  - `int rng_int(rng_t *r, int n);` — [0, n), 0 when n ≤ 0
  - `int rng_weighted(rng_t *r, const uint8_t *w, int n);` — index by weight; −1 when all weights are 0
  - `tests/host/song_test.c`: `CHECK(cond, fmt, ...)` macro counting failures (AMB-1's style), `main` returns 1 on any failure.
  - `tests/run_tests.sh`: AMB-1's `run NAME CMD` pattern; runs `song`, `clash` (Task 9), scenarios (Task 11), `host-build`, `emu` if `emcc` exists.

- [ ] **Step 1: Write the failing tests** in `song_test.c`: `test_rng()` — same (seed, stream) gives the same 1000
  values; streams 0 and 1 of one seed differ in their first value; `rng_float` stays in [0,1) over 100000 draws;
  `rng_int(r, 7)` hits all 7 values in 1000 draws; `rng_weighted` with weights `{0, 3, 1}` never returns 0 and returns
  1 about 75% (±5%) of 10000 draws; all-zero weights return −1.
- [ ] **Step 2: Run** `sh tests/run_tests.sh` — Expected: `FAIL song` (rng.c missing).
- [ ] **Step 3: Implement `rng.c`.** Seeding: mix seed and stream (e.g. `seed * 0x9E3779B1 ^ stream * 0x85EBCA77`),
  then a few `rng_next` rounds; force nonzero.
- [ ] **Step 4: Run** `sh tests/run_tests.sh` — Expected: `ok song`.
- [ ] **Step 5: Commit** `feat(song): seeded random streams and the test runner`.

### Task 2: Music theory

**Files:**
- Create: `app/song/theory.h`, `app/song/theory.c`; Test: `tests/host/song_test.c`

**Interfaces:**
- Produces:
  - `enum { MODE_IONIAN, MODE_DORIAN, MODE_PHRYGIAN, MODE_LYDIAN, MODE_MIXOLYDIAN, MODE_AEOLIAN, MODE_N };`
  - `uint16_t th_mode_pool(int mode);` — bit i: root + i semitones in the mode
  - `enum { COLOR_7, COLOR_9, COLOR_RICH };` — 7ths; 9ths; 9/11/13 where the mode allows
  - `typedef struct { int8_t degree; int8_t alter; uint8_t quality; } th_chord_spec_t;` — degree 0–6 in the mode;
    `alter` in semitones for borrowed/chromatic chords (e.g. iv-minor, tritone sub); `quality`: `Q_DIATONIC`,
    `Q_MIN7`, `Q_DOM7`, `Q_MAJ7` (forced quality for borrowed chords)
  - `int th_chord_pcs(int key, int mode, th_chord_spec_t c, int color, int *pcs);` — absolute pitch classes, root
    first, 4–5 tones (root, 3rd, 5th/omit, 7th, color tone); returns count
  - `void th_voice(const int *pcs, int n, const int *prev, int *out4);` — 4 notes, bottom note within 48–55
    (centered on E3 = 52), top ≤ 72, root not required in the voicing (rootless allowed), moving each voice as little
    as possible from `prev` (prev NULL: closest to a reference E3–C4 voicing). AMB-1's `en_voice_lead` (MPL) may be adapted.
  - `void th_guide_tones(const int *pcs, int n, int *third, int *seventh);`
  - `int th_is_avoid(int key, int mode, const int *pcs, int n, int note);` — 1 for a note a minor 9th above a chord
    tone (e.g. the 4th over maj7, the b9 over min7), else 0
  - `int th_in_scale(int key, int mode, int note);`
  - `const char *th_chord_name(const int *pcs, int n, char *buf);` — "Dm9", "G13", "Cmaj9", "Fm7", "A7" (≤ 8 chars)

- [ ] **Step 1: Write the failing tests** `test_theory()`: C Ionian degree 1 (ii) COLOR_9 → pcs {2,5,9,0,4} and
  name "Dm9"; degree 4 (V) COLOR_RICH → name "G13"; degree 0 COLOR_9 → "Cmaj9"; iv-minor borrowed in C major
  (`{3, 0, Q_MIN7}`) → "Fm7"; `th_voice` bottom note in 48–55 and top ≤ 72 for every degree and color in all 6 modes
  and 12 keys; voice leading from Dm9 to G13 moves no voice more than 4 semitones; `th_is_avoid(C, Ionian, Cmaj7, F4)`
  = 1 and `(…, E4)` = 0; guide tones of G7 are B and F.
- [ ] **Step 2: Run** `sh tests/run_tests.sh` — Expected: FAIL song (theory functions undefined).
- [ ] **Step 3: Implement `theory.c`.**
- [ ] **Step 4: Run** — Expected: ok song.
- [ ] **Step 5: Commit** `feat(song): modes, chords with colors, voicing and voice leading`.

### Task 3: The seed library

**Files:**
- Create: `app/song/library.h`, `app/song/library.c`; Test: `tests/host/song_test.c`

**Interfaces:**
- Consumes: `th_chord_spec_t`, `MODE_*` (Task 2)
- Produces (every entry carries a stable `uint8_t id`, never reused, and a `const char *src` naming its source):
  - Layers (bits, used by maps and the plan): `L_KEYS, L_KICK, L_SNARE, L_HATS, L_BASS, L_LEAD, L_CANDY, L_BED`
    (`L_BED` reserved for Plan 3).
  - `typedef struct { uint8_t id; uint8_t kind; uint8_t n; uint8_t bars_per_chord; th_chord_spec_t c[4]; uint8_t moods; const char *src; } lib_prog_t;`
    `kind`: `PK_VAMP`, `PK_LOOP`, `PK_JAZZY`; `n` 2–4; `moods`: bit mask of moods it suits (`MOOD_SAD, MOOD_WISTFUL, MOOD_WARM, MOOD_SUNNY`)
  - `typedef struct { uint8_t id; const char *name; uint8_t chords, bass, lead, candy, kit; uint8_t flavor; uint8_t moods; const char *src; } lib_set_t;`
    instrument ids are names for Plan 3 (`INS_RHODES, INS_WURLI, INS_FELT_PIANO, INS_NYLON, INS_PAD, INS_UPRIGHT,
    INS_ELEC_BASS, INS_SUB, INS_FLUTE, INS_MUTED_TPT, INS_SAX, INS_VIBES, INS_KALIMBA, INS_GUITAR_LICK, INS_MUSIC_BOX,
    INS_BELLS, INS_VOX_PAD, INS_NONE`; kits `KIT_DUSTY, KIT_SOFT, KIT_808, KIT_BRUSHES`); `flavor`: `FL_LOFI,
    FL_BOSSA, FL_CITYPOP, FL_NEOSOUL, FL_CHILLWAVE`
  - `typedef struct { uint8_t id; uint8_t family; uint16_t hit[6]; uint16_t ghost[6]; uint8_t half_time; const char *src; } lib_pattern_t;`
    lanes `D_KICK, D_SNARE, D_HAT, D_OHAT, D_SHAKER, D_RIM` (16 steps, bit i = step i); `family`: `PF_BOOMBAP,
    PF_HALFTIME, PF_SPARSE, PF_BRUSHES, PF_BOSSA`; a shaker of period 3 (polymeter) is a pattern flag `poly3` on the
    shaker lane (`uint8_t poly3`)
  - `typedef struct { uint8_t id; const char *name; uint8_t intro_bars, entry, middle, ending; uint8_t sparse; const char *src; } lib_map_t;`
    `entry`: `EN_TOGETHER` (drums+bass together), `EN_SNARE_FIRST` (snare+hats 8 bars before kick+bass), `EN_FROM_TOP`;
    `middle`: `MID_BREAK` (one drum break), `MID_KICK_OUT` (8 bars kick+bass out), `MID_HATS_ONLY` (small hat drops),
    `MID_NONE`; `ending`: `END_KEYS_OUTRO`, `END_FADE`, `END_RING`
  - `extern const lib_prog_t LIB_PROGS[]; extern const int LIB_NPROGS;` and the same for `LIB_SETS`, `LIB_PATTERNS`, `LIB_MAPS`
  - `typedef struct { uint8_t mode_w[MODE_N]; uint8_t color_w[3]; int8_t lead_shift; uint8_t candy_pct; } lib_mood_t;`
    `extern const lib_mood_t LIB_MOODS[4];` (sad → sunny)
  - `const lib_prog_t *lib_prog_by_id(uint8_t id);` (and `_set_`, `_pattern_`, `_map_`) — NULL if unknown

**Content (the "good guesses"; source noted per entry):**
- Progressions, at least 14: vamps (ii9–V13; IVmaj7–iii7; i9–iv9; Imaj7–IVmaj7), loops (IVmaj7–iii7–ii7–Imaj7;
  ii9–V13–Imaj9–vi7; IVmaj7–V7–iii7–vi7 "royal road"; vi9–ii9–V13–Imaj9; Imaj9–vi9–ii9–V13), jazzy (IVmaj7–iv7–iii7–VI7;
  ii9–bII7–Imaj9 tritone sub; Imaj7–I7–IVmaj7–iv7), bossa (ii7–V7–Imaj7–VI7) and city pop (IVmaj7–V–iii–vi) flavors;
  kinds in roughly 40/45/15 proportion. Sources: `docs/research/lofi-hip-hop.md` §2, the brainstorm.
- Instrument sets, at least 10, curated so roles sit in separate registers: e.g. Rhodes + upright + flute + music box
  + dusty; nylon + sub + vibes + kalimba + brushes (bossa); Wurli + electric bass + sax + bells + soft (neo-soul);
  felt piano + upright + muted trumpet + none + dusty; pad + sub + kalimba + vox pad + 808 (chillwave); bright Rhodes +
  electric bass + guitar lick + bells + soft (city pop).
- Patterns, at least 8: boom-bap ×3 (kick on 1 + 1–3 offbeats, snare 2 and 4, 8th hats), half-time ×2 (snare on 3),
  sparse ×1, brushes ×1 (ride/brush on the hat lane, rim on the snare lane), bossa ×1; one with `poly3` shaker.
  Source: `lofi-midi-arrangements.md` drum grids, Magenta patterns in `lofi-hip-hop.md` §6.
- Maps, exactly the spec's five: Classic (intro 4, EN_TOGETHER, MID_BREAK, END_KEYS_OUTRO), Slow bloom (intro 8,
  EN_SNARE_FIRST, MID_NONE, END_FADE), Straight in (intro 0, EN_FROM_TOP, MID_HATS_ONLY, END_RING), Breather (intro 4,
  EN_TOGETHER, MID_KICK_OUT, END_KEYS_OUTRO), Drift (intro 4, EN_TOGETHER, MID_NONE, END_FADE, `sparse` 1).
- Moods: sad (Aeolian/Dorian heavy, COLOR_9/RICH, lead −2), wistful (Dorian/Aeolian), warm (Ionian/Mixolydian), sunny
  (Ionian/Lydian, more candy). Phrygian weight 0 in all.

- [ ] **Step 1: Write the failing tests** `test_library()`: ids unique within each table; `lib_*_by_id` finds every
  id and returns NULL for 255; every progression has 2 ≤ n ≤ 4; vamp/loop/jazzy counts within ±10 points of 40/45/15;
  exactly 5 maps with the names above; every pattern's snare lane is nonzero; every set's five instrument ids are
  valid; every mood's mode weights sum > 0 and give Phrygian 0.
- [ ] **Step 2: Run** — Expected: FAIL song.
- [ ] **Step 3: Write the tables** in `library.c`.
- [ ] **Step 4: Run** — Expected: ok song.
- [ ] **Step 5: Commit** `feat(song): seed library of progressions, sets, patterns, maps and moods`.

### Task 4: The roll

**Files:**
- Create: `app/song/roll.h`, `app/song/roll.c`; Test: `tests/host/song_test.c`

**Interfaces:**
- Consumes: `rng_*` (Task 1), `MODE_*`, `COLOR_*` (Task 2), `LIB_*`, `lib_*_by_id`, `MOOD_*` (Task 3)
- Produces:
  - `enum { RF_KEY, RF_TEMPO, RF_MOTION, RF_MOOD, RF_MODE, RF_MAP, RF_PROG, RF_COLOR, RF_SET, RF_PATTERN, RF_SWING, RF_DUST, RF_NOISE, RF_AMBIENCE, RF_BARS, RF_N };`
  - `typedef struct { uint32_t seed; int16_t v[RF_N]; } song_roll_t;` — `v[RF_KEY]` 0–11, `RF_TEMPO` BPM 70–90,
    `RF_MOTION` 0–100, `RF_MOOD` 0–3, `RF_MAP/PROG/SET/PATTERN` library ids, `RF_COLOR` 0–2, `RF_SWING` per-mille
    520–620, `RF_DUST` 0–3, `RF_NOISE` 0 none / 1 vinyl / 2 tape, `RF_AMBIENCE` 0 none / 1 rain, `RF_BARS` 32–68,
    multiple of 4
  - `void roll_song(const song_roll_t *prev, uint32_t seed, song_roll_t *out);` — prev NULL for the first song. Order:
    mood → mode (mood weights) → key (prev: a circle-of-fifths neighbor or the relative, never the same pitch class;
    first song: any) → tempo (prev: differs by ≥ 6; first: triangular peak 80–84) → map, progression (filtered by
    mood bit, kind weights 40/45/15), set (mood bit), pattern, color (mood weights), swing (mean 555), dust, noise,
    ambience, motion (20–80) → bars from a target length rolled in 120–210 s (mode ~150 s), converted with
    `bars = 4 * round(secs * bpm / 960)`, then clamped so `bars * 240 / bpm` stays within 120–210 s. Then novelty:
    count differences against `prev` over {map, prog, set, pattern, mode, dust, noise, ambience, tempo≥6}; while
    fewer than 5, re-roll the matching choices (bounded: at most 16 passes; each choice draws from its own stream).
  - `int roll_dice(song_roll_t *r, int field, uint32_t salt);` — re-rolls one field to a different legal value
    (respecting mood filters for prog/set; tempo ≥ 2 BPM away) and re-derives dependent fields (`RF_PROG` keeps
    `RF_BARS`; `RF_TEMPO` re-clamps `RF_BARS`); returns 1 if it changed, 0 if no other legal value exists.
  - `int roll_set(song_roll_t *r, int field, int value);` — manual pick, validated and clamped; returns the value applied.
  - `int roll_seconds(const song_roll_t *r);` — `RF_BARS * 240 / RF_TEMPO`

- [ ] **Step 1: Write the failing tests** `test_roll()`: for seeds 1..2000 with each roll fed the previous one:
  tempo in 70–90; seconds in 120–210; bars a multiple of 4; ≥ 5 of the 9 novelty choices differ from the previous
  song; the key is related (difference of 5 or 7 semitones, or a relative major/minor pair under the two modes) and
  never equal; the progression and set suit the mood. Same seed + same prev → identical roll. **Review Focus 3:**
  `roll_set(RF_TEMPO, 70)` on a roll with the longest bars gives ≤ 210 s; `roll_set(RF_TEMPO, 90)` with the shortest
  gives ≥ 120 s. **Review Focus 4:** a library filtered to one progression for a mood → `roll_dice(RF_PROG)` returns 0
  without looping; with two, returns 1 and the other id; `roll_dice` on every field 100 times never returns the
  same value when it returns 1.
- [ ] **Step 2: Run** — Expected: FAIL song.
- [ ] **Step 3: Implement `roll.c`.** Use `rng_seed(&r, seed, field)` per field so re-rolling one field never shifts
  another (the spec's "change one choice" rule; Plan 5's saving depends on it).
- [ ] **Step 4: Run** — Expected: ok song.
- [ ] **Step 5: Commit** `feat(song): rolling songs with novelty, key flow, length rules and dice`.

### Task 5: The plan (song maps to blocks)

**Files:**
- Create: `app/song/plan.h`, `app/song/plan.c`; Test: `tests/host/song_test.c`

**Interfaces:**
- Consumes: `song_roll_t`, `RF_*` (Task 4); `lib_map_t`, `L_*` (Task 3); `rng_*`
- Produces:
  - `enum { SEC_INTRO, SEC_GROOVE, SEC_LEAD, SEC_BREAK, SEC_OUTRO };` — names "intro", "groove", "lead", "break", "outro"
  - `typedef struct { uint8_t start, bars; uint8_t section; uint8_t layers; uint8_t flags; } plan_block_t;`
    `flags`: `PB_FILL_LAST` (drum fill in the block's last bar), `PB_DROP_LAST` (1-bar drum drop on the last bar),
    `PB_DARK` (darker chord voicing, intro/outro), `PB_FADE` (fade over the block), `PB_SWELL` (a swell before the next
    block; sound in Plan 3)
  - `typedef struct { uint8_t n; uint8_t bars; plan_block_t b[24]; } song_plan_t;`
  - `void plan_build(const song_roll_t *r, song_plan_t *p);`
  - `const plan_block_t *plan_at(const song_plan_t *p, int bar);`
  - `const char *plan_section_name(int section);`

**Rules (from the spec's measured norms):** blocks start on multiples of 4 bars; intro = map intro bars, keys only
(+`PB_DARK`), `EN_FROM_TOP` has none; drums and bass enter together on a 4-bar boundary (`EN_SNARE_FIRST`: 8 bars of
snare+hats first); hats join 4–8 bars after kick and snare unless `EN_FROM_TOP`; the middle is 8-bar blocks; Motion
(0–100) sets how many optional moves happen (hats out 4–8 bars, 1-bar drops on the last bar of an 8-bar phrase,
lead blocks) — at Motion 0 only the map's skeleton moves; `MID_BREAK` places one 4–8 bar drum break starting 43–65%
through, keys carrying (bass or lead may too); `MID_KICK_OUT` one 8-bar block with kick+bass out, snare+hats carrying;
lead appears in about half the middle blocks, never in intro or break-without-keys; ear candy in other blocks
(never the same block as the lead's first statement); at most 2 layers change per boundary; outro per `ending`
(`END_KEYS_OUTRO`: drums leave 4–8 bars before the end, keys alone, `PB_DARK`); last block of `END_FADE` gets
`PB_FADE`; blocks cover exactly `RF_BARS`.

- [ ] **Step 1: Write the failing tests** `test_plan()`: for seeds 1..2000 × every map: blocks contiguous from 0 and
  sum to `RF_BARS`; every start a multiple of 4; no block has `L_LEAD` and starts at bar 0; the first block of a
  map with intro > 0 has layers == `L_KEYS` and `PB_DARK`; between adjacent blocks the layer mask differs in at most
  2 layers (counting kick+snare+hats as separate bits); `MID_BREAK` maps have exactly one block without
  kick/snare/hats in the middle, 4–8 bars, starting between 43% and 65% of `RF_BARS`; `END_KEYS_OUTRO` maps end with
  a keys-only block of 4–8 bars; Motion 0 vs 100 over 500 seeds: mean count of optional moves strictly greater at 100.
- [ ] **Step 2: Run** — Expected: FAIL song.
- [ ] **Step 3: Implement `plan.c`.**
- [ ] **Step 4: Run** — Expected: ok song.
- [ ] **Step 5: Commit** `feat(song): song maps to block plans`.

### Task 6: Groove (drums and bass)

**Files:**
- Create: `app/song/groove.h`, `app/song/groove.c`; Test: `tests/host/song_test.c`

**Interfaces:**
- Consumes: `lib_pattern_t`, `D_*`, `L_*` (Task 3), `song_roll_t` (Task 4), `plan_block_t` (Task 5), `rng_*`
- Produces:
  - Event type shared by Tasks 6–10 (declared in `groove.h`):
    `typedef struct { uint8_t voice, note, vel; uint8_t pad; uint32_t delay; uint32_t len; } song_ev_t;`
    `voice`: `V_CHORD, V_BASS, V_LEAD, V_CANDY, V_KICK, V_SNARE, V_HAT, V_OHAT, V_SHAKER, V_RIM, V_N`; `delay` and
    `len` in samples at 44100 Hz; `delay` ≥ 0 (late only); `note` MIDI (drums: 0); `vel` 1–127.
  - `int groove_step(const song_roll_t *r, const plan_block_t *b, int bar, int step, float step_samples, const int *chord_pcs, int npcs, uint32_t salt, song_ev_t *out, int max);`
    — the drum and bass events of one 16th step; returns how many. `chord_pcs` (root first) is the bar's chord,
    supplied by the caller (the sequencer gets it from Task 7's `harmony_chord_at`; tests pass a literal chord).
  - Bass styles chosen per song from the set and mood: `BS_ROOTS` (root on kick hits), `BS_ROOT_FIFTH`, `BS_WALK`
    (one block at most), `BS_PEDAL` (one held root per 2 bars; Drift and sad moods) — `int groove_bass_style(const song_roll_t *r);`
  - Bass register 28–47 (E1–B2); bass notes on the chord's root or fifth except `BS_WALK`'s passing notes.

**Values:** swing delays the off-16ths by `(RF_SWING/1000 − 0.5) * 2 * step_samples`; snare delay 10–30 ms
(per song a base in that range, ±3 ms per hit); hat delay 0–70 ms (per song base 0–40 ms, ±8 ms, clamped ≤ 70);
hats at ~0.5 of kick/snare velocity; ghost snares at 20–35 velocity; velocities 25–110 overall; fills in the
block's last bar when `PB_FILL_LAST` (extra snare/kick hits in the last 4 steps); with `PB_DROP_LAST` no drums in the
block's last bar; lanes play only when their layer bit is set (`L_KICK`, `L_SNARE` also covers rim, `L_HATS` covers
hat, open hat, shaker); `poly3` shaker hits every 3rd step counted across the bar line.

- [ ] **Step 1: Write the failing tests** `test_groove()`: with a boom-bap pattern at 85 BPM and swing 560, over 64
  bars: every snare delay in [10 ms, 30 ms]; every hat delay ≤ 70 ms; mean hat velocity between 0.4 and 0.6 of mean
  snare velocity; on-beat 16ths have zero swing delay and off-16ths have the expected swing delay (±1 sample) plus
  lateness; a block without `L_KICK` never emits `V_KICK`; `PB_DROP_LAST` emits no drum events in its last bar; bass
  notes in 28–47 and pitch class ∈ {chord root, chord fifth} except `BS_WALK`; `BS_PEDAL` emits one bass note per 2 bars.
- [ ] **Step 2: Run** — Expected: FAIL song.
- [ ] **Step 3: Implement `groove.c`.**
- [ ] **Step 4: Run** — Expected: ok song.
- [ ] **Step 5: Commit** `feat(song): swung drums with lateness and ghosts, bass styles`.

### Task 7: Harmony over time and the chord part

**Files:**
- Create: `app/song/harmony.h`, `app/song/harmony.c`; Test: `tests/host/song_test.c`

**Interfaces:**
- Consumes: `th_*` (Task 2), `lib_prog_t` (Task 3), `song_roll_t` (Task 4), `plan_block_t`, `PB_DARK` (Task 5), `song_ev_t` (Task 6)
- Produces:
  - `int harmony_chord_at(const song_roll_t *r, int bar, int pass, int dark, int *pcs);` — the chord's pitch classes
    for `bar`; `pass` = how many times the loop has played; **reharmonize on repeat**: on passes ≥ 2, with a per-song
    chance (25–50%), one chord of the pass gets a richer color or one substitution (tritone sub for a dominant, or
    iv-minor for IV); `dark` lowers the color one step and drops the top voice an octave (intro/outro)
  - `enum { CS_HELD, CS_STABS, CS_OFFBEATS, CS_ARP };` `int harmony_style(const song_roll_t *r);` — per song
  - `int harmony_step(const song_roll_t *r, const plan_block_t *b, int bar, int step, int pass, float step_samples, int *voicing_state, uint32_t salt, song_ev_t *out, int max);`
    — chord-part events for one step; chords are **rolled** (notes 10–60 ms apart, bottom up) not struck; a chord
    sounds through its bars for `CS_HELD`; `voicing_state` holds the previous 4 notes for voice leading.

- [ ] **Step 1: Write the failing tests** `test_harmony()`: for every progression in every key and mode: the chord at
  bar b equals the progression's chord `(b / bars_per_chord) % n` on pass 1; over 200 seeds with pass ≥ 2 at least
  one reharmonized chord appears and none on pass 1; a `dark` chord has no note above 64; harmony events' notes are
  48–72 except a color tone allowed to 76; each chord's notes are rolled: onsets strictly increasing, 10–60 ms apart.
- [ ] **Step 2: Run** — Expected: FAIL song.
- [ ] **Step 3: Implement `harmony.c`.**
- [ ] **Step 4: Run** — Expected: ok song.
- [ ] **Step 5: Commit** `feat(song): chords over time, reharmonize on repeat, rolled chord styles`.

### Task 8: Melody (lead and ear candy)

**Files:**
- Create: `app/song/melody.h`, `app/song/melody.c`; Test: `tests/host/song_test.c`

**Interfaces:**
- Consumes: `th_*` (Task 2), `song_roll_t`, `lib_mood_t` (Tasks 3–4), `song_plan_t`, `SEC_*` (Task 5), `harmony_chord_at` (Task 7), `song_ev_t` (Task 6), `rng_*`
- Produces:
  - `typedef struct { uint8_t n; uint8_t at[16]; int8_t step[16]; uint8_t len[16]; } motif_t;` — onsets in 16ths
    within 1–2 bars, intervals in scale steps from the first note, lengths in 16ths
  - `void melody_motif(const song_roll_t *r, uint32_t salt, motif_t *m);` — rhythm from cells, ~1/3 of beats played,
    first note a guide tone of the first chord, pentatonic-leaning, 3–8 notes
  - `typedef struct { uint8_t kind[32]; } melody_phrases_t;` per 4-bar lead phrase: `PH_STATE` (exact), `PH_VARIANT`
    (shape moved onto the current chord, new ending), `PH_NEW`; **head–solos–head**: the first lead phrase is
    `PH_STATE`, the last lead phrase of the song is `PH_STATE`; overall ~50/15/35
  - `void melody_plan(const song_roll_t *r, const song_plan_t *p, melody_phrases_t *ph);`
  - `int melody_step(const song_roll_t *r, const song_plan_t *p, const melody_phrases_t *ph, const motif_t *m, int bar, int step, float step_samples, uint32_t salt, song_ev_t *out, int max);`
    — lead (`V_LEAD`, register 64–84, shifted by mood `lead_shift`) and ear candy (`V_CANDY`, 72–96) events.
  - **Rules:** every note in the scale; on beats 1 and 3 a chord tone; never an avoid note (`th_is_avoid`); notes
    5–25 ms late; lead and candy never sound in the same step (**trading fours**: in blocks with both bits, they
    alternate 4-bar phrases, lead first); candy plays answers of 1–4 notes after a lead phrase ends, or sparse notes
    in candy-only blocks (density from mood `candy_pct`).

- [ ] **Step 1: Write the failing tests** `test_melody()`: over 1000 seeds × full songs (iterate `plan`, `melody_step`
  bar by bar): every lead and candy note in scale; every note on steps 0 and 8 is a chord tone of that bar's chord;
  no avoid notes; lead notes 62–86 and candy 70–98 (mood shift included); no step has both `V_LEAD` and `V_CANDY`;
  counting 4-bar lead phrases across all songs, exact restatements 40–60%, variants 10–20%; the first and last lead
  phrase of every song with ≥ 2 lead phrases are `PH_STATE`; a `PH_STATE` phrase repeats the first phrase's
  (step, interval) pattern exactly.
- [ ] **Step 2: Run** — Expected: FAIL song.
- [ ] **Step 3: Implement `melody.c`.**
- [ ] **Step 4: Run** — Expected: ok song.
- [ ] **Step 5: Commit** `feat(song): generated motif, head-solos-head phrases, trading fours`.

### Task 9: The sequencer

**Files:**
- Create: `app/song/song.h`, `app/song/song.c`, `tests/host/clash_test.c`; Modify: `tests/run_tests.sh` (add `clash`)

**Interfaces:**
- Consumes: everything above.
- Produces:
  - `typedef struct { ... } song_t;` — opaque to callers except the read-only fields below
  - `void song_start(song_t *s, const song_roll_t *r);` — plan, motif, phrases; bar 0, step 0
  - `int song_tick(song_t *s, song_ev_t *out, int max);` — advance one 16th step; returns the events of that step
    (drums, bass, chords, lead, candy; sorted by delay); the caller calls it every `song_step_samples(s)` samples
  - `float song_step_samples(const song_t *s);` — `44100 * 60 / (tempo * 4)`, following a tempo glide
  - `void song_request(song_t *s, int field, int value);` — a pending change (`RF_KEY`, `RF_TEMPO`, `RF_MOTION`,
    `RF_MOOD`, and any other `RF_*` via `roll_set`), applied at the next 4-bar boundary; a later request for the same
    field replaces the earlier one
  - `int song_pending(const song_t *s, int *field, int *bars_left);` — the earliest pending change and bars until it
    lands (for "TEMPO 78 IN 3 BARS"); 0 if none
  - Read-only state: `s->bar`, `s->roll`, `s->plan`, `s->state` (`SS_PLAYING`, `SS_ENDED`)
  - Applying changes: key → transpose everything (motif keeps its shape), all sounding notes released first (a
    `V_*` note-off is the `len` of every event ending at the boundary: the sequencer clips lengths of notes crossing
    a boundary where the key changes); tempo → glide over 2 bars; motion → `plan_build` again for blocks from the
    boundary on (blocks already played stay); mood → re-roll mode, progression, color toward the mood via `roll_dice`
    and rebuild from the boundary. Bar count never changes; requests after the last 4-bar boundary are dropped with
    the song.

- [ ] **Step 1: Write the failing tests** in `clash_test.c` (a `main` that rolls 1000 songs in a chain from seed 1,
  runs each to `SS_ENDED` collecting events, and checks):
  - every song ends; its sample length matches `RF_BARS` bars at its tempo (±1 step) and lies in 120–210 s;
  - **determinism:** a hash of all events of songs 1–50 equals a second run's hash;
  - **no clashes:** every `V_LEAD`/`V_CANDY` note on steps 0 and 8 is a chord tone of the chord sounding then, none
    is an avoid note, all in scale; chord notes 48–76, bass 28–47, lead 62–86, candy 70–98; no lead/candy overlap;
  - **Review Focus 1:** `song_request(RF_TEMPO, 78)` issued at the last step before bar 16, inside the outro, and after
    `SS_ENDED` → the tempo changes exactly at bar 16 in the first case, and in the others the song still ends on
    schedule and the next `song_start` uses its own roll's tempo;
  - **Review Focus 2:** a tempo request 70 → 90 at bar 8: total bars unchanged, every block start still lands on its
    bar's first step, the song ends;
  - **Review Focus 5 (sequencer half):** a key request while chords sound: no event of the old key has `delay+len`
    past the boundary sample;
  - `song_pending` reports the field and the right bars-left countdown each bar until the change lands.
- [ ] **Step 2: Run** `sh tests/run_tests.sh` — Expected: FAIL clash.
- [ ] **Step 3: Implement `song.c`.**
- [ ] **Step 4: Run** — Expected: ok song, ok clash.
- [ ] **Step 5: Commit** `feat(song): the sequencer, boundary-quantized requests, clash checks`.

### Task 10: Placeholder voices

**Files:**
- Create: `app/engine/voices.h`, `app/engine/voices.c`; Test: `tests/host/song_test.c` (a `test_voices()` linking voices.c with `-Ibase/include`)

**Interfaces:**
- Consumes: `song_ev_t`, `V_*` (Task 6), `fm_*` (fastmath.h)
- Produces:
  - `void vx_init(void);`
  - `void vx_event(const song_ev_t *e);` — queue an event (its `delay` counts from now); queue of 64; when full, the
    oldest pending event is dropped (never blocks)
  - `void vx_render(float *l, float *r, uint32_t n);` — adds into the buffers
  - `void vx_release_all(void);` — every voice into release (≤ 50 ms), the queue cleared
  - `void vx_set_gain(float g);` — the song's level ramp (fades, `PB_FADE`), smoothed over 20 ms
  - `int vx_active(void);` — voices sounding
- Voices (deliberately simple; Plan 3 replaces): chords 4 voices of 2-op FM (EP-like, ratio 1:1 body + 1:14 tine at
  low index); bass sine + sub-octave; lead and candy 2-op FM bell, 2 voices each; kick sine with a pitch drop
  (150 → 45 Hz, 120 ms); snare/rim filtered noise burst; hats/shaker high-passed noise, short; a one-pole low-pass
  at 5 kHz on the mix. Levels so a full mix peaks below −3 dBFS.

- [ ] **Step 1: Write the failing tests** `test_voices()`: one `V_KICK` event renders nonzero audio within its delay
  ± 1 sample and silence before; 10 s of a dense synthetic event stream: no NaN or Inf, |sample| < 1.0;
  `vx_release_all` after 64 overlapping notes → `vx_active()` is 0 within 60 ms of rendering (**Review Focus 5**);
  queue overflow by 200 events does not crash and keeps the newest.
- [ ] **Step 2: Run** — Expected: FAIL song.
- [ ] **Step 3: Implement `voices.c`.**
- [ ] **Step 4: Run** — Expected: ok song.
- [ ] **Step 5: Commit** `feat(engine): placeholder voices`.

### Task 11: The app: playing songs on the platform

**Files:**
- Modify: `app/app.c` (replace the template's synth, keep its screen-reader and settings plumbing where still used),
  `app/app.h`, `app/fm1.toml`, `app/words.tsv`, `app/tests/basic.txt` → move to `tests/scenarios/basic.vnl`
- Create: `tests/scenarios/songs.vnl`, `tests/scenarios/requests.vnl`, `tests/scenarios/stop.vnl`
- Modify: `tests/run_tests.sh` (run each scenario with `app/build/host/vnl_host`)

**Interfaces:**
- Consumes: `song_*` (Task 9), `roll_song` (Task 4), `vx_*` (Task 10), `plat.h`
- Produces:
  - `fm1.toml`: `include = ["song", "engine"]`, `o2 = ["song/*.c", "engine/*.c"]`; drop `APP_TOUCH` and `APP_BTDEV`
    from `app.h` (no BLE, no velocity).
  - Threads: the main loop (`app_frame`) rolls the next song (`roll_song(&last, seed)`, seed from `plat_ms()` at the
    first boot, then chained) and posts it to the render through a one-slot mailbox (`volatile` flag + copy, read
    in `app_render` at block start); the render owns `song_t`, calls `song_tick` on schedule, feeds `vx_event`,
    renders, applies MASTER. On `SS_ENDED`: silence for a gap rolled in 2–4 s (true silence: no voices, no bed),
    then the next song from the mailbox.
  - Controls (minimal; Plan 5 builds the real UI): PLAY toggles stop/play — stop: `vx_release_all`, silence; play
    after stop: a fresh roll. K1 key ±1 semitone, K2 tempo ±1 BPM, K3 motion ±5, K4 mood ±1 → `song_request`.
  - Screen (minimal text, `cv_text`): line 1 `APP_NAME` + key/mode + tempo; line 2 map name, section name, bar/bars;
    line 3 the current chord name; line 4 the pending change "TEMPO 78 IN 3 BARS"; the four knob values.
  - `app_expect` keys: `playing` (0/1), `songs` (songs started), `bar`, `section` (`SEC_*`), `tempo`, `key`,
    `pending` (bars left, −1 none), `voices` (`vx_active()`).

- [ ] **Step 1: Write the failing scenarios:**
  - `songs.vnl`: boot; `wait 1000`; `expect playing 1`; `expect songs 1`; `shot out:now.png`; `wav out:songs.wav`;
    `wait 215000` (a song ≤ 210 s plus a gap ≤ 4 s, and the next song lasts ≥ 120 s); `expect songs 2`; `wavstop`.
  - `requests.vnl`: boot; `wait 2000`; `turn K2 6` (tempo +6); `expect pending >0`; `wait` until past the next 4-bar
    boundary at the slowest tempo (4 bars at 70 BPM = 13715 ms → `wait 14000`); `expect pending -1`.
  - `stop.vnl` (**Review Focus 5**): boot; `wait 20000`; `tap PLAY`; `wait 100`; `expect playing 0`; `expect voices 0`;
    `peakreset`; `wait 1000`; `expect peak_db_max <-90`; `tap PLAY`; `wait 500`; `expect playing 1`; `expect songs 2`.
- [ ] **Step 2: Run** `sh tests/run_tests.sh` — Expected: FAIL on the scenarios (the template app).
- [ ] **Step 3: Implement the wiring** in `app.c` / `app.h` / `fm1.toml`; update `words.tsv` for the new screen text.
- [ ] **Step 4: Run** `sh tests/run_tests.sh` — Expected: all ok, including `host-build` and `emu`
  (`./build.sh --web` builds the browser emulator). Also `./build.sh` (firmware) — Expected: the build's own checks
  print `ok` for image/RAM/pool, no soft doubles.
- [ ] **Step 5: Listen:** render `songs.wav` and hand it to Sam with the screen shot; note anything that clashes.
- [ ] **Step 6: Commit** `feat(app): VNL-1 plays generated songs`.
