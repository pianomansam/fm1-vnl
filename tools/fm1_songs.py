#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""VNL-1's saved songs on a connected FM-1, over USB-MIDI. Read-only unless --write.

    python3 tools/fm1_songs.py [--port NAME]                       list the 16 slots
    python3 tools/fm1_songs.py export FILE                         every slot's record into FILE (JSON)
    python3 tools/fm1_songs.py put SLOT FILE [--set ID] --write    a song into SLOT (1-16): FILE is a live-state dump
                                                                   (tools/fm1_state.py's device.json, the workbench's
                                                                   state) or one slot of an export
    python3 tools/fm1_songs.py import FILE --write                 an export's slots back, each into its own slot
    python3 tools/fm1_songs.py delete SLOT --write
    python3 tools/fm1_songs.py show FILE [--set ID]                the record a dump makes, without a device

A saved song (app/ui/saved.h) is its roll (the seed and every rolled value: key, scale, tempo, motion, mood, swing,
layout, progression, color, set, drum pattern, texture, length) and the 23 rows the screens set (instruments, kit,
overrides, re-rolls). A live-state dump has all of it but, from firmware older than the saved songs, the set (its
"set" field) and the motion's live move ("motion_moved", taken as 0): the set is then found from the dump's bass, lead
and pad instruments and its mood among the library's sets (app/song/library.c), or given with --set (refused when
none or several fit). Commands (app/app.c song_slot_cmd), prefix F0 7D 56 4E: 05 ss asks slot ss (answer 15 ss st
<168 nibbles>, st: 0 empty, 1 in flash, 2 in RAM only until the music stops, 3 in RAM only: the flash write failed),
06 ss <168 nibbles> writes it (answer 16 ss rc: 0 written to flash, 1 in RAM only until the music stops: the FM-1
writes its flash only while silent, 2 refused: another save waits, 3 refused: not a record, 4 in RAM only: the flash
write failed). The list tags every song: [flash] is in the flash and survives power-off; [RAM ...] is not yet.
tools/flash.sh exports the songs before it flashes and imports them after (its stock V15 step erases them).
Uses fm1-base's tools (base/tools: fm1_install.py's port lookup); needs mido with python-rtmidi.
Exit codes: 0 done, 1 no answer or refused, 2 bad input, 3 FM-1 not found.
"""
import argparse
import json
import os
import re
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PFX = bytes([0xF0, 0x7D, 0x56, 0x4E])
GET, PUT = 0x05, 0x06
SLOTS, VERSION, NROWS, BYTES = 16, 1, 23, 84          # app/ui/saved.h's UI_SLOTS, SV_VERSION, SV_NROWS, SV_BYTES
# song_roll_t's fields in RF_* order (app/song/roll.h) and the rows a record holds in SV_ROWS' order (app/ui/saved.c),
# by the workbench's row ids: tools/test_fm1_songs.py checks both against the C
RF = ["key", "tempo", "motion", "mood", "mode", "map", "prog", "color", "set", "pattern", "swing", "dust", "noise",
      "ambience", "bars"]
ROWS = ["keys", "comp", "lead", "bass", "pad", "padOn", "phrase", "leadStyle", "leadLoop", "leadOct", "busyThin",
        "keysWith", "voicings", "keysOct", "strumPat", "strumFill", "padPlace", "bassLine", "padNotes", "drumsVar",
        "kit", "kick", "ride"]
# a row an older firmware's dump lacks (newer than it), as the song played it then: as rolled (override.h's)
AS_ROLLED = {"keysWith": -1}
MODES = ["Ionian", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Aeolian"]
KEYS = ["C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"]
STATE = ["empty", "flash", "RAM only: written once the music stops", "RAM only: the flash write failed"]


# ------------------------------------------------------------------------------------------------- records ---
def pack(rec):
    """a record dict ({seed, roll: {RF name: value}, motion_moved, rows: {row: value}}) -> BYTES bytes"""
    b = bytearray(BYTES)
    b[0], b[1] = VERSION, NROWS
    b[2:6] = (rec["seed"] & 0xFFFFFFFF).to_bytes(4, "little")
    vals = [rec["roll"][k] for k in RF] + [rec.get("motion_moved", 0)] + [rec["rows"][k] for k in ROWS]
    for i, v in enumerate(vals):
        b[6 + 2 * i:8 + 2 * i] = (v & 0xFFFF).to_bytes(2, "little")
    return bytes(b)


def unpack(b):
    """BYTES bytes -> a record dict, or None (an empty slot)"""
    if len(b) != BYTES or b[0] != VERSION or b[1] != NROWS:
        return None
    s16 = lambda i: int.from_bytes(b[6 + 2 * i:8 + 2 * i], "little", signed=True)  # noqa: E731
    return {"seed": int.from_bytes(b[2:6], "little"), "roll": {k: s16(i) for i, k in enumerate(RF)},
            "motion_moved": s16(len(RF)), "rows": {k: s16(len(RF) + 1 + i) for i, k in enumerate(ROWS)}}


def library():
    """app/song/library.c's sets: [{id, name, bass, lead, pad, moods}] (instrument ids as the rows have them)"""
    h = open(os.path.join(ROOT, "app", "song", "library.h")).read()
    body = re.sub(r"/\*.*?\*/", "", re.search(r"enum \{ (INS_RHODES.*?)\};", h, re.S).group(1), flags=re.S)
    ins = {n.strip(): i for i, n in enumerate(x for x in body.replace("\n", " ").split(",") if x.strip())}
    moods = {"MOOD_SAD": 1, "MOOD_WISTFUL": 2, "MOOD_WARM": 4, "MOOD_SUNNY": 8}
    c = open(os.path.join(ROOT, "app", "song", "library.c")).read()
    out = []
    for m in re.finditer(r'\{(\d+), "([^"]+)", (INS_\w+), (INS_\w+), (INS_\w+), (INS_\w+), (INS_\w+), (KIT_\w+), '
                         r'(KIT_\w+), (FL_\w+), ([A-Z_ |]+),', c):
        out.append({"id": int(m.group(1)), "name": m.group(2), "keys": ins[m.group(3)], "bass": ins[m.group(4)],
                    "lead": ins[m.group(5)], "pad": ins[m.group(7)],
                    "moods": sum(moods[x.strip()] for x in m.group(11).split("|"))})
    return out


