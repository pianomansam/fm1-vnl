#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Exports the engine's pattern tables to patterns/ (or OUT) as MIDI and MusicXML, with model.json and .manifest.json.
#   tools/patterns/export.sh [OUT]
set -eu
cd "$(dirname "$0")/../.."
CC="${CC:-cc}"
B=app/build/patterns
OUT="${1:-patterns}"
mkdir -p "$B"
FLAGS="-O1 -std=c99 -ffp-contract=off -Wall -Wextra -Werror -Iapp/song -Iapp/engine -Ibase/include"
SRC=""
for f in app/song/*.c; do
    case "$f" in app/song/strum.c|app/song/lead.c|app/song/melody.c) ;; *) SRC="$SRC $f" ;; esac
done
$CC $FLAGS -DDUMP_PART=1 -c tools/patterns/dump.c -o "$B/dump_strum.o"
$CC $FLAGS -DDUMP_PART=2 -c tools/patterns/dump.c -o "$B/dump_lead.o"
$CC $FLAGS -DDUMP_PART=3 -c tools/patterns/dump.c -o "$B/dump_melody.o"
# shellcheck disable=SC2086
$CC $FLAGS -o "$B/dump" tools/patterns/dump.c "$B/dump_strum.o" "$B/dump_lead.o" "$B/dump_melody.o" $SRC -lm
"$B/dump" > "$B/tables.json"
python3 -I tools/patterns/export.py "$B/tables.json" "$OUT"
