#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Candidates are workbench-only until approved (patterns and edits, Task 4): the engine built three ways, outputs in
# app/build/tests/candidates:
#   real       the generated tables as committed: no candidate drawn; the songs' hash
#   candidate  one candidate added to every drawn family (tools/patterns/variant.py): none drawn over 1000 seeds, each
#              forced and heard, every event of 100 chained songs as with the real tables, and the pinned hashes
#              (song_test's strum_same: b852d9cb / a662a137, the voicer 0x7b61830d) unchanged
#   approved   the same entries approved: each drawn (approving moves the rolls, as the importer says)
#   sh tests/candidates.sh
set -eu
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
OUT=app/build/tests/candidates
FLAGS="-O2 -std=c99 -ffp-contract=off -Wall -Wextra -Wdouble-promotion -Werror"
rm -rf "$OUT"
mkdir -p "$OUT/real"
build() {      # $1: the build's folder, its song sources (app/song with its tables replaced, or app/song itself)
    $CC $FLAGS -I"$2" -Iapp/engine -Ibase/include -o "$1/candidates_test" tests/host/candidates_test.c "$2"/*.c app/engine/*.c
}
ct() {         # $1: the build, $2: never or drawn; its output kept in the build's out.txt
    if "$OUT/$1/candidates_test" "$2" > "$OUT/$1/out.txt"; then sed "s/^/$1: /" "$OUT/$1/out.txt"
    else sed "s/^/$1: /" "$OUT/$1/out.txt"; exit 1; fi
}
build "$OUT/real" app/song
ct real never
for v in candidate approved; do
    mkdir -p "$OUT/$v/song"
    cp app/song/*.c app/song/*.h "$OUT/$v/song/"      # the sources beside their tables (a quoted include looks there first)
    python3 -I tools/patterns/variant.py "$v" "$OUT/$v/song"
    build "$OUT/$v" "$OUT/$v/song"
done
ct candidate never
ct approved drawn
real=$(grep '^songs ' "$OUT/real/out.txt")
cand=$(grep '^songs ' "$OUT/candidate/out.txt")
appr=$(grep '^songs ' "$OUT/approved/out.txt")
[ "$real" = "$cand" ] || { echo "FAIL: a candidate moved the songs ($real, with candidates $cand)"; exit 1; }
[ "$real" != "$appr" ] || { echo "FAIL: approving the new entries left every song as it was ($appr)"; exit 1; }
echo "candidates: the songs are the same with the candidates ($cand); approved, they move ($appr)"
$CC $FLAGS -I"$OUT/candidate/song" -Iapp/engine -Ibase/include -o "$OUT/candidate/song_test" tests/host/song_test.c \
    "$OUT/candidate/song"/*.c app/engine/*.c
"$OUT/candidate/song_test" strum_same
