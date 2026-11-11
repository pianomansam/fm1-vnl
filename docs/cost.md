# CPU cost ledger

What the song engine costs to run, per task and per component, measured on the host at commit 16e24ba; Task 7c's
moves are re-measured against 0df8ddc in [Task 7c: moves 1 to 3, measured](#task-7c-moves-1-to-3-measured), and Task
8's mix in [Task 8: the mix, measured](#task-8-the-mix-measured).

## What the number means

Every figure is a **ratio**: the host time to render 60 s of audio, divided by the host time for 60 s of AMB-1's engine
at its heaviest piece (`ref/fm1-amb`, best of its eight pieces). AMB-1 runs on the FM-1 today, so a ratio of 1.0 is the
budget; below it, the song engine is cheaper than something already known to fit.

- **Method:** `-O2`, clang on an Apple-silicon Mac, best of 3 runs for the history and best of 7 for components.
- **Noise:** two full `--parts` runs agreed to within 1 ms per row (about 0.007 of a ratio). The ratios also move
  because the AMB-1 denominator itself drifted between runs (135.5 to 142.6 ms, about 5%), which moves a ratio near 0.85
  by about 0.03. Treat differences under 0.03 in the history table, and under 0.01 in the component table, as noise.
- **Device caveat:** this is a host proxy. The FM-1's core, its FPU, its flash/cache and its memory bus all change the
  proportions (float math, table lookups and big-buffer access scale differently). **The FM-1 bench replaces this
  ledger**; use it for the shape of the costs, not as a prediction.
- **Reproduce:** `tools/cost-bench.sh --forced 20 100 128 183` for the history rows (the standard list from Task 8's
  fix round was `--forced 7 20 49 100 128 183`; from the synths plan's Task 5 `--forced 22 20 7 138 100 200`; since
  Sam's synth cut it is `--forced 20 7 155 138 100 22`, with the picker `tools/cost-bench.sh` ranking 200 seeds by
  forced time), and the same bench built with
  `--parts` for the components (build line as in `tools/cost-bench.sh`, then run `app/build/bench --parts`).

## Cost per task (seeds 20, 100, 128, 183)

Ratio per seed, then the mean ratio and mean song time. The bench is built from the current `bench.c` (the 16e24ba copy)
in each older checkout, with these adaptations: before Task 7 there is no `vx_song`, so `vx_configure` is called in its
place (7e5978e to c852f6a), and at bc2d04a there is no config at all (a no-op). **`--forced` forces the lo-fi fields in
the roll, which only means something from Task 7 on; before it, the plain and forced rows are the same measurement**
(they agree within noise), so one row is shown. At 16e24ba both are shown.

| Task | Commit | s20 | s100 | s128 | s183 | mean ratio | mean ms | delta ratio |
|---|---|---|---|---|---|---|---|---|
| 1 placeholder voices | bc2d04a | 0.685 | 0.659 | 0.682 | 0.655 | 0.670 | 91.0 | |
| 2 patches (Rhodes, sub, vibes, music box) | 7e5978e | 0.693 | 0.672 | 0.687 | 0.646 | 0.675 | 92.9 | +0.005 |
| 3 keys and pads | 1a60a0d | 0.656 | 0.628 | 0.769 | 0.733 | 0.697 | 96.6 | +0.022 |
| 4 plucks and basses | 0af0024 | 0.706 | 0.666 | 0.812 | 0.739 | 0.731 | 102.4 | +0.034 |
| 5 bells and winds | f68a63a | 0.684 | 0.647 | 0.802 | 0.720 | 0.713 | 98.9 | -0.018 |
| 6 drums | c852f6a | 0.690 | 0.656 | 0.807 | 0.750 | 0.726 | 100.6 | +0.013 |
| 7 lo-fi chain and plate | 16e24ba | 0.799 | 0.762 | 0.910 | 0.838 | 0.827 | 117.9 | +0.101 |
| 7, chain forced (dust 3, vinyl, rain) | 16e24ba | 0.814 | 0.781 | 0.918 | 0.850 | 0.841 | 119.9 | +0.115 |
| 7b wet and dynamics (re-measured with 7c) | 0df8ddc | 0.893 | 0.858 | 1.002 | 0.935 | 0.922 | 128.2 | +0.095 |
| 7b, chain forced | 0df8ddc | 0.905 | 0.872 | 1.014 | 0.948 | 0.935 | 130.0 | |
| 7c half-rate keys and pads, dust fast path, quarter-rate plate | 7c | 0.725 | 0.670 | 0.757 | 0.680 | 0.708 | 99.5 | -0.214 |
| 7c, chain forced | 7c | 0.748 | 0.687 | 0.766 | 0.680 | 0.720 | 101.2 | -0.215 |
| 7c with Rings' reverb and chorus (Sam's pick, the default), forced | 7c fix | 0.787 | 0.734 | 0.814 | 0.731 | 0.767 | | +0.047 |
| 8 the mix (mud high-pass, glue, 8 voices for a held pad) | 7ac4bdb | 0.765 | 0.730 | 0.804 | 0.723 | 0.756 | 113.5 | |
| 8, chain forced | 7ac4bdb | 0.797 | 0.746 | 0.815 | 0.743 | 0.775 | 116.4 | +0.008 |

Reading it:

- Only Task 7 is a clear step (+0.10 plain, +0.115 forced). Tasks 2 to 6 are each within about 0.03 of the last: the
  placeholder FM voices of Task 1 were already close to what the real patches cost, and what a song costs is set mostly
  by how many voices its seed keeps busy (seeds 128 and 183 are the busy ones, 0.07 to 0.2 above seeds 20 and 100 from
  Task 3 on).
- The forced chain is only +0.014 over the plain roll at 16e24ba: the rolled lo-fi settings are already close to the
  heaviest one, so the "heaviest chain" upper bound is a small step above a typical song.
- The heaviest seed today is 128 at 0.918 forced: inside the budget, with 0.08 of headroom (about 9%).

## Cost per component (60 s alone, 16e24ba)

Each row renders 60 s at 80 BPM through the public engine API with the chain **dry** (`vx_init` only: no dust, no bed,
the plate shut). Rows are net of the **idle dry engine**, which is itself 20.4 ms = **0.150** (the floor below); a
real song pays that floor once.

- Chord pool: 4 voices holding a 4-note chord a bar, re-struck each bar.
- Lead: 2 voices, 8ths held a dotted 8th, so both stay busy. Candy: the same on 2 voices. Bass: 1 voice, legato 8ths.
- Drums: a boom-bap pattern on all six lanes.
- The chain rows run on a fixed source (Rhodes chord, sub bass, dusty kit; the source dry is 0.634 net of the floor).

### Instruments, drums and floors, sorted by cost

| Component | Role | Ratio (net of floor) |
|---|---|---|
| Rhodes (M_EP) | 4 chord voices | 0.477 |
| Felt piano (M_EP) | 4 chord voices | 0.426 |
| Wurli (M_EP) | 4 chord voices | 0.355 |
| Vox pad (M_PAD, formants) | 4 chord voices | 0.289 |
| Pad (M_PAD) | 4 chord voices | 0.243 |
| Muted trumpet (M_WIND) | 2 lead voices | 0.186 |
| Vibes (M_BELL, tremolo) | 2 lead voices | 0.173 |
| Sax (M_WIND) | 2 lead voices | 0.157 |
| Flute (M_WIND) | 2 lead voices | 0.151 |
| Nylon (M_PLUCK) | 4 chord voices | 0.148 |
| Bells (M_BELL) | 2 candy voices | 0.130 |
| Music box (M_BELL) | 2 candy voices | 0.105 |
| Kalimba (M_BELL) | 2 lead voices | 0.105 |
| Elec bass (M_BASS) | 1 bass voice | 0.085 |
| Sub (M_BASS) | 1 bass voice | 0.060 |
| Guitar lick (M_PLUCK) | 2 lead voices | 0.057 |
| Upright (M_PLUCK) | 1 bass voice | 0.055 |
| Drums, 808 | all six lanes | 0.049 |
| Drums, dusty | all six lanes | 0.046 |
| Drums, soft | all six lanes | 0.043 |
| Drums, brushes | all six lanes | 0.047 |
| Song engine ticking (no voices) | floor | 0.001 |
| Idle dry engine, 60 s of silence | floor (subtracted above) | 0.150 |

(Two runs differed by up to 0.015 on the nylon row and 0.007 elsewhere; the value shown is the mean of the two runs.)

### The lo-fi chain, on the fixed source

| Block | Ratio |
|---|---|
| Plate with a send (dust 0, nothing else) | 0.075 |
| Dust 1, over the plate | 0.019 |
| Dust 2, over the plate | 0.029 |
| Dust 3, over the plate | 0.029 |
| Vinyl bed, over the plate | 0.020 |
| Rain bed, over the plate | 0.019 |
| Tape bed, over the plate | 0.003 |
| The full chain (dust 3, vinyl, rain, plate) | 0.122 |
| Kick ducking | not separable: it runs in `dust_run` at every level, one multiply per sample |

The dust, vinyl, rain and tape rows are at or under the noise (0.01 to 0.02); read them as "small", not as an order.
The plate is the one measurable block: 0.075. A typical song (an instrument per role, drums, the chain) is the sum of
these plus the floor, which lands where the history table says (0.83 to 0.92).

## Task 7c: moves 1 to 3, measured

Moves 1 to 3 of the shortlist below, built and measured. Before is 0df8ddc (Task 7b), after is Task 7c, both timed in
the same session with the same bench (`VNL_RUNS=11` for `--parts`, 5 for the songs; the AMB-1 denominator was 137.9
and 137.3 ms for the two `--parts` runs, 139.0 and 140.5 ms for the songs). The host was busy: two identical runs of
an unchanged build differed by up to 0.05 on the Rhodes row, so read differences under 0.03 as noise.

### What changed

- **Half-rate keys and pads (moves 1 and 2).** A key (M_EP) or pad (M_PAD) note renders at 22.05 kHz, on even
  samples, when its highest partial is under 9 kHz: the FM's sidebands (the index plus three) and the Rhodes tine
  (14x) for the keys, the upper saw's second harmonic for the pads. Higher notes stay at the full rate (the Rhodes from
  about MIDI 76, the Wurli from about 79, the felt piano and the pads only above MIDI 96). Each role's half-rate voices
  are summed and upsampled by a 27-tap half-band (7 coefficient pairs, flat within 0.01 dB to 8 kHz, -60 dB from 14
  kHz; 14 samples, 0.3 ms, of latency). The envelope and every coefficient are rescaled to the step of two samples;
  the half-rate pads swap their bilinear SVF for matched biquads (Vicanek 2016), since at 22 kHz the bilinear filter
  loses 2 to 3 dB at 4 to 8 kHz. The bass is never half-rate; nylon (Karplus-Strong) stays at the full rate.
- **Skipping idle work (from Task 7b's review, and move 3's "skipping idle").** The voice loop runs over a per-segment
  mask of the voices sounding (skipped when none is), the drums are not called when no drum sounds, the drums'
  compressor skips its scan and gain when the drums' bus is silent and the compressor at rest, and `fx_chorus_dry()`
  is read once per chunk. All bit-exact: with these and the half-rate path in, but before the fx changes below, a 40 s
  render of seeds 20, 128 and 183 (plain and forced) with the keys held at the full rate (`VX_OFF_HALF`) hashed the
  same as 0df8ddc.
- **The dust's fast path (move 3).** With the wow off (dust 0), the wow's line is not written; the saturation runs in
  its expanded form, y - K y^3 (the same curve, to a rounding). Grit was already skipped at 0. The chain's output
  matches the full path to -144 dB, through the wow coming on and going off.
- **A quarter-rate plate (move 3).** The plate moved from 14.7 to 11.025 kHz (lengths and filters rescaled in Hz). The
  chorus and the delay stay at 7.35 kHz, so the wet loop is now a 12-sample cycle: the inputs summed per pair, the
  plate every fourth sample, the chorus and the delay every sixth, each output interpolated with running steps. The
  tail stays within 0.9 dB of the third-rate plate per octave band below 3 kHz. The ring stays 16384 floats: the lines
  sum to 8.5 k at the new rate, just over 8192.

### Per component, before and after

Rows are best of 11. "Total" is the whole render over the AMB-1 time; "net" subtracts the idle floor, as the tables
above do. The idle floor dropped because the engine now skips work when nothing sounds, so the net rows shift up for
everything: read the totals.

| Component | Total before | Total after | Saved | Net before | Net after |
|---|---|---|---|---|---|
| Idle dry engine (60 s of silence) | 0.171 | 0.093 | 0.078 | | |
| Rhodes, 4 chord voices | 0.675 | 0.371 | 0.304 | 0.504 | 0.278 |
| Felt piano | 0.621 | 0.353 | 0.268 | 0.449 | 0.260 |
| Wurli | 0.558 | 0.318 | 0.240 | 0.387 | 0.226 |
| Vox pad | 0.457 | 0.276 | 0.181 | 0.285 | 0.184 |
| Pad | 0.402 | 0.261 | 0.141 | 0.231 | 0.168 |
| Nylon (full rate) | 0.326 | 0.302 | 0.024 | 0.154 | 0.209 |
| Leads, candy, basses (each) | 0.24 to 0.38 | 0.21 to 0.34 | 0.02 to 0.04 | 0.07 to 0.21 | 0.12 to 0.24 |
| Drums (each kit) | 0.23 | 0.20 to 0.22 | 0.02 to 0.04 | 0.05 to 0.06 | 0.11 to 0.13 |
| Source dry (Rhodes, sub, dusty kit) | 0.827 | 0.609 | 0.218 | 0.656 | 0.516 |
| Plate + dust 0, over the source | | | | 0.105 | 0.086 |
| Full chain (dust 3, vinyl, rain), over the source | | | | 0.148 | 0.130 |

