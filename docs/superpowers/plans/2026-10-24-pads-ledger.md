# SDD ledger — plan: docs/superpowers/plans/2026-10-24-pads.md

Workspace: branch mix-synths (not main), as the previous plans; plan base 2894050.

## Pre-flight scan

| Rows | Produces / consumes | Found |
|---|---|---|
| T1 -> T2 | T1: V_PAD, PM_PAD, INS_STRINGS/INS_WARM_PAD, lib_set_t.pad, vx_assign(V_PAD); T2 consumes them | consistent |
| T2 -> T3 | T2: V_PAD events; T3 tunes levels, budget | T2 step 4 says "all pass" but allows a level failure noted for T3: ruling below |
| T1 -> T3 | T1: ROLES[V_PAD] gain 0.06, cost row; T3 retunes, re-measures | consistent |
| T1 self | tests (pad voice, role buses, count) vs code (enum, M_ENS, patches, bus) | consistent |
| T2 self | tests vs pads_chord/pads_solo/pads_section_in signatures | pads_chord has no key/mode argument: it takes r (roll) which carries both: fine |
| T3 self | level test vs tuning step; budget cuts vs spec order | consistent |

Ruling: Task 2 may complete with only the level tests (test_lead_over_keys, test_levels_steady) failing because of the
new layer, recorded in its report; Task 3 owns them and must leave them passing unwidened — the plan says so in T2
step 4 — cost if wrong: a red suite between T2 and T3 commits.
Task 1: dispatched (BASE 2894050)
Task 1: implemented 3c55252 (DONE_WITH_CONCERNS: quarter rate taken (cut b), layer 0.070, max 0.903; NPD 4 steals the old chord's release at a change; pad not ducked; chorus depth from the chords' patch); review dispatched
Task 1: review: Needs fixes (3 Important: chorus depth is the chords'; quarter rate unconditional; NPD 4 steals releases (plan-mandated))
Task 1: Ruling: chorus — the pad's send is scaled so its wet equals its own patch's depth over the shared chorus (send = patch depth / the chords' depth, capped at 4x), its dry compensated like the chords' bus — spec says "the chorus's ensemble depth" on a shared chorus — cost if wrong: the strings' width needs retuning by ear.
Task 1: Ruling: quarter rate only where the low-pass allows (the note's highest cutoff with key tracking ≤ 4 kHz), half rate above — spec cut (b)'s own condition — cost if wrong: more CPU on high notes; Task 3 owns the budget cuts if the total passes 0.97.
Task 1: Ruling: NPD stays 4; pad voices are lanes (0-2 the chord, 3 the solo): a V_PAD event carries its lane in song_ev_t.pad (1-4); a note on a lane whose voice is held glides there (legato, about 60 ms) without a restrike, so a chord change moves each voice to its voice-led next note (common tones simply hold) and the sound never stops — the spec's "releases into the next" in the voice-led form strings play — cost if wrong: no overlapping release tails; a swell on a chord change only where a lane was silent. Task 2 must emit lanes.
Task 1: fix round 1/5 implemented (f9f0e5d): lanes glide, the pad's own chorus wet, half rate above a 4 kHz cutoff; layer 0.080-0.082, forced max 0.905-0.917 (layer silent in songs yet)
Task 1: Ruling: the 4 kHz rule never triggers in the pad's range, so the solo note (C5-C6) stays quarter rate (about -31 dB aliasing at C6): kept for Sam's ear (judge-by-ear item in Task 3's renders) rather than half rate now — the budget is already tight (Task 3 expects 0.98-0.99 before cuts) — cost if wrong: a slightly gritty solo note, fixed by half rate for MIDI >= 72 if Sam hears it.
Task 1: fix round 1/5 (4 addressed, 0 open; commits 3c55252..f9f0e5d)
Task 1: minor (deferred): ens_legato on a lane released <25 ms ago sets held/gate without re-entering attack/sustain explicitly (untested path)
Task 1: minor (deferred): a glide keeps its old rate (a cutoff crossing 4 kHz can't happen with current patches)
Task 1: minor (deferred): pad_seg duplicates the envelope math (3 copies); file growth of voices.c
Task 1: minor (deferred): test_pad_voice's determinism is same-process only; MIDI export (score.c NAME/PROG) has no pad track; clash_test's v != V_PAD exemption must go in Task 2
Task 1: minor (deferred): the chorus's modulation depth is still the chords' (only the pad's wet follows its patch)
Task 1: complete (commits 2894050..f9f0e5d, review clean after 1 fix round)
Task 2: dispatched (BASE 0403269)
Ruling: (Sam) Task 3 adds an A/B for the chord part: a host-only render switch (env VNL_PADS=double, threaded like VNL_SET) where the chord part doubles the keys voicing (on the keys strikes, held comping only) instead of its own voice-led 3 notes; renders both ways for Sam to choose — Sam asked to compare a pad layered into the comp voice — cost if wrong: one unused switch.
Task 2: implemented a3896fc (DONE; pads_solo takes bass notes; lead check on known bars only; 2-note chords pick lanes; ties across 4-bar lines restrike same pitch on lane = extend)
Task 2: review: Needs fixes (1 Important: chord-part common tones change lanes in 6.1%)
Task 2: minor (deferred): the chord part checks the bass as the root pc only (not the bar's walking notes); safe by clash test
Task 2: minor (deferred): lead avoidance mostly by construction (first bar of a group); pads.h should say so
Task 2: minor (deferred): have3/have7 computed in the candidate loop; pen lumps b5/5/#5; bass_spike path not modeled for the solo; a self-comparison purity check; no MusicXML pad part
Task 2: Ruling: solo "next legal choice up" (design call 5) vs the brief's smallest move either way: the code's smallest move stands (the brief's interface; voice leading) — cost if wrong: the solo occasionally steps down under the chord top's 3rd... no: it stays ≥ 3 above by the clamp, so only direction differs.
Task 2: fix round 1 implemented (6e86d01): lanes kept 0/18055; top-voice smooth 90.9% (was 94.7%, need 80)
Task 2: fix round 1/5 (2 addressed, 0 open; commits a3896fc..6e86d01)
Task 2: complete (commits 0403269..6e86d01, review clean after 1 fix round)
Task 3: dispatched (BASE 6e86d01)
Task 3: implemented 9a6c48d (BLOCKED on budget: seed 20 forced 0.976-0.999; lanes ~0.011-0.018 each, fixed part ~0.037; cuts (a)-(c) at best ~0.975)
Task 3: Ruling: before any cut, optimize the layer's fixed part (the pad bus's section filter, mud high-pass and sends run in the pad's low-rate domain before the interpolation; the bus and pad_seg skip work when no pad voice sounds and their states are quiet) — not a musical reduction, so inside the builder's latitude; then re-measure, and take (a)/(c) only if still over; if all that is over 0.97, stop and report to Sam — cost if wrong: a filter character change on the pad bus at quarter rate (it's a dark layer, low-passed well under 5 kHz).
Task 3: budget round: fixed part optimized (9a991f6), cut (a) 9ad3b6f (saved ~0), cut (c) c520a5e (2-note chord), cost docs b70da83; seed 20 forced 0.970-0.973 (5 runs), standard list one spike 1.001 under host load 5; layer --parts 0.052-0.057. Still over 0.97 by ~0.003: STOP per spec, reported to Sam.
Task 3: Ruling (Sam chose option 2): revert cut (a) (the solo note gets its own lane voice back; it saved nothing), keep cut (c) (2-note chord part), accept the budget at ~0.97 on the host bench (under the 1.0 hard limit; Plan 2 measures the device); judge the 2-note chord by ear; if thin, a follow-up wins CPU elsewhere and restores 3 notes — cost if wrong: thin pads until then.
Task 3: implemented (b9080f8 revert a, fc4130f docs; 45 renders); seed 20 forced 0.937-0.985, standard max 0.956
Task 3: review: Approved with 1 Important (docs: "bit-exact" claim for 9a991f6 too broad)
Task 3: minor (deferred): quarter-rate filter amounts lag a chunk; a rare misaligned voice on a mid-segment rate switch; vsmooth/moves printed not asserted; a long comment line; seed 20 is the Plan 2 device case to watch; Nylon and sax median 4.1
Task 3: fix round 1 implemented 6d98b5e (docs narrowed; picker seed 20 0.967 forced; --parts 0.045-0.047; bench pad part matches songs)
Task 3: fix round 1/5 (1 addressed, 0 open; commits fc4130f..6d98b5e)
Task 3: minor (deferred): cost.md dry-row wording ambiguous; a very long cost.md line
Task 3: complete (commits 6e86d01..6d98b5e, review clean after 1 fix round)
Final: review: with fixes — 1 Important: 2-note chord part moves notes onto the spare lane (free) so 97% of changes leave a lane releasing, 25% a semitone smear; Minor: gliding lanes keep the old level; pads_double static; lead first bar only (ruled)
Final: Ruling: fix wave = Important 1 (empty-lane cost = the glide it replaces + a small penalty; the free lane only for leaps > 4 semitones, which restrike; tests: no release-plus-swell at a step move, no glide over 5 semitones) + Minor 2 (ramp amp toward the new velocity over a glide); then re-measure cost and re-render — cost if wrong: a fix-wave regression the re-review catches.
Final: Ruling (declined to judge): pad notes vs ear candy — the spec names keys, bass and lead only; added to the ledger's To revisit — cost if wrong: an occasional solo-vs-candy rub until checked.
Final: fix wave implemented d6211c5, 933df7b (releasing lanes 15750->1236 leaps only, semitone smears 0, glides <=5; held lane ramps velocity; cost unchanged ~0.97, one run 0.994 on load 8)
Final: re-review: all addressed, no new breakage. Plan complete.
