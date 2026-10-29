#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# The workbench's smoke test over tables with a candidate in every family, imported as Sam would add them (a new .mid
# in a copy of patterns/: tools/patterns/variant.py files), so a candidate in patterns/ never breaks the smoke test.
# Everything in app/build/tests/candidates-wb; patterns/, app/song and workbench/ are only read.
#   sh workbench/tools/candidates_smoke.sh      (tests/run_tests.sh runs it when emcc is there)
set -eu
cd "$(dirname "$0")/../.."
T=app/build/tests/candidates-wb
rm -rf "$T"
mkdir -p "$T/song"
cp -R patterns "$T/patterns"
rm -f "$T/patterns/status.json"
cp app/song/*.c app/song/*.h "$T/song/"
cp -R app/song/leads "$T/song/"
cp workbench/options.json "$T/options.json"
python3 -I tools/patterns/variant.py files "$T/patterns"
python3 -I tools/patterns/import.py --patterns "$T/patterns" --gen "$T/song" --options "$T/options.json" > "$T/import.out"
grep -c ", a candidate$" "$T/import.out" | sed 's/^/candidates imported: /'
WB_SONG="$T/song" WB_OUT="$T/build" sh workbench/build.sh
WB_WASM="$T/build/wb.wasm" WB_OPTIONS="$T/options.json" WB_GEN="$T/song/patterns_gen.c" node workbench/tools/smoke.mjs
