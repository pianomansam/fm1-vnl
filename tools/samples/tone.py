# SPDX-License-Identifier: GPL-3.0-or-later
# VNL-1's offline tone chain: shapes a bank's raw notes before pack.py packs them (manifest.json: a bank's "tone",
# its settings by name; any left out take DEFAULTS'). Built for the jazz guitar (SB_JAZZ, the Shinyguitar's magnetic
# pickup DI): a guitar's tone knob, a light tube stage, a cabinet. The order is the signal's:
#   peak to in_db -> tone knob (a gentle low-pass) -> tube (tanh) -> cabinet (high-pass, body bump, roll-off) -> peak
#   to out_db
# Settings:
#   bypass      true: the notes as they come (a note re-amped elsewhere, say), only the level set
#   in_db       the note's peak going into the chain (dBFS): how hard it drives the tube
#   tone_hz     the tone knob: a low-pass's corner (Hz), tone_order poles (1: 6 dB/oct, a guitar's knob is gentle)
#   drive       the tube's tanh drive (y = tanh(drive x) / drive: unity for small signals; 0: none)
#   hp_hz       the cabinet's low cut (Hz), 2nd-order
#   body_hz, body_db, body_q   the cabinet's body: a peaking bump (Hz, dB, Q)
#   cab_hz      the cabinet's roll-off (Hz), cab_order poles (Butterworth)
#   out_db      the note's peak out (dBFS)
# Audition (any WAV in, a 32-bit float WAV out; settings as name=value over the bank's, or its variant's: BANK:NAME,
# else the one it uses):
#   python3 -I tools/samples/tone.py SB_JAZZ:chain IN.wav OUT.wav [drive=2 tone_hz=1800 ...]
import json
import math
import os
import sys

import numpy as np
from scipy.signal import butter, sosfilt

DEFAULTS = {"bypass": False, "in_db": -6.0, "tone_hz": 2200.0, "tone_order": 1, "drive": 1.5, "hp_hz": 90.0,
            "body_hz": 350.0, "body_db": 3.0, "body_q": 0.9, "cab_hz": 4500.0, "cab_order": 4, "out_db": -1.0}


def peaking(f0, db, q, rate):
    """a peaking biquad (the RBJ cookbook's) as one second-order section"""
    a = 10 ** (db / 40.0)
    w = 2 * math.pi * f0 / rate
    al = math.sin(w) / (2 * q)
    b0, b1, b2 = 1 + al * a, -2 * math.cos(w), 1 - al * a
    a0, a1, a2 = 1 + al / a, -2 * math.cos(w), 1 - al / a
    return np.array([[b0 / a0, b1 / a0, b2 / a0, 1.0, a1 / a0, a2 / a0]])


def peak_to(x, db):
    return x * (10 ** (db / 20.0) / (np.max(np.abs(x)) + 1e-12))


def settings(tone):
    s = dict(DEFAULTS)
    for k, v in (tone or {}).items():
        if k not in DEFAULTS:
            raise KeyError('tone: no setting %r (the settings: %s)' % (k, ', '.join(DEFAULTS)))
        s[k] = v
    return s


def chain(x, rate, tone):
    """x float mono at rate through the chain (tone: the bank's settings); float mono out"""
    s = settings(tone)
    if s["bypass"]:
        return peak_to(x, s["out_db"])
    y = peak_to(x, s["in_db"])
    if s["tone_hz"]:
        y = sosfilt(butter(int(s["tone_order"]), s["tone_hz"], 'low', fs=rate, output='sos'), y)
    if s["drive"]:
        y = np.tanh(s["drive"] * y) / s["drive"]
    if s["hp_hz"]:
        y = sosfilt(butter(2, s["hp_hz"], 'high', fs=rate, output='sos'), y)
    if s["body_db"]:
        y = sosfilt(peaking(s["body_hz"], s["body_db"], s["body_q"], rate), y)
    if s["cab_hz"]:
        y = sosfilt(butter(int(s["cab_order"]), s["cab_hz"], 'low', fs=rate, output='sos'), y)
    return peak_to(y, s["out_db"])


def main():
    a = sys.argv[1:]
    if len(a) < 3:
        sys.exit('usage: tone.py BANK IN.wav OUT.wav [name=value ...]')
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from pack import load_manifest, read_wav  # noqa: E402
    from scipy.io import wavfile
    bid, _, var = a[0].partition(':')
    bank = next(b for b in load_manifest()['banks'] if b['id'] == bid)
    if 'variants' in bank:
        bank = dict(bank, **bank['variants'][var or bank['use']])
    tone = dict(bank.get('tone', {}))
    for kv in a[3:]:
        k, v = kv.split('=', 1)
        tone[k] = json.loads(v)
    rate, x = read_wav(a[1])
    wavfile.write(a[2], rate, chain(x, rate, tone).astype(np.float32))


if __name__ == '__main__':
    main()
