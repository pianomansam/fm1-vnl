# SPDX-License-Identifier: GPL-3.0-or-later
# VNL-1's sample packer: turns the recordings tools/samples/manifest.json names into the sample bank (the format is in
# app/engine/samples.h): app/engine/samples_blob.c, the bank's bytes (a head, then every zone's ADPCM), which the host
# build maps and tools/fm1_bank.py writes to the FM-1's flash; and app/engine/samples_data.c, the zones' tables (offsets
# into the bank), which the firmware keeps. Ported from the instrument spike's pack.py and pack2.py.
#   Pitched zones: mono, onset trimmed, pitch measured, retuned to the nearest whole MIDI note while resampled to
#   the zone's rate, cut to an attack plus a short loop of whole periods (the loop's decay flattened, its seam
#   crossfaded; the sampler's envelope does the decay), peak-normalized.
#   Kit pieces (the workbench's sampled kits): resampled, cut to length with a cosine fade over the last 40%,
#   peak-normalized.
#   Every zone: 4-bit IMA ADPCM from (0, idx0), idx0 the starting step index that encodes the zone best (the first 5 ms
#   weigh most: a poor start smears a hit's attack), never decoding to the clamp (the zone scaled down 3% at a time
#   until it doesn't), and a gain that brings the first 150 ms to unit RMS.
# The output is deterministic: the same inputs give a byte-identical file.
# Usage (with a Python that has numpy and scipy; run it with -I):
#   pack.py list                 the files to download: "dir<TAB>url<TAB>path" (fetch.sh reads this)
#   pack.py licenses             each source's license file: "dir<TAB>url<TAB>path", then "sha256<TAB>" the CC0 1.0
#                                legal code's hash every license file must match
#   pack.py pack SCRATCH DIR [KITS]  pack from SCRATCH (fetch.sh's dl_* directories) and KITS (the workbench's
#                                sampled kits: workbench/tools/prep_kits.py's folder, default workbench/samples) into
#                                DIR's samples_data.c and samples_blob.c (DIR: app/engine)
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
from scipy.signal import resample_poly

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
    """One packed zone: int16 PCM, its ADPCM bytes and the table fields."""

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
    seen = set()
    for src, p in files:
        if (src, p) not in seen:
            seen.add((src, p))
            out.append(('dl_' + src, m['sources'][src]['raw'] + quote(p), p))
    return out


MAGIC, FORMAT, HEAD = b'VNLB', 2, 16   # samples.h: SMP_MAGIC, SMP_FORMAT, SMP_HEAD
# the bank's two areas in the FM-1's flash (app.h APP_BANK_LO..HI, APP_BANK1_LO..HI): the head and the first part,
# then the rest; the sectors in base/include/plat.h's extra windows (PLAT_XDATA0: the first four of the first area,
# PLAT_XDATA1: the whole second) never have "AT" at their bytes 0xF06..0xF07 (an update record's mark)
AREA0, AREA0_END, AREA1, AREA1_END = 0x93000, 0xDC000, 0xE5000, 0xFC000
XDATA0_END = 0x97000
# samples.h SK_*: wb_drums.h WB_SMP_*; None: a set kept empty, its index held (SK_GOGODZE: Dusty, retired 2026-11-10,
# its pieces out of the bank)
KIT_SETS = [None, 'rusty', 'brushes']
KIT_PIECES = ['kick', 'snare', 'rim', 'hat', 'ohat', 'ride']     # wb_drums.h WB_P_*


def c_rows(b):
    return ',\n'.join(','.join(str(v) for v in b[i:i + 32]) for i in range(0, len(b), 32))


def c_zone(z):
    return '{%du, %du, %du, %du, %du, %d, %d, %.6ef}' % (z.off, len(z.pcm), z.ls, z.ll, z.rate, z.root, z.idx0, z.gain)


def marked(payload, split):
    """the payload's offsets that land on 0xF06 of an extra window's sector and read "AT" there"""
    bad = []
    for sec in range(AREA0, XDATA0_END, 0x1000):
        o = sec + 0xF06 - AREA0 - HEAD
        if o < min(split, len(payload) - 1) and payload[o:o + 2] == b'AT':
            bad.append(o)
    for sec in range(AREA1, AREA1_END, 0x1000):
        o = sec + 0xF06 - AREA1 + split
        if o < len(payload) - 1 and payload[o:o + 2] == b'AT':
            bad.append(o)
    return bad


