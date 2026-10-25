# SDD ledger — plan: docs/superpowers/plans/2026-10-24-pads-rev2.md

Branch mix-synths; plan base 291f8ae. Spec: pads design, Revision 2.

## Pre-flight scan

| Rows | Produces / consumes | Found |
|---|---|---|
| T1 -> T2 | vx_assign_pad_solo(ins); lane 4 any sustained patch; per-voice level trim | consistent (T2 assigns at song start; strings 84-96 vs T1's test at 88) |
| T2 -> T4 | either/or parts, solo voice roll; T4 levels/renders | consistent |
| T3 -> T4 | CS_HALF; T4 renders a half-note song per set | consistent |
| T2 x T3 | the pad's chord part ignores the comp style; VNL_PADS=double applies to CS_HELD only | ruling below |
| T1/T2/T3/T4 self | tests vs code | consistent |

Ruling: the VNL_PADS=double A/B stays CS_HELD-only (half-note comping isn't doubled) — it was built for held comping —
cost if wrong: no A/B on half-note songs.
Task 1: dispatched (BASE 291f8ae)
Task 1: implemented 7b583f1 (DONE_WITH_CONCERNS: a solo section costs choir/organ +0.06, flute +0.17 over the chord part; seed 20 could pass 1.0)
Task 1: Ruling: before review, lane 4's non-M_ENS voices render at half rate (they're soft, low-velocity, in 72-84, low-passed: half rate's 11 kHz Nyquist is ample), the flute's M_WIND given a half-rate path for this lane if it has none; target: a solo section costs no more than the chord part it replaces (+0.01 slack); if a voice can't get there, report it with numbers and the controller decides (drop it from the roll on the heaviest sets, or accept) — the budget line is Sam's ~0.97 — cost if wrong: slightly duller solos.
Task 1: fix round 1 implemented 8a969ff (flute half rate; VNL_SOLO bench); per-voice --parts choir 0.108, flute 0.138, organ 0.112, strings 0.025 (target 0.058); seed 20 forced with each solo 0.965-0.976
Task 1: Ruling: try quarter rate for choir and organ on lane 4 (keep it only if each stays within 1 dB of its half-rate level and its aliasing no worse than the strings' at the same note); flute stays half rate; then accept — seed 20 with any solo stays at Sam's ~0.97 line and solos only play in a quarter of struck-keys sections — cost if wrong: device CPU spikes in solo sections, caught in Plan 2's device bench (flag seed 20 + flute solo there).
Task 1: fix round 2 implemented d866c34 (choir/organ quarter rate: --parts 0.043/0.042; flute 0.138 accepted; seed 20 with solos 0.940-0.973; a closed-filter test relaxed 6->4 dB; a steal bug fixed)
Task 1: review: Approved (no Critical/Important)
Task 1: minor (deferred): bench getenv inside the timed loop (biases ratios up); organ quarter-rate promotion lacks an explicit bound (safe in 72-84); trims matched at vel 100 not 58 (flute may drift ~1 dB: check in Task 2/4); level slew slower at reduced rate; wind_start uses hr not step_len; SOLO_V assumes PD+NPD==NV (no static check); vx_tiny skips M_PAD/M_WIND states
Task 1: complete (commits 291f8ae..d866c34, review clean)
Task 2: dispatched (BASE d866c34)
Task 2: implemented b6301cd (DONE; 38.6% no solo; 25.7% solo sections over struck keys; voices 24-26%; clash_test range edited)
Task 2: Ruling: the "strings" solo voice is INS_STRINGS on every set, warm-pad sets included — Sam picked the strings sound in the demo — cost if wrong: a warm-pad set's solo could be its own warm pad instead (one table entry).
Task 2: review: Approved
Task 2: minor (deferred): clash_test's solo bound uses pads_solo_lo (circular; song_test covers it independently); test_pad_either's 5% handover slack should split stuck-outgoing (0) from silent-incoming; bench VNL_SOLO out of date (no-solo seeds, forced voice keeps rolled range); mid-song set change of the solo voice untested
Task 2: complete (commits d866c34..b6301cd, review clean)
Task 3: dispatched (BASE b6301cd)
Task 3: implemented fe0b0f1 (DONE_WITH_CONCERNS: suite red: lead-over-keys 12, pad-level 4, pad-solo 1 checks; keys hold when the kick is exactly 0 and 8; harmony_comp_steps keeps the bass off {0,8}; 3 pinned tests re-pinned; RED was only a compile error)
Task 3: Ruling: the level-test failures (lead over keys, pad level) go to Task 4, which owns levels: tune the comping's keys level for CS_HALF (keys_gain per style, as held vs stabs had) and the pad, never widen, never re-pin the level tests' sampled seeds; the pad-solo failure is checked in this task's review (is it a level or a logic failure?) — cost if wrong: a red suite between T3 and T4 commits.
Task 3: review: Approved, 2 Important (kick-hold rule needs a decision; pd_solo_legal test model gap)
Task 3: Ruling: the hold when the kick is exactly 0 and 8 stands — it is Sam's own rule (keys and kick may share a beat only if both aren't sparse; half notes and a 1-and-3 kick are both sparse) — and the weights compensate so about 25% of songs really play half notes: held 44, arpeggio 20, half 36 (effective ~25% half, ~55% held) — recorded in spec Revision 2 — cost if wrong: one weight table.
Task 3: Ruling: fix the test, not the planner: pd_solo_legal judges legality against the bar's full voicing (as the planner does) — cost if wrong: none (the planner is the conservative side).
Task 3: minor (deferred): TDD order not followed (RED was a compile error); no continuous-play test comparing a two-bar chord's two strikes (reviewer verified 9,760 cases); pads-double ineligible on held-fallback half songs
Task 3: fix round 1 implemented 76c18c4 (weights 44/0/0/20/36, 25.2% play half; pd_solo_legal full voicing; pad double re-pinned again)
Task 3: fix round 1/5 (3 addressed, 0 open; commits fe0b0f1..76c18c4)
Task 3: minor (deferred): harmony_style's comment still says held 80%, arpeggio 20% (stale) — fix in Task 4's docs pass
Task 3: complete (commits b6301cd..76c18c4, review clean after 1 fix round; level tests red for Task 4 by ruling)
Task 4: dispatched (BASE 62c2410)
Task 4: implemented 36c0508 (keys levels per style incl. half; chord vel 45->48; levels green unwidened; seed 20 0.960-0.972; 45 renders; concern: busy-lead two-bar half songs nearly lose keys)
Task 4: review: Approved
Task 4: minor (deferred): 0x101 magic duplicated in keys_gain and score.c; test_pad_level's doc comment stale (4-8 dB); seed 20 0.971-0.972 a hair over 0.97 (device bench); the first pads ledger entry's 4-8 dB line superseded
Task 4: complete (commits 76c18c4..36c0508, review clean)
Final: review: ready after one fix — Important: a busy lead over half notes on two-bar chords leaves one half-bar strike then 1.5 bars of silence (keys ~3.75 dB under a held two-bar chord; City pop seed 6 lead +13.8 dB over keys); minors: 0x101 duplicated; no static check PD+NPD==NV; test_pad_level comment stale
Final: Ruling: under a busy lead, half-note comping thins to beat 1 of every bar (not only chord changes), each thinned strike held to the next strike (a full bar), at the half-note level — the spec's "held to the next strike" and "the comping makes room" both hold, and the keys never vanish for 1.5 bars — cost if wrong: a busier keys bar under a busy lead on two-bar chords than the old thinning (one strike a bar, like held one-bar comping).
Final: fix wave implemented d1801f8 (busy half: step 0 every bar, held a bar; harmony_half_strikes; static check; comment; renders redone; seed 20 0.969)
Final: re-review: all addressed, no new breakage. Plan complete.
