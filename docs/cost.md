# CPU cost ledger

What the song engine costs to run, per task and per component, measured on the host at commit 16e24ba.

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
