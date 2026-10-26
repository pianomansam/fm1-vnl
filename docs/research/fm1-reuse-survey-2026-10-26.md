# FM-1 firmware reuse survey (2026-10-26)

What VNL-1 (now GPL-3.0-or-later) can lift from other FM-1 firmwares instead of building. Read with
[fm1-licenses-2026-10-25.md](fm1-licenses-2026-10-25.md) (licenses), [fm1-accompaniment-code.md](fm1-accompaniment-code.md),
[arpeggiators.md](arpeggiators.md) and [fm1-firmware-survey-2026-10-24.md](fm1-firmware-survey-2026-10-24.md), whose
"ideas only" verdicts for GPL code are now **reuse verdicts**. Not repeated here.

Clones read (shallow, 2026-10-26): `ref/{skiff,shineon,fm1-fomni,fm1-polly,Felucca,tuna,fomajam,fm1-HYP1}` and, in
`/private/tmp/claude-501/-Users-samueloltz-Documents-fm-1/dfd13d60-fb03-409f-86fe-9daa1a019026/scratchpad/reuse/`:
`optimist` 915fd8a, `sloop-fm1` d893642, `Felucca-Engines` 04ffe90, `fm1-x0x` 54194c5, `fm1-monk` 7937106,
`fm1-velocity` 8f62ce0, `fm1-mocky` facab5d, `eight-bells` 723d3e8, `april-os` 6b65734, `melodee` c14912b,
`poketrack` 2c5d4a6. Paths below are relative to those roots (`ref/...` is in the repo). Re-clone to use them.

## Read this first: three licensing and hazard notes

1. **Almost all of it is GPL-3.0-only, and VNL-1 says "or-later".** Copying a GPL-3.0-only file in is allowed, but the
   firmware you ship is then GPL-3.0-only as a whole (our own files can keep their "or-later" header). Decide once
   whether to say so in `README.md`/`LICENSE`. Copied files keep their SPDX header and copyright line.
