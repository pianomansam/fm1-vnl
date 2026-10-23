# Open-source logue user units: survey for VNL-1

Researched 2026-10-23 via `gh` (repo search and API; license read from the LICENSE file, GitHub's detected SPDX id, and READMEs) and web search. Korg's Unit Index page was not retrieved. Code search was blocked by the GitHub search rate limit, so coverage of obscure repos is partial. No VNL-1 code was changed.

## Summary

- The logue community has little that VNL-1 lacks. Nearly every high-quality reverb or chorus we could reuse is either (a) a thin wrapper around Mutable Instruments DSP (MIT, same code as ref/mutable-dreams), or (b) Freeverb (public domain), which is a weaker design than our Dattorro plate.
- **No logue unit beats Rings' fx or our Dattorro plate for VNL-1.** The Clouds-reverb ports (cldmnky, DanielMajid) are the Griesinger/Dattorro-topology reverb already in Rings (same author, same `fx_engine.h`), so they add nothing beyond what ref/mutable-dreams already holds.
- Reusable (OK under MPL-2.0) and plausibly useful: Freeverb (public domain, via boochow MIT port) as a cheap alternative or comparison reverb; the cldmnky/Squall Clouds ports only as examples of fx_engine on a float buffer; BSD-3 utility units (bvontob lpf/sat, DCDominguez suite) as ideas. Nothing found is a drop-in chorus, ensemble, tape, or BBD.
- Many of the interesting lo-fi and tape units (sscheidl TapeVibe/MixForge, dukesrg Crusher/Kompressor, WarmDrive, MKoussa units, pitchshifter, Rando-vibro) have **no license**: all rights reserved, ideas only.
- GPL: MKoussa/distortion, iliakole1/nts1-mkii-lab, WvTable, VoxHumana, sidlogue, and mosynthkey's AcidTouch. Ideas only.
- Platform caveat: drumlogue units run on Cortex-A7 at 48 kHz with NEON and an SDRAM buffer; prologue/minilogue xd/NTS-1 are Cortex-M4 at 48 kHz with `__sdram` buffers (NTS-1 SDRAM is small, about 1 MB total across slots). VNL-1 is 44.1 kHz, so every delay-line length and LFO rate needs rescaling.

## Table

| Unit | Type | License | Platform | Algorithm | Reusable under MPL-2.0? | Port effort |
|---|---|---|---|---|---|---|
| boochow/freeverb-logue | reverb | MIT (Freeverb itself public domain) | prologue, mnlg-xd, NTS-1 | Freeverb: 8 combs + 4 allpasses per channel, float; about 25k floats (about 100 KB) in `__sdram` | Yes | Low. Plain C++ classes, no SDK deps beyond buffer placement; rescale tunings (they assume 44.1 kHz already) |
| boochow/schroeder | reverb | MIT | logue family | Basic Schroeder, mono, no ERs | Yes | Trivial, but inferior to our plate |
| cldmnky/drumlogue-units (clouds-revfx) | reverb + granular + pitch shift | Apache-2.0 (README says MIT; LICENSE file is Apache-2.0); embeds Clouds MIT code | drumlogue | Clouds reverb (Griesinger/Dattorro topology, 4 AP + 2x(2AP+delay), LFO-modulated), lengths scaled 1.5x for 48 kHz, plus micro-granular, pitch shifter, diffuser; NEON helpers | Yes, license-wise; redundant with Rings reverb | Medium (needs eurorack/stmlib headers; NEON not portable). Value is low |
| DanielMajid/squall_reverb_drumlogue | reverb | MIT (Clouds code MIT, Gillet) | drumlogue | Clouds reverb on float buffer; 16384-float static buffer | Yes | Low, but same code as Rings/Clouds reverb we already have |
| cldmnky Elementish synth | modal synth | Apache-2.0 (Elements DSP is MIT) | drumlogue | Elements bow/blow/strike + resonator | Yes | High (heavy CPU; not lo-fi relevant beyond bell/EP-ish modal tones) |
| boochow/eurorack_drumlogue (Lillian) | osc (Braids) | MIT per LICENSE (GitHub flags NOASSERTION; Braids/stmlib are MIT) | drumlogue | Braids digital oscillator port | Yes | Medium; Braids has FM/bell/pluck-ish shapes but is fixed-point heavy |
| john-k-walton/Prologue-Oscillators | osc (Plaits-derived) | none in repo (Plaits DSP upstream is MIT, but the repo itself has no license) | prologue | Plaits engines (incl. FM, string, modal) | Only by going to upstream Plaits (MIT) | n/a, use upstream |
| DCDominguez/Nts-1-custom-effects (25 units) | modfx/delfx/revfx/osc | BSD-3 | NTS-1 mk1 | Abyss (4-line FDN), Aureole (shimmer FDN), Nebula, Capstan (wow/flutter tape delay), Bucketline, Dust (bit/sample-rate reduction), phaser/flanger variants. 2026, AI-assisted looking, minified single-file code; Capstan/Bucketline use 65536-float stereo buffers (512 KB) | Yes (BSD-3) | Low per unit, but quality unproven; useful as ideas for Dust/Capstan; buffers too large for a 300 KB pool |
| sscheidl/drumlogue_fx_units | TapeVibe, MixForge (filter/comp/sat/bitcrush), GrainDrift, GateVerb80 | none (all rights reserved) | drumlogue | Tape color (wow/flutter, hiss, dropouts), compressor, granular delay | **No, ideas only** | n/a |
| dukesrg/logue-fx | Crusher, Kompressor, Looper, Vibrator, Hyper, Gator, DelayBPM | none | NTS-1, prologue, mnlg-xd | Bitcrush/decimate, peak compressor, vibrato | **No, ideas only** | n/a |
| apiel/drumlogue_WarmDrive | saturation | none | drumlogue | n/a | No | n/a |
| ylmrx/maxisynthsvf | osc + SVF | MIT | drumlogue | Maximilian SVF | Yes | Low; poutoushaper and snh have no license |
| bvontob lpf, sat, bell, key4, dbar, aeio, shps | modfx and osc | BSD-3 | NTS-1 | LPF, saturation; Risset bell (additive); key4 polyphonic organ | Yes | Low. bell is a clean small additive bell oscillator worth a look |
| fairplay/logue-vosim | osc | BSD-3 | logue | VOSIM | Yes | Low; not lo-fi relevant |
| boochow/Waves2 | wavetable osc | MIT | prologue, mnlg-xd, NTS-1 | Wavetable | Yes | Low |
| boochow/teleconf | fx (telephone) | MIT | logue | Band-limit/telephone sim | Yes | Low; relevant to a lo-fi "radio/phone" filter idea |
| akiomik/logue-boy | osc | Apache-2.0 | logue | Game Boy-style oscillators | Yes | Low |
| mosynthkey/my-logue-sdk-units (about 47 units) | NTS-3 glitch fx, drums, osc | BSD-3 overall; AcidTouch is GPL-3.0-only | NTS-3, NTS-1 mk2, microKORG 2 | Beat repeat, DataBend, ReGrain, TapeOsc (varispeed), Karplus-Strong ring-excit, WarpsMorph | Yes except GPL parts | Low to medium; mostly sequencing/glitch, little pad/space |
| xiashj24/pluck_logue_sdk, Wint3rmute/minilogue-xd-karplus-strong | Karplus-Strong osc | none | logue | KS | No (ideas only) | n/a |
| MKoussa/distortion | modfx | GPL-3.0 | logue | Distortion | No | n/a |
| MKoussa/reverse-echo, stutter | delay | none | logue | Reverse delay | No | n/a |
| jamesdcheetham pitchshifter, rando-vibro, Lyre-1 | modfx, osc | none | NTS-1 | ring-buffer pitch shift, random vibrato | No | n/a |
| SpeakingColors supersaw/widener units | osc, fx | "other" (custom; not read in full) | mnlg-xd, drumlogue | JP-8000 supersaw, mono-safe stereo widener | Unverified; treat as no | n/a |
| letrout/logue | modfx saturation | MIT | logue | Saturation | Yes | Trivial |
| marksweiss/logue-sdk-projects | osc, filter, fx | BSD-3 | mnlg-xd | Misc | Yes | Low; small |

## Notes on top candidates

1. **Freeverb (boochow/freeverb-logue, https://github.com/boochow/freeverb-logue).** MIT wrapper around public-domain Jezar code. Float, about 25k floats of delay memory (about 100 KB), 8 combs + 4 allpasses per channel, no modulation, no predelay. It sounds more metallic and static than a Griesinger/Dattorro plate, so it is an option only if VNL-1 wants a cheaper second reverb (for example a "room" send) at about the cost of our plate's memory.
2. **Clouds reverb ports (https://github.com/cldmnky/drumlogue-units, https://github.com/DanielMajid/squall_reverb_drumlogue).** Both are MIT-derived and both depend on Mutable's `fx_engine.h`, which we already have in ref/mutable-dreams. cldmnky's version uses 16-bit buffers (`uint16_t*`) and scales lengths 1.5x for 48 kHz; Squall uses float and a 16384-word static buffer. Neither improves on the Rings reverb already on disk. Useful only as a worked example of rescaling `E::Reserve<>` lengths and LFO rates to a new sample rate (for 44.1 kHz, multiply original 32 kHz lengths by 1.378).
3. **BSD-3 NTS-1 suite (https://github.com/DCDominguez/Nts-1-custom-effects).** Capstan (wow/flutter tape delay), Dust (rate/bit reduction), Bucketline are the closest to VNL-1's lo-fi goals, but they are tiny experimental implementations. Capstan's 65536-float stereo buffers would take about 512 KB, well over our 300 KB pool. Treat as design hints; our own tape delay should use 16-bit or smaller buffers.
4. **Closest-to-lo-fi unlicensed units** (sscheidl TapeVibe/MixForge, dukesrg Crusher/Kompressor): good feature checklists (wow, flutter, hiss, dropout, saturation modes, bitcrush) but cannot be copied. Re-implement from first principles if wanted.
5. **Oscillators.** Nothing licensed and lo-fi-relevant beats existing Plaits/Braids/Elements code (MIT upstream). bvontob's Risset `bell` (BSD-3) and Karplus-Strong units (unlicensed) are the only EP/bell/pluck finds; none is an EP/Rhodes model. Official Mutable ports for logue exist (boochow Lillian; John Walton's Plaits oscillators for prologue), but go to upstream Plaits/Braids for MIT code.

## Comparison with VNL-1 options

| Option | Verdict |
|---|---|
| Our Dattorro plate (from AMB-1, MPL) | Already equal or better than any logue reverb found; keep. |
| Rings fx (reverb.h, chorus.h, ensemble.h; MIT; 16-bit delay memory) | Still the best reusable source for chorus and ensemble. The logue ecosystem has no better open ensemble or chorus. Its reverb duplicates the Clouds ports. |
| Freeverb | Only worthwhile as a cheap secondary reverb; sound quality lower. |
| Logue tape/lo-fi units | Ideas only (license or buffer size). |

## Caveats

- Licenses: GitHub "none" can still hide license text in source headers; we only checked LICENSE files and READMEs. Treat unlicensed repos as all rights reserved. cldmnky's README (MIT badge) conflicts with its Apache-2.0 LICENSE file; both are compatible with MPL-2.0, but cite Apache-2.0.
- The Clouds ports pull in Mutable's eurorack and stmlib sources as submodules, so the effective license is that upstream's (MIT, Emilie Gillet) plus the wrapper's.
- No NTS-3 or microKORG 2 effects beyond mosynthkey's collection were found with real reverb/chorus algorithms.
- Search was incomplete (rate limit; Unit Index not fetched).

## Sources

- https://github.com/korginc/logue-sdk
- https://github.com/boochow/freeverb-logue, https://github.com/boochow/schroeder, https://github.com/boochow/eurorack_drumlogue
- https://github.com/cldmnky/drumlogue-units, https://github.com/DanielMajid/squall_reverb_drumlogue
- https://github.com/DCDominguez/Nts-1-custom-effects, https://github.com/mosynthkey/my-logue-sdk-units
- https://github.com/sscheidl/drumlogue_fx_units, https://github.com/dukesrg/logue-fx, https://github.com/dukesrg/logue-osc
- https://github.com/bvontob/bell, https://github.com/bvontob/sat, https://github.com/bvontob/lpf
- https://github.com/john-k-walton/Prologue-Oscillators
