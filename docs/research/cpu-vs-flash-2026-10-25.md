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

| Pick (Sam) | Flash | CPU (half rate unless noted) |
|---|---|---|
| Knight upright piano (VCSL, CC0) | 38 KB | about 0.15 (a synth Rhodes or felt piano is about 0.27); an added option, the electric pianos stay |
| FreePats nylon (CC0), strum engine | 31-49 KB | about 0.17; **replaces** the Karplus-Strong nylon (about 0.21), Sam's explicit swap |
| FluidR3 solo cello (MIT) | 28 KB | about 0.05 (synth leads are 0.12-0.24); an added lead option |
| Karoryfer Emilyguitar (CC0) | 51 KB | comping about 0.13, strumming about 0.18 (half rate): new |
| Sampled drum pool, 4 kits (CC0) | 42-64 KB | 0.13-0.23 (synth kits are 0.11-0.13); added kits, the synth kits stay |
| Kawai grand: a second acoustic piano for any piano part (chords, stabs, arps, broken chords) | 38-51 KB | about 0.15, the same as the Knight |
| Synth string-ensemble lead (kept as synth) | 0 KB | about 0.10 at half rate |

Total: about 190-230 KB without the Kawai, 230-280 KB with it, against about 215 KB free.

**Samples are added, not swapped in (Sam, 2026-10-25).** Unless Sam names a replacement (so far only the nylon), each
sample is a new option and the synths stay. So samples don't lower the heaviest song: today's (about 0.97, a synth
Rhodes song) stays in the budget. A sampled song is cheaper than that one, except possibly a song with a sampled kit
over heavy keys (sampled drums cost up to about 0.1 more than synth drums); the bench must re-pick the heaviest roll
once sampled sets exist.

**Flash is the constraint.** We are over budget with the Kawai or the long versions of the nylon and drums.

## Proposal

Trim before swapping anything back to a synth:

1. **Shorten samples where it isn't audible:**
   - nylon 0.8 → 0.5 s a note (49 → 31 KB);
   - drum rides and open hats shorter (pool 64 → about 42 KB);
   - low zones at a lower sample rate.

   That gives about **190 KB**, with about 25 KB spare.
2. **Two acoustic pianos, both general (Sam):** the Knight (soft upright) and the Kawai (brighter grand) can each play
   any acoustic-piano part. The Kawai is about 38 KB at 6 zones; to make room, the cello goes to the low string synth
   (reuse, 28 KB back), which gives about 200 KB of 215 with both pianos.
3. **Synth fallbacks if flash still runs out** (the synth already exists, so dropping the sample loses an option, not
   the instrument), in this order:
   - **Cello to the string ensemble synth in a low register** (reuse; demos 2c-strings-synth-low*): frees 28 KB and
     costs about 0.1 CPU in cello songs only. It's the one sample Sam called "good" rather than clearly better.
   - **Fewer drum kits, or hybrid kits:** synth kicks (they synthesize well) under sampled snares and hats (the snare
     carries a kit's character).
   - **Keep sampled:** the nylon, the Emilyguitar, the Knight and the Kawai, each of which Sam heard as clearly better than its
     synth.

## Open: the real flash size

The 400 KB is the budget in `tools/mem-check.sh`, not a confirmed FM-1 limit. If the device has more room, most cuts
above aren't needed. Confirm it (Plan 2, hardware) before cutting anything Sam likes.
