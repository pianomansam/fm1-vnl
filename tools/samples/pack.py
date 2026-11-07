# SPDX-License-Identifier: GPL-3.0-or-later
# VNL-1's sample packer: turns the recordings tools/samples/manifest.json names into the sample bank (the format is in
# app/engine/samples.h): app/engine/samples_blob.c, the bank's bytes (a head, then every zone's ADPCM), which the host
# build maps and tools/fm1_bank.py writes to the FM-1's flash; and app/engine/samples_data.c, the zones' tables (offsets
# into the bank), which the firmware keeps. Ported from the instrument spike's pack.py and pack2.py.
#   Pitched zones: mono, onset trimmed, pitch measured, retuned to the nearest whole MIDI note while resampled to
#   the zone's rate, cut to an attack plus a short loop of whole periods (the loop's decay flattened, its seam
#   crossfaded; the sampler's envelope does the decay), peak-normalized.
#   Drum hits: the listed mic files of one take summed, an optional high-pass, onset trimmed (1 ms kept), resampled,
#   cut to length with a cosine fade over the last 40%, peak-normalized.
#   Both: 4-bit IMA ADPCM from (0, idx0), idx0 the starting step index that encodes the zone best (the first 5 ms
#   weigh most: a poor start smears a hit's attack), never decoding to the clamp (the zone scaled down 3% at a time
#   until it doesn't), and a gain that brings the first 150 ms to unit RMS.
# The output is deterministic: the same inputs give a byte-identical file.
# Usage (with a Python that has numpy and scipy; run it with -I):
#   pack.py list                 the files to download: "dir<TAB>url<TAB>path" (fetch.sh reads this)
#   pack.py licenses             each source's license file: "dir<TAB>url<TAB>path", then "sha256<TAB>" the CC0 1.0
#                                legal code's hash every license file must match
#   pack.py pack SCRATCH DIR     pack from SCRATCH (fetch.sh's dl_* directories) into DIR's samples_data.c and
#                                samples_blob.c (DIR: app/engine)
# Input files are untrusted data: only parsed as RIFF/WAVE or SF2 sample data.
import json
import math
import os
import struct
import sys
import zlib
from fractions import Fraction
from urllib.parse import quote

import numpy as np
from scipy.io import wavfile
from scipy.signal import butter, resample_poly, sosfilt

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import sf2  # noqa: E402

STEP = [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97,
        107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
        876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428,
        4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350,
        22385, 24623, 27086, 29794, 32767]
IDX = [-1, -1, -1, -1, 2, 4, 6, 8]


def adpcm_encode(x, idx0):
    """x int16 array. IMA ADPCM from the state (0, idx0), low nibble first. Returns the bytes."""
    pred, idx = 0, idx0
    out = bytearray()
    nib = None
    for s in x:
        step = STEP[idx]
        d = int(s) - pred
        code = 0
        if d < 0:
            code = 8
            d = -d
        diff = step >> 3
        if d >= step:
            code |= 4; d -= step; diff += step
        if d >= step >> 1:
            code |= 2; d -= step >> 1; diff += step >> 1
        if d >= step >> 2:
            code |= 1; diff += step >> 2
        pred = pred - diff if code & 8 else pred + diff
        pred = max(-32768, min(32767, pred))
        idx = max(0, min(88, idx + IDX[code & 7]))
        if nib is None:
            nib = code
        else:
            out.append(nib | (code << 4))
            nib = None
    if nib is not None:
        out.append(nib)
    return bytes(out)


def adpcm_decode(b, n, idx0):
    pred, idx = 0, idx0
    y = np.zeros(n)
    for i in range(n):
        code = (b[i >> 1] >> (4 * (i & 1))) & 15
        step = STEP[idx]
        diff = step >> 3
        if code & 4: diff += step
        if code & 2: diff += step >> 1
        if code & 1: diff += step >> 2
        pred = pred - diff if code & 8 else pred + diff
        pred = max(-32768, min(32767, pred))
        idx = max(0, min(88, idx + IDX[code & 7]))
        y[i] = pred
    return y


def read_wav(path):
    """Mono float, the file's own scale (int formats to -1..1)."""
    rate, d = wavfile.read(path)
    if d.dtype == np.int32:
        d = d.astype(np.float64) / 2147483648.0
    elif d.dtype == np.int16:
        d = d.astype(np.float64) / 32768.0
    elif d.dtype == np.uint8:
        d = (d.astype(np.float64) - 128) / 128.0
    else:
        d = d.astype(np.float64)
    if d.ndim > 1:
        d = d.mean(axis=1)
    return rate, d


def wav_path(scratch, src, path):
    """Where fetch.sh left a file (FLACs are decoded to WAV beside them)."""
    p = os.path.join(scratch, 'dl_' + src, path)
    return p[:-5] + '.wav' if p.endswith('.flac') else p


