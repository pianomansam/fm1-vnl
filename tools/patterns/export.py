#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Writes every pattern table as a .mid and a .musicxml (patterns/<family>/<id>-<slug>), the model (model.json: every
   field of every entry, keyed by id in table order, each with a status) and .manifest.json (each pattern file's
   SHA-256 as written). The conventions are patterns/README.md's. It never deletes a file, and never renames one:
   an entry keeps the file name model.json gives it. A file whose SHA-256 differs from the manifest's (Sam edited it)
   is left untouched and named in the output.
   python3 -I tools/patterns/export.py TABLES_JSON OUT   (export.sh builds dump and runs this)"""
import hashlib
import json
import os
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import midi  # noqa: E402
import mxl  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent.parent
TEMPO = 85
TICKS = 120                                     # a 16th at PPQ 480
DRUM_TICKS = 60                                 # score.c's drum note length
HIT_VEL, GHOST_VEL = 90, 40                     # a drum hit (> 60) and a ghost (<= 60)

PF = ["PF_BOOMBAP", "PF_HALFTIME", "PF_SPARSE", "PF_BRUSHES", "PF_BOSSA"]
PF_SLUG = ["boombap", "half-time", "sparse", "brushes", "bossa"]
PK = ["PK_VAMP", "PK_LOOP", "PK_JAZZY"]
Q = ["Q_DIATONIC", "Q_MIN7", "Q_DOM7", "Q_MAJ7"]
MOODS = ["MOOD_SAD", "MOOD_WISTFUL", "MOOD_WARM", "MOOD_SUNNY"]
MODES = ["MODE_IONIAN", "MODE_DORIAN", "MODE_PHRYGIAN", "MODE_LYDIAN", "MODE_MIXOLYDIAN", "MODE_AEOLIAN"]
SCALE = [[0, 2, 4, 5, 7, 9, 11], [0, 2, 3, 5, 7, 9, 10], [0, 1, 3, 5, 7, 8, 10], [0, 2, 4, 6, 7, 9, 11],
         [0, 2, 4, 5, 7, 9, 10], [0, 2, 3, 5, 7, 8, 10]]
ROOT_NAME = ["C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"]
QUALITY = {  # symbol suffix: (tones above the root, MusicXML kind)
    "maj7": ([0, 4, 7, 11], "major-seventh"), "7": ([0, 4, 7, 10], "dominant"),
    "m7": ([0, 3, 7, 10], "minor-seventh"), "m7b5": ([0, 3, 6, 10], "half-diminished")}

# lanes: (key, name, MIDI note, display step, display octave, notehead)
DRUM_LANES = [("kick", "Kick", 36, "F", 4, None), ("snare", "Snare", 38, "C", 5, None),
              ("hat", "Closed hat", 42, "G", 5, "x"), ("ohat", "Open hat", 46, "A", 5, "circle-x"),
              ("shaker", "Shaker", 70, "E", 5, "triangle"), ("rim", "Rim", 37, "B", 4, "x")]
STROKES = [("T", "Thumb", 64, "E", 4, None), ("B", "Thumb and pinch", 65, "F", 4, None),
           ("D", "Down, accented", 67, "G", 4, None), ("d", "Down", 69, "A", 4, None),
           ("X", "Chuck", 71, "B", 4, "x"), ("U", "Up", 72, "C", 5, None), ("u", "Up, soft", 74, "D", 5, None),
           ("P", "Pinch", 76, "E", 5, None)]
TONE = [60, 64, 67, 71]                         # the reference Cmaj7: chord tones 1-4 = C4 E4 G4 B4

STRUM_SLUG = ["d-du-udu", "chuck", "bossa", "ballad"]
SP_N = 4                                        # strum.h's approved patterns; the ballad's fill holds id SP_N + 1
SP_BOSSA = 2
# the fill figures (kind "fill"): (id, figure, fill_of, table index, slug). The ballad's (fill_of: its pattern's id),
# then step back and drive (Sam, 2026-11-04) for every other pattern (fill_of 0) and the bossa (its thumb and
# pinches), given the ids after the 15 patterns of the day: no pattern is renumbered (Sam's approvals are keyed by id)
FIGURE = {"step": "step-back", "drive": "drive"}   # the tables' names (strum_fill_*) to the model's figures
FIGURE_OF = {v: k for k, v in FIGURE.items()}
FILLS = [(SP_N + 1, None, SP_N, 0, "ballad-fill"), (16, "step-back", 0, 0, "fill-step-back"),
         (17, "drive", 0, 0, "fill-drive"), (18, "step-back", SP_BOSSA + 1, 1, "bossa-fill-step-back"),
         (19, "drive", SP_BOSSA + 1, 1, "bossa-fill-drive")]
FILL_IDS = {f[0] for f in FILLS}


# metadata keys that differ from the entry's: "family" in the metadata is the pattern family (the folder)
META_KEY = {"family": "pattern_family"}
META_KEY_OF = {v: k for k, v in META_KEY.items()}


def hx(v, width=4):
    return f"0x{v:0{width}X}"


def status(v):
    """a generated *_STATUS value: 1 approved (the engine draws it), 0 candidate, 2 retired"""
    return {1: "approved", 2: "retired"}.get(v, "candidate")


def strum_id(index):
    """a strum pattern's id: the index-th (from 0) id from 1 that no fill holds (index + 1 for the first SP_N, + 2 past
       the ballad's fill, and so on)"""
    k = 0
    while True:
        k += 1
        if k not in FILL_IDS:
            if index == 0:
                return k
            index -= 1


