#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 pianomansam (VNL-1's changes)
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
run ui sh -c "$CC -O2 -std=c99 -ffp-contract=off -Wall -Wextra -Werror -DPLAT_HOST -Iapp/ui -Iapp/song -Iapp/engine \
    -Ibase/include -Itests/host -o $OUT/ui_test tests/host/ui_test.c app/ui/nav.c app/ui/values.c app/ui/kitpick.c app/ui/screens_gen.c \
    app/ui/choices_gen.c app/song/*.c app/song/leads/*.c app/engine/*.c -lm && $OUT/ui_test"   # the screens' runtime
run candidates sh tests/candidates.sh       # candidates are workbench-only until approved (patterns_gen.h)
run kits-native sh -c "$CC -O2 -std=gnu99 -ffp-contract=off -Wall -Wextra -Wno-unused-parameter -Werror -Iapp/song \
    -Iapp/engine -Iapp/drums -Iapp/ui -Ibase/include -Iworkbench/engine -o $OUT/wb_native tests/host/wb_native.c \
    app/song/*.c app/song/leads/*.c app/engine/*.c app/drums/*.c app/drums/fomajam/*.c app/ui/kitpick.c \
    app/ui/choices_gen.c workbench/engine/*.c && sh workbench/build.sh && node workbench/tools/kits_native.mjs $OUT/wb_native"
run cpuguard sh -c "$CC -O2 -std=gnu99 -Wall -Wextra -Werror -Iapp/drums -Iapp/engine -Iapp/song -Ibase/include \
    -o $OUT/cpuguard_test tests/host/cpuguard_test.c app/drums/cpuguard.c && $OUT/cpuguard_test"   # the guard's stepping
run bank sh -c "python3 -I tools/fm1_bank.py --check && python3 -I tools/test_fm1_bank.py"   # the bank: head, CRC, areas, no update mark
run screens-export node workbench/tools/export_screens.mjs --check   # ui/screens.json matches screens.js, in ui/vocab.json's words
run screens-gen python3 -I tools/screens/gen.py --check           # app/ui's *_gen.c/.h match the workbench's data
run screens-gen-test python3 -I tools/screens/test_gen.py
run host-build sh build.sh --host
run parity node workbench/tools/parity.mjs $B/tests/parity   # the device's screens (app/ui) against the mock's, pixel for pixel
run livestate python3 -I tools/test_fm1_state.py   # the live state: the simulator answers as the FM-1, fm1_state.py decodes
mkdir -p $B/scenarios
for f in tests/scenarios/*.vnl; do      # the app in the host simulator; outputs (songs.wav, shots) in app/build/scenarios
    name=$(basename "$f" .vnl)         # its song log, MIDI and MusicXML beside its outputs (songs.log, songs-1.mid ... go with songs.wav)
    run "scenario-$name" env VNL_SCORE=$B/scenarios/$name $B/host/vnl-1_host "$f" $B/scenarios
done
if command -v emcc >/dev/null 2>&1; then
    run emu sh build.sh --web
    run smoke-candidates sh workbench/tools/candidates_smoke.sh   # the workbench with a candidate in every family
    # the device's value bag's fixture: the mock's values in fixed states (tests/host/ui_values_fixture.h)
    run ui-fixture sh -c "sh workbench/build.sh && node workbench/tools/ui_fixture.mjs --check"
else
    echo "  skip emu (no emcc)"
fi
[ $FAIL -eq 0 ] && echo "all tests passed" || echo "TESTS FAILED"
exit $FAIL
