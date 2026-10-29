# SPDX-License-Identifier: GPL-3.0-or-later
"""The engine's rules for a pattern entry (model.json's shape), checked before import regenerates any table.
   check(fam, e) -> [(bar or None, rule)]: one entry. check_family(fam, entries) -> [(key, bar, rule)]: the rules
   across a family (landing weights, unique ids). Bars are 1-based. A rule names what the engine needs, so the
   message tells Sam what to change."""
import re

PF = ["PF_BOOMBAP", "PF_HALFTIME", "PF_SPARSE", "PF_BRUSHES", "PF_BOSSA"]
PK = ["PK_VAMP", "PK_LOOP", "PK_JAZZY"]
Q = ["Q_DIATONIC", "Q_MIN7", "Q_DOM7", "Q_MAJ7"]
MOODS = ["MOOD_SAD", "MOOD_WISTFUL", "MOOD_WARM", "MOOD_SUNNY"]
MODES = ["MODE_IONIAN", "MODE_DORIAN", "MODE_PHRYGIAN", "MODE_LYDIAN", "MODE_MIXOLYDIAN", "MODE_AEOLIAN"]
SCALE = [[0, 2, 4, 5, 7, 9, 11], [0, 2, 3, 5, 7, 9, 10], [0, 1, 3, 5, 7, 8, 10], [0, 2, 4, 6, 7, 9, 11],
         [0, 2, 4, 5, 7, 9, 10], [0, 2, 3, 5, 7, 8, 10]]
UP_STEPS = {2, 6, 10, 14}                       # the "and"s, where an up stroke may go
STROKES = "TBDdXUuP"
SRC_RE = re.compile(r"[ -~]{0,200}")            # printable ASCII: it becomes a C string literal (and
#                                                 no backslash or "??", whatever c_str escapes)


