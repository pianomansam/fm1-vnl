# SPDX-License-Identifier: MPL-2.0
# A minimal SoundFont 2 reader for tools/samples/pack.py (from the instrument spike): a preset's zones and their
# sample headers. Data only: nothing in the file is executed.
import struct

GEN_KEYRANGE, GEN_VELRANGE, GEN_INSTRUMENT, GEN_SAMPLEID = 43, 44, 41, 53
GEN_ROOTKEY, GEN_FINETUNE, GEN_COARSE = 58, 52, 51
GEN_SAMPLEMODES = 54
GEN_STARTOFS, GEN_ENDOFS, GEN_STARTLOOPOFS, GEN_ENDLOOPOFS = 0, 1, 2, 3
GEN_STARTCOARSE, GEN_ENDCOARSE, GEN_STARTLOOPCOARSE, GEN_ENDLOOPCOARSE = 4, 12, 45, 50


def _chunks(buf, off, end):
    while off + 8 <= end:
        cid, size = struct.unpack_from('<4sI', buf, off)
        yield cid, off + 8, size
        off += 8 + size + (size & 1)


class SF2:
    def __init__(self, path):
        buf = open(path, 'rb').read()
        assert buf[:4] == b'RIFF' and buf[8:12] == b'sfbk'
        self.buf = buf
        pd = {}
        for cid, off, size in _chunks(buf, 12, len(buf)):
            if cid == b'LIST':
                kind = buf[off:off + 4]
                for c2, o2, s2 in _chunks(buf, off + 4, off + size):
                    if kind == b'sdta' and c2 == b'smpl':
                        self.smpl = (o2, s2)
                    elif kind == b'pdta':
                        pd[c2] = buf[o2:o2 + s2]
        self.phdr = [struct.unpack_from('<20sHHHIII', pd[b'phdr'], i) for i in range(0, len(pd[b'phdr']), 38)]
        self.pbag = [struct.unpack_from('<HH', pd[b'pbag'], i) for i in range(0, len(pd[b'pbag']), 4)]
        self.pgen = [struct.unpack_from('<Hh', pd[b'pgen'], i) for i in range(0, len(pd[b'pgen']), 4)]
        self.inst = [struct.unpack_from('<20sH', pd[b'inst'], i) for i in range(0, len(pd[b'inst']), 22)]
        self.ibag = [struct.unpack_from('<HH', pd[b'ibag'], i) for i in range(0, len(pd[b'ibag']), 4)]
        self.igen = [struct.unpack_from('<Hh', pd[b'igen'], i) for i in range(0, len(pd[b'igen']), 4)]
        self.shdr = [struct.unpack_from('<20sIIIIIBbHH', pd[b'shdr'], i) for i in range(0, len(pd[b'shdr']), 46)]

    def _gens(self, bags, gens, b):
        g = {}
        for gi in range(bags[b][0], bags[b + 1][0]):
            op, amt = gens[gi]
            g[op] = amt
        return g

    def preset(self, bank, prog):
        for i, p in enumerate(self.phdr[:-1]):
            if p[1] == prog and p[2] == bank:
                return i
        raise KeyError((bank, prog))

    def zones(self, bank, prog):
        """The preset's sample zones: dicts with name, keys, vels, root, tune (cents), rate, start/end/loop (in the
        smpl chunk, samples), and the merged generators."""
        pi = self.preset(bank, prog)
        out = []
        pb0, pb1 = self.phdr[pi][3], self.phdr[pi + 1][3]
        for pb in range(pb0, pb1):
            pg = self._gens(self.pbag, self.pgen, pb)
            if GEN_INSTRUMENT not in pg:
                continue
            ii = pg[GEN_INSTRUMENT]
            ib0, ib1 = self.inst[ii][1], self.inst[ii + 1][1]
            glob = {}
            for ib in range(ib0, ib1):
                ig = self._gens(self.ibag, self.igen, ib)
                if GEN_SAMPLEID not in ig:
                    glob = ig
                    continue
                g = dict(glob)
                g.update(ig)
                sh = self.shdr[g[GEN_SAMPLEID]]
                name, start, end, ls, le, rate, opitch, pcorr, link, stype = sh
                kr = g.get(GEN_KEYRANGE, 0x7f00)
                vr = g.get(GEN_VELRANGE, 0x7f00)
                pkr = pg.get(GEN_KEYRANGE, 0x7f00)
                lo, hi = max(kr & 0xff, pkr & 0xff), min((kr >> 8) & 0xff, (pkr >> 8) & 0xff)
                if lo > hi:
                    continue
                s = start + g.get(GEN_STARTOFS, 0) + 32768 * g.get(GEN_STARTCOARSE, 0)
                e = end + g.get(GEN_ENDOFS, 0) + 32768 * g.get(GEN_ENDCOARSE, 0)
                l0 = ls + g.get(GEN_STARTLOOPOFS, 0) + 32768 * g.get(GEN_STARTLOOPCOARSE, 0)
                l1 = le + g.get(GEN_ENDLOOPOFS, 0) + 32768 * g.get(GEN_ENDLOOPCOARSE, 0)
                root = g.get(GEN_ROOTKEY, opitch)
                tune = pcorr + g.get(GEN_FINETUNE, 0) + 100 * g.get(GEN_COARSE, 0)
                out.append(dict(name=name.split(b'\0')[0].decode('latin1'), inst=self.inst[ii][0].split(b'\0')[0].decode('latin1'),
                                keys=(lo, hi), vels=(vr & 0xff, (vr >> 8) & 0xff), root=root, tune=tune,
                                rate=rate, start=s, end=e, loop=(l0, l1), stype=stype, mode=g.get(GEN_SAMPLEMODES, 0)))
        return out

    def pcm(self, start, end):
        """int16 samples [start, end) as a list via array (no numpy needed)."""
        import array
        o, _ = self.smpl
        a = array.array('h')
        a.frombytes(self.buf[o + 2 * start:o + 2 * end])
        return a