2. **Fixed-point ports can hit the 64-bit-multiply crash.** `base/bsp/docs/CPU.md`: a 32x32->64 multiply beside 16-bit
   loads now and then corrupts a register on the FM-1 (found in fm1-radio's AAC decoder; fix is a `noinline` helper,
   about 50% slower there). Felucca, Optimist, SLOOP and Felucca-Engines are integer DSP full of such multiplies, and
   they were not written with this in mind (Optimist's emulator patch even adds `e1fe`/`e1d8` 64-bit ops). Prefer float
   code (X0X, Fomni, Monk: floating point has never shown it) or check the disassembly of anything ported.
3. **The second core cannot be counted on.** Some FM-1 units' CPU1 misreads RAM (`ref/skiff`, Optimist
   `docs/DUAL-CORE.md` section 4, Melodee#17); every firmware that uses it falls back to one core on those units. The
   budget has to hold on one core, so CPU1 is headroom, not a plan.

## Ranked table

| # | Our need | Best source (repo, path, license) | What it saves us | CPU / flash | Fit |
|---|---|---|---|---|---|
| 1 | Lead as a phrase + motif memory, A-B-A second lead | Tuna `ref/tuna/firmware/src/tuna_gen.c` (`period_of` 1209, `motif_segment` 1273, `melody_tick` 1287, `pad_counter` 1713), GPL-3.0; LOF-1 `ref/fm1-HYP1/lof1/firmware/src/dsp/amb.c` (`answer` 1479, `ghost_phrase` 1533), GPL-3.0-only | The phrase and motif logic the arpeggiator research ranked first, already debugged on the device | Control rate, a few hundred ops a bar; flash a few KB | Adapt (needs our RNG streams and rolled-chord API) |
| 2 | Piano broken-chord / arp comping | LOF-1 `amb.c` `voicing` 1225, `comp_strike` 1286, `comp_step` 1300 (case 3 = our arp); Tuna `tuna_gen.c` `pad_arpeggio` 1908, `pad_voice` 1067; Yarns arp MIT (upstream) | Onset spread/jitter, top-note accent, hold-common-tones. The 5-step climb itself is in no firmware | Negligible | Idea + adapt; we write the climb (about 60 lines) |
| 3 | More drum kits / 808-909-style voices | poketrack `plugins/trommelsynthesizer/src/trommelsynthesizer.c` (zlib), integer table form in `ref/fomajam/tools/drums.py` + `src/drums.c` (GPL-3.0-only); kit parameters in Optimist `tools/gen_drumkits.py` (GPL-3.0-only: BOOMBAP, LO-FI, JAZZ, PHONK kits) | 4 modeled machines (808, 909, SDS-V, CR-78) as a few voice types plus constants; about 20 ready kit recipes | Fomajam: 750 lines, tables; X0X 808/909 channels cost 85-467 target instr/sample (see 3 below) | Adapt (voice types) / idea (kit recipes) |
| 4 | Plan 2: worst-tick and CPU on device | Optimist `firmware/src/core/audio.c` ~205 (`audio_max_us` from TIMER4), `firmware/src/system/overload.h`, `tools/fm1_cpu.py`, `docs/CPU-GUARD.md`; Melodee `firmware/src/audio_profile.h` (per-stage spans); emulator simonjohansson/fm1-emulator (via `tools/emu.py`) | A worst-half timer and a host script; an instruction-count emulator for pre-hardware numbers | Under 100 B RAM; flash trivial | Adapt (about 30 lines) + use the emulator as a tool |
| 5 | Cheaper DSP: use CPU1 | Optimist `firmware/src/system/dual.c`, `docs/DUAL-CORE.md`, `firmware/hal/fm1_dual*.h`; fm1-base already offers `plat_cpu2_*` | The fork/join job split, the guard against units that misread RAM | Emulator: CPU0 load down 40-44%; real gain lower, unmeasured; 3.7 KB flash, 3.8 KB RAM | Adapt (pattern); headroom only |
| 6 | Cheaper DSP: shed load before dropping out | Optimist `firmware/src/system/cpuguard.c`, `cpuguard_costs.h` (from Flowstate) | Predictive guard: steps quality down (drum tails, reverb, voice cap) before a late half | +3.5 KB flash, +220 B RAM | Adapt (our `APP_OVERLOAD` is reactive) |
| 7 | Tape warble, lo-fi effects | Nothing new beyond what we have (`app/engine/fx.c` already has wow, flutter, dust, vinyl, tape); X0X `firmware/src/dsp/fxbus.c` 304-387 (wow 0.5 Hz / flutter 6.1 Hz delay), Optimist `fx/insfx/tapespit.c`, `tools/gen_hiphop_pack.py` | Per-role warble: reuse our existing wow line per role bus; Optimist's sample-prep chain recipe | A short delay line per role (about 2-4 KB RAM each) | Idea; reuse our code |
| 8 | Birds / ambient bed | None. No FM-1 firmware has birdsong. Only GM "Bird Tweet" inside Polly's Sonivox EAS set (`ref/fm1-polly/app/dsp/eas/`, Apache-2.0 upstream) | Nothing practical (EAS is 3.7 MB of source) | n/a | Synthesize ourselves |
| 9 | Plan 4: screens and on-device UI work | Mocky `firmware/` (GPL-3.0): push PNG mockups to the real screen; Skiff `ref/skiff/firmware/src/ui_layer.c`, `ui_blocks.c`, `DESIGN.md` (control-help screen); Monk `firmware/src/app/face.c` (AA path filler, no framebuffer), `inflate.c` (from fm1-radio, MPL) | Check `now-playing-v2.html` mockups on hardware before building them; flash-cheap bitmap art if wanted | Mocky is a tool, no firmware cost | Tool (Mocky) / idea (rest) |

(Cost units: "target instr/sample" are Optimist's emulator-fitted instructions per output sample at 44.1 kHz, from
`firmware/src/system/cpuguard_costs.h`. Our ratios in `docs/cost.md` are a different scale; compare orders of
magnitude only.)

## 1. Piano broken-chord and arp comping

- **We already have** held, stabs, offbeats and an eighth-note arp; they are the AMB-1/LOF-1 lineage (`comp_step` in
  LOF-1's `amb.c` is the same four cases). Nothing in any firmware does the rising root-5th-10th-voicing climb or the
  top block chord, so that part is ours to write.
- **Take from LOF-1** (`ref/fm1-HYP1/lof1/firmware/src/dsp/amb.c`): `comp_strike` (1286) spreads a chord's notes 4-49 ms
  apart with 0-4 ms jitter, the top note 8% louder and the rest 5% softer, pan spread by note. Reuse that for the
  climb's onsets and velocities (our target is velocity 50-70 up the climb, 75 on the top chord). The `chord_new`
  push rule (re-strike on a chord change) matches our "last bar of a phrase" logic.
- **Take from Tuna** (`tuna_gen.c`): `pad_arpeggio` (1908, sixteenths up and down two octaves, "Saint-Saëns") as a
  second reference for the 16th-note option; `pad_voice` (1067) keeps common tones ringing and re-strikes only the
  new ones, which suits the electric pianos and pads but not the strum.
- **MIT upstream, not these repos:** Yarns arpeggiator and its 23 rhythm masks (`pichenettes/eurorack`
  `yarns/part.cc`, `yarns/resources.cc` `lut_arpeggiator_patterns`) for the 16th/8th masks; already analyzed in
  `arpeggiators.md`.
- **Acoustic piano rule:** none of these rolls a chord bottom-up on a piano except as a "strum"; keep the Knight
  striking together and give it the climb (ledger, "To revisit").

## 2. Two-lead arrangements, arp and sequenced leads

- **Tuna is the best source for "a lead that is a phrase"** (`ref/tuna/firmware/src/tuna_gen.c`): `period_of` (1209)
  picks the question/answer period and anchor, `motif_segment` (1273) says which segment of the phrase a beat is in,
  `melody_tick` (1287) emits the notes (half-phrase rest on the 3rd/5th, step neighbor before arrival, step-only last
  bar). Its LO-FI HIP HOP style row is `TG_STYLE_G_LOFI_HIP_HOP` (line 1789, `tuna_genres.inc`). `pad_counter` (1713)
  is a consonance-scored counter-melody, the guard for a second lead. All GPL-3.0; the Marbles deja-vu port
  `firmware/src/tuna_marbles*.c` is MIT (take it from Tuna or upstream `marbles/random/random_sequence.h`).
- **LOF-1** (`amb.c`): `answer` (1479) imitates the lead's rhythm and contour (inverted 30%) in its own instrument
  and the lead's breath; `ghost_phrase` (1533) is a one- to two-bar melody with `rest_last` (the empty last beat).
  This is the "different lead in the middle" in miniature.
- **Euclid, Eight Bells** (`eight-bells/index.html`, MIT, JS): `bjorklund`/`euclid` at lines 995-1020 (about 15 lines,
  port to C); the lo-fi genre row at 1340 (bass `fixed` kicks [0,10], [0,7,10], [0,3,10], snare 4 and 12, stab k 2-4,
  lead k 3-5); melodies `callresp`, `seqnc` (motif up a degree per group), `pedal`, `turn`, `lament` at 2700-2740;
  the TIMELINES table (~1101) for tresillo/habanera/clave masks. Already summarized in the ledger; the code is
  trivial, so this is "port from the JS" not "reuse".
- **Optimist `firmware/src/seq/gen.c`** (GPL-3.0-only): EUCLID, RANDOM, PULSE and GRIDS note generators with
  REPLACE/ADD/MASK modes; and `seq/chance.c` plus the step conditions (Elektron-style A:B, FILL, `FELUCCA_COND`) in
  `seq/seq.c`. Useful for per-step conditions on an arp lane (the "deja vu without hidden state" idea). Not worth
  copying whole: it is tied to a hardware step sequencer.
- **Not useful:** SLOOP/Skiff/Felucca arps (`seq.c` `arp_*`, UP/DOWN/ORD/CHORD for held keys), Shine On's arp
  (`shn_seq.h`): they arpeggiate keys a player holds.

## 3. Tape warble, vinyl noise, lo-fi effects, birds

- **We are ahead here.** `app/engine/fx.c` has wow and flutter, dust, vinyl, rain, tape bed and kick ducking; the
  cost ledger puts the whole chain's dust/vinyl/rain/tape at 0.003-0.03 each.
- **Per-role warble (Sam's ledger item):** the other firmwares apply tape warble on one bus too. X0X's
  `firmware/src/dsp/fxbus.c` 304-387 (TAPE delay: 0.5 Hz wow plus 6.1 Hz flutter, `fx_psin` on phase accumulators) and
  Optimist's TAPE insert (`fx/insfx/tapespit.c`, after Melodee) are the same technique as ours. Reuse our own wow
  code on per-role buses (keys, pads, sustained leads) rather than copying.
- **Sample coloring recipe:** Optimist `tools/gen_hiphop_pack.py` (high-pass, warm low shelf, gentle low-pass,
  tape saturation, a little wow, then 22 kHz ADPCM for grit; GPL-3.0-only, 328 lines, Python) is a ready prep chain
  for our CC0 sample pipeline. Its CC0 sources are the same VCSL/VSCO-2 CE files we already mine.
- **Spring reverb (a lo-fi staple we lack):** Optimist `fx/spring/spring.c` (Felucca 1.0, GPL-3.0-only, 6.5 KB
  source): a low cut, 10 stretched allpasses, a damped loop, two pickups; shares a 4 KB line. Cheap, and a color
  option beside the plate.
- **Alternative reverb:** Optimist `fx/reverb/reverb_airwin.c`, Airwindows VerbTiny (MIT upstream
  `airwindows/airwindows`): sixteen delay lines, Householder mixing with no multiplies, run at 22.05 kHz. Worth a
  bench against the Rings plate (ours costs 0.075); unmeasured here.
- **Birds:** no source. Synthesize (a few swept sine chirps with random gaps); the only bird in any FM-1 firmware is
  GM program 123 in the Sonivox EAS set that Polly carries.

## 4. Drum kits, drum synthesis, bass/kick separation

- **trommelsynthesizer (poketrack `plugins/trommelsynthesizer/src/trommelsynthesizer.c`, 857 lines, zlib)** is the
  best parametric source: 808 (bridged-T resonators, six-square metallic cluster, burst clap), 909, Simmons SDS-V and
  CR-78 from "a few voice types (swept sine, snare, metallic, ping, clap, noise) with a handful of constants". It is
  `double` math with `malloc`; `ref/fomajam/tools/drums.py` + `src/drums.c` (750 lines) is the same thing as integer
  tables for the FM-1, proven on hardware. FOMAJAM is GPL-3.0-only; the upstream is zlib (keep the notice, mark
  alterations). Our `app/engine/drums.c` is 238 lines with 4 kits and 6 lanes, so this adds machines (SDS-V and CR-78
  are new colors) and 22 voices (toms, congas, clave, cowbell).
- **Do not use:** FOMAJAM's ROM recordings (TR-707, LinnDrum, DMX, SK-1 etc. from `geikha/tidal-drum-machines`, no
  license), X0X's break generator (copied from `mestela/schwung-breakbeat` "with the author's permission", no license
  file), and X0X's 909 hat/ride/crash samples (ER-99, GPL-3.0 but the recording source is unstated; Optimist flags it
  as an open question).
