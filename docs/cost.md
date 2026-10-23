# CPU cost ledger

What the song engine costs to run, per task and per component, measured on the host at commit 16e24ba; Task 7c's
moves are re-measured against 0df8ddc in [Task 7c: moves 1 to 3, measured](#task-7c-moves-1-to-3-measured).

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
- **Reproduce:** `tools/cost-bench.sh --forced 20 100 128 183` for the history rows, and the same bench built with
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
