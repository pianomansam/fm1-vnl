#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Imports patterns/ (Sam's edited or added .mid and .musicxml files) into the engine's generated tables
# (app/song/patterns_gen.c and .h), workbench/options.json's generated blocks, patterns/model.json and .manifest.json,
# with patterns/status.json's statuses (the workbench's Approve). When the tables changed it rebuilds the workbench
# (its engine plays the new tables; reload the page) and runs the engine suite. A refusal names the file, the bar and
# the rule, and writes nothing. First, the workbench's saved drum patterns (workbench/patterns/*.json, the drum
# screen's "Save as new pattern") not yet in patterns/drums become new candidate files there (saved.py).
#
# Approving, retiring or changing an approved pattern moves the songs, so the suite's pinned song hashes fail on
# purpose. The importer says why before and after the suite; if only the pins failed it exits 3 with their new values
# (anything else failing is a real failure: exit 1). Then, after listening, re-pin them: import.sh --repin rewrites
# them in tests/host/song_test.c with a comment naming the approval(s) and reruns the suite. Nothing is ever re-pinned
# without --repin.
#   tools/patterns/import.sh [--check | --repin]     --check: validate only, write nothing
set -u
cd "$(dirname "$0")/../.."
LOGS=app/build/tests
mkdir -p "$LOGS"
out=$LOGS/import.out
suite() {      # the engine suite, then what its result means for the pins: import.sh's exit status
    PATH="$PWD/.venv/bin:$PATH" sh tests/run_tests.sh > "$LOGS/suite.out" 2>&1
    cat "$LOGS/suite.out"
    python3 -I tools/patterns/pins.py after "$LOGS/suite.out"
    st=$?
    [ $st -eq 0 ] && rm -f "$LOGS/pins-expected"
    exit $st
}
if [ "${1:-}" = "--repin" ]; then
    python3 -I tools/patterns/pins.py repin || exit 1
    echo "import: rerunning the engine suite with the new pins"
    suite
fi
before=$(cat app/song/patterns_gen.c app/song/patterns_gen.h | shasum -a 256)
python3 -I tools/patterns/saved.py "$@" || exit 1     # the drum screen's saved patterns, as new candidates
python3 -I tools/patterns/import.py "$@" > "$out"
st=$?
cat "$out"
[ $st -eq 0 ] || exit $st
case " $* " in *" --check "*) exit 0 ;; esac
if [ "$(cat app/song/patterns_gen.c app/song/patterns_gen.h | shasum -a 256)" != "$before" ]; then
    if command -v emcc >/dev/null 2>&1; then
        echo "import: the tables changed; rebuilding the workbench (reload its page)"
        sh workbench/build.sh || exit 1
    else
        echo "import: WARNING: the workbench was NOT rebuilt (no emcc): its engine still plays the old tables, while"
        echo "import: WARNING: options.json offers the new ones. Install emscripten, then run sh workbench/build.sh."
    fi
    why=$(sed -n 's/^import: pins move: //p' "$out")
    if [ -n "$why" ]; then
        [ -s "$LOGS/pins-expected" ] && why="$(cat "$LOGS/pins-expected"); $why"   # not re-pinned yet: both reasons
        printf '%s\n' "$why" > "$LOGS/pins-expected"
        echo "import: EXPECTED: the pinned song hashes move because of: $why (the suite's pin checks will fail)"
    fi
    echo "import: the tables changed; running the engine suite"
    suite
fi
