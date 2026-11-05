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
  candidates"):** in progress (2026-11-05): Charleston, tresillo, push, dotted, two-feel push and late one, added to
  patterns/comp-rhythms as workbench candidates for Sam to approve or remove.
- **To explore (Sam, 2026-10-30): remove the busy-lead interaction from the comping.** Today a bar with 4 or more lead
  onsets thins half notes and stabs to beat 1 (and offbeats and the arpeggio, now off). Sam isn't a fan of a busy lead
  modifying the comping. A/B songs with and without it; if he doesn't miss it, remove it (and its tests). The A/B
  tool (Sam): a workbench checkbox, "Busy lead thins the comping" (on by default; firmware unchanged).
- **Drum patterns ignore the set (found in the rule-docs refresh, 2026-10-30; to revisit).** Each of the 8 engine
  patterns is equally likely in every set, flavor and family. Sam's two-bar kick phrases (k2-k6) exist only in the
  workbench, and no engine pattern is two bars long. Decide per-set or per-flavor pattern weights, and bring the
  kick phrases into the engine.
- **Drums, 2026-11-04:**
  - **CR-78 rim:** Sam: "i don't like the cr-78 rimshot sound." The CR-78 kit's rim lane plays its cymbal (45ffa71).
  - **Saved patterns:** a pattern Sam saves in the workbench becomes a drums candidate on the next import
    (tools/patterns/saved.py, 777c97f). His "half-time-2-shaker-2" is drums pattern 9 (5cacad0).
  - **Hats in breaks:** Sam: "I don't want them to always drop out in quiet sections, but they could sometimes."
    Kick-out breaks drop their hats in about 40% of songs (b3788ef; Sam, 2026-11-05: "sound great").

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
