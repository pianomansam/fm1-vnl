#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Test only (tests/candidates.sh): the generated tables (patterns_gen.c and .h) from patterns/model.json with one
   entry added to every family the engine draws (a drum pattern, a progression, a strum pattern, an arpeggio rhythm
   and pattern, a melody cell), each unlike every entry before it, as candidates or approved.

   python3 -I tools/patterns/variant.py candidate|approved OUTDIR"""
import copy
import importlib
import json
import os
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
imp = importlib.import_module("import")
export = importlib.import_module("export")


def variant(model, status):
    m = copy.deepcopy(model)
    d = copy.deepcopy(m["drums"]["1"])
    nid = max(e["id"] for e in m["drums"].values()) + 1
    d.update(id=nid, status=status, src="variant.py", file=f"drums/{nid}-variant")
    d["hit"] = ["0x1111", "0x1010", "0x5555", "0x0000", "0x0000", "0x0000"]        # four on the floor
    m["drums"][str(nid)] = d
    p = copy.deepcopy(m["progressions"]["1"])
    nid = max(e["id"] for e in m["progressions"].values()) + 1
    p.update(id=nid, status=status, src="variant.py", file=f"progressions/{nid}-variant",
             moods=["MOOD_SAD", "MOOD_WISTFUL", "MOOD_WARM", "MOOD_SUNNY"])
    p["chords"] = list(reversed(p["chords"]))                                       # G7 then Dm7
    m["progressions"][str(nid)] = p
    n = sum(1 for e in m["strum"].values() if e["kind"] == "pattern")
    m["strum"][str(export.strum_id(n))] = {"index": n, "status": status, "kind": "pattern",
                                           "steps": "D.D.D.D.D.D.D.D.", "file": "strum/variant"}
    off_snare = [[0, 2], [2, 4], [6, 4], [10, 4], [14, 2]]                           # (the lead's draw avoids it)
    for kind, field, v in (("rhythm", "notes", off_snare), ("pattern", "tones", [0, 3, 1, 2])):
        n = sum(1 for e in m["lead-arp"].values() if e["kind"] == kind)
        m["lead-arp"][f"{kind}-{n + 1}"] = {"kind": kind, "index": n, "status": status, field: v,
                                            "file": f"lead-arp/{kind}-variant"}
    n = len(m["melody-cells"])
    m["melody-cells"][str(n + 1)] = {"index": n, "status": status, "mask": "0xF", "weight": 6,
                                     "file": "melody-cells/variant"}                   # every 16th, weighted heavily
    return m


def main(argv):
    if len(argv) != 3 or argv[1] not in ("candidate", "approved"):
        raise SystemExit(__doc__)
    root = Path(__file__).resolve().parent.parent.parent
    tables, _ = imp.rows_from_model(variant(json.loads((root / "patterns/model.json").read_text()), argv[1]))
    c_text, h_text = imp.gen_c(tables)
    out = Path(argv[2])
    (out / "patterns_gen.c").write_text(c_text)
    (out / "patterns_gen.h").write_text(h_text)


if __name__ == "__main__":
    main(sys.argv)
