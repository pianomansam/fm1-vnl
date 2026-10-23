#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Host cost bench: 60 s of a VNL song (placeholder voices today) against 60 s of AMB-1's engine at its heaviest piece.
#   tools/cost-bench.sh [SEED...]   no seeds: the heaviest roll among 200 (seeds 1-200, by measured render time)
# Prints: vnl <ms> amb <ms> ratio <x>. Needs ref/fm1-amb (gitignored) and base/include/fastmath.h.
set -e
cd "$(dirname "$0")/.."
[ -f ref/fm1-amb/app/dsp/amb.c ] || { echo "cost-bench: no ref/fm1-amb"; exit 1; }
OUT=app/build/bench
mkdir -p "$OUT"
${CC:-cc} -O2 -std=gnu99 -ffp-contract=off -Wall -Wextra -DVNL_BENCH -DEN_HOST \
    -Iapp/song -Iapp/engine -Ibase/include -Iref/fm1-amb/app/dsp \
    -o "$OUT/bench" app/host/bench.c app/song/*.c app/engine/*.c ref/fm1-amb/app/dsp/amb.c
exec "$OUT/bench" "$@"
