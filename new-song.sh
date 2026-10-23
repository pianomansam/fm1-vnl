#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Render a new random song to listen to, with its log, MIDI and MusicXML, and open it.
#   ./new-song.sh          a fresh seed
#   ./new-song.sh SEED     replay a song by the seed its log prints
#   BED=0 ./new-song.sh    no surface noise or rain (VNL_BED)
#   BASS=0 ./new-song.sh   the groove's own bass rhythm instead of the bass spike (VNL_BASS: 1 a rhythm cell per song,
#                          2 the kick's, 3 half notes on 1 and 3, 4 beats 1 and 2, 5 beats 1, 2 and 4)
set -e
cd "$(dirname "$0")"
seed=${1:-$(od -An -N4 -tu4 /dev/urandom | tr -d ' ')}
out=app/build/new/$seed
./build.sh --host >/dev/null
mkdir -p "$out"
VNL_SEED=$seed VNL_BASS=${BASS:-1} VNL_BED=${BED:-1} VNL_SCORE=$out/song app/build/host/vnl_host new-song.vnl "$out" >/dev/null
rm -f "$out"/song-2* "$out"/now.png "$out"/midi_out.txt
.venv/bin/python - "$out" <<'PY'
import re, sys, wave
out = sys.argv[1]
log = open(f"{out}/song.log").read()
m = re.search(r"\n=== song 2 at ([\d.]+)s", log)
if m:                                    # keep song 1 only: the log, and the WAV up to the next song
    open(f"{out}/song.log", "w").write(log[:m.start() + 1])
    w = wave.open(f"{out}/song.wav")
    p, frames = w.getparams(), w.readframes(int(float(m.group(1)) * w.getframerate()))
    w.close()
    w = wave.open(f"{out}/song.wav", "wb"); w.setparams(p); w.writeframes(frames); w.close()
print(re.search(r"=== song 1.*\n(.*\n.*)", log).group(1))
PY
echo "$out"
open "$out/song.wav" 2>/dev/null || true