- **X0X 808/909 as written** (`fm1-x0x/firmware/src/dsp/drum808.c` 1773 lines, `drum909.c`, `drum909_dsp.h`; float,
  GPL-3.0-only, Open303/sc808 parts MIT): excellent but heavy. Measured per sounding channel (Optimist
  `cpuguard_costs.h`): 909 voices 84-234 target instr/sample, 808 lanes 149-467, plus 216 shared; Optimist's whole
  synthesized drum groove is 525 on top of a 153 idle mix. Our six lanes cost 0.043-0.049 on our scale. Use X0X only
  for one or two hero sounds (an 808 sub kick, a 909 clap), or as a reference for the circuit models. Float, so no
  64-bit multiply hazard.
- **Kit recipes:** Optimist `tools/gen_drumkits.py` lines 219-416: 37 kits written in musical units (Hz, ms, dB):
  BOOMBAP (241), LO-FI (250: sine kick 54 Hz with a 1.3 kHz low-pass, snare band-passed at 2.4 kHz, hats band-passed
  at 5.2 kHz), JAZZ (374: brush snare, soft kick), PHONK (258). Convert them to our `dk_t` rows (`app/engine/drums.c`
  line 28): kick f0/f1/ptau/t60, snare tone and noise, hat lp/hp. This is a table edit, not code. The synthesis model
  they target (`drums/synth/drum_synth.c`, header at lines 1-20: tone + click + noise + SVF + drive) is close to ours.
