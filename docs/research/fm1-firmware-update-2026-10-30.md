# FM-1 firmware update survey (2026-10-30)

What changed in the firmwares we track since the 10-24/10-26 surveys, and what's new. Read with
[fm1-reuse-survey-2026-10-26.md](fm1-reuse-survey-2026-10-26.md) (not repeated here) and
[fm1-licenses-2026-10-25.md](fm1-licenses-2026-10-25.md). The full list of 277 FM-1 repos we had not tracked is in
[new-fm1-firmwares-2026-10-30.tsv](new-fm1-firmwares-2026-10-30.tsv) (source: thefirmwarehub.com catalog, GitHub search,
fm1-base dependents, 2bbb/fm1-firmware-collection).

All 40 `ref/` clones were fetched; 23 had changes. fm1-dub, tuna and frodhes were history-rewritten upstream, so their
working trees are still old: read the new code with `git -C ref/X show FETCH_HEAD:path`. `ref/mutable-dreams` is a
clone of our own repo, not kidbee85-beep/mutable-dreams; re-clone it if needed.

License reminder: everything below is GPL-3.0-only unless marked. Copying it makes the shipped image GPL-3.0-only.
Haunted FM-1 (MIT) and lunar-modulator (MIT) avoid that.

## Ranked: what to act on

| # | What | Source | VNL-1 need | Cost | Verdict |
|---|---|---|---|---|---|
| 1 | Bump fm1-base 865bc2f → d8ff8f4 | fm1-base | Free CPU (memset/memcpy 7–9x, our mix-buffer clears named), accurate `plat_cpu_pct` (read up to 6 pts low), 360 MHz forced at boot, `plat_ticks24` timer, `plat_cpu2_*` (absent at our pin) | Fast-forward, no API breaks for us | Do it: build, host tests, one device install |
| 2 | Font run-length packing | fm1-gm b2853d9 `tools/gen_font.py pack()`, `gfx.c cv_text` | Lead-styles plan Task 8: flash 16.2 KB over 409,600 B. Measured on our fonts: 44.1 → 17.8 KB | −26 KB flash, screen-only CPU | Reuse; lives in base/ (rewrite or propose upstream) |
| 3 | Load shedding (we have none: `APP_OVERLOAD` unused) | PERF-1 `ui.c` ~6143 (xruns/late → shed 0–3, 20 s calm to recover), `amb.c en_shed`; Tessera `audio.c:66`; lunar-modulator `devkit/voice_budget.h` (MIT) | Safety net for worst block (cost.md: up to 0.92) | ~50 lines, <100 B RAM | Adapt |
| 4 | Per-pass loop variation from a seed | Haunted FM-1 (MIT) `worn-tape/dsp/engine.c rebuild(age)` 146; DUB-1 `amb.c ph_thin` 2791, `phrase_plan` 2808, `ph_bar` 2824 (A A A B, per-phrase velocity/timing); Tessera `vary.c`, `step_evolve.c` (one change byte per note: rest, ±step, octave, ghost, repeat) | Variation speed (Not yet), Fills (Partial), lead loop toggle + stuck-note stutter, stay on the motif | Control rate, ~1–2 KB flash | Adapt (Haunted first: MIT) |
| 5 | Drift: slow random walk with smoothstep glides | DUB-1 `amb.c dr_rand` 3766, `dr_at` 3771, `lfo_tick` 3788 | Automation arcs (Partial): dust LPF, wow depth, bed level, delay send | ~25 lines | Reuse |
| 6 | Darker intro: filter opens across intro, staggered fades, bed first | DUB-1 `song_start` 2213, `song_tick` 2297; Haunted `midnight-route` `SHAPE_CURVE[]` 39, `route_rebuild` 278 | Darker intro filter (Partly), bed alone (Not yet), song maps | Negligible | Adapt; entries stay on 4/8-bar lines |
| 7 | Drum pocket and density | Cut-A-Dor `cad.c cad_step_tick` ~89 (lofi: snare +6/96 PPQN, kick +2, offbeat hats +4), two-bar kick "conversations"; PERF-1 `en_lane_mask` ~2120 (thin/fill by step importance), `GHOST_SN/KK` 2926, `COMP_MASK` 2919 | Boom-bap feel, energy moves, vary-not-avoid | Trivial | Adapt |
| 8 | Device CPU meter per stage | DUB-1 `main_fm1.c` ~28–65, `EN_PROF` 713, `host/en_prof.c` (p99/max, worst-block breakdown) | Plan 2 device bench; track-cost rule | ~1 KB, a few µs/block | Adapt with `plat_ticks24` |
| 9 | Echo that dissolves (allpasses in the feedback, 350 Hz HP on send) | DUB-1 `eapf` 3447, `ec_fb` 3462 | Dustier tape-delay repeats | 5 allpasses/sample, ~2 KB RAM | Adapt, A/B soloed on a dense song |
| 10 | 12-bit drum grit | zp12 `sp_core.c sp_channel` 228 (12-bit, no-interp rate pitching, 2044-style LPF on decay) | Drum-bus character option | Integer, near zero | Adapt, A/B in songs |
| 11 | Realistic tape movement (drifting noise speed + capstan + pinch roller) | frodhes `dly_tape.c tp_mod` | Our wow/flutter are fixed sines | ~2 one-poles/sample | Idea, A/B |
| 12 | 64-bit-multiply build check | demake `tools/fm1/build.py mul64_check` 224 | Guard the CPU fault (0 hits in `vnl.dis` today) | Build only | Adapt (~10 lines) |
| 13 | Render click detector | frodhes `tools/control_sweep.py` | Clicks at entries, drops, stutter | Tool only | Adapt logic |

## Ideas only / low priority

- PERF-1 lo-fi chord graph with phrase cadence (`CHT[]`, `chord_change` 1694–1800). Diatonic only, but check against
  "no substitutions" before borrowing anything.
- Tessera section develop / GRAVITY (`tess_form_develop` 588): a returning section exact or with one change, never
  saved back.
- Tessera unequal loops {5,7,11,13} + soft echo note (`answer.c`) for the Ambient lead.
- frodhes vibes motor as tremolo (`bars.c` 491; lesson: motor must not change decay). For "Vibes as comping".
- DUB-1 tuned-comb crackle bed (`noise_step` 5650): possible "tuned vinyl" bed; its own user found it too loud.
- DUB-1 slow mixer rides (`mixer_tick` 2570): keep rides, put drop-out moves on the grid; each must pass audible-or-cut.
- HERA's 48 comp phrases: captured from Roland J-6 presets; rhythm ideas only, don't copy the tables.
- `vnl_memmove` (`app/app.c:37`) copies byte by byte; word-at-a-time if it's on the audio path.

## Skip

DUB-1 width (we're mono), set transitions (we want silence between songs); frodhes physical EP/vibes (3+ ms of a
5.8 ms block); Mini-Tone TR-808 (64-bit products); Clipper EP patches (cross-check only); Daurade/Sill/Jam
Playground/Tidepool (VNL-1 already does more). Tuna, AMB-1, HYP-1/LOF-1, MAKU: nothing new musically.

## Housekeeping found

- `README.md:38` says hold OCT- and OCT+ for update mode; after the base bump it's REC held at power-on. Recheck the
  MANUAL recovery section too.
- Second core: if ever tried, set `cpu2_fallback = true` (default restarts, then safe mode, on units whose CPU1
  misreads RAM). The 10-26 survey's "fm1-base already offers `plat_cpu2_*`" is true only after the bump.
- GitHub still reports pianomansam/fm1-vnl as MPL-2.0 (last push 10-26); local is GPL since `mpl-final`.