def chord_symbol(spec, mode):
    deg, alter, q = spec["degree"], spec["alter"], spec["quality"]
    sc = SCALE[mode]
    root = (sc[deg] + alter) % 12
    if q == "Q_DIATONIC":
        third, fifth, sev = ((sc[(deg + k) % 7] - sc[deg]) % 12 for k in (2, 4, 6))
        suffix = {(4, 7, 11): "maj7", (4, 7, 10): "7", (3, 7, 10): "m7", (3, 6, 10): "m7b5"}[(third, fifth, sev)]
    else:
        suffix = {"Q_MIN7": "m7", "Q_DOM7": "7", "Q_MAJ7": "maj7"}[q]
    return root, suffix


# ---------------------------------------------------------------------------------------------------- the model
def build_model(t, kick_phrases):
    """the tables (dump.c's JSON, or import.py's rows_from_model) and options.json's kickPhrases -> the model"""
    m = {"_note": "Generated by tools/patterns/export.py from the engine's tables; patterns/README.md has the "
                  "conventions. Entries are keyed by id, in table order."}
    m["drums"] = {}
    for p in t["patterns"]:
        m["drums"][str(p["id"])] = {
            "id": p["id"], "status": status(p["status"]), "family": PF[p["family"]], "hit": [hx(v) for v in p["hit"]],
            "ghost": [hx(v) for v in p["ghost"]], "half_time": p["half_time"], "poly3": p["poly3"], "src": p["src"],
            "two_bar": p["two_bar"], "hit_b": [hx(v) for v in p["hit_b"]], "ghost_b": [hx(v) for v in p["ghost_b"]],
            "file": f"drums/{p['id']}-{PF_SLUG[p['family']]}"}
    m["kick-phrases"] = {}
    for k in kick_phrases:
        slug = "-".join(map(str, k["a"])) + ("" if k["a"] == k["b"] else "-then-" + "-".join(map(str, k["b"])))
        m["kick-phrases"][k["id"]] = {**k, "status": k.get("status", "approved"),
                                      "file": f"kick-phrases/{k['id']}-{slug}"}
    m["progressions"] = {}
    for p in t["progs"]:
        chords = [{"degree": c[0], "alter": c[1], "quality": Q[c[2]]} for c in p["c"][:p["n"]]]
        if any(c != [0, 0, 0] for c in p["c"][p["n"]:]):
            raise SystemExit(f"export: progression {p['id']} has chords past its n")
        modes = [MODES[i] for i in range(len(MODES)) if p["modes"] >> i & 1]
        first = next(i for i in range(len(MODES)) if p["modes"] >> i & 1)
        symbols = [ROOT_NAME[r] + s for r, s in (chord_symbol(c, first) for c in chords)]
        slug = PK[p["kind"]][3:].lower() + "-" + "-".join(s.lower().replace("#", "s") for s in symbols)
        m["progressions"][str(p["id"])] = {
            "id": p["id"], "status": status(p["status"]), "kind": PK[p["kind"]], "n": p["n"],
            "bars_per_chord": p["bars_per_chord"], "chords": chords,
            "moods": [MOODS[i] for i in range(4) if p["moods"] >> i & 1], "modes": modes, "src": p["src"],
            "symbols": symbols, "symbols_mode": MODES[first], "file": f"progressions/{p['id']}-{slug}"}
    c = t["comp"]
    st = c["stabs"]

    def strikes(k, name, style):
        """a strikes rhythm's entry: second_vel_pct only where it says something"""
        x = c[name]
        e = {"name": name, "status": "approved", "style": style, "vel_range": x["vel"]}
        if x["later_vel_pct"] != 100:
            e["second_vel_pct"] = x["later_vel_pct"]
        e["strikes"] = [{"step": s, "len": n} for s, n in zip(x["steps"], x["lens"])]
        e["file"] = f"comp-rhythms/{k}-{name}"
        return e
    m["comp-rhythms"] = {
        "1": strikes(1, "held", "CS_HELD"), "2": strikes(2, "half", "CS_HALF"),
        "3": {"name": "stabs", "status": "approved", "style": "CS_STABS", "vel_range": st["vel"], "len": st["len"],
              "first": st["first"], "second": st["second"], "busy_over": st["busy_over"],
              "busy_steps": st["busy_steps"], "file": "comp-rhythms/3-stabs"},
        "4": strikes(4, "held-on-2", "CS_HELD2"), "5": strikes(5, "light-eighths", "CS_LIGHT8"),
        "6": strikes(6, "held-on-2-and-4", "CS_HELD24")}
    strum = {}
    for i, s in enumerate(t["strum"]):
        k = strum_id(i)
        strum[k] = {"index": i, "status": status(t["strum_status"][i]), "kind": "pattern", "steps": s,
                    "file": f"strum/{k}-{STRUM_SLUG[i] if i < len(STRUM_SLUG) else 'pattern'}"}
    for k, fig, of, i, slug in FILLS:
        steps = t["strum_ballad_fill"] if fig is None else t["strum_fill_" + FIGURE_OF[fig]][i]
        strum[k] = {"index": i, "status": "approved", "kind": "fill", **({"figure": fig} if fig else {}),
                    "fill_of": of, "steps": steps, "file": f"strum/{k}-{slug}"}
    m["strum"] = {str(k): strum[k] for k in sorted(strum)}
    return m


