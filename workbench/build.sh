#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# The workbench: the song engine in WebAssembly (app/build/workbench/wb.wasm) and the page beside it.
#   sh workbench/build.sh      then: python3 workbench/server.py
set -e
cd "$(dirname "$0")/.."
out=app/build/workbench
mkdir -p "$out"
emcc -O2 -ffp-contract=off -std=gnu99 -Wall -Wextra -Wno-unused-parameter -Werror \
    -Iapp/song -Iapp/engine -Ibase/include -Iworkbench/engine \
    --no-entry -sSTANDALONE_WASM -sALLOW_MEMORY_GROWTH \
    -o "$out/wb.wasm" app/song/*.c app/engine/*.c workbench/engine/*.c \
    workbench/engine/poketrack/trommelsynthesizer.c workbench/engine/poketrack/samples_stub.c
cp workbench/index.html workbench/wb.js workbench/wb-worklet.js workbench/rows.js workbench/desk.js workbench/drums.js "$out/"
echo "workbench  $out  ($(wc -c < "$out/wb.wasm" | tr -d ' ') B wasm)"
