#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Imports patterns/ (the .mid and .musicxml files Sam edits or adds) into the engine's tables: app/song/patterns_gen.c
   and .h (generated; never edit them), workbench/options.json's kickPhrases, patterns/model.json and .manifest.json.

   Which file to read: .manifest.json holds each file's SHA-256 as last written. A pattern whose files both match is
   unchanged (model.json has it). One changed (or a new pattern's only file) is imported from that file; both changed
   must agree, else the import is refused naming both. After an import the pattern's other file is rewritten from the
   imported entry and both hashes go in the manifest. A file name not in model.json is a new pattern: the next free
   id, status candidate. patterns/status.json ({family: {id: "approved" | "candidate" | "retired"}}, the workbench's
   Approve button writes it) sets an entry's status; the engine draws approved entries only. Every entry is checked
   against the engine's rules (validate.py); any refusal names the file, the bar and the rule, and nothing is written.

   python3 -I tools/patterns/import.py [--check] [--patterns DIR] [--gen DIR] [--options FILE]
     --check: read and validate only, write nothing (exit 1 on a refusal)"""
import copy
import hashlib
import json
import os
import re
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import export  # noqa: E402
import midi  # noqa: E402
import mxl  # noqa: E402
import validate  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent.parent
FAMS = ["drums", "kick-phrases", "progressions", "comp-rhythms", "strum"]
# families the engine no longer has (the lead styles replaced the hybrid lead's arpeggio and the struck melody,
# 2026-10-31): a file in one is refused, naming why
REMOVED = {"lead-arp": "the lead styles replaced the hybrid lead's arpeggio",
           "melody-cells": "the lead styles replaced the struck melody"}
LANE_FAMS = {"drums", "kick-phrases", "strum"}
EXTS = (".mid", ".musicxml")
DERIVED = ("status", "file", "symbols", "symbols_mode", "onsets")
HASHES = "the pinned song hashes (tests/host/song_test.c's strum_same)"
STATUS = {"approved": 1, "candidate": 0, "retired": 2}     # the generated *_STATUS values (the engine draws 1 only)
# what a pattern sounds like, to tell a renamed or saved-as copy of an entry
SOUND_KEYS = {"drums": ("hit", "ghost", "hit_b", "ghost_b", "poly3", "two_bar"), "kick-phrases": ("a", "b"),
              "progressions": ("n", "bars_per_chord", "chords"), "comp-rhythms": ("strikes", "second", "first"),
              "strum": ("kind", "steps")}
NOTICES = []                                    # (bar, note) from the file being decoded

DRUM_LANE = {ln[2]: i for i, ln in enumerate(export.DRUM_LANES)}
STROKE_OF = {ln[2]: ln[0] for ln in export.STROKES}
XML_LANES = {"drums": {(ln[3], ln[4]): ln[2] for ln in export.DRUM_LANES},
             "kick-phrases": {(ln[3], ln[4]): ln[2] for ln in export.DRUM_LANES},
             "strum": {(ln[3], ln[4]): ln[2] for ln in export.STROKES}}
SHAPES = [("maj7", {0, 4, 7, 11}), ("7", {0, 4, 7, 10}), ("m7", {0, 3, 7, 10}), ("m7b5", {0, 3, 6, 10})]
EXTENSIONS = {1, 2, 3, 5, 6, 8, 9}              # 9ths, 11ths, 13ths over a 7th chord
TITLE_RE = re.compile(r"^Progression [^:]*: .*\((vamp|loop|jazzy); ([a-z, ]+); ([A-Za-z, ]+)\)\s*$")
META_FIELDS = {"drums": ("pattern_family", "half_time", "src"), "kick-phrases": (), "progressions": ("kind", "moods", "modes",
               "src"), "comp-rhythms": ("vel_range", "second_vel_pct", "busy_over", "busy_steps"), "strum": ()}
# the fixed comp rhythms written as strikes in a bar (the stabs are their own shape)
STRIKES = ("held", "half", "held-on-2", "held-on-2-and-4")


class Refused(Exception):
    def __init__(self, bar, rule):
        super().__init__(rule)
        self.bar, self.rule = bar, rule


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def content(e):
    return {k: v for k, v in e.items() if k not in DERIVED}


# ------------------------------------------------------------------------------------------------- reading a file
def score_of(path, fam):
    """-> {"title", "meta" (dict or None), "notes": [{start, len, pitch, vel, ghost}]}, in 16th steps"""
    if path.suffix == ".mid":
        m = midi.read(str(path))
        ppq = m["ppq"]
        lane = fam in LANE_FAMS
        want = [n for n in m["notes"] if (n["channel"] == 9) == lane and (lane or n["channel"] == 0)]
        if not want:      # a DAW may move the part: drums off channel 10, a pitched part off channel 1
            want = [n for n in m["notes"] if lane or n["channel"] != 9]
        if m["time_sig"] not in (None, (4, 4)):
            raise Refused(1, f"4/4 only (the file is {m['time_sig'][0]}/{m['time_sig'][1]})")
        notes = []
        for n in want:
            if n["tick"] * 4 % ppq:
                raise Refused(n["tick"] * 4 // ppq // 16 + 1, "a note off the 16th grid (quantize to 16ths)")
            notes.append({"start": n["tick"] * 4 // ppq, "len": max(1, round(n["len"] * 4 / ppq)), "pitch": n["note"],
                          "vel": n["vel"], "ghost": False})
        title, meta = m["title"], m["meta"]
    else:
        try:
            x = mxl.read(str(path), XML_LANES.get(fam))
        except mxl.OffGrid as e:
            raise Refused(e.bar, str(e) + " (quantize to 16ths)") from None
        notes, title, meta = x["notes"], x["title"], x["meta"]
    try:
        meta = json.loads(meta) if meta and len(meta) < 65536 else None
    except ValueError:
        meta = None
    if isinstance(meta, dict) and fam == "drums" and meta.get("family") in validate.PF:
        meta = {**meta, "family": fam, "pattern_family": meta["family"]}    # metadata written before pattern_family
    if not isinstance(meta, dict) or meta.get("family") != fam:
        meta = None
    notes.sort(key=lambda n: (n["start"], n["pitch"]))
    return {"title": title, "meta": meta, "notes": notes}


def bar_of(step):
    return step // 16 + 1


def only_bars(notes, n, what):
    for x in notes:
        if x["start"] >= 16 * n:
            raise Refused(bar_of(x["start"]), f"{what} is {n} bar{'s' if n > 1 else ''} long: nothing after bar {n}")


def one_per_onset(notes, what):
    seen = {}
    for x in notes:
        if x["start"] in seen:
            raise Refused(bar_of(x["start"]), f"{what}: one note at a time (two at step {x['start'] % 16})")
        seen[x["start"]] = x
    return list(seen.values())


# ------------------------------------------------------------------------------------------------------ decoding
def decode(fam, key, score, base):
    """the file's notes over base's other fields -> a model entry (without file, status or derived fields)"""
    e = {k: v for k, v in base.items() if k not in DERIVED}
    notes = score["notes"]
    if fam == "drums":
        hit, ghost = [[0] * 6, [0] * 6], [[0] * 6, [0] * 6]
        only_bars(notes, 2, "a drum pattern")
        for x in notes:
            if x["pitch"] not in DRUM_LANE:
                raise Refused(bar_of(x["start"]), f"note {x['pitch']} is not a drum lane (kick 36, snare 38, closed "
                                                  "hat 42, open hat 46, shaker 70, rim 37)")
            g = x["ghost"] or (x["vel"] is not None and x["vel"] <= 60)
            (ghost if g else hit)[x["start"] // 16][DRUM_LANE[x["pitch"]]] |= 1 << x["start"] % 16
        for b in range(2):
            ghost[b] = [g & ~h for g, h in zip(ghost[b], hit[b])]
        shaker = {b * 16 + s for b in range(2) for s in range(16) if hit[b][4] >> s & 1}
        poly3 = int(shaker == {s for s in range(32) if s % 3 == 0} and not (ghost[0][4] | ghost[1][4]))
        if poly3:
            hit[1][4] = hit[0][4]                       # the polymeter runs on: bar 2's shaker is bar 1's
        two = int(hit[0] != hit[1] or ghost[0] != ghost[1])
        hx = [export.hx(v) for v in (0,) * 6]
        e.update({"hit": [export.hx(v) for v in hit[0]], "ghost": [export.hx(v) for v in ghost[0]], "poly3": poly3,
                  "two_bar": two, "hit_b": [export.hx(v) for v in hit[1]] if two else hx,
                  "ghost_b": [export.hx(v) for v in ghost[1]] if two else hx})
    elif fam == "kick-phrases":
        only_bars(notes, 2, "a kick phrase")
        for x in notes:
            if x["pitch"] != 36:
                raise Refused(bar_of(x["start"]), f"a kick phrase is the kick (36) only, not note {x['pitch']}")
        e["a"] = sorted({x["start"] for x in notes if x["start"] < 16})
        e["b"] = sorted({x["start"] - 16 for x in notes if x["start"] >= 16})
    elif fam == "strum":
        only_bars(notes, 2, "a strum pattern")
        steps = ["."] * (32 if any(x["start"] >= 16 for x in notes) else 16)
        for x in one_per_onset(notes, "a strum pattern"):
            if x["pitch"] not in STROKE_OF:
                raise Refused(bar_of(x["start"]), f"note {x['pitch']} is not a stroke lane (T 64, B 65, D 67, d 69, "
                                                  "X 71, U 72, u 74, P 76)")
            steps[x["start"]] = STROKE_OF[x["pitch"]]
        e["steps"] = "".join(steps)
    elif fam == "progressions":
        decode_prog(e, score, notes)
    elif fam == "comp-rhythms":
        try:                                            # the file's velocities are placeholders (vel_range's middle)
            mid = sum(e["vel_range"]) // 2
            later = mid * e.get("second_vel_pct", 100) // 100 if e.get("name") != "stabs" else mid
            first = min((x["start"] for x in notes), default=0)
            if any(x["vel"] is not None and x["vel"] != (mid if x["start"] == first else later) for x in notes):
                NOTICES.append((None, "velocities are ignored: the comp rhythms' vel_range and second_vel_pct are "
                                      "metadata (model.json)"))
        except (TypeError, KeyError, ValueError):
            pass
        if e.get("name") in STRIKES or e.get("kind") == "pattern":
            only_bars(notes, 1, f"{e.get('name', 'a comp rhythm')} comping")
            by = {}
            for x in notes:
                by[x["start"]] = max(by.get(x["start"], 0), x["len"])
            e["strikes"] = [{"step": s, "len": n} for s, n in sorted(by.items())]
        else:
            bars = sorted({x["start"] // 16 for x in notes})
            if bars != list(range(len(bars))):
                raise Refused(len(bars) + 1, "the stabs are one bar per second-stab candidate, with no empty bar")
            firsts, second = set(), []
            for b in bars:
                ons = sorted({x["start"] % 16 for x in notes if x["start"] // 16 == b})
                if len(ons) != 2:
                    raise Refused(b + 1, "each bar of the stabs is the first stab and one second-stab candidate")
                firsts.add(ons[0])
                second.append(ons[1])
            if len(firsts) != 1:
                raise Refused(1, "the first stab is on the same step in every bar")
            e["first"] = firsts.pop()
            e["second"] = second
            e["len"] = next(x["len"] for x in notes if x["start"] == e["first"])
    return e


def decode_prog(e, score, notes):
    m = TITLE_RE.match(score["title"] or "")
    if m:                                               # the title is what Sam sees and edits: it wins
        e["kind"] = "PK_" + m.group(1).upper()
        e["moods"] = ["MOOD_" + x.strip().upper() for x in m.group(2).split(",") if x.strip()]
        e["modes"] = ["MODE_" + x.strip().upper() for x in m.group(3).split(",") if x.strip()]
    modes = [x for x in e.get("modes") or [] if x in validate.MODES]
    if not modes:
        raise Refused(1, "no mode: name the modes in the title, as \"Progression N: ... (loop; warm; Dorian)\"")
    mode = validate.MODES.index(modes[0])
    by = {}
    for x in notes:
        by.setdefault(x["start"], []).append(x)
    if not by:
        raise Refused(1, "a progression has chords")
    starts = sorted(by)
    if starts[0] != 0:
        raise Refused(bar_of(starts[0]), "a progression starts on bar 1")
    lens = [max(x["len"] for x in by[s]) for s in starts]
    for i, s in enumerate(starts):
        if lens[i] != lens[0] or lens[i] % 16 or (i + 1 < len(starts) and starts[i + 1] != s + lens[i]):
            raise Refused(bar_of(s), "every chord lasts the same whole number of bars, one after another")
    hints = [c for c in (e.get("chords") or []) if isinstance(c, dict)]
    chords = []
    for i, s in enumerate(starts):
        root, suffix = chord_of(by[s], bar_of(s))
        chords.append(spec_for(root, suffix, mode, hints[i:i + 1] + hints, bar_of(s)))
    e["n"], e["bars_per_chord"], e["chords"] = len(starts), lens[0] // 16, chords


def chord_of(notes, bar):
    """(root pitch class, symbol suffix): the lowest note is the root; 9ths, 11ths and 13ths over the 7th chord are
       left to the engine (it colors every chord itself)"""
    lo = min(x["pitch"] for x in notes)
    ivs = {(x["pitch"] - lo) % 12 for x in notes}
    for suffix, shape in SHAPES:
        rest = ivs - shape
        if shape <= ivs and rest <= EXTENSIONS:
            if rest:
                NOTICES.append((bar, "written extensions are ignored (the engine colors every chord itself)"))
            return lo % 12, suffix
    raise Refused(bar, "not an allowed chord: a maj7, 7, m7 or m7b5 over its lowest note (no triads, no sus)")


def spec_for(root, suffix, mode, hints, bar):
    """the {degree, alter, quality} that sounds root+suffix in the mode: a hint that does (the metadata's, model.json's)
       first, since a mode's own chord and a forced quality look alike; else the mode's own chord on that root, else
       a forced quality on the nearest degree (flat before sharp)"""
    for h in hints:
        try:
            if export.chord_symbol(h, mode) == (root, suffix):
                return {"degree": h["degree"], "alter": h["alter"], "quality": h["quality"]}
        except (KeyError, IndexError, TypeError, ValueError):
            pass
    sc = validate.SCALE[mode]
    for alter in (0, -1, 1):
        if (root - alter) % 12 in sc:
            d = sc.index((root - alter) % 12)
            spec = {"degree": d, "alter": alter, "quality": "Q_DIATONIC"}
            if alter == 0 and export.chord_symbol(spec, mode) == (root, suffix):
                return spec
            q = {"maj7": "Q_MAJ7", "7": "Q_DOM7", "m7": "Q_MIN7"}.get(suffix)
            if q:
                return {"degree": d, "alter": alter, "quality": q}
    raise Refused(bar, f"a m7b5 only where the mode has it as its own chord ({validate.MODES[mode][5:].capitalize()})")


# ------------------------------------------------------------------------------------------- the model's tables
def rows_from_model(model):
    """model.json -> (the tables, in dump.c's JSON shape, kickPhrases)"""
    st = lambda e: STATUS[e["status"]]  # noqa: E731
    t = {"patterns": [], "progs": []}
    for e in model["drums"].values():
        t["patterns"].append({"id": e["id"], "family": validate.PF.index(e["family"]),
                              **{k: [int(x, 16) for x in e[k]] for k in ("hit", "ghost", "hit_b", "ghost_b")},
                              "half_time": e["half_time"], "poly3": e["poly3"], "src": e["src"],
                              "two_bar": e["two_bar"], "busy": e["busy"], "status": st(e)})
    for e in model["progressions"].values():
        c = [[x["degree"], x["alter"], validate.Q.index(x["quality"])] for x in e["chords"]]
        t["progs"].append({"id": e["id"], "kind": validate.PK.index(e["kind"]), "n": e["n"],
                           "bars_per_chord": e["bars_per_chord"], "c": c + [[0, 0, 0]] * (4 - len(c)),
                           "moods": sum(1 << validate.MOODS.index(x) for x in e["moods"]),
                           "modes": sum(1 << validate.MODES.index(x) for x in e["modes"]), "src": e["src"],
                           "status": st(e)})
    def by_index(fam, kind=None):
        es = [e for e in model[fam].values() if kind is None or e.get("kind") == kind]
        return sorted(es, key=lambda e: e["index"])
    comp = {e["name"]: e for e in model["comp-rhythms"].values() if e.get("kind") != "pattern"}
    rows = [comp[name] for _k, name, _s in export.COMP_FIXED_ROWS] + by_index("comp-rhythms", "pattern")
    t["comp"] = [{"style": export.CS.index(e["style"]), "weight": e["weight"], "busy": e["busy"],
                  "rules": export.CR_THIN * e["thin"] | export.CR_HOLD * e["hold"], "keys": export.keys_mask(e["keys"]),
                  "vel": e["vel_range"], "steps": [x["step"] for x in e.get("strikes", [])],
                  "lens": [x["len"] for x in e.get("strikes", [])], "later_vel_pct": e.get("second_vel_pct", 100),
                  "status": st(e)} for e in rows]
    s = comp["stabs"]
    t["stabs"] = {"first": s["first"], "second": s["second"], "len": s["len"], "busy_over": s["busy_over"],
                  "busy_steps": s["busy_steps"]}
    pats = by_index("strum", "pattern")
    t["strum"], t["strum_status"] = [e["steps"] for e in pats], [st(e) for e in pats]
    t["strum_busy"] = [e["busy"] for e in pats]
    fills = by_index("strum", "fill")
    t["strum_ballad_fill"] = next(e["steps"] for e in fills if "figure" not in e)
    for fig in ("step", "drive"):
        t[f"strum_fill_{fig}"] = [e["steps"] for e in fills if e.get("figure") == export.FIGURE[fig]]
    kicks = [{"id": e["id"], "a": e["a"], "b": e["b"], **({} if e["status"] == "approved" else {"status": e["status"]})}
             for e in model["kick-phrases"].values()]
    return t, kicks


# ------------------------------------------------------------------------------------------------ the C tables
def c_str(s):
    """a C string literal: backslash, quote and every '?' escaped (no trigraph such as ??/ can form, whatever the
       compiler's -std)"""
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"').replace("?", "\\?") + '"'


def c_list(xs, fmt="{}"):
    return "{" + ", ".join(fmt.format(x) for x in xs) + "}"


def bits(v, names, shift):
    on = [n for i, n in enumerate(names) if v >> i & 1]
    return " | ".join(f"(1 << {n})" if shift else n for n in on) or "0"


def gen_c(t, kicks=()):
    """(the tables, the kick phrases as rows_from_model gives them) -> (patterns_gen.c, patterns_gen.h)"""
    head = ("/* SPDX-License-Identifier: GPL-3.0-or-later */\n"
            "/* generated by tools/patterns/import.py; edit patterns/ (the .mid or .musicxml files:\n"
            "   patterns/README.md), then run tools/patterns/import.sh. Never edit this file. */\n")
    H = [head, "#ifndef SONG_PATTERNS_GEN_H", "#define SONG_PATTERNS_GEN_H", "#include <stdint.h>",
         '#include "library.h"', "",
         "/* Each *_STATUS runs parallel to its table: 1 approved (the roll draws it), 0 candidate (the workbench",
         "   only, until Sam approves it: patterns/status.json), 2 retired (never drawn). The comp rhythms and the",
         "   strum fills are fixed entries, always approved. */",
         "",
         "/* drums (LIB_PATTERNS) and progressions (LIB_PROGS): the types and the tables in library.h */",
         "extern const uint8_t LIB_PATTERNS_STATUS[];",
         "extern const uint8_t LIB_PROGS_STATUS[];",
         "/* each drum pattern's busyness, parallel to LIB_PATTERNS: 0 sparse, 1 middle, 2 busy (the roll's draw",
         "   leans on the song's motion: roll.h's motion_tilt) */",
         "extern const uint8_t LIB_PATTERNS_BUSY[];",
         "",
         "/* The keys' comping rhythms (patterns/comp-rhythms, harmony.c plays them), one table, one row each,",
         "   in id order: the fixed five (held, half, stabs, held on 2, held on 2 and 4: ids 1-6, 5 removed), then",
         "   the added ones (ids 7 on, played as CS_PAT). The roll draws one per song from the approved rows the",
         "   song's keys play (`keys`), at `weight` tilted by the song's motion and the row's `busy` (roll.h's",
         "   motion_tilt). A row's strikes in a bar: `later_vel_pct` the later strikes' velocity in percent of the",
         "   first's; len in 16ths (held: written a bar, the engine holds the chord; a strike as long as the gap to",
         "   the next, or to the bar's end, is held to it). The stabs' strikes are COMP_STABS's (n 0 here). rules:",
         "   CR_THIN, a busy lead thins it to its first strike (held to the next bar where it was held); CR_HOLD,",
         "   it holds instead over sustained keys or a kick of its very rhythm. */",
         "#define CR_THIN 1",
         "#define CR_HOLD 2",
         "/* keys \"all\": every keys instrument but the guitars (they only strum: CS_STRUM, its own engine) */",
         "#define CK_ALL (0xFFFFFFFFu & ~(1u << INS_NYLON | 1u << INS_EMILY))",
         "typedef struct {",
         "    uint8_t style;                      /* harmony.h's CS_* that plays it */",
         "    uint8_t weight;                     /* the draw's base weight (patterns/model.json: 1 each) */",
         "    uint8_t busy;                       /* 0 sparse, 1 middle, 2 busy (the motion tilt) */",
         "    uint8_t rules;                      /* CR_* */",
         "    uint32_t keys;                      /* the chords instruments that play it: 1u << INS_* */",
         "    uint8_t vel_lo, vel_hi;",
         "    uint8_t n;                          /* strikes */",
         "    uint8_t step[8], len[8];",
         "    uint8_t later_vel_pct;",
         "} patterns_comp_t;",
         "/* the stabs' strikes: one on `first`, the second on the first of `second` the kick leaves free;",
         "   over a kick of more than `busy_over` hits a bar, `busy_steps` (a mask). len: the written length (the",
         "   engine strikes 60% of a quarter) */",
         "typedef struct {",
         "    uint8_t first;",
         "    uint8_t n;                          /* second-stab candidates, tried in order */",
         "    uint8_t second[4];",
         "    uint8_t len;",
         "    uint8_t busy_over;",
         "    uint16_t busy_steps;",
         "} patterns_stabs_t;",
         "extern const patterns_stabs_t COMP_STABS;",
         "/* the fixed rows (always approved, first in every build) */",
         "enum { COMP_I_HELD, COMP_I_HALF, COMP_I_STABS, COMP_I_HELD2, COMP_I_HELD24, COMP_NFIXED };",
         "/* The host's and the workbench's builds hold every row (a candidate plays when the workbench forces it);",
         "   the firmware only the approved, the ones the roll draws: a candidate costs it nothing */",
         "#if defined PLAT_HOST || defined __EMSCRIPTEN__ || defined __APPLE__ || defined __linux__",
         "#define COMP_ALL 1",
         f"#define COMP_N {len(t['comp'])}",
         "#else",
         "#define COMP_ALL 0",
         f"#define COMP_N {sum(1 for x in t['comp'] if x['status'] == 1)}",
         "#endif",
         "extern const patterns_comp_t COMP[COMP_N];",
         "#if COMP_ALL",
         "extern const uint8_t COMP_STATUS[COMP_N];",
         "#define COMP_APPROVED(i) (COMP_STATUS[i] == 1)",
         "#else",
         "#define COMP_APPROVED(i) 1                     /* (the firmware's are all approved) */",
         "#endif",
         "#define COMP_HELD (COMP[COMP_I_HELD])",
         "#define COMP_HALF (COMP[COMP_I_HALF])",
         "#define COMP_HELD2 (COMP[COMP_I_HELD2])",
         "#define COMP_HELD24 (COMP[COMP_I_HELD24])",
         "/* the added rhythms (ids 7 on, CS_PAT), by their own index: the workbench's comp_pat override names one */",
         "#define COMP_NPAT (COMP_N - COMP_NFIXED)",
         "#define COMP_PAT (COMP + COMP_NFIXED)",
         "#define COMP_PAT_APPROVED(i) COMP_APPROVED(COMP_NFIXED + (i))",
         "",
         "/* strum.c's patterns (16 steps; 32 for a two-bar one) and the ballad's fill-bar variant */",
         f"#define STRUM_NPAT {len(t['strum'])}",
         "extern const char *const STRUM_PAT[STRUM_NPAT];",
         "extern const uint8_t STRUM_PAT_STATUS[STRUM_NPAT];",
         "extern const uint8_t STRUM_PAT_BUSY[STRUM_NPAT];   /* 0 sparse, 1 middle, 2 busy (the motion tilt) */",
         "extern const char STRUM_BALLAD_FILL[];",
         "/* the fill-bar figures a strummed song draws one of (16 steps): step back, a strike on 1 and space for the",
         "   drum fill, and drive, pushing into the change; [0] every pattern's but the ballad's (its own",
         "   fill) and the bossa's, [1] the bossa's (its thumb and pinches) */",
         f"#define STRUM_NFILL {len(t['strum_fill_step'])}",
         "extern const char *const STRUM_FILL_STEP[STRUM_NFILL];",
         "extern const char *const STRUM_FILL_DRIVE[STRUM_NFILL];",
         "",
         "/* The kick phrases (patterns/kick-phrases; options.json's kickPhrases, in its order): two bars of",
         "   kick, bit i = step i, bar A then bar B. A boom-bap song plays one in place of its pattern's kick now",
         "   and then (roll.h's lib_song_kick); the workbench and the device's kick row force one, approved or",
         "   candidate. */",
         "typedef struct { uint16_t a, b; } patterns_kick_t;",
         f"#define KICK_NPHRASE {max(len(kicks), 1)}",
         "extern const patterns_kick_t KICK_PHRASE[KICK_NPHRASE];",
         "extern const uint8_t KICK_PHRASE_STATUS[KICK_NPHRASE];",
         "",
         "#endif", ""]
    C = [head, '#include "patterns_gen.h"', '#include "harmony.h"', "",
         "/* Degrees are 0-based in the song's mode; {degree, alter, quality}. A progression lists only the modes",
         "   where its degrees give the chords named in src. R25: a vamp's or loop's chords lie in each mode it lists",
         "   at every color (Q_DIATONIC where the mode's own chord is the one named, so its color tones fit too); only",
         "   jazzy progressions borrow. */",
         "const lib_prog_t LIB_PROGS[] = {"]
    for p in t["progs"]:
        cs = ", ".join("{%d, %d, %s}" % (d, a, validate.Q[q] if (d, a, q) != (0, 0, 0) or i < p["n"] else "0")
                       for i, (d, a, q) in enumerate(p["c"]))
        C.append(f"    {{{p['id']}, {validate.PK[p['kind']]}, {p['n']}, {p['bars_per_chord']}, {{{cs}}}, "
                 f"{bits(p['moods'], validate.MOODS, False)}, {bits(p['modes'], validate.MODES, True)}, "
                 f"{c_str(p['src'])}}},")
    C += ["};", "const int LIB_NPROGS = (int)(sizeof LIB_PROGS / sizeof LIB_PROGS[0]);",
          "const uint8_t LIB_PROGS_STATUS[] = " + c_list(p["status"] for p in t["progs"]) + ";", "",
          "/* Lanes: kick, snare, hat, open hat, shaker, rim. 16 steps, bit i = step i. */",
          "const lib_pattern_t LIB_PATTERNS[] = {"]
    for p in t["patterns"]:
        m = {k: c_list(p[k], "0x{:04X}") for k in ("hit", "ghost", "hit_b", "ghost_b")}
        C.append(f"    {{{p['id']}, {validate.PF[p['family']]}, {m['hit']}, {m['ghost']}, {p['half_time']}, "
                 f"{p['poly3']}, {c_str(p['src'])}, {p['two_bar']}, {m['hit_b']}, {m['ghost_b']}}},")
    C += ["};", "const int LIB_NPATTERNS = (int)(sizeof LIB_PATTERNS / sizeof LIB_PATTERNS[0]);",
          "const uint8_t LIB_PATTERNS_STATUS[] = " + c_list(p["status"] for p in t["patterns"]) + ";",
          "const uint8_t LIB_PATTERNS_BUSY[] = " + c_list(p["busy"] for p in t["patterns"]) + ";", ""]
    pad = lambda v: list(v) + [0] * (8 - len(v))  # noqa: E731
    rows, cand = [], False
    for x in t["comp"]:                             # the firmware's (COMP_ALL 0): the approved alone
        if (x["status"] != 1) != cand:
            cand = not cand
            rows.append("#if COMP_ALL" if cand else "#endif")
        keys = "CK_ALL" if x["keys"] == validate.KEYS_ALL else \
            " | ".join(f"1u << {n}" for i, n in enumerate(validate.INS) if x["keys"] >> i & 1)
        rules = " | ".join(n for b, n in ((1, "CR_THIN"), (2, "CR_HOLD")) if x["rules"] & b) or "0"
        rows.append(f"    {{{export.CS[x['style']]}, {x['weight']}, {x['busy']}, {rules}, {keys}, {x['vel'][0]}, "
                    f"{x['vel'][1]}, {len(x['steps'])}, {c_list(pad(x['steps']))}, {c_list(pad(x['lens']))}, "
                    f"{x['later_vel_pct']}}},")
    if cand:
        rows.append("#endif")
    s = t["stabs"]
    C += ["const patterns_comp_t COMP[COMP_N] = {", *rows, "};", "#if COMP_ALL",
          "const uint8_t COMP_STATUS[COMP_N] = " + c_list(x["status"] for x in t["comp"]) + ";", "#endif",
          f"const patterns_stabs_t COMP_STABS = {{{s['first']}, {len(s['second'])}, "
          f"{c_list(list(s['second']) + [0] * (4 - len(s['second'])))}, {s['len']}, {s['busy_over']}, "
          f"0x{sum(1 << x for x in s['busy_steps']):04X}}};"]
    C += ["", "const char *const STRUM_PAT[STRUM_NPAT] = {"] + [f"    {c_str(x)}," for x in t["strum"]] + [
        "};", "const uint8_t STRUM_PAT_STATUS[STRUM_NPAT] = " + c_list(t["strum_status"]) + ";",
        "const uint8_t STRUM_PAT_BUSY[STRUM_NPAT] = " + c_list(t["strum_busy"]) + ";",
        f"const char STRUM_BALLAD_FILL[] = {c_str(t['strum_ballad_fill'])};",
        "const char *const STRUM_FILL_STEP[STRUM_NFILL] = {" + ", ".join(map(c_str, t["strum_fill_step"])) + "};",
        "const char *const STRUM_FILL_DRIVE[STRUM_NFILL] = {" + ", ".join(map(c_str, t["strum_fill_drive"])) + "};",
        ""]
    mask = lambda steps: sum(1 << x for x in steps)  # noqa: E731
    ks = list(kicks) or [{"a": [0], "b": [0], "status": "retired"}]     # (an empty table: one never drawn)
    C += ["const patterns_kick_t KICK_PHRASE[KICK_NPHRASE] = {"] + [
        f"    {{0x{mask(k['a']):04X}, 0x{mask(k['b']):04X}}},   /* {k.get('id', '-')} */" for k in ks] + [
        "};", "const uint8_t KICK_PHRASE_STATUS[KICK_NPHRASE] = " +
        c_list(STATUS[k.get("status", "approved")] for k in ks) + ";", ""]
    return "\n".join(C), "\n".join(H)


def option_blocks(model, text):
    """workbench/options.json's generated blocks: the kick phrases, the drum patterns (named as before; a new one by its
       file), and the strum patterns the workbench's override row forces ({id: the model's, index: the table's})"""
    old = {p["id"]: p["name"] for p in json.loads(text).get("patterns", [])}
    st = lambda e: f', "status": "{e["status"]}"'  # noqa: E731
    name = lambda e: json.dumps(e["file"].split("/", 1)[1])  # noqa: E731
    kicks = [f'    {{ "id": "{e["id"]}", "a": {json.dumps(e["a"])}, "b": {json.dumps(e["b"])}'
             f'{st(e) if e["status"] != "approved" else ""} }}' for e in model["kick-phrases"].values()]
    drums = [f'    {{ "id": {e["id"]}, "name": {json.dumps(old[e["id"]]) if e["id"] in old else name(e)}{st(e)} }}'
             for e in model["drums"].values()]

    def indexed(fam, kind):
        es = sorted((x for x in model[fam].items() if x[1].get("kind") == kind), key=lambda x: x[1]["index"])
        return [f'    {{ "id": "{k}", "index": {e["index"]}, "name": {name(e)}{st(e)} }}' for k, e in es]
    return {"patterns": drums, "kickPhrases": kicks, "strumPats": indexed("strum", "pattern"),
            "compRhythms": comp_rhythms(model)}


# the fixed comp rhythms' names in the workbench (an added one's from its file: "9-late-one" -> "Late one")
COMP_LABEL = {"held": "Held", "half": "Half notes", "stabs": "Stabs", "held-on-2": "Held on 2",
              "held-on-2-and-4": "Held on 2 and 4"}


def comp_rhythms(model):
    """options.json's compRhythms: every keys comping rhythm, in the engine's table order, as the Comping row offers
       it. value: the row's (a fixed rhythm's CS_* id, the COMP override; an added one's 100 + its index, the COMP_PAT
       override); keys: the keys instruments that play it (\"all\": notKeys, the guitars, which strum)"""
    es = model["comp-rhythms"]
    fixed = {e["name"]: (k, e) for k, e in es.items() if e.get("kind") != "pattern"}
    rows = [fixed[n] for _i, n, _s in export.COMP_FIXED_ROWS] + \
        sorted(((k, e) for k, e in es.items() if e.get("kind") == "pattern"), key=lambda x: x[1]["index"])
    out = []
    for k, e in rows:
        if e.get("kind") == "pattern":
            slug = re.sub(r"^\d+-", "", e["file"].split("/", 1)[1]).replace("-", " ")   # ("9-late-one": "Late one")
            name = json.dumps(slug[:1].upper() + slug[1:])
            head = f'"value": {100 + e["index"]}, "index": {e["index"]}, "name": {name}'
        else:
            head = f'"value": {export.CS.index(e["style"])}, "name": {json.dumps(COMP_LABEL[e["name"]])}'
        keys = (f'"notKeys": {json.dumps([validate.INS.index(g) for g in validate.GUITARS])}' if e["keys"] == "all"
                else f'"keys": {json.dumps(sorted(validate.INS.index(x) for x in e["keys"]))}')
        out.append(f'    {{ "id": "{k}", {head}, "status": "{e["status"]}", {keys} }}')
    return out


def options_text(text, blocks):
    """workbench/options.json with its generated blocks regenerated, a missing one added at the end (the rest of the
       file byte for byte)"""
    for key, lines in blocks.items():
        new, n = re.subn(r'("%s": \[\n)(.*?)(\n  \])' % key, lambda m: m.group(1) + ",\n".join(lines) + m.group(3),
                         text, count=1, flags=re.S)
        if n != 1:
            end = text.rstrip().rfind("}")
            new = text[:end].rstrip() + f',\n  "{key}": [\n' + ",\n".join(lines) + "\n  ]\n" + text[end:]
        text = new
    return text


# ------------------------------------------------------------------------------------------------- new entries
def defaults(fam, stem, model, meta):
    """a new pattern's entry before its notes: its key, the next free id (status candidate), and placeholder fields
       the metadata or the title may fill"""
    es = model[fam]
    src = f"imported from patterns/{stem}"
    if fam in ("drums", "progressions"):
        nid = max((e["id"] for e in es.values()), default=0) + 1
        if fam == "drums":
            slug = stem.split("/", 1)[1].lower()
            fam_name = next((validate.PF[i] for i, s in enumerate(export.PF_SLUG) if s in slug), "PF_BOOMBAP")
            return str(nid), {"id": nid, "family": fam_name, "half_time": 0, "src": src, "busy": 1}
        return str(nid), {"id": nid, "kind": "PK_JAZZY", "moods": [], "modes": [], "src": src}
    if fam == "kick-phrases":
        n = max((int(k[1:]) for k in es if k[1:].isdigit()), default=1) + 1
        return f"k{n}", {"id": f"k{n}"}
    if fam == "comp-rhythms":                       # a comp rhythm past the fixed six (CS_PAT): one bar of strikes
        n = sum(1 for e in es.values() if e.get("kind") == "pattern")
        return str(export.comp_pat_id(n)), {"style": "CS_PAT", "kind": "pattern", "index": n,
                                            **copy.deepcopy(export.COMP_PAT_DRAW),
                                            "vel_range": list(export.COMP_PAT_VEL)}
    if fam == "strum":
        n = sum(1 for e in es.values() if e["kind"] == "pattern")
        return str(export.strum_id(n)), {"index": n, "kind": "pattern", "busy": 1}
    raise Refused(None, f"no new {fam} pattern")


def with_meta(fam, base, meta):
    if meta:
        base = {**base, **{export.META_KEY_OF.get(k, k): meta[k] for k in META_FIELDS[fam] if k in meta}}
        if fam == "progressions" and isinstance(meta.get("chords"), list):
            base["chords"] = meta["chords"] + [c for c in base.get("chords") or [] if c not in meta["chords"]]
    return base


# -------------------------------------------------------------------------------------------------- status.json
def fixed(fam, e):
    """the six comp rhythms and the strum fills: fixed entries, always approved"""
    return (fam == "comp-rhythms" and e.get("kind") != "pattern") or e.get("kind") == "fill"


def set_status(pat, model, notes, refuse):
    """patterns/status.json ({family: {id: status}}, the workbench's Approve) onto the model's entries"""
    path = pat / "status.json"
    if not path.exists() and not path.is_symlink():
        return
    if path.is_symlink() or not path.is_file():
        return refuse(["status.json"], None, "not a plain file")
    try:
        want = json.loads(path.read_text(encoding="utf-8"))
    except (ValueError, UnicodeDecodeError) as err:
        return refuse(["status.json"], None, f"unreadable ({err})")
    if not isinstance(want, dict):
        return refuse(["status.json"], None, "not a JSON object of families")
    for fam, ids in want.items():
        if fam not in FAMS or not isinstance(ids, dict):
            notes.append(("status.json", None, f"no family {fam!r}; ignored"))
            continue
        for k, st in ids.items():
            e = model[fam].get(k)
            if e is None:
                notes.append(("status.json", None, f"no {fam} {k}; ignored"))
            elif st not in STATUS:
                refuse(["status.json"], None, f"{fam} {k}: status {st!r} is none of {', '.join(STATUS)}")
            elif fixed(fam, e):
                if st != "approved":
                    notes.append(("status.json", None, f"{fam} {k} is fixed (always approved); ignored"))
            else:
                e["status"] = st


def drawn_left(model):
    """-> refusals: a table the roll draws left with no approved entry (or a mood with no approved progression)"""
    def none(es):
        return es and not any(e["status"] == "approved" for e in es)
    out = []
    for fam, kind, what in (("drums", None, "drum pattern"), ("strum", "pattern", "strum pattern")):
        if none([e for e in model[fam].values() if kind is None or e.get("kind") == kind]):
            out.append(f"no approved {what} would be left: keep at least one approved")
    for mood in validate.MOODS:
        if none([e for e in model["progressions"].values() if mood in e["moods"]]):
            out.append(f"no approved {mood[5:].lower()} progression would be left: keep at least one approved")
    return out


# -------------------------------------------------------------------------------------------------------- import
def atomic_write(path, data):
    path = Path(path)
    fd, tmp = tempfile.mkstemp(dir=path.parent, prefix="." + path.name + ".")
    with os.fdopen(fd, "wb") as f:
        f.write(data if isinstance(data, bytes) else data.encode())
    os.replace(tmp, path)


def run(pat, gen, options, check=False):
    """-> exit code; prints what it did or why it refused"""
    model = json.loads((pat / "model.json").read_text())
    manifest = json.loads((pat / ".manifest.json").read_text()) if (pat / ".manifest.json").exists() else {}
    stems = {}
    for fam, why in REMOVED.items():
        d = pat / fam
        gone = sorted(p.name for p in d.iterdir() if p.suffix in EXTS) if d.is_dir() else []
        if gone:
            print(f"import: refused: patterns/{fam}/{gone[0]}: {fam} is a removed family ({why}): delete "
                  f"patterns/{fam}/")
            print("import: nothing written")
            return 1
    for fam in FAMS:
        d = pat / fam
        if not d.is_dir():
            continue
        for p in sorted(d.iterdir()):
            if p.suffix not in EXTS:
                continue
            if p.is_symlink() or not p.is_file():
                print(f"import: refused: {p.relative_to(pat)}: not a plain file")
                return 1
            stems.setdefault(f"{fam}/{p.stem}", {})[p.suffix] = p
    by_stem = {e["file"]: (fam, k) for fam in FAMS for k, e in model[fam].items()}
    new = copy.deepcopy(model)
    errors, imported, notes = [], {}, []
    NOTICES.clear()
    present = {st + x for st, fs in stems.items() for x in fs}
    for fam in FAMS:
        for k, e in model[fam].items():
            gone = [x for x in EXTS if e["file"] + x not in present]
            if gone:
                what = "its files are" if len(gone) == 2 else f"its {gone[0]} is"
                notes.append((e["file"], None, f"{what} missing; the entry stays (export.sh writes them again)"))

    def refuse(rels, bar, rule):
        errors.append((rels, bar, rule))
    for stem, files in stems.items():
        fam = stem.split("/")[0]
        changed = {x: p for x, p in files.items() if manifest.get(stem + x) != sha(p)}
        if not changed:
            continue
        decoded = {}
        for x, p in changed.items():
            rel = f"{stem}{x}"
            try:
                score = score_of(p, fam)
                if stem in by_stem:
                    key = by_stem[stem][1]
                    base = with_meta(fam, model[fam][key], score["meta"])
                    base["status"] = model[fam][key]["status"]
                else:
                    key, base = defaults(fam, stem, new, score["meta"])
                    base = with_meta(fam, base, score["meta"])
                    base["status"] = "candidate"
                decoded[x] = (key, decode(fam, key, score, base), base["status"])
            except Refused as r:
                refuse([rel], r.bar, r.rule)
            except (ValueError, KeyError, IndexError, TypeError, ArithmeticError) as err:
                refuse([rel], None, f"unreadable ({err})")
            finally:
                notes += [(rel, bar, msg) for bar, msg in NOTICES]
                NOTICES.clear()
        if len(decoded) != len(changed):
            continue
        (key, e, status), *rest = decoded.values()
        if rest and content(rest[0][1]) != content(e):
            diff = sorted(k for k in set(content(e)) | set(content(rest[0][1]))
                          if content(e).get(k) != content(rest[0][1]).get(k))
            refuse([stem + ".mid", stem + ".musicxml"], None,
                   f"both files changed and they disagree ({', '.join(diff)}): keep the one you mean, revert the other")
            continue
        if stem not in by_stem:
            sound = {x: e.get(x) for x in SOUND_KEYS[fam]}
            same = [(k, o) for k, o in model[fam].items() if {x: o.get(x) for x in SOUND_KEYS[fam]} == sound]
            for k, o in same:
                notes.append((stem + next(iter(changed)), None, f"the same as {fam} {k} ({o['file']}): a renamed or "
                              "saved-as copy? It imports as a new candidate"))
        e["status"], e["file"] = status, stem
        new[fam][key] = e
        imported[stem] = (fam, key, set(changed))
    set_status(pat, new, notes, refuse)
    for rule in drawn_left(new):
        refuse(["status.json"], None, rule)
    def files_of(e):                                 # a refusal names the files the entry was read from
        got = imported.get(e["file"])
        return [e["file"] + x for x in sorted(got[2])] if got else [e["file"] + ".mid"]
    for fam in FAMS:
        for k, e in new[fam].items():
            for bar, rule in validate.check(fam, e):
                refuse(files_of(e), bar, rule)
        for k, bar, rule in validate.check_family(fam, new[fam]):
            refuse(files_of(new[fam][k]) if k else [fam + "/"], bar, rule)
    for rel, bar, msg in notes:
        print(f"import: note: patterns/{rel}: " + (f"bar {bar}: " if bar else "") + msg)
    if errors:
        for rels, bar, rule in errors:
            print(f"import: refused: {' and '.join('patterns/' + r for r in rels)}: " +
                  (f"bar {bar}: " if bar else "") + rule)
        print("import: nothing written")
        return 1
    tables, kicks = rows_from_model(new)
    c_text, h_text = gen_c(tables, kicks)
    o_text = options_text(options.read_text(), option_blocks(new, options.read_text()))
    out_model = export.build_model(tables, kicks)
    for fam in FAMS:
        for k, e in out_model[fam].items():
            e["file"] = new[fam][k]["file"]
            if content(e) != content(new[fam][k]):
                raise SystemExit(f"import: internal: {fam} {k} does not round-trip through the tables")
    changes, moves = [], []                          # moves: why the pins move ("approved drums 9")
    for fam in FAMS:
        for k, e in out_model[fam].items():
            old = model[fam].get(k)
            if old is None:                              # a candidate: never drawn, never moves the pins
                changes.append(f"import: new {fam} {k} ({e['file']}), " +
                               ("a candidate" if e["status"] == "candidate" else e["status"]))
                if e["status"] == "approved":
                    moves.append(f"new approved {fam} {k}")
                continue
            if content(old) != content(e):
                changes.append(f"import: {old['status']} {fam} {k} changed ({e['file']})")
                if old["status"] == "approved":
                    moves.append(f"changed approved {fam} {k}")
            if old["status"] != e["status"]:            # status.json: approving or retiring moves the rolls
                changes.append(f"import: {fam} {k} ({e['file']}) {old['status']} -> {e['status']}")
                if "approved" in (old["status"], e["status"]):
                    moves.append(f"{e['status']} {fam} {k}" if e["status"] == "approved" else
                                 f"{e['status']} (was approved) {fam} {k}")
    for line in changes:
        print(line)
    if moves:
        print(f"import: {HASHES} will move: tools/patterns/import.sh runs the suite and says so; "
              "import.sh --repin re-pins them")
        print("import: pins move: " + "; ".join(moves))  # (import.sh reads this line)
    if check:
        print(f"import: check: ok ({len(imported)} pattern(s) to import, {len(changes)} change(s)); nothing written")
        return 0
    for path, text in ((gen / "patterns_gen.c", c_text), (gen / "patterns_gen.h", h_text),
                       (options, o_text)):
        if not path.exists() or path.read_text() != text:
            atomic_write(path, text)
    for stem, (fam, k, changed) in imported.items():
        e = out_model[fam][k]
        for x in EXTS:
            if x not in changed:                         # the other file, rewritten from the imported entry
                export.write_file(pat, fam, k, e, x)
                print(f"import: wrote patterns/{stem}{x} from patterns/{stem}{next(iter(changed))}")
            manifest[stem + x] = sha(pat / (stem + x))
    for path, text in ((pat / "model.json", json.dumps(out_model, indent=2) + "\n"),
                       (pat / ".manifest.json", json.dumps(dict(sorted(manifest.items())), indent=2) + "\n")):
        if not path.exists() or path.read_text() != text:
            atomic_write(path, text)
    print(f"import: {len(imported)} pattern(s) imported; tables in {gen}")
    return 0


def main(argv):
    args, check = argv[1:], False
    opts = {"--patterns": ROOT / "patterns", "--gen": ROOT / "app/song", "--options": ROOT / "workbench/options.json"}
    while args:
        a = args.pop(0)
        if a == "--check":
            check = True
        elif a in opts and args:
            opts[a] = Path(args.pop(0)).resolve()
        else:
            raise SystemExit(__doc__)
    return run(opts["--patterns"], opts["--gen"], opts["--options"], check)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