def sets_for(state):
    """the library sets a dump's song can be on: its bass and lead (the set's, or the song's own), its pad (the set's,
       or a pad the song drew: the choir, the organ), its mood among the set's"""
    rows = state["rows"]
    v = lambda r: rows[r][0] if isinstance(rows[r], list) else rows[r]  # noqa: E731
    lib = library()
    mood = 1 << v("mood")
    fit = [s for s in lib if s["moods"] & mood]
    for r in ("bass", "lead", "pad", "keys"):           # (an instrument changed by hand fits no set: skipped)
        narrower = [s for s in fit if s[r] == v(r) or (r == "pad" and v(r) in (29, 18))]
        if narrower:
            fit = narrower
    return fit


def from_state(state, set_id=None):
    """a live-state dump (fm1_state.py's device.json) -> a record dict; ValueError when the set can't be told"""
    rows = state["rows"]
    v = lambda r: rows[r][0] if isinstance(rows[r], list) else rows[r]  # noqa: E731
    roll = {k: v(k) for k in RF if k not in ("set", "bars")}
    roll["bars"] = state["bars"]
    if set_id is None:
        set_id = state.get("set")
    if set_id is None:
        fit = sets_for(state)
        if len(fit) != 1:
            raise ValueError("the dump has no set and " + ("none fits" if not fit else
                             "several fit: " + ", ".join(f"{s['id']} ({s['name']})" for s in fit)) + "; give --set")
        set_id = fit[0]["id"]
    roll["set"] = int(set_id)
    if (state.get("waiting") or {}).get("changes"):
        raise ValueError("a change waits in the dump: dump again once it has landed")
    missing = [k for k in ROWS if k not in rows]
    if [k for k in missing if k not in AS_ROLLED]:
        raise ValueError(f"the dump has no {', '.join(missing)} row")
    if missing:
        print(f"note: the dump has no {', '.join(missing)} row: as rolled", file=sys.stderr)
    return {"seed": state["seed"], "roll": roll, "motion_moved": state.get("motion_moved", 0),
            "rows": {k: v(k) if k in rows else AS_ROLLED[k] for k in ROWS}}


def describe(rec):
    r = rec["roll"]
    return (f"seed {rec['seed']}, {KEYS[r['key'] % 12]} {MODES[r['mode'] % 6]}, {r['tempo']} BPM, mood {r['mood']}, "
            f"motion {r['motion']}, map {r['map']}, prog {r['prog']}, set {r['set']}, {r['bars']} bars; keys "
            f"{rec['rows']['keys']}, lead {rec['rows']['lead']}, bass {rec['rows']['bass']}, kit {rec['rows']['kit']}")


# ---------------------------------------------------------------------------------------------- the device ---
def nibbles(b):
    return bytes(x for y in b for x in (y >> 4, y & 15))


