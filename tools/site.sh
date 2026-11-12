#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# VNL-1's site (GitHub Pages): the release build, the browser emulator, fm1-base's tools/make_site.py (the front page
# from app/site/landing.md, the web installer, the manual, the emulator), then the sample bank beside the package
# (firmware/vnl-VERSION-bank.bin: the installer writes it after the firmware, app/web/install_app.js).
#   tools/site.sh VERSION             build the site into app/build/site (serve it: python3 -m http.server -d app/build/site)
#   tools/site.sh VERSION --publish   the same, then push it to origin's gh-pages branch, replacing what was there
# The same steps as fm1-base's tools/publish_site.sh, plus the bank.
set -e
cd "$(dirname "$0")/.."
V="${1:?usage: tools/site.sh VERSION [--publish]}"
PY="${PYTHON:-$( [ -x .venv/bin/python ] && echo .venv/bin/python || echo python3)}"
OUT=app/build/site
sh build.sh --release "$V"
sh build.sh --web --release "$V"
"$PY" base/tools/make_site.py app "app/build/vnl-$V.fwsc" "$V" "$OUT"
rm -rf "$OUT/emu/o2"                  # the emulator build's object files (make_site copies all of app/build/emu)
"$PY" -c '
import sys
sys.path.insert(0, "tools")
import fm1_bank as K
bank = K.load(K.DEFAULT)
K.placed(bank, K.split_of(K.DEFAULT))
open(sys.argv[1], "wb").write(bank)
print(f"bank     {sys.argv[1]}  {len(bank)} B")
' "$OUT/firmware/vnl-$V-bank.bin"
[ "${2:-}" = "--publish" ] || exit 0
REMOTE="$(git remote get-url origin)"
SHA="$(git rev-parse --short HEAD)"
cd "$OUT"
rm -rf .git
git init -q -b gh-pages
git add -A
git -c user.email=884872+pianomansam@users.noreply.github.com commit -q -m "Site for VNL-1 $V (from $SHA)"
git push -q -f "$REMOTE" gh-pages
rm -rf .git
# ask GitHub Pages to build it now: a push alone has left the first deploy stuck on "building"
REPO="$(printf %s "$REMOTE" | sed -E 's#(git@github.com:|https://github.com/)##; s#\.git$##')"
gh api -X POST "repos/$REPO/pages/builds" >/dev/null 2>&1 || echo "(could not ask GitHub Pages to build: check the repository's Pages settings)"
echo "pushed the site for VNL-1 $V to gh-pages"