The lo-fi chain alone (dust 0, the plate open, the chorus at 0.7, steady input on every wet input, 60 s, best of 15)
went from 28.5 to 24.3 ms (about 0.03): 26.8 ms with the dust's fast path and the quarter-rate plate, its outputs
still interpolated with a multiply each (the 12-sample cycle needs a second interpolation, which ate most of the
plate's saving), then 24.3 ms with the interpolation as running steps. Dust 1 and 3: 30.9 to 26.4 and 32.2 to 27.8 ms.

**Move 3 on its own, measured.** The chain alone is also a `--parts` row now ("chain alone (dust 0)"; the same
setup, best of 11): 28.4 ms at 0df8ddc, 25.6 ms with our plate after move 3, a saving of **0.021** of a ratio. That
is under the shortlist's 0.05 to 0.1 for move 3, and under the brief's gate (half of 0.05, 0.025): move 3 delivered
about 0.02, not 0.05. (The idle floor's fall, 0.171 to 0.093, is mostly not move 3: it came with moves 1 and 2, from
skipping the voices and the drums while nothing sounds, and is real only in silence.) With Rings' reverb and chorus,
now the default, the chain alone is 29.8 ms, 0.010 above 0df8ddc's: the reverb's choice gives back move 3's saving.

### The forced maximum, before and after

| Seed | Before (0df8ddc) | After (7c) | Forced before | Forced after |
|---|---|---|---|---|
| 20 | 0.893 | 0.725 | 0.905 | 0.748 |
| 100 | 0.858 | 0.670 | 0.872 | 0.687 |
| 128 | 1.002 | 0.757 | **1.014** | **0.766** |
| 183 | 0.935 | 0.680 | 0.948 | 0.680 |
| heaviest of 200 (picker) | 128 at 1.023 (the Task 7b run) | | | 128 at 0.775 |

The heaviest seed is back inside the budget with about 0.23 of headroom, against the shortlist's estimate of 0.3 to
0.35 for moves 1 and 3: the estimate assumed a song's chords are all Rhodes; seed 128 is not. With Rings' reverb and
chorus (the default since the head-to-head below) seed 128 forced is 0.814 (our pair, same run: 0.785): about 0.19 of
headroom.

### The gate

`tools/cost-gate.sh BEFORE.txt [AFTER.txt]` checks the brief's measured gate, half the shortlist's lower estimates:
moves 1 and 2 on the Rhodes, felt piano and Wurli rows (0.10 each) and the pad and vox pad (0.05); move 3 on the chain
alone (0.025). The idle floor is printed, not gated. Both sides are divided by the before run's AMB-1 time (it drifts
up to 10% between runs on a busy host). Against 0df8ddc, with our plate (`VNL_FX=ours`): Rhodes -0.211, felt -0.176,
Wurli -0.147, pad -0.064, vox pad -0.107: ok; **the chain alone -0.021: below the estimate** (the gate reports it so,
and exits 1). With Rings' pair, the default: the chain alone +0.010. The pad's margin is smaller than its total
suggests (its total dropped 0.141): the net rows subtract a floor that fell with the silence skip, which raises every
net row, so the floor's shift shrinks the pad's net saving, not the other way round.

### Head-to-head: Rings' reverb and chorus against our plate and chorus