- **Plaits drums (MIT upstream):** `plaits/dsp/drums/{synthetic_bass_drum,synthetic_snare_drum,hi_hat}.cc`; Optimist's
  integer ports are `drums/synth/drum_plaits.c` (GPL, via Hortator). Take them from upstream if we want a fatter
  kick; we already use Plaits' ensemble.
- **Real hip-hop timing:** Polly's `ref/fm1-polly/styles/drums/drummer8-session1-3_hiphop_90_beat_4-4.mid` and
  `styles/09-hiphop.sty` come from the Groove MIDI Dataset (Google Magenta, **CC BY 4.0**, needs attribution); the
  dataset has many more hip-hop loops. Useful as human velocity and micro-timing curves for our swing/ghost rules.
  Polly's compile step is `tools/make_styles.py` (GPL).
- **Sampled kit playback:** nothing better than ours. Optimist's `engines/sample/eng_sample.c` and `dsp/dsp.c`
  (`ima_nibble`, `lerp16`, `smp_lp_k`) are the same IMA ADPCM design as `app/engine/sampler.c` (ours is 4-point
  Hermite, theirs linear, so ours is the richer one). The CC0 one-shot pool is in Optimist
  `assets/samples-cc0/KIT/` (16 VCSL hits, provenance in `ATTRIBUTION.txt`): the same VCSL set our drum-sample
  survey already lists; take them from `sgossner/VCSL` upstream.