def ask(link, msg, want, timeout=2.0):
    """send msg, return the answer frame whose command byte is want (bytes F0 .. F7), or None"""
    link.drain()
    if not link.send(msg):
        return None
    t0 = time.monotonic()
    while time.monotonic() - t0 < timeout:
        f = link.read(0.05)
        if f is not None and f[:4] == PFX and len(f) > 6 and f[4] == want and f[5] == msg[5]:
            return f
    return None


def get(link, slot):
    f = ask(link, PFX + bytes([GET, slot, 0xF7]), GET + 0x10)
    if f is None or len(f) < 8 + 2 * BYTES:
        return None, None
    b = bytes((f[7 + 2 * i] << 4) | f[8 + 2 * i] for i in range(BYTES))
    return f[6], unpack(b)


def put(link, slot, rec):
    b = pack(rec) if rec else bytes(BYTES)
    f = ask(link, PFX + bytes([PUT, slot]) + nibbles(b) + bytes([0xF7]), PUT + 0x10)
    return None if f is None else f[6]


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cmd", nargs="?", default="list", choices=["list", "export", "put", "import", "delete", "show"])
    ap.add_argument("args", nargs="*")
    ap.add_argument("--port", metavar="NAME", help="MIDI port to use (part of its name; default: VNL-1 FM-1's)")
    ap.add_argument("--set", type=int, metavar="ID", help="the song's library set (a dump without one)")
    ap.add_argument("--write", action="store_true", help="really write (put, import, delete)")
    a = ap.parse_args(argv)
    todo = []                                           # (slot from 0, record or None)
    try:
        if a.cmd == "show":
            rec = from_state(json.load(open(a.args[0])), a.set)
            print(describe(rec))
            print(pack(rec).hex())
            return 0
        if a.cmd == "put":
            slot, data = int(a.args[0]) - 1, json.load(open(a.args[1]))
            todo = [(slot, data if "roll" in data and "seed" in data and "rows" in data and isinstance(
                data["roll"], dict) else from_state(data, a.set))]
        elif a.cmd == "import":
            todo = [(int(k) - 1, r) for k, r in json.load(open(a.args[0]))["slots"].items() if r]
            if not todo:
                print(f"{a.args[0]}: no songs in it, nothing to write")
                return 0
        elif a.cmd == "delete":
            todo = [(int(a.args[0]) - 1, None)]
        elif a.cmd == "export" and not a.args:
            raise ValueError("export FILE")
    except (ValueError, IndexError, KeyError, OSError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 2
    if any(not 0 <= s < SLOTS for s, _ in todo):
        print("error: a slot is 1-16", file=sys.stderr)
        return 2
    if todo and not a.write:
        for s, r in todo:
            print(f"would write slot {s + 1}: {describe(r) if r else 'deleted'} (--write to do it)")
        return 0
    sys.path.insert(0, os.path.join(ROOT, "base", "tools"))
    import fm1_install as I  # noqa: E402
    import fm1_backup as B  # noqa: E402
    try:
        dev = B.find(I.MidoBackend(), a.port, B.Kind(identity="FM-1_22", name="VNL-1", port="VNL-1 FM-1"))
    except I.InstallError as e:
        print(f"error: {e}", file=sys.stderr)
        return 3
    try:
        if todo:
            for s, r in todo:
                rc = put(dev.link, s, r)
                print(f"slot {s + 1}: " + {None: "no answer", 0: "written to flash",
                                           1: "in RAM only; written to flash once the music stops",
                                           2: "refused: another save waits (stop the music)",
                                           3: "refused: not a record",
                                           4: "in RAM only: the flash write failed (save again)"}.get(
                                               rc, f"answer {rc}"))
                if rc not in (0, 1):
                    return 1
            return 0
        out = {}
        for s in range(SLOTS):
            st, rec = get(dev.link, s)
            if st is None:
                print("error: the FM-1 did not answer (a VNL-1 without saved songs?)", file=sys.stderr)
                return 1
            out[str(s + 1)] = rec
            tag = STATE[st] if st < len(STATE) else f"state {st}"
            print(f"{s + 1:2d}  {f'[{tag}] ' + describe(rec) if rec else 'empty'}")
        if a.cmd == "export":
            with open(a.args[0], "w") as f:
                json.dump({"format": "vnl-1 saved songs", "version": VERSION, "slots": out}, f, indent=1)
            print(f"wrote {a.args[0]}")
        return 0
    finally:
        dev.link.close()


if __name__ == "__main__":
    sys.exit(main())
