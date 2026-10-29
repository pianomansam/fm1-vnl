# SPDX-License-Identifier: GPL-3.0-or-later
"""MusicXML 4.0 (score-partwise) for MuseScore 4: divisions 4 (one division a 16th), 4/4, C major, one voice a part.

   write(path, parts, chords=None, title="", meta=None, tempo_bpm=85, bars=1)
     parts: [{"name": str, "notes": [{"start": step, "len": steps, "hits": [(pitch, vel, ghost)]}], then either
              "lanes": [{"key", "name", "midi", "step", "octave", "notehead"}] (a percussion staff: a hit's pitch
              is a lane key, written unpitched) or "program": GM program 0-127 (a treble staff: pitches MIDI)}]
     chords: [{"start": step, "root_step": "A", "root_alter": -1, "kind": "major-seventh", "text": "maj7"}]
   Pitched notes keep their lengths (split into written values, tied across beats and bar lines); percussion notes
   are onsets, drawn up to a beat long. Velocity rides on the note's dynamics attribute (percent of 90), and a ghost
   note's head is in parentheses."""
from xml.sax.saxutils import escape, quoteattr

DIV = 4                                  # divisions a quarter: a 16th is 1
BAR = 16
TYPES = {1: ("16th", 0), 2: ("eighth", 0), 3: ("eighth", 1), 4: ("quarter", 0), 6: ("quarter", 1), 8: ("half", 0),
         12: ("half", 1), 16: ("whole", 0)}
SPELL = [("C", 0), ("D", -1), ("D", 0), ("E", -1), ("E", 0), ("F", 0), ("F", 1), ("G", 0), ("A", -1), ("A", 0),
         ("B", -1), ("B", 0)]          # flats (Bb, not A#); F# for the one tritone
ACC = {-1: "flat", 1: "sharp"}


def _note_pieces(remaining):
    """written values for a held note: largest first"""
    out = []
    while remaining:
        d = next(d for d in (16, 12, 8, 6, 4, 3, 2, 1) if d <= remaining)
        out.append(d)
        remaining -= d
    return out


def _rest_pieces(start, length):
    """rests that start on their own multiple (an 8th rest on an 8th, ...), so beats read clearly"""
    out = []
    while length:
        d = next(d for d in (16, 8, 4, 2, 1) if d <= length and start % d == 0)
        out.append((start, d))
        start += d
        length -= d
    return out


