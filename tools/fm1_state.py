#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Read VNL-1's live state from a connected FM-1, over USB-MIDI (read-only).

    python3 tools/fm1_state.py [--port NAME] [--watch SECONDS] [--reset] [--ab FLAGS]

Asks the FM-1 (the browser-remote prefix's command 04: F0 7D 56 4E 04 F7) and reads its answer, JSON in frames
F0 7D 56 4E 14 pp nn <text> F7 (frame pp of nn; app/app.c live_state): the screen and its cursor, the song (seed,
key, mode, tempo, section, bar), each row's value and name, what waits, mutes, solo, levels, the transport, and its
CPU (the render's load now and its peak in % of a block, late blocks, the slowest redraw and block render in us, the
CPU guard's level, its steps up and levels come off, the highest, the voices it cut, and the platform's blocks over
90 %). Writes app/build/live/device.json (as the workbench writes workbench.json) and prints a
summary. --watch N: again every N seconds until Ctrl-C. --reset: each answer then zeroes the measures (the peak, the
late blocks and their causes, the slowest render and redraw, the guard's counts: 04 01), to measure one event at a time.
--ab FLAGS[,FLAGS2]: set the A/B switches first (04 02 ff gg; 0 all as built), to find a drop-out or a sound without a
reflash. FLAGS, added together: 1 the CPU guard off, 2 the Parts roll's look-ahead off, 4 the kit voice render paused,
8 the shipped kits' lanes off, 16 the chorus and delay off, 32 the plate off, 64 every voice at the full rate.
FLAGS2: 1 the bed off, 2 the buses' filters off, 4 the drums' compressor off, 8 the glue off, 16 the mud high-pass
off, 64 the output summed to mono. The late blocks' causes (cpu.late_by, the first that applies): song (a new song, its
first bar's planning, its end, a resume), row (a change landing), jump, roll (the Parts roll's look-ahead), bar (the
downbeat), plan (a piece of the next bar's planning: its voicings, the lead's plan, a bar of the pad group), cmd (a
mute, solo, level, jump or A/B command), guard (it stepped), ramp (a section's filters or the fade moving), step (a
16th ticked), notes (notes starting), draw, kit (the kit voices rendering), flash, usb (a SysEx, this tool's answers
among them), next (the next song rolled), voices (only the voices and effects sounding), other. Then the last late
blocks (cpu.late_at, up to 4, oldest first): the bar and 16th each ticked (bar:step, from 1; "-" none), the
planning's piece that 16th made, the block's render in us and all its causes.
Then the CPU by stage since the last reset (prof, app/engine/prof.h): each stage of the audio render (drums, bass,
comp, lead, pad, texture: the bed, bus: the buses' filters and mix, fx: dust, chorus, delay, reverb, control: the
song's steps, notes starting, the output) as us a block on average, % of the CPU, and its share of the heaviest block;
then the main loop's redraws (ui) and kit voices, and what is left (the rest of the main loop, and idle). The voices'
parts are sampled (a sample a segment timed part by part), so read them over a few seconds or more.
Coexists with the browser remote (its other commands are left alone). Uses fm1-base's tools (base/tools: fm1_install.py's port lookup); needs mido with python-rtmidi.
Exit codes: 0 done, 1 no answer, 3 FM-1 not found.
"""
import argparse
import json
import os
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "app", "build", "live", "device.json")
PFX = bytes([0xF0, 0x7D, 0x56, 0x4E])
ASK = PFX + bytes([0x04, 0xF7])
ASK_RESET = PFX + bytes([0x04, 0x01, 0xF7])


def ask_ab(flags, flags2=0):
    return PFX + bytes([0x04, 0x02, flags & 0x7F, flags2 & 0x7F, 0xF7])
REPLY = 0x14


def decode(frames):
    """the state from its frames (bytes each, F0 .. F7, any others among them ignored) -> dict, or None while some
    are missing; ValueError if they don't make JSON"""
    parts, total = {}, None
    for f in frames:
        if len(f) < 8 or f[:4] != PFX or f[4] != REPLY or f[-1] != 0xF7:
            continue
        k, n = f[5], f[6]
        if total is None:
            total = n
        if n != total:              # (an answer to another ask: start again from this one)
            parts, total = {}, n
        parts[k] = f[7:-1]
    if total is None or len(parts) < total:
        return None
    return json.loads(b"".join(parts[k] for k in range(total)).decode("ascii"))


def summary(s):
    rows = s.get("rows", {})
    name = lambda r: (rows.get(r) or [None, ""])[-1]
    cpu = s.get("cpu", {})
    guard = ["off", "tails", "bed", "plate", "voices", "thin"]   # (app/drums/cpuguard.h CG_*)
    gname = lambda k: guard[cpu.get(k, 0)] if 0 <= cpu.get(k, 0) < len(guard) else '?'
    return (f"{'playing' if s.get('playing') else 'stopped'} seed {s.get('seed')} bar {s.get('bar')}/{s.get('bars')} "
            f"{s.get('section') or '-'}; {s.get('key')} {s.get('mode')} {s.get('tempo')} BPM; screen {s.get('screen')} "
            f"(cursor {s.get('cursor')}); keys {name('keys')}, lead {name('lead')}, kit {name('kit')}; "
            f"mute {','.join(s.get('mute', [])) or '-'} solo {s.get('solo') or '-'}; "
            f"cpu {cpu.get('load')}% (peak {cpu.get('peak')}%), late {cpu.get('late')}, render {cpu.get('render_us')} us, "
            f"redraw {cpu.get('redraw_us')} us, record turn {cpu.get('spin_us')} us, fade {cpu.get('fade_us')} us, "
            f"guard {gname('guard')} "
            f"(top {gname('guard_top')}, {cpu.get('guard_steps')} up, {cpu.get('guard_recov', 0)} back, "
            f"{cpu.get('shed', 0)} voices cut, {cpu.get('over90', 0)} over 90%); "
            f"late by {' '.join(f'{k} {v}' for k, v in cpu.get('late_by', {}).items() if v) or '-'}; "
            f"last late {late_at(cpu)}"
            f"{'; A/B ' + str(cpu.get('ab')) + ',' + str(cpu.get('ab2')) if cpu.get('ab') or cpu.get('ab2') else ''}")


PIECES = ["", "voicing", "next voicing", "lead plan", "pad bar 1", "pad bar 2", "pad bar 3", "pad bar 4"]


def late_at(cpu):
    """the last late blocks (cpu.late_at), as text: bar:16th (from 1), the planning's piece, us, causes"""
    names = list(cpu.get("late_by", {}))             # (the causes' bits, in late_by's order: app.c MC_*)
    out = []
    for bar, step, piece, us, causes in cpu.get("late_at", []):
        where = f"{bar + 1}:{step + 1}" if step < 16 else f"{bar + 1}:-"
        why = ",".join(n for i, n in enumerate(names) if causes >> i & 1) or "-"
        what = f" {PIECES[piece]}" if 0 < piece < len(PIECES) else (f" piece {piece}" if piece else "")
        out.append(f"{where}{what} {us} us ({why})")
    return "; ".join(out) or "-"


def prof_table(s):
    """the CPU by stage (the live state's prof), as lines; [] when there is none"""
    p = s.get("prof")
    if not p or not p.get("blocks"):
        return []
    n, blk, ms = p["blocks"], p["block_us"], max(p["ms"], 1)
    wall = ms * 1000.0                                   # us the window lasted
    render = sum(p["us"])
    worst = p.get("worst_us", 0)
    pct = lambda us: 100.0 * us / wall               # noqa: E731
    out = [f"CPU by stage over {ms / 1000:.1f} s ({n} audio blocks of {blk} us; since the last reset):",
           f"  {'stage':<9} {'us/block':>9} {'% CPU':>7}   {'heaviest block (us)':>19}"]
    for name, us, w in zip(p["stages"], p["us"], p["worst"]):
        out.append(f"  {name:<9} {us / n:9.0f} {pct(us):6.1f}%   {w:19d}")
    out.append(f"  {'render':<9} {render / n:9.0f} {pct(render):6.1f}%   {worst:19d}  "
               f"(heaviest {100.0 * worst / blk:.0f}% of its block)")
    ui, kit = p.get("ui_us", 0), p.get("kit_us", 0)
    out.append(f"  {'ui':<9} {'':9} {pct(ui):6.1f}%   redraws (longest {s.get('cpu', {}).get('redraw_us')} us)")
    out.append(f"  {'kit':<9} {'':9} {pct(kit):6.1f}%   the kit voices, rendered ahead in the main loop")
    out.append(f"  {'rest':<9} {'':9} {max(0.0, 100.0 - pct(render + ui + kit)):6.1f}%   the rest of the main loop, "
               "and idle")
    return out


def ask(link, timeout=2.0, pkt=ASK):
    link.drain()
    if not link.send(pkt):
        return None
    got, t0 = [], time.monotonic()
    while time.monotonic() - t0 < timeout:
        pkt = link.read(0.05)
        if pkt is None:
            continue
        got.append(pkt)
        s = decode(got)
        if s is not None:
            return s
    return None


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", metavar="NAME", help="MIDI port to use (part of its name; default: VNL-1 FM-1's)")
    ap.add_argument("--watch", type=float, metavar="SECONDS", help="ask again every SECONDS until Ctrl-C")
    ap.add_argument("--reset", action="store_true", help="zero the measures after each answer")
    ap.add_argument("--ab", metavar="FLAGS[,FLAGS2]", help="set the A/B switches first (see above)")
    a = ap.parse_args(argv)
    sys.path.insert(0, os.path.join(ROOT, "base", "tools"))
    import fm1_install as I  # noqa: E402
    import fm1_backup as B  # noqa: E402
    try:
        dev = B.find(I.MidoBackend(), a.port, B.Kind(identity="FM-1_22", name="VNL-1", port="VNL-1 FM-1"))
    except I.InstallError as e:
        print(f"error: {e}", file=sys.stderr)
        return 3
    try:
        if a.ab is not None and ask(dev.link, pkt=ask_ab(*[int(x) for x in a.ab.split(",")][:2])) is None:
            print("error: the FM-1 did not answer the A/B switches", file=sys.stderr)
            return 1
        while True:
            s = ask(dev.link, pkt=ASK_RESET if a.reset else ASK)
            if s is None:
                print("error: the FM-1 did not answer (a VNL-1 without the live state?)", file=sys.stderr)
                return 1
            os.makedirs(os.path.dirname(OUT), exist_ok=True)
            with open(OUT, "w") as f:
                json.dump(s, f, indent=1)
            print(summary(s))
            print("\n".join(prof_table(s)))
            if not a.watch:
                return 0
            time.sleep(a.watch)
    except KeyboardInterrupt:
        return 0
    finally:
        dev.link.close()


if __name__ == "__main__":
    sys.exit(main())
