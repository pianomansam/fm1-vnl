# SPDX-License-Identifier: GPL-3.0-or-later
"""Standard MIDI files, the patterns' way (app/host/score.c's conventions): format 1, PPQ 480, a conductor track
   (title, the vnl1 metadata as a text event, 4/4, the tempo, chord symbols as text events), then one track per part.

   write(path, tracks, ppq=480, title="", meta=None, tempo_bpm=85, texts=())
     tracks: [{"name": str, "channel": 0-15, "program": int or None, "notes": [(tick, len_ticks, note, vel)]}]
     texts:  [(tick, str)] on the conductor track (chord symbols)
   read(path) -> {"ppq", "tempo_bpm", "time_sig", "title", "meta", "texts", "notes": [{tick, len, note, vel,
     channel, track}]}"""
import struct

META_PREFIX = "vnl1 "


def _vlq(n):
    out = [n & 0x7F]
    n >>= 7
    while n:
        out.append(0x80 | (n & 0x7F))
        n >>= 7
    return bytes(reversed(out))


def _meta(kind, data):
    return b"\xff" + bytes([kind]) + _vlq(len(data)) + data


def _track(events):
    """events: [(tick, order, bytes)]; sorted by tick then order (note-offs before note-ons at one tick)"""
    body, now = b"", 0
    for tick, _, data in sorted(events, key=lambda e: (e[0], e[1])):
        body += _vlq(tick - now) + data
        now = tick
    body += b"\x00\xff\x2f\x00"
    return b"MTrk" + struct.pack(">I", len(body)) + body


def write(path, tracks, ppq=480, title="", meta=None, tempo_bpm=85, texts=()):
    us = round(60_000_000 / tempo_bpm)
    cond = [(0, 0, _meta(0x03, title.encode())), (0, 1, _meta(0x58, bytes([4, 2, 24, 8]))),
            (0, 2, _meta(0x51, us.to_bytes(3, "big")))]
    if meta is not None:
        cond.append((0, 3, _meta(0x01, (META_PREFIX + meta).encode())))
    cond += [(t, 4, _meta(0x01, s.encode())) for t, s in texts]
    chunks = [_track(cond)]
    for tr in tracks:
        ch = tr["channel"]
        ev = [(0, 0, _meta(0x03, tr["name"].encode()))]
        if tr.get("program") is not None:
            ev.append((0, 1, bytes([0xC0 | ch, tr["program"]])))
        for tick, length, note, vel in tr["notes"]:
            ev.append((tick, 3, bytes([0x90 | ch, note, vel])))
            ev.append((tick + length, 2, bytes([0x80 | ch, note, 0])))
        chunks.append(_track(ev))
    head = b"MThd" + struct.pack(">IHHH", 6, 1, len(chunks), ppq)
    with open(path, "wb") as f:
        f.write(head + b"".join(chunks))


MAX_BYTES = 1 << 20                     # a pattern file is a few hundred bytes; anything past 1 MB is refused
MAX_NOTES = 10000


def _read_vlq(b, i, end):
    n = 0
    for _ in range(4):
        if i >= end:
            raise ValueError("a length runs past its track")
        c = b[i]
        i += 1
        n = (n << 7) | (c & 0x7F)
        if not c & 0x80:
            return n, i
    raise ValueError("a variable-length number longer than 4 bytes")


def read(path):
    """a standard MIDI file (untrusted: every length is bounds-checked; a malformed file raises ValueError)"""
    with open(path, "rb") as f:
        b = f.read(MAX_BYTES + 1)
    if len(b) > MAX_BYTES:
        raise ValueError(f"{path}: larger than {MAX_BYTES} bytes")
    if len(b) < 14 or b[:4] != b"MThd":
        raise ValueError(f"{path}: not a MIDI file")
    hlen, _, ntracks, ppq = struct.unpack(">IHHH", b[4:14])
    if hlen < 6 or 8 + hlen > len(b):
        raise ValueError(f"{path}: bad header length")
    if ppq & 0x8000 or ppq == 0:
        raise ValueError(f"{path}: SMPTE or zero time division is not supported")
    out = {"ppq": ppq, "tempo_bpm": None, "time_sig": None, "title": "", "meta": None, "texts": [], "notes": []}
    i = 8 + hlen
    for t in range(ntracks):
        if i + 8 > len(b) or b[i:i + 4] != b"MTrk":
            raise ValueError(f"{path}: bad track chunk {t}")
        end = i + 8 + struct.unpack(">I", b[i + 4:i + 8])[0]
        if end > len(b):
            raise ValueError(f"{path}: track {t} runs past the end of the file")
        i += 8
        tick, status, on = 0, 0, {}
        try:
            while i < end:
                d, i = _read_vlq(b, i, end)
                tick += d
                if i >= end:
                    raise ValueError("an event runs past its track")
                if b[i] == 0xFF:
                    if i + 2 > end:
                        raise ValueError("a meta event runs past its track")
                    kind = b[i + 1]
                    n, i = _read_vlq(b, i + 2, end)
                    if i + n > end:
                        raise ValueError("a meta event's data runs past its track")
                    data = b[i:i + n]
                    i += n
                    if kind == 0x51 and n == 3 and int.from_bytes(data, "big"):
                        out["tempo_bpm"] = round(60_000_000 / int.from_bytes(data, "big"), 3)
                        if out["tempo_bpm"] == int(out["tempo_bpm"]):
                            out["tempo_bpm"] = int(out["tempo_bpm"])
                    elif kind == 0x58 and n >= 2:
                        out["time_sig"] = (data[0], 2 ** min(data[1], 6))
                    elif kind == 0x03 and t == 0:
                        out["title"] = data.decode("utf-8", "replace")
                    elif kind == 0x01:
                        s = data.decode("utf-8", "replace")
                        if s.startswith(META_PREFIX):
                            out["meta"] = s[len(META_PREFIX):]
                        else:
                            out["texts"].append(s)
                    continue
                if b[i] in (0xF0, 0xF7):
                    n, i = _read_vlq(b, i + 1, end)
                    if i + n > end:
                        raise ValueError("a sysex event runs past its track")
                    i += n
                    continue
                if b[i] & 0x80:
                    status = b[i]
                    i += 1
                if not status & 0x80 or status >= 0xF0:
                    raise ValueError("a data byte with no running status")
                kind, ch = status & 0xF0, status & 0x0F
                if kind in (0xC0, 0xD0):
                    i += 1
                    continue
                if i + 2 > end:
                    raise ValueError("a channel event runs past its track")
                a, v = b[i], b[i + 1]
                i += 2
                if kind == 0x90 and v > 0:
                    if len(out["notes"]) >= MAX_NOTES:
                        raise ValueError(f"more than {MAX_NOTES} notes")
                    note = {"tick": tick, "len": 0, "note": a, "vel": v, "channel": ch, "track": t}
                    on.setdefault((ch, a), []).append(note)
                    out["notes"].append(note)
                elif kind == 0x80 or (kind == 0x90 and v == 0):
                    if on.get((ch, a)):
                        n = on[(ch, a)].pop(0)
                        n["len"] = tick - n["tick"]
        except ValueError as e:
            raise ValueError(f"{path}: track {t}: {e}") from None
        i = end
    out["notes"].sort(key=lambda n: (n["tick"], n["note"]))
    return out
