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

### General

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
