#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Write VNL-1's sample bank (the sampled instruments and drum hits) into the FM-1's flash, over USB-MIDI.

    python3 tools/fm1_bank.py [BANK] [--port NAME] [--yes]
    python3 tools/fm1_bank.py --check [BANK]

BANK: app/engine/samples_blob.c (the default: tools/samples/pack.py's) or a raw bank file (the same bytes). The
firmware keeps only the zones' tables; the bank lives in the flash's data area (app.h APP_BANK_LO), outside the
update's reach, so a firmware update leaves it, and it is written once here (again only when the samples change).

The FM-1 must run VNL-1 built from the same samples, and be stopped (STOP FIRST otherwise). It checks the bank's head
against its tables first (another build's bank: refused, nothing changed), then erases its bank area, pulls the bank
and writes it, the head last, and checks it as it does at boot (CRC-32). Cut short, the FM-1 has no bank (it plays
its synth instruments only: NO SAMPLES) until this runs again. --check only checks the file.

Uses fm1-base's tools (base/tools: fm1_install.py's port lookup, fm1_backup.py's framing); needs mido with
python-rtmidi. Exit codes: 0 done, 1 failed, 2 bad arguments or bank, 3 FM-1 not found, 4 connection lost, 5 playing,
6 not VNL-1.
"""
import argparse
import os
import re
import struct
import sys
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "base", "tools"))
import fm1_install as I  # noqa: E402
import fm1_backup as B  # noqa: E402

BANK_CMD = 0x60                                  # app.c's
MAGIC, FORMAT, HEAD = b"VNLB", 1, 16             # app/engine/samples.h's SMP_MAGIC, SMP_FORMAT, SMP_HEAD
DEFAULT = os.path.join(ROOT, "app", "engine", "samples_blob.c")
KIND = B.Kind(identity="FM-1_22", name="VNL-1", port="VNL-1 FM-1")
PLAYING = 10
ERRORS = {2: "the transfer (a read request unanswered)",
          3: "the bank's head: not the bank this firmware was built with (install the firmware and the bank from the "
             "same checkout)",
          7: "a flash erase", 8: "a flash write", 9: "the check after writing (CRC-32)"}


def load(path):
    """the bank's bytes, checked (head and CRC-32); ValueError if it is not one"""
    with open(path, "rb") as f:
        data = f.read()
    if path.endswith(".c"):
        m = re.search(rb"smp_blob\[\d+\] = \{(.*?)\};", data, re.S)
        if not m:
            raise ValueError(f"{path}: no smp_blob in it")
        data = bytes(int(v) for v in m.group(1).split(b","))
    if len(data) < HEAD:
        raise ValueError(f"{path}: too short for a sample bank")
    magic, fmt, n, crc = struct.unpack_from("<4sIII", data)
    if magic != MAGIC or fmt != FORMAT:
        raise ValueError(f"{path}: not a VNL-1 sample bank (format {FORMAT})")
    if n != len(data) - HEAD or zlib.crc32(data[HEAD:]) != crc:
        raise ValueError(f"{path}: damaged (its length or CRC-32)")
    return data


def main(argv=None, backend=None, out=sys.stdout, ask=I.ask_tty):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("bank", nargs="?", default=DEFAULT, help="the bank (default: app/engine/samples_blob.c)")
    ap.add_argument("--port", metavar="NAME", help=f"MIDI port to use (part of its name; default: {KIND.port}'s)")
    ap.add_argument("--yes", action="store_true", help="do not ask first")
    ap.add_argument("--check", action="store_true", help="only check the bank file")
    a = ap.parse_args(argv)
    try:
        bank = load(a.bank)
    except (OSError, ValueError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 2
    crc = struct.unpack_from("<I", bank, 12)[0]
    print(f"bank: {len(bank)} bytes, samples CRC-32 {crc:08X}", file=out)
    if a.check:
        return 0
    try:
        if backend is None:
            try:
                backend = I.MidoBackend()
            except ImportError:
                raise I.InstallError("usage", "needs mido and python-rtmidi (pip install mido python-rtmidi)")
        dev = B.find(backend, a.port, KIND)
        print(f"device: {dev.id.text}  ({dev.name})", file=out)
        if not a.yes and not ask("Write the sample bank to the FM-1? (stop it first) [y/N] "):
            dev.link.close()
            print("cancelled", file=out)
            return 1
        try:
            dev.link.drain()
            dev.link.send(B.frame(BANK_CMD, struct.pack("<II", len(bank), zlib.crc32(bank))))
            code = B.serve(dev.link, bank, out, KIND)
        finally:
            print(file=out)
            dev.link.close()
    except I.InstallError as e:
        print(f"error: {e}", file=sys.stderr)
        return B.EXIT.get(e.code, 1)
    except KeyboardInterrupt:
        print("\ninterrupted: the FM-1 may have no bank now (synths only); run this again", file=sys.stderr)
        return 130
    if code == PLAYING:
        print("STOP FIRST: VNL-1 is playing. Stop it, then run this again.", file=sys.stderr)
        return B.EXIT["playing"]
    if code:
        print(f"error: not written: {ERRORS.get(code, 'code %d' % code)}." +
              (" Nothing changed." if code == 3 else " The FM-1 has no bank until this runs again."), file=sys.stderr)
        return 1
    print("done: the FM-1 plays its sampled instruments again", file=out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
