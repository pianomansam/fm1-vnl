# The drums' licenses

The drum lanes' own code here (`wb_drums.c`, `wb_drums.h`, `wb_opt.c`, `wb_opt.h`) is VNL-1's: GPL-3.0-or-later. It
builds with two third-party drum engines, each kept under its own license, into both the firmware and the workbench's
wasm. Because the Optimist files are GPL-3.0-only, the firmware binary (`app/build/fm1/`) and the workbench's wasm
(`app/build/workbench/wb.wasm`) are each, as a whole, GPL-3.0-only; VNL-1's own source stays GPL-3.0-or-later.

| Directory | From | License | Files |
| --- | --- | --- | --- |
| `poketrack/` | [konsumer/poketrack](https://github.com/konsumer/poketrack) 2c5d4a6, `plugins/trommelsynthesizer/src/`, by David Konsumer | zlib (`poketrack/LICENSE`, the repository's notice) | `gains.h`, `samples.h`, `samples_stub.c`: copied unaltered. `trommelsynthesizer.c`, `trommelsynthesizer.h`: altered for VNL-1 (single precision with `fastmath.h`'s functions, so the FM-1, which has no double, and the workbench render the same samples; engines from a fixed pool; the plug-in's parameter list left out), marked so in the file |
| `optimist/` | [w0ts/optimist](https://github.com/w0ts/optimist) 915fd8a: Leo Kuroshita (@kurogedelic), Hügelton Instruments, and the Optimist contributors | GPL-3.0-only (`optimist/LICENSE`) | `drum_synth.c` (`firmware/src/drums/synth/`): copied unaltered. `ds_deps.h`: the parts of Optimist's firmware it calls, copied (each from the file it names). `felucca_tables.h`, `felucca_drumkits.h`: Optimist's generated tables (`tools/gen_tables.py`, `tools/gen_drumkits.py`), trimmed to what the LO-FI kit needs |

The poketrack files have no per-file notice. The repository's zlib notice in `poketrack/LICENSE` covers them, and it
must stay with them; the altered two say so at their top, as the notice asks. Each Optimist file keeps its `SPDX-License-Identifier: GPL-3.0-only` header and its copyright line.
