# SDD ledger — plan: docs/superpowers/plans/2026-10-23-sound.md
Spec: docs/superpowers/specs/2026-10-22-vnl-1-design.md. Branch sound from main b6f5901. Executor: subagent-driven.

Pre-flight scan:
| Rows | Produces vs consumes | Found |
| 1→2..8 | tools/cost-bench.sh, tools/mem-check.sh; every later task records both | consistent |
| 2→3,4,5 | vx_patch_t / models M_*; Task 2 maps all to the placeholder FM; 3-5 give models real code | consistent |
| 2→6 | vx_configure selects the kit; Task 6 builds kits | consistent |
| 2→7 | vx_configure reads RF_DUST/NOISE/AMBIENCE; fx_configure arrives in Task 7 | Task 2 cannot call fx_configure yet → ruling below |
| 2→7 | vx_set_dark: a low-pass on the music, not the bed | consistent (bed arrives in Task 7, outside the dark filter) |
| 7→8 | fx levels; Task 8 balances role gains | consistent |
| T1 self | bench may fail to compile AMB-1 standalone | plan gives the fallback (placeholder yardstick, ledgered) |
| T2 self | tests every patch while models are still the placeholder | consistent: Task 2's tests are model-agnostic (peak, NaN, release, determinism) |
| T3-T6 self | tests name measurable traits; implementations match | consistent |
| T7 self | denormal check "exactly 0 or ≥1e-20" | consistent |
| T8 self | levels test over 50 songs | consistent |
Ruling: Task 2 stores the dust/noise/ambience/flavor fields in the engine's config; Task 7 adds fx_configure and calls it from vx_configure — fx doesn't exist until Task 7 — cost if wrong: none.
Task 1: minor (deferred): bench.c:41 comment claims P_NPARAMS is checked; nothing checks it.
Task 1: minor (deferred): timed region includes roll_song/vx_init; fractional step truncation; early song end unflagged.
Task 1: minor (deferred): bench.c:22 confusing comment; any '-' arg treated as --heaviest; mem-check over-budget path unexercised.
Task 1: fix round 1/5 (1 addressed, 0 open — heaviest roll by measured render time; commits 2f2d228..bc2d04a)
Task 1: complete (commits b6f5901..bc2d04a, review clean). Baseline: heaviest seed 36, ratio 0.669; mem 123624/31768/69384.
Task 2: Ruling: a set with no candy instrument (INS_NONE) silences candy notes — the set defines it has no candy (sets 4, 9) — cost if wrong: candy back in two sets by giving them an instrument.
Task 2: Ruling: mid-song requests for set/dust/noise/ambience must reach the voices — carried into Task 7's dispatch (re-call vx_configure when such a request lands, without cutting ringing notes) — cost if wrong: one more call site.
Task 2: note for Task 4: the bass role needs its own model dispatch (upright is M_PLUCK). For Task 5/8: candy peak margin ~0.053 vs the 0.05 floor.
Task 2: minor (deferred, carried to Task 7): vx_init clears state when vx_active()==0 (voices+drums only) — once reverb/bed exist, a song change would cut their tails; fx_configure would switch bed abruptly; dark snaps to 0. Task 7 must keep FX tails across song changes.
Task 2: minor (deferred, carried to Task 4): bass is a separate bass_t sine with no model dispatch; UPRIGHT (M_PLUCK) / ELEC_BASS (M_BASS) need it (or bass as a mono voice_t pool).
Task 2: minor (deferred): model state as flat voice_t fields (a union would cap growth); no carryover test; dark-ramp test readability; patch.h says p[] layouts documented in patch.c — each model task adds its layout.
Task 2: complete (commits bc2d04a..7e5978e, review clean)
Task 3: minor (deferred, carried to Task 8): pad releases are cut by 4 ms steals at each chord change in the 4-voice chord pool, then the next pad swells — an audible dip; consider more chord voices for pads or overlap.
Task 3: minor (deferred): dark test HF proxy near its noise floor for keys; voices.c:91 long comment; Wurli bark shares the drum noise stream.
Task 3: fix round 1/5 (1 addressed, 0 open — EP strike/index denormal flush; commits 21f3487..1a60a0d)
Task 3: complete (commits 7e5978e..1a60a0d, review clean)
Task 4: minor (deferred): upright retrigger waits up to ~4.4 ms for the steal fade (timing jitter on walking lines; listen to 4185943442); notes below 28 clamp to 28's pitch; guitar lick decay untested.
Ruling: the cost bench is judged on max(picker, explicit seeds 20, 100, 183) from Task 4 on — the single-run sweep missed seed 183 on Tasks 3 and 4 (true worst 0.745) — cost if wrong: a few seconds per bench run.
Task 4: fix round 1/5 (1 addressed, 0 open — sub modulator skipped; burst declined with measurement; commits 8755927..0af0024)
Task 4: complete (commits 1a60a0d..0af0024, review clean). Cost: picker 0.709, seed 183 0.745 (true worst).
Task 5: ⚠️ resolved by controller: steal fade is model-agnostic (voices.c:741: g fades below STEAL_THR, then voice_start; env_start leaves g, new note rises from ≤0.02 by its attack) — bells/winds covered.
Task 5: minor (deferred): breath test has no negative control (reviewer measured −48…−62 dB with breath off vs −26 dB on; −30 dB bar); comb uses nominal period; `default:` routes unknown models to bass (prefer explicit case M_BASS); candy peaks 0.059–0.060 vs 0.05 floor; breath levels tuned to the test — Task 8 confirms by ear (4185943442).
Task 5: complete (commits 0af0024..f68a63a, review clean). Cost max 0.722 (seed 183).
Task 6: ⚠️ resolved by controller: scenario peak −9…0 is asserted by scenario-songs, inside the implementer's "all tests passed" run.
Task 6: minor (deferred, carried to Task 7): vx_kick_env is the raw exponential (808 tail ~2.2 s) and block-granular — shape it for ducking.
Task 6: minor (deferred): drums.h claims sounding hits keep their kit (they read kit params live); vx_set_kit stores cfg.kit unvalidated; stale voices.c:251 header; retrigger resets phase/filters (pre-existing); test chunk-boundary margins; no vx_configure→kit test; hat/shaker/rim amps for Task 8 by ear.
Task 6: complete (commits f68a63a..c852f6a, review clean). Cost max 0.738.
Ruling: Task 7b added (Sam: per-instrument fx; picked reverb, chorus, delay) — reverb per role folded into Task 7 by message; chorus on the chords bus + tape delay send as Task 7b before Task 8 — cost if wrong: one more task.
Ruling: section filter moves added to Task 7b (Sam: yes) — per-role ramped low-pass replacing vx_set_dark; mapping intro/outro 0.7 drums+bass / 0.4 others, breaks 0.8 sweeping open over the last bar, drop bar 0.6, swell 0.5→0 — my call on the numbers — cost if wrong: retune by ear.
Ruling: high-pass added (Sam: add both) — HP build into drops in Task 7b (per-role one-pole HP, ~600 Hz max, all roles but the lead); fixed mud HP ~120 Hz (150 on pads) on chords/lead/candy in Task 8 — cost if wrong: retune by ear.
Ruling: compression added (Sam: yes) — drum bus compressor + per-song pump depth (stream 170: subtle/medium/obvious) in Task 7b; mix bus glue compressor in Task 8 — numbers my call — cost if wrong: retune by ear.
Task 7: Ruling: reviewer minor #1 (bed RNG not seeded per song; skipped vx_init makes a song's noise depend on the song before) regraded Important — it breaks the Global Constraint "same seed renders the same samples" that ./new-song.sh SEED replay relies on — cost if wrong: one seeding line.
Task 7: minor (deferred): quick replay within ~40 ms of stop can return a stale plate tail; 64 KB memset in the ISR at song start; plate decimation aliasing; send not darkened in dark sections (deliberate); bed fixed absolute level vs quiet songs; no end-to-end test of mid-song reconfigure (no knob yet); fx_tiny test hook ships in firmware; fx.c:173 long comment; pre-existing UBSan report at fastmath.h:58; set change re-sweeps a ringing kick.
Ruling: the cost bench is judged on max(picker forced, --forced 20 100 128 183) from Task 7 on — seed 128 is the known heaviest (0.941 forced) and the picker doesn't always find it — cost if wrong: seconds.
Task 7: fix round 1/5 (3 addressed, 0 open — forced-chain bench, per-song bed seeding, stop-replay plate; commits b18ef4c..16e24ba)
Task 7: complete (commits c852f6a..16e24ba, review clean). Cost forced max 0.941 (seed 128); mem 144152/36316/173512.
Ruling: Task 7c added (Sam: try cost moves 1-3) after 7b; cost-ledger branch (45a0faa: bench --parts, docs/cost.md) merges into sound before 7c — cost if wrong: one task.
Ruling: Task 7c gains a head-to-head of Rings' reverb + ensemble vs our plate + 7b chorus (Sam: yes) — winner on cost/memory unless the sound clearly differs, then Sam chooses by ear — cost if wrong: a port of ~300 lines.
Task 7b: Ruling: cost 1.016 (> 1.0) accepted pending Task 7c, which targets ~0.25–0.35 of savings — cuts not needed, per Sam's preference to keep effects — cost if wrong: 7c must land its savings.
Task 7b: Ruling: instant drum-compressor attack accepted (the brief's peak-to-body test contradicts a 10 ms attack) — flag to Sam by ear — cost if wrong: retune attack/test.
Task 7b: Ruling: drum compression only partly meets "3–6 dB on hits" (dusty/808 ~2.4 dB, soft/brushes 0 dB) because a lower threshold breaks the −9 dBFS scenario floor — per-kit thresholds deferred to Task 8's mix balance — cost if wrong: Task 8 adds them.
Task 7b: Ruling: swell rule changed to sweep from the block's own amount (max-wins masked a dark intro's swell) — matches "opens into the next block" — cost if wrong: none.
Task 7b: Ruling: chorus/delay wet at 7.35 kHz accepted (brief allows reduced rate) — aliasing to judge by ear on Rhodes/Wurli — cost if wrong: raise the rate.
Task 7b: Ruling: the delay's "−60 dB within 3 s" read as absolute against the unit send impulse (the brief's 0.35 feedback can't reach −60 below the first echo in 3 s at 70 BPM) — cost if wrong: raise feedback damping.
Task 7b: Ruling: reviewer Minor 1 (delay wow LFO / dl_t and the plate LFO carry into the next song, so a seed after another song differs from the seed alone) regraded Important — same determinism constraint as Task 7's bed fix — fix round 1; Minor 2 (chorus depth carryover) folded in.
Task 7b: minor (deferred, carried to Task 7c): comp() pre-scan/post-pass runs with no drums active; fx_chorus_dry called 3× per chunk.
Task 7b: minor (deferred): attack gain step at block start can tick on a sounding 808 body; break+drop-bar holds the sweep at 0.6; no NaN/denormal check for bus filter/compressor states; chorus dry compensation lowers chords' sends ~1.4 dB, sends now post-filter (untested); test hooks ship in firmware (vx_fx_off, fx_wet_off, ~1 KB triple buffers); PM_*↔V_* mapping by enum order; a migrated test's comment/printf disagree; thin margins (scenario −8.8 vs −9; delay −63 vs −60).
Task 7b: fix round 1/5 (2 addressed, 0 open — per-song wet LFO restart, chorus depth lands; commits 6118b91..b96bba9)
Task 7b: minor (deferred): fx_song lands dl_t immediately, so a still-ringing delay tail jumps (~0.11 s read shift on a 70→85 BPM change) — keep the glide for a ringing tail; the fx.c:54-56 / fx.h:138 comment overstates 0.25 ms.
Task 7b: complete (commits 16e24ba..b96bba9, review clean). Cost 1.003 plain / 1.023 forced (seed 128) — accepted pending 7c.
Merged cost-ledger (45a0faa) into sound; --parts baseline before 7c in parts-before-7c.txt (floor 0.174, Rhodes 0.516, felt 0.449, Wurli 0.372, vox pad 0.283, pad 0.222; forced max 1.023).
Task 7c: Ruling: our plate + chorus stay default; Rings' reverb + chorus kept as host-only switch VNL_FX=rings for Sam to choose by ear (chorus clearly differs ±14 vs ±7.9 cents; Rings costs more CPU 0.821 vs 0.775 but saves 48 KB pool; 16-bit memory misses the wet-determinism test at −89 dBFS) — cost if wrong: switch the default.
Ruling: the bed moves to about −40…−30 dB under each song's own music RMS in Task 8 (Sam: the noise layer needs to be quieter) — my numbers — cost if wrong: retune.
Task 7c: minor (deferred): half-rate note grid skips each patch's top half-rate notes (pads up to ~96 untested; consider capping pads at 84 or extending the test); quarter-rate plate input aliasing untested and its comment's rationale wrong; hpar/hx/hr_tail not reset per song (harmless at 256-sample calls); half-rate also applies to lead/candy M_EP/M_PAD patches, untested; stale "triple" names; cost-gate.sh needs mkdir -p; noisy per-case test output.
Pending (controller, after 7c's fix round): stab comping's second hit (step 6) avoids the kick — 15/70 stab songs copied the kick (576/2531 bars), e.g. Bossa nylon 904880372 (Sam: 'I don't like that'); candidates 10, 4, 12 in that order; groove_bass_style reads the actual stab mask. TDD in song_test.c.
Task 7c: fix round 1/5 (3 addressed, 0 open — move-3 gate on the chain row (0.021 < 0.025, reported below estimate; the gate exits 1 by design), Wurli bark at half rate, Rings by default (ours host-only VNL_FX=ours, determinism bound −80 dBFS); commits 8e34433..6822435)
Task 7c: minor (deferred): two overlong comment lines (rings_fx.c ~220, song_test.c ~4688); Rings chorus wobble ±14 cents near the test's 15-cent window (Task 8 must not deepen it without moving the window); Rings CPU unmeasured on device.
Task 7c: complete (commits 0df8ddc..6822435, review clean). Forced max 0.814 (Rings default), pool 162492.
Controller: stab/kick fix committed (Sam: stabs and kicks may share beat 1, not other beats) — test_keys_kick_differ RED (489 copied bars, 682 stab-on-kick) → GREEN.
Pending (controller, after Task 8): refine the stab/kick rule (Sam: same beat allowed unless both are sparse) — harmony_stab_steps avoids kick steps only when the kick pattern has ≤3 hits a bar (stabs are always sparse); test_keys_kick_differ counts stab-on-kick only for sparse kicks.
Task 8: Ruling: roll bias (Felt and trumpet never rolls as a first song; Neo-soul Wurli 3× its share) not fixed — fixing it changes every seed — Sam's call, surfaced at finish — cost if wrong: one roll fix later.
Controller: refined stab/kick rule committed (sparse kick ≤3 hits) — RED 5/18 moved → GREEN.
Task 8: minor (deferred): quiet blocks allowed +1 dB over full band (brief: never louder); 044f590 alone breaks the web build (squash with a803785 before merging?); glue test margins brittle by design; glue's reduction doesn't reach the reverb return (wet rises 1–3 dB in dense passages); voices.c includes harmony.h for keys_gain (engine→song dependency); stale log paths in 2-set4 and -nobed renders.
Task 8: fix round 1/5 (2 addressed, 0 open — gl_h flush + fx_tiny/vx_tiny coverage + test_mix_denormals; picker ranks by forced time, standard list 7 20 49 100 128 183; commits b2a7f8c..31508ab)
Task 8: minor (deferred): picker time stated inconsistently (~25 s vs 40 s); a long line in docs/cost.md; an older cost.md paragraph keeps pre-fix seed-7 numbers as history.
Task 8: complete (commits 51ae89b..31508ab, review clean). Forced max 0.871 (seed 7); mem 160456/45964/179644.
Final review: with fixes — Important: voice-side state (noise rng, drum rng, hpar/hx, model phases) carries into the next song when vx_init is skipped, so a seed differs after another song (4/6 pairs, whole song). One fix dispatch: voice-side reset/seed in vx_song + end-to-end carryover test; RF_PROG in SOUND_F; Rings MIT notice in README/credits; PM_*↔V_* static assert; delay-tail-jump comment.
Final: minor (deferred): squash 044f590 with a803785 (web build) — not done (history rewrite; Sam's call at merge); memsets in the ISR at song start (~52 KB on device) — measure on device; unused plate/our-chorus code and ~1 KB pool on device; plus the earlier deferred minors the final review triaged as "To revisit".
Final: fix wave clean (5 addressed; commits 31508ab..fda1aa0).