- **Bass/kick separation (back burner per Sam):** if it is picked up, Skiff's `ref/skiff/firmware/src/duck.c` (65
  lines) is the template: a track ducks under another's kick notes (event-keyed, 3 ms attack, linear release, gain
  ramped through the block, applied before level/pan/sends). X0X's `dsp/master.c` PUMP does the same on the whole mix
  and records that a 3 ms rise clicks and 12 ms is right (line 51). Both are event-keyed, like ours.

## 5. Cheaper DSP

- **Second core:** see the table. The pattern worth copying is Optimist's `dual_split`: each block, assign the three
  part jobs and the drum job to the core with less measured time (longest first), each job writing its own
  accumulators; join before the buses and master; the reverb runs one block late on CPU1. Our voices are float, so
  the sum order changes the last bit (not audible). fm1-base's `plat_cpu2_*` already wraps the start sequence (all
  four implementations agree: X0X `firmware/hal/fm1_cpu1.h`, Optimist `hal/fm1_dual.h`, Melodee
  `audio_worker.h`), including the "run only the next job whose check word matches" guard for units that misread RAM.
  The emulator's 40-44% is instructions, not cycles; Optimist says the hardware gain is lower and unmeasured.
- **Predictive CPU guard** (`firmware/src/system/cpuguard.c`, 26 KB source): steps through quality (drum tails end
  sooner), unison caps, reverb off, then voice shedding, using a per-voice cost table and the last half's load; a
  reactive path counts late halves (about 12-17 a second at 41-63% load on one unit). We shed reactively via
  `APP_OVERLOAD`; the idea worth taking is the ordered ladder and the "predict from what started" step.
- **Hand-written pi32v2 kernels:** `firmware/hal/fm1_dsp_asm.h` (GPL-3.0-only, 826 lines): FM operator loops
  (`asm_fm_pure`, `asm_fm_mod`), saw accumulators (`asm_saw_acc`, `asm_saw2_acc`), an SVF low-pass (`asm_svf_lp`),
  each bit-identical to its C twin. They are **integer** and keyed to Optimist's data layout, so they do not drop
  into our float voices; the *techniques* (post-increment addressing, decrement-and-branch, `uextra` bit-field table
  index) can speed our own hot loops. Packed 16-bit SIMD (`fm1_simd.h`) is **wrong on the real FM-1** (crackles);
  avoid.
