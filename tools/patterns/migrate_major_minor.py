#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""One-off (Sam, 2026-11-05: modes become major or minor, "easier writing and approving of progressions. plus
   reasoning about what i'm hearing"): patterns/model.json's progressions, spelled in the six modes, respelled as
   degrees of the major or the natural minor scale with accidentals, every chord the same pitch classes in the same
   key at every color.

   A mode's song becomes a major song (Ionian, Lydian, Mixolydian) or a minor one (Dorian, Aeolian; Phrygian had no
   progression). Each chord's root takes the simplest spelling (validate.DEG: b2 b3 #4 b6 b7 in major, b2 3 #4 6 7 in
   minor); its quality is the old one where it still sounds the same, else the key's own chord there (Q_DIATONIC),
   else the forced quality that does (Q_MIN7B9, Q_MAJ7S11: the colors the modes gave). A progression listed in modes
   whose respellings differ is split: the first mode keeps its id (so an old snapshot's Ionian or Dorian, the values
   major and minor took over, needs no mapping), each other spelling takes the next free id.

   python3 -I tools/patterns/migrate_major_minor.py [--map FILE]    rewrites patterns/model.json; FILE: the old
   (mode, progression) -> (major/minor, progression) map, "old_mode old_prog new_mode new_prog" a line. Then
   tools/patterns/import.py (the tables) and tools/patterns/export.sh (the files). It refuses a migrated model."""
import json
import os
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import export  # noqa: E402
import validate  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent.parent

# the six modes as they were (validate.py and theory.c before 2026-11-05)
OLD_Q = ["Q_DIATONIC", "Q_MIN7", "Q_DOM7", "Q_MAJ7"]
OLD_MODES = ["MODE_IONIAN", "MODE_DORIAN", "MODE_PHRYGIAN", "MODE_LYDIAN", "MODE_MIXOLYDIAN", "MODE_AEOLIAN"]
OLD_SCALE = [[0, 2, 4, 5, 7, 9, 11], [0, 2, 3, 5, 7, 9, 10], [0, 1, 3, 5, 7, 8, 10], [0, 2, 4, 6, 7, 9, 11],
             [0, 2, 4, 5, 7, 9, 10], [0, 2, 3, 5, 7, 8, 10]]
TONALITY = [0, 1, 1, 0, 0, 1]                   # each old mode's new value: 0 major, 1 minor
FIXTURE = ROOT / "tools/patterns/major_minor_2026-11-05.json"
FIXTURE_NOTE = ("Written by tools/patterns/migrate_major_minor.py (2026-11-05): each progression in each mode it "
                "listed, with its chords' pitch classes then (key C; per chord, at 7ths, 9ths and rich), and the "
                "major or minor progression it became. test_patterns.py holds the engine to them in every key; "
                "the workbench maps an old snapshot's mode and progression by it.")


def old_stack(mode, d, k):
    s = OLD_SCALE[mode]
    return (s[(d + k) % 7] - s[d]) % 12


def old_chord_pcs(mode, spec, color):
    """theory.c's th_chord_pcs before 2026-11-05 (key 0)"""
    d = max(0, min(6, spec["degree"]))
    q = OLD_Q.index(spec["quality"])
    root = (OLD_SCALE[mode][d] + spec["alter"]) % 12
    i3, i5, i7, i9 = (old_stack(mode, d, k) for k in (2, 4, 6, 8))
    ext = -1
    if q != 0:
        i5, i9 = 7, 2
        i3 = 3 if q == 1 else 4
        i7 = 11 if q == 3 else 10
        ext = 9 if q == 2 else 5 if q == 1 else 6 if mode == 3 else -1
    elif i3 == 4 and i7 == 10:
        ext = old_stack(mode, d, 12)
    elif i3 == 3 and i5 == 7 and i7 == 10:
        ext = old_stack(mode, d, 10)
    elif i3 == 4 and i7 == 11 and old_stack(mode, d, 10) == 6:
        ext = 6
    t = [0, i3] + ([i5] if color != 2 or i5 != 7 else []) + [i7]
    base = len(t)
    if color >= 1:
        t.append(i9)
    if color == 2 and ext >= 0:
        t.append(ext)
    i = base
    while i < len(t):
        if any(t[i] == (t[k] + 1) % 12 for k in range(i)):
            del t[i]
        else:
            i += 1
    return [(root + x) % 12 for x in t]


