#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""The saved songs over USB end to end, without a device: fm1_songs.py's layout against the C (roll.h's RF_* order,
saved.c's SV_ROWS, saved.h's sizes); then the simulator (app/build/host/vnl-1_host, ./build.sh --host) plays a song
with the comp's instrument changed and saves it on the Song screen; the record the FM-1 sends for that slot (05) is
byte for byte the one fm1_songs.py makes from the live-state dump of the same moment (fm1_state.py's), and a record
written over USB (06) into another slot reads back the same, the list showing it.
    python3 -I tools/test_fm1_songs.py"""
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import fm1_songs as T  # noqa: E402
import fm1_state as S  # noqa: E402

# the layout, as the C has it
roll_h = open(os.path.join(ROOT, "app", "song", "roll.h")).read()
rf = re.search(r"enum \{ (RF_KEY.*?), RF_N \};", roll_h, re.S).group(1).replace("\n", " ")
assert [x.strip()[3:].lower() for x in rf.split(",")] == T.RF, rf
saved_c = open(os.path.join(ROOT, "app", "ui", "saved.c")).read()
rows = re.search(r"SV_ROWS\[SV_NROWS\] = \{(.*?)\};", saved_c, re.S).group(1)
camel = lambda s: re.sub(r"_(\w)", lambda m: m.group(1).upper(), s.lower())  # noqa: E731
assert [camel(x.strip()[4:]) for x in rows.split(",") if x.strip()] == T.ROWS, rows
saved_h = open(os.path.join(ROOT, "app", "ui", "saved.h")).read()
for name, v in (("SV_VERSION", T.VERSION), ("SV_NROWS", T.NROWS), ("SV_BYTES", T.BYTES)):
    assert re.search(rf"\b{name} = {v}\b", saved_h), name
assert T.BYTES == 8 + 2 * len(T.RF) + 2 * len(T.ROWS)

rec = {"seed": 2800145832, "roll": {k: i + 3 for i, k in enumerate(T.RF)}, "motion_moved": -4,
       "rows": {k: -1 if i % 3 == 0 else 100 + i for i, k in enumerate(T.ROWS)}}
assert T.unpack(T.pack(rec)) == rec and T.unpack(bytes(T.BYTES)) is None

hexs = lambda b: " ".join(f"{x:02X}" for x in b)  # noqa: E731
other = dict(rec, seed=12345)
with tempfile.TemporaryDirectory() as d:
    sc = os.path.join(d, "songs.vnl")
    with open(sc, "w") as f:
        # (the home: the main screen first, Settings' HOME SCREEN DETAILED; a fresh device's is now playing)
        f.write("wait 1400\ntap GLO\nwait 50\nspin SELECT 2\nturn K1 1\ntap GLO\nwait 50\n"
                "tap PLAY\nwait 3000\nspin ALGO 3\ntap EDIT\nspin K1 2\nwait 15000\ntap HOME\nspin ALGO -3\n"
                "tap EDIT\nwait 50\ntap SAVE\nwait 50\nlive\nsysex_out\n"
                "sysex F0 7D 56 4E 05 00 F7\nsysex_out\n"
                f"sysex {hexs(T.PFX + bytes([T.PUT, 1]) + T.nibbles(T.pack(other)) + bytes([0xF7]))}\n"
                "sysex_out\ntap PLAY\nwait 3000\n"
                f"sysex {hexs(T.PFX + bytes([T.PUT, 1]) + T.nibbles(T.pack(other)) + bytes([0xF7]))}\n"
                "sysex F0 7D 56 4E 05 01 F7\nsysex F0 7D 56 4E 05 02 F7\nsysex_out\nexpect saved 2\n")
    out = subprocess.run([os.path.join(ROOT, "app", "build", "host", "vnl-1_host"), sc, d], capture_output=True,
                         text=True).stdout
assert "FAIL" not in out, out
state = S.decode([bytes.fromhex(h) for h in [l for l in out.splitlines() if l.startswith("live ")][0].split()[1:]])
frames = [bytes.fromhex(l.split("<", 1)[1].replace(" ", "")) for l in out.splitlines() if "sysex<" in l]
got, puts = {}, []
for f in frames:
    if f[4] == T.GET + 0x10:
        got.setdefault(f[5], (f[6], bytes((f[7 + 2 * i] << 4) | f[8 + 2 * i] for i in range(T.BYTES))))
    elif f[4] == T.PUT + 0x10:
        puts.append((f[5], f[6]))
mine = T.from_state(state)
assert mine["rows"]["keys"] == state["rows"]["keys"][0] and "set" in state, state.get("set")
assert got[0][0] == 2 and got[0][1] == T.pack(mine), (T.unpack(got[0][1]), mine)   # (waiting for silence)
assert puts == [(1, 2), (1, 0)], puts   # (playing, slot 1's save waiting: refused; stopped: written)
assert got[1][0] == 1 and T.unpack(got[1][1]) == other
assert got[2][0] == 0 and T.unpack(got[2][1]) is None
old = {k: v for k, v in state.items() if k not in ("set", "motion_moved")}   # (an older firmware's dump: the set found)
assert [s["id"] for s in T.sets_for(old)] == [mine["roll"]["set"]] and T.from_state(old) == mine
print(f"test_fm1_songs: ok (slot 1 from the dump: {T.describe(mine)})")