- **Shared-block lesson:** Optimist's `docs/DSP-SHARED.md` moved every duplicated primitive into `dsp/dsp.c` and
  proved it by comparing disassembly (`tools/dis_diff.py`) and golden renders. A good model if we dedupe our own.
- **Samples:** no cheaper playback found. FM1-GM and Polly show it is the flash, not the CPU, that costs (already in
  the cpu-vs-flash note).
- **Felucca-Engines (MIT, `src/`):** ANALOG, LOFI, TRIO, WHEEL, NOISE, PHASE as fixed-point engines
  (`dsp.c` 8.6 KB, engines 11-16 KB source each; `README.md` lists them). Only WHEEL (a nine-drawbar tonewheel organ
  with key click and rotary) fills a hole (the ledger lists organ as an unmined CC0 sample family); NOISE has
  random-impulse vinyl crackle but we already have dust. Fixed-point, so check for 64-bit multiplies (note 2).

## 6. Plan 2 (hardware) and Plan 4 (screens, controls)

**Plan 2: what to copy**

- **Worst-tick timer:** Optimist `core/audio.c` ~205: `us = (fm1_ticks() - t0) / 24` per audio half, kept as a
  running max and compared with the half's length (5805 us for 256 frames at 44.1 kHz). fm1-base only exposes
  `plat_cpu_pct()` and `plat_xruns()`; a running max is the missing number. `fm1_ticks()` is TIMER4, 24 MHz
  (`firmware/hal/fm1_time.h`). Optimist's `system/overload.h` adds a 5-second peak and a late-half count.
- **Per-stage spans:** Melodee `firmware/src/audio_profile.h`: inclusive TIMER4 spans for events, voices, mix, FX,
  output, so "worst tick" can say which stage. GPL-3.0-only; 60 lines.
