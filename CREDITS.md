# Credits

VNL-1 stands on other people's work: a platform, code, published algorithms, ideas from other FM-1 firmwares, and
sample libraries. This file lists all of it, with thanks. Each entry names the author, the license, where it comes
from and where it lives here. The licenses themselves are summarized at the end and in the README.

## The platform

| What | Author | License | Where |
| --- | --- | --- | --- |
| [fm1-base](https://github.com/charlesvestal/fm1-base): the FM-1 platform VNL-1 is an app on (the device layer, the build, the installer, the host simulator, the browser emulator, the screen reader's firmware side, the fonts' packing). Its own credits are in `base/CREDITS.md` and `base/LICENSING.md` | Charles Vestal, with code from Leo Kuroshita, Kerem Kilic and Chris Farrell (permissions in `base/LICENSING.md`) | MPL-2.0 | `base/` (a git submodule) |
| [felucca-bsp](https://github.com/hugelton/felucca-bsp): the FM-1's board support (drivers, loader, rescue), from [Felucca](https://github.com/hugelton/Felucca) | Leo Kuroshita ([@kurogedelic](https://github.com/kurogedelic)), Hügelton Instruments | MPL-2.0 | `base/bsp/` |
| fm1-base's template: VNL-1 started from it; `app/app.c`, `app/app.h`, `app/words.tsv`, `build.sh` and `tests/run_tests.sh` keep its MPL-2.0 notice | Charles Vestal | MPL-2.0 (`LICENSES/MPL-2.0.txt`) | those files |
| The fonts, packed into the firmware's bitmaps by fm1-base: Barlow Semi Condensed (also the splash's wordmark, `app/ui/font_mark.h`) and Terminus | The Barlow Project Authors; Dimitar Toshkov Zhekov | SIL OFL 1.1 (`base/assets/fonts/`) | the firmware image |
| The JieLi AC79 SDK's boot files every FM-1 package carries (`uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin`), read from the SDK at build time | Zhuhai Jieli Technology | Apache-2.0 | `vnl-1.fwsc` |

## Code in the firmware

| What | Author | License | Where |
| --- | --- | --- | --- |
| The lo-fi chain's reverb and chorus, adapted from [Mutable Instruments Rings](https://github.com/pichenettes/eurorack) (`rings/dsp/fx/reverb.h`, `chorus.h`, `fx_engine.h`) | Emilie Gillet | MIT (the notice is in the README and the file) | `app/engine/rings_fx.c`, `rings_fx.h` |
| The plate reverb (after Dattorro's 1997 paper) and the TPT state-variable filter, adapted from [AMB-1](https://github.com/charlesvestal/fm1-amb) (`app/dsp/amb.c`) | Charles Vestal | MPL-2.0 | `app/engine/fx.c`, `app/engine/voices.c` |
| The 808 and CR-78 kits: the modeled drum machines of [FOMAJAM](https://github.com/konsumer/fomajam) at 138c5d9, altered (the modeled machines only, a voice a slot) | konsumer (David Konsumer) and FOMAJAM's authors | GPL-3.0-only (`app/drums/fomajam/LICENSE`) | `app/drums/fomajam/` |
| FOMAJAM's machines are ported from trommelsynthesizer in [poketrack](https://github.com/konsumer/poketrack) | David Konsumer | zlib (`app/drums/fomajam/LICENSE.poketrack`) | `app/drums/fomajam/` |
| The Crunchy, Boom-bap and Jazz machine kits: [Optimist](https://github.com/w0ts/optimist)'s synth drum engine (`drum_synth.c`, as is) and its LO-FI, BOOMBAP and JAZZ kit recipes, at 915fd8a | Leo Kuroshita (@kurogedelic), Hügelton Instruments, and the Optimist contributors | GPL-3.0-only (`app/drums/optimist/LICENSE`) | `app/drums/optimist/` |
| Inside Optimist's drum engine: its extra metal tunings, re-strikes and rises, after Kawa Tracker's kit engine ([Kawa-Tracker-Fm1](https://github.com/Guizmoh/Kawa-Tracker-Fm1), `kawa_drums.c`) | Guizmoh | GPL-3.0 | `app/drums/optimist/drum_synth.c` |
| The CPU guard: Optimist's predictive guard (`firmware/src/system/cpuguard.c`, at 8a91efd), altered (our ladder of savings, a learned cost a voice) | the Optimist contributors, after Flowstate's guard by Zakaria Chowdhury | GPL-3.0-only (`app/drums/optimist/LICENSE`) | `app/drums/cpuguard.c`, `cpuguard.h` |
| The drums' pocket (each drum's lo-fi timing), four of the kick phrases and the eight hat bars, adapted from [Cut-A-Dor](https://github.com/kidbee85-beep/cut-a-dor-release)'s rhythm engine (`firmware/src/dsp/cad.c`: `cad_step_tick`, `kick_a`/`kick_b`, `hats`), v1.2.1 | Kid Bunka | GPL-3.0-only | `app/song/groove.c`, `app/song/roll.c`, `patterns/kick-phrases/k7`-`k10` |

The drum engines' files, what was changed and why: `app/drums/LICENSES.md`.

## Approaches and values (no code taken)

These parts are VNL-1's own code, written after another project's approach, numbers or idea:

| What | After | Author | License there | Where |
| --- | --- | --- | --- | --- |
| The tonewheel organ (drawbars, percussion, key click, rotary) | the WHEEL engine of [SLOOP](https://github.com/isod89/sloop-fm1) | isod89 | GPL-3.0-only | `app/engine/voices.c` (`tw_*`) |
| The modeled vibraphone (a struck bar as decaying modes, a soft mallet) | the TINE marimba of [Ravel_FM1](https://github.com/De-Pattern/Ravel_FM1) | De-Pattern | GPL-3.0 | `app/engine/modal.c`, `modal.h` |
| The CPU guard's voice shedding (the quietest released voice first, a part's own note kept) | `shed_voice` in Felucca's `firmware/src/audio.c` | Leo Kuroshita | GPL-3.0-only | `app/engine/voices.c` (`vx_guard`) |
| The whole-mix stutter (a slice of the mix caught and repeated with short fades) | Cut-A-Dor's captured-slice stutter (`cad.c`) | Kid Bunka | GPL-3.0-only | `app/engine/voices.c` (`vx_stutter`) |
| The plate and the Rings reverb's topology | J. Dattorro, "Effect Design, Part 1: Reverberator and Other Filters" (JAES, 1997) | | a published paper | `app/engine/fx.c`, `rings_fx.c` |

Studied for ideas only, with no code or material taken: the FM-1 firmwares surveyed in `docs/research/` (their
screens, controls and engines), and the accompaniment and lo-fi generators in `docs/music/ideas-from-other-apps.md`
(among them Magenta's Lo-Fi Player). The drum patterns, progressions and comping rhythms are common idioms of the
style, written for VNL-1 (`patterns/`).

## Samples

Every sample VNL-1 plays comes from a source dedicated to the public domain under
[CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/). No attribution is required; it is given here with
thanks. `tools/samples/fetch.sh` checks each source's license file against CC0 1.0 on every fetch;
`tools/samples/manifest.json` lists the exact files, and `tools/samples/pack.py` turns them into the sample bank
(`app/engine/samples_blob.c`, written to the FM-1 by `tools/fm1_bank.py`) and the jazz guitar's notes into the
firmware image (`app/engine/samples_pcm.c`).

| Source | Used for | License | URL |
| --- | --- | --- | --- |
| Versilian Community Sample Library (VCSL), Versilian Studios: the Knight upright, sampled by Simon Dalzell (Ivy Audio) | Piano | CC0 1.0 | https://github.com/sgossner/VCSL |
| FreePats Spanish Classical Guitar (SF2, 2019-06-18), by roberto@zenvoid.org | Nylon guitar | CC0 1.0 | https://freepats.zenvoid.org/Guitar/acoustic-guitar.html |
| Karoryfer Samples (D. Smolken, Brian Wood): Emilyguitar | Electric guitar | CC0 1.0 | https://github.com/sfzinstruments/karoryfer.emilyguitar |
| Karoryfer Samples (D. Smolken): Shinyguitar, an archtop's pickup, re-amped by Sam through his amp and cabinet | Jazz guitar | CC0 1.0 (the source); the re-amp: VNL-1's | https://github.com/sfzinstruments/karoryfer.shinyguitar |
| Karoryfer Samples x Bigcat Instruments: the open-source cello (sustained bowing) | Cello | CC0 1.0 | https://github.com/sfzinstruments/karoryfer-bigcat.cello |
| Versilian Studios Chamber Orchestra 2 Community Edition (VSCO-2 CE): concert harp | Harp | CC0 1.0 | https://github.com/sgossner/VSCO-2-CE |
| Karoryfer Samples: Big Rusty Drums, at f07ce00 (sticks and brushes) | The Room - big and Room - brushes kits | CC0 1.0 | https://github.com/sfzinstruments/karoryfer.big-rusty-drums |

The workbench's kit tool (`workbench/tools/prep_kits.py`) also fetches Karoryfer's Gogodze Phu Vol II (CC0 1.0), the
retired Dusty kit; it is not in the sample bank and VNL-1 doesn't play it.

## Tools that don't ship in the firmware

| What | Author | License | Used for |
| --- | --- | --- | --- |
| fm1-base's tools: the installer (`fm1_install.py`), the package builder, the screen reader's words, the web installer (with code from [fm-static](https://github.com/seajaysec/fm-static) by Chris Farrell) | Charles Vestal and others (`base/CREDITS.md`) | MPL-2.0 | building and installing |
| felucca-bsp's rescue (`fm1_rescue.sh`), which fetches JieLi's flash loader and M-VAVE's stock firmware when run | Charles Vestal, Leo Kuroshita | MPL-2.0 | restoring a stuck FM-1 |
| [m-bootloader](https://github.com/jvitkauskas/m-bootloader), offered by fm1-base's web installer | Julius Vitkauskas | MIT | the web installer only |
| [eSpeak NG](https://github.com/espeak-ng/espeak-ng), through [fm1-reader](https://github.com/charlesvestal/fm1-reader) | the eSpeak NG authors; Charles Vestal | GPL-3.0-or-later | the browser emulator's voice |
| JieLi's toolchain and AC79 SDK, Emscripten, Docker, Pillow, mido, python-rtmidi, MuseScore (pattern files) | their authors | their own | the build, the tools |

## Licenses at a glance

- VNL-1's own code: GPL-3.0-or-later (`LICENSE`). Up to the tag `mpl-final` it was MPL-2.0.
- Files from fm1-base and AMB-1: MPL-2.0 (`LICENSES/MPL-2.0.txt`), which allows combining with GPL code.
- The drum engines and the CPU guard (FOMAJAM, Optimist, Cut-A-Dor's tables): GPL-3.0-only, so the firmware binary
  and the workbench's wasm are, as a whole, GPL-3.0-only.
- Rings: MIT. poketrack: zlib. The JieLi boot files: Apache-2.0. The fonts: SIL OFL 1.1. The samples: CC0 1.0.
