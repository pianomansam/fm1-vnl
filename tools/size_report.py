#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Where the firmware's flash and RAM go, by category (read-only: the last build's ELF, no device).

    python3 tools/size_report.py [--files]

Reads app/build/vnl-1.elf's symbol table and gives each sized symbol to the file that defines it: the -O2 units
(app/build/o2/*.o) by their own symbol tables, the unity build (platform, app.c, UI) by a column-0 definition in
the source. The unity build inlines across files, so its platform/app/UI split is approximate (a few KB); the o2
units are exact. Unclaimed bytes (padding, assembly, constant pools) are shown as such. The sample bank (its own
flash region) is split by instrument from app/engine/samples_data.c. --files lists every file under its category.
All the FM-1's RAM is on-chip SRAM; the linker splits it into the regions below (base/device/app.ld)."""
import collections, glob, os, re, struct, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))) + '/'
BUILD = ROOT + 'app/build/'
# (budget, hardware limit) in bytes: VNL-1's budgets as tools/mem-check.sh checks them; the regions from app.ld
LIMITS = {'image': (409600, 409600), 'ram': (81920, 98304), 'pool': (307200, 344064), 'ram_text': (32768, 32768)}
CATS = [('Built-in sample (jazz guitar, 16-bit)', r'samples_pcm'),
        ('Engines: instruments', r'engine/(voices|patch|modal|sampler|samples|samples_data)\.c'),
        ('Engines: drums', r'app/drums/|engine/drums\.c'),
        ('Engines: effects', r'engine/(fx|rings_fx)\.c'),
        ('Profiler', r'engine/prof\.c'),
        ('Song planner', r'song/(song|plan|roll|library|override|rng)\.c'),
        ('Harmony and theory', r'song/(harmony|theory)\.c'),
        ('Lead planner', r'song/(lead\.c|leads/)'),
        ('Accompaniment and pads', r'song/(strum|rolled|pads)\.c'),
        ('Drum and bass patterns', r'song/(groove|patterns_gen)\.c'),
        ('UI: fonts', r'font'),
        ('UI: screens', r'app/ui/'),
        ('App control (app.c)', r'app/app\.[ch]'),
        ('Platform (fm1-base)', r'^base/'),
        ('Unattributed symbols', r'.')]


def elf(path):
    """(sections {name: (addr, size)}, symbols [(name, size, addr, section)]) of a little-endian ELF32."""
    d = open(path, 'rb').read()
    shoff, = struct.unpack_from('<I', d, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from('<HHH', d, 0x2e)
    sh = [struct.unpack_from('<10I', d, shoff + i * shentsize) for i in range(shnum)]
    cstr = lambda off: d[off:d.index(b'\0', off)].decode('latin1')
    names = [cstr(sh[shstrndx][4] + x[0]) for x in sh]
    syms = []
    for x in sh:
        if x[1] != 2:          # SHT_SYMTAB
            continue
        for k in range(x[5] // 16):
            nm, addr, size, info, _, shn = struct.unpack_from('<IIIBBH', d, x[4] + k * 16)
            if size and info & 15 in (1, 2) and 0 < shn < len(names):      # sized objects and functions
                syms.append((cstr(sh[x[6]][4] + nm), size, addr, names[shn]))
    return {n: (x[3], x[5]) for n, x in zip(names, sh)}, syms


def owners():
    """symbol name -> its file, for the -O2 units (by their objects) and the unity build (by its sources)."""
    own = collections.defaultdict(set)
    for o in glob.glob(BUILD + 'o2/*.o'):
        stem = os.path.basename(o)[:-2]
        parts = stem.split('_')
        src = next((ROOT + 'app/' + '/'.join(parts[:i]) + '/' + '_'.join(parts[i:]) + '.c'
                    for i in range(1, len(parts)) if os.path.exists(ROOT + 'app/' + '/'.join(parts[:i]) + '/'
                                                                    + '_'.join(parts[i:]) + '.c')), None)
        if src:                # objects of sources since removed are skipped
            for n, *_ in elf(o)[1]:
                own[n].add(src[len(ROOT):])
    return own


def unity_finder():
    files = [ROOT + 'app/app.c'] + sorted(glob.glob(ROOT + 'app/ui/*.[ch]') + glob.glob(ROOT + 'app/build/gen/*.h')
                                          + glob.glob(ROOT + 'app/*.h') + glob.glob(ROOT + 'base/device/*.[ch]')
                                          + glob.glob(ROOT + 'base/lib/*.[ch]')
                                          + glob.glob(ROOT + 'base/bsp/**/*.[ch]', recursive=True))
    text = {f: open(f, encoding='latin1').read().split('\n') for f in files}

    def find(name):
        rx = re.compile(r'\b' + re.escape(name) + r'\b\s*(\(|\[|=|\{|$)')
        for f in files:        # a definition starts at column 0 and is no prototype
            for ln in text[f]:
                if ln[:1] and not ln[0].isspace() and ln[0] not in '#/*' and not ln.startswith('extern') \
                        and not ln.rstrip().endswith(');') and rx.search(ln):
                    return f[len(ROOT):]
        for f in files:        # made by a macro (the fonts' FONT_X_DATA)
            if any(name in ln for ln in text[f]):
                return f[len(ROOT):]
        return '?'
    return find


def category(f):
    return next(c for c, rx in CATS if re.search(rx, f))


def bank():
    t = open(ROOT + 'app/engine/samples_data.c').read()
    total = int(re.search(r'smp_bytes\(void\) \{ return (\d+)u', t).group(1))
    zones = [(name, int(o), int(i)) for name, body in re.findall(r'smp_zone_t (\w+)\[\d+\] = \{(.*?)\n\};', t, re.S)
             for o, i in re.findall(r'\{(\d+)u, \d+u, \d+u, \d+u, \d+u, \d+, (\d+),', body)]
    z = sorted((o, n) for n, o, i in zones if i != 255)     # 255: the image's 16-bit PCM, not the bank's
    per = collections.Counter()
    for (o, n), end in zip(z, [o for o, _ in z[1:]] + [total]):
        per[{'KZ': 'Sampled drum kits'}.get(n, n[2:].capitalize())] += end - o
    return per, total


def table(title, rows, total, note=''):
    print(f'\n{title}{note}')
    for k, v in rows:
        print(f'  {v:8,d}  {100 * v / total:5.1f}%  {k}')


def main():
    secs, syms = elf(BUILD + 'vnl-1.elf')
    own, find = owners(), unity_finder()
    by = {'flash': collections.Counter(), 'ram': collections.Counter(), 'pool': collections.Counter(),
          'noinit': collections.Counter()}
    files = collections.defaultdict(collections.Counter)
    where = {'.text': ['flash'], '.ram_text': ['flash'], '.data': ['flash', 'ram'], '.bss': ['ram'],
             '.pool': ['pool'], '.noinit': ['noinit']}
    # by address: named symbols first, then the compiler's merged blocks (.L_MergedGlobals: several statics in one
    # symbol, overlapping the named ones) for the bytes nothing named claims; each byte counted once
    taken = set()
    for n, size, addr, sec in sorted(syms, key=lambda s: (s[0].startswith('.L'), s[2])):
        span = [a for a in range(addr, addr + size) if a not in taken]
        if not span:
            continue
        taken.update(span)
        base = re.sub(r'\.\d+$', '', n)
        o = own.get(n) or own.get(base)
        f = sorted(o)[0] if o and len(o) == 1 else find(base)
        if f == '?' and '.' in base[1:]:      # a function's static (speak.last): the function's file
            f = find(base.split('.')[0])
        for w in where.get(sec, []):
            by[w][f] += len(span)
    size = lambda s: secs.get(s, (0, 0))[1]
    regions = {'flash': size('.text') + size('.ram_text') + size('.data'), 'ram': size('.data') + size('.bss'),
               'pool': size('.pool'), 'noinit': size('.noinit')}
    lim = {'flash': LIMITS['image'], 'ram': LIMITS['ram'], 'pool': LIMITS['pool'], 'noinit': (15696, 15696)}
    titles = {'flash': 'Flash: the firmware image', 'ram': 'SRAM: .data + .bss (globals)',
              'pool': 'SRAM: the pool (big buffers: delay lines, sample voices, song state)',
              'noinit': 'SRAM: .noinit (kept across a reset: crash log)'}
    for w in ('flash', 'ram', 'pool', 'noinit'):
        cats = collections.Counter()
        for f, v in by[w].items():
            cats[category(f)] += v
            files[(w, category(f))][f] += v
        cats['Padding, assembly, constant pools'] = regions[w] - sum(cats.values())
        rows = [(c, cats[c]) for c, _ in CATS + [('Padding, assembly, constant pools', 0)] if cats[c]]
        b, hw = lim[w]
        table(titles[w], rows, regions[w], f'  {regions[w]:,d} B of {b:,d} budget ({b - regions[w]:,d} left)'
              + (f', {hw:,d} hardware' if hw != b else ''))
        if '--files' in sys.argv:
            for c, _ in rows:
                for f, v in files[(w, c)].most_common():
                    print(f'            {v:8,d}  {c[:20]:20s} {f}')
    print(f'\nSRAM: .ram_text (flash code copied to RAM)  {size(".ram_text"):,d} B of {LIMITS["ram_text"][0]:,d}'
          ' (counted in the image above too)')
    print('SRAM: stacks  user 24,320 B, system 7,936 B (fixed: app.ld _ustack_*, _sstack_*)')
    per, total = bank()
    table('Sample bank (its own flash region)', per.most_common(), total,
          f'  {total + 16:,d} B with its 16 B header, of 393,216 ({393216 - total - 16:,d} left)')


if __name__ == '__main__':
    main()