def layout(first, kit):
    """the payload: first (every zone), then kit's in the first area where they fit (first fit), the rest in the
    second (a zone whole in one); a zone that would put "AT" on a marked spot moves on a byte. Returns (payload, split)."""
    cap0, cap1 = AREA0_END - AREA0 - HEAD, AREA1_END - AREA1
    pad = {}
    while True:
        a0, a1 = [], []
        n0 = sum(len(z.data) + pad.get(id(z), 0) for z in first)
        for z in kit:
            if n0 + len(z.data) + pad.get(id(z), 0) <= cap0:
                a0.append(z)
                n0 += len(z.data) + pad.get(id(z), 0)
            else:
                a1.append(z)
        payload = bytearray()
        split = 0
        for zs in (first + a0, a1):
            if zs is a1:
                split = len(payload)
            for z in zs:
                payload += bytes(pad.get(id(z), 0))
                z.off = len(payload)
                payload += z.data
        assert split <= cap0 and len(payload) - split <= cap1, 'the bank does not fit its areas: %d + %d kit B, %d + %d' % (sum(len(z.data) for z in first), sum(len(z.data) for z in kit), split, len(payload) - split)
        bad = marked(payload, split)
        if not bad:
            return payload, split
        z = max((z for z in first + a0 + a1 if z.off <= bad[0]), key=lambda z: z.off)
        pad[id(z)] = pad.get(id(z), 0) + 1


def write(banks, kits, out):
    """banks [(id, tag, [Zone])], kits [[piece or None]] (a piece: ([Zone], [hi], curve, vel))
    -> out/samples_blob.c (the bank) and out/samples_data.c (the tables); each zone's offset (z.off) set. The bank:
    the head (magic, format, the payload's length and CRC-32, little-endian), then the payload, every zone's ADPCM
    (layout's order). Returns (the payload's length, split)."""
    kz = []
    for pieces in kits:
        for pc in pieces:
            for z in pc[0] if pc else []:
                if all(z is not k for k in kz):
                    kz.append(z)
    payload, split = layout([z for _, _, zs in banks for z in zs], kz)
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
    o.append('};\nstatic const smp_zone_t KZ[%d] = {\n%s};\n' % (len(kz), ''.join('    %s,\n' % c_zone(z) for z in kz)))
    o.append('static const smp_piece_t PIECES[SK_N][SK_P_N] = {\n')
    for pieces in kits:
        o.append('    {%s},\n' % ',\n     '.join(
            '{{%s}, {%s}, {%s}, {%s}, %d, %d, %d}' % (
                ', '.join('&KZ[%d]' % next(i for i, k in enumerate(kz) if k is z) for z in pc[0]),
                ', '.join(str(h) for h in pc[1]), ', '.join(str(u) for u in pc[2]),
                ', '.join('%.4ff' % v for v in pc[3]), len(pc[1]), pc[4], pc[5])
            if pc else '{{0}, {0}, {0}, {0}, 0, 0, 0}' for pc in pieces))
    o.append('};\n\n')
    o.append('const smp_bank_t *smp_bank(int id) { return smp_ready() && id >= 0 && id < SB_N ? &BANKS[id] : 0; }\n')
    o.append('const smp_piece_t *smp_piece(int set, int p)\n{\n    return smp_ready() && set >= 0 && set < SK_N && p >= 0 && '
             'p < SK_P_N && PIECES[set][p].n ? &PIECES[set][p] : 0;\n}\n')
    o.append('uint32_t smp_bytes(void) { return %du; }\n' % len(payload))
    o.append('uint32_t smp_split(void) { return %du; }\n' % split)
    o.append('uint32_t smp_crc(void) { return 0x%08Xu; }\n' % crc)
    with open(os.path.join(out, 'samples_data.c'), 'w', newline='\n') as f:
        f.write(''.join(o))
    return len(payload), split


def kit_zone(x, rate, length):
    """a kit piece's layer (float mono at 44.1 kHz, prep_kits.py's: from its onset) at rate, cut to length s (the
    last 40% faded); its gain gives back the file's level (the decoded sample, as -1..1, times gain)"""
    y = resample(x, 44100, rate)
    L = min(int(length * rate), len(y)) & ~1
    y = y[:L].copy()
    F = int(0.4 * L)
    y[L - F:] *= 0.5 + 0.5 * np.cos(np.pi * np.arange(F) / F)
    pk = np.max(np.abs(y)) + 1e-12
    z = Zone('', np.round(y / pk * 0.95 * 32767).astype(np.int16), rate, 60, 0, 0)
    z.gain = float(pk * 32768.0 / max(1, int(np.max(np.abs(z.pcm.astype(np.int32))))))
    return z


