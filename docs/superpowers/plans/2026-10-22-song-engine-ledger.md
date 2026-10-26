# SDD ledger — plan: docs/superpowers/plans/2026-10-22-song-engine.md

## How to read this

This ledger records the build: rulings (R1–R30), deferred findings per task, and items to revisit after listening.
The music behind the rulings is kept in [`docs/music/`](../../music/README.md), split by why a rule exists:
[general.md](../../music/general.md) (genre-independent musicianship: chord-scales, voice leading, bass lines,
rhythm, form, melody), [lofi.md](../../music/lofi.md) (lo-fi hip hop and Sam's taste: tempo, swing, maps, one loop,
no substitutions, diatonic bass) and [ideas-from-other-apps.md](../../music/ideas-from-other-apps.md) (not adopted
yet). "To revisit" below follows the same split.

Spec: docs/superpowers/specs/2026-10-22-vnl-1-design.md. Branch: song-engine (from main da06323).
Baseline: fm1-base tests pass with `PATH=/Users/samueloltz/Documents/fm-1/.venv/bin:$PATH` (system python3 lacks Pillow); host sim template test passes.

## Pre-flight scan

| Rows | Produces / consumes | Found |
| --- | --- | --- |
| T1 ↔ T9, T11 | T1's run_tests.sh names `clash` and scenarios that don't exist yet | Ruling below (R1) |
| T1 ↔ T2–T10 | song_test.c must compile with every module as they arrive | Ruling R2 (compile line) |
| T2 → T3, T7, T8 | th_chord_spec_t, MODE_*, COLOR_*, th_* | consistent |
| T3 → T4, T5, T6 | lib tables, L_*, D_*, MOOD_*, lib_*_by_id | consistent |
| T4 internal | RF_BARS range "32–68" vs 120–210 s at 70–90 BPM (90 BPM × 210 s = 78 bars; 70 × 120 s = 35) | Ruling R3 |
| T4 → T5, T9 | song_roll_t, roll_set, roll_dice | consistent |
| T5 → T6, T7, T8, T9 | plan_block_t (layers, flags), song_plan_t b[24] | 76 bars / 4 = 19 ≤ 24: ok |
| T6 → T7, T8, T9, T10 | song_ev_t, V_* in groove.h | consistent; engine includes groove.h via -Iapp/song |
| T7 → T6 (via caller), T8, T9 | harmony_chord_at | groove gets chord pcs as a parameter: no cycle |
| T8 internal | lead 64–84 ± lead_shift vs tests 62–86 | consistent (±2 shift) |
| T7 ↔ T2 | th_voice top ≤ 72; harmony color tone ≤ 76; T9 checks 48–76 | consistent |
| T10 internal | needs fastmath: -Ibase/include | R2 covers |
| T11 ↔ fm1-base | o2 globs supported by build.py; include dirs | consistent |
| T11 internal | moves app/tests/basic.txt → tests/scenarios/basic.vnl; must drop BLE/velocity parts | implementer adapts basic to VNL-1 |
| Every task | test-first steps, files created vs touched | agree |

