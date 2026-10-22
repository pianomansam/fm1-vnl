# How AMB-1 works, and how VNL-1's songs fit on it

> Source material, kept as researched. The distilled, current reading (general musicianship vs. lo-fi choices) is in [`docs/music/`](../music/README.md).

Source read: `ref/fm1-amb/app/dsp/amb.h`, `amb.c` (the engine, ~1,800 lines), `ui.c`, and `ref/fm1-dub/PLAN.md`
(DUB-1, a fork of AMB-1 that already turned it toward a beat-driven genre).

## AMB-1 in one paragraph

AMB-1 is a generative ambient "garden" that never ends: it plays forever, slowly changing. Everything lives in one
C file (`amb.c`) that runs inside the audio interrupt. The UI never touches sound directly: it posts commands
(`en_set`, `en_next`, `en_regrow`…) that the audio code picks up at the start of each block. A random number
generator (xorshift) makes every musical choice.

## Its parts

| Part | What it does | Lo-fi relevance |
| --- | --- | --- |
| **Harmony** | A scale (mode + root) and one chord that drifts through it every 20 s to 5 min, preferring near, strong moves. Voice leading keeps shared notes and moves others as little as possible. Names chords for the screen ("Em7"). | High. Reuse voice leading and chord naming; replace "drift" with fixed progressions of 2–4 chords that loop every 4–8 bars. |
| **Pad** | Four FM Rhodes (or formant "voices") that swell in and out on unequal cycles. | The FM Rhodes is a lo-fi staple. Change from breathing swells to played, rhythmic chord comping. |
| **Bells** | Four tape loops of unequal length (13/17/23/29 beats), each a short phrase of FM bells. | Possibly a melody/texture layer (music box, vibes), but lo-fi melodies are phrase-based, not phasing. |
| **Bass** | Sine + sub on the chord root, gliding, sometimes the fifth. | Keep, add rhythm (boom-bap bass follows the kick). |
| **Pulse** | Thump, closed hat, open hat, each on a Euclidean pattern, with swing. | Replace with boom-bap patterns: kick/snare backbeat, swung hats, ghost notes. Swing already exists. |
| **Arc** | A slow, minutes-long tide from sparse to full that thins bells, darkens tone, quiets drums. | This is the closest thing to "song structure", and the part VNL-1 rewrites most. |
| **Rests** | With "Evolve", drums sit out a few bars, bass skips a chord, a bell loop skips a pass. | Exactly the "parts drop out" move, but random rather than structured. |
| **Space** | Echoplex delay, tape loop echo, Dattorro plate reverb. | Keep the tape and reverb; lo-fi needs less space and more grit (see the lo-fi research). |
| **Pieces** | Eight presets ("recipes") that set every parameter. | Becomes the per-song roll: instead of picking a fixed recipe, VNL-1 generates one. |

## DUB-1: AMB-1 with a bar grid and phrases

Source: `ref/fm1-dub/firmware/src/dsp/amb.c` (~3,100 lines), `amb.h`, `PLAN.md`. Same engine skeleton as AMB-1
(commands into an audio-interrupt render, xorshift dice, recipes), but everything is locked to **bars**.

**Licensing catch:** DUB-1 is **GPL-3.0-only**. It forked AMB-1 0.9, before AMB-1 moved to fm1-base and MPL-2.0,
and it still carries its own copy of the old platform (`firmware/src/usb.c`, `lcd.c`…) instead of fm1-base.
Copying its code would make VNL-1 GPL. Studying its ideas and reimplementing them is fine; decide the licence
before borrowing any code.

### Its structure

