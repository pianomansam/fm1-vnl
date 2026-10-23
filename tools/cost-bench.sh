#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Host cost bench: 60 s of a VNL song (placeholder voices today) against 60 s of AMB-1's engine at its heaviest piece.
#   tools/cost-bench.sh [--forced] [SEED...]   no seeds: the heaviest roll among 200 (seeds 1-200, ranked by their
#   render time with the chain forced to the heaviest: dust 3, vinyl, rain; best of 2), timed plain and forced: the
#   upper bound; --forced does the same for the given seeds. The standard list (docs/cost.md):
#   --forced 22 20 7 138 100 200 (the synths plan's Task 5, from a forced sweep of seeds 1-200: 22 Rhodes and Moog,
#   the heaviest; 20 the heaviest organ set, 7 a held pad, 138 DX keys, 100 the Juno set, 200 the chip set)
# Prints: vnl <ms> amb <ms> ratio <x> (and a `forced` line per seed when forcing). Needs ref/fm1-amb (gitignored)
# and base/include/fastmath.h.
set -e
cd "$(dirname "$0")/.."
[ -f ref/fm1-amb/app/dsp/amb.c ] || { echo "cost-bench: no ref/fm1-amb"; exit 1; }
OUT=app/build/bench
mkdir -p "$OUT"
${CC:-cc} -O2 -std=gnu99 -ffp-contract=off -Wall -Wextra -DVNL_BENCH -DEN_HOST \
    -Iapp/song -Iapp/engine -Ibase/include -Iref/fm1-amb/app/dsp \
    -o "$OUT/bench" app/host/bench.c app/song/*.c app/engine/*.c ref/fm1-amb/app/dsp/amb.c
exec "$OUT/bench" "$@"