def _segments(part, bars):
    """the part's voice as (start, dur, note or None, tie_stop, tie_start), bar by bar, gaps as rests"""
    perc = "lanes" in part
    notes = sorted(part["notes"], key=lambda n: n["start"])
    total = bars * BAR
    segs, t = [], 0
    for i, n in enumerate(notes):
        s = n["start"]
        if s < t:
            raise ValueError(f"{part['name']}: notes overlap at step {s}")
        if s + n["len"] > total:
            raise ValueError(f"{part['name']}: a note runs past the last bar at step {s}")
        for rs, d in _rest_pieces(t, s - t):
            segs.append((rs, d, None, False, False))
        if perc:                                         # an onset: drawn up to a beat long, never past the next
            nxt = notes[i + 1]["start"] if i + 1 < len(notes) else total
            gap = min(nxt, (s // BAR + 1) * BAR) - s
            d = next(d for d in (4, 2, 1) if d <= gap and s % d == 0)
            segs.append((s, d, n, False, False))
            t = s + d
            continue
        pieces, pos, left = [], s, n["len"]
        while left:                                      # bar lines first, then written values
            room = min(left, (pos // BAR + 1) * BAR - pos)
            for d in _note_pieces(room):
                pieces.append((pos, d))
                pos += d
            left -= room
        for k, (ps, d) in enumerate(pieces):
            segs.append((ps, d, n, k > 0, k + 1 < len(pieces)))
        t = pos
    for rs, d in _rest_pieces(t, total - t):
        segs.append((rs, d, None, False, False))
    return segs


def _pitched(midi):
    step, alter = SPELL[midi % 12]
    return step, alter, midi // 12 - 1


def _note_xml(part, pid, seg, hit, chord):
    d, stop, begin = seg[1], seg[3], seg[4]
    pitch, vel, ghost = hit
    typ, dot = TYPES[d]
    attrs = f' dynamics="{vel * 100 / 90:.2f}"' if not stop else ""
    x = [f"<note{attrs}>"]
    if chord:
        x.append("<chord/>")
    if "lanes" in part:
        lane = next(ln for ln in part["lanes"] if ln["key"] == pitch)
        x.append(f"<unpitched><display-step>{lane['step']}</display-step>"
                 f"<display-octave>{lane['octave']}</display-octave></unpitched>")
    else:
        step, alter, octave = _pitched(pitch)
        x.append(f"<pitch><step>{step}</step>" + (f"<alter>{alter}</alter>" if alter else "") +
                 f"<octave>{octave}</octave></pitch>")
    x.append(f"<duration>{d}</duration>")
    if stop:
        x.append('<tie type="stop"/>')
    if begin:
        x.append('<tie type="start"/>')
    if "lanes" in part:
        x.append(f'<instrument id="{pid}-I{lane["midi"] + 1}"/>')
    x.append("<voice>1</voice>")
    x.append(f"<type>{typ}</type>" + ("<dot/>" if dot else ""))
    if "lanes" not in part and alter and not stop:
        x.append(f"<accidental>{ACC[alter]}</accidental>")
    head = lane["notehead"] if "lanes" in part else None
    if ghost or head:
        x.append("<notehead" + (' parentheses="yes"' if ghost else "") + f">{head or 'normal'}</notehead>")
    if stop or begin:
        x.append("<notations>" + ('<tied type="stop"/>' if stop else "") + ('<tied type="start"/>' if begin else "") +
                 "</notations>")
    x.append("</note>")
    return "".join(x)


def _harmony_xml(c):
    alter = f"<root-alter>{c['root_alter']}</root-alter>" if c["root_alter"] else ""
    return (f"<harmony><root><root-step>{c['root_step']}</root-step>{alter}</root>"
            f"<kind text={quoteattr(c['text'])}>{c['kind']}</kind></harmony>")


def write(path, parts, chords=None, title="", meta=None, tempo_bpm=85, bars=1):
    L = ['<?xml version="1.0" encoding="UTF-8" standalone="no"?>',
         '<!DOCTYPE score-partwise PUBLIC "-//Recordare//DTD MusicXML 4.0 Partwise//EN" '
         '"http://www.musicxml.org/dtds/partwise.dtd">',
         '<score-partwise version="4.0">',
         f"  <work><work-title>{escape(title)}</work-title></work>",
         "  <identification>",
         "    <encoding><software>VNL-1 tools/patterns/export.py</software></encoding>"]
    if meta is not None:
        L.append(f'    <miscellaneous><miscellaneous-field name="vnl1">{escape(meta)}</miscellaneous-field>'
                 "</miscellaneous>")
    L += ["  </identification>", "  <part-list>"]
    for i, part in enumerate(parts):
        pid = f"P{i + 1}"
        L.append(f'    <score-part id="{pid}">')
        L.append(f"      <part-name>{escape(part['name'])}</part-name>")
        if "lanes" in part:
            for ln in part["lanes"]:
                L.append(f'      <score-instrument id="{pid}-I{ln["midi"] + 1}">'
                         f"<instrument-name>{escape(ln['name'])}</instrument-name></score-instrument>")
            for ln in part["lanes"]:
                L.append(f'      <midi-instrument id="{pid}-I{ln["midi"] + 1}"><midi-channel>10</midi-channel>'
                         f"<midi-unpitched>{ln['midi'] + 1}</midi-unpitched></midi-instrument>")
        else:
            L.append(f'      <score-instrument id="{pid}-I1"><instrument-name>{escape(part["name"])}'
                     "</instrument-name></score-instrument>")
            L.append(f'      <midi-instrument id="{pid}-I1"><midi-channel>{i + 1}</midi-channel>'
                     f"<midi-program>{part['program'] + 1}</midi-program></midi-instrument>")
        L.append("    </score-part>")
    L.append("  </part-list>")
    for i, part in enumerate(parts):
        pid = f"P{i + 1}"
        segs = _segments(part, bars)
        L.append(f'  <part id="{pid}">')
        for b in range(bars):
            L.append(f'    <measure number="{b + 1}">')
            if b == 0:
                clef = ("<sign>percussion</sign><line>2</line>" if "lanes" in part else "<sign>G</sign><line>2</line>")
                L.append(f"      <attributes><divisions>{DIV}</divisions><key><fifths>0</fifths></key>"
                         f"<time><beats>4</beats><beat-type>4</beat-type></time><clef>{clef}</clef></attributes>")
                if i == 0:
                    L.append('      <direction placement="above"><direction-type><metronome>'
                             f"<beat-unit>quarter</beat-unit><per-minute>{tempo_bpm}</per-minute></metronome>"
                             f'</direction-type><sound tempo="{tempo_bpm}"/></direction>')
            mine = [s for s in segs if b * BAR <= s[0] < (b + 1) * BAR]
            for seg in mine:
                if i == 0:
                    for c in chords or ():
                        if c["start"] == seg[0]:
                            L.append("      " + _harmony_xml(c))
                if seg[2] is None:
                    rest = '<rest measure="yes"/>' if seg[1] == BAR else "<rest/>"
                    typ = "" if seg[1] == BAR else f"<type>{TYPES[seg[1]][0]}</type>" + ("<dot/>" if TYPES[seg[1]][1]
                                                                                           else "")
                    L.append(f"      <note>{rest}<duration>{seg[1]}</duration><voice>1</voice>{typ}</note>")
                    continue
                for k, hit in enumerate(seg[2]["hits"]):
                    L.append("      " + _note_xml(part, pid, seg, hit, k > 0))
            L.append("    </measure>")
        L.append("  </part>")
    starts = {s[0] for s in _segments(parts[0], bars)} if parts else set()
    for c in chords or ():
        if c["start"] not in starts:
            raise ValueError(f"a chord symbol at step {c['start']} has no note or rest to sit on")
    L.append("</score-partwise>")
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(L) + "\n")


# ------------------------------------------------------------------------------------------------------- reading
MAX_BYTES = 4 << 20                      # MuseScore's verbose files run to tens of KB; past 4 MB is refused
NAMES = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}


class OffGrid(ValueError):
    """a note off the 16th grid: .bar is 1-based"""

    def __init__(self, bar, what):
        super().__init__(what)
        self.bar = bar


def _parse(path):
    """ElementTree over untrusted bytes: no entity declarations (no billion laughs, no external entities); the
       DOCTYPE's external DTD is never fetched (expat does not load it)"""
    import xml.etree.ElementTree as ET
    with open(path, "rb") as f:
        data = f.read(MAX_BYTES + 1)
    if len(data) > MAX_BYTES:
        raise ValueError(f"{path}: larger than {MAX_BYTES} bytes")
    if b"<!ENTITY" in data or b"<!ATTLIST" in data:
        raise ValueError(f"{path}: entity or attribute declarations are not allowed")
    try:
        return ET.fromstring(data)
    except ET.ParseError as e:
        raise ValueError(f"{path}: not well-formed XML ({e})") from None


def _text(el, path, default=None):
    x = el.find(path)
    return x.text.strip() if x is not None and x.text else default


def read(path, lanes=None):
    """-> {"title", "meta" (str or None), "bars", "harmony": [(step, root_step, root_alter, kind)],
          "notes": [{"start", "len", "pitch", "vel" (None: no dynamics), "ghost", "part"}]}
       Steps are 16ths from the first bar. Durations count in each part's own divisions (MuseScore rewrites them),
       ties join pieces into one note, chords, backup and forward move the voice time, grace and cue notes and rests
       are skipped. An unpitched note's pitch is its instrument's MIDI unpitched number (lanes: {(display step,
       octave): midi}, when it names none). A note onset off the 16th grid raises OffGrid."""
    from fractions import Fraction
    root = _parse(path)
    if root.tag != "score-partwise":
        raise ValueError(f"{path}: not a score-partwise MusicXML file")
    title = _text(root, "work/work-title") or _text(root, "movement-title") or ""
    meta = None
    for f in root.iter("miscellaneous-field"):
        if f.get("name") == "vnl1":
            meta = (f.text or "").strip()
    unpitched = {}
    for mi in root.iter("midi-instrument"):
        u = _text(mi, "midi-unpitched")
        if u is not None and u.isdigit():
            unpitched[mi.get("id")] = int(u) - 1
    notes, harmony, bars = [], [], 0
    for pn, part in enumerate(root.findall("part")):
        div, bar_q, measure_q = None, Fraction(0), Fraction(4)
        open_ties = {}
        for mn, m in enumerate(part.findall("measure")):
            pos, top = Fraction(0), Fraction(0)
            last_start = Fraction(0)
            for el in m:
                if el.tag == "attributes":
                    d = _text(el, "divisions")
                    if d is not None:
                        div = int(d)
                        if div <= 0:
                            raise ValueError(f"{path}: divisions {div}")
                    beats, bt = _text(el, "time/beats"), _text(el, "time/beat-type")
                    if beats is not None:
                        if (beats, bt) != ("4", "4"):
                            raise ValueError(f"{path}: bar {mn + 1}: not 4/4 ({beats}/{bt})")
                elif el.tag in ("backup", "forward"):
                    d = Fraction(int(_text(el, "duration", "0")), div or 1)
                    pos += -d if el.tag == "backup" else d
                    if pos < 0:
                        raise ValueError(f"{path}: bar {mn + 1}: a backup before the bar's start")
                    top = max(top, pos)
                elif el.tag == "harmony" and pn == 0:
                    off = Fraction(int(_text(el, "offset", "0")), div or 1)
                    q = bar_q + pos + off
                    harmony.append((int(q * 4), _text(el, "root/root-step"),
                                    int(float(_text(el, "root/root-alter", "0"))), _text(el, "kind")))
                elif el.tag == "note":
                    if el.find("grace") is not None:
                        continue
                    if div is None:
                        raise ValueError(f"{path}: a note before any divisions")
                    dur = Fraction(int(_text(el, "duration", "0")), div)
                    if el.find("chord") is not None:
                        start = last_start
                    else:
                        start = pos
                        pos += dur
                        top = max(top, pos)
                        last_start = start
                    if el.find("rest") is not None or el.find("cue") is not None:
                        continue
                    abs_q = bar_q + start
                    if el.find("unpitched") is not None:
                        inst = el.find("instrument")
                        pitch = unpitched.get(inst.get("id")) if inst is not None else None
                        if pitch is None and lanes:
                            pitch = lanes.get((_text(el, "unpitched/display-step"),
                                               int(_text(el, "unpitched/display-octave", "0"))))
                        if pitch is None:
                            raise ValueError(f"{path}: bar {mn + 1}: an unpitched note with no instrument")
                    elif el.find("pitch") is not None:
                        step = _text(el, "pitch/step")
                        if step not in NAMES:
                            raise ValueError(f"{path}: bar {mn + 1}: pitch step {step!r}")
                        pitch = (12 * (int(_text(el, "pitch/octave", "4")) + 1) + NAMES[step] +
                                 round(float(_text(el, "pitch/alter", "0"))))
                    else:
                        continue
                    ties = {t.get("type") for t in el.findall("tie")} | {t.get("type") for t in el.iter("tied")}
                    key = (pn, pitch)
                    if "stop" in ties and key in open_ties and open_ties[key]["end_q"] == abs_q:
                        n = open_ties.pop(key)
                        n["end_q"] = abs_q + dur
                        if "start" in ties:
                            open_ties[key] = n
                        continue
                    if (abs_q * 4).denominator != 1:
                        raise OffGrid(int(abs_q // 4) + 1, f"a note off the 16th grid (beat {float(start + 1):g})")
                    dyn = el.get("dynamics")
                    head = el.find("notehead")
                    n = {"start_q": abs_q, "end_q": abs_q + dur, "pitch": pitch,
                         "vel": round(float(dyn) * 90 / 100) if dyn else None,
                         "ghost": head is not None and head.get("parentheses") == "yes", "part": pn}
                    notes.append(n)
                    if "start" in ties:
                        open_ties[key] = n
            bar_q += max(measure_q, top)
            bars = max(bars, mn + 1)
    out = []
    for n in notes:
        length = max(1, round((n["end_q"] - n["start_q"]) * 4))
        out.append({"start": int(n["start_q"] * 4), "len": length, "pitch": n["pitch"], "vel": n["vel"],
                    "ghost": n["ghost"], "part": n["part"]})
    out.sort(key=lambda n: (n["start"], n["pitch"]))
    return {"title": title, "meta": meta, "bars": bars, "harmony": harmony, "notes": out}