def _stack(mode, d, k):
    """the interval from degree d's root to the note k scale steps above it (theory.c's stack)"""
    s = SCALE[mode]
    return (s[(d + k) % 7] + 12 * ((d + k) // 7) - s[d]) % 12


def chord_pcs(mode, spec, color):
    """theory.c's th_chord_pcs in C (key 0): the chord's pitch classes, root first, avoid notes left out.
       color 0: 7ths, 1: 9ths, 2: rich (9/11/13 where the mode allows)"""
    d = max(0, min(6, spec["degree"]))
    q = Q.index(spec["quality"])
    root = (SCALE[mode][d] + spec["alter"]) % 12
    i3, i5, i7, i9 = (_stack(mode, d, k) for k in (2, 4, 6, 8))
    ext = -1
    if q != 0:
        i5, i9 = 7, 2
        i3 = 3 if q == 1 else 4
        i7 = 11 if q == 3 else 10
        ext = 9 if q == 2 else 5 if q == 1 else 6 if mode == 3 else -1
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
        for i, c in enumerate(s):
            if c != "." and c not in STROKES:
                bad(i // 16 + 1, f"stroke {c!r} is not one of {STROKES}")
            elif c in "Uu" and i % 16 not in UP_STEPS:
                bad(i // 16 + 1, f"an up stroke ({c}) only on an \"and\" (steps 2, 6, 10, 14), not step {i % 16}")
        if not s.strip("."):
            bad(1, "a strum pattern has at least one stroke")
    elif fam == "lead-arp":
        _check_arp(e, bad)
    elif fam == "melody-cells":
        on = e.get("onsets", [])
        if not on:
            bad(1, "a melody cell has at least one onset")
        if any(not _int(s, 0, 3) for s in on):
            bad(1, "a melody cell is one beat: onsets on 16ths 1-4 of beat 1 only")
        if not _int(e.get("weight"), 1, 255):
            bad(None, "a melody cell's weight is 1-255")
    return out


def _check_prog(e, bad):
    if not _int(e.get("id"), 1, 254):
        bad(None, "a progression's id is 1-254")
    if e.get("kind") not in PK:
        bad(None, f"kind is one of {', '.join(PK)}")
    moods, modes = e.get("moods") or [], e.get("modes") or []
    if not moods or any(m not in MOODS for m in moods):
        bad(None, "at least one mood, each one of " + ", ".join(MOODS))
    if not modes or any(m not in MODES for m in modes):
        bad(None, "at least one mode, each one of " + ", ".join(MODES))
        return
    if "MODE_PHRYGIAN" in modes:
        bad(None, "no Phrygian: the engine never plays it")
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
        for i, c in enumerate(chords):
            bar = i * bpc + 1
            pcs = roots[i]
            name = mode_name[5:].capitalize()
            if not jazzy and any(p not in SCALE[mode] for p in pcs):
                bad(bar, f"a chord outside {name} (a borrowed chord or substitution): only a jazzy progression "
                         "borrows")
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
    if e.get("name") in ("held", "half"):
        st = e.get("strikes") or []
        if e["name"] == "held" and [s.get("step") for s in st] != [0]:
            bad(1, "held comping is one strike, on step 0 (the engine holds it through the chord)")
        if e["name"] == "half":
            if not 1 <= len(st) <= 2 or st[0].get("step") != 0:
                bad(1, "half comping is one or two strikes in the bar, the first on step 0")
            if not _int(e.get("second_vel_pct"), 50, 100):
                bad(None, "second_vel_pct is 50-100")
        end = 0
        for s in st:
            if not (_int(s.get("step"), 0, 15) and _int(s.get("len"), 1, 16)) or s["step"] < end or \
               s["step"] + s["len"] > 16:
                bad(1, "strikes are in the bar, in order, never overlapping")
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


def _check_arp(e, bad):
    kind = e.get("kind")
    if kind == "rhythm":
        notes = e.get("notes") or []
        if not 1 <= len(notes) <= 8:
            bad(1, "an arpeggio rhythm has 1-8 notes")
        end = 0
        for at, ln in notes:
            if not (_int(at, 0, 15) and _int(ln, 1, 16)) or at < end:
                bad(1, "an arpeggio rhythm's notes are in the bar, in order, never overlapping")
            end = at + ln
    elif kind == "pattern":
        tones = e.get("tones") or []
        if not 2 <= len(tones) <= 6:
            bad(1, "an arpeggio pattern has 2-6 notes")
        for t in tones:
            if _int(t, 4, 99):
                bad(1, f"a chord-tone index {t + 1} > 4: the reference voicing has 4 tones (C4 E4 G4 B4)")
            elif not _int(t, 0, 3):
                bad(1, "an arpeggio pattern's notes are chord tones 1-4 of the reference Cmaj7")
    elif kind == "landing":
        if not (_int(e.get("at"), 0, 15) and _int(e.get("len"), 1, 16)):
            bad(1, "a landing is one note in the bar")
        if not _int(e.get("weight"), 0, 100):
            bad(None, "a landing's weight is 0-100")
    else:
        bad(None, "a lead-arp entry is a rhythm, a pattern or a landing")


def check_family(fam, entries):
    """[(key, bar, rule)] across the family"""
    out = []
    if fam == "lead-arp":
        ws = [e.get("weight", 0) for e in entries.values() if e.get("kind") == "landing"]
        if len(ws) != 6:
            out.append((None, None, "the landings are exactly six (the engine's ways a chord change lands)"))
        elif sum(w for w in ws if isinstance(w, int)) != 100:
            k = next(k for k, e in entries.items() if e.get("kind") == "landing")
            out.append((k, None, f"the landings' weights add up to 100 (they add up to {sum(ws)})"))
    if fam == "comp-rhythms" and sorted(e.get("name") for e in entries.values()) != ["half", "held", "stabs"]:
        out.append((None, None, "comp-rhythms are held, half and stabs, one each"))
    if fam == "strum" and sum(1 for e in entries.values() if e.get("kind") == "fill") != 1:
        out.append((None, None, "strum has one fill (the ballad's)"))
    return out
