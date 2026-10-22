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
    -o $OUT/song_test tests/host/song_test.c app/song/*.c app/engine/*.c && $OUT/song_test"
run clash sh -c "$CC -O2 -std=c99 -ffp-contract=off -Wall -Wextra -Wdouble-promotion -Werror -Iapp/song -Iapp/engine -Ibase/include \
    -o $OUT/clash_test tests/host/clash_test.c app/song/*.c && $OUT/clash_test"
run host-build sh build.sh --host
mkdir -p $B/scenarios
for f in tests/scenarios/*.vnl; do      # the app in the host simulator; outputs (songs.wav, shots) in app/build/scenarios
    run "scenario-$(basename "$f" .vnl)" $B/host/vnl_host "$f" $B/scenarios
done
if command -v emcc >/dev/null 2>&1; then
    run emu sh build.sh --web
else
    echo "  skip emu (no emcc)"
fi
[ $FAIL -eq 0 ] && echo "all tests passed" || echo "TESTS FAILED"
exit $FAIL
