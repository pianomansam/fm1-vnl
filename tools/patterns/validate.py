# SPDX-License-Identifier: GPL-3.0-or-later
"""The engine's rules for a pattern entry (model.json's shape), checked before import regenerates any table.
   check(fam, e) -> [(bar or None, rule)]: one entry. check_family(fam, entries) -> [(key, bar, rule)]: the rules
   across a family (the comp rhythms' names, the strum's fills). Bars are 1-based. A rule names what the engine needs,
   so the message tells Sam what to change."""
import re
from pathlib import Path

PF = ["PF_BOOMBAP", "PF_HALFTIME", "PF_SPARSE", "PF_BRUSHES", "PF_BOSSA"]
PK = ["PK_VAMP", "PK_LOOP", "PK_JAZZY"]
Q = ["Q_DIATONIC", "Q_MIN7", "Q_DOM7", "Q_MAJ7", "Q_MIN7B9", "Q_MAJ7S11"]
MOODS = ["MOOD_SAD", "MOOD_WISTFUL", "MOOD_WARM", "MOOD_SUNNY"]
MODES = ["MODE_MAJOR", "MODE_MINOR"]           # a song is major or minor (Sam, 2026-11-05; six modes before)
SCALE = [[0, 2, 4, 5, 7, 9, 11], [0, 2, 3, 5, 7, 8, 10]]   # major, natural minor
# the scale degree a pitch class over the key spells, the simplest way: b2 b3 #4 b6 b7 in major, b2 3 #4 6 7 in minor
# (theory.c's DEG)
DEG = [0, 1, 1, 2, 2, 3, 3, 4, 5, 5, 6, 6]
# the major scales (their roots over the key) a vamp's or loop's chords may keep to: the key's modes, Ionian,
# Mixolydian (F major over C) and Lydian (G major) in a major key, Aeolian (Eb major) and Dorian (Bb major) in a minor
# one (R25, kept from the six modes)
KEY_MODES = [[0, 5, 7], [3, 10]]
UP_STEPS = {2, 6, 10, 14}                       # the "and"s, where an up stroke may go
STROKES = "TBDdXUuP"
# the strum fills as (figure, fill_of), sorted: the ballad's (no figure, pattern 4), step back and drive for every
# pattern (0) and for the bossa (3)
STRUM_FILLS = [("None", 4), ("drive", 0), ("drive", 3), ("step-back", 0), ("step-back", 3)]


def _ins_enum():
    """library.h's INS_* in enum order (a comp rhythm's keys are a mask of them)"""
    text = (Path(__file__).resolve().parent.parent.parent / "app/song/library.h").read_text()
    body = re.search(r"enum\s*\{\s*(INS_RHODES.*?)\}", text, re.S).group(1)
    return re.findall(r"\b(INS_[A-Z0-9_]+)\b", re.sub(r"/\*.*?\*/", "", body, flags=re.S))


INS = _ins_enum()
GUITARS = ("INS_NYLON", "INS_EMILY")            # they only strum (CS_STRUM, its own engine): never a comp rhythm's
KEYS_ALL = 0xFFFFFFFF & ~sum(1 << INS.index(g) for g in GUITARS)   # keys "all" (patterns_gen.h's CK_ALL)
# a comp rhythm's, drum pattern's or strum pattern's busyness:
BUSY = (0, 1, 2)                                 # sparse, middle, busy (roll.h's motion_tilt)
SRC_RE = re.compile(r"[ -~]{0,200}")            # printable ASCII: it becomes a C string literal (and
#                                                 no backslash or "??", whatever c_str escapes)