Ruling R1: T1's runner includes only `song`, `host-build`, `emu` (if emcc); T9 adds `clash`, T11 adds scenarios — a runner naming missing tests would fail from day one — cost if wrong: none.
Ruling R2: compile line for unit tests: `cc <flags> -Iapp/song -Iapp/engine -Ibase/include -o OUT tests/host/song_test.c app/song/*.c` (T10 adds app/engine/*.c); runner exports PATH with .venv/bin first and PYTHON=.venv/bin/python — cost if wrong: trivial edit.
Ruling R3: RF_BARS range is 36–76 (multiples of 4), derived from 120–210 s over 70–90 BPM; the seconds rule binds (spec) — cost if wrong: a test bound changes.

## Tasks
Task 1: dispatched (base da06323, implementer sonnet)
Task 1: minor (deferred): rng modulo bias in rng_int/rng_weighted (negligible)
Task 1: minor (deferred): song_test shared `bad` counter across rng sections
Task 1: minor (deferred): (seed,stream) distinctness not guaranteed in general (XOR mix) — later tasks must not assume it
Task 1: complete (commits da06323..f0ba9e3, review clean)
Task 2: dispatched (base f0ba9e3, implementer sonnet). Ruling: COLOR_RICH = root,3,7,9 + characteristic ext (13 dom, 11 min7, #11 maj7 Lydian), 5th omitted, ≤5 tones — brief said '4–5 tones' and 'G13' — cost if wrong: chord-color retune in curation
Task 2: minor (deferred): th_is_avoid returns 0 for out-of-scale notes (narrower than brief) — Task 8/9 must call it only with scale notes (melody rule already requires in-scale)
Task 2: minor (deferred): NULL-prev reference voicing E3–D4 vs brief E3–C4
Task 2: minor (deferred): theory test gaps (forced qualities, alter, non-NULL prev, avoid-tone colors, name length sweep, RICH in-scale, strict ascending)
Task 2: complete (commits f0ba9e3..d38990a, review clean)
Task 3: dispatched (base d38990a, implementer sonnet)
Task 3: Ruling R4: progressions get an allowed-mode mask `uint8_t modes` (bit MODE_*) listing modes where their degrees give the intended chords; Task 4's roll filters progressions by the rolled mode AND mood; the library adds minor-family progressions so every mood × every mode it weights > 0 has ≥ 3 compatible progressions — spec wants "no clashes" and mood-steered modes; mode-relative degrees with major-key functions produce wrong chords (review Issue 1) — cost if wrong: a field to drop later.
Task 3: Ruling R5: every progression's cycle n × bars_per_chord ∈ {2, 4, 8}; #15 becomes 4 chords at 1 bar (ii9–bII7–Imaj9–Imaj9) — spec: changes land on 4-bar boundaries, loops 2–4 bars — cost if wrong: none.
Task 3: Ruling R6: #13 (city pop) gets a distinct sequence IVmaj7–III7–vi7–I7 (Just-the-Two-of-Us flavor) instead of duplicating #9 — cheap, removes a double weight — cost if wrong: a curation edit.
Task 3: minor (deferred): pattern #2 kick busy (steps 7+8); #5 half-time kick on 2 and 4; bossa kick 0,3,8,11 vs typical — check by ear in curation
Task 3: minor (deferred): sets #8/#11 overlap registers (vibes with kalimba/Wurli)
Task 3: minor (deferred): MOOD_* bit masks vs LIB_MOODS 0–3 index — easy to misuse
Task 3: minor (deferred): test gaps (grid asserts, one poly3, degree range, mood weights, all map fields); sunny lead_shift 0 arguably positive
Task 3: fix round 1/5 (3 addressed, 0 open; commits f748be6..481e723)
Task 3: minor (deferred): Lydian masks and #19–#21 not name-spot-checked (hand-checked by re-reviewer)
Task 3: complete (commits d38990a..481e723, review clean)
Task 4: dispatched (base 481e723, implementer opus — subtle novelty/dice logic). Ruling R7: novelty 'pattern family' compares lib_pattern_t.family; relative key = 3 semitones across major/minor families; CoF neighbor = ±5/±7 — brief left these open — cost if wrong: test bound edits
Task 4: Ruling R8: progression mix target applies to the effective mix across rolls; add ≥3 minor-family (Dorian/Aeolian) jazzy progressions to the library and a test that over ≥2000 chained rolls each kind lands within ±10 points of 40/45/15 — spec 40/45/15 (call) is about what the listener hears; roll per-cell weighting is already right — cost if wrong: curation retunes weights.
Task 4: fix round 1 opened with R8 gap + minor test-direction fix + hook comment
Task 4: minor (deferred): roll_set refused manual pick indistinguishable from no-op (both return current value) — Plan 5 UI must know
Task 4: minor (deferred): after a mood change a legal-but-zero-weight mode/color can persist (e.g. sunny + Aeolian)
Task 4: fix round 1/5 (3 addressed, 0 open; commits 13b21e0..6067fab)
Task 4: minor (deferred): #23 not name-checked; library table vamp share at −9.2 of the ±10 check — next curation should add a vamp
Task 4: complete (commits 481e723..6067fab, review clean)
Task 5: dispatched (base 6067fab, implementer opus). Ruling R9: map-mandated moves (drums+bass entry, MID_BREAK drop) are exempt from the ≤2-layers-per-boundary rule; optional Motion moves are not; L_BED unset in Plan 1; plan dice on stream 100; Drift drums ≤ ~60% of middle bars — brief's 2-layer rule conflicts with its own mandated 3-layer moves — cost if wrong: test bound edit
Task 5: Ruling R10: candy and lead MAY share a block — plan Task 8 mandates trading fours "in blocks with both bits" and forbids them in the same step; spec's "never over the lead" is enforced at note level by Task 8, which clash_test (Task 9) checks — review Important 2 parked as plan-mandated — cost if wrong: plan.c skip rule + one test.
Task 5: Ruling R11: map skeleton text binds over Motion moves: Slow bloom ("drums never drop") gets no drum-layer moves after the kick enters; Straight in ("only small hat drop-outs") gets hat-outs only, no 1-bar full drops — spec's map table — cost if wrong: less variety in two maps.
Task 5: Ruling R12: section label "break" for any middle block with kick and snare out OR the MID_KICK_OUT block (spec: break = drums out or thinned) — progress strip accuracy — cost if wrong: label only.
Task 5: fix round 1 opened (lead per-song coverage; R11; R12)
Task 5: minor (deferred): Drift drum drop-outs keep the bass (spec: bass "with the drums"); whole-unit drops overshoot on short Drift songs (seed 11 @36 bars drumless after bar 16)
Task 5: minor (deferred): PB_FADE often on a 4-bar last block; Slow bloom spec says "long fade"
Task 5: minor (deferred): MID_BREAK silently places no break if no candidate (unreachable for 36–76 bars)
Task 5: minor (deferred): fills only before kick/snare/bass changes; no phrase-end fills
Task 5: fix round 1/5 (3 addressed, 0 open; commits a3d23e8..8af5f0e)
Task 5: minor (deferred): plan.c:50 comment "never the first" now true only on the main lead path
Task 5: complete (commits 6067fab..8af5f0e, review clean)
Task 6: dispatched (base 8af5f0e, implementer sonnet). Ruling R13: groove per-song bases on rng streams 110+; on-beat = even steps; hat 70 ms cap excludes swing; BS_PEDAL on step 0 of even bars for 2 bars (Drift or sad ~50%); BS_WALK only for upright sets, one 8-bar span chosen by seed hash, else root-fifth; fills steps 12–15 snare + kick 14 — brief left mechanics open — cost if wrong: groove retune in curation
Task 6: Ruling R14: BS_WALK may recur once per 32 bars (up to 3 windows in a 76-bar song); accepted as musically fine — groove_step lacks song length; R13's gate — cost if wrong: pass song bars and gate one window.
Task 6: minor (deferred): song_ev_t facts undocumented in groove.h (drum len 0/note 0; vel actually 25–110; kick/bass swing-only, snare/rim/hats swing+lateness; bass len may span bar lines) — carried into Task 7–10 dispatches
Task 6: minor (deferred): off-16th bass swing test vacuous (found unasserted); WALK window untested; fill test doesn't check replacement/no-doubling; per-step re-derivation of per-song constants (cheap)
Task 6: complete (commits 8af5f0e..4baaeed, review clean)
Task 7: dispatched (base 4baaeed, implementer sonnet). Ruling R15: pass 1-based; reharm chance 25–50% on stream 120, one chord per pass ≥2 (dom→tritone sub, IV→iv7, else color+1); dark = color−1, no note >64; 4 notes only (48–72); styles on stream 121 with set step patterns; velocities 25–110 bands — brief left mechanics open — cost if wrong: curation retune
Task 7: fix round 1 opened (rolled-note len overshoot; silent reharm; voicing reuse; dead locals)
Task 7: minor (deferred): no direct tests that dark lowers color / tritone sub & iv-minor fire / reharm+dark combination
Task 7: fix round 1/5 (4 addressed, 0 open; commits f652807..f0f2bd1)
Task 7: minor (deferred): dark-path stored voicing may drift (not verified to re-voice to itself); bar_notes test check convoluted; callers must keep salt constant within a song (carried to Task 9)
Task 7: complete (commits 4baaeed..f0f2bd1, review clean)
Task 8: dispatched (base f0f2bd1, implementer opus). Ruling R16: lead phrase = 4 bars where lead plays (trading fours alternate lead first); notes never cross a chord change; lead/candy sounding intervals never overlap; no swing on lead; streams 130+; melody_step stateless; salt = roll->seed — needed to make Task 9's clash test satisfiable — cost if wrong: melody retune
Task 8: minor (deferred): melody_plan hard-codes r->seed as motif salt (contract carried to Task 9); melody_motif rebuilds plan internally (hidden coupling)
Task 8: minor (deferred): restatements can cluster next to the closing head (seed 3: 12 bars of the same phrase); repeated same-pitch adjacent 16ths (stutter); untested pentatonic lean/guide-tone first/variant rhythm/density
Task 8: complete (commits f0f2bd1..bef1188, review clean)
Task 9: dispatched (base bef1188, implementer opus). Ruling R17: boundary = first bar B%4==0 after the current position (B==bar if its step 0 not yet ticked); B ≥ RF_BARS drops the request; key change clips crossing notes and keeps the motif; tempo glides 2 bars from B; motion merges plans at B and re-runs melody_plan; mood = roll_set(RF_MOOD)+roll_dice(RF_PROG) and rebuilds motif+plan from B — brief left mechanics open — cost if wrong: request behavior retune in Plan 5
Task 9: Ruling R18: a listener tempo request may move the current song outside 120–210 s (RF_BARS stays fixed per R17); the seconds rule governs rolls, not the listener's own change to the current song — spec "changes apply to the current song only"; keeping bars keeps the map intact — cost if wrong: re-clamp bars on tempo requests.
Task 9: fix round 1 opened: glide overlap (Imp 1), oracle independence (Imp 2), plus folded minors: duplicate motif build at boundary, drop past-edge events instead of len 1
Task 9: Ruling R19: ear candy too sparse (~1.3 events/song vs spec "occasional": a few gestures per song) — after Task 9, dispatch a tuning task (Task 8b) on plan.c + melody.c: candy present in ≥ 70% of songs with ≥ 3 gestures (answers or sparse figures) per song on average, still never over the lead; tests in song_test/clash_test — cost if wrong: candy retune in curation.
Task 9: minor (deferred): song.c duplicates melody.c's motif reading (mode_offsets/deg_of/note_of) for key transposition — prefer a melody_transpose in melody.c
Task 9: minor (deferred): song_pending tie-break reports lowest field while Mood applies first (display only)
Task 9: minor (deferred): clipping at every 4-bar line would silence a future 4–8-bar-per-chord HELD vamp — gate to real changes if such progressions are added
Task 9: Ruling R20: the 57/16,715 strong-step semitone rubs are clashes (anti-goal). Task 8b fixes them at the source — lead never sounds the root against a rootless maj7 voicing's 7th (or any note a semitone from a sounding chord/bass note) on steps 0/8; weak lead notes may not be held into a strong step unless they pass the same test; bass fifth only when the chord has a perfect fifth (else root) — and clash_test's rub report becomes a hard failure (0 allowed) — cost if wrong: slightly fewer melodic choices.
Task 9: fix round 1/5 (5 addressed, 0 open; commits bfc6f99..88cd071)
Task 9: minor (deferred): during a speed-up glide swing is slightly understated for 2 bars; clash_test MAXE 40000 drops silently (assert ne < MAXE)
Task 9: complete (commits bef1188..88cd071, review clean)
Task 8b (R19+R20): dispatching follow-up — ear candy density + strong-step rubs + harden rub oracle
Task 8b: dispatched (base 88cd071, implementer opus)
Task 8b: Ruling R21: BS_PEDAL restrikes the new chord's root at every chord change (held until the next change, ≥ 1 bar) instead of a 2-bar hold that leaves a 1-bar hole under 1-bar chords; update the groove pedal test accordingly — 'one bar on, one bar off' is a gap a listener hears; a held root per chord is the pedal feel without rubs — cost if wrong: pedal retune
Task 8b: Ruling R22: keep song.c's bass cut at chord changes as defense in depth (document it in song.c's header); groove's swung bass length spill stays a deferred minor — works today, the fix-at-source is cosmetic now — cost if wrong: bass length policy split across two modules.
Task 8b: minor (deferred): bass len ignores swing delay in groove.c (source of spill); bpc lookup repeated in groove/song/song_test; no unit test for fifth rule on dim/aug chords; candy presence 765 vs ≥700 thin margin; no metric on dropped strong-step lead notes
Task 8b: complete (commits 88cd071..acf29e4, review clean)
Task 10: dispatched (base acf29e4, implementer sonnet). Ruling R23: sample-accurate starts; voice pools chords 4 (steal oldest), bass mono, lead 2, candy 2, drums 1 per lane; engine-owned xorshift noise; vx_render adds into float l/r, peak < −3 dBFS; app scales MASTER — brief left mechanics open — cost if wrong: Plan 3 replaces voices anyway
Task 10: fix round 1 opened (steal/retrigger clicks; headroom/soft clip; bell modulator phase; gain snap; header notes)
Task 10: minor (deferred): pitched len 0 would hold forever (robustness); queue memmove on overflow O(n); up to 23 fm_sin_turns/sample at full polyphony — measure on device in Plan 2; vx_event/vx_render not locked — app must call both from the render context
Task 10: fix round 1/5 (5 addressed, 0 open; commits fac9be2..a1542f2)
Task 10: minor (deferred): steal onset delay (~4.4 ms) untested; drum retrigger restarts envelope abruptly (percussive)
Task 10: complete (commits acf29e4..a1542f2, review clean)
Task 11: dispatched (base a1542f2, implementer opus)
Task 11: Ruling R24: device seed mixes entropy without flash writes — raw MASTER pot reading, plat_ms() at the first app_frame, and a main-loop spin count until then (device only; host stays deterministic under PLAT_HOST) — spec "power-on = a fresh roll" outranks the brief's plat_ms() seed — cost if wrong: songs repeat at power-on.
Task 11: Ruling R25 (→ Task 8c): no out-of-scale chord tones against the melody: (a) non-jazzy progressions' chords (all colors) must be in the mode's scale for every mode their mask allows — fix masks + library test; (b) jazzy borrowed chords stay, and melody/candy legality per bar uses the chord-scale (scale notes a semitone from a chord tone are replaced by that chord tone); (c) clash_test: no lead/candy note a semitone from any sounding chord/bass note at ANY step, not only steps 0/8 — anti-goal "no clashes" — cost if wrong: fewer modes per progression.
Task 11: fix round 1 opened (R24 seed; output clamp; mix to ~−6 dBFS peak via voices gain staging; requests.vnl robust to 90 BPM; memmove out-of-line)
Task 11: minor (deferred): line_chord repeats song.c pass/dark rule; command() shadows song.c pending state and refusal rule; r_pos/r_frac duplicate song pos — prefer accessors (Plan 5); knob turns outside a playing song dropped; last_roll is as-rolled not as-played (Plan 5 novelty)
Task 11: fix round 1/5 (5 addressed, 0 open; commits 73844f9..bed46a3)
Task 11: minor (deferred): boot entropy thin and untested on hardware (verify in Plan 2); NaN→int32 cast UB in clamp (theoretical)
Task 11: complete (commits a1542f2..bed46a3, review clean)
Task 8c (R25): dispatched (base bed46a3, implementer opus)
Task 8c: Ruling R26: accept the implementer's library calls — progressions 12/13/18/19/21 re-kinded jazzy and trimmed to their named mode; 2/5/7/9/26 take the mode's own quality on one chord (names unchanged); 18 new progressions (27–44, src R25) await curation by ear; jazzy 14–16 keep Lydian/Mixolydian borrowings (melody rub-free via chord-scale) — keeps the 40/45/15 mix and mood×mode coverage without loosening tests — cost if wrong: curation prunes entries.
Task 8c: Ruling R27: approve touching groove.c/h to export one bass pitch-class helper used by both groove_step and melody's bar masks (removes the duplicated walk-window/approach rule) — duplicated logic would silently reopen rubs — cost if wrong: small API addition.
Task 8c: Ruling R28: the lead's narrow legal set (strong steps often 2 pcs; chord 3rd excluded in 23% of combos) is a spec-level consequence of the pitch-class rub rule over all 5 harmony pcs; defer any relaxation (e.g. safe from voiced pcs only) until Sam listens — "no clashes" outranks melodic range for now — cost if wrong: a lead that sounds arpeggiated/dull.
Task 8c: fix round 1 opened (groove bass helper; stale spot-check 13 → Ionian; rewrap long comment)
Task 8c: minor (deferred): in_chord_scale duplicated across two test binaries; table jazzy share exactly 25% (next jazzy add fails the mix test); src label 31 "v7-I" → "v7-i"; progressions 28/34 tonally ambiguous — curate by ear; sad/wistful Lydian refused by roll_set (weight 0; check if a UI mode picker per mood appears in Plan 5)
Task 8c: fix round 1/5 (4 addressed, 0 open; commits 1aeaa20..fc2aac8)
Task 8c: minor (deferred): groove.h doc comment for groove_bass_note sits between groove_step's comment and its prototype
Task 8c: complete (commits bed46a3..fc2aac8, review clean)
Final review: dispatching (merge base da06323..fc2aac8)
Final review: ready to merge after (a) firmware rebuild on head, (b) groove.h comment order. No Critical.
Final: Ruling R29: song_start and boundary rebuilds stay in the audio interrupt for Plan 1 (plan-mandated; host worst case ~19 µs); Plan 2 must measure the worst boundary tick and interrupt stack (~2.5 KB est.) on the device; Plan 5 must make plan.c's static grid_t a caller workspace (or move song_start to the main loop) before any main-loop plan_build (preview screen F) — spec says planning on the main loop — cost if wrong: an audio glitch at boundaries on the device.
Final: Ruling R30: the user's listening check (Task 11 Step 5) happens at hand-off, before Plan 3; R28 (lead legality strictness) is decided then — cost if wrong: an arpeggiated-sounding lead persists.
Final: fix wave dispatched (groove.h comment; ./build.sh on head; re-render songs.wav)
Final: fix wave cd9f186 (comment-only, verified by controller reading groove.h:16-28 — review-package script denied by the auto-mode classifier); suite green; firmware rebuilt on head: image 112596 B, RAM 31160/98304, pool 69384/344064, no soft doubles; songs.wav re-rendered 15:20.
Final: open — 4 commits carry "Co-Authored-By: Claude Sonnet 5.5" instead of Opus 5.5; history rewrite denied by the auto-mode classifier; left for Sam.

## To revisit (Sam, after listening)

- **Bookmark: full listening pass after the GPL imports (Sam, 2026-10-26):** "I want to hear everything again once we
  finish switching over to the stuff we're importing from gpl projects." Then re-render sets 21-33 and pick up:
  - Set 26's guitar sound: "not sold on the sound. I can't quite pin down the issue." My guess: the Emilyguitar
    sample loop sustains held and half-note chords like an organ; offer an A/B with a natural decay.
  - The "vibes" (INS_VIBES, an M_BELL FM patch) sound like a plucked synth, not vibes: re-voice the bell (soft
    mallet, motor tremolo, longer ring; no flash) or sample a vibraphone (~30 KB; ~23 KB left).
  - Still unanswered: the C3 guitar floor; half-rate sampled kits A/B; 16 vs 32 kHz cymbals.
  - Settled this pass: Knight stab level fine; Knight no-roll fine; ride quarter notes at most with a 1.5 s tail
    (7773f9d, a162c90); guitars never arpeggiate; sampled hats/shaker play 8ths below 55% swing; the vibes lead uses
    the phrase planner (the last three in commit "Set 26 fixes", tests not run at Sam's request).

- **Piano broken chords take the rolled-chord role on the acoustic piano (Sam, 2026-10-25):** the bottom-up roll
  (10-60 ms a note) on the Knight's stabs in set 30 "is not how people usually play the acoustic piano. The strumming
  affect. Electric pianos work with it, but not acoustic." The Knight now strikes together; when the broken-chord climb
  is built, use it for at least the Knight under a sustained lead (set 30's situation), and consider it for the
  electric pianos too.

### General

- **CC0 sample libraries to mine (2026-10-25): VCSL and VSCO-2 CE (sgossner, both CC0).** Beyond the spike's piano
  (VCSL Kawai grand; also Steinway B, Knight and Yamaha uprights) and strings (VSCO-2 CE Violin, Viola and Cello
  Sections): sampled vibraphone, marimba, glockenspiel, several kalimbas and mbiras (could replace our FM vibes and
  kalimba); the TX81Z FM Piano and Clavisynth (real 80s FM); organ; tenor sax and flute (sampled leads to A/B with our
  winds); harmonica; percussion for the kit and ear candy (shakers, tambourine, cabasa, claves, conga, bongos,
  woodblock, cajon, mark trees, finger cymbals, wine glasses); an ocean drum (a bed texture beside the rain). No
  nylon or acoustic guitar there (FluidR3 or another permissive source for the strum).
- **Sampled drum kits (Sam, 2026-10-25: "lo-fi has a large number of drum kits... a great range of sounds").** Today
  4 synthesized kits, fixed per set. Idea: a shared pool of CC0 one-shots (~21 hits, ~35-45 KB) re-tuned, shortened,
  filtered and saturated per kit, so dozens of kits come from little flash, and each song rolls a kit its set and mood
  allow. Sources and the pool: docs/research/drum-samples-survey-2026-10-25.md (Karoryfer Gogodze Phu Vol II, Big
  Rusty, Swirly, Frankensnare: CC0, checked; VCSL percussion: CC0). Guitar sources:
  docs/research/guitar-samples-survey-2026-10-25.md.
- **Bass and kick separation: back burner (Sam, 2026-10-24: "Don't pursue right now. Put it way on the back
  burner. If it's idiomatic to pair them, that's fine.").** The bass locked to the kick is the genre's idiom; the
  parked bass-rhythm spike (VNL_BASS) stays parked.
- **Piano broken-chord climb (Sam, 2026-10-24: "soon, just not quite yet").** The arpeggio comping on piano-like keys
  (Rhodes, Wurli, felt, DX EP) as a rising broken chord over 2-3 octaves: root an octave below the voicing, 5th,
  10th, then the voicing and the voicing an octave up; chord tones only down low. It sometimes lands on the voicing as a
  block chord at the top, held to the bar's end: always in a 4-bar phrase's last bar, about 1 in 3 other bars,
  deterministic per loop position. Pedal-like ringing; velocity about 50 to 70 up the climb, 75 on the top chord. Under
  the lead in lead sections (tops at the voicing's top), an octave higher elsewhere; a busy lead thins it to 8ths with
  no top chord. Open question for Sam: 16ths (two climbs a bar), 8ths (one climb landing near beat 4), or both per song.
- **The pad's solo note against the lead (Sam, 2026-10-24: "leave out for right now... we can revisit later").** The
  pads design's solo note ignores the lead; if they clash by ear, add the check (sit out a bar where the lead comes
  within a 3rd or a semitone).
- **Acoustic piano spike (Sam, 2026-10-24: "yes", queued after the pads plan).** A throwaway spike, the same chords
  rendered two ways for Sam's ear: (1) a better FM patch (3 operators, a detuned string pair for beating, a fast
  brightness fall, decay shortening with pitch; about the felt piano's cost, 0.43 for 4 voices) and (2) a waveguide
  piano on the Karplus-Strong lines (2 detuned strings, a hammer noise burst, stiffness allpass, a soundboard filter;
  roughly 1.5-2x the felt piano a voice). Samples are out (synthesis only; the 400 KB image).
  First (Sam): survey the existing FM-1 firmwares (ref/: fm1-base, fm1-dub, the others) for piano, string or bowed
  voices to borrow (MIT/BSD/MPL with attribution; GPL ideas only) or learn lessons from. Also search the web for
  FM-1 firmwares released since the last survey (Sam: "new firmwares coming out all the time") and add them to ref/
  and docs/research.
- **Lessons from Eight Bells and Shine On (2026-10-24, Sam pointed at github.com/Discodave-Glitch/eight-bells).**
  Eight Bells (MIT) is a browser/desktop pattern generator that plays the FM-1 over USB MIDI, not a firmware. Useful:
  its firmware list (about 40 FM-1 firmwares, plus thefirmwarehub.com as a hub to watch); melody/chord engines
  named "pedal point", "call and response", "turnaround" (ideas for the lead and pads); "Auto song" changing the
  pattern every 1-8 loops and sometimes returning to an earlier one (A-B-A thinking); share codes (seed, genre,
  tempo) for Plan 4. New firmwares not yet in ref/: fm1-x0x-rmx (GPL, an X0X fork), noiz_demons (GPL, a Felucca mod),
  Felucca_8_patterns (GPL fork), and **Shine On** (zvenson/shineon, GPL: ideas only), a 70s string machine on the FM-1:
  top-octave-divider saws at 16', 8', 4' (and an 8' square), **fixed formant filters on the sum rather than per voice**
  (registers CONTRABASS, CELLO, VIOLA, VIOLIN, TRUMPET, HORN), three bucket-brigade delay lines swayed by two LFOs
  120 degrees apart (the Solina ensemble) and a phaser. Lessons for the pads and string leads: a shared formant
  filter on the pad bus instead of a filter per voice (CPU), the three-line 120-degree ensemble for width, a
  formant-register cello. Survey these properly with the piano spike.
  Eight Bells' generators (index.html, MIT; read 2026-10-24): melodies are Euclidean rhythms over scale degrees,
  chords are plain triads on a 4-chord loop (no 7ths): generic next to ours. Worth taking: (1) its "sequence" engine
  (a 4-note motif transposed up a degree per group, `seqnc` ~line 2700) for the hybrid lead's arpeggio; (2) the
  lament descent progression i-VII-VI-V (`lament`) as a new progression or a B section; (3) pedal point (bass holds the
  root while the upper voices move) for the pads or a break; (4) Auto song's bank: 40% of the time return to an
  earlier pattern (~line 5212), for A-B-A and long streams; (5) a check on our drums: lo-fi kicks [0,10], [0,7,10],
  [0,3,10] with snare 4 and 12, swing 62.
  Its rhythm engines (the ENGINES table, ~line 1023): for lo-fi take World cycles (the TIMELINES table ~line 1101:
  son/bossa/rumba clave, tresillo 0 3 6, habanera 0 3 4 6; bossa percussion, bass and kick rhythms), Bursts (rolls,
  flams, ghost-note clusters: phrase-end drum fills, which we lack), Polyrhythm (3 against 4: a dotted-8th hat or
  shaker), and sparingly Drift (ear candy on a loop a step shorter than the bar). Weighted, Markov and Offbeat are what
  we already do; Cellular, Fractal, Number, Pi, Chaos, Sieve, L-system, Golomb, Collatz, Accelerando, Random walk and
  Mirror don't groove: skip.
- **String leads: a string ensemble and a cello (Sam, 2026-10-24: "explore strings for the leads, as well as a cello
  for leads"; corrected: "Not a solo-violin strings. A string orchestra/ensemble").** Sustained leads (they would sing
  the phrase lead). The ensemble lead: the melody played by a section (several detuned voices or the pads' ensemble
  voice with chorus, a slower attack, octave doubling as an option), not a solo violin. The cello: a solo bowed voice
  (a saw with bow noise, a delayed vibrato and a body filter, or a bowed waveguide). Spike both beside the piano. The cello wants a lower register
  than the lead's (60-88 today, the floor over percussive keys): a cello lead sits around C3-A4, so it needs its own
  register rule and room in the keys (they move up or thin out under it).
- **Tape warble per instrument (Sam, 2026-10-24: "tape warble, on instruments it makes sense").** Pitch wow and
  flutter on the instruments that suit it (keys, pads, the sustained leads), not on the drums or bass. Today's wow sits
  on the whole mix in the chain (fx.c's wow line); this would make it per role, with depth by instrument.
- **Birds in the bed (Sam, 2026-10-24: "on the noise layer, birds chirping in the background").** A synthesized
  birdsong layer for the bed beside vinyl, tape and rain: sparse chirps, far back, seeded per song.
- **The pad's notes against the ear candy (final review, 2026-10-24).** The pad rules check keys, bass and lead only;
  the solo note's 7th could rub a candy tonic. Check by ear; add the candy to the pad's clear-of set if it does.
- **Two lead parts, A-B-A (Sam, 2026-10-24).** "The lead taking that long to come in seems strange... a different
  lead at the beginning. So two different lead parts. Never playing the same time, but like A B A." Today every map
  brings the lead in late (average first lead bar 17-31 of about 53, by map; 30-58% in). Next after the pads.
- **Background pads/strings layer (Sam, 2026-10-24: "would make a nice layer").** A quiet, slow-moving sustained
  layer under the keys, out of the lead's register. Needs its own design (role/bus, which sets, voicing, cost). **Done** (2026-10-24; see "Pads: done" below).

- **Lead phrase starts are square.** Over 1,000 songs: 35% of lead gestures start exactly on beat 1, 59% on a beat,
  19% on 8th offbeats, 21% on 16ths, and true pickups are impossible (phrases begin on their first bar; notes may not
  cross a chord change). Cause: melody.c `gen_figure` picks beat 1 first 60% of the time, and 8 of 14 rhythm-cell
  weights include the beat. Proposed (not done): starts ~15% beat 1, 25% other beats, 35% 8th offbeats (and-of-1,
  and-of-4), 10% 16ths, 15% pickups (1–3 short notes in the bar before, ending before the chord change, landing on a
  chord tone at or just after the downbeat); offbeat-leaning cells inside phrases; restatements keep the rhythm.
  (General: phrase starts need variety in any style; the target percentages are a lo-fi amount, see
  `docs/music/lofi.md`.)
- **Lead strong-beat choice (R28, R30).** Strong steps often allow only 2 pitch classes; decide at the listening
  check. A rub-free loosening (voiced color tones on strong beats) is in `docs/music/ideas-from-other-apps.md`.
- **Lead and candy swing (R16).** They are late but straight while hats and kick swing; every engine studied swings
  all parts. Hear both before reversing R16.

### Leads (Sam: not happy with the current leads; start here when we revisit them)

- **The slow arpeggio as a lead.** One chord tone per 8th, up then down, sounded *melodic* rather than like
  accompaniment when it was a comping style (it's now in 16ths for comping). Use that 8th-note arpeggio, drawn from
  the song's voicings, as lead material.
- **"Simple" whole-note voice-leading lines.** Lines that move one chord tone to the nearest one, a note per chord
  (whole notes), built on the voice-leading work the keys already do (common tones held, smallest steps, top voice
  first), as a lead or a counter-line. A strong, safe starting point before more ornate melody.
- Together with the phrase-start item above (more off-beat starts and pickups) and the research's "land on the
  keys' color tones" idea (docs/music/ideas-from-other-apps.md).
- **Move away from the hybrid lead toward a sophisticated arpeggiator (Sam, 2026-10-23: the hybrid "still doesn't
  feel musical").** Research first: docs/research/arpeggiators.md (how sophisticated arpeggiators work, and existing
  work to use or learn from). **Sam: the research informs both the lead AND the accompaniment** — one phrase engine
  (authored figures over chord-tone indexes, lanes for rhythm/octave/accent/gate, phrase repetition A A A′ B with
  looped variation, rank-based thinning by motion) serving the lead and the comping (today's 16th-note comping
  arpeggio, and the guitar strumming idea below), each with its own figure library and register.
  **Before applying it, walk Sam through all the research again (arpeggiators.md and fm1-accompaniment-code.md),
  separated into accompaniment and lead** (Sam, 2026-10-23), then design each.
  **Walkthrough done (2026-10-24). Sam's priorities:** struck/plucked leads (the hybrid) "aren't terrible, but
  dropping different notes is bad and should be corrected" (fix: thinning by rank, so motion always drops the same
  notes); wind and sustained synth leads (the skeleton line) "are down right terrible right now and need the most
  improvement" (next design: the phrase spine — Tuna-style question/answer/arrival/breath, motif memory with small
  nudges and a literal restatement — phrased like a wind player). Then the figure library and the accompaniment
  work (comp figures, strumming). Sam: "Get rid of the ugly, then optionally come back and improve the decent" — the sustained
  lead first (docs/superpowers/specs/2026-10-24-sustained-lead-design.md); the hybrid's rank thinning after, optional.
- **Done (sustained-lead plan, 2026-10-24):** the sustained lead (flute, sax, muted trumpet, saw lead) sings phrases
  instead of the skeleton line: one motif per song, an 8-bar question and answer with a literal restatement, a held
  question and arrival, breaths, wind phrasing, motion by rank (low motion: fewer notes held longer). See
  docs/superpowers/specs/2026-10-24-sustained-lead-design.md and `docs/music/lofi.md`. The skeleton line's
  `test_line_wanders` is gone (it measured the old line); `test_sustained_phrases` and `test_sustained_motion`
  replace it.
- **Sustained lead: judge by ear (2026-10-24).** The motif's length (1 bar played twice a step apart, or 2 bars);
  the nudges (a step, the last interval, an inverted second half, an 8th's delay) and whether bar 3 and 7 sound
  related enough; the breath (half a bar: too long at slow tempos?); the arrival (the root, or the 3rd over home and
  over maj7 chords, where the root rubs: about 40% land on the 3rd); the tie-in push (the question or arrival tied
  over from the bar before; where a rule refuses the tie it lands on 1, and then beat 1 can sound in two bars
  running across the breath); the velocity arc (+16 with the line's height, the arrival 8 softer, short notes and low
  notes among percussive keys louder so every song sits 4–6 dB over the keys; the muted trumpet 0.5 dB down); a
  planned leap past a 5th sung within a 5th, and a leap answered by a step back where one is legal; phrases high
  over percussive keys (a 4th over the keys' top leaves about an octave, so long phrases fold at the top).
- **A cap on how busy the lead can get (Sam, 2026-10-23: "I will also want to work towards putting a limit on how
  busy it can get").** After the hybrid follows the motion lever (lead plan Task 6): a ceiling on onsets per bar or
  per phrase, whatever the motion.
- **Done (lead plan, 2026-10-23):** the lead by instrument class (hybrid for struck and plucked leads, the sung
  line for winds); the comping making room; register by the keys' attack; the line's phrase arc; the hybrid
  following motion. See docs/superpowers/specs/2026-10-22-lead-accompaniment-design.md.
- **To judge by ear with real instruments (Plan 3):** the pad placements (especially "above"), the sung line on a
  real flute, sax or trumpet, whether the lead belongs only in lead blocks (lead plan Task 1 ruling), and the sung
  line without suspensions (dropped: a held non-chord tone on 1 breaks the no-rub rules; could return as a tie).

### Accompaniment

- **Guitar strumming as a comping style (Sam, 2026-10-23, for later).** A good concept of strumming for the
  accompaniment: down/up strokes on a 16th grid, the strings spread a few ms apart (low to high on downstrokes, high
  to low on upstrokes), muted "chuck" strokes, accents, and patterns per song; voiced for a guitar's six strings
  rather than the keys' four voices. Would pair with the nylon and guitar plucks (Karplus–Strong, Plan 3a Task 4).
  **Sam (2026-10-24), after the accompaniment walkthrough:** strum speed varies, slow (each string heard on its own,
  roughly 30–80 ms apart) or quick (roughly 5–15 ms, one chunky hit); real strumming patterns with lots of strums in
  a bar (e.g. D DU UDU on an 8th/16th grid, downstrokes on beats, upstrokes on the "ands", muted chucks, ghost
  strokes), in contrast to today's single rolled chord. The accompaniment walkthrough's proposal (approved as
  direction): a comp-figure library (lanes: rhythm, tone, accent, length; re-voiced per chord), variation by phrase
  (a planned bar-4 variant, new figure at section changes), thinning by rank, pushes shared with the lead, strumming
  as a figure family, a counter-melody voice later.

### Bass rhythm (spike VNL_BASS, kept as is by Sam to revisit)

- Tones approved by ear; rhythm was the open question. The spike (roots and root-fifth songs only) keeps all four
  cells, one per song: the kick's with beat 3 sat out now and then 34%, half notes on 1 and 3 22%, beats 1 and 2
  22%, beats 1, 2 and 4 22%. Plus anticipation (~35% of chord changes, an 8th or 16th early, ringing over), beat 1
  held and the rest long or short per song, ghost notes (60% of songs, ~30% of strikes). Revisit: the weights, how
  much anticipation, and whether to bring it into the engine proper (it lives in song.c as a spike).

### Sound (judge by ear, Task 8)

Renders: `app/build/new/<seed>/` and `<seed>-nobed/`, one per set (Task 8's report lists them).

- **Rings' reverb and chorus against ours.** Sam picked Rings' (Task 7c); ours stays host-only (`VNL_FX=ours`).
  Rings' chorus wobbles ±14 cents, near its test's 15-cent window: don't deepen it without moving the window.
- **The drums' compressor.** Its attack is instant (a 10 ms attack let the snare's transient through, against the
  peak-to-body test); per-kit thresholds give full-velocity kicks 5.5–5.7 dB and snares 3.2–3.5 dB.
  Too squashed or too little?
- **The chorus and the delay at 7.35 kHz.** Some aliasing in their wet above 3.7 kHz (a cost trade); listen on the
  Rhodes and the Wurli.
- **The pump depths** (subtle 1, medium 3, obvious 6 dB; the 808 pumps about 2 dB deeper, the brushes shallower).
- **The bed's level:** about 35 dB under the music (33–38 by song); `BED=0` renders leave it out.
- **The wind breath levels:** tuned to a test (the breath's residual −30 dB) in Task 5, kept in Task 8; the sax and
  muted trumpet patches moved up as a whole (the breath with them). Seed 4185943442 is the one to check.
- **The section filter amounts** (intro and outro: drums and bass 0.7, keys, lead and candy 0.4; breaks 0.8; the
  drop bar 0.6). Keys in intros sit at about 3 kHz; darker would be the curve's knob.
- **The pad crossfade:** a held pad takes 8 voices and a 2 s release (its old chord rings under the new one's swell);
  a pad's swell takes at most half its note (a pad arpeggio speaks, with a faster swell than the patch's 0.6 s).
- **The mix's balance:** the keys per comping (held keys lifted 2–4.5 dB, nylon 2 more), the upright's body under
  the other basses, the kits' levels (the 808 a little hot), the hats about 10 dB under the snare, the vibes as ear
  candy (in Rhodes neo-soul) about 4 dB over the other candy (the patch is shared with the lead role).
- **The glue compressor** (2:1, −26 dBFS, 30 ms, 200–400 ms auto release, 4.5 dB make-up): 1–3 dB on full sections.
- **The mud high-pass** (135 Hz, 150 on pads): thinner keys, or clearer?

### Synths and the lead's level (judge by ear, synths plan and the lead-level fix, 2026-10-23)

Renders: `app/build/new/<seed>/` and `<seed>-nobed/`, one per synth set, and the 11 earlier sets again (the synths
plan's Task 5 report lists them by set).

- **Sam's cut (2026-10-24, after the synth renders: "Some of these sound bad"):** kept the DX electric piano, the FM
  slap bass, the triangle (chip) bass, the organ, the saw lead and the synth pluck; removed the Juno, the Moog bass,
  the square and sine leads, the chip chords, lead and blip, and the FM bell (patches, the 4-pole, the chip's pulse,
  noise, vibrato and tick, M_SUB's sine, triangle, noise and vibrato; mono glide now only on the lead). The synth
  sets are now DX keys and FM bass, Organ and saw lead, Rhodes and saw lead, DX and pluck, Nylon and synth pluck
  (ids 12, 14-16 retired). The bullets below on the removed patches are history.

- **The new patches' character:** the Juno (a saw and sub under 600 Hz: hazy enough?), the organ (a square and its
  sub, no envelope), the Moog bass (the 4-pole at resonance 0.35, plucked open), the saw, square and sine leads,
  the synth pluck, the DX EP (the 14x tine over a faster body), the FM bass's slap (gone in 150 ms), the FM bell.
- **The glide times:** Moog bass 30 ms, saw lead 90 ms, sine lead 70 ms, square lead 60 ms. Too slidey or too
  subtle? A glide happens only from a held note.
- **The chip grit:** a 7-bit crush over each note's own level (6 bits failed the aliasing limit at note 88). More
  or less grit?
- **The Juno's chorus depth:** 0.8, the deepest of the keys (Rhodes 0.7).
- **The square lead's vibrato:** 10 cents, delayed like the winds' (a voicing choice; the sine lead has 14).
- **The pairings:** the Moog bass in three sets (Juno, organ, Rhodes) and the FM bell in three (Juno, DX, organ).
- **The guitar lick's retone** (the lead-level fix): a longer, brighter ring (3 s, damping 4500 Hz plus 3x the note)
  so it carries a line; its peaks stand out more than a wind's.
- **The lead over the keys:** 4–6 dB in every set while the lead plays (each set's mean 4.3–5.7 dB), the keys ducking
  3 dB under a held lead note (25 ms in, 400 ms out). The duck follows the gate, so a ringing pluck releases it while
  it still sounds.

### Lo-fi / taste

- **Progressions 27–44 (R26)** await curation by ear; 28 and 34 are tonally ambiguous (Task 8c minor).
- **Drum kicks to check by ear** (Task 3 minor): pattern #2 busy (steps 7+8), #5 half-time kick on 2 and 4, bossa
  kick 0,3,8,11.
- **Slow bloom's "long fade"** often lands on a 4-bar last block (Task 5 minor).

## Approved by ear

- **Comping style weights (Sam, after comping-styles.musicxml):** held 55%, stabs 20%, offbeats 15%, arpeggio 10%;
  the comping arpeggio in 16ths (in 8ths it read as a melody).

- **Chord voicing (Sam, after listening to song 1 at c25d4af):** rootless, one voicing per chord voiced as a cycle,
  pianist voice leading (common tones held, top voice first), dominants always extended, no dark re-voicing. E.g.
  Db Lydian loop: Ab3 C4 F4 Ab4 / Bb3 Db4 F4 Ab4 / Bb3 Db4 F4 G4 / Ab3 C4 Eb4 G4. Treat changes to the voicer as
  needing a fresh listen.

- **Instrument spike picks (Sam, 2026-10-25, after the spike demos):**
  - Piano: the Knight upright (VCSL, CC0) for sustained/held chords. Kawai grand for stabs: demo pending a listen
    (1d-piano-*-stabs, spike branch 49f6efc).
  - String-ensemble lead: synthesized (Plaits-style ensemble). The sampled strings were scratchy/grainy, and FluidR3
    cut off awkwardly on quick notes with gaps. Open: report better string samples if found.
  - Cello lead: FluidR3 solo cello (MIT).
  - Strummed nylon: quick strums for every pattern except the ballad, where slow and quick both work.
  - Sam (follow-up): the FreePats nylon "sounds much better than our synth one": it replaces the synth nylon.
    "Definitely keep the Knight piano."
  - Stab comping comes back (Sam: "with better sounding instruments, they may work"), but not the offbeat style
    (strikes only on the "and"s), which stays retired.
  - Next demos: the sampled drum kits from the drum survey, and the jazz guitar options.
  - Jazz/clean electric guitar: Karoryfer Emilyguitar (CC0), "and it's not even close."
  - Drums round 1: Swirly (brushes) is out. The kits' snares are too high-pitched; Sam wants something more subtle.
    The drum demos used the upright bass by mistake; round 2 uses the electric bass.
  - One acoustic piano, the Knight (Sam, correcting): it plays any acoustic-piano part (held chords, stabs, arps, the
    upcoming broken-chord climb, even the lead), as an option beside the electric pianos, which stay. The Kawai is out.
  - Reuse over reinvent (Sam): the cello is the string ensemble synth in a low register, the "low and dark" voicing
    (demo 2c-strings-synth-low-dark: "passing"). The FluidR3 cello is out (28 KB back).
  - Snares (Sam, from the 4-bar lineup): VCSL rope snare with snares off; Frankensnare 20x6 poplar; Gogodze center
    pitched down 3 semitones, low-passed at 3.5 kHz. No cross-sticks survived. The demo hi-hats are disliked: next, a
    hi-hat lineup.
  - Hi-hats (Sam, from the hat lineup): Big Rusty foot chick; VCSL pedal close; VCSL closed, softest layer; Gogodze
    closed pitched down 3 semitones, low-passed at 5 kHz; the VCSL small shaker in the hat role. "Other shakers could
    be good too." Each chosen hat needs its matching open hat (same kit, same processing).
  - Shakers (Sam, from the shaker lineup): only the VCSL small shaker (high fast); the rest are out.
  - Hat pairs (Sam, after 5-drums-hat-pairs{,-poplar,-rope}): all four closed/open pairs, with choke, over all three
    approved snares, "all sound good". Sam will keep listening in future songs and flag what wears.
  - The warm pool kit is approved (Sam, after 5-drums-pool-warm): Gogodze kick and closed hat, the Gogodze sidestick
    as the backbeat, the Big Rusty 22" ride, all down 1 semitone, low-passed at 3.2 kHz, drive 1.2. Note the sidestick
    works here although the cross-sticks failed the raw lineup (processing and context matter).
  - The ride is shared (Sam: "that roll sounds good"): one ride recording any sampled kit can use, following the
    kit's processing. Per song: about 1 in 4 rides throughout (the warm kit's home); about half of the rest switch from
    hats to ride for one section (a B section, the last groove, or a jazzier roll); the rest stay on hats.

### Sound engine: deferred minors (Plan 3a reviews, 2026-10-23)

From the task and final reviews; none blocks merging. Revisit when the area is next touched.

- **Task 1:** bench.c:41 comment claims P_NPARAMS is checked; nothing checks it.
- **Task 1:** timed region includes roll_song/vx_init; fractional step truncation; early song end unflagged.
- **Task 1:** bench.c:22 confusing comment; any '-' arg treated as --heaviest; mem-check over-budget path unexercised.
- **Task 2:** vx_init clears state when vx_active()==0 (voices+drums only) — once reverb/bed exist, a song change would cut their tails; fx_configure would switch bed abruptly; dark snaps to 0. Task 7 must keep FX tails across song changes.
- **Task 2:** bass is a separate bass_t sine with no model dispatch; UPRIGHT (M_PLUCK) / ELEC_BASS (M_BASS) need it (or bass as a mono voice_t pool).
- **Task 2:** model state as flat voice_t fields (a union would cap growth); no carryover test; dark-ramp test readability; patch.h says p[] layouts documented in patch.c — each model task adds its layout.
- **Task 3:** pad releases are cut by 4 ms steals at each chord change in the 4-voice chord pool, then the next pad swells — an audible dip; consider more chord voices for pads or overlap.
- **Task 3:** dark test HF proxy near its noise floor for keys; voices.c:91 long comment; Wurli bark shares the drum noise stream.
- **Task 4:** upright retrigger waits up to ~4.4 ms for the steal fade (timing jitter on walking lines; listen to 4185943442); notes below 28 clamp to 28's pitch; guitar lick decay untested.
- **Task 5:** breath test has no negative control (reviewer measured −48…−62 dB with breath off vs −26 dB on; −30 dB bar); comb uses nominal period; `default:` routes unknown models to bass (prefer explicit case M_BASS); candy peaks 0.059–0.060 vs 0.05 floor; breath levels tuned to the test — Task 8 confirms by ear (4185943442).
- **Task 6:** vx_kick_env is the raw exponential (808 tail ~2.2 s) and block-granular — shape it for ducking.
- **Task 6:** drums.h claims sounding hits keep their kit (they read kit params live); vx_set_kit stores cfg.kit unvalidated; stale voices.c:251 header; retrigger resets phase/filters (pre-existing); test chunk-boundary margins; no vx_configure→kit test; hat/shaker/rim amps for Task 8 by ear.
- **Task 7:** quick replay within ~40 ms of stop can return a stale plate tail; 64 KB memset in the ISR at song start; plate decimation aliasing; send not darkened in dark sections (deliberate); bed fixed absolute level vs quiet songs; no end-to-end test of mid-song reconfigure (no knob yet); fx_tiny test hook ships in firmware; fx.c:173 long comment; pre-existing UBSan report at fastmath.h:58; set change re-sweeps a ringing kick.
- **Task 7b:** comp() pre-scan/post-pass runs with no drums active; fx_chorus_dry called 3× per chunk.
- **Task 7b:** attack gain step at block start can tick on a sounding 808 body; break+drop-bar holds the sweep at 0.6; no NaN/denormal check for bus filter/compressor states; chorus dry compensation lowers chords' sends ~1.4 dB, sends now post-filter (untested); test hooks ship in firmware (vx_fx_off, fx_wet_off, ~1 KB triple buffers); PM_*↔V_* mapping by enum order; a migrated test's comment/printf disagree; thin margins (scenario −8.8 vs −9; delay −63 vs −60).
- **Task 7b:** fx_song lands dl_t immediately, so a still-ringing delay tail jumps (~0.11 s read shift on a 70→85 BPM change) — keep the glide for a ringing tail; the fx.c:54-56 / fx.h:138 comment overstates 0.25 ms.
- **Task 7c:** half-rate note grid skips each patch's top half-rate notes (pads up to ~96 untested; consider capping pads at 84 or extending the test); quarter-rate plate input aliasing untested and its comment's rationale wrong; hpar/hx/hr_tail not reset per song (harmless at 256-sample calls); half-rate also applies to lead/candy M_EP/M_PAD patches, untested; stale "triple" names; cost-gate.sh needs mkdir -p; noisy per-case test output.
- **Task 7c:** two overlong comment lines (rings_fx.c ~220, song_test.c ~4688); Rings chorus wobble ±14 cents near the test's 15-cent window (Task 8 must not deepen it without moving the window); Rings CPU unmeasured on device.
- **Task 8:** quiet blocks allowed +1 dB over full band (brief: never louder); 044f590 alone breaks the web build (squash with a803785 before merging?); glue test margins brittle by design; glue's reduction doesn't reach the reverb return (wet rises 1–3 dB in dense passages); voices.c includes harmony.h for keys_gain (engine→song dependency); stale log paths in 2-set4 and -nobed renders.
- **Task 8:** picker time stated inconsistently (~25 s vs 40 s); a long line in docs/cost.md; an older cost.md paragraph keeps pre-fix seed-7 numbers as history.
- **Final:** squash 044f590 with a803785 (web build) — not done (history rewrite; Sam's call at merge); memsets in the ISR at song start (~52 KB on device) — measure on device; unused plate/our-chorus code and ~1 KB pool on device; plus the earlier deferred minors the final review triaged as "To revisit".

### Synths and levels: to judge by ear (mix-synths, 2026-10-23)

- Two old-set instruments moved for the lead-over-keys rule: the muted trumpet +1.0 dB, the felt piano +0.6 dB.
- Glide: a mono synth glides from a note that ended under 25 ms earlier, within a 4th (21–58% of note changes);
  a glide after silence re-attacks over the patch's attack and does not re-strike the filter envelope.
- Thin level margins (a future voice change needs a rebalance): Felt and trumpet's median 5.9 of 6 dB; the lowest
  songs +2.4/+2.5 dB; Felt and flute's mean 4.3. All rulings: docs/superpowers/plans/2026-10-23-synths-ledger.md.

### Sustained lead: open items (2026-10-24)

- On a key change, about half the songs revoice the percussive keys an octave away; the sustained lead's range
  follows the keys' top, so its answer jumps 4–8 semitones. Fix in the voicer/register, not the lead.
- About half the arrivals on prog 23's home chord are reached by more than a step.
- Thin, deterministic margins (contour 90.6% of 90%; Nylon and sax and Rhodes neo-soul medians at 6.0 of 6): a future
  voice or level change will need a rebalance. All rulings: docs/superpowers/plans/2026-10-24-sustained-lead-ledger.md.

### Pads: done (2026-10-24)

The background layer (pads design and plan, 2026-10-24; branch mix-synths): a 2-note chord part (the 3rd and the 7th)
over percussive keys and a high solo note on its own voice, strings or a warm pad per set, 4-8 dB under the keys
(test_pad_level). Budget: the bus's filters moved to the layer's quarter rate, then the design's cut (c), the 2-note
chord part; cut (a) (the solo note riding a chord voice) saved nothing and was undone (Sam). Seed 20 forced about 0.97
on the host bench, accepted by Sam (under 1.0; the device is measured in Plan 2): docs/cost.md. Host renders can A/B
the chord part against a doubling of the keys' voicing: `PADS=double ./new-song.sh SEED`.

To judge by ear:
- The levels: the chord part 4-8 dB under the keys (set medians 4.1-7.8); the solo note's level (not tested).
- The solo note's shares by section (groove 60%, intro/break/outro 50%, under the lead 30%).
- Strings vs the warm pad, per set.
- The 2-note chord part: if thin, a follow-up wins CPU elsewhere and restores 3 notes.
- The chord part vs PADS=double (the keys' own voicing doubled), Sam's A/B.
- The solo note's quarter-rate grit: about -31 dB of aliasing at C6; half rate for MIDI 72 and up is the fix if heard.
- (The solo note against the lead is already in "To revisit".)

### Pads revision 2: done (2026-10-24)

After Sam listened (pads design, "Revision 2"; branch mix-synths): the pad layer plays **either** its chord part
**or** its solo note in a section, never both; over struck keys the chord part is the default and the solo takes about
25% of sections (intro and break 35%, groove 25%, under the lead 15%, outro 30%), over sustained keys the first
design's shares. **40% of songs never play the solo** (my call); the rest roll its voice per song from the choir, the
flute and a soft organ (MIDI 72-84) and the strings an octave up (84-96), equally likely, never the flute under a flute
lead nor the organ over organ keys, each level-matched (within 0.5 dB at the songs' velocity 58). The chord part sits
about 8.5 dB under the keys (set medians 7.4-10.2; its velocity 48, Sam's "3 dB down" kept against the retuned keys).
**Half-note comping** joins the keys: struck on beats 1 and 3, the second strike at 90% velocity, held to the next,
thinned to the chord changes under a busy lead; it holds instead over sustained keys and over a kick on exactly 1 and 3
(Sam's kick rule). Comp shares as heard: held about 55%, half notes 25%, arpeggio 20% (weights 44 / 36 / 20).
**Levels retuned with half notes in** (gains only, no bound widened, no seed re-picked): the keys per comping held a
bar 3.75 dB (was 2), held two bars 4.5, half notes 0.75 (new), an arpeggio -0.25 (was 1), nylon +2 on held; half
notes that hold sit as held (they had fallen to the 0 dB default). Lead over keys: median +4.8 dB over 96 songs, set
means 4.2-6.4; the thin margins: Rhodes neo-soul's lowest song +2.0 (its seed 4, held two bars, unchanged from
before), City pop Rhodes's mean 6.4 of 6.5 (one busy two-bar half-note song at +13.8: its keys strike once per two bars
under a busy lead and the Rhodes dies away in the second bar). Budget: seed 20 forced 0.960-0.972 on the host bench
(Sam's ~0.97 line): docs/cost.md.

To judge by ear:
- The solo voices and their level: choir, flute, organ, strings an octave up; level-matched to each other, under the
  chord part; is any too quiet or too present?
- The either/or shares: does the solo come often enough over struck keys (about a quarter of sections), and does the
  chord part cutting out for it read as a change of color?
- The 40% of songs with no solo at all.
- Half-note comping: the second strike's softness (90% velocity), the thinning under a busy lead (two-bar chords go
  quiet in their second bar), and the kick rule.
- The keys' new levels per comping: held one-bar songs up 1.75 dB, arpeggios down 1.25 dB.
- Renders (one per set with and without the bed, plus half-note songs): .superpowers/sdd/2026-10-24-pads-rev2/task-4-report.md.
