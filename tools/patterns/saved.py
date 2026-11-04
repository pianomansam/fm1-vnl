#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""The workbench's saved drum patterns (workbench/patterns/<name>.json, the drum screen's "Save as new pattern") as
   library candidates: each one not yet in patterns/drums (no file named <id>-<name>) is written there as a new drums
   entry (the next id, a candidate: the pattern row offers it, the roll never draws it until approved), from its grid:
   six lanes (kick, snare, closed hat, open hat, shaker, rim), each [hit A, ghost A, hit B, ghost B] (16-bit step
   masks); its family and half-time from the pattern it was drawn from (grid.base), two bars where B differs from A.
   import.sh runs it before the import (with --check: only says what it would write).
   python3 -I tools/patterns/saved.py [--check] [--saved DIR] [--patterns DIR]"""
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(HERE))
import export  # noqa: E402

NAME = re.compile(r"^[a-z0-9-]{1,40}$")        # the server's names


def grid_of(path):
    """the saved file's lanes and base pattern id, or None (named why) if it isn't a six-lane two-bar grid"""
    try:
        d = json.loads(path.read_text())
        g, lanes = d["grid"], d["grid"]["lanes"]
    except (ValueError, KeyError, TypeError):
        return None, "not a saved pattern (no grid)"
    if not (isinstance(lanes, list) and len(lanes) == 6 and
            all(isinstance(l, list) and len(l) == 4 and all(isinstance(v, int) and 0 <= v <= 0xFFFF for v in l)
                for l in lanes)):
        return None, "its grid isn't six lanes of four 16-step masks"
    return (lanes, g.get("base")), None


def convert(saved, pat, check=False):
    """-> the lines to print; writes the new candidates' files under pat (not with check)"""
    model = json.loads((pat / "model.json").read_text())
    drums = model["drums"]
    have = {Path(e["file"]).name.split("-", 1)[1] for e in drums.values()}
    have |= {p.stem.split("-", 1)[1] for p in (pat / "drums").glob("*-*.mid")}
    nxt = max(int(k) for k in drums) + 1
    for p in sorted((pat / "drums").glob("*-*.mid")):     # files added since the last import hold their ids
        if p.stem.split("-", 1)[0].isdigit():
            nxt = max(nxt, int(p.stem.split("-", 1)[0]) + 1)
    out = []
    for path in sorted(saved.glob("*.json")) if saved.is_dir() else []:
        name = path.stem
        if not NAME.match(name) or name in have:
            continue
        got, why = grid_of(path)
        if not got:
            out.append(f"import: workbench pattern {name}: skipped, {why}")
            continue
        lanes, base = got
        b = drums.get(str(base)) if base is not None else None
        hx = [lambda l, i=i: "0x%04X" % l[i] for i in range(4)]
        e = {"status": "candidate", "family": b["family"] if b else "PF_BOOMBAP",
             "hit": [hx[0](l) for l in lanes], "ghost": [hx[1](l) for l in lanes],
             "half_time": b["half_time"] if b else 0, "poly3": 0,
             "src": f"Sam, the workbench's saved pattern {name}",
             "two_bar": int(any(l[0] != l[2] or l[1] != l[3] for l in lanes)),
             "hit_b": [hx[2](l) for l in lanes], "ghost_b": [hx[3](l) for l in lanes],
             "file": f"drums/{nxt}-{name}"}
        if not check:
            for ext in (".mid", ".musicxml"):
                export.write_file(pat, "drums", str(nxt), e, ext)
        out.append(f"import: workbench pattern {name} -> {e['file']}, a new candidate" +
                   (" (to write)" if check else ""))
        have.add(name)
        nxt += 1
    return out


def main(argv):
    check = "--check" in argv
    saved, pat = ROOT / "workbench/patterns", ROOT / "patterns"
    for flag, dflt in (("--saved", None), ("--patterns", None)):
        if flag in argv:
            v = Path(argv[argv.index(flag) + 1])
            saved, pat = (v, pat) if flag == "--saved" else (saved, v)
    for line in convert(saved, pat, check):
        print(line)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