# ---------------------------------------------------------------------------------------------------- the files
def lane_part(name, lanes, hits):
    """hits: [(step, lane key, vel, ghost)] -> an mxl percussion part"""
    by = {}
    for s, key, vel, ghost in hits:
        by.setdefault(s, []).append((key, vel, ghost))
    order = [ln[0] for ln in lanes]
    notes = [{"start": s, "len": 1, "hits": sorted(h, key=lambda x: order.index(x[0]))} for s, h in sorted(by.items())]
    return {"name": name, "notes": notes, "lanes": [{"key": k, "name": n, "midi": p, "step": st, "octave": o,
                                                     "notehead": nh} for k, n, p, st, o, nh in lanes]}


def lane_track(name, lanes, hits, ticks):
    note = {ln[0]: ln[2] for ln in lanes}
    return {"name": name, "channel": 9, "program": None,
            "notes": [(s * TICKS, ticks, note[k], v) for s, k, v, _ in sorted(hits, key=lambda h: (h[0], note[h[1]]))]}


def pitched(name, program, notes):
    """notes: [(step, len, [pitches], vel)] -> (midi track, mxl part)"""
    tr = {"name": name, "channel": 0, "program": program,
          "notes": [(s * TICKS, n * TICKS, p, v) for s, n, ps, v in notes for p in ps]}
    part = {"name": name, "program": program,
            "notes": [{"start": s, "len": n, "hits": [(p, v, False) for p in ps]} for s, n, ps, v in notes]}
    return tr, part


