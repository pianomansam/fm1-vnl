#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Flash the FM-1 clean: M-VAVE's stock V15 first, then VNL-1, then its sample bank, keeping the saved songs.
#   sh tools/flash.sh            # asks once, then runs all three
#   sh tools/flash.sh --check    # checks the files only, touches no device
# The stock package isn't in git (M-VAVE's): FM1_STOCK overrides where it is. fm1_install.py accepts it only
# by its SHA-256 (STOCK_SHA256), so a wrong file is refused before anything is written.
# The saved songs: V15's package claims 0x93000..0xE8FFF as its VM (its region table), and stock's install or boot
# clears it, VNL-1's saved songs (0xDE000) with it. So they are exported first (into backups/, kept) and imported last.
set -eu
cd "$(dirname "$0")/.."
PY=.venv/bin/python
STOCK=${FM1_STOCK:-ref/fm1_open/sdk/firmware/FM-1_015.fwsc}
VNL=app/build/vnl-1.fwsc
SONGS=backups/songs-$(date +%Y%m%d-%H%M%S).json

for f in "$STOCK" "$VNL"; do [ -f "$f" ] || { echo "missing: $f" >&2; exit 2; }; done
[ "$(shasum -a 256 "$STOCK" | cut -c1-64)" = db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a ] \
    || { echo "$STOCK is not M-VAVE's FM-1 V15" >&2; exit 2; }
[ "$(find "$VNL" -newer app/app.c)" ] || echo "warning: $VNL is older than app/app.c (run ./build.sh?)" >&2
$PY tools/fm1_bank.py --check
[ "${1:-}" = --check ] && { echo "files ok"; exit 0; }

printf 'Flash stock V15, then VNL-1, then the sample bank? [y/N] '
read -r ok; [ "$ok" = y ] || exit 1

echo "== 0/3 saved songs out (into $SONGS)"
mkdir -p backups
rc=0; $PY tools/fm1_songs.py export "$SONGS" || rc=$?
if [ "$rc" = 3 ]; then
    echo "(VNL-1 not found: no saved songs to keep)"; SONGS=
elif [ "$rc" != 0 ]; then
    printf 'The saved songs could not be read. Flash anyway, losing them? [y/N] '
    read -r ok; [ "$ok" = y ] || exit 1; SONGS=
fi

echo "== 1/3 stock V15";  $PY base/tools/fm1_install.py "$STOCK" --yes
echo "== 2/3 VNL-1";      $PY base/tools/fm1_install.py "$VNL" --yes
echo "== 3/3 sample bank"
# ponytail: fixed retries while VNL-1's USB port comes up; exit 3 = not found yet
n=0; until $PY tools/fm1_bank.py --yes; do
    rc=$?; n=$((n + 1)); [ "$rc" = 3 ] && [ "$n" -lt 5 ] || exit "$rc"; sleep 2
done
if [ -n "$SONGS" ]; then
    echo "== saved songs back (from $SONGS)"
    $PY tools/fm1_songs.py import "$SONGS" --write || {
        echo "not all written back; again, stopped: $PY tools/fm1_songs.py import $SONGS --write" >&2; exit 1; }
    $PY tools/fm1_songs.py list
fi
$PY base/tools/fm1_install.py --info