def resample(x, rate, out_rate, shift=0.0):
    """x at `rate` to `out_rate`, its pitch raised `shift` semitones (a slower or faster read)."""
    r = Fraction(out_rate / (rate * 2 ** (shift / 12))).limit_denominator(2000) if shift else Fraction(out_rate, rate)
    return x.copy() if r == 1 else resample_poly(x, r.numerator, r.denominator)


def onset_trim(x, rate, th=0.03, keep=0.002):
    a = np.abs(x)
    i = int(np.argmax(a > th * a.max()))
    return x[max(0, i - int(keep * rate)):]


def measure_f0(x, rate, hint):
    """f0 in Hz near MIDI `hint` (within +-14 semitones): autocorrelation, the smallest strong lag."""
    t0 = int(0.15 * rate)
    seg = x[t0:t0 + int(0.25 * rate)]
    seg = seg - seg.mean()
    fmax = 440 * 2 ** ((hint + 14 - 69) / 12)
    fmin = 440 * 2 ** ((hint - 14 - 69) / 12)
    lmin, lmax = int(rate / fmax), int(rate / fmin) + 2
    n = len(seg)
    f = np.fft.rfft(seg, 2 * n)
    ac = np.fft.irfft(f * np.conj(f))[:n]
    ac = ac / ac[0]
    r = ac[lmin:lmax]
    best = r.max()
    cand = [i for i in range(1, len(r) - 1) if r[i] >= 0.85 * best and r[i] >= r[i - 1] and r[i] >= r[i + 1]]
    i = cand[0] if cand else int(np.argmax(r))
    p = 0.0
    if 0 < i < len(r) - 1:
        a, b, c = r[i - 1], r[i], r[i + 1]
        if a - 2 * b + c != 0:
            p = 0.5 * (a - c) / (a - 2 * b + c)
    return rate / (lmin + i + p)


def midi_of(f):
    return 69 + 12 * math.log2(f / 440.0)


class Zone:
    """One packed zone or hit: int16 PCM, its ADPCM bytes and the table fields."""

    def __init__(self, name, pcm, rate, root, ls, ll):
        while True:  # a decode that overshoots to the clamp: the zone 3% quieter until none does
            z = pcm.astype(np.float64)
            best = None
            for idx0 in range(0, 89, 4):
                b = adpcm_encode(pcm, idx0)
                d = adpcm_decode(b, len(pcm), idx0)
                e = np.sum((z[:80] - d[:80]) ** 2) * 4 + np.sum((z - d) ** 2)
                if best is None or e < best[0]:
                    best = (e, idx0, b, d)
            if np.max(best[3]) < 32767 and np.min(best[3]) > -32768:
                break
            pcm = np.round(z * 0.97).astype(np.int16)
        self.name, self.pcm, self.rate, self.root, self.ls, self.ll = name, pcm, rate, root, ls, ll
        _, self.idx0, self.data, dec = best
        self.snr = 10 * math.log10(np.sum(z ** 2) / (np.sum((z - dec) ** 2) + 1))
        w = int(0.15 * rate)
        self.loud = float(np.sqrt(np.mean((z[:w] / 32768.0) ** 2)))
        self.gain = 1.0 / max(self.loud, 1e-6)


def make_zone(name, x, rate, root_f, attack, loop, out_rate):
    """x float mono at `rate`, onset-trimmed, sounding MIDI `root_f`: retuned to the nearest whole note, cut to an
    attack and a loop of whole periods, the loop flattened and crossfaded."""
    root = int(round(root_f))
    y = resample(x, rate, out_rate, root - root_f)
    a = int(attack * out_rate) & ~1
    period = out_rate / (440 * 2 ** ((root - 69) / 12))
    L = max(int(round(round(loop * out_rate / period) * period)) & ~1, 64)
    need = a + L
    if len(y) < need + 8:
        y = np.concatenate([y, np.zeros(need + 8 - len(y))])
    y = y[:need].copy()
    w = max(int(period * 2), 32)  # hold the loop's level at its start's (the envelope does the decay)
    env = np.sqrt(np.convolve(y ** 2, np.ones(w) / w, mode='same')) + 1e-9
    y[a:] *= np.clip(env[a] / env[a:], 0.25, 4.0)
    X = int(L * 0.5)  # crossfade the loop's end into what comes before its start
    if X > 0 and a - X >= 0:
        f = np.linspace(0, 1, X, endpoint=False)
        f = 0.5 - 0.5 * np.cos(np.pi * f)
        e = a + L
        y[e - X:e] = y[e - X:e] * (1 - f) + y[a - X:a] * f
    y = y / (np.max(np.abs(y)) + 1e-12) * 0.95
    return Zone(name, np.round(y * 32767).astype(np.int16), out_rate, root, a, L)