def respell(old_mode, c):
    """the chord in the major or minor key: the same pitch classes (in order) at every color, or None"""
    ton = TONALITY[old_mode]
    want = [old_chord_pcs(old_mode, c, color) for color in range(3)]
    spec = validate.spec_of(ton, want[0][0])
    qs = [c["quality"]] if c["quality"] != "Q_DIATONIC" else []
    qs += ["Q_DIATONIC"] if spec["alter"] == 0 else []
    qs += [q for q in validate.Q[1:] if q not in qs]
    for q in qs:
        s = {**spec, "quality": q}
        if all(validate.chord_pcs(ton, s, color) == want[color] for color in range(3)):
            return s
    return None


def main(argv):
    map_file = argv[argv.index("--map") + 1] if "--map" in argv else None
    path = ROOT / "patterns/model.json"
    model = json.loads(path.read_text())
    progs = model["progressions"]
    if any(m in validate.MODES for e in progs.values() for m in e["modes"]):
        raise SystemExit("migrate: patterns/model.json is already major/minor")
    next_id = max(e["id"] for e in progs.values()) + 1
    out, added, mapping, report = {}, {}, [], []
    for k, e in progs.items():
        spellings = []                                   # [(tonality, chords, [old modes])], first mode first
        for mode in sorted(OLD_MODES.index(m) for m in e["modes"]):
            chords = [respell(mode, c) for c in e["chords"]]
            if None in chords:
                raise SystemExit(f"migrate: progression {k} in {OLD_MODES[mode]}: a chord with no exact respelling")
            same = next((s for s in spellings if s[0] == TONALITY[mode] and s[1] == chords), None)
            if same:
                same[2].append(mode)
            else:
                spellings.append((TONALITY[mode], chords, [mode]))
        for i, (ton, chords, modes) in enumerate(spellings):
            nid = e["id"] if i == 0 else next_id
            if i:
                next_id += 1
            names = ", ".join(OLD_MODES[m][5:].capitalize() for m in modes)
            ne = {x: v for x, v in e.items() if x not in ("symbols", "symbols_mode", "file")}
            ne.update(id=nid, chords=chords, modes=[validate.MODES[ton]])
            if i:                                        # (its file named as export.py names a new entry's)
                ne["src"] = f"split from {e['id']} ({names})"      # (short: the firmware holds it)
                ne["status"] = e["status"]
                syms = [export.ROOT_NAME[r] + x for r, x in (export.chord_symbol(c, ton) for c in chords)]
                ne["file"] = (f"progressions/{nid}-{e['kind'][3:].lower()}-" +
                              "-".join(s.lower().replace("#", "s") for s in syms))
            else:
                ne["file"] = e["file"]
            (out if i == 0 else added)[str(nid)] = ne
            for m in modes:
                mapping.append({"id": e["id"], "mode": OLD_MODES[m], "chords": e["chords"],
                                "pcs": [[old_chord_pcs(m, c, color) for color in range(3)] for c in e["chords"]],
                                "new_id": nid, "new_mode": validate.MODES[ton]})
            report.append(f"{e['id']:>2} {'/'.join(OLD_MODES[m][5:8] for m in modes):<7} -> {nid:>2} "
                          f"{validate.MODES[ton][5:].lower()}: " +
                          " ".join(validate.numeral(ton, c) for c in chords) + (" (split)" if i else ""))
    model["progressions"] = {**out, **added}
    path.write_text(json.dumps(model, indent=2) + "\n")
    FIXTURE.write_text(json.dumps({"_note": FIXTURE_NOTE, "progressions": mapping}, indent=1) + "\n")
    for line in report:
        print(line)
    if map_file:                                         # (old mode index, old id, new mode index, new id)
        Path(map_file).write_text("".join(f"{OLD_MODES.index(x['mode'])} {x['id']} "
                                          f"{validate.MODES.index(x['new_mode'])} {x['new_id']}\n" for x in mapping))
    print(f"migrate: {len(progs)} progressions -> {len(model['progressions'])} ({len(added)} split)")


if __name__ == "__main__":
    main(sys.argv)
