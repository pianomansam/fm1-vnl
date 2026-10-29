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
