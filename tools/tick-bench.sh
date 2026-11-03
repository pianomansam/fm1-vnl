#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# The song engine's worst tick on the host (app/host/tick_bench.c): song_tick and song_start run in the audio
# interrupt. tools/tick-bench.sh [N [R [STYLE [COMP]]]]: seeds 2..N (3000), each song played R times (5), a tick's time
# its least; STYLE forces the lead_style override (2 + LS_*: a lead style), COMP a comping style (CS_*); VNL_LEAD_LOOP=1
# turns the lead loop on.
# Prints the worst tick and start for strummed songs and the rest, and an event hash per kind (docs/cost.md).
set -e
cd "$(dirname "$0")/.."
OUT=app/build/bench
mkdir -p "$OUT"
${CC:-cc} -O2 -std=c99 -ffp-contract=off -Wall -Wextra -Werror -DVNL_BENCH -Iapp/song -Iapp/engine -Ibase/include \
    -o "$OUT/tick_bench" app/host/tick_bench.c app/song/*.c app/song/leads/*.c app/engine/*.c
exec "$OUT/tick_bench" "$@"
