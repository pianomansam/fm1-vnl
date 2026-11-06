#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# VNL-1's tests: the song engine's unit tests, the host simulator build and its scenarios, the browser emulator build.
# Adapted from AMB-1's runner (MPL-2.0). Outputs go under app/build/tests.
#   tests/run_tests.sh
set -u
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
PATH="$PWD/.venv/bin:$PATH"
export PATH
B=app/build
OUT=$B/tests
FAIL=0
mkdir -p "$OUT"
run() {
    name="$1"; shift
    if "$@" > "$OUT/$name.log" 2>&1; then
        echo "  ok   $name"
    else
        echo "  FAIL $name (see $OUT/$name.log)"
        tail -15 "$OUT/$name.log" | sed 's/^/       /'
        FAIL=1
    fi
}
run song sh -c "$CC -O2 -std=c99 -ffp-contract=off -Wall -Wextra -Wdouble-promotion -Werror -Iapp/song -Iapp/engine -Ibase/include \
    -o $OUT/song_test tests/host/song_test.c app/song/*.c app/song/leads/*.c app/engine/*.c && $OUT/song_test"
run clash sh -c "$CC -O2 -std=c99 -ffp-contract=off -Wall -Wextra -Wdouble-promotion -Werror -Iapp/song -Iapp/engine -Ibase/include \
    -o $OUT/clash_test tests/host/clash_test.c app/song/*.c app/song/leads/*.c && $OUT/clash_test"
run candidates sh tests/candidates.sh       # candidates are workbench-only until approved (patterns_gen.h)
run screens-export node workbench/tools/export_screens.mjs --check   # ui/screens.json matches screens.js, in ui/vocab.json's words
run screens-gen python3 -I tools/screens/gen.py --check           # app/ui's *_gen.c/.h match the workbench's data
run screens-gen-test python3 -I tools/screens/test_gen.py
run host-build sh build.sh --host
mkdir -p $B/scenarios
for f in tests/scenarios/*.vnl; do      # the app in the host simulator; outputs (songs.wav, shots) in app/build/scenarios
    name=$(basename "$f" .vnl)         # its song log, MIDI and MusicXML beside its outputs (songs.log, songs-1.mid ... go with songs.wav)
    run "scenario-$name" env VNL_SCORE=$B/scenarios/$name $B/host/vnl_host "$f" $B/scenarios
done
if command -v emcc >/dev/null 2>&1; then
    run emu sh build.sh --web
    run smoke-candidates sh workbench/tools/candidates_smoke.sh   # the workbench with a candidate in every family
    # the workbench's device mock against the simulator's screen, pixel for pixel
    run screen-check sh -c "sh workbench/build.sh && node workbench/tools/screen_check.mjs $B/tests/screens"
else
    echo "  skip emu (no emcc)"
fi
[ $FAIL -eq 0 ] && echo "all tests passed" || echo "TESTS FAILED"
exit $FAIL
