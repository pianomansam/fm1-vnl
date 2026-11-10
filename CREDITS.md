# Credits

VNL-1's code, its licenses and the third-party code it adapts are listed in the README ("Credits and licenses").
This file lists the recorded material the firmware contains.

## Samples

Every sample in the sample bank (`app/engine/samples_blob.c`) comes from a source dedicated to the public domain under
[CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/). Each source's license file was checked against the
CC0 1.0 legal code (`tools/samples/fetch.sh` checks it on every fetch). No attribution is required; it is given here
with thanks. `tools/samples/manifest.json` lists the exact files; `tools/samples/pack.py` turns them into the bank.

| Source | Used for | License | URL |
| --- | --- | --- | --- |
| Versilian Community Sample Library (VCSL), Versilian Studios; the Knight upright sampled by Simon Dalzell (Ivy Audio) | Knight upright piano (`Upright Piano, Knight`, `vl2`); rope-tension snare (low, snares off); hi-hat pedal close, closed (softest) and open; small shaker (high, fast) | CC0 1.0 (`LICENSE`) | https://github.com/sgossner/VCSL |
| FreePats Spanish Classical Guitar 2019-06-18 (SF2), by roberto@zenvoid.org | nylon guitar | CC0 1.0 (`cc0.txt`, `readme.txt`) | https://freepats.zenvoid.org/Guitar/acoustic-guitar.html |
| Karoryfer Samples (D. Smolken, Brian Wood): Emilyguitar | electric guitar (`mf`) | CC0 1.0 (`LICENSE`) | https://github.com/sfzinstruments/karoryfer.emilyguitar |
| Karoryfer Samples: Gogodze Phu Vol II | kick, sidestick, snare (center), closed and open hi-hat (retro mic) | CC0 1.0 (`LICENSE`) | https://github.com/sfzinstruments/karoryfer.gogodze-phu-vol-ii |
| Karoryfer Samples: Big Rusty Drums | kick, hi-hat foot chick and open, 22" ride | CC0 1.0 (`LICENSE`) | https://github.com/sfzinstruments/karoryfer.big-rusty-drums |
| Karoryfer Samples: Frankensnare | 20x6 poplar snare | CC0 1.0 (`license`) | https://github.com/sfzinstruments/karoryfer.frankensnare |

## Code

The firmware adapts no third-party code beyond what the README lists (the string synth and its Plaits Ensemble
went with the string and cello leads, 2026-10-29). Two of its models follow GPL-3.0 FM-1 firmwares' approach and
values, with no code taken: the tonewheel organ (M_TW) after SLOOP's WHEEL engine, and the modeled vibraphone
(M_MODAL, `app/engine/modal.h`; it replaced the VCSL vibraphone sample, 2026-11-03) after Ravel_FM1's TINE marimba.

The drum lanes (`app/drums/`: the shipped kits 808, CR-78, Crunchy machine, Boom-bap machine, Jazz machine and Phonk
machine, in the firmware and the workbench) build
with two drum engines, credited here; their licenses and the files taken are in `app/drums/LICENSES.md`. Optimist's is
GPL-3.0-only, as is FOMAJAM's, so the firmware binary as a whole is GPL-3.0-only (the README's "Credits and licenses"):

| What | License | URL | Where |
| --- | --- | --- | --- |
| FOMAJAM's modeled drum machines (the 808 and CR-78; the 909 and SDS-V there, unused), integer tables of poketrack's trommelsynthesizer (by David Konsumer, zlib), by FOMAJAM's authors, at 138c5d9 | GPL-3.0-only (`app/drums/fomajam/LICENSE`; the upstream's zlib notice in `LICENSE.poketrack`) | https://github.com/konsumer/fomajam | `app/drums/fomajam/` (altered: the modeled machines only, a voice a slot) |
| Optimist's predictive CPU guard (`cpuguard.c`, after Flowstate's guard by Zakaria Chowdhury), at 8a91efd | GPL-3.0-only (`app/drums/optimist/LICENSE`) | https://github.com/w0ts/optimist | `app/drums/cpuguard.c` (altered: our ladder, a learned voice cost) |
| Optimist's synth drum engine (`drum_synth.c`) and four of its kits (LO-FI, BOOMBAP, JAZZ, PHONK: the recipes and levels of its `tools/gen_drumkits.py`), by Leo Kuroshita (@kurogedelic), Hügelton Instruments, and the Optimist contributors, at 915fd8a | GPL-3.0-only (`app/drums/optimist/LICENSE`) | https://github.com/w0ts/optimist | `app/drums/optimist/` (as is, its SPDX header and copyright kept) |

The shipped sampled kits (Dusty, Room - big, Room - brushes) play recordings dedicated to the public domain under CC0
1.0, fetched and mixed by `workbench/tools/prep_kits.py` (which checks each `LICENSE`) into `workbench/samples/`
(git-ignored), then reduced by `tools/samples/pack.py` into the sample bank (the firmware's and the workbench's). No
attribution is required; it is given with thanks:

| Source | Used for | License | URL |
| --- | --- | --- | --- |
| Karoryfer Samples: Gogodze Phu Vol II, at 69a0274 | the Gogodze kit (`Programs/Kit.sfz` as shipped: kick, snare, sidestick, closed and open hi-hat, every mic at its default mix) | CC0 1.0 (`LICENSE`) | https://github.com/sfzinstruments/karoryfer.gogodze-phu-vol-ii |
| Karoryfer Samples: Big Rusty Drums, at f07ce00 | the Big Rusty kits (`Programs/02-basic.sfz` as shipped, and with its brush maps: kick, snare, sidestick, hi-hat, 22" ride) | CC0 1.0 (`LICENSE`) | https://github.com/sfzinstruments/karoryfer.big-rusty-drums |
