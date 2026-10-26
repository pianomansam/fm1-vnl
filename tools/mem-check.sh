#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Memory budget: runs ./build.sh and checks its "image N B; RAM .data+.bss N B of ...; pool N B of ..." line.
# Budget (the sound plan's Global Constraints): image <= 400 KB, RAM <= 80 KB, pool <= 300 KB. Exits 1 if over.
set -e
cd "$(dirname "$0")/.."
LOG=$(./build.sh 2>&1) || { echo "$LOG"; echo "mem-check: build failed"; exit 1; }
LINE=$(echo "$LOG" | grep -E '^ *ok +image [0-9]' | tail -1)
[ -n "$LINE" ] || { echo "$LOG" | tail -5; echo "mem-check: no image line in the build output"; exit 1; }
echo "$LINE" | awk '
{ for (i = 1; i <= NF; i++) { if ($i == "image") img = $(i+1); if ($i == ".data+.bss") ram = $(i+1); if ($i == "pool") pool = $(i+1) } }
END {
    bad = 0
    printf "image %d B (budget 409600), RAM %d B (budget 81920), pool %d B (budget 307200)\n", img, ram, pool
    if (img == "" || ram == "" || pool == "") { print "mem-check: could not parse"; exit 1 }
    if (img > 409600) { print "OVER: image"; bad = 1 }
    if (ram > 81920) { print "OVER: RAM"; bad = 1 }
    if (pool > 307200) { print "OVER: pool"; bad = 1 }
    if (!bad) print "mem-check: ok"
    exit bad
}'