def make_hit(name, parts, length, hp, out_rate):
    """parts: [(rate, float mono, weight)] the mics of one take (sample-aligned), summed."""
    rate = parts[0][0]
    n = max(len(d) for r, d, w in parts)
    x = np.zeros(n)
    for r, d, w in parts:
        assert r == rate, name + ': mic files at different rates'
        x[:len(d)] += w * d
    if hp:
        x = sosfilt(butter(4, hp, 'highpass', fs=rate, output='sos'), x)
    x = onset_trim(x, rate, 0.05, 0.001)
    y = resample(x, rate, out_rate)
    L = int(length * out_rate) & ~1
    if len(y) < L:
        y = np.concatenate([y, np.zeros(L - len(y))])
    y = y[:L].copy()
    pk = np.max(np.abs(y)) + 1e-12
    F = int(0.4 * L)
    y[L - F:] *= 0.5 + 0.5 * np.cos(np.pi * np.arange(F) / F)
    y = y / pk * 0.95
    return Zone(name, np.round(y * 32767).astype(np.int16), out_rate, 60, 0, 0)


def load_manifest():
    with open(os.path.join(HERE, 'manifest.json')) as f:
        return json.load(f)


def downloads(m):
    """(dir, url, path) for every file to fetch; an archive source is one file, its name the URL's."""
    out = []
    for key in sorted(m['sources']):
        s = m['sources'][key]
        if 'archive' in s:
            out.append(('dl_' + key, s['archive'], os.path.basename(s['archive'])))
            continue
        out.append(('dl_' + key, s['raw'] + quote(s['license']), s['license']))
    files = [(b['src'], z['file']) for b in m['banks'] for z in b.get('zones', [])]
    files += [(h['src'], p) for h in m['hits'] for p, w in h['files']]
    seen = set()
    for src, p in files:
        if (src, p) not in seen:
            seen.add((src, p))
            out.append(('dl_' + src, m['sources'][src]['raw'] + quote(p), p))
    return out


MAGIC, FORMAT, HEAD = b'VNLB', 1, 16   # samples.h: SMP_MAGIC, SMP_FORMAT, SMP_HEAD


def c_rows(b):
    return ',\n'.join(','.join(str(v) for v in b[i:i + 32]) for i in range(0, len(b), 32))


def c_zone(z):
    return '{%du, %du, %du, %du, %du, %d, %d, %.6ef}' % (z.off, len(z.pcm), z.ls, z.ll, z.rate, z.root, z.idx0, z.gain)


def write(banks, hits, out):
    """banks [(id, tag, [Zone])], hits [(id, Zone)] -> out/samples_blob.c (the bank) and out/samples_data.c (the
    tables); each zone's offset (z.off) set. The bank: the head (magic, format, the payload's length and CRC-32,
    little-endian), then the payload, every zone's ADPCM in table order. Returns the payload's length."""
    payload = bytearray()
    for z in [z for _, _, zs in banks for z in zs] + [z for _, z in hits]:
        z.off = len(payload)
        payload += z.data
    crc = zlib.crc32(payload) & 0xFFFFFFFF
    blob = MAGIC + struct.pack('<III', FORMAT, len(payload), crc) + payload
    gen = ['/* SPDX-License-Identifier: GPL-3.0-or-later */\n',
           '/* Generated by tools/samples/pack.py from tools/samples/manifest.json: do not edit (re-run the packer).\n']
    o = gen + ['   The sample bank (the format is in samples.h; the sources and their licenses in CREDITS.md). The host\n',
               '   build maps it from here; the FM-1 reads its copy in flash (tools/fm1_bank.py writes it), so the\n',
               '   firmware\'s build leaves this file empty. */\n',
               '#include "samples.h"\n\n',
               '#ifndef FELUCCA_ID\n',
               'const uint8_t smp_blob[%d] = {\n%s};\n' % (len(blob), c_rows(blob)),
               '#endif\n']
    with open(os.path.join(out, 'samples_blob.c'), 'w', newline='\n') as f:
        f.write(''.join(o))
    o = gen + ['   The zones\' tables: offsets into the sample bank\'s payload (samples_blob.c; the format is in samples.h;\n',
               '   the sources and their licenses in CREDITS.md). */\n',
               '#include "samples.h"\n\n']
    for _, tag, zs in banks:
        o.append('static const smp_zone_t B_%s[%d] = {\n%s};\n' % (tag, len(zs), ''.join('    %s,\n' % c_zone(z) for z in zs)))
    o.append('static const smp_bank_t BANKS[SB_N] = {\n')
    o.extend('    [%s] = {B_%s, %d},\n' % (bid, tag, len(zs)) for bid, tag, zs in banks)
    o.append('};\nstatic const smp_zone_t HITS[SH_N] = {\n')
    o.extend('    [%s] = %s,\n' % (hid, c_zone(z)) for hid, z in hits)
    o.append('};\n\n')
    o.append('const smp_bank_t *smp_bank(int id) { return smp_ready() && id >= 0 && id < SB_N ? &BANKS[id] : 0; }\n')
    o.append('const smp_zone_t *smp_hit(int id) { return smp_ready() && id >= 0 && id < SH_N ? &HITS[id] : 0; }\n')
    o.append('uint32_t smp_bytes(void) { return %du; }\n' % len(payload))
    o.append('uint32_t smp_crc(void) { return 0x%08Xu; }\n' % crc)
    with open(os.path.join(out, 'samples_data.c'), 'w', newline='\n') as f:
        f.write(''.join(o))
    return len(payload)