def _stack(mode, d, k):
    """the interval from degree d's root to the note k scale steps above it (theory.c's stack)"""
    s = SCALE[mode]
    return (s[(d + k) % 7] + 12 * ((d + k) // 7) - s[d]) % 12


def chord_pcs(mode, spec, color):
    """theory.c's th_chord_pcs in C (key 0): the chord's pitch classes, root first, avoid notes left out.
       color 0: 7ths, 1: 9ths, 2: rich (9/11/13 where the key or the quality allows)"""
    d = max(0, min(6, spec["degree"]))
    q = Q.index(spec["quality"])
    root = (SCALE[mode][d] + spec["alter"]) % 12
    i3, i5, i7, i9 = (_stack(mode, d, k) for k in (2, 4, 6, 8))
    ext = -1
    if q != 0:
        i5, i9 = 7, 1 if q == 4 else 2
        i3 = 3 if q in (1, 4) else 4
        i7 = 11 if q in (3, 5) else 10
        ext = {1: 5, 2: 9, 4: 5, 5: 6}.get(q, -1)
    elif i3 == 4 and i7 == 10:
        ext = _stack(mode, d, 12)
    elif i3 == 3 and i5 == 7 and i7 == 10:
        ext = _stack(mode, d, 10)
    elif i3 == 4 and i7 == 11 and _stack(mode, d, 10) == 6:
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


def _is_dominant(pcs):
    ivs = {(p - pcs[0]) % 12 for p in pcs[1:]}
    return 4 in ivs and 10 in ivs


def spec_of(mode, root):
    """{degree, alter}: a root (a pitch class over the key) spelled the simplest way (DEG)"""
    d = DEG[root % 12]
    return {"degree": d, "alter": (root - SCALE[mode][d] + 6) % 12 - 6}


def numeral(mode, spec):
    """the chord as a Roman numeral in its key: its root's degree (b or # off the key's scale), upper case over a
       major 3rd, then 7 (maj7 over a major 7th, 7b5 over a diminished 5th); a forced color the key's own chord
       there lacks: (#11) for Q_MAJ7S11, (no 9) for Q_MIN7B9 (its 9th a semitone over the root: left out)"""
    pcs = chord_pcs(mode, spec, 0)
    ivs = [(p - pcs[0]) % 12 for p in pcs]
    r = ["I", "II", "III", "IV", "V", "VI", "VII"][max(0, min(6, spec["degree"]))]
    acc = {-1: "b", 1: "#"}.get(spec["alter"], "")
    major = 4 in ivs
    suffix = ("maj7" if 11 in ivs else "7") if major else ("7b5" if 6 in ivs else "7")
    return acc + (r if major else r.lower()) + suffix + {"Q_MAJ7S11": "(#11)", "Q_MIN7B9": "(no 9)"}.get(
        spec["quality"], "")


def _dominant_has_color(mode, spec):
    """harmony.c's dominant_colors: a dominant sounds a 9th, or else a 13th; with neither it drops to a bare triad"""
    for color in (1, 2):
        pcs = chord_pcs(mode, spec, color)
        if any((p - pcs[0]) % 12 not in (4, 7, 10) for p in pcs[1:]):
            return True
    return False


def _mask(x):
    return int(x, 16) if isinstance(x, str) else x


def _steps(mask):
    return [i for i in range(16) if mask >> i & 1]


def _int(v, lo, hi):
    return isinstance(v, int) and not isinstance(v, bool) and lo <= v <= hi


def check(fam, e):
    out = []

    def bad(bar, rule):
        out.append((bar, rule))
    src = e.get("src", "")
    if not isinstance(src, str) or not SRC_RE.fullmatch(src) or "\\" in src or "??" in src:
        bad(None, "src: printable ASCII only, at most 200 characters, no backslash and no \"??\"")
    if fam == "drums":
        if not _int(e.get("id"), 1, 254):
            bad(None, "a drum pattern's id is 1-254")
        if e.get("family") not in PF:
            bad(None, f"family is one of {', '.join(PF)}")
        if e.get("half_time") not in (0, 1) or e.get("poly3") not in (0, 1) or e.get("two_bar") not in (0, 1):
            bad(None, "half_time, poly3 and two_bar are 0 or 1")
        if e.get("busy") not in BUSY:
            bad(None, "busy is 0 (sparse), 1 (middle) or 2 (busy)")
        for bar in range(2):
            b = bar == 1 and e.get("two_bar")
            hit = [_mask(x) for x in (e["hit_b"] if b else e["hit"])]
            ghost = [_mask(x) for x in (e["ghost_b"] if b else e["ghost"])]
            if not (hit[0] | ghost[0]):
                bad(bar + 1, "a drum pattern has a kick in every bar")
            if not (hit[1] | ghost[1]):
                bad(bar + 1, "a drum pattern has a snare in every bar")
    elif fam == "kick-phrases":
        for bar, k in enumerate(("a", "b")):
            steps = e.get(k)
            if not steps:
                bad(bar + 1, "a kick phrase has a kick in each of its two bars")
            elif not all(_int(s, 0, 15) for s in steps) or steps != sorted(set(steps)):
                bad(bar + 1, "kick steps are 0-15, ascending, once each")
    elif fam == "progressions":
        _check_prog(e, bad)
    elif fam == "comp-rhythms":
        _check_comp(e, bad)
    elif fam == "strum":
        s = e.get("steps", "")
        if len(s) not in (16, 32) or (e.get("kind") == "fill" and len(s) != 16):
            bad(None, "a strum pattern is 1 bar (2 for a two-bar pattern; a fill 1)")
        if e.get("kind") == "pattern" and e.get("busy") not in BUSY:
            bad(None, "busy is 0 (sparse), 1 (middle) or 2 (busy)")
        for i, c in enumerate(s):
            if c != "." and c not in STROKES:
                bad(i // 16 + 1, f"stroke {c!r} is not one of {STROKES}")
            elif c in "Uu" and i % 16 not in UP_STEPS:
                bad(i // 16 + 1, f"an up stroke ({c}) only on an \"and\" (steps 2, 6, 10, 14), not step {i % 16}")
        if not s.strip("."):
            bad(1, "a strum pattern has at least one stroke")
    return out


def _check_prog(e, bad):
    if not _int(e.get("id"), 1, 254):
        bad(None, "a progression's id is 1-254")
    if e.get("kind") not in PK:
        bad(None, f"kind is one of {', '.join(PK)}")
    moods, modes = e.get("moods") or [], e.get("modes") or []
    if not moods or any(m not in MOODS for m in moods):
        bad(None, "at least one mood, each one of " + ", ".join(MOODS))
    if len(modes) != 1 or modes[0] not in MODES:
        bad(None, "major or minor: one of " + ", ".join(MODES))
        return
    n, bpc, chords = e.get("n"), e.get("bars_per_chord"), e.get("chords") or []
    if not _int(n, 1, 4) or len(chords) != n:
        bad(None, "a progression has 1-4 chords")
        return
    if not _int(bpc, 1, 8) or n * bpc not in (2, 4, 8):
        bad(None, "a progression's cycle (chords x bars per chord) is 2, 4 or 8 bars, every chord the same length")
        return
    for c in chords:
        if not (_int(c.get("degree"), 0, 6) and _int(c.get("alter"), -2, 2) and c.get("quality") in Q):
            bad(None, "a chord is {degree 0-6, alter -2..2, quality Q_*}")
            return
    jazzy = e["kind"] == "PK_JAZZY"
    for mode_name in modes:
        mode = MODES.index(mode_name)
        roots = [chord_pcs(mode, c, 0) for c in chords]
        name = mode_name[5:].lower()
        notes = set()
        for i, c in enumerate(chords):                  # the first chord that leaves every mode of the key
            notes |= {p for color in range(3) for p in chord_pcs(mode, c, color)}
            if not jazzy and not any(notes <= {(k + x) % 12 for x in SCALE[0]} for k in KEY_MODES[mode]):
                bad(i * bpc + 1, f"a chord outside every mode of the {name} key (a borrowed chord or substitution): "
                                 "a vamp's or loop's chords keep to one of " +
                                 ("Ionian, Mixolydian or Lydian" if mode == 0 else "Aeolian or Dorian") +
                                 "; only a jazzy progression borrows")
                break
        for i, c in enumerate(chords):
            bar = i * bpc + 1
            pcs = roots[i]
            if _is_dominant(pcs):
                nxt = roots[(i + 1) % n][0]
                if pcs[0] in (1, 8) or (pcs[0] - nxt) % 12 == 1:
                    bad(bar, f"a tritone substitution (a dominant on bII or bVI, or a half step above the next "
                             f"chord, in {name}): no substitutions")
                if not _dominant_has_color(mode, c):
                    bad(bar, f"a dominant below the 9th in {name}: it would sound bare (no 9th or 13th fits)")


def _check_comp(e, bad):
    lo_hi = e.get("vel_range") or []
    if len(lo_hi) != 2 or not (_int(lo_hi[0], 25, 110) and _int(lo_hi[1], 25, 110) and lo_hi[0] <= lo_hi[1]):
        bad(None, "vel_range is [lo, hi] within 25-110")
    if not _int(e.get("weight"), 0, 20):
        bad(None, "weight is 0-20 (the keys' draw: 1 each by default; 0 never drawn)")
    if e.get("busy") not in BUSY:
        bad(None, "busy is 0 (sparse), 1 (middle) or 2 (busy)")
    if e.get("thin") not in (0, 1) or e.get("hold") not in (0, 1):
        bad(None, "thin and hold are 0 or 1")
    keys = e.get("keys")
    if keys != "all" and (not isinstance(keys, list) or not keys or len(set(keys)) != len(keys) or
                          any(k not in INS or k in GUITARS or k == "INS_NONE" for k in keys)):
        bad(None, "keys is \"all\" (every keys instrument but the guitars, which strum) or a list of INS_* names "
                  "(library.h), never a guitar")
    if e.get("name") in ("held", "half", "held-on-2", "held-on-2-and-4") or e.get("kind") == "pattern":
        st = e.get("strikes") or []
        if e.get("kind") == "pattern" and not 1 <= len(st) <= 8:
            bad(1, "a comp rhythm is 1-8 strikes in one bar (the engine has no push over the bar line)")
        if e.get("name") == "held" and [s.get("step") for s in st] != [0]:
            bad(1, "held comping is one strike, on step 0 (the engine holds it through the chord)")
        if e.get("name") == "half":
            if not 1 <= len(st) <= 2 or st[0].get("step") != 0:
                bad(1, "half comping is one or two strikes in the bar, the first on step 0")
            if not _int(e.get("second_vel_pct"), 50, 100):
                bad(None, "second_vel_pct is 50-100")
        if e.get("name") == "held-on-2" and [s.get("step") for s in st] != [4]:
            bad(1, "held on 2 is one strike, on step 4 (beat 2)")
        if e.get("name") == "held-on-2-and-4" and [s.get("step") for s in st] != [4, 12]:
            bad(1, "held on 2 and 4 is two strikes, on steps 4 and 12 (beats 2 and 4)")
        if "second_vel_pct" in e and not _int(e["second_vel_pct"], 25, 100):
            bad(None, "second_vel_pct is 25-100")
        end = 0
        for s in st:
            if not (_int(s.get("step"), 0, 15) and _int(s.get("len"), 1, 16)) or s["step"] < end or \
               s["step"] + s["len"] > 16:
                bad(1, "strikes are in the bar, in order, never overlapping (none rings past the bar line)")
            end = s.get("step", 0) + s.get("len", 0)
    elif e.get("name") == "stabs":
        first, second = e.get("first"), e.get("second") or []
        if not _int(first, 0, 15):
            bad(1, "the first stab is on a step 0-15")
        if not 1 <= len(second) <= 4:
            bad(None, "the second stab has 1-4 candidates (one bar each)")
        for i, s in enumerate(second):
            if not _int(s, 0, 15) or s == first or second.index(s) != i:
                bad(i + 1, "each candidate is a step 0-15, once, never the first stab's")
        if not _int(e.get("len"), 1, 4):
            bad(1, "a stab is 1-4 16ths long")
        if not _int(e.get("busy_over"), 0, 16) or not e.get("busy_steps") or \
           not all(_int(s, 0, 15) for s in e["busy_steps"]):
            bad(None, "busy_over is 0-16 and busy_steps 1-16 steps 0-15")


def check_family(fam, entries):
    """[(key, bar, rule)] across the family"""
    out = []
    if fam == "comp-rhythms" and sorted(e.get("name") for e in entries.values() if e.get("kind") != "pattern") != \
            ["half", "held", "held-on-2", "held-on-2-and-4", "stabs"]:
        out.append((None, None, "comp-rhythms are held, half, stabs, held-on-2 and held-on-2-and-4, one each (light "
                                "eighths, 5, removed 2026-11-05)"))
    if fam == "strum" and sorted((str(e.get("figure")), e.get("fill_of")) for e in entries.values()
                                 if e.get("kind") == "fill") != STRUM_FILLS:
        out.append((None, None, "strum has five fills: the ballad's, and step back and drive for every pattern and "
                                "for the bossa"))
    return out