Rings' reverb and chorus (MIT, Emilie Gillet; `app/engine/rings_fx.c`) ported to C99 with their 16-bit delay memory,
their lengths and LFO rates rescaled from Rings' 48 kHz (the rate its reverb's LFOs are written for; this ref's
`dsp.h` says 44.1 kHz because Mutable Dreams runs it unscaled) to the slots they replace: the reverb at the plate's
11.025 kHz (x 0.2297), the chorus at our chorus's 7.35 kHz (x 0.1531). Rings' chorus, not its ensemble: the ensemble's
taps give L = R from a mono input, and the chords' bus is mono. The reverb's settings are tuned to the plate: Rings'
diffusion (0.625) and FX_REVERB damping (about 2.7 kHz), a reverb time of 0.485 for the plate's -60 dB time, an input
gain of 4 (our sends peak near 0.05; Rings' 0.2 is for full-scale input) and the return matched to the plate's tail
energy. The chorus maps the chords patch's chorus p to Rings' amount p and depth 0.15 + 0.5 p, its dry to 1 - p/2.
Sam's pick by ear (the no-bed renders): **Rings' pair is the default on every build** ("ours sounds more like an
echo"); ours stays on the host only, `VNL_FX=ours` (the app, the bench), for comparison, and the device and web
images leave the plate's ring out.

| | Ours (plate, chorus) | Rings (reverb, chorus) |
|---|---|---|
| Plate + dust 0 over the source (Rhodes, chorus 0.7), net | 0.091 | 0.122 |
| Full chain over the source, net | 0.133 | 0.164 |
| The chain alone, 60 s: reverb only / with the chorus at 0.7 | 25.2 / 26.3 ms | 28.6 / 31.0 ms |
| Forced, seeds 20 / 100 / 128 / 183 | 0.750 / 0.698 / 0.775 / 0.693 | 0.787 / 0.734 / 0.821 / 0.731 |
| Memory (pool) | 64 KB plate ring + 1 KB chorus line | 16 KB reverb (8192 x 16 bit) + 1 KB chorus line; the device's pool 210616 -> 162492 B |
| Impulse below -60 dB within 4 s; no denormals; 60 s of random input | pass | pass |
| -60 dB time; tail energy 0.1 to 1 s | 1.71 s; reference | 1.66 s; -0.06 dB |
| Tail spectrum and stereo | Dattorro's output taps; L/R correlation -0.02 | the two loops' outputs; 0.01, more early energy (+0.9 dB in the first 0.1 s) |
| Chorus on a Rhodes chord at 0.7: L/R correlation; wobble | 0.930; +-7.9 cents at 0.53 Hz | 0.854; +-14.0 cents at 0.83 Hz |
| A seed's wet after another song, against alone (Task 7b's test) | 3e-8 | 3.5e-5 (-89 dBFS): 16-bit rounding carries the last song's tail; the test's bound is now -80 dBFS (inaudible, under the bed) |
| Renders (seeds 904880372, 4185943442, 3228541874): RMS; peak; side/mid | -23.1, -30.3, -19.6 dBFS; -7.8, -8.2, -7.3; -23.6, -15.1, -25.4 dB | within 0.1 dB; 0.1 dB; 0.4 dB |

Reading it: ours is cheaper in CPU (Rings' 16-bit reads and writes, a float-int conversion and a clip each, cost more
than they save on the host; its chorus reads four taps to our two), Rings' is cheaper in memory (48 KB less pool).
The sound differs clearly in the chorus (Rings' wobbles about twice as far and spreads wider) and in kind in the
reverb (no output taps, its long delays modulated: a shimmer), at the same level and decay. Sam chose Rings' by ear,
so it is the default: it costs about 0.03 to 0.05 per song on the host (seed 128 forced 0.814 against 0.785), still
under the budget, and should be measured on the device, where float-int conversions may be cheaper than here.

## Task 8: the mix, measured

Same bench, `--parts` best of 7 and the songs best of 3, on a busy host (AMB-1's time 144-153 ms across these runs).

| Piece | Cost | Notes |
|---|---|---|
| The mud high-pass (a Butterworth per role and rate, on the source: a Rhodes chord, sub, dusty kit) | 0.011 | `mix mud high-pass` row: the source with it against `VX_OFF_MUD`. It runs only for a role with voices at that rate, or while its states ring: the first version, running all six filters every sample, cost 0.051 and lifted the idle floor by about 0.1 |
| The glue compressor (over the plate + dust 0) | 0.00 (-0.002, noise) | a mean square per sample, a log and an exp per 16 samples; its gain rides the duck's |
| A held pad's 8 voices (2 s release) | pad row 0.167 -> 0.245 | the bench's pad re-strikes a chord every bar, so the old chord's 4 voices release under the new one's for 2 s: about twice the voices. Only held pad notes (half a second or longer) take the other 4 voices; a pad arpeggio stays on 4 (one at 8 cost 0.92 forced at seed 49) |
| Per-kit drum levels, compressor thresholds, role gains, the keys per comping, the bed's level | 0 | constants |

**The forced maximum:** seeds 20 / 100 / 128 / 183 forced 0.797 / 0.746 / 0.815 / 0.743 (7c's fix: 0.787 / 0.734 /
0.814 / 0.731, within noise). A held-pad song is now the heaviest kind: seed 7 (Pad and guitar, held, from bar 1)
measured 0.825-0.839 forced over three runs (one outlier of 1.33 plain, a host stall: it re-ran at 0.830). The picker
(a single noisy run per seed over 200) chose seed 108 at 0.631. Budget 1.0; headroom about 0.16.

**Fix round 1:** the picker now ranks the 200 seeds by forced time (best of 2, about 40 s) and the standard list is
`--forced 7 20 49 100 128 183`. The picker found seed 7: 0.843 plain, 0.845 forced. The list: 7 0.852 / **0.871**, 20
0.773 / 0.807, 49 0.742 / 0.770, 100 0.724 / 0.744, 128 0.805 / 0.814, 183 0.733 / 0.744 (plain / forced). Max
**0.871** (seed 7, a held pad); headroom about 0.13. Memory: image 160456 B, RAM 45964 B, pool 179644 B.

**Memory** (`tools/mem-check.sh`): image 160200 B, RAM 45964 B, pool 179644 B (was 157048 / 45040 / 162492): the pool
grew by the 4 new chord voices' Karplus-Strong lines (4 x 1072 floats), RAM by their voice structs and the
high-pass's states.

**The keys' duck under the lead** (the lead leads, by level): a follower per 256-sample chunk (the two lead voices'
gates, a one-pole: about 25 ms in, 400 ms out) and the keys' gain ramped across the chunk in place of the chorus's
stepped dry scale: one add per keys sample on each path (full rate, half rate, the filtered mix), a multiply-add per
half-rate sample of the lead and candy. Not separable on the host bench: `--forced 7 20 49 100 128 183` gave 0.848 /
0.785 / 0.758 / 0.745 / 0.817 / 0.742 (forced), max **0.848** (seed 7), against 0.843 before and seed 7 at HEAD
re-run beside it at 0.846 / 0.838: within the noise (about 0.007). The rebalanced patch levels and the guitar lick's
longer ring (its notes end at note-off as before) cost nothing measurable. Memory: image 161216 B (was 160712), RAM
45964 B, pool 179644 B: four floats of state.

## Synths Task 1: the chip voice (M_CHIP), measured

`--parts` best of 7 on a busy host (AMB-1's time 166.5 ms in this run against 141-142 ms for the songs, so every row
reads low against the Task 7c and 8 tables: compare a chip row with its role's rows from the same run, shown here).
The chip voice runs at the full rate (no half-rate path: its edges are the sound), a phase, a compare and, at an edge,
the one-sample band-limiting; the vibrato at the winds' control rate (32 samples); the blip's noise tick until it
falls under 1e-4.

| Component | Role | Ratio (net of floor) | Same run, for scale |
|---|---|---|---|
| Chip chords (pulse, duty 0.25) | 4 chord voices | 0.272 | Rhodes 0.258, pad 0.237, nylon 0.205 |
| Chip lead (pulse, duty 0.5, vibrato) | 2 lead voices | 0.207 | flute 0.207, vibes 0.230, guitar lick 0.130 |
| Chip bass (stepped triangle) | 1 bass voice | 0.097 | sub 0.113, upright 0.111 |
| Chip blip (pulse 0.125, noise tick) | 2 candy voices | 0.150 | music box 0.177, bells 0.195 |

The chip chords are the dearest chords row only because they are the one chord instrument always at the full rate
(the Rhodes and the pads render at half): the oscillator is cheap, the voice's envelope, its mud high-pass at the full
rate and the mix are what a full-rate voice pays. A chip set's forced cost is Task 5's to bench.

**The forced maximum** (at Task 1 no set used a chip voice, so this is the cost of widening `p[]` and the new
dispatch case): `--forced 7 20 49 100 128 183` gave 0.838 / 0.787 / 0.758 / 0.740 / 0.819 / 0.740 (forced), max **0.838**
(seed 7), against 0.848 before: within the noise. **Memory:** image 162848 B (was 161216: `p[]` 8 to 16 floats in 21
const patches, 672 B of flash, and the chip's code), RAM 45964 B, pool 179644 B: unchanged (the chip's state fits in
the voice union).

## Synths Task 2: the subtractive voice (M_SUB), measured

`--parts` best of 7 (AMB-1's time 138.1 ms in this run, the idle floor 0.098). One oscillator (a polyBLEP saw or
pulse, a sine or a triangle), a sub-octave square sharing the oscillator's polyBLEP, noise, then the low-pass: a TPT
SVF per sample, its envelope and coefficients at a control rate of 16 samples (a `fm_exp2f` and a `fm_tanf`, only
while the cutoff moves). On the chords a 2-pole patch whose highest cutoff stays at 8 kHz or under renders at half the
rate through a matched biquad (`bq_match`, as the pads; a bilinear SVF at half the rate measured 3 dB short in the
4-8 kHz band). The Moog bass's 4-pole (two SVFs, a zero-delay feedback solve) runs at the full rate on its one voice.

| Component | Role | Ratio (net of floor) | Same run, for scale |
|---|---|---|---|
| Juno (saw + sub, half rate) | 4 chord voices | 0.284 | Rhodes 0.292, pad 0.267, chip chords 0.231 |
| Organ (square + sub, no envelope, half rate) | 4 chord voices | 0.224 | vox pad 0.229, nylon 0.225 |
| Moog bass (saw + sub, 4-pole, full rate) | 1 bass voice | 0.231 | elec bass 0.163, sub 0.132 |
| Saw lead (mono, glide 90 ms) | 1 lead voice (legato) | 0.156 | flute 0.240, chip lead 0.158 |
| Square lead (mono, glide 60 ms, vibrato) | 1 lead voice (legato) | 0.166 | |
| Sine lead (mono, glide 70 ms, vibrato) | 1 lead voice (legato) | 0.161 | |
| Synth pluck (pulse, poly) | 2 lead voices | 0.249 | vibes 0.269, kalimba 0.205 |

The bench's lead part overlaps its 8ths, so a mono lead holds one voice, gliding, where the poly leads use two. The
Juno pays for its matched biquad's redesign while its filter envelope moves (about 1 s a chord); the organ's cutoff
never moves, so it is set once a note. The Moog bass is the dearest bass: the 4-pole at the full rate, its envelope
always moving under 8ths.

**The forced maximum** (at Task 2 no set used a subtractive voice; the sets came in Task 4, below): `--forced 7 20 49 100 128 183` gave
0.821 / 0.781 / 0.747 / 0.735 / 0.818 / 0.746 (forced), max **0.821** (seed 7), against 0.838 before: within the
noise. **Memory:** image 168156 B (was 162848: the seven patches and the voice's code), RAM 46172 B (was 45964: the
voice union grows to M_SUB's 128 B, 8 B a voice, and a slew flag), pool 179644 B (unchanged).

## Synths Task 4: the synth sets, measured

Six sets (ids 12-17) and the chip voices' bit crush (a multiply, a floor and a multiply a chip sample). Adding sets
redraws the set of many seeds: of the standard list only 7 keeps its set (Pad and guitar); 20 is now Organ and sine
lead, 49 DX keys and FM bass, 100 Juno and saw lead, 128 Bossa nylon, 183 Chillwave pad.

`--forced 7 20 49 100 128 183` gave 0.844 / 0.845 / 0.717 / 0.811 / 0.737 / 0.799 (forced), max **0.845** (seed 20,
the organ set). The picker (`tools/cost-bench.sh`, 200 seeds): the heaviest is now seed **22, Rhodes and Moog**, at
0.833 plain and **0.863** forced (was 0.822): a synth set is the heaviest roll, inside the budget with 0.14 of
headroom. `--parts` for the chips with the crush (one run): chords 0.263, lead 0.184, bass 0.113, blip 0.177, within
the noise of Task 1's rows. **Memory:** image 169004 B, RAM 46172 B, pool 179644 B.

## Synths Task 5: balance, the new standard list, every patch's row

**The levels** (test_lead_over_keys, each set's mean 4-6.5 dB and median 4-6): kalimba 1.69 to 1.614, saw lead 1.06 to
0.945, sine lead 0.82 to 0.899, square lead 0.61 to 0.70, muted trumpet 1.56 to 1.75, felt piano 0.668 to 0.716, synth
pluck 1.4 to 1.307. Levels cost nothing.

**The new standard list.** The synth sets redrew the old list's seeds (only 7 kept its set), so a forced sweep of seeds
1-200 (`--forced`, best of 3 each) picked the heaviest real roll overall and per family. Held comping is the heaviest
style in every family (the chord pool stays full):

| Seed | Set | Why on the list | Plain | Forced |
|---|---|---|---|---|
| 22 | Rhodes and Moog (held, rain) | the heaviest of the 200 | 0.832 | **0.863** |
| 20 | Organ and sine lead (held, dust 3) | the heaviest organ set | 0.828 | 0.845 |
| 7 | Pad and guitar (held pad) | the heaviest held pad | 0.830 | 0.833 |
| 138 | DX keys and FM bass (held, tape) | the heaviest DX set | 0.790 | 0.820 |
| 100 | Juno and saw lead (held, dust 3) | the heaviest Juno set | 0.788 | 0.804 |
| 200 | Chip lo-fi (held, tape and rain) | the heaviest chip set | 0.651 | 0.682 |

`--forced 22 20 7 138 100 200`: max **0.863** (seed 22), 0.14 of headroom. The lightest family is the nylon and
synth pluck (its heaviest, seed 80, 0.662 forced). The picker (no arguments, best of 2 per seed) is noise-limited at
this spread: across four runs it chose 22 (0.857 forced), 183 (0.782), 6 (0.618) and, under load, 90; the top ten of the sweep lie
within 0.05 of each other, so the list above, not the picker, is the bound.

**`--parts`, every synth patch** (best of 7, AMB-1 138.5 ms, idle floor 0.097; old rows in the same run for scale):

| Component | Role | Ratio | Same run |
|---|---|---|---|
| Organ | chords | 0.247 | nylon 0.247 |
| DX EP | chords | 0.332 | the dearest chords |
| Chip (triangle) bass | bass | 0.117 | sub 0.141 |
| FM bass | bass | 0.175 | |
| Saw lead | lead | 0.168 | flute 0.247 |
| Synth pluck | lead | 0.261 | vibes 0.272 |

(The rows of the patches Sam cut, below, are gone: the chip chords, lead and blip, the Juno, the Moog bass, the square
and sine leads, the FM bells.)

**Memory:** image 169148 B, RAM 46172 B, pool 179644 B (`tools/mem-check.sh`: ok).

## Sam's synth cut (2026-10-24), measured

Sam kept six synths (the DX EP, the FM slap bass, the triangle bass, the organ, the saw lead, the synth pluck) and the
sets became DX keys and FM bass, Organ and saw lead, Rhodes and saw lead, DX and pluck, Nylon and synth pluck. The cut
patches went, and the model code only they used: M_SUB's 4-pole, sine, triangle, noise and vibrato; M_CHIP's pulse,
noise, vibrato and tick (the triangle and the crush stay).

**The picker** (`tools/cost-bench.sh`, 200 seeds): seed **20, Rhodes and saw lead**, 0.847 plain, **0.866** forced. A
forced sweep of seeds 1-200 (one run each) kept five of Task 5's six seeds on their families; 200 (the chip set) is now
Rhodes neo-soul, so 155 (DX keys and FM bass, the heaviest of that set) takes its place:

| Seed | Set | Why on the list | Plain | Forced |
|---|---|---|---|---|
| 20 | Rhodes and saw lead | the heaviest of the 200 | 0.859 | **0.875** |
| 7 | Pad and guitar (held pad) | the heaviest held pad | 0.831 | 0.835 |
| 155 | DX keys and FM bass | the heaviest DX keys set | 0.783 | 0.808 |
| 138 | DX and pluck | the heaviest DX and pluck set | 0.759 | 0.783 |
| 100 | Organ and saw lead | the heaviest organ set | 0.681 | 0.699 |
| 22 | Nylon and synth pluck | the heaviest nylon and pluck set | 0.694 | 0.725 |

`--forced 20 7 155 138 100 22`: max **0.875** (seed 20), 0.125 of headroom.

**`--parts`, the kept synths** (best of 7, AMB-1 137.0 ms, idle floor 0.098): organ 0.211, DX EP 0.324 (the dearest
chords; Rhodes 0.292), triangle bass 0.107, FM bass 0.171, saw lead 0.151, synth pluck 0.242. (`--parts` had its role
counts hard-coded; it now counts its tables.)

**The levels** (test_lead_over_keys): the saw lead 0.945 to 1.12 (it leads three sets now), the DX EP 1.0 to 0.944,
the organ 0.27 to 0.248; each new set's mean 4.4-5.4 dB, median 4.9-5.4. DX and pluck plays the soft kit: on the 808
the Slow bloom map's kick-and-bass entry (seed 47) jumped 7 dB over the keys, snare and hats block before it.

**Memory:** image 166652 B (was 169148), RAM 46028 B (was 46172), pool 179644 B (`tools/mem-check.sh`: ok).

## The three biggest costs

1. **The FM electric-piano chords (Rhodes, felt piano and Wurli: 0.36 to 0.48, four voices).** Each voice runs, per
   sample, two `fm_sin_turns` (carrier and modulator), a third for the tine at 14x, the strike and bark envelopes
   (Wurli adds a noise burst), a tone one-pole and the envelope in `voice_sample`; four of them are held at once, and
   the chord pool is the only role that is full for the whole song.
2. **The pads (0.24 to 0.29, four voices).** Two polyBLEP saws plus a TPT state-variable filter per voice (two filters
   and the formant mix for the vox pad), per sample, and the pad voices hold the longest (attack 0.6 s, release 0.8 s),
   so all four are almost always sounding.
3. **The always-on chain floor (0.150 idle, plus 0.075 for the plate).** `dust_run` and the clip run on every sample
   at the full rate even at dust 0 (a delay-line write, the cubic saturation, the low-pass, the duck gain, then the
   soft clip), and the plate's Dattorro tank runs at a third rate with its LFO-modulated allpasses. They are paid
   whether or not anything sounds, which is why they rank above any one lead or bass.

## Paths to reduce CPU and memory

Numbers above are host ratios; savings below are estimates in the same units, from reading the code and the table, not
from a prototype. The balance, as the owner framed it: samples cost memory and cut CPU; synthesis is flexible and costs
CPU. Memory on the device (fm1-base): **581 KB app flash** with about 144 KB used today; **96 KB RAM** for `.data` and
`.bss`; **336 KB pool** (big zero-initialized buffers, where the KS lines and the plate already live); and the **flash
data area beyond the store** (`plat_data*`: `PLAT_DATA_LO..HI` 0x97000-0xE0000 = 292 KB, `PLAT_XDATA0` 16 KB and
`PLAT_XDATA1` 92 KB, about 400 KB), which is where samples would go: read in place, written only from the main loop
(a write stops the sound for a moment, so the bank is flashed at update time, not during a song).

### 1. Samples versus synthesis, per instrument

LOF-1 fits 18 instruments and a kit at 16 kHz, 4-bit IMA ADPCM in 318 KB (`docs/research/other-firmwares.md`): that
fits the data windows (about 400 KB) but not alongside much else. A sample voice costs one interpolated read plus an
ADPCM nibble decode per sample, plus the envelope: I estimate **0.25 to 0.4 of an FM-EP voice**, a smaller win than it
sounds because the envelope, the steal logic and the mix stay. Per-role view:

| Role | Now (ratio) | As samples | CPU | Memory | Flexibility lost |
|---|---|---|---|---|---|
| Drums (4 kits) | 0.043 to 0.049 | one-shots, 16 kHz ADPCM, about 20 to 40 KB per kit | about 0 saved (already cheap) | flash 80 to 160 KB | kit character is fixed (no per-hit pitch/decay); a real gain in realism, not CPU |
| Upright, guitar lick, nylon | 0.055 to 0.148 | multi-zone loops, 8 to 20 KB per instrument | 0.03 to 0.1 | flash | little: plucks are mostly one decay with no filter sweeps; velocity layers needed |
| Vibes, kalimba, music box, bells | 0.105 to 0.173 | one-shots with a tremolo done in the mixer | 0.05 | flash | the vibes' tremolo and the bells' key-tracked decay are baked; low loss |
| Rhodes, felt piano, Wurli | 0.36 to 0.48 | 3 to 4 velocity layers of 3 zones, about 15 KB each | 0.25 to 0.35 | flash 60 to 100 KB each | velocity-driven brightness becomes layer crossfades; no live tine/bark tweak |
| Pads, vox pad | 0.24 to 0.29 | looped, but filter sweeps and the formant blend are the instrument | 0.1 to 0.15 | flash | **high loss**: keep synthesized |
| Winds (flute, sax, muted trumpet) | 0.15 to 0.19 | note-length loops with vibrato baked | 0.05 to 0.1 | flash | delayed vibrato and breath are baked in; medium loss |
| Sub, electric bass | 0.06 to 0.085 | not worth it | about 0 | | keep synthesized |

Order of preference for samples: **the EP keys first (the biggest cost, a clear win), then the plucks and bells**.
Drums and the bass are cheap already, so sample them only for sound, not CPU.

### 2. Wavetables and lookup tables

- **FM-EP as precomputed single-cycle tables per velocity layer** (a table per index step, crossfaded as the index
  decays): replaces two `fm_sin_turns` per sample with two interpolated reads. Estimated saving **0.1 to 0.15 of the
  0.48**; the cost is memory (about 8 tables of 2048 samples at 16 bit per key zone, roughly 40 to 100 KB of flash for
  the three keys) and the live index decay becomes a crossfade (flexibility: a continuous velocity-to-index mapping is
  approximated by layers).
- **A shared sine table** in place of `fm_sin_pi`: the host proxy cannot say whether the device's polynomial is slower
  than a flash read (see device unknowns); measure before spending the 2 to 4 KB.
- **A saw table** (band-limited, per octave) in place of polyBLEP for the pads: saves the BLEP branch only, about 0.02.

### 3. Rate reduction, control rate, skipping idle work

- **Half-rate chord voices**: render the four chord voices at 22.05 kHz into a mono send and upsample with a short FIR
  (or a one-pole pair; the chord role is already sent through the tone one-pole at 1 to 3 kHz for EP). Saves about
  **0.2 to 0.24 of the Rhodes 0.48** (half the per-voice work, less the upsample), the biggest single saving without
  new data; flexibility lost: the tine's top partial (14x the note) and bark above 11 kHz are gone, which the lo-fi
  5 kHz dust low-pass would remove anyway at dust 0 and above.
- **The same for pads** (their low-pass already sits at 0.9 to 1.5 kHz plus key tracking): saves about **0.1 to 0.14**.
- **Dust fast path**: `dust_run` always does the delay write, saturation, low-pass and gain. At dust 0 the saturation
  and the wow read could be skipped (the code already skips the read, not the write or the cubic). The idle floor
  0.150 could drop to about 0.08 to 0.1 at dust 0, and the plate to 0.05 at a quarter rate. Saving **0.05 to 0.1**, no
  musical loss at dust 0 (skipped work is the saturation at 0.5 drive, near linear).
- **The plate and the bed already run at a third of the rate** (14.7 kHz); a quarter rate would save about 0.02.
- **Control rate**: voices update envelopes per sample (`voice_sample`: `nat`, `amp`, `g`). Moving the envelope and the
  gate to a 32-sample control rate (as the winds already do for vibrato) saves about 10% per voice (0.03 to 0.05 on
  chords), at the price of small steps unless the control values are interpolated.
- **Skipping idle**: `render_seg` already skips voices that are off; the idle dry floor (0.15) is the chain, not voices.
  A silent bar could skip `fx_process` entirely if the plate tail and bed are quiet (`fx_active`), but a song seldom
  has silence in its middle.

### 4. Fewer voices, smarter allocation

- **Chord pool 4 to 3**: saves a quarter of the chord role (0.06 to 0.12) but drops the fourth chord tone; most voicings
  here are 4 notes, so this is a real musical cost. Alternative: keep 4 but drop the **bass voice duplicate** (the chord
  voicing's lowest note is often the bass note): a voicing without the root saves a voice for free in some bars.
- **Steal sooner**: a voice's release is only 0.06 to 0.15 s, so voices free quickly; the cost is the held time, which is
  musical. No gain there.
- **Mono where stereo isn't heard**: the music bus is already mono until the plate (the send is mono); only the plate's
  return and the bed are stereo. Nothing to save.
- **Leads and candy are 2 voices each**: a mono lead (one voice) saves about 0.08 to 0.09 on the wind and vibes leads
  but loses overlapping tails (legato releases will cut). Candy (ear candy) at one voice saves about 0.05.

### 5. Fixed point versus float

The engine is float-only (`fastmath.h`, no fixed-point paths) and written for the single-precision FPU. If the FM-1's
core has a hardware FPU, fixed point wins nothing, and it loses headroom (the dust saturation, the SVF states and the
index decay all want dynamic range). If it does not, **float is far more expensive than this ledger shows and the
whole table scales up**: that is a device question, not a code one. Not recommended until the bench says otherwise.

### 6. Work off the audio path

The song engine itself is 0.001 (note events at 16th rate), so there is nothing to move there. What could move is
**note-start work**: `pluck_start` fills a Karplus-Strong line with a noise burst (up to 1072 samples) and `bell_start`,
`pad_start` and `wind_start` compute coefficients with `expf` and `exp2f`, all at a note's first sample. The 60 s
average above hides this: **the worst block, not the average, is what a device underruns on**, and a bar line (four
chord strikes, the bass, a kick) makes the worst block. A precomputed burst table, or a staged start (one voice per
block), would flatten it. Everything else (voices, dust, plate) is per-sample DSP that has to run in the audio
interrupt.

### 7. What only the FM-1 bench can tell

- **Float throughput and `fm_sin`/`expf` cost** (FPU or soft float): sets every FM and filter row.
- **Flash read speed and cache behaviour** for samples and tables: a sequential ADPCM read is cheap if the cache
  streams, but 4 to 8 voices reading different regions can thrash a small cache, which would eat the saving in 1; a
  table lookup in 2 may be slower than the polynomial it replaces.
- **Pool RAM access speed** for the KS lines (4.3 KB per voice, 9 voices) and the plate's ring buffer.
- **Worst-block cost**, which the host average cannot give (see 6).
- Each saving above is a host estimate; the ones that trade compute for memory access (1, 2) are the most likely to
  change on the device, and the rate reductions (3) the least.

### Ranked shortlist: the five best moves for this project

1. **Half-rate chord voices** (3): saves about 0.2 to 0.24, no new data, loses only content above 11 kHz that the lo-fi
   chain removes anyway.
2. **Half-rate pads** (3): about 0.1 to 0.14, same reasoning, the pad's filter sits low.
3. **Dust fast path at dust 0, 1 and a quarter-rate plate** (3): about 0.05 to 0.1 on the always-on floor, no musical
   loss.
4. **EP keys as velocity-layered samples or tables** (1, 2): about 0.25 to 0.35 for the Rhodes/felt/Wurli, but 60 to
   100 KB of flash each and some velocity flexibility; do it only if 1 to 3 do not land the headroom wanted.
5. **Mono lead and one candy voice, or a control-rate envelope** (3, 4): about 0.05 to 0.09, with a small musical cost
   (tails cut or small steps): the last resort among the cheap moves.

The remaining headroom today is about 0.08 at the heaviest seed with the heaviest chain (0.918); moves 1 and 3 together
would leave roughly 0.3 to 0.35 of headroom on the host proxy.

## FM patches (DX electric piano, FM bass, FM bells; synths Task 3)

New patches on the existing M_EP, M_BASS and M_BELL models: no new model code. `--parts` best of 7 (AMB-1 136.6 ms,
idle floor 0.099).

| Component | Role | Ratio (net of floor) | Same run, for scale |
|---|---|---|---|
| DX EP (M_EP, tine + tone one-pole) | 4 chord voices | 0.324 | Rhodes 0.294 |
| FM bass (M_BASS, slap index) | 1 bass voice | 0.172 | elec bass 0.160 |
| FM bells (M_BELL, ratio 3.5) | candy voices | 0.241 | bells 0.247 |

At Task 3 no set used them (sets came in Task 4). The forced maximum then: **0.822** on `--forced 7 20 49 100 128
183`. Memory: image 168444 B, RAM 46172 B.

**2026-10-24, the arpeggio cap and offbeat comping retired:** the forced maximum on the standard list
(`--forced 20 7 155 138 100 22`) is **0.902** (0.896 before; noise-level, held comping a little more often). Memory
unchanged (mem-check ok).

## Pads Task 1: the pad layer (M_ENS), measured

A new pitched role, V_PAD, on its own bus (the keys' section moves, the plate at 0.4, a send into the chorus at its
patch's depth, the mud high-pass at 150 Hz), with 4 voices (a 3-note chord and a solo note) of a new model, M_ENS: a
polyBLEP saw and a partner saw a few cents sharp and quieter, into a fixed 2-pole low-pass (two one-poles), the patch
envelope. The first cut, per voice through the generic voice loop at half the rate, measured **0.19-0.20**: the model
itself was about 0.05 of it, the generic loop's dispatch and envelope about 0.12, the half-band upsampler about 0.02.
So the layer renders in a loop of its own (`pad_seg`: the voices side by side, no dispatch, the envelope inline) at a
**quarter of the rate** (the design's cut (b)), its sum through the mud high-pass at that rate and back to the full
rate by linear interpolation (no upsampler). Its aliasing and images, against the full-rate reference (VX_OFF_HALF;
non-harmonic energy over 0-11 kHz, the partner off): 42 dB under the harmonics at G3, 37 dB at C5, 31 dB at C6 (the
full rate 47 dB), before the chorus and the lo-fi chain's low-pass.

`--parts` best of 7 (AMB-1 141.1 ms, idle floor 0.102). The bench's pad part: a 3-note chord and a solo note an
octave up, held a bar and re-struck each bar (the old chord's release stolen by the new). The "layer" rows are the
pad part over the fixed source (Rhodes chords, sub bass, dusty kit), net of the source alone:

| Component | Role | Ratio (net of floor) | Over the source, dry | Over the source, full chain |
|---|---|---|---|---|
| Pad layer (M_ENS), strings | 4 pad voices | **0.071** | 0.071 | 0.068 |
| Pad layer (M_ENS), warm pad | 4 pad voices | **0.070** | 0.073 | 0.069 |

Within the layer's target (0.08). **The forced maximum** (no song plays the layer yet: the pads plan's Task 2 writes
its events): `--forced 20 7 155 138 100 22` gave 0.903 / 0.850 / 0.813 / 0.786 / 0.747 / 0.735 (forced), max **0.903**
(seed 20), against 0.902 before: unchanged. With the layer in, the heaviest roll (seed 20, Rhodes and saw lead: the
warm pad) should land near 0.97. **Memory:** image 176296 B (was 166652), RAM 48960 B (was 46028: four voices and the
layer's segment buffer), pool 179644 B (unchanged: the layer's voices have no Karplus-Strong line)
(`tools/mem-check.sh`: ok).

**Fix round (pads Task 1 review):** the layer's notes now play on lanes (lanes 1-3 the chord, 4 the solo; a held
lane glides to its next note), a note whose key-tracked low-pass is above 4 kHz renders at half the rate (a matched
biquad, through the upsampler; none of the patches' notes up to C6 crosses it), and the pad's send into the chorus is
scaled to its own patch's depth, its dry scaled to match. The bench's pad part now plays lanes, each held into the next
bar and gliding there (the old part stole each chord's release instead). `--parts` best of 7 (AMB-1 143.1 ms, idle
floor 0.106):

| Component | Role | Ratio (net of floor) | Over the source, dry | Over the source, full chain |
|---|---|---|---|---|
| Pad layer (M_ENS), strings, lanes | 4 pad voices | **0.082** | 0.067 | 0.072 |
| Pad layer (M_ENS), warm pad, lanes | 4 pad voices | **0.080** | 0.068 | 0.079 |

At the 0.08 target (the alone rows a hair over, the over-source rows under). The glides cost about 0.01 against the
stolen notes. `--forced 20 7 155 138 100 22`, two runs (the AMB-1 denominator drifting 145.8 to 150.1 ms): max
**0.917** and **0.905** (seed 20); no song plays the layer yet, so the change from 0.903 is bench noise and the
per-segment check for pad voices.

## Pads Task 3: the layer in the songs, measured (about 0.97, accepted by Sam)

With Task 2's songs playing the layer (and its level at `ROLES[V_PAD]` 0.048, was 0.06: a gain only), on a busy host
(load about 5; AMB-1 138-153 ms across these runs). `--forced 20 7 155 138 100 22`: 0.969 / 0.911 / 0.893 / 0.846 /
0.768 / 0.795 plain, **0.987** / 0.897 / 0.914 / 0.872 / 0.785 / 0.829 forced; seed 20 (Rhodes and saw lead, the warm
pad) again over four more runs (`VNL_RUNS=5`): 0.976-0.999 forced. The picker (`tools/cost-bench.sh`, 200 seeds)
re-picks seed 20: 0.942 plain, **0.976** forced. Over the 0.97 line on every run.

**What the cuts could save** (seed 20 forced, `VNL_RUNS=5`, best VNL time of three runs each; a temporary engine hack
dropping the pad events above a lane, not committed): all 4 lanes 140.0 ms, lanes 1-3 138.4, lanes 1-2 135.9, lane 1 134.1,
none 128.9. So a lane costs about 1.5-2.5 ms (0.011-0.018) and the layer's fixed part (its
per-sample loop and interpolation, the pad bus's section filter, mud high-pass and sends) about 5 ms (0.037). Cut (a)
(the solo note on the ensemble voice) saves at most a lane (dropping lane 4 outright: 0.964-0.977); (b) is in place;
(c) (2 chord notes) at most one more: (a) and (c) together at most 2.5-4 ms, about 0.975 against AMB-1's 139-140 ms.
Not enough: the pads plan's rule (stop and report) applies; the layer's fixed part is where the rest is.
**Memory:** image 183228 B, RAM 49600 B, pool 180216 B (`tools/mem-check.sh`: ok).

**The controller's ruling, then the cuts (2026-10-24).** Measured on the same busy host (load about 5).
- *The fixed part:* the pad bus's section filters (and its mud high-pass, already) run on the quarter-rate sum before
  the interpolation; the quarter-rate path does nothing once silent; the quarter-rate voices render voice by voice.
  Only the voice-by-voice rendering is bit-exact. The rest changes the output slightly, by design:
  - the gain ramp per quarter-rate sample is computed as `g0 + gd*(i+1)`, not accumulated, so it rounds differently
    while the pad's chorus dry scale eases;
  - the bus filters run at the quarter rate when engaged, with their amounts read once per chunk (one chunk late)
    (test_pad_bus_filter: within 1.5 dB of the full-rate reference);
  - the idle cutoff moved from exactly 0 to 1e-7.
  A hash comparison (seeds 20 and 1013, 60 s) matched before and after the voice-by-voice step only. It didn't
  exercise the chorus-dry easing, and the full-rate filter path is only the reference. A profile (sample, voices.c and fx.c not inlined) showed the layer's cost is its voices (the polyBLEP
  saws and the envelope), not the bus: these saved about 2-3 ms. Seed 20 forced: 0.973-0.989.
- *Cut (a), the solo note on a chord lane's voice:* no measurable saving on seed 20 (the solo note plays 8 of the
  bench's 20 bars): 0.981-0.989.
- *Cut (c), the chord part 2 notes:* seed 20 forced, `VNL_RUNS=7`, five runs: **0.970 / 0.973 / 0.973 / 0.973 / 0.970**
  (VNL 135.8-136.4 ms, AMB-1 139.7-140.2). The standard list, two runs: 0.947 / 0.885 / 0.849 / 0.825 / 0.759 / 0.773
  plain, **1.001** / 0.885 / 0.878 / 0.855 / 0.783 / 0.804 forced, then 0.946 / ..., **0.961** / 0.887 / 0.874 / 0.853 /
  0.779 / 0.800 forced. The picker re-picks seed 20: 0.933 plain, **0.949** forced (AMB-1 143.1). On the line, not
  reliably under 0.97.
- `--parts` at the time of cut (a) (the bench's pad part still a 3-note chord, its solo note riding): pad strings
  0.054, warm pad 0.052. Superseded below.
- **Memory:** image 187508 B, RAM 50944 B, pool 180216 B (`tools/mem-check.sh`: ok).

**Sam's decision (2026-10-24):** keep cut (c), the 2-note chord part; revert cut (a) (the solo note on its own lane-4
voice again: riding a chord voice saved nothing measurable). The budget is **accepted at about 0.97 on the host
bench**, under the hard limit 1.0; the device is measured in Plan 2. Final numbers after the revert (host load 4.2-5.3,
AMB-1 140.9-152.6 ms): seed 20 forced, `VNL_RUNS=7`, three runs **0.985 / 0.937 / 0.979**; the standard list once,
`--forced 20 7 155 138 100 22`: 0.944 / 0.871 / 0.848 / 0.845 / 0.788 / 0.779 plain, **0.956** / 0.874 / 0.906 / 0.855
/ 0.793 / 0.804 forced. Memory: image 185412 B, RAM 50944 B, pool 180216 B (`tools/mem-check.sh`: ok).

**Re-measured after the revert (fix round 1).** The bench's pad part now plays as the songs do: a 2-note chord on
lanes 1-2 and the solo note on its own lane-4 voice (`app/host/bench.c`, P_PAD).
- **`--parts`, best of 7, two runs** (AMB-1 146.0 / 142.7 ms, idle floor 0.115 / 0.118, host load 5.5-5.8):
  - pad strings **0.046 / 0.047**, warm pad **0.045 / 0.046**;
  - over the source with the full chain 0.048-0.049 (strings) and 0.045-0.048 (warm pad);
  - dry 0.050 / 0.050. The first run's dry rows read 0.011-0.013, which is noise.
- **The picker** (200 seeds, host load 4.9) re-picks seed 20: 0.954 plain, **0.967** forced (AMB-1 142.4).


**Final-review fix (2026-10-24): a step glides on its own lane; a held lane takes its new velocity.** The 2-note chord
part used to move a note onto its spare lane at nearly every change, leaving the old lane's 1.5-2 s release under the
new chord, a third voice sounding where 2 would do. A step now glides on its own lane (released
lanes at a change: 15,750 -> 1,236 of 16,023 over 500 songs). Measured on a busy host (load 4.8-8.0):
- seed 20 forced, `VNL_RUNS=7`, five runs: **0.971 / 0.978 / 0.980 / 0.994 / 0.916** (VNL 135.2-138.6 ms, AMB-1
  139.2-149.2);
- the standard list, `--forced 20 7 155 138 100 22`, twice: 0.949 / 0.963 / 0.899 / 0.852 / 0.779 / 0.793 plain,
  **0.976** / 0.928 / 0.914 / 0.864 / 0.797 / 0.805 forced (AMB-1 140.9); then (`VNL_RUNS=3`) 0.942 / 0.893 / 0.842 /
  0.823 / 0.770 / 0.771 plain, **0.959** / 0.889 / 0.876 / 0.851 / 0.789 / 0.803 forced (AMB-1 142.5). (A first run
  with AMB-1 at 197.6 ms, a host stall, is left out.)
- No saving shows above the host's noise: VNL's ms for seed 20 forced (135-139) match the accepted numbers'
  (135.8-136.4): a releasing voice is a quarter-rate saw pair, under the bench's noise. The budget stays at about 0.97 (Sam's acceptance), under the hard limit 1.0.

**Pads revision 2, Task 1 (2026-10-24): the pad's solo lane takes its own voice.** Lane 4 may hold its own patch
(`vx_assign_pad_solo`: the choir, flute, organ or strings), rendered in `pad_seg` beside the ensemble at its model's
rate (the choir and the organ at half, the flute at full, the strings at a quarter) through `solo_sample`, out of
line. A first cut rendered it in `render_seg`'s voice loop: +6 ms on seed 20 forced with no solo playing (the loop's
extra work); a second call to `voice_sample` outlined it (+9 ms). The models' samplers (`pad_sample`, `wind_sample`,
`sub_sample`) are now always inlined, into `voice_sample` and `solo_sample` both: seed 20 forced at parity with the
base, alternating binaries, `VNL_RUNS=7`, three pairs (host load 4.1-4.7): base 0.975 / 0.967 / 0.974, new 0.972 /
0.971 / 0.967.
- **`--parts`, the new `solo` rows** (the solo note alone on lane 4 over the strings pad, held a bar and gliding, vel
  58; best of 7, AMB-1 139.8 ms, idle floor 0.108): choir **0.107**, flute **0.220**, organ **0.111**, strings
  **0.025**; the pad's chord part (2 notes) 0.047 / 0.046 in the same run. So a solo section costs about what the
  chord part does on the strings, about 0.06 more on the choir or the organ, and about 0.17 more on the flute (a wind
  at the full rate, its breath noise and filters), while it plays. Task 2 rolls solo voices into songs: the standard
  list will need re-measuring there.
- **The standard list** (no song plays a solo yet), `--forced 20 7 155 138 100 22`: 0.941 / 0.887 / 0.845 / 0.819 /
  0.755 / 0.768 plain, **0.958** / 0.889 / 0.871 / 0.846 / 0.778 / 0.796 forced (AMB-1 139.8).
- **Memory:** image 193096 B (185652 at the base: +7.4 KB, the samplers inlined twice), RAM 51216 B (+272: the
  voices' glide fields), pool 180216 B (`tools/mem-check.sh`: ok).

**Task 1 fix round (controller's ruling): the solo lane's other voices at half the rate.** The flute (M_WIND) gains a
half-rate path for the pad's solo lane: its phase increments, low-pass, breath and mute band-passes, and chiff are
set per two-sample step; the breath is scaled by 1/sqrt(2), since half-rate noise has twice the power in band; and
the vibrato still updates every 32 samples. The choir and the organ were already at half rate. At half rate each is
within 0.4 dB of its full-rate level (test_pad_solo_patch). The target was a solo section costing no more than the
chord part it replaces, +0.01.
- **`--parts`** (best of 7, AMB-1 134.5 ms, idle floor 0.108, host load 4.1):

  | row | ratio | target |
  |---|---|---|
  | chord part (strings) | 0.048 | - |
  | solo choir | **0.108** | 0.058 |
  | solo flute | **0.138** (was 0.220 at the full rate) | 0.058 |
  | solo organ | **0.112** | 0.058 |
  | solo strings | 0.025 | 0.058 |

  The choir, the flute and the organ miss the target. They are already at half rate, so the remaining cost is their
  models plus the half-band upsampler.
- **Seed 20 forced, `VNL_SOLO`** (new: the song's existing solo note on that voice, with the chord part left out
  everywhere; this is a lower bound, since the solo plays only in its rolled sections), `VNL_RUNS=7`, one run each:
  none 0.959, strings 0.929, choir 0.973, flute 0.976, organ 0.965.
- Memory: image 192920 B, RAM 51216 B, pool 180216 B (ok).

**Task 1 fix round 2 (controller's ruling): the choir and the organ at a quarter of the rate.** On the solo lane, the
choir (M_PAD) and the organ (M_SUB) now render in pad_seg's quarter-rate path beside the ensemble (SOLO_Q4). They use
the matched biquads at a step of 4 and the organ's control rate at 4 steps, and share the quarter-rate mud high-pass,
bus filters and interpolation. The ruling allowed quarter rate per voice only if two tests passed; both voices pass
(test_pad_solo_patch):

| voice | level, quarter vs half rate | folds at MIDI 84 | vs strings (32.0) |
|---|---|---|---|
| choir | 0.0 dB | 37.8 dB under | better |
| organ | -0.2 dB | 37.2 dB under, against its sub's pitch | better |

The flute stays at half rate.
- **`--parts`** (best of 7, AMB-1 138.9 ms, idle floor 0.108):

  | row | ratio |
  |---|---|
  | chord part (strings / warm pad) | 0.046 / 0.046 |
  | solo choir | **0.043** (was 0.108) |
  | solo organ | **0.042** (was 0.112) |
  | solo flute | **0.138** (0.09 over the chord part) |
  | solo strings | 0.024 |

- **Seed 20 forced, `VNL_SOLO`** (`VNL_RUNS=7`, one run each; a lower bound, see above):

  | solo voice | ratio |
  |---|---|
  | none | 0.970 |
  | strings | 0.940 |
  | choir | 0.944 |
  | organ | 0.944 |
  | flute | 0.973 |

- **The standard list**, `--forced 20 7 155 138 100 22`:
  - plain: 0.961 / 0.897 / 0.843 / 0.821 / 0.783 / 0.767;
  - forced: **0.977** / 0.896 / 0.874 / 0.848 / 0.802 / 0.797 (AMB-1 141.8, busy host).
- **Accepted at Sam's ~0.97 line; solos play in about a quarter of struck-keys sections; flag seed 20 + a flute solo
  for Plan 2's device bench.**
- Memory: image 192888 B, RAM 51216 B, pool 180216 B (ok).

## Pads revision 2, Task 4: re-measured with the retuned levels

Pads revision 2 in place (either the chord part or the solo note per section, the solo optional on a rolled voice,
half-note comping) and Task 4's level retune (gains only: the keys per comping, the chord part's velocity 45 to 48).
Host load about 4-5, AMB-1 136.5-138.4 ms.
- **The standard list**, `--forced 20 7 155 138 100 22`:
  - plain: 0.942 / 0.881 / 0.664 / 0.815 / 0.790 / 0.769;
  - forced: **0.960** / 0.878 / 0.695 / 0.848 / 0.804 / 0.797.
- **Seed 20 forced** again, `VNL_RUNS=5`, two runs: 0.972, 0.971. Under Sam's ~0.97 line or on it, as at Task 1's fix
  round (0.977); the hard limit is 1.0. Half-note comping adds keys strikes but no voices (the keys' 4 voices cover a
  struck chord's release), and the solo and the chord part never sound together, so the layer costs no more than
  before.
- **Memory** (`tools/mem-check.sh`): image 193736 B, RAM 51216 B, pool 180216 B (ok).

## Sampled instruments, Task 4: the strum engine

CS_STRUM plays the guitars' strums (strum.c), up to 6 sampler voices a stroke; the chords' pool is 8 voices when its
patch is M_SAMP (as for a pad). (As first built, a stroke took the free voices and stole when none was; since fix
round 1 below, each string has its own voice and a re-strike damps its last note on one of the 2 spares.)
- **The standard list**, `--forced 20 7 155 138 100 22`: forced 0.960 / 0.881 / 0.693 / 0.848 / 0.797 / 0.708
  (plain 0.938 / 0.875 / 0.667 / 0.814 / 0.776 / 0.671); none of them strums.
- **Strummed seeds** (their own set, forced): 24 0.857, 27 0.875, **32 0.937** (Nylon and synth pluck, D-DU-UDU, the
  heaviest), 52 0.777, 73 0.744, 83 0.848, 101 0.798, 140 0.809, 185 0.842. Seed 32 played held before: 0.824, so a
  strum costs about 0.11 over held comping (the sampler voices' release tails). Under the 0.97 line.
- The voicer (a search over movable shapes, once per new chord, cached per song): about 8 us a chord on the host.
- **Memory** (`tools/mem-check.sh`): image 365000 B, RAM 51472 B, pool 180216 B (ok; +3984 B image, +256 B RAM: the
  per-song shape cache).
- **Fix round 1 (one voice per string):** a stroke re-strikes its strings' voices (the chords' first 6), the old note
  damped on one of the 2 spare voices; no stroke takes another string's tail. Seed 32 forced 0.904 (from 0.937),
  seed 24 0.821 (from 0.857). Memory: image 364664 B, RAM 51536 B (ok).

## Sampled instruments, Task 5: the Knight and Emilyguitar sets

Nine sets (ids 21-29) put the Knight and the Emilyguitar on the keys and the lead. Adding sets moves every seed's set
(the set draw is hashed apart from the mood's now, roll.c), so the standard list's seeds play other sets: 20 Knight
and flute (held), 7 Felt and trumpet, 155 Rhodes neo-soul, 138 Nylon and sax, 100 Felt and trumpet, 22 Rhodes and
Emilyguitar. The sampler's per-sample path is inline now (sampler.h): seed 20 forced went from 1.02-1.03 to the
numbers below; on seed 20 the Knight's keys cost about 0.06 over the felt piano's (the same voice count).
- **The standard list**, `--forced 20 7 155 138 100 22` (`VNL_RUNS=5`): forced 0.990 / 0.877 / 0.678 / 0.664 / 0.907 /
  0.892 (plain 0.968 / 0.878 / 0.651 / 0.637 / 0.886 / 0.861), AMB-1 137.8 ms (a low run).
- **The picker** (200 seeds): the heaviest is still **seed 20** (now Knight and flute): 0.939 plain, **0.966** forced
  (AMB-1 141.0 ms). Its VNL time is steady at 136-137 ms; the ratio swings 0.966-0.990 with AMB-1's run (137.8-142.7
  ms). At Sam's ~0.97 line, as at Task 1's fix round (0.977); the hard limit is 1.0.
- **Memory** (`tools/mem-check.sh`): image 365192 B, RAM 51536 B, pool 180216 B (ok).

## Sampled instruments, Task 6: the string synth (M_STR)

The spike's string section as a mono lead (strsyn.c): three polyBLEP saws, a one-pole high-pass, three TPT SVFs (the
low-pass and two formants) and the Plaits Ensemble's three taps, at half the rate on the lead (its low-pass is 3.2
kHz at most), into the lead's half-band upsampler. Four sets (30-33) add the high lead and the cello; adding sets
moved the seeds' sets again: 20 Knight and flute (unchanged), 7 Rhodes and saw lead, 155 DX and strings, 138 Knight
and sax, 100 Emilyguitar and flute, 22 City pop Rhodes.
- **The voice** (a held note on the lead, 60 s, over the idle engine's 14.3 ms): 21.5 ms at half the rate (about
  0.16 of the budget, the lead's upsampler included), 31.8-32.2 ms at the full rate (0.23); the high lead and the
  cello alike. Over the spec's estimate (0.10): its Ensemble and three filters run per voice sample.
- **The standard list**, `--forced 20 7 155 138 100 22`: forced 0.969 / 0.892 / 0.692 / 0.880 / 0.757 / 0.869 (plain
  0.951 / 0.891 / 0.658 / 0.852 / 0.743 / 0.838), AMB-1 136.9 ms.
- **The string sets** (their seeds' own rolls, forced): Knight and strings 69 0.721, 75 0.781; Knight and cello 5
  0.770, 50 0.815; Rhodes and cello 8 0.722, **41 0.945** (the heaviest); DX and strings 44 0.855, 155 0.684. Under
  the 0.97 line.
- **The picker** (200 seeds): the heaviest is still **seed 20** (Knight and flute): 0.933 plain, 0.947 forced (AMB-1
  144.6 ms); no string set is heavier.
- **Memory** (`tools/mem-check.sh`): image 369928 B, RAM 52688 B, pool 184312 B (ok; the pool +4096 B: the
  Ensemble's lines, 512 floats for each of the 2 lead voices).

## Sampled instruments, Task 7: the sampled kits (skits.c)

Five sampled kits (KIT_S_WARM, DARK, RUSTY, GENTLE, CLEAN) from the one drum pool: up to 8 one-shot voices (ADPCM
decoded as they go, 4-point Hermite at the kit's pitch, a lane's one-pole low-pass on the darkened Gogodze hits), then
the kit's bus (two one-poles at 3.2 kHz and a tanh drive on the warm kit only). No set names a sampled kit yet (Task
8), so every song renders bit-identical to before (checked on seed 101) and the standard list is unchanged.
- **The kit** (`bench --parts`, all six lanes of the boom-bap part, the ride's 8ths over the hats as an upper bound):
  ratio 0.34-0.36 (dusty 0.249, the synth kits 0.24-0.25); without the ride (a song plays hats or the ride, not both)
  0.29-0.31, about +0.05 over the synth kits. The warm and dark kits are the dearest (the drive; the lane low-passes).
- **In a song** (seed 20, the heaviest roll, its set's kit swapped for a test build, three interleaved runs on a busy
  host): Dusty 151-153 ms, warm 166, dark 169-170, rusty 163-165 ms of VNL time, so **+11 to +17 ms (about +0.08 to
  +0.12)** over the synth kit: longer hits keep the drum bus live more of the time. On a quiet host's 0.966 that is
  about 1.05-1.08 forced, over the 0.97 line: the heaviest roll on a sampled kit needs Task 9's cuts (options: the kit
  at half rate, its hits are 16 kHz; linear interpolation; fewer voices).
- **Cymbals at 32 kHz** (the A/B, not taken unless Sam picks it): the hats, shaker and ride re-packed at 32 kHz add
  **20000 B** of sample data (smp_bytes 160928 to 180928; image 394456 B, still ok) and about +0.01-0.03 on the kit row.
- **Memory** (`tools/mem-check.sh`): image 374456 B (+4320 B), RAM 53376 B (+688 B), pool 184312 B (ok).

## Sampled instruments, Task 8: the kit per song and the ride roll

Every set but the 808 and Brushes sets names a sampled kit; a song plays it or the synth kit, about half each (stream
192), and the sampled songs ride per stream 193 (the warm kit throughout; the others 15% throughout, 50% one block).
Host under load (load average about 4; AMB-1 139.3-142.1 ms):
- **The standard list** (`--forced 20 7 155 138 100 22`): seed 20 now rolls Knight and flute on the **dark sampled
  kit** (hats): 1.185 plain, **1.203** forced (VNL 165-168 ms, Task 7's dark-kit measure on this seed: 169-170 ms);
  7 0.917 / 0.918 (Rhodes and saw lead, Dusty); 155 0.727 / 0.755 (DX and strings, clean kit, a ride block);
  138 0.866 / 0.894 (Knight and sax, Soft); 100 0.748 / 0.766 (Emilyguitar and flute, Dusty); 22 0.869 / 0.897
  (City pop Rhodes, Soft).
- **The picker** (200 seeds): the heaviest is **seed 20** (the dark kit, as above): 1.173 plain, **1.194** forced.
- Over the 0.97 line by about 0.2 on the heaviest sampled-kit roll: Task 9's cuts (the roll was not tuned to dodge it).
- **Memory** (`tools/mem-check.sh`): image 375448 B, RAM 53408 B, pool 184312 B (ok).

## Sampled instruments, Task 9: the budget cuts, measured

Task 8 left the heaviest roll over the line: seed 20 (Knight and flute on the dark sampled kit) at **1.20-1.23**
forced. A profile (`sample` on a -O2 -g build) put the sampled kit at about 29% of that render: it was called once per
sample, looping 8 voices and 5 kit buses each time, its state reloaded at every step. The cuts, in the controller's
order, each measured alone (seed 20 forced unless named; best of 3 x 4-5 interleaved rounds against the build before
it, so the host's drift cancels; the host was loaded, load average 2-6):

| cut | what | sound | before | after | delta |
|---|---|---|---|---|---|
| 0 | the drums rendered a segment at a time (`drums_render`; each hit's run in one loop, then each kit's bus) | bit-exact | 169.0 ms | 146.0 ms | -0.17 |
| 1 | the sampled kits at half the rate (22.05 kHz, a 27-tap half-band up; low-passes matched) | A/B below | 147.3 | 140.0 | -0.05 |
| 2 | render_seg's voice loop steps through the sounding voices' bits (not all 13) | bit-exact | 140.0 | 135.8 | -0.03 |
| 3 | an M_SAMP voice skips the shared envelope's steps (its level there is exactly 1) | bit-exact | 130.0 | 126.5 | -0.025 (seed 138: -0.04) |
| 4 | a sampled hit's run in locals (`sv_run`: acc[] could alias the voice) | bit-exact | 129.1 (s189) | 123.4 | -0.04 (seed 20: -0.03) |
| 5 | a kit bus's states in locals | bit-exact | 123.9 (s189) | 117.4 | -0.05 |

Measured and not taken (they did not pay, or were not needed once every ratio was under the line):
- **Half rate for M_SAMP leads** (the plan's cut): seeds 42, 200, 10, 70, 24 (Knight and Emilyguitar leads) moved
  0 to -1.8 ms (noise): the lead's two sampler voices are a small part of any song.
- **Fewer strum voices sounding**: not needed; the heaviest strummed seeds are 184 (Bossa nylon) 0.78 and 135
  (Emilyguitar and vibes) 0.75 forced.
- **Linear interpolation in place of Hermite on the Knight**: -2.5 ms on seed 20 (about -0.02); it dulls the top
  (about -1 dB at 3 kHz, more above) and adds images, for little.
- Synth kit lanes in locals, the glue detector's sum in a local: no change.

Cut 1, the sound: the hits are 16 kHz (nothing above 8 kHz) and every kit pitches down or not at all, so half the
rate keeps their band; the half-band is flat within 0.01 dB to 8 kHz. The cut assumes 16 kHz hits: the 32 kHz
cymbals of Task 7's A/B (not taken) would lose their top octave at half the rate, so picking them means the kits back
at the full rate (`VX_OFF_HALF`, about +0.05) or a mixed path (the cymbal lanes at the full rate, the rest at half). The voices' steps, the choke's fall (still -60
dB in 6 ms) and the low-passes are rescaled; the low-passes are matched to the full rate's gain at 1.4 times their
cutoff (the impulse-invariant coefficient alone lifted the warm kit's bright hits 0.9 dB). song_test's test_skits:
per kit and lane, the level within 0.3 dB of the full rate (measured: within 0.22), every octave band 125 Hz-8 kHz
within 1 dB (0.69), an unfiltered lane's waveform at least 20 dB under (24-69 dB; only the interpolator's images
above 8 kHz differ). The drums are 14 samples (0.3 ms) later, as the half-rate keys. `VX_OFF_HALF` keeps the full
rate, bit-identical to before. A/B: `app/build/new/ab-halfkits-{warm,dark}-{before,after}[-nobed]` (seeds 34 and 20).

### The budget after the cuts

`VNL_RUNS=7`, AMB-1 133.7 ms (a low run: the ratios read high):

| seed | set (kit) | plain | forced |
|---|---|---|---|
| 20 | Knight and flute (dark sampled) | 0.889 | **0.913** (was 1.226) |
| 7 | Rhodes and saw lead (Dusty) | 0.886 | 0.884 |
| 155 | DX and strings (clean sampled) | 0.627 | 0.656 |
| 138 | Knight and sax (Soft) | 0.793 | 0.818 |
| 100 | Emilyguitar and flute (Dusty) | 0.701 | 0.720 |
| 22 | City pop Rhodes (Soft) | 0.836 | 0.867 |
| 189 | Rhodes neo-soul (warm sampled, ride throughout) | 0.880 | 0.890 |
| 142 | Neo-soul Wurli (Soft) | 0.922 | **0.936** |
| 184 | Bossa nylon (Brushes; strummed, the heaviest strum) | 0.754 | 0.784 |
| 135 | Emilyguitar and vibes (rusty sampled; strummed) | 0.717 | 0.746 |

- **The picker** (200 seeds, `VNL_RUNS=5`): the heaviest is now **seed 142** (Neo-soul Wurli and sax on the synth
  Soft kit), 0.911 plain, **0.933** forced (AMB-1 136.2 ms); the heaviest sampled-kit roll is seed 20 at 0.913.
  Every ratio is under the 0.97 line by about 0.035 at a low AMB-1 run (at a typical 138-140 ms, about 0.90-0.91).
- **Memory** (`tools/mem-check.sh`): image 377336 B, RAM 56560 B (+3152 B: the kits' call buffers and the
  half-band's history), pool 184312 B (ok); sample data 160928 B (unchanged).

### Per component, after the cuts

`bench --parts` (best of 7, AMB-1 136.3 ms, idle floor 0.102; rows net of the floor, as above):

| component | ratio |
|---|---|
| Knight (M_SAMP), 4 chord voices, half rate | 0.285 (Rhodes 0.304, felt piano 0.294) |
| Emilyguitar (M_SAMP), 4 chord voices | 0.276 |
| Nylon (M_SAMP), 4 chord voices | 0.259 |
| Knight on the lead, 2 voices, full rate | 0.225 (flute 0.242) |
| Sampled kits, all lanes and the ride over the hats: warm / dark / rusty / gentle / clean | 0.148 / 0.152 / 0.119 / 0.121 / 0.120 |
| Synth kits: Dusty / Soft / 808 / Brushes | 0.128 / 0.118 / 0.142 / 0.114 |
| String synth (M_STR), a held lead note, half rate | 0.16 (Task 6; unchanged by the cuts) |

In songs (seed forced, with and without the role, best of 5, AMB-1 about 136 ms):
- **M_SAMP on the keys**: seed 20's Knight keys 36.6 ms, about 0.27 (about 4.1 voices sounding on average, so
  about 0.065 a voice).
- **The strum** (the chords' role, strummed): seed 184 (Bossa nylon) 39.5 ms, about 0.29; seed 135 (Emilyguitar)
  37.9 ms, about 0.28.
- **The sampled kits**: warm (seed 34, ride throughout) 14.1 ms, about 0.10; dark (seed 20, hats) 17.1 ms, 0.13;
  rusty (54) 6.2 ms, gentle (79) 5.8 ms, clean (24) 5.7 ms, about 0.04-0.05. For scale, seed 142's synth Soft kit:
  17.0 ms.

## Final review: the worst tick (the strum voicer)

song_tick and song_start run in the audio interrupt, so their worst case is what counts. `tools/tick-bench.sh N R`
(app/host/tick_bench.c) plays seeds 2..N, each song R times; a tick's time is its least over the R plays (the host's
noise filtered out), then the worst over every tick, strummed songs and the rest apart. The strummed songs' worst came
from the guitar voicer (strum_voicing, a search over movable shapes, about 8 us a chord, up to 45 on a cold cache) run
for up to 4 new chords in one tick (the pads' 4-bar look-ahead), then again when the bars reached them. Two changes,
the music unchanged:
- **The voicer's search pruned** (branch and bound: each branch's least possible cost, from the terms that only grow
  as strings are added and the top note's terms at their least; a branch that cannot beat the best so far is left).
  The same shape for every chord: over 26400 chords and voice-leadings against the old voicer, none differs. About
  2x faster (mean 4.2 us from 8.3).
- **The pads' look-ahead keeps its shapes**: they are the ones the bars find later (the same order, from the same last
  shape), so the voicer runs once a chord.

`tools/tick-bench.sh 6000 5` (5895 other songs, 104 strummed; host, -O2):

| | worst tick, strummed | worst tick, other | worst song_start |
|---|---|---|---|
| before (ae70aef) | **67-74 us** | 27-33 us | 6-9 us |
| after | **29-30 us** | 26-34 us | 7-8 us |

The event hashes (every event of the 6000 songs) are the same before and after (strummed 4f31fde7, other 4dd291be),
as are the rendered audio of 16 strummed songs (plain and with two set changes) and the scenarios' songs.wav and MIDI;
song_test's test_strum_same holds the voicer's shapes and 24 strummed songs' events to their values from ae70aef.
groove_ride and plan_build (the ride's extra plan builds at a song's start) cost about 1 us each; song_start's worst,
all of it, is under 10 us. The standard list after: forced 0.904 / 0.881 / 0.648 / 0.817 / 0.715 / 0.861 (AMB-1
135.8 ms). Memory: image 378072 B (+736 B), RAM 56560 B (unchanged), pool 184312 B (ok).

### Green baseline (2026-10-28, after cf0dca0, a162c90, 3025034 and the 1.5 s ride)

`--forced 142 16`: 0.913 / 0.915 (AMB-1 142.9 ms); the standard list (`--forced 20 7 155 138 100 22`) 0.933 / 0.851 /
0.823 / 0.820 / 0.660 / 0.833 (AMB-1 145.5 ms). Memory: image 386344 B (budget 409600; +8272 B since the strum
work, the ride sample and the other imports), RAM 56560 B (unchanged), pool 184312 B (ok).

### Workbench round 2, Task 1b: no ear candy; the pad in about two songs and sections of three (2026-10-29)

No song plays the ear candy (V_CANDY unassigned, nothing emits it), and the pad layer sits out about a third of
songs and of its sections, so the songs do less work, not more. What is added: per 4-bar group, `pad_part` for the
block and its two neighbors (each `song_pad_in` seeds at most 24 tiny rng streams), and in the engine one multiply
per 256-sample chunk (the pad layer's level and its 1 dB share of the keys' duck folded into its dry scale's
target). `--forced 20 7 155 138 100 22`, HEAD (f67842d) and this task interleaved, twice each: the host was loaded
(load average about 7.6, AMB-1 at 338-411 ms against the usual 136-146), and the ratios moved up to 0.3 between runs
of the same binary, so the comparison is noise: HEAD 1.13 / 0.92 / 0.82 / 0.81 / 0.64 / 0.89 and 1.12 / 1.01 /
1.15 / 0.85 / 0.68 / 0.92; Task 1b 0.95 / 0.83 / 0.82 / 1.31 / 0.68 / 0.87 and 1.08 / 1.03 / 0.88 / 0.85 / 0.64 /
1.00. Re-measure on a quiet host before reading anything into these.

### Leads: the sampled vibraphone and the felt piano in; five synth leads out (2026-10-29)

The vibes are the VCSL vibraphone (M_SAMP, 5 zones C4-E6, 16 kHz IMA ADPCM, 0.46 s each: 18416 B of sample data);
the felt piano leads some sets (its keys patch, on the lead); the muted trumpet, sax and guitar lick patches and the
string synth (strsyn.c, its Plaits Ensemble and the lead's two 512-float lines in the pool) are gone.

- **Memory** (`tools/mem-check.sh`): image **386908 B before, 402108 B after** (+15200: the vibraphone's 18416 B
  less about 3.3 KB of the string synth's code and the three patches; budget 409600, about 7.4 KB left), RAM 56592 ->
  55440 B, pool 184312 -> 180216 B (the string synth's lines).
- **CPU, per component** (`bench --parts`, the lead row: 2 voices of 8ths, held a dotted 8th; best of 3, two runs,
  HEAD 44827cd's bench beside this one, a loaded host, load about 7): the vibes **0.20** (were 0.27-0.30 as the FM
  bell: the sampler is cheaper), the felt piano on the lead **0.17-0.18** (new row); the removed rows were the muted
  trumpet 0.25-0.27, the sax 0.22-0.23, the guitar lick 0.12-0.13 and the string synth 0.16 (Task 6). The other
  leads read as before (flute 0.23, kalimba 0.18-0.21, saw lead 0.12, synth pluck 0.22, Knight 0.21-0.22).
- **CPU, songs** (`--forced 20 7 155 138 100 22`, interleaved with HEAD, twice): none of the standard list's sets
  changed lead, so the rows read the same within the loaded host's noise (seed 155 forced 0.85 / 0.78 HEAD, 0.83 /
  0.74 now; seed 22 forced 0.74 / 0.81 HEAD, 0.84 / 0.77 now). A vibes song now costs less than before (the sampler
  under the bell), a felt-lead song about what a struck sampled lead does.

### Vibes rebalance (2026-10-29)
Set 10 (Pad and pluck) leads with the synth pluck at +1.2 dB (lib_set_lead_db, keyed by set id, base id in the workbench): its lead over keys sat 7.4 dB median, under the 8 dB floor; now 8.6. A per-song float multiply: no measurable CPU or memory change.

### Lead styles, Task 1: the lead framework and the keys' room (2026-10-29)

New: app/song/leads/ (the motif form, the register centers, the dispatch: no style registered yet), and the keys'
window in harmony.c (the voicer's ceiling worked out per voicing call, with the loop's voicings, and once at
song_start for the lead's band). `tools/tick-bench.sh 3000 5`, this task and HEAD (358f7c5) interleaved, three runs
each: worst tick, other songs **27-29 us** (HEAD 27-28), strummed **23-29 us** (HEAD 22-24; strummed songs never take
the window, their events hash the same, 936e9bb7: host noise); worst song_start 8 us (unchanged). Memory
(`tools/mem-check.sh`): image **404544 B** (budget 409600), RAM 55456 B, pool 180216 B.

### Lead styles, Task 2: Impressionism and Romanticism (2026-10-29)

Two style files (app/song/leads/impressionism.c, romanticism.c) and the helpers they share in leads.c (lead.c's
older engines now use the same copies). Image by `./build.sh`, measured style by style:

| build | image | delta |
|---|---|---|
| Task 1 (54e2723) | 404544 B | |
| shared helpers, context builder, triplet delay, window recompute (no style) | 405232 B | +688 |
| + Impressionism | 409456 B | +4224 (impressionism_plan 2952 B) |
| + Romanticism | 413376 B | +3920 (romanticism_plan 2704 B) |

**Over budget (409600) by 3776 B until Task 8** (ruling: accepted on the unpushed branch; no device install before
then). Projection: Task 6's removals free about 16 KB (lead.c's older engines ~6.8 KB, melody.c ~6.5 KB, phrase.c
~2.7 KB, the lead-arp tables); eight styles at ~4 KB each need ~32 KB, so with today's headroom the plan ends **about
11 KB short after Task 6**: Task 8 brings trims to Sam. RAM 55488 B, pool 180216 B (unchanged). CPU: a style plans
once a bar, as the phrase planner did.

CPU (fix round 1): `tools/tick-bench.sh 3000 5 [STYLE]` (a third argument now forces a lead_style), three runs
each, interleaved: worst tick over the 2933 other songs, no style forced **27-28 us**, Impressionism forced (2)
**27-33 us**, Romanticism forced (3) **27-31 us**; strummed songs 22-27 us in all three; worst song_start 7-8 us in
all three. The styles' worst ticks sit within the host's noise of the older engines' (the bar's plan runs on step 0
either way); the unforced events hash is unchanged (7dbc0885, 936e9bb7).

### Lead styles, Task 3: '60s Jazz and Modal Jazz (2026-10-29)

Two style files (app/song/leads/jazz60s.c, modal.c); the scale mask and the mode's color note moved into leads.c's
shared helpers (Impressionism uses the same copies). Image by `./build.sh`, measured style by style:

| build | image | delta |
|---|---|---|
| Task 2 (768211a) | 413376 B | |
| shared helpers (no new style registered) | 413536 B | +160 |
| + '60s Jazz | 416448 B | +2912 |
| + Modal Jazz | 419424 B | +2976 |

**Over budget (409600) by 9824 B until Task 8** (the Task 2 ruling). Four styles now average ~3.5 KB; the Task 2
projection (~11 KB short after Task 6 for all eight) holds, a little better. RAM .data+.bss 55632 B (+144: the two
styles' state across bars in lead_state_t's union), pool 180216 B (unchanged).

CPU: `tools/tick-bench.sh 3000 5 [STYLE]`, three runs each, interleaved: worst tick over the 2933 other songs, no style
forced **27-28 us**, '60s Jazz forced (4) **28 us**, Modal Jazz forced (5) **27-28 us**; strummed songs 23-28 us in
all three; worst song_start 6-8 us. Within the host's noise of the older engines'; the unforced events hash is
unchanged (7dbc0885, 936e9bb7).

### Lead styles, Task 4: Jazz-Hop and Film Score (2026-10-29)

Two style files (app/song/leads/jazzhop.c, filmscore.c); the mode's pentatonic mask moved into leads.c's shared
helpers (Impressionism uses the same copy). Image by `./build.sh`, measured style by style:

| build | image | delta |
|---|---|---|
| Task 3 (a2b3fa3) | 419424 B | |
| shared helper (no new style registered) | 419488 B | +64 |
| + Jazz-Hop | 422432 B | +2944 |
| + Film Score | 425808 B | +3376 |

**Over budget (409600) by 16208 B until Task 8** (the Task 2 ruling). Six styles now average ~3.4 KB; the Task 2
projection (~11 KB short after Task 6 for all eight) holds. RAM .data+.bss 55632 B (unchanged: neither style keeps
state across bars beyond lead_state_t's shared tie), pool 180216 B (unchanged).

CPU: `tools/tick-bench.sh 3000 5 [STYLE]`, three runs each, interleaved: worst tick over the 2933 other songs, no style
forced **27-28 us**, Jazz-Hop forced (6) **27-30 us**, Film Score forced (7) **30-32 us**; strummed songs 21-31 us in
all three; worst song_start 6-11 us. Film Score sits a little over the rest (each of its notes is fitted by its own
scan of the bar's range), within Impressionism's 27-33 us; the unforced events hash is unchanged (7dbc0885,
936e9bb7).

### Lead styles, Task 5: Bossa Nova and Ambient (2026-10-30)

Two style files (app/song/leads/bossa.c, ambient.c); Ambient keeps one int across bars (its motif's level, in
lead_state_t's style union: RAM unchanged). Image by `./build.sh`, measured style by style on today's tree (its base/
carries an uncommitted font packing, -26272 B):

| build | image | delta |
|---|---|---|
| neither registered (Task 4's six) | 399536 B | |
| + Bossa Nova | 402912 B | +3376 |
| + Ambient | 405440 B | +2528 |

**+5904 B for the two.** On the committed base (no packed fonts) that is about 431712 B, 22112 B over 409600; with
the packed fonts, 405440 B, 4160 B under. RAM .data+.bss 55632 B (unchanged), pool 180216 B (unchanged).

CPU: `tools/tick-bench.sh 3000 5 [STYLE]`, three runs each, interleaved: worst tick over the 2933 other songs, no style
forced **28 us**, Bossa Nova forced (8) **28-32 us**, Ambient forced (9) **27-33 us**; strummed songs 22-30 us in all
three; worst song_start 6-8 us. Ambient's first lead bar scores 15 motif levels over the loop's chords (once a song);
within Film Score's and Impressionism's range. The unforced events hash is unchanged (7dbc0885, 936e9bb7).

### Keys comping: held on 2, held on 2 and 4, light eighths; the busy-lead thinning switch (2026-10-30)

Three comping styles (CS_HELD2, CS_LIGHT8, CS_HELD24, harmony.c), candidates on the percussive keys only; the roll
weighs them 0, so an unforced song is unchanged (tests/run_tests.sh's pinned hashes hold; `tools/tick-bench.sh`
unforced events 95171cd4, strummed 93256f33, the same before and after). The comping tables widen to 8 strikes a bar
(patterns_comp_t step[8]/len[8]) and gain COMP_HELD24: flash only. song_ov_t gains one int8_t (no_busy_thin, the
workbench's "Busy lead thins the comping" off, `VNL_BUSY=0` on the host): 1 B of .bss and one more test in
harmony_step; 0 keeps today's thinning. No new state otherwise: the three styles keep one voicing per chord and draw
nothing per bar.

Image: **not measured yet.** `./build.sh` hung in its Docker step (the daemon stopped answering after the disk filled);
to be added once Docker is back.

CPU: `tools/tick-bench.sh 3000 5 -1 COMP` (a fourth argument, new: forces a comping style, held where the keys can't
play it), interleaved runs: worst tick over the other songs, none forced **28-33 us** (4 runs), held on 2 forced (6)
**28-35 us** (6 runs), held on 2 and 4 (8) **28 us** (2), light eighths (7) **28 us** (2); worst song_start 7-10 us in
all. The worst seed moves from run to run (held on 2's 34-35 us fell on four different seeds), so the spread is the
host's noise: held on 2 strikes once a bar, fewer than any comping but held. Light eighths strikes four times as often
as half notes, but each strike is one voicing already built: no measurable cost. With the thinning off, a busy bar
strikes as a quiet one does, which the unforced songs' quiet bars already cost.

### The formant choir: a candidate pad layer and solo voice (2026-10-30)

The spike's "oo-aah" choir ported (INS_CHOIR: an M_ENS patch with p[4] set; voices.c's choir bank): a candidate the
roll never draws (no set plays it; `VNL_PAD=choir` or the workbench's pad row), and the vox pad's solo notes sung by it
(song_ov_t's solo_choir, `VNL_SOLO_CHOIR=1` or the "Vox pad solo" row). Off, nothing changes: the pinned hashes hold.
The bank's coefficients and level match update once per 1024 samples of the engine's clock (43 a second), where the
spike recomputed them every segment.

CPU (`app/build/bench/bench --parts`, best of 7, AMB-1 135.7 ms, idle floor 0.104): pad row **choir 0.061** against the
warm pad's and the strings' 0.048 (the spike's 0.066); solo row **choir 0.038** against the vox pad's 0.043. Whole
songs (`--forced`, best of 3, AMB-1 138.3 ms) with every pad layer the choir: seed 20 0.927 -> 0.933 forced, seed 7
0.857 -> 0.866, seed 155 0.799 -> 0.809 (+0.006 to +0.010).

Flash (host -O2 objects of app/engine and app/song against f41748f; the image not built, Docker down): **+4.3 KB
text** (voices.o +4133 B: the bank, its cell update and pad_seg's split; patch.o +96 B: the patch), +372 B of data/bss
(the bank's quarter-rate buffer, 264 B, and its state). Stage 2 (the vox pad retired, M_PAD's formant path and M_PAD
off the solo lane) would take about 2.2 KB of that back on the same measure.

### The choir in the vox pad's place, and a pad the songs draw (2026-10-31)

Stage 2 of the choir: the pad's solo voice that was the vox pad is the formant choir (the same draw, the same notes;
its level the vox pad's over C5-C6, -33.48 dBFS, pinned in test_choir_solo), and a song draws the choir as its pad
layer in 1 song in 4 (lib_song_pad, seed stream 197) over the set's strings or warm pad. The vox pad is gone (no
patch; its id kept), with M_PAD's formant path and its solo-lane glide (M_PAD now plays only the chillwave pad's
keys); the stage-1 switches (`VNL_PAD`, `VNL_SOLO_CHOIR`, the "Vox pad solo" row) are gone. Event hashes: unchanged
(the instrument never reaches the events).

CPU (`bench --parts`, best of 7, AMB-1 211.9 ms on a busy host, idle floor 0.104): pad row **choir 0.053** (strings
0.048, warm pad 0.047); layer over the full chain choir 0.059 (strings 0.046); solo row **choir 0.037** (organ 0.043,
flute 0.138, strings 0.025; the vox pad was 0.043). Whole songs (`--forced`, best of 3, AMB-1 211.3 ms, stage 1 ->
stage 2): seed 20 (its solo now the choir) 0.937 -> 0.943 forced, seed 7 0.857 -> 0.855, seed 155 0.801 -> 0.796.
None of the three draws the choir pad; one in four songs now carries the pad row's +0.005 to +0.013.

Flash (host -O2 objects of app/engine and app/song; the image not built, Docker down): **-3.9 KB text against stage 1**
(voices.o -4084 B: the formants, the vox pad's legato branch and pad_sample's glide, inlined at several sites;
roll.o +180 B: lib_song_pad), so **+0.3 KB text and +372 B data against f41748f** (before the choir) for the choir
in all.

### The tonewheel organ: the keys' organ and a pad the songs draw (2026-10-31)

The spike's tonewheel (M_TW) ported as code, with Sam's picks. It replaces the square organ (INS_ORGAN on M_SUB) on
the keys and on the pad's solo lane. On the keys, a song rolls one of three drawbar settings (A, B with percussion,
or C; seed stream 198, equal odds). It plays an octave up, or at its register with the 16' out where the chords sit
high (harmony_keys_high: the voicings' mean note F4 or over). It runs at half the rate with the slow rotor and the
scanner chorus C3 (a 32-sample line on the keys' half-rate sum). The pad layer can also draw the organ: the one-stop
4' with C3 switched by section, in 15% of songs and never over organ keys (lib_song_pad, stream 197). It runs at a
quarter of the rate, its voices in a tight loop of their own (tw_run, sample-identical to the generic path). M_SUB
stays, because the saw lead and the synth pluck still play it. Only its solo-lane and keys-only paths went (sub_top,
the half-rate branch, the lane glide). Event hashes are unchanged: the instrument never reaches the events.

CPU (`app/build/bench/bench --parts`, best of 7, AMB-1 about 141 ms, idle floor 0.105; workbench c4aad6f against
this build):

| Row | Today | Tonewheel |
|-----|-------|-----------|
| chords, the organ | 0.224 (the square) | A 0.295-0.303, **B (percussion) 0.320-0.329**, C 0.254-0.264 |
| pad layer | warm pad 0.047 | **organ 0.074** (0.123 before tw_run) |
| pad's solo lane | the square organ 0.043 | organ 0.046 |

Whole songs (`--forced`):

| Seed | What it has | Today | Tonewheel |
|------|-------------|-------|-----------|
| 20 | the heaviest seed, now drawing the organ pad | 0.916 / 0.936 forced | 0.937 / **0.960 forced** |
| 108 | set 18, organ B raised | 0.633 / 0.629 forced | 0.701 / 0.703 forced |
| 37 | set 18, organ A at its register | 0.748 / 0.751 forced | 0.759 / 0.759 forced |

The worst case stays under the ~0.97 line by about 0.01. Seed 20 read 0.980 forced before tw_run, which is what
brought it back under.

Flash: Docker is down (`./build.sh` timed out), so this is estimated from host arm64 -Os objects against c4aad6f.
Text grows by **+4.3 KB**: voices.o +3094 B, patch.o +748 B (seven organ patches), roll.o +128 B, harmony.o +256 B
and plan.o +88 B. The wheels' sine is made at vx_init into RAM (2 KB of .bss), so it costs no flash. Deleting M_SUB
saves nothing, since it keeps its users; trimming its organ-only paths is already counted in the voices.o figure.
On ~406.4 KB measured last, the image is about **410.7 KB against the 409,600 B budget: about 1.1 KB over
(estimated)**. RAM grows by +3.7 KB of data and bss: the sine, the two scanner lines, the pad organ's quarter-rate
buffer, and a voice union about 64 B larger.

### Lead styles Task 6: the old lead engines, the melody path and the lead-arp patterns go (2026-10-31)

Removed: lead.c's phrase planner and hybrid arpeggio (lead.c keeps the dispatch), phrase.c, melody.c (the struck
melody, which never sounded), the lead-arp and melody-cells tables (LEAD_RHY, LEAD_PAT, LEAD_LANDING_*, MELODY_CELLS)
and the lead_double, arp_rhy and arp_pat overrides. Every song plays a lead style; a rolled song draws one of its lead
instrument's fits evenly (leads_style, stream 143; Task 7's weighted roll replaces it).

Flash: Docker hung (`./build.sh` gave no output in 3 minutes and was stopped), so this is **estimated** from host
arm64 -Os objects (text + data, app/song and app/song/leads) against e7f983f: **-22.1 KB** (123,418 B to 101,310 B).

| Object | Before | After | Delta |
|--------|--------|-------|-------|
| lead.o | 8,928 | 428 | -8,500 |
| melody.o | 8,186 | 0 | -8,186 |
| phrase.o | 4,230 | 0 | -4,230 |
| song.o | 8,224 | 7,192 | -1,032 |
| patterns_gen.o | 5,485 | 5,290 | -195 |
| leads.o (the interim style draw) | 4,769 | 4,940 | +171 |
| the eight style files (no lead_double test) | | | -144 |
| override.o | 552 | 528 | -24 |

On the ~410.7 KB estimated after the tonewheel organ, the image is about **388.6 KB against the 409,600 B budget:
about 21 KB under (estimated)**. CPU: the styles were already planning every forced song; rolled songs now take the
same path (a bar's plan, once a bar), with no melody step per 16th, so the tick costs no more than before.

### Lead styles Task 8: the models nothing plays go (bell, sub, pluck) (2026-10-31)

Removed: three voice models and the six patches that were their only users, none of them in any set's sounding role.
M_BELL went with the kalimba, music box and bells, which were left only in the sets' unused ear-candy slot (now none).
M_SUB went with the saw lead and synth pluck, along with the mono lead, its glides and vx_glides. M_PLUCK went with
the upright and its Karplus-Strong lines. The ids stay reserved. An old snapshot's kalimba, saw lead or synth pluck
plays the flute or the vibes, as before. Its upright plays the electric bass (lib_bass_approved, new). Song hashes
are unchanged.

Flash: Docker hung, so this is **estimated** from host arm64 -Os objects (text, literals, const and data) against
12d3848: **about -4.9 KB**. voices.o drops 4,876 B (47,620 B to 42,744 B), override.o drops 84 B, and library.o gains
16 B for the bass stand-in. patch.o is unchanged, since its table is sized by the instrument count. On the ~388.6 KB
estimated after Task 6, the image is about **383.7 KB against the 409,600 B budget, plus Task 7's changes, which
were not measured: about 25.9 KB under before them (estimated)**. RAM: the Karplus-Strong lines (13 voices × 1,072
floats) leave the .pool region, **-55,744 B of pool**. The voice union is unchanged, since the organ's is the
largest. CPU: no song played these models, so no song's cost changes. The bench loses its upright, kalimba, saw lead,
synth pluck and candy rows.

### Lead styles Task 8: the modeled vibraphone in the sample's place (2026-11-03)

Sam A/B'd the vibes (the sample, the model with its motor, the model still) and could not tell them apart. The
modeled vibraphone (M_MODAL, `app/engine/modal.h`: three modes, a soft mallet, the tube's bloom) now plays INS_VIBES
everywhere, at the sample's level (0.541). The motor is cut, since it was inaudible. The VCSL vibraphone's five zones
and its bank (SB_VIBES) are removed. No switch is left. The sampler stays for the Knight, the nylon and the
Emilyguitar. Event hashes are unchanged because the instrument never reaches the events. Only the vibes-lead songs
sound different: of seeds 1-80, the 20 with the vibes lead render differently and the other 60 are bit-identical
(their per-bar energies hashed).

CPU (`app/build/bench/bench --parts`, best of 7, AMB-1 about 142 ms, idle floor 0.103, b9653f4 run beside this build
on a loaded host, load about 4.5):

| Row | Sample (b9653f4) | Model, motor off |
|-----|------------------|------------------|
| lead, vibes | 0.212 | **0.144-0.147** (about -0.066, -31%) |

Whole songs with the vibes lead (`--forced`): seed 4 0.545 to 0.541 forced, seed 22 0.823 to **0.809**, seed 73
0.704 to 0.701. The lead plays sparsely, so a song gains little. The heaviest seed (20) has no vibes and is unchanged.

Flash: Docker hung, so this is **estimated** from host arm64 -Os objects (text, literals, const and data; no bss)
against b9653f4: **-15,568 B**. samples_data.o drops 18,592 B (the zones' 18,416 B of ADPCM and their table),
modal.o adds 2,464 B and voices.o adds 560 B. patch.o is unchanged, since its table is sized by the instrument count.
For the running total, the same measure from Task 6's commit (9675898) to b9653f4 reads -4,537 B (Task 7, the dead
models and the fixes), so on Task 6's ~388.6 KB the image was about 384.1 KB before this change. It is now about
**368.5 KB against the 409,600 B budget: about 41 KB under (estimated)**. RAM: unchanged. The voice union keeps its
size (the organ's is the largest; the vibraphone's state is 76 B), and voices.o's bss is the same.

### Jazz-Hop hook floor: 3 pitches, a 4th or wider (2026-11-03)

- Flash +1,040 B measured (`./build.sh`, image 398,200 to 399,240 B against 01f35b8, ~10.4 KB under the 409,600 B
  budget); RAM unchanged; CPU: plan-time only (once per hook bar, a small chord-tone search), no audio-path cost.

### Lead loop: Jazz-Hop and Ambient replay a recorded phrase (2026-11-03)

- Flash **+3,648 B measured** (`./build.sh`, image 399,240 to 402,888 B against c5cd893, about 6.7 KB under the
  409,600 B budget). RAM: .data+.bss 59,584 to 59,872 B (**+288 B**, under the spec's +576 B estimate); pool
  unchanged (124,472 B). The stats and the per-bar move log (`leads_loop_stat`, `leads_loop_log`) are host and
  workbench only: `nm app/build/vnl.elf` shows neither, only `leads_loop_bar`, `_lp` and `_plays`.
- CPU: the song-engine tick, plus the loop's filter once per step (the audio path: see the 2026-11-04 loop-leftovers
  line below). `tools/tick-bench.sh 600 5 STYLE` (seeds 2-600, 5 plays each, 3 runs; Jazz-Hop is style 6, Ambient 9;
  `VNL_LEAD_LOOP=1` for on), worst tick of the non-strummed songs, on a loaded host (load about 4):

  | Style | Loop off (3 runs) | Loop on (3 runs) |
  |-------|-------------------|------------------|
  | Jazz-Hop | 26, 33, 29 us | 34, 32, 32 us |
  | Ambient | 30, 35, 35 us | 35, 35, 34 us |

  The worst is a cold first tick (seeds 2 and 3) in every run; the difference is inside the run-to-run noise (off
  alone varies 26-33 us). Worst song_start, plan_build and groove_ride stay at 1 us. Strummed songs: 13 us both ways.
- 2026-11-04, every style looping, Bossa's riff and the loop's pitch floor (23ed21a..797877d): image 402,888 to
  **404,488 B (+1,600 B)**, about 5.1 KB under the budget. RAM: the loop keeps each slot's chord and range for the
  floor (about +46 B a lead state). CPU: the floor runs once per recording (a scan of 64 steps); not re-measured.
- 2026-11-04, on by default with the stutter, octave and hook-nudge fixes (8c230af): image 404,824 B (+336 B),
  about 4.8 KB under the budget. Every song now runs the loop's replay path; CPU not re-measured on the device.
- 2026-11-04, loop-leftovers (final review M1, M2, M4): the loop's filter is computed once per step, after the tick
  (it was twice, before it). Image 404,824 to **404,840 B (+16 B)**, 4,760 B under the budget. Audio path:
  `tools/tick-bench.sh 600 5` (loop at its default, on) now times `song_lead_lp_at` per step (each step's 200 calls in
  a batch, the host clock being 1 us): worst **0.07 us**, mean 0.010 us over 520,576 steps (3 runs; host, -O2, load
  about 4.8), against a worst tick of 27-28 us in the same runs: about 0.3% of the worst tick, now paid once a step.
  The batch runs warm, so a cold call may cost more; not measured on the device. Stack: `widen`'s counts are bytes
  (`uint8_t cnt[128]`, 64 at most), so `leads_loop_bar`'s frame (widen and stutter still inlined) is 472 B plus 52 B of
  saved registers, 524 B, from 908 B (app/build/vnl.dis).

### Strum: the guitars only strum; fill bars vary, step back or drive (2026-11-04)

- Flash: image **407,240 B** (`./build.sh`), 2,360 B under the 409,600 B budget; RAM .data+.bss 59,936 B, pool
  124,472 B. The fill figures are four 17-byte strings and two pointer tables; the guitar weight tables went.
- CPU: `tools/tick-bench.sh 600 5` style runs (host, -O2, load about 3-4), worst tick against 4367dfc: strummed songs
  24 us (99 songs; every guitar song strums now) against 23 us (20 songs), the rest 27 against 26 us: inside the
  noise. The pad's keys check reads up to 6 notes, not 4 (a strummed song's whole shape), once per chord.

### Keys retired: the Wurli and the DX electric piano (2026-11-04)

- Flash: image 407,192 to **405,464 B (-1,728 B)** (`./build.sh`, against 067cfc0), 4,136 B under the 409,600 B
  budget. Gone: the two patches, the Wurli's bark in M_EP (its state, its note-on setup and its per-sample branch),
  the seven sets' rows and strings, the DX's comping-style and register cases. RAM .data+.bss 59,936 B and the pool
  124,472 B unchanged (M_EP's state is not the voice union's largest member).
- CPU: the Rhodes and the felt piano render bit-identical (whole-mix hashes of five songs, sets 1, 4, 6, 7 and 11,
  against 067cfc0), so their rows do not move; the EP's per-sample bark test is gone (not measurable). The `--parts`
  bench has no Wurli or DX row now. Songs that rolled a retired set roll another, so per-seed ratios move with their
  new sets (`--forced`, host, 2 runs each: seed 7 0.857 to 0.804-0.830, 100 0.811-0.816 to 0.834-0.861, 128 0.800 to
  0.886-0.915, 183 0.813-0.815 to 0.798-0.826). The heaviest of seeds 1-200 is still seed 20 (its set unchanged), 0.941
  forced against 0.938-0.947 before.

### The vibes on the keys (2026-11-05)

- Flash: image 379,288 to **381,160 B (+1,872 B)** (`./build.sh` against b3788ef, both on today's base/ with its
  uncommitted font packing; the Wurli/DX entry's 405,464 B is the same tree on the committed fonts, so about 407,336 B
  there, 2,264 B under the 409,600 B budget). The four sets' rows and strings, the keys' vibes patch, the motor in
  M_MODAL, the vibes' voicing, comping and register cases, the pad's raw voicing call, the strum's kick twin. RAM
  .data+.bss 59,936 B and the pool 124,472 B unchanged (the motor's three floats fit the voice union).
- CPU (`bench --parts`, one run, AMB-1 158.5 ms): the vibes on the keys with the motor **0.238**, under the Rhodes'
  0.262 and the felt piano's 0.258 in the same run (the motor is a sine per voice per control step). They take the
  keys' place in their sets, so no song gets heavier.

### Transport: boot stopped, continue after a stop, SEQ for a new song (2026-11-06)

- Flash: image 386,008 to **386,392 B (+384 B)** (`./build.sh`, against 4b47df8). RAM .data+.bss 60,656 to
  **62,000 B (+1,344 B)**: the song as it was at its bar's start (a second `song_t`), copied at each bar's first 16th so
  a stop can go back to it. Pool 124,472 B unchanged.
- CPU: one `song_t` copy (about 1.3 KB) a bar in the render; not measurable.

### The six modes back: the move to major or minor undone (2026-11-06)

- Flash: image 388,120 to **388,040 B (-80 B)** (`./build.sh`, against b8d2a8e): the four split progressions (45-48)
  and the scale over a chord go; the six modes' tables and the per-mode key names come back. RAM .data+.bss 62,112 B
  and the pool 124,472 B unchanged.
- CPU (`tools/tick-bench.sh 1000 3`, host): worst tick 28.0 us (others) and 30.0 us (strummed), against 27.0 and 32.0
  us with major or minor (other songs: noise). The mode's scale is a table lookup again; not measurable.

### Optimist's BOOMBAP, JAZZ and PHONK kits: Boom-bap, Jazz and Phonk machine (2026-11-10)

Three more kits on the Crunchy machine's engine (Optimist's `drum_synth.c`, unchanged): data only, 40 more sounds in
its packed table (`felucca_drumkits.h`, 12 B a sound) and 22 rare rows, each kit's level a float (`wb_drums.c` OPT_G).

- Flash: image 293,608 to **294,460 B (+852 B for the three; about 280 B a kit)** (`./build.sh`, against bb1c2f7). RAM
  .data+.bss 78,548 B and the pool 304,956 B unchanged: the voices are the engine's fixed pool (`wb_opt.c`, 8), shared.
  The build's checks pass (no 64-bit multiply beside a load, no soft-double): no new code.
- CPU (`bench --parts`, the shipped kits' rows: every lane, over Brushes; two runs, AMB-1 140-142 ms): Crunchy machine
  **0.196 / 0.200**, Boom-bap **0.176 / 0.181**, Jazz **0.203 / 0.213**, Phonk **0.149 / 0.152** (808 0.223-0.229,
  CR-78 0.342-0.357). Jazz costs about Crunchy's (its cymbal hats ring longer); the others less.
- In a song (`VNL_KIT=<row> bench --forced 20`, the heaviest roll, one run each on a busy host, load 4): Crunchy
  1.004 / 1.027 forced (again: 0.993 / 1.017), Boom-bap 0.986 / 1.012, Jazz 0.990 / 1.010, Phonk 0.942 / 0.957: none
  heavier than Crunchy's.

### Cut-A-Dor's drum programming: the pocket, kick phrases and hat figures (2026-11-10)

Sequencing only (no new voice or sound): per-drum timing in `groove_step`, and `lib_song_hits` reading the song's kick
phrase or hat figure where the parts read the pattern's lanes. Float timing math, integer table lookups; no doubles, no
heap.

- Flash: image 294,460 to **295,500 B (+1,040 B)** (`./build.sh`, against 6032be4): the nine kick phrases (4 B and a
  status byte each), the eight hat bars (16 B), the draws and the kick row's choices. RAM .data+.bss 78,548 B and the
  pool 304,956 B unchanged. The build's checks pass (no 64-bit multiply beside a load, no soft-double).
- CPU (`tools/tick-bench.sh 1000 3`, host, old and new built side by side, two runs each): worst tick 13.0 to 14.0 us
  (others), 19.0 to 19.0 us (strummed); the mean 16th 0.16 to 0.17 us, the downbeat 0.46 to 0.50 us. `groove_step`
  alone (300 songs, 64 bars): 0.150 to 0.179 us a 16th. About 0.01-0.03 us a step, a few times a second: not
  measurable in the audio.

### The sample bank trimmed: the mixed kits' hits and Dusty go (2026-11-10)

The 15 drum hits the five mixed sampled kits (KIT_S_*, retired, now removed) played, and with them `skits.c` (their
voices, buses and half-rate path), left the bank; so did Dusty's pieces (the Gogodze set, retired: its index kept,
empty). The KIT_S_* ids stay as a sampled kit's song's mark (the ride section, the hats' eighths) under the shipped
Room kits, whose lanes play every hit; their own hits are now silent.

- Bank (`tools/samples/pack.py`): payload 386,114 to **272,064 B (-114,050)**: the hits 46,800 B, Dusty's pieces
  67,250 B. Everything now fits the first area (272,080 B with the head, of 299,008); the second area is empty, so
  121,136 B are free in all. Every kept zone (the Knight, nylon and Emilyguitar banks, Room - big's and Room -
  brushes' pieces) has the same table fields and ADPCM bytes as before, and the songs scenario renders bit for bit
  the same on both Room kits.
- Flash: image 295,500 to **288,812 B (-6,688)**; RAM .data+.bss 78,548 to **74,660 B (-3,888: the hits' voices and
  half-band history)**; the pool 304,956 B unchanged (`./build.sh`, against 2aadd63).
- CPU: none on the shipped kits (their hits never reached `skits.c`); `drums_render` no longer runs the empty
  sampled-voice pass each block.

### Lead motifs start on beat 1 more often; restatements flip their start (2026-11-10)

Weight tables, two '60s Jazz rhythm rows, Impressionism's arabesque from the downbeat and one draw per restatement
(`leads_flip`). Integer math; no doubles, no heap. (The Soft brushes snare before it, 5cdc08f: new table values
only, nothing to measure.)

- Flash: image 288,812 to **289,852 B (+1,040 B)** (`./build.sh`, against 5cdc08f). RAM .data+.bss 74,660 B and the
  pool 304,956 B unchanged.
- CPU (`tick_bench 1000 3`, host, old and new built side by side): worst tick 14.0 to 14.0 us (others), 25.0 to
  21.0 us (strummed: noise); the mean 16th 0.16 to 0.16 us, the downbeat 0.47 to 0.47 us.

### The jazz guitar: a sampled lead on trial (2026-11-10)

A candidate lead (INS_JAZZ_GTR, workbench-only until approved): the Karoryfer Shinyguitar archtop's magnetic pickup,
ten notes A3-C6 (`vl2`, `rr1`), shaped offline by `tools/samples/tone.py` (tone knob, light tube, cabinet) and packed
at 12 kHz. It plays on the sampler every sampled instrument uses (M_SAMP); no new code runs per sample, no doubles, no
heap.

- Bank (`tools/samples/pack.py`): payload 272,064 to **314,054 B (+41,990)** (Sam's re-amp in its place, the same
  day: its cut 0.55 s plus a 0.15 s loop, **314,046 B (+41,982), 79,154 B free**), laid out after the kits
  (`after_kits`): the old payload is the new one's first 272,064 bytes, byte for byte, so every other zone keeps its
  offset, table fields and ADPCM. Area 0 297,270 of 299,008 B, area 1 16,800 of 94,208 B: **79,146 B free**.
- Flash: image 289,852 to **290,316 B (+464)**: the zone table, the patch, its name. RAM .data+.bss 74,660 B and the
  pool 304,956 B unchanged (`./build.sh`, against 90efddd).
- CPU (`tools/cost-bench.sh --parts`, host, lead rows): jazz guitar 28.1 ms net, ratio 0.207 (the Knight 0.225, the
  flute 0.238, the felt piano 0.176, the vibes 0.151): a 12 kHz zone decodes fewer ADPCM samples a second than the
  Knight's 16 kHz.

### The jazz guitar an octave low, cleaner and with chorus (2026-11-10)

A low lead (`lib_lead_low`: the jazz guitar only): its style centers and ceiling an octave down, the keys an octave
down under it where the workbench leaves their register as rolled, the pad's chord part an octave down; its patch's
chorus (0.5, none before on the lead) its send into the keys' chorus, its dry scaled for it. The bank's re-amp zones at
16 kHz with their ADPCM codes found by a beam search (`pack.py`'s `adpcm_search`: the same format and decoder), 4-5 dB
less noise between the harmonics on the upper notes (the fuzz Sam heard was the 12 kHz encoding's, not his take).

- Bank: the jazz guitar 41,982 to **55,976 B (+13,994)**; payload **328,040 B**, area 0 294,469 of 299,008 B, area 1
  33,587 of 94,208 B: **65,160 B free**. Every other zone unchanged (the old payload the new one's prefix).
- Flash: image 290,316 to **290,384 B (+68)**; RAM .data+.bss 74,660 to **74,676 B (+16)**; the pool unchanged.
- CPU (`tools/cost-bench.sh --parts`, lead rows, a busier host run: every row higher): jazz guitar ratio 0.199 (the
  Knight 0.250, the flute 0.239): a multiply and an add a sample for the chorus send, only under a low lead.

The same day, Sam: "the other instruments should have gone up an octave instead of down": over the low lead the keys
an octave up (the workbench's keys-octave path, its guard for a lead on top passed by) and the pad's chord part an
octave up (67-84); the lead never pushed over the keys, its ceiling not raised with them. Image 290,384 to
**290,448 B (+64)**; RAM unchanged.

Then (Sam: "in their normal octave while the lead isn't playing, then jump up when the lead does"; "I also still hear
some buzzing"): the lift by the plan's lead blocks (harmony_keys_lift, from the plan song_tick names), and the re-amp
zones at 22.05 kHz, still searched: the ADPCM noise on F4 and up 3.5-4.5 dB lower again (the take is 15-20 dB
cleaner than the decode there).

- Bank: the jazz guitar 55,976 to **77,138 B (+21,162)**; payload **349,202 B**, area 0 295,231 of 299,008 B, area 1
  53,987 of 94,208 B: **43,998 B free**. Every other zone unchanged.
- Flash: image 290,448 to **290,752 B (+304)**; RAM unchanged.
