# Credits

VNL-1's code, its licenses and the third-party code it adapts are listed in the README ("Credits and licenses").
This file lists the recorded material the firmware contains.

## Samples

Every sample in `app/engine/samples_data.c` comes from a source dedicated to the public domain under
[CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/). Each source's license file was checked against the
CC0 1.0 legal code (`tools/samples/fetch.sh` checks it on every fetch). No attribution is required; it is given here
with thanks. `tools/samples/manifest.json` lists the exact files; `tools/samples/pack.py` turns them into the table.

| Source | Used for | License | URL |
| --- | --- | --- | --- |
| Versilian Community Sample Library (VCSL), Versilian Studios; the Knight upright sampled by Simon Dalzell (Ivy Audio) | Knight upright piano (`Upright Piano, Knight`, `vl2`); vibraphone (`Vibraphone`, soft mallets, `v2`: the vibes lead); rope-tension snare (low, snares off); hi-hat pedal close, closed (softest) and open; small shaker (high, fast) | CC0 1.0 (`LICENSE`) | https://github.com/sgossner/VCSL |
| FreePats Spanish Classical Guitar 2019-06-18 (SF2), by roberto@zenvoid.org | nylon guitar | CC0 1.0 (`cc0.txt`, `readme.txt`) | https://freepats.zenvoid.org/Guitar/acoustic-guitar.html |
| Karoryfer Samples (D. Smolken, Brian Wood): Emilyguitar | electric guitar (`mf`) | CC0 1.0 (`LICENSE`) | https://github.com/sfzinstruments/karoryfer.emilyguitar |
| Karoryfer Samples: Gogodze Phu Vol II | kick, sidestick, snare (center), closed and open hi-hat (retro mic) | CC0 1.0 (`LICENSE`) | https://github.com/sfzinstruments/karoryfer.gogodze-phu-vol-ii |
| Karoryfer Samples: Big Rusty Drums | kick, hi-hat foot chick and open, 22" ride | CC0 1.0 (`LICENSE`) | https://github.com/sfzinstruments/karoryfer.big-rusty-drums |
| Karoryfer Samples: Frankensnare | 20x6 poplar snare | CC0 1.0 (`license`) | https://github.com/sfzinstruments/karoryfer.frankensnare |

## Code

The firmware adapts no third-party code beyond what the README lists (the string synth and its Plaits Ensemble
went with the string and cello leads, 2026-10-29).

The workbench (`workbench/`, a local tool, not in the firmware) builds with two drum engines, credited here; their
licenses and the files taken are in `workbench/engine/LICENSES.md`:

| What | License | URL | Where |
| --- | --- | --- | --- |
| poketrack's trommelsynthesizer (synthesized TR-808 and CR-78 voices), by David Konsumer, at 2c5d4a6 | zlib (`workbench/engine/poketrack/LICENSE`) | https://github.com/konsumer/poketrack | `workbench/engine/poketrack/` |
| Optimist's synth drum engine (`drum_synth.c`) and its LO-FI kit, by Leo Kuroshita (@kurogedelic), Hügelton Instruments, and the Optimist contributors, at 915fd8a | GPL-3.0-only (`workbench/engine/optimist/LICENSE`) | https://github.com/w0ts/optimist | `workbench/engine/optimist/` |

Its sampled kits play recordings dedicated to the public domain under CC0 1.0, fetched and mixed by
`workbench/tools/prep_kits.py` (which checks each `LICENSE`) into `workbench/samples/` (git-ignored; never in the
firmware). No attribution is required; it is given with thanks:

| Source | Used for | License | URL |
| --- | --- | --- | --- |
| Karoryfer Samples: Gogodze Phu Vol II, at 69a0274 | the Gogodze kit (`Programs/Kit.sfz` as shipped: kick, snare, sidestick, closed and open hi-hat, every mic at its default mix) | CC0 1.0 (`LICENSE`) | https://github.com/sfzinstruments/karoryfer.gogodze-phu-vol-ii |
| Karoryfer Samples: Big Rusty Drums, at f07ce00 | the Big Rusty kits (`Programs/02-basic.sfz` as shipped, and with its brush maps: kick, snare, sidestick, hi-hat, 22" ride) | CC0 1.0 (`LICENSE`) | https://github.com/sfzinstruments/karoryfer.big-rusty-drums |
