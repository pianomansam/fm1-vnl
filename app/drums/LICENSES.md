# The drums' licenses

The drum lanes' own code here (`wb_drums.c`, `wb_drums.h`, `wb_opt.c`, `wb_opt.h`) is VNL-1's: GPL-3.0-or-later. It
builds with two third-party drum engines, each kept under its own license, into both the firmware and the workbench's
wasm. Because the FOMAJAM and Optimist files are GPL-3.0-only, the firmware binary (`app/build/fm1/`) and the
workbench's wasm (`app/build/workbench/wb.wasm`) are each, as a whole, GPL-3.0-only; VNL-1's own source stays GPL-3.0-or-later.

| Directory | From | License | Files |
| --- | --- | --- | --- |
| `fomajam/` | [FOMAJAM](https://github.com/konsumer/fomajam) 138c5d9, `src/drums.c`, `src/drums.h`, `src/gains.h` (and its `platform/common.c` sine), by FOMAJAM's authors (its `LICENSING.md`); its modeled machines are ported from trommelsynthesizer in [konsumer/poketrack](https://github.com/konsumer/poketrack) by David Konsumer | GPL-3.0-only (`fomajam/LICENSE`); the upstream's zlib notice in `fomajam/LICENSE.poketrack` | `fj_drums.c`: altered from `drums.c` (the modeled machines only, a voice a slot, the knobs at the middle, float out, a noise generator a slot, the 64-bit multiplies in helpers kept out of line on the FM-1), marked so in the file. `fj_tables.h`: `drums.h` trimmed to what they use, plus `tables.h`'s `SEMI`. `gains.h`: its include renamed. `fj_drums.h`: the interface (its enums from `drums.h`). The ROM recordings (`roms.h`, from tidal-drum-machines, no license) are not taken |
| `cpuguard.c`, `cpuguard.h` | [w0ts/optimist](https://github.com/w0ts/optimist) 8a91efd, `firmware/src/system/cpuguard.c`, `cpuguard.h` (after Flowstate's guard by Zakaria Chowdhury) | GPL-3.0-only (`optimist/LICENSE`) | altered for VNL-1: its control (ceiling, hold, release counting each level's saving, the reactive count of late blocks, the prediction from what started) kept; our ladder (the drum lanes' tails, the bed, the plate, the voices' release tails, the keys thinned) and a learned cost a voice in place of its engines' cost table; recovery after 20 s of calm; marked so in the file |
| `optimist/` | [w0ts/optimist](https://github.com/w0ts/optimist) 915fd8a: Leo Kuroshita (@kurogedelic), Hügelton Instruments, and the Optimist contributors | GPL-3.0-only (`optimist/LICENSE`) | `drum_synth.c` (`firmware/src/drums/synth/`): copied unaltered. `ds_deps.h`: the parts of Optimist's firmware it calls, copied (each from the file it names). `felucca_tables.h`, `felucca_drumkits.h`: Optimist's generated tables (`tools/gen_tables.py`, `tools/gen_drumkits.py`), trimmed to four kits (LO-FI; BOOMBAP, JAZZ and PHONK added 2026-11-10, encoded from `gen_drumkits.py`'s recipes and `tools/drumkit_levels.json` as Optimist's generator does, in its packing over every kit) |

FOMAJAM's source files carry no SPDX header or copyright line of their own (its `LICENSE` and `LICENSING.md` cover
them); ours say GPL-3.0-only and where they came from, and the zlib notice of the trommelsynthesizer they port stays
beside them (`fomajam/LICENSE.poketrack`), the alterations marked as it asks. FOMAJAM also has the 909 and the SDS-V
(`fj_drums.h` `M_909`, `M_SDS`), not used yet. Each Optimist file keeps its `SPDX-License-Identifier: GPL-3.0-only`
header and its copyright line.
