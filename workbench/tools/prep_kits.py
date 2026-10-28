#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""The workbench's sampled kits as their makers ship them: Karoryfer's Gogodze Phu Vol II (Programs/Kit.sfz) and Big
Rusty Drums (Programs/02-basic.sfz; its brushes: the same program with the brush maps and sticking switches), both CC0.
   python3 -I workbench/tools/prep_kits.py FETCH_DIR workbench/samples
fetches each source (shallow, sparse, at a pinned commit: only the programs and the samples the pieces use) into
FETCH_DIR, then writes one float32 mono 44.1 kHz file per piece, velocity layer and round robin (at most 4 layers x 2
round robins) into the output folder, and kits.json: the pieces, their layers and which lanes they serve.

A piece is a key of the program at its default controller values: its regions (every mic) mixed to mono at the top
velocity of each layer, as the drum spike's SFZ reader did (amplitude CCs as level, vel squared or amp_velcurve's
linear ramp; envelopes, tuning, mic delays and filters ignored: neutral at the defaults or inactive). The engine
scales a hit by (vel / the layer's top) ^ curve. Each file ends where the piece falls under -80 dB of its peak."""
import array
import json
import os
import re
import subprocess
import sys
import tempfile
import wave
from pathlib import Path

SR = 44100
MAX_VL, MAX_RR = 4, 2
TAIL = 1e-4                             # -80 dB of the piece's peak: the file ends there
FADE = 441                              # (10 ms faded to it)

REPOS = {
    "gogodze": ("https://github.com/sfzinstruments/karoryfer.gogodze-phu-vol-ii", "69a0274cdc39c99c3098cde1bc3789690de86d62"),
    "big-rusty": ("https://github.com/sfzinstruments/karoryfer.big-rusty-drums", "f07ce00df34a46b6b08375be56fe116cf15782bc"),
}
# pieces: id (wb_drums.h's WB_P_*), key, controller overrides; "same": another piece's files at vel (x its velocity).
# lanes: kick, snare, closed hat, open hat, shaker, rim, ride -> a piece (None: silent)
STICKS = [{"id": 0, "name": "kick", "key": 36}, {"id": 1, "name": "snare", "key": 38},
          {"id": 2, "name": "rim", "key": 37}, {"id": 3, "name": "hat", "key": 42},
          {"id": 4, "name": "ohat", "key": 46, "cc": {4: 127}}]
KITS = [
    {"id": 0, "name": "gogodze", "title": "Gogodze Phu Vol II", "kit": 19, "repo": "gogodze",
     "program": "Programs/Kit.sfz", "cc": {}, "swap": {}, "pieces": STICKS,
     "lanes": [0, 1, 3, 4, None, 2, 3]},          # no ride: its closed hat; no shaker
    {"id": 1, "name": "rusty", "title": "Big Rusty Drums (sticks)", "kit": 20, "repo": "big-rusty",
     "program": "Programs/02-basic.sfz", "cc": {}, "swap": {},
     "pieces": STICKS + [{"id": 5, "name": "ride", "key": 51}], "lanes": [0, 1, 3, 4, None, 2, 5]},
    {"id": 2, "name": "brushes", "title": "Big Rusty Drums (brushes)", "kit": 21, "repo": "big-rusty",
     "program": "Programs/02-basic.sfz", "cc": {25: 80, 27: 80, 28: 80},    # snare, hat, cymbal sticking: brushes
     "swap": {"mappings/snare_14_map_basic.sfz": "mappings/snare_14_map_brush.sfz",
              "mappings/snare_14_map_sidestick.sfz": None,
              "mappings/hihat_14_map_basic.sfz": "mappings/hihat_14_map_brush.sfz",
              "mappings/ride_22_map_basic.sfz": "mappings/ride_22_map_brush.sfz",
              "mappings/crash_17_map_basic.sfz": "mappings/crash_17_map_brush.sfz"},
     "pieces": [STICKS[0], STICKS[1], {"id": 2, "name": "rim", "same": 1, "vel": 0.45}, STICKS[3], STICKS[4],
                {"id": 5, "name": "ride", "key": 51}],   # no brush sidestick: the brush snare at 45%
     "lanes": [0, 1, 3, 4, None, 2, 5]},
]


# ---- SFZ (the drum spike's reader): regions as their effective opcodes, and the <control> defaults ----
def load(program, swap):
    root = os.path.dirname(os.path.abspath(program))
    defs = {}

    def read(p):
        out = []
        with open(p, errors="replace") as f:
            text = f.read().splitlines()
        for line in text:
            line = re.sub(r"//.*", "", line).rstrip()
            m = re.match(r"\s*#define\s+(\$\w+)\s*(.*)", line)
            if m:
                defs[m.group(1)] = m.group(2).strip()
                continue
            m = re.match(r'\s*#include\s+"([^"]+)"', line)
            if m:
                inc = swap.get(m.group(1), m.group(1))
                if inc:
                    out += read(os.path.normpath(os.path.join(root, inc)))
                continue
            for k in sorted(defs, key=len, reverse=True):
                line = line.replace(k, defs[k])
            out.append(line)
        return out

    toks = re.findall(r"<(\w+)>|(\w+)=((?:(?!\s+\w+=|\s*<).)*)", "\n".join(read(os.path.abspath(program))))
    order = ["global", "master", "group", "region"]
    ctl, lvl, regs, cur = {}, {h: {} for h in order}, [], None

    def flush():
        if cur == "region":
            d = {}
            for h in order:
                d.update(lvl[h])
            regs.append(d)

    for h, k, v in toks:
        if h:
            flush()
            cur = h
            if h in lvl:
                for lower in order[order.index(h):]:
                    lvl[lower] = {}
            continue
        v = v.strip()
        if cur == "control":
            m = re.match(r"set_cc(\d+)", k)
            if m:
                ctl[int(m.group(1))] = float(v)
        elif cur in lvl:
            lvl[cur][k] = v
    flush()
    return regs, ctl


def num(d, k, default=None):
    try:
        return float(d[k])
    except (KeyError, ValueError):
        return default


def select(regs, cc, key, vel):
    out = []
    for r in regs:
        if "sample" not in r or "silence" in r["sample"]:
            continue
        lo, hi = (num(r, "key"),) * 2 if "key" in r else (num(r, "lokey", 0), num(r, "hikey", 127))
        if not lo <= key <= hi or not num(r, "lovel", 0) <= vel <= num(r, "hivel", 127):
            continue
        ok = True
        for k, v in r.items():
            m = re.match(r"(lo|hi)cc(\d+)$", k)
            if m:
                c = cc.get(int(m.group(2)), 0)
                ok &= c >= float(v) if m.group(1) == "lo" else c <= float(v)
        if ok:
            out.append(r)
    return out


def velcurve(r):
    """the amp_velcurve top (linear to full there), or None (vel squared)"""
    tops = [int(m.group(1)) for k in r for m in [re.match(r"amp_velcurve_(\d+)$", k)] if m]
    return max(tops) if tops else None


def gain(r, cc, vel):
    amps = [(int(k[len("amplitude_cc"):]), float(v)) for k, v in r.items() if re.match(r"amplitude_cc\d+$", k)]
    g = sum(a * cc.get(c, 0) / 127 for c, a in amps) / 100 if amps else 1.0
    if "amplitude" in r and not amps:
        g = float(r["amplitude"]) / 100
    top = velcurve(r)
    return g * (min(1.0, vel / top) if top else (vel / 127.0) ** 2)


# ---- audio in and out ----
def read_audio(path):
    """mono float samples (the channels' mean) of a WAV, or of a FLAC through `flac` or macOS's afconvert"""
    if not os.path.exists(path):
        raise FileNotFoundError(f"missing sample: {path}")
    if path.lower().endswith(".flac"):
        with tempfile.TemporaryDirectory() as t:
            wav = os.path.join(t, "x.wav")
            for cmd in (["flac", "-s", "-d", "-o", wav, path], ["afconvert", "-f", "WAVE", "-d", "LEI16", path, wav]):
                try:
                    subprocess.run(cmd, check=True, capture_output=True)
                    return read_audio(wav)
                except (OSError, subprocess.CalledProcessError):
                    continue
            raise RuntimeError(f"can't decode {path}: install flac (or use macOS's afconvert)")
    with wave.open(path, "rb") as f:
        ch, w, sr, n = f.getnchannels(), f.getsampwidth(), f.getframerate(), f.getnframes()
        raw = f.readframes(n)
    if sr != SR:
        raise ValueError(f"{path}: {sr} Hz (the kits are 44.1 kHz; resampling isn't needed so isn't done)")
    if w == 3:
        raw = b"".join(b"\0" + raw[i:i + 3] for i in range(0, len(raw), 3))   # 24 -> 32 bit
        w = 4
    a = array.array({2: "h", 4: "i"}[w])
    a.frombytes(raw)
    if sys.byteorder == "big":
        a.byteswap()
    s = 1.0 / (1 << (8 * w - 1))
    return [sum(a[i:i + ch]) * s / ch for i in range(0, len(a), ch)] if ch > 1 else [x * s for x in a]


def write_f32(path, x):
    a = array.array("f", x)
    if sys.byteorder == "big":
        a.byteswap()
    path.write_bytes(a.tobytes())


# ---- a kit ----
def program(kit, root):
    regs, ctl = load(os.path.join(root, kit["program"]), kit.get("swap", {}))
    ctl.update(kit.get("cc", {}))
    return regs, ctl


def sample_path(kit, root, r):
    return os.path.normpath(os.path.join(root, os.path.dirname(kit["program"]), r["sample"].replace("\\", "/")))


def layers(regs, ctl, piece):
    """[(top velocity, [round robin -> regions])]: at most MAX_VL layers (the softest, the loudest, evenly between)"""
    cc = dict(ctl)
    cc.update(piece.get("cc", {}))
    key, runs = piece["key"], []
    for v in range(1, 128):
        sel = select(regs, cc, key, v)
        ident = tuple(sorted((num(r, "lovel", 0), num(r, "hivel", 127)) for r in sel))
        if not sel:
            continue
        if runs and runs[-1][0] == ident:
            runs[-1][1] = v
        else:
            runs.append([ident, v])
    if not runs:
        raise ValueError(f"no regions for {piece['name']} (key {key})")
    n = len(runs)
    keep = sorted({round(i * (n - 1) / (MAX_VL - 1)) for i in range(MAX_VL)}) if n > MAX_VL else range(n)
    out = []
    for i in keep:
        top = runs[i][1]
        sel = select(regs, cc, key, top)
        seq = max(int(num(r, "seq_length", 1)) for r in sel)
        rrs = [[r for r in sel if int(num(r, "seq_position", 1)) == (k % int(num(r, "seq_length", 1))) + 1]
               for k in range(min(seq, MAX_RR))]
        out.append((top, rrs, cc))
    return out


def needed(kit, root):
    """the sample files (relative to root) the kit's pieces use"""
    regs, ctl = program(kit, root)
    out = set()
    for p in kit["pieces"]:
        if "same" in p:
            continue
        for _, rrs, _ in layers(regs, ctl, p):
            for rr in rrs:
                out |= {os.path.relpath(sample_path(kit, root, r), root) for r in rr}
    return sorted(out)


def build(kit, root, out_dir):
    """the kit's files into out_dir; its kits.json entry"""
    root, out_dir = str(root), Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    regs, ctl = program(kit, root)
    pieces = []
    for p in kit["pieces"]:
        e = {"id": p["id"], "name": p["name"]}
        if "same" in p:
            e.update(same=p["same"], vel=round(100 * p["vel"]))
            pieces.append(e)
            continue
        mixes = []
        for top, rrs, cc in layers(regs, ctl, p):
            row = []
            for rr in rrs:
                x = []
                for r in rr:
                    g = gain(r, cc, top)
                    if g <= 0:
                        continue
                    s = read_audio(sample_path(kit, root, r))
                    if len(s) > len(x):
                        x += [0.0] * (len(s) - len(x))
                    for i, y in enumerate(s):
                        x[i] += g * y
                row.append(x)
            mixes.append((top, row))
        peak = max(abs(y) for _, row in mixes for x in row for y in x) or 1.0
        e.update(curve=1 if any(velcurve(r) for _, rrs, _ in layers(regs, ctl, p) for rr in rrs for r in rr) else 2,
                 vel=100, layers=[])
        for vl, (top, row) in enumerate(mixes):
            files = []
            for rr, x in enumerate(row):
                end = len(x)
                while end > 1 and abs(x[end - 1]) < TAIL * peak:
                    end -= 1
                x = x[:end]
                for i in range(max(0, end - FADE), end):
                    x[i] *= (end - i) / FADE if end > FADE else 1.0
                name = f"{kit['name']}-{p['name']}-v{vl + 1}-r{rr + 1}.f32"
                write_f32(out_dir / name, x)
                files.append(name)
            e["layers"].append({"hi": top, "files": files})
        pieces.append(e)
    return {"id": kit["id"], "name": kit["name"], "title": kit["title"], "kit": kit["kit"], "pieces": pieces,
            "lanes": kit["lanes"], "source": kit.get("source")}


def write_index(entries, out_dir):
    idx = {"_note": "Generated by workbench/tools/prep_kits.py: the shipped sampled kits (float32 mono 44.1 kHz files "
                    "beside this one, git-ignored). A layer plays up to its top velocity hi; a hit's level is (vel / hi) "
                    "^ curve; a piece with same plays that piece's files at vel percent of the velocity. lanes: kick, "
                    "snare, closed hat, open hat, shaker, rim, ride -> a piece (null: silent).",
           "kits": entries}
    (Path(out_dir) / "kits.json").write_text(json.dumps(idx, indent=1) + "\n")


# ---- fetching ----
def git(dest, *args, stdin=None):
    subprocess.run(["git", "-C", dest, *args], check=True, input=stdin, text=True, capture_output=True)


def fetch(repo, dest):
    """a shallow, sparse checkout of the repo at its pinned commit (the programs and the license first)"""
    url, sha = REPOS[repo]
    if not os.path.isdir(os.path.join(dest, ".git")):
        os.makedirs(dest, exist_ok=True)
        git(dest, "init", "-q")
        git(dest, "remote", "add", "origin", url)
        git(dest, "config", "remote.origin.partialclonefilter", "blob:none")
        git(dest, "sparse-checkout", "set", "--no-cone", "/LICENSE", "/Programs/")
        git(dest, "fetch", "-q", "--depth", "1", "--filter=blob:none", "origin", sha)
        git(dest, "checkout", "-q", sha)


def main(fetch_dir, out_dir):
    entries = []
    for kit in KITS:
        dest = os.path.join(fetch_dir, kit["repo"])
        fetch(kit["repo"], dest)
        if "CC0 1.0 Universal" not in Path(dest, "LICENSE").read_text(errors="replace"):
            raise SystemExit(f"{kit['repo']}: LICENSE isn't CC0 1.0")
        need = [p for p in needed(kit, dest) if not os.path.exists(os.path.join(dest, p))]
        if need:
            print(f"{kit['name']}: fetching {len(need)} samples", flush=True)
            git(dest, "sparse-checkout", "add", "--stdin", stdin="".join(f"/{p}\n" for p in need))
        kit = dict(kit, source={"url": REPOS[kit["repo"]][0], "commit": REPOS[kit["repo"]][1], "license": "CC0-1.0"})
        e = build(kit, dest, out_dir)
        print(f"{kit['name']}: {sum(len(l['files']) for p in e['pieces'] for l in p.get('layers', []))} files", flush=True)
        entries.append(e)
    write_index(entries, out_dir)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("usage: python3 -I workbench/tools/prep_kits.py FETCH_DIR workbench/samples")
    main(sys.argv[1], sys.argv[2])
