#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Host cost bench: 60 s of a VNL song (placeholder voices today) against 60 s of AMB-1's engine at its heaviest piece.
#   tools/cost-bench.sh [--forced] [SEED...]   no seeds: the heaviest roll among 200 (seeds 1-200, ranked by their
#   render time with the chain forced to the heaviest: dust 3, vinyl, rain; best of 2), timed plain and forced: the
#   upper bound; --forced does the same for the given seeds. The standard list (docs/cost.md):
#   --forced 20 7 155 138 100 22 (Sam's synth cut, from a forced sweep of seeds 1-200: 20 Rhodes and saw lead, the
#   heaviest; 7 a held pad; the heaviest of the other synth sets: 155 DX keys and FM bass, 138 DX and pluck, 100 Organ
#   and saw lead, 22 Nylon and synth pluck)
# Each song plays the kit the FM-1 would (the seeded pick; VNL_KIT=<kit row value> forces one, VNL_KITLOG=1
# names it). Prints: vnl <ms> amb <ms> ratio <x> (and a `forced` line per seed when forcing). Needs ref/fm1-amb (gitignored)
# and base/include/fastmath.h.
set -e
cd "$(dirname "$0")/.."
[ -f ref/fm1-amb/app/dsp/amb.c ] || { echo "cost-bench: no ref/fm1-amb"; exit 1; }
OUT=app/build/bench
mkdir -p "$OUT"
${CC:-cc} -O2 -std=gnu99 -ffp-contract=off -Wall -Wextra -DVNL_BENCH -DEN_HOST \
    -Iapp/song -Iapp/engine -Iapp/drums -Iapp/ui -Ibase/include -Iref/fm1-amb/app/dsp \
    -o "$OUT/bench" app/host/bench.c app/song/*.c app/song/leads/*.c app/engine/*.c app/drums/*.c \
    app/drums/fomajam/*.c app/ui/kitpick.c app/ui/choices_gen.c ref/fm1-amb/app/dsp/amb.c
exec "$OUT/bench" "$@"
