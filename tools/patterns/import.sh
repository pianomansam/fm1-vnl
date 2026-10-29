#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Imports patterns/ (Sam's edited or added .mid and .musicxml files) into the engine's generated tables
# (app/song/patterns_gen.c and .h), workbench/options.json's kickPhrases, patterns/model.json and .manifest.json, then
# runs the engine suite when the tables changed. A refusal names the file, the bar and the rule, and writes nothing.
#   tools/patterns/import.sh [--check]     --check: validate only, write nothing
set -eu
cd "$(dirname "$0")/../.."
before=$(cat app/song/patterns_gen.c app/song/patterns_gen.h | shasum -a 256)
python3 -I tools/patterns/import.py "$@"
case " $* " in *" --check "*) exit 0 ;; esac
if [ "$(cat app/song/patterns_gen.c app/song/patterns_gen.h | shasum -a 256)" != "$before" ]; then
    echo "import: the tables changed; running the engine suite"
    PATH="$PWD/.venv/bin:$PATH" sh tests/run_tests.sh
fi