| Mechanism | How it works |
| --- | --- |
| **Phrases** | Always 8 bars. Each phrase is one of four sections: **Full** (everything), **Break** (kick, bass, hats out; filter dark, then opens from bar 3), **Groove** (every other stab, drums forward), **Thin** (no bass, no shaker). |
| **Choosing the next phrase** | On bar 8, a weighted dice roll (a Markov chain): from Full, stay Full unless Motion says leave (then 45% Break, 30% Groove, 25% Thin); from anything else, 75% back to Full. |
| **Fills** | The last bar before the kick returns: kick drops out halfway, last stab thrown into the echo. Always after a Break; sometimes otherwise. |
| **Filter "arc"** | Each section sets a target brightness and a glide time (0.7–4 bars). Breaks go dark then sweep open: the classic build. |
| **Harmony** | Two modes: generated chords from the scale with a fixed voicing shape, or a **written progression** of up to N chord slots (root, quality, extension: 6, 7, maj7, 9, 11, add9). The chord changes every *Drift* bars (2–64) or holds. |
| **Drums** | Five lanes (kick, shaker, open hat, rim, clap). Generated (Euclidean) or a **pattern page** of 16 steps with per-lane chance and accents. |
| **Layer switches** | Per-section on/off flags (`lay_kick`, `lay_bass`…) gate each layer on the bar. |
| **Live control** | PERFORM: mutes, solos, Drop (kick+bass out while held), a bipolar DJ filter, and macro knobs (Dub, Energy, Space). Energy can force the next phrase Thin or Full. |
| **Sound** | Chord stab (detuned saws or 4-op FM keys with algorithms), FM pings, five bass grooves, tape echo with saturating feedback, granular cloud, plate reverb, vinyl crackle. |

### How it compares to AMB-1 and to VNL-1's songs

| | AMB-1 | DUB-1 | VNL-1 (planned) |
| --- | --- | --- | --- |
| Time | Free-floating, minutes-long tides | 8-bar phrase grid | Bar grid, sections of 4/8 bars |
| Structure | None (one continuous arc) | Local: each phrase picks the next, forever | Global: a whole song planned up front (intro → build → drops → outro) |
| Ends? | Never | Never | Yes, ~5 min, then the next song |
| Harmony | Drifting chord | Drifting chord or a written progression | A progression rolled per song |
| Variety over time | Slow random drift | Random phrase order | Each song deliberately different from the last |

DUB-1 is the right model for the **inside of a section** (layer flags, filter targets, fills, phrase-length
grid) and for the harmony (chord slots with extensions are exactly lo-fi's vocabulary). It has no idea of a song:
its Markov chain only looks one phrase ahead, so it can't do "start minimal, build, peak, strip back, end". VNL-1
replaces the Markov chain with a **song plan**: the whole section list rolled at the start, then played in order.

## What VNL-1 adds: songs

AMB-1 is a stream. VNL-1 is a playlist. The new piece is a layer **above** the engine:

1. **A song generator** runs once per song (on the main loop, not in the audio interrupt). It rolls:
   tempo, key, progression, instruments (which EP/bass/drum kit flavor), effect amounts, and an arrangement:
   a list of sections, each a multiple of 4 bars, each with the set of layers that play in it.
2. **The engine** plays one section at a time, the way AMB-1 plays a piece, but follows the song's plan instead
   of the drifting arc. Inside a section, small dice still roll (ghost notes, fills, a dropped bar).
3. **Song memory** keeps the last song's (or last few songs') choices and steers the next roll away from them:
   a different key, tempo bucket, lead instrument, intro style, etc. Simple version: reroll anything that
   matches the previous song, or weight choices by how recently they were used.
4. **The transition**: a song ends (fade, tape stop, or drop to vinyl noise), a short gap, and the next begins.

Length: at ~80 BPM in 4/4, a bar is 3 s, so 5 minutes is ~100 bars, about 24 sections of 4 bars. Real lo-fi
tracks are often shorter (see the lo-fi research), which is a design choice to settle.

## Reused vs. new

- **Reuse from AMB-1:** the engine skeleton (commands, render, parameters, pieces), FM Rhodes, voice leading,
  bass, swing, tape echo, reverb, xorshift RNG, the UI and screen reader plumbing, tests in the simulator.
- **Learn from DUB-1 (ideas, not code, unless we go GPL):** vinyl crackle, layer flags per section, filter
  sweeps tied to sections, drum fills, chord slots with extensions, pattern lanes with per-step chance.
- **New:** the song generator and arrangement plan, song-to-song memory, boom-bap drums, lo-fi coloring
  (wow/flutter, bit reduction, low-pass), progression-based harmony, endings and transitions.

## Open questions

- Is the player hands-off (a radio) or do they steer (skip song, lock key, favorite a song)?
- Fixed 5 minutes, or a range around it?
- Should a song be reproducible from a seed (so a good one can be saved and replayed)? Cheap to do if decided early.
- Licensing: AMB-1 is MPL-2.0, so we can copy and change its files, but changes to *those files* stay MPL-2.0.
