#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Exports the engine's pattern tables (app/song/patterns_gen.c) to patterns/ (or OUT) as MIDI and MusicXML, with
# model.json and .manifest.json.
#   tools/patterns/export.sh [OUT]
set -eu
cd "$(dirname "$0")/../.."
CC="${CC:-cc}"
B=app/build/patterns
OUT="${1:-patterns}"
mkdir -p "$B"
$CC -O1 -std=c99 -Wall -Wextra -Werror -Iapp/song -Iapp/engine -Ibase/include -o "$B/dump" \
    tools/patterns/dump.c app/song/patterns_gen.c
"$B/dump" > "$B/tables.json"
python3 -I tools/patterns/export.py "$B/tables.json" "$OUT"