- **Host-side reader:** Optimist `tools/fm1_cpu.py` (from SLOOP PR #45) samples a CDC console `status` twice a second
  and prints cpu min/mean/max, `audio_max_us`, voices shed, exit 1 above 85%. We use USB-MIDI, not CDC, per the spec,
  so take its parsing and report shape only.
- **Emulator before hardware:** `simonjohansson/fm1-emulator` (GPL-2.0 upstream; fork `w0ts/fm1-emulator`, GPL-3.0)
  runs a built `.fwsc` and counts instructions per half; Optimist wraps it in `tools/emu.py`. It is a tool, not linked
  code, so its license does not touch VNL-1. Instruction counts, not cycles (one per cycle assumed), and it needs a
  patch for 64-bit ops (`tests/emu/emulator-wide-ops.patch`); float code has not been checked here.
- **Flash by component:** Optimist `tools/symsize.py`, `tools/symdiff.py`, `tools/builder/measure_costs.py`,
  `tools/builder/costs.json` (per-feature flash/RAM/pool from real builds); `docs/MEMORY-MAP.md` for the budget. Our
  378 KB of 409.6 KB will need this view; it is Python over the ELF, license GPL-3.0-only, so reusable.
- **Do not repeat:** Optimist's measured numbers are emulator-only ("nothing is measured on an FM-1").

**Plan 4: what to copy**

- **Mocky (tool, `fm1-mocky`, GPL-3.0):** install, then send `docs/superpowers/specs/mockups/now-playing-v2.html`
  rendered to a PNG from the web page (RGB565, optional dither, a 276 KB gallery in flash). Checks colors and text size
  at arm's length without building any screen code. It replaces VNL-1 on the device while testing.
- **Skiff** (`ref/skiff`, `DESIGN.md`, `firmware/src/ui_layer.c`, `ui_blocks.c`, `ui_graph.c`): "a screen that
  shows what every control does", layers, a SHIFT map. Layout ideas for our four-knob pages; its code is tied to its
  six-track model.
- **Monk** (`fm1-monk/firmware/src/app/face.c`, 25 KB): a small anti-aliased path rasterizer that draws into a
  240x240 RGB565 band buffer with no framebuffer, plus `inflate.c` (raw deflate, 2.4 KB stack, from fm1-radio, which
  is MPL: take that copy). Only if we want bitmap or vector art; our screens are text and gauges.
- **Velocity (`fm1-velocity`)**: key velocity from the ADC; we dropped APP_TOUCH and the generator needs no key
  dynamics. Skip unless keys become performable.

## Per-repo verdicts

| Repo | License (file read) | Verdict |
|---|---|---|
| w0ts/optimist | GPL-3.0-only (`LICENSE`, `LICENSING.md`); bundled MIT: Plaits, Rings, DaisySP, Open303, TB3PO, sc808, airwindows, Felucca-Engines; BSD: micro-ecc, uPD933; Apache: msfa, SBC, JieLi; **GPL-2.0 reSID** (SID engine: off-limits) | The biggest quarry: dual core, CPU guard, drum kit recipes, gen_hiphop_pack, spring/VerbTiny, asm kernels, host tools. Take MIT parts from upstream. |
| SLOOP (isod89/sloop-fm1) | GPL-3.0-only; msfa Apache-2.0 | Optimist is a superset; nothing unique except 37 kit list (same as Optimist's `gen_drumkits.py`). Skip. |
| hugelton/Felucca (+ `ref/Felucca`) | GPL-3.0-only; Hügelton Sample Pack GPL since 1.0 | Base layer; `tests/cpu_baseline.txt` (instr/sample per engine) is a useful cost reference. |
| hugelton/Felucca-Engines | MIT (`LICENSE`) | Reusable freely; only WHEEL organ looks useful; fixed-point (hazard note 2). |
| charlesvestal X0X | GPL-3.0-only; break generator unlicensed (permission only), ER-99 samples unclear | 808/909 voices, `master.c` PUMP, tape delay, `fm1_cpu1.h`. Heavy; hero sounds only. |
| Fomni | GPL-3.0-only; CC0 data from Chordian (take from `Jan125/pb.chordian`) | Omnichord rhythms/strum voicing: not lo-fi. Idea only. |
| Polly | GPL-3.0-only; Sonivox EAS Apache-2.0; Groove MIDI Dataset CC BY 4.0 | Groove-dataset hip-hop MIDI for feel; EAS too big. |
| Monk | GPL-3.0-only (README: MonkSynth is "free of charge"; its own license is unstated) | Not needed (voices exist); face.c/inflate.c ideas only. |
| Velocity | GPL-3.0-only | Skip. |
| Mocky | GPL-3.0-only | Use as a tool for Plan 4. |
| Skiff (`ref/skiff`) | GPL-3.0-only; DaisySP/Rings/msfa kept their licenses; `fm1_clk320` MIT (it lowers SYS 360 to 320 MHz: not a speedup) | `duck.c`, UI layout ideas. |
| Shine On (`ref/shineon`) | GPL-3.0-only | String-machine recipes already in the ledger (formant filter on the sum, three-line ensemble); no new need. |
| Discodave-Glitch/eight-bells | MIT (`LICENSE`); browser JS, not firmware | Euclid and melody-engine ideas; port from JS. |
| beashoko/april-os | `LICENSE` says MIT but README says GPL-3.0-only (a Felucca derivative) | **Repo has no firmware source** (web pages only). Nothing to reuse. |
| Others read | Tuna GPL-3.0 (marbles files MIT); LOF-1/HYP-1 family GPL-3.0-only; FOMAJAM GPL-3.0-only (ROM recordings unlicensed); poketrack zlib; Melodee GPL-3.0-only | Tuna and LOF-1 for leads and comping, FOMAJAM/poketrack for drums, Melodee for the profiler. |

## Suggested order

1. Tuna/LOF-1 phrase, motif and answer logic for the leads (need 1, 2): highest musical payoff, no CPU.
2. Worst-half timer and a host script for Plan 2, before any voice work.
3. poketrack/FOMAJAM voice types plus Optimist kit recipes for more drum kits (cheap if the model stays our 6 lanes).
4. Prototype the second-core split behind a flag, measuring on a unit that passes the misread check.
5. Per-role warble from our own wow code; spring reverb as an optional color.
