# The workbench engine's licenses

The workbench's own code here (`wb.c`, `wb.h`, `wb_drums.c`, `wb_drums.h`, `wb_opt.c`, `wb_opt.h`) is VNL-1's:
GPL-3.0-or-later. It builds with two third-party drum engines, each kept under its own license. Because the Optimist
files are GPL-3.0-only, the workbench's wasm build (`app/build/workbench/wb.wasm`) as a whole is GPL-3.0-only. None of
this code is in the firmware (`app/`).

| Directory | From | License | Files |
| --- | --- | --- | --- |
| `poketrack/` | [konsumer/poketrack](https://github.com/konsumer/poketrack) 2c5d4a6, `plugins/trommelsynthesizer/src/`, by David Konsumer | zlib (`poketrack/LICENSE`, the repository's notice) | `trommelsynthesizer.c`, `trommelsynthesizer.h`, `gains.h`, `samples.h`, `samples_stub.c`: copied unaltered |
| `optimist/` | [w0ts/optimist](https://github.com/w0ts/optimist) 915fd8a: Leo Kuroshita (@kurogedelic), Hügelton Instruments, and the Optimist contributors | GPL-3.0-only (`optimist/LICENSE`) | `drum_synth.c` (`firmware/src/drums/synth/`): copied unaltered. `ds_deps.h`: the parts of Optimist's firmware it calls, copied (each from the file it names). `felucca_tables.h`, `felucca_drumkits.h`: Optimist's generated tables (`tools/gen_tables.py`, `tools/gen_drumkits.py`), trimmed to what the LO-FI kit needs |

The poketrack files have no per-file notice. The repository's zlib notice in `poketrack/LICENSE` covers them, and it
must stay with them. Each Optimist file keeps its `SPDX-License-Identifier: GPL-3.0-only` header and its copyright line.
