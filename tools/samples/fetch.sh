#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Download every recording tools/samples/manifest.json names, each source into its own new directory (SCRATCH/dl_*),
# check that each source's license file is the CC0 1.0 legal code (stops if not), unpack the FreePats 7z (bsdtar)
# and decode the FLACs to WAV (afconvert on macOS, else flac). Nothing from a download is executed; files already
# fetched are kept.
#   tools/samples/fetch.sh SCRATCH
#   PY: a Python 3 (default python3); then: $PY -I tools/samples/pack.py pack SCRATCH app/engine
set -eu
[ $# -eq 1 ] || { echo "usage: fetch.sh SCRATCH"; exit 2; }
D=$1
HERE=$(cd "$(dirname "$0")" && pwd)
PY=${PY:-python3}
mkdir -p "$D"
TAB=$(printf '\t')

sha256() { if command -v shasum >/dev/null 2>&1; then shasum -a 256 "$1"; else sha256sum "$1"; fi | cut -d' ' -f1; }

"$PY" -I "$HERE/pack.py" list > "$D/files.tsv"
while IFS="$TAB" read -r dir url path; do
    mkdir -p "$D/$dir/$(dirname "$path")"
    [ -s "$D/$dir/$path" ] || curl -sfL --retry 3 -o "$D/$dir/$path" "$url" || { echo "fetch.sh: failed: $url"; exit 1; }
done < "$D/files.tsv"

for a in "$D"/dl_*/*.7z; do               # the FreePats archive, unpacked beside itself (bsdtar refuses ../ paths)
    [ -e "$a" ] && bsdtar -xf "$a" -C "$(dirname "$a")"
done

"$PY" -I "$HERE/pack.py" licenses > "$D/licenses.tsv"
WANT=$(grep '^sha256' "$D/licenses.tsv" | cut -f2)
grep -v '^sha256' "$D/licenses.tsv" | while IFS="$TAB" read -r dir url path want; do
    f="$D/$dir/$path"
    [ -n "$want" ] || want=$WANT        # (a source's own copy of the CC0 1.0 text: its hash, in the manifest)
    [ -s "$f" ] || { echo "fetch.sh: $dir: no license file ($path)"; exit 1; }
    [ "$(sha256 "$f")" = "$want" ] || { echo "fetch.sh: $dir: $path is not the CC0 1.0 legal code ($url); stopping"; exit 1; }
    echo "  CC0 1.0  $dir ($url)"
done

find "$D" -name '*.flac' | while read -r f; do
    [ -s "${f%.flac}.wav" ] && continue
    if command -v afconvert >/dev/null 2>&1; then afconvert -f WAVE -d LEI24 "$f" "${f%.flac}.wav"
    else flac -s -d -o "${f%.flac}.wav" "$f"; fi
done
echo "fetch.sh: done ($(wc -l < "$D/files.tsv" | tr -d ' ') files in $D)"
