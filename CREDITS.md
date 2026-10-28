# Credits

VNL-1's code, its licenses and the third-party code it adapts are listed in the README ("Credits and licenses").
This file lists the recorded material the firmware contains, and the code adapted for the string synth.

## Samples

Every sample in `app/engine/samples_data.c` comes from a source dedicated to the public domain under
[CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/). Each source's license file was checked against the
CC0 1.0 legal code (`tools/samples/fetch.sh` checks it on every fetch). No attribution is required; it is given here
with thanks. `tools/samples/manifest.json` lists the exact files; `tools/samples/pack.py` turns them into the table.

| Source | Used for | License | URL |
| --- | --- | --- | --- |
| Versilian Community Sample Library (VCSL), Versilian Studios; the Knight upright sampled by Simon Dalzell (Ivy Audio) | Knight upright piano (`Upright Piano, Knight`, `vl2`); rope-tension snare (low, snares off); hi-hat pedal close, closed (softest) and open; small shaker (high, fast) | CC0 1.0 (`LICENSE`) | https://github.com/sgossner/VCSL |
| FreePats Spanish Classical Guitar 2019-06-18 (SF2), by roberto@zenvoid.org | nylon guitar | CC0 1.0 (`cc0.txt`, `readme.txt`) | https://freepats.zenvoid.org/Guitar/acoustic-guitar.html |
| Karoryfer Samples (D. Smolken, Brian Wood): Emilyguitar | electric guitar (`mf`) | CC0 1.0 (`LICENSE`) | https://github.com/sfzinstruments/karoryfer.emilyguitar |
| Karoryfer Samples: Gogodze Phu Vol II | kick, sidestick, snare (center), closed and open hi-hat (retro mic) | CC0 1.0 (`LICENSE`) | https://github.com/sfzinstruments/karoryfer.gogodze-phu-vol-ii |
| Karoryfer Samples: Big Rusty Drums | kick, hi-hat foot chick and open, 22" ride | CC0 1.0 (`LICENSE`) | https://github.com/sfzinstruments/karoryfer.big-rusty-drums |
| Karoryfer Samples: Frankensnare | 20x6 poplar snare | CC0 1.0 (`license`) | https://github.com/sfzinstruments/karoryfer.frankensnare |

## Code

| What | License | URL | Where |
| --- | --- | --- | --- |
| The Ensemble (string-machine chorus) of [Mutable Instruments Plaits](https://github.com/pichenettes/eurorack), `plaits/dsp/fx/ensemble.h`, by Emilie Gillet; adapted mono, its delays rescaled, for the string synth (via the instrument spike's port) | MIT (below; verified in the file's own header) | https://github.com/pichenettes/eurorack/blob/master/plaits/dsp/fx/ensemble.h | `app/engine/strsyn.c`, `strsyn.h` |

The workbench (`workbench/`, a local tool, not in the firmware) builds with two drum engines, credited here; their
licenses and the files taken are in `workbench/engine/LICENSES.md`:

| What | License | URL | Where |
| --- | --- | --- | --- |
| poketrack's trommelsynthesizer (synthesized TR-808 and CR-78 voices), by David Konsumer, at 2c5d4a6 | zlib (`workbench/engine/poketrack/LICENSE`) | https://github.com/konsumer/poketrack | `workbench/engine/poketrack/` |
| Optimist's synth drum engine (`drum_synth.c`) and its LO-FI kit, by Leo Kuroshita (@kurogedelic), Hügelton Instruments, and the Optimist contributors, at 915fd8a | GPL-3.0-only (`workbench/engine/optimist/LICENSE`) | https://github.com/w0ts/optimist | `workbench/engine/optimist/` |

The MIT notice for the Plaits code, which the firmware contains:

    Copyright 2014 Emilie Gillet.

    Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
    documentation files (the "Software"), to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and
    to permit persons to whom the Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all copies or substantial portions of
    the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
    CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.