def render(fam, k, e):
    """-> (title, bars, midi tracks, mxl parts, chords, midi texts)"""
    if fam == "drums":
        hits = []
        for bar in range(2):
            b = bar == 1 and e["two_bar"]
            for lane, (key, *_r) in enumerate(DRUM_LANES):
                if e["poly3"] and key == "shaker":                # the period-3 polymeter, written out
                    hits += [(bar * 16 + s, key, HIT_VEL, False) for s in range(16) if (bar * 16 + s) % 3 == 0]
                    continue
                hit = int((e["hit_b"] if b else e["hit"])[lane], 16)
                ghost = int((e["ghost_b"] if b else e["ghost"])[lane], 16)
                if hit & ghost:
                    raise SystemExit(f"export: drums {k} lane {key}: a step both hit and ghost")
                hits += [(bar * 16 + s, key, HIT_VEL, False) for s in range(16) if hit >> s & 1]
                hits += [(bar * 16 + s, key, GHOST_VEL, True) for s in range(16) if ghost >> s & 1]
        title = f"Drums {k}: " + e["file"].split("-", 1)[1].replace("-", " ") + (" (poly3 shaker)" if e["poly3"] else "")
        return (title, 2, [lane_track("Drums", DRUM_LANES, hits, DRUM_TICKS)],
                [lane_part("Drums", DRUM_LANES, hits)], None, ())
    if fam == "kick-phrases":
        lanes = DRUM_LANES[:1]
        hits = [(s, "kick", HIT_VEL, False) for s in e["a"]] + [(16 + s, "kick", HIT_VEL, False) for s in e["b"]]
        return (f"Kick phrase {k}", 2, [lane_track("Kick", lanes, hits, DRUM_TICKS)],
                [lane_part("Kick", lanes, hits)], None, ())
    if fam == "strum":
        hits = [(i, c, 80, False) for i, c in enumerate(e["steps"]) if c != "."]
        title = f"Strum {k}: " + e["file"].split("-", 1)[1].replace("-", " ")
        return (title, len(e["steps"]) // 16, [lane_track("Strum", STROKES, hits, DRUM_TICKS)],
                [lane_part("Strum", STROKES, hits)], None, ())
    if fam == "progressions":
        notes, chords, texts, step = [], [], [], 0
        for spec, sym in zip(e["chords"], e["symbols"]):
            root, suffix = chord_symbol(spec, MODES.index(e["symbols_mode"]))
            base = (48 if root >= 5 else 60) + root
            length = e["bars_per_chord"] * 16
            notes.append((step, length, [base + i for i in QUALITY[suffix][0]], 60))
            ra = mxl.SPELL[root]
            chords.append({"start": step, "root_step": ra[0], "root_alter": ra[1], "kind": QUALITY[suffix][1],
                           "text": suffix})
            texts.append((step * TICKS, sym))
            step += length
        tr, part = pitched("Chords", 4, notes)
        moods = ", ".join(x[5:].lower() for x in e["moods"])
        modes = ", ".join(x[5:].capitalize() for x in e["modes"])
        title = f"Progression {k}: {' '.join(e['symbols'])} ({e['kind'][3:].lower()}; {moods}; {modes})"
        return title, step // 16, [tr], [part], chords, texts
    if fam == "comp-rhythms":
        lo, hi = e["vel_range"]
        vel = (lo + hi) // 2
        if e["name"] == "stabs":
            notes = [(bar * 16 + s, e["len"], TONE, vel)
                     for bar, s2 in enumerate(e["second"]) for s in (e["first"], s2)]
            bars = len(e["second"])
        else:
            notes = [(x["step"], x["len"], TONE, vel * e.get("second_vel_pct", 100) // 100 if i else vel)
                     for i, x in enumerate(e["strikes"])]
            bars = 1
        tr, part = pitched("Keys", 4, notes)
        return f"Comp rhythm {k}: {e['name']} over Cmaj7", bars, [tr], [part], None, ()
    raise ValueError(fam)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_file(out, fam, k, e, ext):
    """writes one of the entry's files (ext: .mid or .musicxml), whatever is there"""
    title, bars, tracks, parts, chords, texts = render(fam, k, e)
    meta = json.dumps({"family": fam, "id": k, **{META_KEY.get(x, x): y for x, y in e.items()
                                                   if x not in ("file", "id")}}, separators=(",", ":"))
    path = Path(str(out / e["file"]) + ext)
    path.parent.mkdir(parents=True, exist_ok=True)
    if ext == ".mid":
        midi.write(str(path), tracks, ppq=480, title=title, meta=meta, tempo_bpm=TEMPO, texts=texts)
    else:
        mxl.write(str(path), parts, chords=chords, title=title, meta=meta, tempo_bpm=TEMPO, bars=bars)


def write_entry(out, fam, k, e, manifest):
    """writes the entry's two files, except one Sam edited (its SHA-256 differs from the manifest's, or it has none):
       that one is left as it is (importing is the path for edits). -> (paths written, paths skipped)"""
    written, skipped = [], []
    for ext in (".mid", ".musicxml"):
        rel, path = e["file"] + ext, out / (e["file"] + ext)
        if path.exists() and manifest.get(rel) != sha(path):
            skipped.append(rel)
            continue
        write_file(out, fam, k, e, ext)
        written.append(rel)
    return written, skipped


def load(path):
    return json.loads(path.read_text()) if path.exists() else {}


def main(argv):
    if len(argv) != 3:
        raise SystemExit(__doc__)
    tables = json.loads(Path(argv[1]).read_text())
    options = json.loads((ROOT / "workbench/options.json").read_text())
    model = build_model(tables, options["kickPhrases"])
    out = Path(argv[2])
    out.mkdir(parents=True, exist_ok=True)
    old_model, manifest = load(out / "model.json"), load(out / ".manifest.json")
    written, skipped = [], []
    for fam, entries in model.items():
        if fam.startswith("_"):
            continue
        for k, e in entries.items():
            prev = old_model.get(fam, {}).get(k)
            if prev and prev.get("file"):
                e["file"] = prev["file"]                 # a file's name is fixed when first exported, never renamed
            w, sk = write_entry(out, fam, k, e, manifest)
            written += w
            skipped += sk
    (out / "model.json").write_text(json.dumps(model, indent=2) + "\n")
    manifest.update({rel: sha(out / rel) for rel in written})
    (out / ".manifest.json").write_text(json.dumps(dict(sorted(manifest.items())), indent=2) + "\n")
    print(f"export: {len(written)} files in {out}")
    for rel in skipped:
        print(f"export: skipped {rel}: edited since it was written (import it with tools/patterns/import.sh)")


if __name__ == "__main__":
    main(sys.argv)