def pack(scratch, out):
    m = load_manifest()
    rate = m.get('rate', 16000)
    banks, hits, log = [], [], []
    for b in m['banks']:
        zs = []
        r = b.get('rate', rate)
        tag = b['id'][3:].lower()
        if 'sf2' in b:
            sf = sf2.SF2(os.path.join(scratch, 'dl_' + b['src'], b['sf2']))
            seen = set()
            for z in sf.zones(*b['preset']):
                if z['root'] in b['roots'] and z['root'] not in seen and z['vels'][1] == 127:
                    seen.add(z['root'])
                    pcm = np.array(sf.pcm(z['start'], z['end']), dtype=np.float64)
                    pcm /= np.max(np.abs(pcm)) + 1e-9
                    root_f = z['root'] - z['tune'] / 100.0
                    zs.append(make_zone('', onset_trim(pcm, z['rate']), z['rate'], root_f, b['attack'], b['loop'], r))
                    log.append('%s %s: root %.2f' % (b['id'], z['name'], root_f))
            assert seen == set(b['roots']), '%s: roots missing from the SF2: %s' % (b['id'], set(b['roots']) - seen)
        else:
            for zm in b['zones']:
                sr, x = read_wav(wav_path(scratch, b['src'], zm['file']))
                x = onset_trim(x / (np.max(np.abs(x)) + 1e-12), sr)
                root_f = midi_of(measure_f0(x, sr, zm['hint']))
                zs.append(make_zone('', x, sr, root_f, b['attack'], b['loop'], r))
                log.append('%s %s: hint %d measured %.2f' % (b['id'], os.path.basename(zm['file']), zm['hint'], root_f))
        zs.sort(key=lambda z: z.root)
        banks.append((b['id'], tag, zs))
    for h in m['hits']:
        parts = []
        for p, w in h['files']:
            sr, d = read_wav(wav_path(scratch, h['src'], p))
            parts.append((sr, d, w))
        hits.append((h['id'], make_hit('', parts, h['len'], h.get('hp', 0), h.get('rate', rate))))
    total = write(banks, hits, out)

    print('%-10s %5s %6s %7s %5s %6s %7s %6s %4s' % ('bank', 'zone', 'root', 'bytes', 'len_s', 'loop', 'snr_dB', 'gain',
                                                   'idx0'))
    for bid, _, zs in banks:
        for k, z in enumerate(zs):
            print('%-10s %5d %6d %7d %5.2f %6d %7.1f %6.2f %4d' % (bid, k, z.root, len(z.data), len(z.pcm) / z.rate, z.ll,
                                                                 z.snr, z.gain, z.idx0))
        print('%-10s total %d B' % (bid, sum(len(z.data) for z in zs)))
    print('%-13s %7s %5s %7s %6s %4s' % ('hit', 'bytes', 'len_s', 'snr_dB', 'gain', 'idx0'))
    for hid, z in hits:
        print('%-13s %7d %5.2f %7.1f %6.2f %4d' % (hid, len(z.data), len(z.pcm) / z.rate, z.snr, z.gain, z.idx0))
    print('hits total %d B' % sum(len(z.data) for _, z in hits))
    print('all samples %d B' % total)
    print('\n'.join(log))


def main():
    a = sys.argv[1:]
    if a[:1] == ['list']:
        for d, url, p in downloads(load_manifest()):
            print('%s\t%s\t%s' % (d, url, p))
    elif a[:1] == ['licenses']:
        m = load_manifest()
        for key in sorted(m['sources']):
            print('dl_%s\t%s\t%s' % (key, m['sources'][key]['url'], m['sources'][key]['license']))
        print('sha256\t%s' % m['license_sha256'])
    elif a[:1] == ['pack'] and len(a) == 3:
        pack(a[1], a[2])
    else:
        sys.exit('usage: pack.py list | licenses | pack SCRATCH DIR')


if __name__ == '__main__':
    main()
