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

**Families (Sam, 2026-10-29): the synth/acoustic divide applies to drums only.** Keys, bass, pad and lead may mix
across families; a song's family chooses its drum kit only. (Replaces "supporting roles swap automatically".)

## To revisit (Sam, after listening)

- **Cut-A-Dor's hat pocket may be too loose (Sam, 2026-11-10, seed 560: "a little too loose on this render, but
  maybe they'll sound better on others, so don't worry about it").** Approved and merged as is; if it comes up
  again, the per-song amount (0.5-1.0) or the off-16th lag (4 ticks, ~47 ms average) is the knob.
- **Parked (Sam, 2026-11-03: "Let's forget about them and see if I ask about similar things later").** The
  listening set and its switches were deleted. Don't raise these again unless Sam asks about something similar:
  - The lead's level under the styles: median 6.2 dB over the keys (was 8-10); set 28 seed 12 at -0.7 dB.
  - The old lead rules the styles dropped (sustained no-arpeggio, percussive 80%, the Motion lever, the bass re-roll
    reaction): none changed a note in the demo songs.
  - Duplicate sets: 19 and 31 are identical definitions (bit-identical renders); 29 and 32 differ only in bass and pad.
  - The lead's pushes: the 377 to 190 drop is mostly the test missing '60s Jazz's pushes plus fewer Bossa Nova songs
    (32 to 13 of 300); all-notes count 542 to 339. Lever if wanted: Bossa Nova's roll weight.
  - '60s Jazz and Impressionism drop notes when the workbench raises the keys (seed 34: 13 of 28).
  - The organ register split (65% at register with the 16' out). Engine fact: the octave-up organ plays ~1.7 dB
    quieter than at register.

- **Hip hop record-style repetition for leads (Sam, 2026-10-29).** Claude's reading, to confirm with Sam: the lead
  behaves like a sampled phrase lifted from a record — a 1-, 2- or 4-bar phrase generated once, then played back
  identically every pass (same notes, timing, velocities, even its "imperfections"), the way a producer loops a
  sample. Variation comes only from producer moves on the loop, not new notes: muting it for a bar or a section,
  filtering it (low-pass in, open up), chopping or re-triggering a slice (stutters, a repeated last beat), dropping it
  out before the drop, or re-pitching the whole loop to follow the chords. Builds on Jazz-Hop ("loops are actually
  good") and the hybrid-styles idea. **Sam confirmed (2026-10-29) and added the key effect: the stuck-note stutter** —
  a few notes of the phrase "get stuck" and repeat a few times (like a skipping record or a re-triggered chop,
  e.g. one note or a 2-3 note fragment repeated 2-4 times in 8ths or 16ths) instead of continuing, then the phrase
  carries on (or picks up where it would have been). Repeats are the same notes as recorded, maybe each a touch
  softer or filtered; placed at phrase ends or turnarounds, not every pass.
  **Plan (Sam, 2026-10-29): after the eight styles are built as approved, prototype loop playback as a shared engine,
  as a toggle** (per song, a workbench row "Lead loop: on/off"; the firmware default decided by ear later). With it
  on, a style generates its phrase once (1-4 bars, matched to the chord loop) and a shared player replays it
  identically, adding the producer moves (mute, filter, stutter, drop-out); with it off, the style plays as built.
  Start with Jazz-Hop and Ambient. Why: simpler logic (no per-pass variation; rules checked once per phrase), less
  CPU and likely less flash per style. Catch: the bass's walking stretches, the pad's sections and the keys' room
  differ by section, so the phrase is checked against every bar version it plays over, with rule-driven nudges
  where it would rub.
  **Done (2026-11-03):** Jazz-Hop hooks: at least 3 pitches spanning a 4th (Sam: "I guess this is better. Keep the
  change"); 57 of 1000 songs had 2 pitches, now 0.
  **Done (2026-11-03):** the lead loop: a per-song switch (workbench row "Lead loop"; firmware default on since
  2026-11-04) makes Jazz-Hop and Ambient record their lead and replay it over the chord loop (2 or 4 bars) with
  producer moves; other styles ignore it. Cost: flash +3,648 B (image 402,888 B, about 6.7 KB under budget), RAM
  +288 B, worst tick unchanged within noise (docs/cost.md).
  **Done (2026-11-04):** every lead style loops (Sam: "Apply it to the rest of the genres"); Bossa records a riff
  (bar 2 answers the motif; "C works"); a loop pitch floor for every style (4 pitches, 3 under 8 notes, over a 4th;
  Modal Jazz 23% of loops under it to 0%; Sam on the A/B: "yes"); **on by default** (Sam: "default on, merge"),
  lead-loop 8c230af. Image 404,824 B (4.8 KB under budget). With it on, four loop bugs fixed that Sam hasn't heard
  (a stutter ringing over the bar line, stutter repeats off the chord on beats 1 and 3, an octave change not
  re-recording, harmony nudges folding hooks).
  **To revisit (by ear):**
  - Levels with the loop on (the level tests run loop-off): vibes -0.80 dB from the sample's (vs -0.33 off); set 28
    seed 12's lead -1.2 dB under its keys; Knight and Emilyguitar leads 1-1.6 dB hotter against the flute;
    Wurli/Knight's struck lead sings in 62% of bars (71% off). Rule on each, then add loop-on versions of the checks.
  - Jazz-Hop's hook floor with the loop on: hook-bar stutters fold about 2.7% of hooks (by design) and harmony
    nudges another 0.6% (capped at 0.75%). Accept as the loop's character, or keep stutters off hook bars.
  - Fixed (2026-11-04, loop-leftovers): on a live change the entry filter opened one step late (a bright first note
    in about 4 of 36 mid-run key changes). The loop's filter is now computed once per step, after the tick, for the
    16th it played (song_lead_lp_at); all 19 filtered entries in the probe are closed from step 0.
  - Old workbench snapshots and feedback items (before 2026-11-04) now replay looped: a snapshot without the Lead
    loop row restores to the default, which is now on. Set the Lead loop row to Off to hear the old phrase.
  - Known effect: the keys-up override (keys_oct) test's 80% "keeps its room" check runs loop-off. With the loop on,
    a keys-up song's lost lead notes in the recorded bars repeat on every pass (seed 8: 47 lead notes, 59 off).
    Workbench only (keys_oct is an override); no fix planned.
  - '60s Jazz on felt piano or vibes plays only its targets in about 3% of songs (2 notes per 4 bars), loop on or
    off (seeds 67, 71): its lead-ins drop out. A fix in jazz60s.c.
  - **Bolder stutters (Sam, 2026-11-05: "the stutters are the thing that is too subtle"; on the short list, not
    started).** Today: 40% of turnaround bars and 30% of block-ending bars, 1-3 notes repeated 2-4 more times in 16ths
    or 8ths, each repeat 12% softer, so a stutter lasts under a beat and fades into the phrase. Proposal: 4-7
    repeats (one to two beats), mostly 16ths with an occasional 32nd roll on the last beat, repeats at full level
    while the lead's low-pass closes to about 600 Hz by the last one (the entry filter's), snapping open when the
    phrase resumes. **More often too** (Sam, 2026-11-05: "Another reason the stutter might be too subtle is how
    infrequent it is"): today at most 40% of 8-bar passes (40% of turnaround hook bars, 30% of block ends, none in the first
    lead pass). Measured (7,112 songs, every style): 0.68 stutters a song; **49% of songs never stutter**, 36% once,
    14% twice, 1% 3 or more. Propose about 70% and 50%, measured per song with the loop stats, and A/B both changes against
    today's stutter.
  - **The stutter on the comp too (Sam, 2026-11-05: "add this stutter to the comping"; on the short list, not
    started).** After the bolder lead stutter, the comp gets the same move: a chord (or its top notes) stuck and
    repeated with the closing filter, at the comp's own turnarounds. Struck comps only, never held ones (pad, organ).
    Most important where there is no lead (Sam, 2026-11-05). Never in the same bar as a lead stutter. How often: Sam
    has no idea yet; propose a rate with renders.

- **Hybrid lead styles (Sam, 2026-10-29).** One lead style makes the motif, then a different style takes it over and
  develops it: after 1 bar, after 3 bars, or for the second 4 bars of an 8-bar phrase. Needs the styles to share a
  motif format (rhythm + contour + scale steps) so the second style can restate and vary the first's motif. Builds on
  the shortlisted styles (Impressionism, Romanticism, '60s Jazz, Modal Jazz, Jazz-Hop, Film Score, Bossa Nova,
  Ambient).

  **Dropped (Sam, 2026-11-04):** the lead loop doesn't let the motif evolve, so a second style taking it over has
  nothing to develop.
- **Piano-roll editing paused (Sam, 2026-10-29: "Pause piano roll editing until later").** Plan
  docs/superpowers/plans/2026-10-29-patterns-and-edits.md Tasks 5-6 (edit glue, then editing with a "why" per edit)
  not started; Tasks 1-4 (doubling switch, pattern export, import, candidates) done.

- **Vibes as a comping (keys) instrument (Sam, 2026-10-29).** Today the sampled vibes are a lead only. Add them to the
  keys: which comping styles suit mallets (held chords ringing, half notes; maybe broken chords), voicing range against
  the 5 sampled zones (C4-E6), level against the lead, and which sets/families take them (electric: goes with either).
  Never vibes on keys and lead in the same song.
  **Done (Sam, 2026-11-05: "vibes with the motor on definitely work"):** the vibes on the keys, motor always on (0.9 at
  4 Hz), four mallets from F3 struck together; held, half notes, held on 2, held on 2 and 4 (no stabs, strum or light
  eighths); never with the vibes lead. Sets 34 Vibes and flute, city pop (warm, sunny), 35 Vibes and Knight (wistful,
  warm), 36 Vibes and felt (sad, wistful), 37 Bossa vibes and flute (warm, sunny).

- **The melody path never sounds (found 2026-10-29).** Since the ear candy went, the struck melody (melody.c: motif,
  rhythm cells) is dropped in every block the lead plays and turned up in no song over 400 seeds, yet it's still
  built per song (CPU for nothing) and exported as the melody-cells pattern family. Audible or cut: cut the whole
  path (and its pattern family) or give it a place to play. Sam to decide.

- **Review the rules for the rule-based parts with Sam (Sam, 2026-10-29).** Bass lines, comping voicings, pads and the
  sustained lead are generated by rules, not tables. They're written up in docs/music/general.md (musicianship, with
  file/function status) and docs/music/lofi.md (genre and taste), last updated 2026-10-24, so they're stale (ear
  candy, upright, sax/trumpet/cello leads, the pad's on/off and fades, the lead changes). First refresh both against
  the code, then walk Sam through them part by part (rule → numbers → proposal → one question), alongside his
  piano-roll edits and their "why" notes. Related: turning these parts into chord-relative patterns he can edit as
  files (he's thinking about it).

- **Bookmark: full listening pass after the GPL imports (Sam, 2026-10-26):** "I want to hear everything again once we
  finish switching over to the stuff we're importing from gpl projects." Then re-render sets 21-33 and pick up:
  - Set 26's guitar sound: "not sold on the sound. I can't quite pin down the issue." My guess: the Emilyguitar
    sample loop sustains held and half-note chords like an organ; offer an A/B with a natural decay.
  - The "vibes" (INS_VIBES, an M_BELL FM patch) sound like a plucked synth, not vibes: re-voice the bell (soft
    mallet, motor tremolo, longer ring; no flash) or sample a vibraphone (~30 KB; ~23 KB left). **Done (2026-10-29):**
    the VCSL vibraphone, sampled (see Leads below).
  - Still unanswered: the C3 guitar floor; half-rate sampled kits A/B; 16 vs 32 kHz cymbals.
  - **Families (Sam, 2026-10-28):** acoustic vs synth/digital, electric (Rhodes, Wurli, electric bass, vibes,
    guitar lick) goes with either ("sure, we can try that"). A song's supporting roles (bass, pad, ear candy) and its
    drums swap automatically to match its family: "YES". Drums reset: none of our synth kits stay (808, Dusty, Soft
    out; "None of our 3 kits should remain"); the synth side liked: poketrack 808, CR-78, 909 and Optimist BOOMBAP,
    LO-FI, vintage (not X0X 808); the acoustic kits will be shaken up (original, unmixed kits; pick pending). Next
    question: should the drum patterns split by family too.
  - **Drums, 2026-10-28 later:** the synth side is now poketrack 808, poketrack CR-78 and Optimist LO-FI ("get rid
    of the 909, boombap, and vintage"); Optimist LO-FI's ride needs a substitute (p13 k5 at 0:22). Kicks: two-bar
    kick phrases; k2-k6 "could all work. we don't always have to do k5-k6" (k2 0,10; k3 0,7,10; k4 0,6,8; k5 0,10 |
    0,7,10; k6 0,6 | 0,3,10; research in docs/research/hiphop-kicks-2026-10-28.md). A "drummer layer" for acoustic
    songs ("that will help a bit"; Sam may want it always on): 8th hats with a strong-weak accent, two-bar variation,
    wider dynamics, kick feel, tom fills and a crash.
  - **Synth ride section (2026-10-28):** no ride substitute worked (LO-FI ride short, its open/closed hat, poketrack
    808/CR-78 cymbals, 909 ride); percussion layers (808 congas, CR-78 bongos martillo/sparse, LO-FI congas) "none
    work"; the synth kits' shakers "all sound like closed hats". Untried: the clap layer, or no ride (hats carry).
  - **Workbench (2026-10-28):** Sam wants a local web UI to test the songs and pairings: change ANY decision or roll
    the dice on one aspect, and give feedback; desktop only, not the FM-1. Being designed (brainstorm).
  - The 16th arpeggio comping is off on every instrument (guitars cf0dca0, keys 3025034): "We may bring it back
    later, but not right now" (Sam, 2026-10-28). The piano climb may take its place.
  - Settled this pass: Knight stab level fine; Knight no-roll fine; ride quarter notes at most with a 1.5 s tail
    (7773f9d, a162c90); guitars never arpeggiate; sampled hats/shaker play 8ths below 55% swing; the vibes lead uses
    the phrase planner (the last three in commit "Set 26 fixes", tests not run at Sam's request).

- **Piano broken chords take the rolled-chord role on the acoustic piano (Sam, 2026-10-25):** the bottom-up roll
  (10-60 ms a note) on the Knight's stabs in set 30 "is not how people usually play the acoustic piano. The strumming
  affect. Electric pianos work with it, but not acoustic." The Knight now strikes together; when the broken-chord climb
  is built, use it for at least the Knight under a sustained lead (set 30's situation), and consider it for the
  electric pianos too.

### General

- **Direction: the workbench becomes a device UI in the browser (Sam, 2026-11-05).** "Eventually we'll probably do
  away with the workbench and ONLY have an in-browser UI representing the device. And it's controllable from the
  device USB. Many other firmwares do that." Until then, the workbench is where we work out what the device can do
  (not one to one): parts as sections now, then a live mock of the main screen and its per-part sub-screens. Later:
  the browser page talks to the FM-1 over USB to show and steer the playing song.
- **Device buttons, not just screens (Sam, 2026-11-05: "we should start thinking about not only the screens, but what
  the buttons might do").** First rule: any register/octave choice (lead register, comp register, any later octave
  row) is changed on the device by the octave up/down buttons while that part's page is showing. To do: map every
  FM-1 button and knob to a role on each screen (home, each part's page, preview), alongside the screen designs.
- **A mixer screen (Sam, 2026-11-05: "a screen to show a mixer with volumes, then that can be modified").** The
  device (and the browser UI) gets a mixer page: each part's level (drums, bass, comp, pad, lead, texture/bed) shown
  and adjustable, per song. To decide with the screens: whether a mix change is saved with the song, and how it sits
  on the knobs (one part per knob, paged).
- **An "Advanced" section for the rules (Sam, 2026-11-05: "having many (all?) of the rules we have toggle-able into
  the device's UI under an advanced section").** Each musical rule the engine applies (e.g. calm songs thin or drop the
  hats, motion leans the draws, the lead loop, busy lead thins the comp, the keys never copy the bass or kick, hats
  sometimes drop in breaks, dominants without the 7th) gets an on/off (and, where it has one, its number) in an
  Advanced area of the device and browser UI, defaults as today. To do first: an inventory of the rules (from the
  ledger and the rule docs), which ones make sense to expose, and how they're saved with a song.
- **Transport: no autoplay, stop/play continues, a button to reroll (Sam, 2026-11-06: built on branch
  transport-no-autoplay, to merge).** The
  firmware boots stopped (a song ready, silent until PLAY). PLAY after a stop continues the same song, from the start
  of the bar it stopped in, instead of rolling a new one. A separate button (TBD; provisionally SEQ, one constant)
  rolls a new song: playing, it starts with the usual between-songs gap; stopped, it waits for PLAY. The song's end
  still moves on to a new song. The screen reader says "NEW SONG". To settle: pending knob requests across a stop,
  and the button (try it in the workbench mock first). Built: the screen says STOPPED at boot and stopped, NEXT SONG
  in the gap (SEQ while playing cuts to it); a change waiting at a stop lands on the bar it would have (the knobs do
  nothing while stopped); the button is B_NEW (= B_SEQ) in app.c. Open: the workbench mock's SEQ (its reference
  screen has none), and whether the stopped screen should show the song that's ready.
- **User patterns made on the device (Sam, 2026-11-06: future, not started).** Users create their own drum patterns,
  comp rhythms and the like on the device, saved on the device (its data flash) and drawn by the roll beside the
  built-in ones (the built-ins are data already: patterns/model.json). Stretch: a companion website that edits them
  in the browser and uploads/downloads them to and from the device over USB (the same SysEx upload as the sample
  banks; see "From other FM-1 firmwares", item 3). Also song layouts (Sam, 2026-11-06): users build their own maps (the
  song's sections in order and their lengths: intro, groove, lead, break, outro), drawn beside the built-in ones.
- **A free-flowing song, no map (Sam, 2026-11-06: future, not started).** A switch turns the song's map off. The
  user gets a screen of the sections there are to go to (intro, groove, lead, break, outro and the like) and jumps
  between them with the piano keys, as the song map's white keys do (each queued change lands on the 4-bar boundary,
  marked while it waits). With nothing queued, the section playing repeats. Goes with the song map screen and its
  key jumps in the workbench mock (2026-11-06) and with user song layouts (above).
  **Record and play back (Sam, 2026-11-06).** With the map off, the user can record the song they make: every manual
  change as it plays (each section jump, knob turn, instrument or rhythm change, mute, solo, level, re-roll), with
  the bar it landed on, over the song's seed and starting state. Playback replays those changes on the same bars, so
  the take comes back exactly (the engine is deterministic from the seed). Saved on the device beside saved songs
  (PRESET browses them); REC arms and stops it.
- **A section without the comp (Sam, 2026-11-06).** The engine can already play a block without the keys (a plan
  block's layers without L_KEYS: the pad goes with them, song.c's pad_part), but the planner never plans one: every
  section's skeleton has L_KEYS (plan.c). To decide, by ear: how often (e.g. one break or groove in some songs, drums
  and bass alone, or drums only), and where (never the intro or the outro). Also a manual switch for it (a part's mute
  is the listening-only version today).
- **The song planner changes an instrument mid-song (Sam, 2026-11-06: future, not started).** The plan can switch a
  part's instrument at a section change (the pad, the comp, the lead): e.g. the comp moves from the Rhodes to the
  felt piano for the break, or the lead from the flute to the vibes for the second lead section. To settle: which
  parts, how often, which pairs go together (the family and the set's rules), and how the screens show it (the
  main screen's tile, the part's "as rolled").
- **A "more movement" display setting (Sam, 2026-11-06: future, not started).** The Parts screen's piano roll steps a
  bar at a time by default (live scrolling was too much motion), and in that default reduced motion no note lights
  up as it sounds and played vs to come changes a bar at a time (Sam, 2026-11-06). A setting for more movement turns
  on smooth, live scrolling with the sounding notes lit (the mock's roll item already takes `live: true`), and perhaps
  other motion (a gliding playhead, animated transitions). Belongs with the device's settings / the Advanced section.
- **ENV: a part's sound page (Sam, 2026-11-06: future, not started).** On the device, ENV (the FM-1 convention: the
  current track's envelope) opens the current or highlighted part's sound: attack, release, brightness, a little
  drive, and the instrument's own controls (the organ's drawbars, percussion and chorus: the organ lead waits on
  them; the pad's swell, the Rhodes' bark). EDIT stays what the part plays; ENV is how it sounds. Needs those
  controls in the engine first.
- **The song's energy over time (Sam, 2026-11-06: an idea, not started).** A curve of how busy and loud the song is
  through its sections (the intro builds, the break drops out), shown as an envelope for the whole song, with knobs
  for a flatter or more dramatic arc. A generative player's own page; where it lives on the device is open.
- **Done (Sam, 2026-11-11): the Espresso theme and the boot splash (design C1).** The screen's background is dark
  roast (#211710), its neutrals warm, its white cream; amber stays (fm1-base's gfx.c palette ESPRESSO, VNL-1's
  APP_PALETTE 5; other fm1-base apps keep black and white). No THEME setting on VNL-1: it is built in. At power-on
  half a record enters from the left (screens.js's splash, a `record` draw item: grooves, an amber label with a cream
  dot and a wedge of light so the turn shows), the wordmark "vnl-1" (its own small face, MARK), an amber rule, LO-FI
  GENERATOR. The record is still while the platform boots, then spins up, slows and settles over 1.3 s with the engine
  and USB already running (nothing starts later); any control ends it. No reduced-motion setting exists yet
  (Settings' MOVEMENT is a placeholder): the splash turns regardless.
- **A simplified and an expanded main screen (Sam, 2026-11-06: an idea, part of the screens work).** Two main
  screens: a simple one (e.g. the chord and where the song is) and an expanded one (the song and every part's rolls);
  the expanded one opens from the simple one, and the user sets which is the default (HOME's). Mockups A-C in the
  workbench's device mock are candidates for either.
- **From other FM-1 firmwares (Sam, 2026-11-05: on the short list, not started).** Evaluated setBfree-fm1
  (github.com/R-Jeb/setBfree-fm1), Cut-A-Dor (github.com/kidbee85-beep/cut-a-dor-release) and the B1 Drawbar Organ
  (github.com/SikkenWorks/B1-Drawbar-Organ). License per item: GPL-2.0-or-later code may be copied (credit it);
  GPL-3.0-only code may be copied too (corrected 2026-11-07: an earlier "approaches only" here was wrong; the
  shipped firmware is then GPL-3.0-only as a whole, our files keep "or-later"; see "Borrow, don't rebuild").
  1. **A rotary speaker on the organ comp** (setBfree-fm1 `organ_fx.h`, GPL-2.0-or-later: copyable). Horn and drum
     with Doppler and speed changes; `LES_REFL2 0` saves about 30% of its CPU. About 28 KB of tables (over today's
     image headroom: decimate them or put them in the data area) and 8 KB of delay lines.
  2. **Organ key click and key compression** (from setBfree itself, GPL-2.0-or-later). Random click envelopes, a
     percussion click, the gain dropping as more keys are held. Cheap.
  3. **Sample-bank upload to the data area** (Cut-A-Dor `docs/PROTOCOL.txt`, `editor.c`; GPL-3.0-only). **Done
     2026-11-07, our own code** (d0e858f, tools/fm1_bank.py). Banks read in place from data flash; uploaded over SysEx, erased 4 KB at a time while stopped, CRC32,
     the header written last so a half-finished upload never counts. One protocol for moving our banks off the image
     and for the browser UI. Goes with the sample-banks-to-flash item.
  4. **A captured-slice stutter** (Cut-A-Dor `cad.c`; GPL-3.0-only: copyable). Repeats a slice of the audio with edge fades,
     a gate, reverse and a brake. A base for the bolder stutters (lead and comp), with our closing filter added.
  5. **CPU tools** (B1, setBfree-fm1; copyable). An on-screen peak-load and missed-deadline readout, the
     idle instruction in the main loop, and redrawing only the screen rows that changed.
  Skip: B1's update and recovery code (unverified on hardware by its own notes).
- **Borrow, don't rebuild: GPL-3.0-only code is copyable (Sam, 2026-11-07).** VNL-1 is GPL-3.0-or-later; copying a
  GPL-3.0-only file makes the shipped firmware GPL-3.0-only as a whole (the workbench wasm already is, for the
  Optimist drums). Copied files keep their SPDX header and copyright; credit in CREDITS.md. Still out: GPL-2.0-only
  and unlicensed code. Older notes saying "ideas only" for GPL-3.0-only (docs/research/fm1-accompaniment-code.md,
  other-firmwares.md, written in the MPL days) and the 2026-11-05 "approaches only" above are superseded. Porting
  caution: integer DSP (Felucca, Optimist) can hit the FM-1's 64-bit-multiply fault (base/bsp/docs/CPU.md): check the
  disassembly; float code is safe.
  Built our own where a borrow existed: the sample-bank upload (Cut-A-Dor; works, keep ours), the device's SAVE CPU
  readout (Optimist's `core/audio.c` `audio_max_us` from TIMER4 measures the real audio time: swap in), the lead
  loop and comping (Tuna, LOF-1; approved, keep ours).
  **Short list (Sam, 2026-11-07: "stutter will definitely be on the shortlist, as will all the new drums"; after
  the work in progress):**
  1. **The bolder stutter, lead then comp**, from Cut-A-Dor's captured-slice stutter (`cad.c`): slice repeats with
     edge fades, gate, reverse, brake; our closing filter added.
  2. **New drums:** Optimist's kit recipes (`tools/gen_drumkits.py`: BOOMBAP, LO-FI, JAZZ, PHONK), Cut-A-Dor's
     boom-bap drum generator (patterns and kick phrases), PERF-1's LOFI genre, zp12's 12-bit grit.
     **Optimist's kits in (2026-11-10; all three approved by ear, Sam: "both of these sound nice", Phonk "adds variety"):** BOOMBAP, JAZZ and PHONK on the Crunchy machine's engine
     (data only), as the Boom-bap, Jazz and Phonk machine (kit rows 22-24, appended past the sampled kits). All three
     are approved synth kits: a synth-family song (today the chillwave sets 5 and 10) rolls one of the six synth
     kits. Phonk is built aggressive (driven kick and snare); its defining
     sounds (the blown-out long 808, the melodic cowbell) are lanes our songs don't play, and in a song it measures
     close to the 808's color. BOOMBAP was cut on 2026-10-28 ("get rid of the 909, boombap, and vintage") and is back
     on the 2026-11-07 short list. Costs in docs/cost.md.
     **Phonk machine retired (Sam, 2026-11-10: "i'm changing my mind about phonk machine. get rid of it"):** the
     kit row keeps its value (a saved song on it falls back like any retired kit); not rolled, not offered.
     **Rejected (Sam, 2026-11-10: "none of these 4 nor their 808 or CR-78 are good"):** Optimist's AMBIENT,
     SYNTHWV, MINIMAL and LATIN, and its 808 and VINTAGE (ours, FOMAJAM's, stay). Built and heard on the
     branch kept at `rejected-kits-0d857f8`; not to be offered again. Optimist's other kits are club styles.
     **Cut-A-Dor's drum programming in (2026-11-10; to judge by ear):** three pieces, each narrowed to what we
     lacked. (1) The drum pocket: its lo-fi `cad_step_tick` timing over our swing, times a per-song amount of 0.5-1.0
     (its HUMAN, 0.65 by default): a kick off the beat 2 ticks of 96 PPQN late, the bass's roots with it; the closed
     hat 1 tick ahead on the beat, 3 on the and, 4 behind on an off 16th. Its snare (+6) and its random human timing
     we had (our snare 13-27 ms, our per-hit jitter). (2) Kick phrases: Sam's k2-k6 were the workbench's only; now a
     boom-bap song (patterns 1-3) plays an approved phrase in 30% of songs, and four of Cut-A-Dor's sixteen two-bar
     conversations join them (k7 0,6,10 | 0,7,10,14; k8 0,8 | 0,10,15; k9 0,3,14 | 0,3,7,10; k10 0,6,11 | 0,3,14;
     the rest are busier, double kicks or kick on the backbeat). The device's KICK row forces one (OV_KICK). (3) Its
     rhythm families: for the boom-bap/lo-fi styles they are the kick conversations plus eight hat bars; a boom-bap
     song plays a hat figure (bar A and B) in 25% of songs. Over 4000 songs: 1316 boom-bap, a phrase in 28%, a hat
     figure in 24%, both 8%, the pattern as written 55%. Not taken: its other styles (funk break, fast break, half
     time, hip-hop), its evolving mutation (random kicks every 4 bars), its fills (ours) and its CC0 samples (the
     bank is full; its Virtuosity Drums vintage kick, snare and closed hat, about 10 KB as our ADPCM, are a possible
     future swap).
  Also copyable, not on the short list: Optimist's load guard (`system/cpuguard.c`: steps quality down before a late
  block), its spring reverb (`fx/spring/spring.c`), SLOOP's synth engines (the two-engine spike: code, not just preset
  values), Optimist's dual-core split (`system/dual.c`; CPU1 is unreliable on some units).
- **A "B" section in the song planner (Sam, 2026-11-08: ledger, not started).** Today's maps vary arrangement
  (intro, groove, lead, break, outro, drops, a section without the comp) over one progression. A B section is a
  contrasting part with its own progression (and maybe its own voicing color, comp rhythm or drum pattern variation),
  so a song can run A-A-B-A or A-B-A-B. The B section can also switch the comp (its instrument, rhythm pattern or
  approach: struck, broken chord, held), the lead (its instrument and pattern) and the drums (pattern, maybe kit or
  hats to ride), and move the lead or comp an octave, then all back for the return of A (Sam, 2026-11-08; ties to "The song
  planner changes an instrument mid-song" above). A big piece of work. Related ideas already noted: the lament descent as a B section (Eight Bells
  notes above), the two-lead A-B-A item, hats to ride for one section. Open questions: how often a song gets one,
  how the B progression relates to A (same key, relative or a neighbor key), and whether the lead's motif carries
  over or answers it, and how B pairs with the break sections (a break before or after B, B in
  place of a break, or a break inside B).
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
- **Piano broken-chord climb (Sam, 2026-10-24: "soon, just not quite yet"; 2026-11-05: still wanted, keep it).** The
  arpeggio comping on piano-like keys (Rhodes, felt, the Knight; the Wurli and DX are retired; maybe the vibes) as a rising broken chord over 2-3 octaves: root an octave below the voicing, 5th,
  10th, then the voicing and the voicing an octave up; chord tones only down low. It sometimes lands on the voicing as a
  block chord at the top, held to the bar's end: always in a 4-bar phrase's last bar, about 1 in 3 other bars,
  deterministic per loop position. Pedal-like ringing; velocity about 50 to 70 up the climb, 75 on the top chord. Under
  the lead in lead sections (tops at the voicing's top), an octave higher elsewhere; a busy lead thins it to 8ths with
  no top chord. Open question for Sam: 16ths (two climbs a bar), 8ths (one climb landing near beat 4), or both per song.
  Sam's shapes (2026-10-27): "it plays the chord for three octaves. example: 1 5, (next octave) 1 5, (3rd octave)
  chord. another example: 1 5, (next octave) 1 3, (2nd/3rd octave) 5/1/3 chord. or first example over just two
  octaves". For its timing and touch, reuse LOF-1's `comp_strike` (`ref/fm1-HYP1/lof1/firmware/src/dsp/amb.c` 1286:
  4-49 ms spread, 0-4 ms jitter, top note +8%, the rest -5%); Tuna's `pad_voice` (`ref/tuna/firmware/src/tuna_gen.c`
  1067, hold common tones, restrike only new ones) for the EPs. Still open: the rhythm, and the low root vs the
  rootless rule and the bass. Parked again 2026-10-27 ("save the piano climb for later").
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

- **Motifs start on beat 1 more often (Sam, 2026-11-10: "I'm good with your proposal"; about a third on beat 1 in
  the off-beat styles, syncopated starts still the majority).** Phrase heads on beat 1, over 2,000 songs a style
  (lead loop on): Jazz-Hop 20 to 36% (hook start weights beat 1 / and-of-1 / 16th before 2 / and-of-2 / 16th after
  1 from 15/35/15/25/10 to 30/30/12/18/10; the loop's riff answer chops the hook's first note half the time),
  Ambient 40 to 52% (start weights 20/25/35/20 to 30/25/30/15), Bossa 15 to 35% (the 2-note cell keeps its beat-1
  note; the beat-1 cell's weight 20 to 40), '60s Jazz 0 to 25% (two rhythms whose run-in begins on 1, 37% of songs;
  a beat-1 run-in note off the chord moves to a chord tone 4 semitones or less away instead of dropping), Modal 0
  to 29% (a beat-1 start at weight 30), Impressionism 5 to 29% (40% of songs build the arabesque forward from a
  chord tone on 1, the landing right behind the run). Romanticism (98%) and Film Score (57%) unchanged.
- **A restatement flips the motif's start (Sam, 2026-11-10: "the motif doesn't seem to use the opposite note on beat
  or off beat as a way to vary the second or third bar. If the motif did not start on the downbeat, the second bar
  could. Or vice versa.").** A head off beat 1 restates from beat 1, one on beat 1 restates off it (an 8th later, or
  the style's own syncopated spot); same pitches, contour and rhythm, only the start moved (leads_flip, about a third
  of restatements; '60s Jazz folds it into its displacement draw, Modal into its one-change draw). Restatements whose
  start is opposite their head's, before to after (lead loop on): Impressionism 10 to 34%, '60s Jazz 2 to 37%, Modal
  9 to 35%, Jazz-Hop 10 to 26% (its riffs and turnarounds count, unflipped), Bossa 9 to 33%, Ambient 5 to 32%.
  Romanticism (its cell on 1 every bar) and Film Score left as they are.

- **The Impressionism lead (Sam, 2026-10-29: this impressionist lead "sounds fantastic").** A new lead style: 4-bar
  phrases (arabesque, float, varied arabesque, falling sigh), 3-5-note pentatonic/modal arabesques in 16ths or 8th
  triplets, landings behind the beat on color tones (9th/13th over major, 11th over minor) often tied over the bar,
  rests after each phrase, whole-tone only as the one legal 4-note fragment over dominants. Research:
  docs/research/lead-impressionism-2026-10-29.md; spike (lead_style 2 in lead.c): worktree branch
  `worktree-agent-aaf267bc7c3b70bee` (93c14e3, f5bca57); renders: app/build/new/lead-impressionism-2026-10-29/.
  **Next: bring it into the engine properly** (tests, the workbench's lead-style row, which songs roll it). Open
  from the spike: phrase heads may sound loopy (one variation per restatement); beat 1 rarely struck (vary, not
  avoid); the felt piano plays it soft and its ties decay (suits flute and vibes best); triplets don't take the
  swing; the lead's floor over percussive keys (~C5) blocks the flute's low register; unplaceable notes are dropped.
- **Lead-style spikes, Sam's verdicts** (same five songs; queue in .superpowers/sdd/lead-styles-2026-10-29/queue.md;
  research in docs/research/lead-<genre>-2026-10-29.md):
  - **Neoclassical** (lead_style 3, branch `worktree-agent-a36c80b9ab11ec46a`): songs 1 (G Aeolian, flute) and 5 (A
    Ionian, flute) the best. Song 5's 4th bar of each 4 too busy; song 1 handled it much better. Songs 2-4 "just had issues"
    (Db Lydian flute, B Dorian vibes, Ab Mixolydian felt). Spike notes: leaps where few notes are legal (songs 3, 5),
    density ~3.5 notes/bar, strong-beat leaning notes.
  - **Romanticism** (branch `worktree-agent-aeec8cac90861a25b`, 4100d01): "sounds much better than" the Neoclassical
    lead. "It doesn't have any major flaws."
  - **All styles so far (Sam, 2026-10-29): too much variance after the initial line** — "It sounds like it's
    wandering... not connected enough to the original motif." Every lead style should restate the opening motif more
    literally and vary it less (one small change at a time, the motif recognizable each time), so the line develops
    the motif instead of drifting away from it. Applies to Impressionism, Romanticism, Neoclassical and '60s Jazz, and
    to the spikes still running.
  - **'60s Jazz** (branch `worktree-agent-a4802cdbc40dd46b4`, 3b387cd): strong. **Sam's shortlist so far:
    Impressionism, Romanticism and '60s Jazz "are really strong."** Don't rework their motifs (Sam); the
    stay-on-the-motif rule applies to the spikes still to come.
  - **Names (Sam):** each kept style takes a genre name, and every trace of the source person is then scrubbed from
    the repo (lead-style work only): Impressionism, Romanticism, '60s Jazz, Cool Jazz, Modal Jazz, Post-Bop, Bossa Nova,
    Jazz-Hop, Film Score, Ambient — for whichever of these are kept. (The not-kept long-period style goes by
    Neoclassical in the scrub.) Done 2026-10-31: the research docs are renamed and rewritten by genre
    (docs/research/lead-<genre>-2026-10-29.md); the not-kept styles' research was removed (Post-Bop's v3 rules stay
    here); tools/scrub/check-names.sh checks the tree and history against a name list kept outside the repo.
  - **Cool Jazz** (branch `worktree-agent-a62c91983b0fa60c5`, f359ad9): "fair. Not the best."
  - **Modal Jazz** (branch `worktree-agent-a8cba358d5d753da1`, 12843da): "sounds really good." Shortlist now:
    Impressionism, Romanticism, '60s Jazz, Modal Jazz. Note: its song 5 stayed within a 3rd because the spike centers a
    narrow "middle register" just above the floor (a 4th over the keys' top), not because of the pad; widen the window
    when it's built for real.
  - **Jazz-Hop** (branch `worktree-agent-acdb178f7a5283930`, 15fbca9): "sounds really nice in this lofi genre. Loops
    are actually good. And that's behind my request to have less variance." Shortlisted.
  - **Post-Bop** (branch `worktree-agent-a31d3b76047665641`, 310524a): the 4ths are "really interesting, but it can't
    always be just fourths." Idea (Sam): the first bar of each pair in 4ths, the second resolving better (by step, onto
    a chord tone). Reworked as a second pass.
    **Post-Bop v3, for the build (Sam, 2026-10-29):** "after the opening fourth-based notes of a phrase, make the rest
    of it not fourth-based" — and the 4ths cell needn't open the phrase: it can sit in any bar of the phrase (1st,
    2nd, 3rd…) but **never the last**. The rest of the phrase draws from post-bop's other moves, one picked per song
    (a loop): a modal scale run by step (often contrary to the 4ths), an upper-structure triad arpeggio (a triad on the
    chord's 5th or 9th; the #11 one in Lydian only), a "side" pentatonic run (e.g. E minor pentatonic over Dm7: 9ths,
    11ths, 13ths), a diatonic sequence of a 2-3 note cell; then a long chord or color tone and space. At most one leap
    of a 4th or more outside the 4ths cell. One thing varies per restatement.
  - **Film Score, Bossa Nova, Ambient** (branches `worktree-agent-a58dfa512ae37f56c` 266e2af,
    `worktree-agent-a8d4733e8156e2a1b` 071ffee, `worktree-agent-aedeac7d79de88df6` 867b1db): all "sound good."
    Shortlist: Impressionism, Romanticism, '60s Jazz, Modal Jazz, Jazz-Hop, Film Score, Bossa Nova, Ambient.
    Not kept: Neoclassical. Cool Jazz "fair". Post-Bop v2 not yet heard.
  - **Sam questions the lead's floor over percussive keys** ("why does it matter what the nylon keys are doing?"):
    lead.c sets the floor a 4th over the keys' top note (`c.lo = top + 5`), so high voicings squeeze the lead and
    force leaps. **Decided (Sam, 2026-10-29): duck the keys instead** — when the lead needs room, the keys voice lower
    (the voice-leading rules still pick the smoothest path, in a lower window); the fixed floor goes. Part of the lead
    styles' build.
    **Follow-up (Sam, 2026-10-29: add to the ledger):** the lead styles build's fixed band (floor = style center − 6)
    raised the floor where the keys sat low, costing Romanticism and '60s Jazz their low approach notes ('60s Jazz
    plays ~9% fewer notes). Fix: the floor = the LOWER of the band's floor and the old floor (a 4th over the keys'
    top), so the lead dips where the keys sit low and the keys still duck where they'd crowd it; then restore the
    strict test gates and re-render the built styles.
    **Jazz-Hop song 1 (A/B, G Aeolian):** the built style plays its hook ~5 semitones lower than the spike (home note
    ties between G4 and G5 at the band; the tie goes low) and loses its turnaround lift. **Decided (Sam, 2026-10-29):
    break the tie toward the higher note** (to do; then drop the per-song test allowance that pins it).
    **Bossa Nova (built 2026-10-30):** the band squeezes it hardest: its span is 0.67x the spike's on the flute
    (0.77x felt, 0.82x vibes), because its home note sits mid-range from the floor up and rises with it. A/B song 1
    plays 82-86 (spike 72-86) and loses its low opening; song 5's question goes to E6. The band-floor fix above should
    restore it. Its color tone on beat 3 (11ths and 13ths) is kept by a narrow test exemption (beat 3 only, these two
    styles only; avoid notes and rubs still checked). Spike quirk: the "first statement is the motif itself" rule
    never fires.
    **Ambient (built 2026-10-30):** note for note the spike in the A/B songs. Spike quirks, to revisit: its added
    passing note can land on beat 3 unchecked (exempted as above); its level is fixed from the first lead bar's
    range, so a later octave or set change keeps the old level; on m9 chords over a walking bass it plays 7% fewer
    notes.
  - **Decided (Sam, 2026-10-29): remove the two old lead engines** — the phrase planner and the hybrid arpeggio
    ("these new ones are light years ahead"). The shortlisted styles replace them. Goes with them: the lead-arp
    pattern family (rhythms, chord-tone orders, landings) and its workbench rows, and whatever of the melody path only
    they use. Part of the lead styles' build.
  - **Decided (Sam, 2026-10-29): remove the kalimba, Emilyguitar, saw lead and synth pluck as leads** ("none of these
    sound good"). Leaves flute, felt piano, sampled vibes, Knight. 10 sets lose their lead (5 kalimba; 10, 17, 20
    pluck; 13, 18, 19, 33 saw; 28, 29 Emilyguitar), and the synth family has no lead of its own left. Emilyguitar as
    keys stays. **Wanted (Sam): a warm chorus guitar, a real legato string section, a better Hammond B3 organ** —
    "might need to be samples" (flash: ~7.4 KB left).
    **Delayed (Sam, 2026-10-29): the B3 and the legato strings spikes.** The strings spike was stopped mid-way (its
    worktree `agent-a0cfe80a339f7f02b`, branch `worktree-agent-a0cfe80a339f7f02b`, holds partial work, including the
    flash-trim survey if it got that far); the B3 part of the guitar spike was dropped. The warm chorus guitar
    (Emilyguitar + chorus + warmth) continues.
    **Chorus guitar from the Emilyguitar: rejected (Sam, 2026-10-29):** "too thin. It doesn't sound like a chorus jazz
    guitar. It sounds like sitar." (spike branch `worktree-agent-a18fa86763a20027d` 194da1c; renders in
    app/build/new/instruments-guitar-organ-2026-10-29/). A real jazz/clean electric sample is the remaining path
    (Karoryfer Shinyguitar archtop, CC0, from docs/research/real-leads-2026-10-29.md), but flash is already over
    budget with the new lead styles merged (413776 of 409600 B before any guitar).
    **All guitar lead candidates rejected (Sam, 2026-10-29: "these do not work"):** Shinyguitar archtop (pickup and
    mic, written and an octave down, with and without a 30 ms attack trim) and the FluidR3 jazz guitar
    (app/build/new/instruments-shinyguitar-2026-10-29/, guitar-candidates-2026-10-29/; branches
    `worktree-agent-adb211453a61fddd9` 929c2a5, `worktree-agent-afe3be65f311152c4` 2280a67). The guitar lead is
    parked. Untried: Sam recording a guitar himself (he offered to record from his own instruments), the SampleScience
    clean guitar (public-domain samples), the Polyphone jazz guitar SF2 (needs a sign-in).

- **Done (Sam, 2026-10-29, after the lead-candidates spike, app/build/new/lead-candidates/):** "i only liked the felt
  piano and vibraphones." The vibes are the VCSL vibraphone (soft mallets, v2, 5 zones C4-E6, 18.4 KB of flash), on the
  phrase planner as before (its level matched: its sets' lead over keys 8.5-9.8 dB, as the other leads'); the felt
  piano is a lead (the struck hybrid, as the spike played it; +3.2 dB on the lead over its keys level), never over
  felt keys. The muted trumpet, sax, guitar lick, string lead and cello are out of
  every set (their patches and the string synth gone; ids kept, the workbench lists them retired and plays an approved
  lead of their family for an old snapshot naming one). Sets: 3 Neo-soul Wurli, 11 Rhodes neo-soul, 22 Knight and felt,
  31 Rhodes and felt: the felt; 4 Felt and vibes, 6 City pop Rhodes, 10 Pad and vibes, 30 Knight and vibes,
  33 DX and vibes: the vibes; 9 Nylon and flute, 32 Knight, flute and brushes: the flute (set 9 needs a sustained
  lead to keep its strum). The flute came down 0.3 dB (its pad solo trim up as much): over set 9's strummed nylon it
  read 10.2 dB over the keys, past the 8-10 the mix holds; Emilyguitar and flute now reads 8.0, at the floor. The pads of
  sets 30-33 stay the warm pad (chosen so the string leads weren't doubled; the strings could come back). To hear:
  those sets.
- **Lead candidates that "might work eventually, but would need lots of tweaks" (Sam, 2026-10-29).** From the spike
  (app/build/new/lead-candidates/INDEX.md; docs/research/real-leads-2026-10-29.md):
  - The comp instruments as leads: **Rhodes, Wurli, DX, nylon, organ** (the felt is in). Caveats: level (organ 3-5 dB
    and DX 2-3 dB under the old leads in the mix, nylon 2-3 dB over); style (each played the struck hybrid, the organ
    phrases: in set22 and set31, where the old lead sang phrases, the struck ones played a different line); a comp
    instrument never leads over itself (no Rhodes lead over Rhodes keys).
  - The sampled real instruments: **trumpet harmon (VSCO-2-CE), Weresax alto, Shinyguitar archtop, violin section
    (VSCO-2-CE susVib), both cellos (Karoryfer bigcat, FluidR3)**. Caveats: the spike rendered them outside the engine
    (a generic reverb and filters in place of the lead bus's plate, delay, section filters, wow, saturation, dust and
    the mono lead's glides), so they sounded cleaner and closer than an engine lead would; struck vs phrase line (each
    heard on a struck line in set6, a phrase line in set22 and set31); one round robin, no release or key noise; the
    sustained ones need loops cut (vibrato baked in the violin); register: set22's line runs C5-C6, above the alto
    sax's top sample (G5, +5 semitones), the trumpet's (A5, +3) and very high for a cello; and the flash (about 7 KB of
    room is left after the vibraphone).

- **New synthesized leads to try (Sam, 2026-11-05: on the short list, not started).** Today's leads are flute, felt
  piano, vibes and acoustic piano; the guitar lead is parked and the organ lead waits on its UI controls. Each is
  judged in songs, level-matched to the leads we have:
  - **Harp:** a plucked string with a long, soft ring and no pick noise; a retone of the existing plucked-string
    model (the retired guitar lick), so almost no flash. First to try.
  - **Whistle:** a sine with breath noise and slow vibrato; sustained, on the phrase planner like the flute.
  - **Soft synth lead:** a rounded triangle or square with vibrato and glide; the synth family has no lead now (the
    saw lead and synth pluck were cut, 2026-10-29).
  - **Candidates, round 1 (2026-11-11; workbench candidates, never rolled, no set):** the sampled cello
    (`INS_SAMP_CELLO`: the Karoryfer x Bigcat cello's sustained bowing, CC0, five zones Gb3-Eb5 in the bank, ADPCM),
    the synth cello (`INS_SYNTH_CELLO`: the string synth's "low and dark" cello voicing re-expressed on the wind
    model with a saw; the string synth itself stays gone) and the harp (`INS_HARP`: the vibraphone's modal model
    retoned, three harmonic modes, a soft finger, a long ring; the plucked-string model went 2026-10-31, and its
    Karplus-Strong lines would need RAM the pool no longer has). The cellos are low leads (an octave under the
    flute's band, ceiling E5) but the keys don't lift over them as over the jazz guitar: on the A/B song (seed 114)
    the lift would have put the keys on the cello's own notes, so the cello goes a 4th over the keys as any lead.
    Levels within 1 dB of the flute (cellos) and between the vibes and the piano (harp). Renders: seed 114 (G minor,
    86 BPM, Rhodes, electric bass, strings, Soft brushes; Film Score forced), lead 0:22-0:43, 1:29-2:14, 2:58-3:10.
    To judge by ear; the whistle and the soft synth lead wait for round 2.
    **Round 1b (Sam, 2026-11-11: "the harp definitely sounds like an electric piano. the two cellos are an octave too
    high"):** the cellos two octaves under the flute's band (lib_lead_drop 24; the jazz guitar 12), ceiling A4, and
    the keys and pad lift over them as over the jazz guitar (their top under the keys' lowest note); the lead bus's
    mud high-pass at 70 Hz under a cello (135 elsewhere). Measured from the rendered audio on seed 114 (set 1, Film
    Score): both cellos sound F3-D4, the body F3-C4 (a cello's melody: G2-E4, its top to A4), under the lifted keys
    (Eb4-D5). The sampled cello's zones moved to A2 Eb3 A3 Eb4 A4 (the files an octave under their names; ADPCM floor
    -50 to -66 dB between the harmonics), its tone opened (14x the note); the synth cello's low-pass 8x the note (3.5x
    muffled it down there). The harp is sampled now: VSCO-2 CE's concert harp (CC0, its license the CC0 text without
    the legal code's header: its own hash in the manifest), seven zones F4-D6, 16 kHz ADPCM (floor -55 to -68 dB, its
    source -77 to -99: far from the jazz guitar's audible -35), the sampler's envelope the ring; it sounds F5-D6 on
    the song (body F5-C6), the flute's register. Renders: scratchpad round1b/.
    **Round 1c (Sam: the harp "sounds nice. The delay/reverb is a bit too long on it, though. The echo doesn't time
    well"; the cellos "sound nice too. Can the synth cello be less mellow?"):** under the harp only, the tape delay a
    quarter note (was a dotted 8th, which crosses the beat and the swing) fed back at 0.2 (0.35: about 3 echoes over
    -40 dB, was 5), its plate send 0.15 (0.35: -7.4 dB), its release 0.6 s (0.9). The synth cello moved to the
    sampled one's spectrum on seed 114: a low-pass 18x the note (8x), a body band-pass at 1 kHz, bow noise at 2.5 kHz,
    an 80 ms swell (120): its centroid 326 to 552 Hz (the sample's 453), 1-2 kHz -17 to -10 dB of the whole (the
    sample's -10), 2-4 kHz -29 to -21 (-20); level re-matched (within 0.1 dB of the flute). Sounding octaves
    unchanged (cellos F3-D4, harp F5-D6). Renders: scratchpad round1c/.
    **Round 1d (Sam, 2026-11-11):** the harp with no echo (its delay send 0; the harp-only quarter-note delay of
    round 1c removed: every song's delay one feel again), its plate send kept small (0.15), its lead level +0.8 dB
    for the echo's energy (4.2 dB under the flute over six songs). The sampled cello kept. Renders: scratchpad
    round1d/.
  - **Candidates, round 2 (2026-11-11; workbench candidates, never rolled, no set): the whistle and the soft synth
    lead,** both the flute's wind model retoned (no new DSP; the synth cello's saw stays removed). The whistle
    (`INS_WHISTLE`): a pure sine, a breath of air (a wide band at 2.6 kHz), a slow, wide vibrato (4 Hz, 20 cents: the
    model's vibrato rate now per patch), held as the flute; in the flute's band, which on the A/B song is a
    whistler's lower register (measured F5-D6, body F5-C6): an octave down (F4-D5) would sit under a real whistle's
    floor (about A4-C5) and read as a sine, an octave up piercing. The soft synth lead (`INS_SOFT_SYNTH`, the synth
    family's only lead): the triangle a third of the way to a square under a low-pass 4x the note, the delayed
    vibrato (12 cents), a 60 ms glide between joined notes (the solo lane's glide, on the lead); a low lead a 5th
    under the flute's band (lib_lead_drop 7: an octave down put its floor a 4th over the keys and cut the motif's
    shape), the keys as under the flute (not lifted): measured G4-D5 (body G4-C5), in a synth lead's C4-C6. Both
    take the flute's styles. Levels matched to the flute over six songs (within 0.1 dB on average; the synth's
    spread about 1 dB). Renders: seed 114 (set 1, Film Score: G minor, 86 BPM), lead 0:22-0:43, 1:29-2:14,
    2:58-3:09; scratchpad round2/.
  - **Parked (Sam, 2026-11-11: "in our back pocket"): the synth cello.** The string synth's "low and dark" cello
    voicing re-expressed on the wind model: a saw added to M_WIND's blend (p[0] 3), a low-pass 18x the note, the
    mute's band-pass as a 1 kHz body resonance, bow noise at 2.5 kHz, an 80 ms swell, the keys' chorus as its
    ensemble; a low lead two octaves under the flute's band like the sampled cello. Removed with its id and the saw
    option (nothing else used them); restore from 2f51a53 (`INS_SYNTH_CELLO`, patch.c, voices.c's wind_sample and
    wind_start's blend clamp, options.json). Sam: "better" after round 1c's brightening; the real one "still has
    better wood resonance". Idea for when it comes back: 2-3 body resonances around 200-600 Hz.
- **Done (Sam, 2026-11-11): the whistle approved for songs** (the soft synth lead stays a candidate, manual only).
  Three sets, one per flute-like flavor, each from a flute set's keys, bass, pad and kits: 46 Rhodes and whistle
  (lofi, wistful/warm, set 1's), 47 Nylon and whistle (bossa, warm/sunny, set 9's), 48 Pad and whistle (chillwave,
  wistful/warm, set 10's). Its styles: the flute's but '60s Jazz and Modal Jazz (a whistled line is a tune, not a
  run). Shares over 20,000 rolls: whistle 7.5%; the others flute 17.8% (18.9), vibes 19.2% (20.8), Knight 17.4%
  (19.1), felt 16.6% (17.8), jazz guitar 8.0% (9.1), harp 8.0% (8.8), cello 5.4% (5.5). Measured from the audio on
  rolled songs: seed 25 (set 46, G Mixolydian, 84 BPM, Jazz-Hop) body C5-Ab5, median D5, top A5; seed 32 (set 47, Db
  Mixolydian, 87 BPM, Bossa Nova) Bb5-Eb6, median C6; seed 1 (set 48, Ab Dorian, 79 BPM, Ambient, over the pad)
  Db5-Ab5 by its notes. Pinned hashes moved (the seeds draw other sets): strum_same's and its six echoes
  47d257cc/7e94dc42 to 796fd966/8d04939b; the lead loop's 617710c3/6f39f64e to a19e0ce1/7bfb417c (with the cello's
  move); song_rolled at seeds 72 and 140. Eased: clash's Film Score floor 40 to 37 notes a song (the Knight an octave
  down: 38.8), the pushes' count 140 to 80 (92), the synth sets' share 2.4% to 2.1% (Organ and felt 2.3%), the pad
  chord's smooth top voice 80% to 79% (79.6), task 1b's pad_on 40 of 40 to 39 (seed 23 now rolls set 5, the
  chillwave pad's sustained keys, whose solo note no section draws: pad_on can't force it; to revisit). The stab
  test counts a bar's kick as played (seed chain 5353's set-29 song: a kick phrase's 4th hit made the bar busy, where
  the rule allows the stab on it). The sampled-sets test lets the whistle's sets in. Smoke's lead-down and lead-only
  seed 2 to 7 and 14 (seed 2 now the vibes over the Emilyguitar); parity's rules step on seed 72 (now the nylon and
  the whistle) sets the lead to the piano first. Renders: scratchpad cello-fix2/.
- **Done (Sam, 2026-11-11): the harp (round 1d) and the sampled cello approved for songs.** Five sets: the harp
  over gentle keys, 41 Felt and harp (lofi, sad/wistful), 42 Pad and harp (chillwave, wistful/warm), 43 Rhodes and
  harp (city pop, warm/sunny); the cello over struck keys the lift raises, the warm pad under it, 44 Knight and cello
  and 45 Rhodes and cello (lofi, sad/wistful). Their styles: Impressionism, Romanticism, Film Score, Ambient (about a
  quarter each). Shares over 20,000 rolls: harp 8.8%, cello 5.5%; the others flute 18.9% (22.6), vibes 20.8% (24.1),
  Knight 19.1% (21.7), felt 17.8% (21.3), jazz guitar 9.1% (10.4). The cello's register rules hold over 80 songs on
  its sets (test_jazz_lift, now over all five lift sets: its top never over the keys' lowest note, the keys in their
  own register outside its blocks); measured from the audio, it sounds Db4-Ab4 at most on rolled songs (bodies
  A3-C4, B3-Eb4, Db4-F4), the harp Eb4-C6 (Eb5-Bb5 over struck keys, Eb4-C5 over the pad). The cello's lead level
  3.2 dB down: at the flute's it read 12-13 dB over its sets' keys (the mix's 1.5-10.5): now 8.4-9.5. Pinned
  hashes moved (seeds draw other sets): strum_same's 89d868ef/ee29ff88 to 47d257cc/7e94dc42, the lead loop's
  6ec226d2/572d1848 to 3df1a20a/616b00b8, song_rolled at seeds 21 and 140. Thresholds: the synth sets' share 2.8%
  to 2.4%, the pad chord's common pitches 4000 to 3700, ov oct's seed 3 (now the felt over the Rhodes) held to half,
  the lead loop octave's dropped notes 1 in 250 to 1 in 200; smoke's lead-down seed 5 to 2.
  - **Fixed (Sam, 2026-11-11: "the cello is an octave too high!", seed 105, set 44, Bb minor, 76 BPM):** its band
    was two octaves under the flute's style center, which floats with the style (Ambient's and its ceiling A4 put
    seed 105's line at Db4-Ab4). Now an absolute register: the cello's center G3 whatever the style (leads.c's
    CELLO_C), its ceiling E4 (A4); the lift (keys and pad +12 in its blocks, its top under the keys' lowest) as
    before. Measured from the audio (10-90% / median / top) on 13 songs of sets 44-45 across its four styles:
    seed 105 Db3-F3 / Eb3 / Ab3 (was Db4-F4, top Ab4); 101 F3-C4 / F3 / Eb4; 102 Gb3-D4 / A3 / D4; 103 E3-Db4 / A3
    / Db4; 106 G3-A3 / G3 / C4; 107 F3-D4 / Bb3 / D4; 108 F3-C4 / G3 / D4; 111 (Film Score) G3-D4 / A3 / D4; 113
    Ab3-Db4 / Bb3 / Eb4; 114 F3-C4 / G3 / C4; 115 A3-Db4 / A3 / E4; 119 G3-C4 / C4 / Db4. Seed 114 on set 1 (Sam's
    OK) unchanged, F3-C4. In seed 105's lead blocks the keys lift to Eb4-Eb5 over the cello's Db3-Ab3, back at
    Eb3-Eb4 outside; test_jazz_lift holds. Its zones (A2-A4) cover the line, the 70 Hz lead high-pass sits under its
    lowest note (F#2, 92 Hz). Lower, it read about 1 dB louder over its keys: its lead level 0.95 dB down (SAMP_LEAD
    +4.65 dB), test_lead_over_keys (loop) +9.4 Knight and cello, +8.4 Rhodes and cello (were 9.5, 8.4). Every other
    lead renders bit for bit as before. The lead loop's hashes 3df1a20a/616b00b8 to 617710c3/6f39f64e. Renders:
    scratchpad cello-fix/.
  - **Moved up (Sam, 2026-11-11: a cello's melody sits about D3-A4, peaking at D5; under G3 it reads as bass, and G3's
    center put seed 105 at Db3-F3):** the center Bb3 (CELLO_C 58), the ceiling A4, a hard floor at D3 (CELLO_LO: the
    range's floor, and any note a style still puts under it an octave up). The lift as before (test_jazz_lift: 0 bars
    with the keys' lowest under the cello's top). Measured from the audio (10-90% / median / top): seed 105 Db4-F4 /
    Eb4 / Ab4; 101 F3-C4 / F3 / F4; 102 Gb3-D4 / A3 / Ab4; 103 G3-E4 / B3 / E4; 106 A3-B3 / A3 / D4; 107 F3-F4 / Eb4
    / G4; 108 F3-A4 / G4 / A4 (Impressionism's wide arpeggios, A3-A4 sweeps); 111 G3-D4 / B3 / D4; 113 Ab3-Db4 / Bb3
    / Eb4; 114 F3-C4 / G3 / C4; 115 A3-E4 / Db4 / E4; 119 G3-C4 / C4 / Db4; seed 114 on set 1 G3-C4 / Bb3 / F4. No
    note under D3 (the lowest F3). Medians F3-G4: seed 105 lands where Sam first heard it (Db4-Ab4), the ceiling A4
    giving its Ambient line room; a Gb4 ceiling measured medians F3-C4 (105 Ab3-Eb4), rendered as the alternative.
    Its level over its keys (loop) +9.2 Knight and cello, +8.1 Rhodes and cello. Renders: scratchpad cello-fix2/.
  - **Done (Sam, 2026-11-11, on the Gb4-ceiling render: "the cello has the correct register now"):** the ceiling Gb4
    (the Bb3 center and the D3 floor stay). Measured from the audio (10-90% / median / top): seed 105 Ab3-Eb4 / Ab3 /
    Eb4; 101 F3-C4 / F3 / F4; 102 Gb3-D4 / A3 / Gb4; 103 G3-E4 / B3 / E4; 106 A3-B3 / A3 / D4; 107 F3-Eb4 / C4 / F4;
    108 F3-C4 / G3 / F4; 111 G3-D4 / B3 / D4; 113 Ab3-Db4 / Bb3 / Eb4; 114 F3-C4 / G3 / C4; 115 A3-Db4 / B3 / Db4;
    119 G3-C4 / C4 / Db4; seed 114 on set 1 G3-C4 / Bb3 / F4. Medians F3-C4, tops Db4-Gb4, nothing under F3. Seed
    105 renders bit for bit as the alternative Sam heard. The lift clears (test_jazz_lift: 0 bars); over its keys
    (loop) +9.2 Knight and cello, +8.1 Rhodes and cello. The lead loop's hashes a19e0ce1/7bfb417c to
    bcb028dd/02b479c0.
- **Done (Sam, 2026-11-11): the jazz guitar approved for songs.** Three sets lead with it, one per flavor its styles
  weigh most, over struck keys the lift raises: 38 Rhodes and jazz guitar (neo-soul, wistful/warm), 39 Knight and
  jazz guitar (lofi, sad/wistful), 40 Bossa Rhodes and jazz guitar (bossa, warm/sunny). About 10% of songs (20,000
  rolls: 3.1/3.4/3.8% by set); its styles there: Jazz-Hop 34%, Bossa Nova 23%, '60s Jazz 22%, Modal Jazz 22%. The
  other leads: flute 22.6%, vibes 24.1%, Knight 21.7%, felt 21.3% (each about a quarter before). Its register rules
  hold (low lead; the keys and the pad's chord part +12 in its lead blocks where the keys' register is as rolled).
  Rolled songs showed what seed 127 hadn't: the guitar's band (to E5) ran over the lifted keys in 71% of lead bars,
  so its top is now capped at the keys' lowest note (Sam: "the keys' lowest note never under the guitar's top"),
  and a low lead sets no keys window (it squeezed set 40's Rhodes into clusters, F#3 G3 A3 B3). Seed 127 keeps its
  ranges (guitar C4-G4, keys G4-F5 in the lead blocks), a few notes moved. Pinned hashes moved (seeds draw other
  sets): strum_same's (1be763b7/b465f94c to 89d868ef/ee29ff88), the lead loop's (feba4ec4/23854a09 to
  6ec226d2/572d1848), song_rolled at seeds 140 and 300. Thresholds eased by the new sets' share: synth set Pad and
  flute 3% to 2.8%, the pad solo's 3000 notes to 2900, glue seeds 26 and 43 (now Vibes and felt, 0.88 dB) held to
  0.85; the voicing, pad-chord and keys-window tests leave the jazz guitar's songs to test_jazz_lift (the octave
  lift is by design). To hear: songs on sets 38-40.
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

- **To do (Sam, 2026-10-30): dominant chords sound cheesy; fix the voicing, likely with a rule or two.** Today a
  dominant is colored with its 9th or 13th (never bare), voiced rootless where it helps, four voices E3-A4, the top
  voice led smoothly. First find which shapes Sam hears as cheesy (renders of dominants in songs, voicing by
  voicing). Candidate rules to test by ear: keep the 3rd off the top voice; keep the 3rd-7th tritone off the
  bottom pair; prefer the 13th over the plain 9th where the mode allows; no 9th and 13th stacked a whole step
  apart at the top; a dominant's voicing as a fourths or upper-structure shape. A/B in songs, one rule at a time.
  **Spiked 2026-10-30; pinned (Sam: "I will come back to them").** Census of 2,790 dominants (300 songs): 86% have an
  adjacent whole step, 42% a whole step on top (mostly the 9th under the 3rd), 36% the 3rd on top, 89% the plain 9th,
  40% the root voiced; commonest shape 5-b7-9-3. Rules (`VNL_DOMRULE`, `worktree-agent-a5a9423a57e338cc0` 3e176c8,
  renders `app/build/new/dominants-2026-10-30/`): (1) no 3rd on top: moves it to a bare tritone at the bottom, fails
  the voice-leading checks; (2) no 3rd-7th tritone at the bottom: changes little (7%), likely a cut; (3) the 13th
  for the plain 9th where the mode allows: the only rule better in all three songs, voice leading as today (watch for
  a 6th-chord "lounge" top, b7-3-5-13); (4) no whole step on top: helps, top voice kept 94.1% (just under the 95%
  gate). Next if rule 3 is close: a pianist rule, 3-7 plus 9/13 only, the 13th preferred.
  **Done (2026-11-04, a19d443):** Sam: the 7th is what sounds cheesy. The keys and the pad sound a dominant without
  its 7th (its major triad with the 9th or 13th: add9, 6/9); the lead and bass still read the full chord. A/B renders
  `app/build/new/dominants-no7-2026-11-04/` (Sam: "yes"). To hear: the pad's chord part moves less smoothly (voices
  moving a step or less 84.9% to 78.1%, the top voice 83.9% to 80.8%: the 7th-to-3rd line is gone).
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
  **Done (sampled instruments, d015b4f):** the strum engine (strum.c): six-string shapes, D-DU-UDU, chuck, bossa,
  ballad, quick or slow strokes, the ballad's fill bar. **2026-11-04:** Sam approved four new patterns (offbeat skank,
  syncopated push, anticipation, charleston), removed straight eighths and swing quarters, kept boom-chuck and five
  boom-chuck variations as candidates (fe4f344); the keys never copy the bass's rhythm (a soft up added where they
  would). Muted chucks: today's kept (a scratch demo was heard only soloed). **Done (2026-11-04, ed406bd):** the
  guitars only strum (Sam: "drop the comping capabilities for guitars"; held, stabs and half notes gone; they strum
  under any lead, 2 dB up), and a fill bar's figure varies song to song: step back (one stroke) or drive
  (D.u..du..dU..du., syncopated at Sam's ask); bossa B............... or B.P.T.P.T.P.T.P. A workbench "Strum fill" row
  auditions either. Sam declined a figure for drum-drop bars. Not wanted yet: ghost strokes, a new figure per section.
- **Comping mix drifted (found in the rule-docs refresh, 2026-10-30; to revisit).** Turning the arpeggio off moved
  the electric and synth keys' draw to held 55% / half notes 45% (half was 36%). The last mix Sam stated was
  55/25/20, and "Approved by ear" still lists stabs and offbeats. Decide the mix by ear.
- **New comp styles (Sam, 2026-10-30; workbench-only, weight 0):** "held on 2" sounds good; the optional beat-4
  re-strike split off as its own style, "held on 2 and 4" (always on 4). "Light eighths": less sure; could work in
  some situations, so it needs rules for when it fits before it's ever rolled. None of the three is thinned by a busy
  lead.
- **Comp rhythm candidates (Sam, 2026-11-04: "like we did for strumming, give me some more comping pattern
  candidates"):** **Done (2026-11-05, 98b0076..346e4b1):** Charleston, tresillo and late one added and approved by
  Sam (dotted and two-feel push duplicated them; push needs a chord held over the bar line: an engine task if
  wanted). Sam also approved held on 2 and held on 2 and 4, and removed light eighths completely ("remove light
  eighths completely"; an old snapshot plays Held). Draw on the Rhodes, felt and vibes: held 35%, half 29%, held on 2
  12%, held on 2 and 4 8%, a comp rhythm 16% (the Knight keeps stabs). The new styles never thin under a busy lead,
  so they strike softer (held on 2 about 2 dB under held): to judge in songs. The vibes play the rhythms too (Sam).
  The workbench picks all of them in one Comping row (Sam: one place, not two).
- **Every comping rhythm is data, drawn evenly, leaning on the song's motion (Sam, 2026-11-05: "I think of all comping
  rhythms the same"; "Any reason to not make them even?"; the rolled rhythm follows the song's motion, and a live Motion
  change leaves it: "It should stay"):** **Done (2026-11-05).** Held, half, stabs, held on 2 (and 4), charleston,
  tresillo and late one are rows of one table, each with its weight (1: even), busyness, keys, thinning, holding and
  velocities in `patterns/model.json`; at motion 50 even, at 20 sparse (held, late one) 3x busy (tresillo, stabs), at 80
  the reverse. The Rhodes at motion 50: 1 in 7 each (was held 35%, half 29%). The pad and organ keep held 44 / half 36.
  Late one up to 36-50 (was 32-46): drawn more often, the pad sat under 5 dB below the vibes' late one.
- **The drum pattern, the bass style and the strum pattern lean on the song's motion too (Sam, 2026-11-05: "motion
  roll -> informed busy-ness roll"):** **Done (2026-11-05).** The roll draws the motion first; each part's options
  are drawn evenly (the bass from the styles the song allows) with the comping's tilt, and a live Motion change
  re-draws none. Busy: drums boom-bap 1-3 and bossa, brushes middle, half-time, sparse and half-time-2-shaker-2
  sparse; bass pedal sparse, roots middle, walk busy; strum ballad and anticipation sparse, D-DU-UDU, bossa,
  charleston and the boom-chucks middle (the train busy), chuck, offbeat skank and syncopated push busy
  (`patterns/model.json`). At motion 50 the bass is even too: a walking set walks 50% (was 35%), with a pedal a third
  each (was pedal 50%, walk 17.5%). The kick phrases stay the workbench's (not rolled). The lead and the pad unchanged.
- Calm songs thin the hats (Sam, 2026-11-05: 'The high hats on Motion 20 (calm) are still a bit busy'; 'anything below
  25 motion should have no hats'; always, by the rolled motion): below 25 no closed or open hats (ride, shaker, kick,
  snare and rim kept); 25-34 the closed hat drops its ghosts; 35 and up as written.
- **To explore (Sam, 2026-10-30): remove the busy-lead interaction from the comping.** Today a bar with 4 or more lead
  onsets thins half notes and stabs to beat 1 (and offbeats and the arpeggio, now off). Sam isn't a fan of a busy lead
  modifying the comping. A/B songs with and without it; if he doesn't miss it, remove it (and its tests). The A/B
  tool (Sam): a workbench checkbox, "Busy lead thins the comping" (on by default; firmware unchanged).
- **Drum patterns ignore the set (found in the rule-docs refresh, 2026-10-30; to revisit).** Each of the 8 engine
  patterns is equally likely in every set, flavor and family (the motion's tilt aside, 2026-11-05). Sam's two-bar kick phrases (k2-k6) exist only in the
  workbench, and no engine pattern is two bars long. Decide per-set or per-flavor pattern weights, and bring the
  kick phrases into the engine. (Kick phrases in the engine since 2026-11-10: boom-bap songs, 30%; above.)
- **Drums, 2026-11-04:**
  - **CR-78 rim:** Sam: "i don't like the cr-78 rimshot sound." The CR-78 kit's rim lane plays its cymbal (45ffa71).
  - **Saved patterns:** a pattern Sam saves in the workbench becomes a drums candidate on the next import
    (tools/patterns/saved.py, 777c97f). His "half-time-2-shaker-2" is drums pattern 9 (5cacad0).
  - **Hats in breaks:** Sam: "I don't want them to always drop out in quiet sections, but they could sometimes."
    Kick-out breaks drop their hats in about 40% of songs (b3788ef; Sam, 2026-11-05: "sound great").
- **The six modes stay (Sam, 2026-11-06):** the move to major or minor (2026-11-05) is undone; he heard odd
  progressions and voicings with it. Progression 19 is drawn again; 45-48 are gone.

### Bass rhythm (spike VNL_BASS, kept as is by Sam to revisit)

- Tones approved by ear; rhythm was the open question. The spike (roots and root-fifth songs only) keeps all four
  cells, one per song: the kick's with beat 3 sat out now and then 34%, half notes on 1 and 3 22%, beats 1 and 2
  22%, beats 1, 2 and 4 22%. Plus anticipation (~35% of chord changes, an 8th or 16th early, ringing over), beat 1
  held and the rest long or short per song, ghost notes (60% of songs, ~30% of strikes). Revisit: the weights, how
  much anticipation, and whether to bring it into the engine proper (it lives in song.c as a spike).
- **Done (Sam, 2026-11-04: "cheesy bass lines that go from 1 to 5... get rid of them"; faf260c):** the root-fifth
  line (BS_ROOT_FIFTH) is gone; its songs play roots on the kick.

### Sound (judge by ear, Task 8)

Renders: `app/build/new/<seed>/` and `<seed>-nobed/`, one per set (Task 8's report lists them).

- **To do (Sam, 2026-10-30): less noise on the flute.** Its breath is too present. Today: a band-passed noise
  layer (`patch.c` INS_FLUTE, its band at 1800 Hz, wide: BREATH_K 1.4) plus an onset chiff
  (CHIFF 1.5, gone in 80 ms; `voices.c`). Try a lower breath level, a narrower or higher band, and a softer
  chiff, as an A/B in songs; keep the tone's level matched. (The breathy-pad spike reuses this breath, so tune
  them together.) (Sam later cut the breathy pad, below.)
  **Spiked 2026-10-30, no change (Sam: "I honestly can't tell a difference"):** the breath halved, then a narrower
  band and half the chiff, then a quarter with no chiff (noise -25.5 -> -37.5 dB under the tone;
  `worktree-agent-a46212430593ae35a`, renders `app/build/new/flute-noise-2026-10-30/`). The breath is not what Sam
  hears; if the flute still sounds noisy, look elsewhere (the pulse in its blend, the bed, the dust chain, aliasing
  high up).
- **Cut (Sam, 2026-10-30): the breathy pad.** A spike added the flute's breath noise to the pad layer (about -22 dB
  under the tone), then opened its filter, slowed its swell and sent more of it to the plate
  (`worktree-agent-abd52963f71c8c0ef`, 0efc297; renders `app/build/new/pad-breath-2026-10-30/`). Sam: none of it
  did much; cut, not merged. Ideas only, if an airier pad comes back: a sampled choir or "aah" pad, a brighter
  slow-moving filter, tape-style movement on the pad.
- **To do, spikes (Sam, 2026-10-30): two new synth engines, from the SLOOP firmware's list** (GPL-3.0-only code:
  borrow ideas and preset values, or accept GPL-3.0-only if code is copied). Each is a new kind of sound, not another
  preset on an engine we have; each costs a few KB of flash and some CPU, and stays only if Sam clearly hears it in
  songs. (1) **Formant voice**: choir and "aah" pads, the airy pad's best path (the breathy pad was cut). (2)
  **Tonewheel organ**: drawbars, as the synth route to the better B3 Sam asked for (the sampled B3 spike is delayed).
  **Formant choir spiked 2026-10-30** (`VNL_PAD_CHOIR`, `worktree-agent-ae04880fa4064e176` 159ff9d, renders
  `app/build/new/formant-choir-2026-10-30/`): saws through one shared 3-formant bank on the pad layer. Sam: the static
  "aah" (A) "doesn't work well"; the slow "oo-aah" morph (B, 12 s cycle) "does". Keep B; ~3.5 KB flash (est.), +0.018
  CPU for the layer. Next: make it a pad-layer choice, then decide how often songs roll it.
  **Tonewheel organ spiked 2026-10-30** (`worktree-agent-a5487d5ef8ed95342`, 0f39f8f..14dba3b, renders
  `app/build/new/tonewheel-2026-10-30/`). Sam: "The organs sound great. All settings sound good"; A (888) and B on
  song 3 sit too low (follow-ups rendered: an octave up, or the 16' out), plus a fast rotor, a scanner chorus, an
  organ bass and a one-stop organ pad, awaiting his ear. ~6 KB flash (est.).
  **Sam on the follow-ups (2026-10-31):** register: A an octave up on songs 1 and 3, A with the 16' out on song 2
  (whose voicings already sit higher). Rotor: the fast rotor isn't fast enough, but the scanner chorus "does the
  trick, even on slow"; so the slow rotor plus the chorus, and the fast rotor is dropped (my call, over making it
  faster). Organ bass: no clear difference from the electric bass; dropped. Organ pad: the one-stop 4' beats the 8'
  but is too static; next, the 4' pad with the chorus. Choir: the oo-aah pad layer and the choir as the solo voice
  both sound good; stage 2 (the solo moves onto the choir, the vox pad and M_PAD's formants go) is next.
  **Organ pad (2026-10-31, renders `app/build/new/organ-pad-chorus-2026-10-31/`, spike commits 0de966c..4c0824e):**
  Sam picked the 4' with chorus C3, used as the rotary substitute and switched by section: on where the full kit
  plays (kick and hats), still in intros, outros, breaks and hat-less grooves, 60 ms crossfade. Sam: "a good start";
  he may tweak the on/off rules later. A stuck-note bug in the spike's organ legato retune (the pad held G from
  0:12 on song 5) is fixed (1de05bc); the organ pad renders in `tonewheel-2026-10-30/` still have it.
- **To do (Sam, 2026-10-30): control the organ's stops and settings in the workbench, and maybe eventually on the
  device.** Workbench rows for the nine drawbars (0-8 each), percussion (off / 2nd / 3rd, fast or slow), key click,
  rotor (slow / fast), scanner chorus (off, C1-C3), and the register (octave), per song and saved in snapshots and
  feedback. On the device, later: a page of knobs for the drawbars and the rotor, once the organ is in the firmware
  and the UI has room (the screens spec).
  **The organ as a lead waits on these controls (Sam, 2026-11-04):** "hold off on the organ lead until we have the
  ability to modify it in the UI (drawbar settings, chorus, percussion, etc)". Tried once as a lead (2026-10-29, the
  old organ, struck line): "might work eventually, but would need lots of tweaks".
- **Keys that sound alike or fake (Sam, 2026-11-04).** "The wurli sounds fake"; and "a tough time hearing the
  difference between the rhodes, felt piano, and dx piano" (all modeled). A sampled Wurli is being searched (a
  licensed set, as a workbench candidate); the keys compared in one song: `app/build/new/keys-compare-2026-11-04/`.
  Options if they stay alike: keep one modeled EP, make each distinct, or sample them.
  **Done (Sam, 2026-11-04: "dump the wurli", "retire"; the DX, not told from the Rhodes in a mix, "yes"):** the Wurli
  (INS_WURLI) and the DX electric piano (INS_DX_EP) are retired with their sets: 3 Neo-soul Wurli, 8 Wurli and vibes,
  24 Wurli and Knight; 13 DX keys and FM bass, 20 DX and felt, 25 DX and Knight, 33 DX and vibes (ids never reused).
  Removed as the 2026-10-31 removals were: no patch, no set, offered nowhere in the workbench (not even behind "show
  retired"); an old snapshot's keys play the Rhodes (lib_keys_approved), said on restore. The Wurli's bark went from
  the electric-piano core; the Rhodes and felt piano render bit-identical. Every mood keeps sets (sad 10, wistful 15,
  warm 11, sunny 8). Left for Sam: the synth sets' share of songs falls from 20%+ to 12.5% (the organ, triangle and FM
  basses' three sets; the test's floor follows them, 10%), and the Knight leads only warm and sunny sets now (23, 28).
- **Flash, for Task 8 (found 2026-10-30): models left unused by the removals.** The Karplus-Strong pluck (M_PLUCK)
  only played the upright (removed); the FM bell (M_BELL) plays the kalimba, music box and bells, which leave with
  the kalimba lead and the ear candy. Their code and patches can go unless a spike reuses them.
- **To do, for Task 8 (Sam, 2026-10-30): the triangle bass on the FM bass engine.** Rebuild the approved filtered,
  mellow triangle bass as an M_BASS patch (index near 0 for a near-sine, a little odd-harmonic edge), A/B in songs
  against today's; if Sam can't hear a loss (or prefers it), switch, and drop the chip engine (M_CHIP): the triangle
  bass (INS_CHIP_BASS) is its only user. Saves an engine's flash.
- **To do, spike (Sam, 2026-10-30): modal vibes in place of the vibes sample.** A struck-bar model (a few modes, the
  second tuned 4x the first, the motor's tremolo, a soft mallet strike), after Ravel_FM1's mallets (GPL-3.0: borrow
  the approach and values; clone and read it first). Our old synth vibes were the 2-op FM bell, which Sam heard as a
  plucked synth; this is a different model. A/B against the sampled vibes in the same songs; switch only if Sam can't
  tell or prefers it. Saves about 16 KB (the sample ~18 KB, the model ~1-2 KB); CPU ~0.1-0.2 for 2 voices, to measure.
  **Spiked 2026-10-30 (Sam: "The synth vibes sound passable!"):** 3 modes at 1 : 4 : 9.9, a soft mallet, the
  resonator's bloom, a 5 Hz motor (`VNL_VIBES=modal`, `worktree-agent-abb4f5ea87495da93` f81b4e8, renders
  `app/build/new/modal-vibes-2026-10-30/`). ~3.6 KB against the sample's 18.4 KB (~15 KB saved), ~30% less CPU
  (0.15 vs 0.21 for 2 voices). Kept on its branch: a ready flash trim for Task 8 if needed; otherwise the sample stays.
  **Done (2026-11-03, Sam: motor on and off "both sound good... can't tell the difference"):** the modal vibes, motor
  off, are the vibes; the sample and the motor are gone (6235b2b, 9589832). Est. -15.6 KB flash, vibes CPU 0.21 to
  0.145. The FM triangle bass (Task 8's other A/B) was dropped: Sam couldn't tell, and it saved only 0.8 KB for +0.054
  CPU, so M_CHIP stays.

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
- Thin level margin (leads change, 2026-10-29): Emilyguitar and flute's lead-over-keys median sits exactly on the 8.0 dB
  floor after the flute's −0.3 dB trim for set 9. A later level change failing that test is this, not a regression.
- To check by ear: the vibraphone loops a 60 ms segment under its 10 s decay (long notes may sound static); the vibes
  led about 29% of songs (9 of 29 sets); rebalanced 2026-10-29 (Sam): set 10 now leads with the synth pluck, set 30 the flute, set 33 the saw lead, so the vibes lead 5 of 29 sets (2, 4, 6, 8, 26) instead of 9.

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
