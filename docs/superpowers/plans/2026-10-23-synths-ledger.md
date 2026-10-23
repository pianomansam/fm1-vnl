# SDD ledger — plan: docs/superpowers/plans/2026-10-23-synths.md
Spec: docs/superpowers/specs/2026-10-23-synths-design.md. Branch mix-synths (from main a27abce). Executor: subagent-driven. Waits for the lead-level fix (in progress on the same branch).

Pre-flight scan:
| Rows | Produces vs consumes | Found |
| 1→2,3,4,5 | p[16]; INS_* before INS_NONE; lib_ins_sustained entries | consistent; each task appends INS entries — later tasks must not renumber earlier ones |
| 2→4 | M_SUB INS (Juno, organ, Moog, saw/square/sine lead, pluck) used by sets | consistent |
| 3→4 | DX EP, FM bass, FM bells used by sets | consistent |
| 4→5 | sets change the roll; level/cost tests draw from it | consistent |
| level fix→all | lead-over-keys test + keys duck (on this branch first) | Task 5 extends that test; tasks 1-3 must keep it passing — new lead patches may need levels before Task 5 |
| T1..T5 self | tests name measurable traits | consistent |
Ruling: new lead patches (chip lead in T1, synth leads in T2) get provisional levels that keep the lead-over-keys test passing; Task 5 does the final balance — the test is on the branch before T1 — cost if wrong: a level tweak per task.
Lead-level fix 7b946ff landed (lead +5.5 dB over keys in every set; keys duck 3 dB; forced max 0.848). Quick review dispatched.
Lead-level review: engine sound; 2 Important test gaps — test_lead_over_keys asserts only a global median 4–7 / per-song floor +2 (spec: every set 4–6 dB); the duck is asserted only on the solo/filtered path (not the unfiltered full-rate and half-rate ramps).
Ruling: both carried into synth Task 5 (which extends test_lead_over_keys to the new sets) — Task 1 is editing song_test.c concurrently; Task 5 adds a per-set mean assertion ~4–6.5 dB, median 4–6, and a non-solo duck check at full and half rate — cost if wrong: the gaps stay unguarded through Tasks 1–4 (the code itself reviewed sound).
Lead-level: minor (deferred): lk_cmp_f wraps lv_cmp; LIB_NSETS_MAX 16 silently skips sets (make it a CHECK — matters once synth sets push past 16!); amp_t comment stale; vx_solo adds a production branch for a test hook. Guitar lick retone: Sam's ear. Bark window 0.75 dB has a thin margin (reads 0.55).
Task 1: Ruling: the spec's "a little bit-crush character" for chiptune is owned by no task — routed to Task 4 (a per-patch amplitude quantize on the chip patches, e.g. 6-bit, cheap) — cost if wrong: a small add later.
Task 1: minor (deferred): aliasing test excludes ±(swing+40 Hz) bands and only the lead at 88/duty 0.5; first sample outputs −1 (inaudible); dead noise wave 2 untested (drop or use); long comment lines; stepped triangle not band-limited above ~MIDI 60 (note beside the p[] doc); chip cost rows from a busy host — re-bench in Task 5.
Task 1: complete (commits 7b946ff..f08c7d5, review clean). Forced max 0.838.
Task 2: ⚠️ resolved: the no-arg picker run is carried to Task 5's full bench (Task 2 reported --forced only, max 0.821).
Task 2: minor (deferred): steal test averages 5–25 ms (tighten to 5–10 ms or −40 dB); no Moog-bass glide/after-rest test on the bass role; noise path (p[3]) untested and its half-rate scale dead; half-rate rule test restates sub_top's formula; square lead has 10 cents vibrato (voicing choice for Sam).
Task 2: complete (commits f08c7d5..9fcf367, review clean). Forced max 0.821.
Task 3: minor (deferred): bench.c:53 long line; doubled blank line song_test.c:238; DX EP "decays faster" check includes the transient (weaker than a body-decay measure); docs/cost.md:166 should state the forced max (0.822), not point to a task report.
Task 3: complete (commits 9fcf367..6aae108, review clean). Forced max 0.822.
Task 4: Ruling: the −91 dBFS carryover residue (Rings' 16-bit reverb memory, pre-existing) is accepted like Plan 3a's ruling (−80 dBFS bound for the Rings path) — Task 5 restores the left-out Juno carryover pair under a −80 dBFS bound with the reason in the test — cost if wrong: none audible.
Task 4: ⚠️ resolved: is_synth's enum range holds today (INS_CHIP_CHORDS..INS_FM_BELL are exactly the synths); minor (deferred): an acoustic INS appended later would count as synth; the old pads aren't counted as synths (share undercounts).
Task 4: minor (deferred): docs/cost.md:295 stale 'no set uses a subtractive voice yet'; only p[7]'s range checked for chords/bass/blip crush; report's 'steal fade not quantized' claim is wrong (harmless); vx_chip_out called twice per sample in the test.
Task 4: fix round 1/5 (1 addressed, 0 open — crush tested on the voice's output, all four chip patches, RED on two mutants; commits 4e2b6ce..30be26f)
Task 4: complete (commits 6aae108..30be26f, review clean).
Task 5: minor (deferred): carryover pair 9 uses a blanket 1e-4 from 12 s (3e-5 would pass); test_levels_steady seen[] indexed by set id unguarded (fine ≤ 32); lofi.md "Steady levels" bullet says 2/4 dB while the test checks 3/6 dB (pre-existing); muted trumpet +1.0 dB and felt piano +0.6 dB (old sets) need Sam's ear — add to the judge-by-ear list; thin margins (Felt and trumpet median 5.9/6; lowest songs +2.4/+2.5; Felt and flute mean 4.3) — a future voice change needs a rebalance.
Task 5: complete (commits 30be26f..HEAD, review clean). Standard bench list --forced 22 20 7 138 100 200, max 0.863.
Final review: with fixes — Important: glide never fires in songs (no held transitions on lead; bass only same-pitch ties). Ruling: engine-only fix — a mono patch glides when the previous note ended < 25 ms before the next onset and the interval is ≤ a perfect 4th (selective portamento, ~a third of eligible notes); Sam judges by ear — cost if wrong: one window/interval constant.
Final: minor (deferred): two notes at once on a mono lead within a steal window (rare); chip blip aliasing above MIDI 88 untested; plus the earlier deferred minors triaged "fine to leave".
Final: fix wave clean (glide 25 ms / a 4th; docs; comment; commit e8eda3c).
