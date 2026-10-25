# CPU vs flash: samples vs synths

Status: working notes, 2026-10-25, after Sam's picks from the instrument spike (spike/instruments). Numbers are the
host proxy (docs/cost.md) and the spike's packer (16 kHz mono 4-bit IMA ADPCM); the FM-1 bench replaces them.

## The rule

The two budgets are spent differently:

- **CPU** must fit the **heaviest single song**. A song plays one keys sound, one lead, one bass and one kit at a time,
  so CPU is the worst combination, not the sum of the library.
- **Flash** must hold **every sample in the library at once**. It is the sum.

Samples and synths sit on opposite sides of that:

- **A sample** is cheap in CPU (about 0.03 a voice at half rate, 0.06-0.07 at full rate) but adds flash for every
  instrument (about 6.4 KB per key zone).
- **A synth** costs CPU (0.10-0.28 for keys or a lead) but almost no flash (a few hundred bytes of code).

So the risk with samples is running out of flash; the CPU budget barely moves.

## The numbers

Flash: the image is 194 KB of a 400 KB budget (`tools/mem-check.sh`), so about **215 KB** is left for samples.

| Pick (Sam) | Flash | CPU vs the synth it replaces |
|---|---|---|
| Knight upright piano (VCSL, CC0) | 38 KB | about 0.15 at half rate vs a synth Rhodes or felt piano at about 0.27: **saves about 0.1** |
| FreePats nylon (CC0), strum engine | 31-49 KB | about 0.17 vs about 0.21 (Karplus-Strong): about even |
| FluidR3 solo cello (MIT) | 28 KB | about 0.05 vs a synth lead at 0.12-0.24: **saves CPU** |
| Karoryfer Emilyguitar (CC0) | 51 KB | comping about 0.13, strumming about 0.18 (half rate): new |
| Sampled drum pool, 4 kits (CC0) | 42-64 KB | 0.13-0.23 vs synth kits at 0.11-0.13: **costs up to +0.1** |
| Kawai grand for stabs (pending Sam's listen) | 38-51 KB | about 0.15, the same as the Knight |
| Synth string-ensemble lead (kept as synth) | 0 KB | about 0.10 at half rate |

Total: about 190-230 KB without the Kawai, 230-280 KB with it, against about 215 KB free.

**CPU is fine.** Today's heaviest song (about 0.97) is a synth Rhodes song. Sampled keys and leads are cheaper than the
synths they replace, so the heaviest song should get lighter. Sampled drums are the one sample that costs more CPU;
the savings elsewhere cover it.

**Flash is the constraint.** We are over budget with the Kawai or the long versions of the nylon and drums.

## Proposal

Trim before swapping anything back to a synth:

1. **Shorten samples where it isn't audible:**
   - nylon 0.8 → 0.5 s a note (49 → 31 KB);
   - drum rides and open hats shorter (pool 64 → about 42 KB);
   - low zones at a lower sample rate.

   That gives about **190 KB**, with about 25 KB spare.
2. **One piano for held chords and stabs.** A second piano is about 45 KB on its own: the first thing that doesn't
   fit unless the Kawai stab demo clearly wins.
3. **Synth fallbacks if flash still runs out,** in this order:
   - **Cello back to the synth cello:** frees 28 KB and costs about 0.05-0.1 CPU in cello songs only. It's the one
     sample Sam called "good" rather than clearly better.
   - **Fewer drum kits, or hybrid kits:** synth kicks (they synthesize well) under sampled snares and hats (the snare
     carries a kit's character).
   - **Keep sampled:** the nylon, the Emilyguitar and the Knight, each of which Sam heard as clearly better than its
     synth.

## Open: the real flash size

The 400 KB is the budget in `tools/mem-check.sh`, not a confirmed FM-1 limit. If the device has more room, most cuts
above aren't needed. Confirm it (Plan 2, hardware) before cutting anything Sam likes.