def pack_kits(m, kits_dir):
    """the sampled kits' pieces, KIT_SETS x KIT_PIECES: ([Zone], [hi], [use], [lvl], curve, vel) or None. Every
    velocity range kits.json gives (its top hi) keeps its level: it plays the kept layer nearest it (use) at the level
    ratio of its own file to that one's (lvl, RMS over the first 200 ms). A piece kits.json gives as "same" shares
    that piece's zones; a file two kits ship (the same bytes) is packed once."""
    k = m['kits']
    with open(os.path.join(kits_dir, 'kits.json')) as f:
        kj = {kit['name']: kit for kit in json.load(f)['kits']}
    made, out = {}, []
    for name in KIT_SETS:
        if name is None:
            out.append([None] * len(KIT_PIECES))
            continue
        by_id = {p['id']: p for p in kj[name]['pieces']}
        row = []
        for pid, pname in enumerate(KIT_PIECES):
            p = by_id.get(pid)
            if not p:
                row.append(None)
                continue
            q = by_id[p['same']] if p.get('same') is not None else p
            qn = KIT_PIECES[q['id']]
            ln = k.get('kit_len', {}).get(name, {}).get(qn, k['len'][qn])
            nl = len(q['layers'])
            keep = sorted(set(min(li, nl - 1) for li in k['layers']))
            x, zs = [], []
            for lay in q['layers']:
                with open(os.path.join(kits_dir, lay['files'][0]), 'rb') as f:
                    x.append(f.read())
            for li in keep:
                key = (zlib.crc32(x[li]), len(x[li]), ln)
                if key not in made:
                    made[key] = kit_zone(np.frombuffer(x[li], dtype='<f4').astype(np.float64), k['rate'], ln)
                zs.append(made[key])
            rms = [float(np.sqrt(np.mean(np.frombuffer(b, dtype='<f4')[:8820].astype(np.float64) ** 2))) for b in x]
            use = [min(range(len(keep)), key=lambda j: (abs(keep[j] - li), -keep[j])) for li in range(nl)]
            lvl = [rms[li] / rms[keep[use[li]]] for li in range(nl)]
            row.append((zs, [lay['hi'] for lay in q['layers']], use, lvl, q.get('curve', 1), p.get('vel', 100)))
        out.append(row)
    return out


def pack(scratch, out, kits_dir):
    m = load_manifest()
    rate = m.get('rate', 16000)
    banks, log = [], []
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
    kits = pack_kits(m, kits_dir)
    total, split = write(banks, kits, out)

    print('%-10s %5s %6s %7s %5s %6s %7s %6s %4s' % ('bank', 'zone', 'root', 'bytes', 'len_s', 'loop', 'snr_dB', 'gain',
                                                   'idx0'))
    for bid, _, zs in banks:
        for k, z in enumerate(zs):
            print('%-10s %5d %6d %7d %5.2f %6d %7.1f %6.2f %4d' % (bid, k, z.root, len(z.data), len(z.pcm) / z.rate, z.ll,
                                                                 z.snr, z.gain, z.idx0))
        print('%-10s total %d B' % (bid, sum(len(z.data) for z in zs)))
    for name, row in zip(KIT_SETS, kits):
        if name is None:
            continue
        for pname, pc in zip(KIT_PIECES, row):
            if pc:
                print('kit %-8s %-6s %s; tops %s plays %s at %s; vel %d' % (
                    name, pname, ' '.join('%dB/%.2fs/%.1fdB' % (len(z.data), len(z.pcm) / z.rate, z.snr) for z in pc[0]),
                    pc[1], pc[2], ' '.join('%.2f' % v for v in pc[3]), pc[5]))
    print('all samples %d B: area 0 %d of %d B, area 1 %d of %d B' % (total, HEAD + split, AREA0_END - AREA0,
                                                                     total - split, AREA1_END - AREA1))
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
    elif a[:1] == ['pack'] and len(a) in (3, 4):
        pack(a[1], a[2], a[3] if len(a) == 4 else os.path.join(HERE, '..', '..', 'workbench', 'samples'))
    else:
        sys.exit('usage: pack.py list | licenses | pack SCRATCH DIR [KITS]')


if __name__ == '__main__':
    main()
