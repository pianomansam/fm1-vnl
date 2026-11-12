#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""The live state end to end, without a device: the simulator (app/build/host/vnl-1_host, ./build.sh --host) plays a
song and answers the state request ("live": the frames as the FM-1 sends them over USB), and fm1_state.py's decode
reads them; the frames out of order, among the browser remote's, still decode; one missing does not.
    python3 -I tools/test_fm1_state.py"""
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import fm1_state as S  # noqa: E402

with tempfile.TemporaryDirectory() as d:
    sc = os.path.join(d, "live.vnl")
    with open(sc, "w") as f:
        f.write("tap PLAY\nwait 3000\nlive reset\nlive\n")
    out = subprocess.run([os.path.join(ROOT, "app", "build", "host", "vnl-1_host"), sc, d], capture_output=True,
                         text=True, check=True).stdout
lines = [l for l in out.splitlines() if l.startswith("live ")]
frames = [bytes.fromhex(h) for h in lines[0].split()[1:]]
after = S.decode([bytes.fromhex(h) for h in lines[1].split()[1:]])   # (after the reset: the measures from zero)
assert len(frames) > 1 and all(f[:5] == bytes([0xF0, 0x7D, 0x56, 0x4E, 0x14]) and max(f[1:-1]) < 0x80 for f in frames)
s = S.decode(frames)
assert s["source"] == "device" and s["playing"] is True and s["bar"] >= 0 and s["tempo"] >= 70, s
assert s["screen"] and isinstance(s["rows"], dict) and "kit" in s["rows"] and "cpu" in s and "guard" in s["cpu"], s
assert S.summary(s)
assert "late_by" in s["cpu"] and "ab" in s["cpu"] and after["cpu"]["guard_steps"] == 0 and after["cpu"]["late"] == 0
assert all(after["cpu"][k] == 0 for k in ("guard", "guard_recov", "guard_top", "shed", "over90")), after["cpu"]
assert after["cpu"]["render_us"] <= max(s["cpu"]["render_us"], 1000000)
p = after["prof"]   # (the CPU by stage: the blocks since the reset, a figure a stage, the table)
assert p["blocks"] > 0 and len(p["stages"]) == len(p["us"]) == len(p["worst"]) and "bass" in p["stages"], p
assert S.prof_table(after)[0].startswith("CPU by stage")
remote = bytes([0xF0, 0x7D, 0x56, 0x4E, 0x11, 0x00, 0x01, 0xF7])   # (the remote's ack among them)
assert S.decode([remote] + frames[::-1]) == s
assert S.decode(frames[:-1]) is None
print(f"test_fm1_state: ok ({len(frames)} frames, {sum(len(f) for f in frames)} bytes)")
