# FM-1 firmwares that accompany: how the code does it

> Source material, kept as researched (2026-10-23). Goes one level below [`other-firmwares.md`](other-firmwares.md):
> not what each firmware offers, but how its code picks notes and rhythms. General arpeggiators (outside the FM-1
> world) are in `arpeggiators.md`.

## Summary

Two FM-1 firmwares generate a full band with a lead in the sense VNL-1 does: **LOF-1** (a lo-fi band that follows a
player, with a generated "ghost" lead and a call-and-response voice) and **Tuna** (a style-driven composer with a
lo-fi hip hop style). Close behind: **MIN-1** (a self-playing minimalist ensemble: seeded progressions, arp figures,
a chord-tone lead), **Polly** (a QY70-style arranger that re-voices hand-written cells to any chord), **AMB-1** and
its forks **HYP-1**, **CTY-1**, **DUB-1** (chord drift, voice-led pads, bass grooves, an acid line), and **MAKU**'s
phrase-table arp. All of these are GPL-3.0-only (**ideas only** for VNL-1) **except AMB-1, which is MPL-2.0**.

The code that *is* reusable under MPL-2.0:

- **AMB-1** (`ref/fm1-amb/app/dsp/amb.c`, MPL-2.0): `en_voice_lead` (a 4-voice, 24-permutation minimal-motion
  solver), `en_chord_tones`, `en_euclid`, and the bell loops whose notes are scale offsets above the chord degree.
- Tuna's **C port of Marbles' random section** (`tuna_marbles.c`, MIT, bit-exact), including the "deja vu"
  loop-or-wander sequence.
- Mutable Dreams' app (MIT), but it has **no** Yarns, Marbles or Grids sequencer: a 25-line Euclidean-plus-chance
  stepper. Its vendored **Plaits** (MIT) carries `chord_bank.cc`, a chord-shape table.

The best mechanisms found, in one line each:

- **LOF-1 Daydream** (`ghost_phrase`): a lead built from one-beat **rhythm cells** weighted by a density knob, scale
  steps with occasional leaps, chord tones forced on strong beats and on the last note, a **motif memory** (rhythm +
  intervals) replayed with 20% interval nudges, and an explicit **rest at the phrase end "for the answer"**.
- **LOF-1 Answer** (`answer`): call and response that imitates the caller's rhythm and contour (or inverts it),
  re-rooted on a chord tone. It also answers the ghost, so the machine converses with itself.
- **Tuna's melody** (`melody_tick`): a real **period** (question on the 3rd/5th at the half, answer landing on the
  tonic exactly when the progression comes home), an approach note on the beat before, a homing bar, leaps answered
  by a step back, appoggiaturas and suspensions, and motif record/develop/restate with invert and alter odds.
- **Voice-led comping** in both: every inversion tried, the one nearest the previous chord's center kept (LOF-1
  `voicing`, Tuna `pad_voice`), and a **strum/roll** as per-note onset offsets (LOF-1: 4–49 ms per note plus 0–4 ms
  jitter, top note louder; Tuna: `strum_ms` 4–14 per note, low to high).
- **Tuna's counter-melody** (`pad_counter`): a second line in contrary motion under the lead, consonance-scored on
  strong beats, no parallel fifths or octaves. A model for an "answering" or second voice that can't clash.

## License table

| Firmware | License | For VNL-1 |
| --- | --- | --- |
| LOF-1 (`ref/fm1-HYP1/lof1`) | GPL-3.0-only | Ideas only |
| HYP-1, ECO-1, GRAV-1, CTY-1, OPEN-1 (`ref/fm1-HYP1`) | GPL-3.0-only | Ideas only |
| Tuna (`ref/tuna`) | GPL-3.0-only as a whole | Ideas only, **except** `firmware/src/tuna_marbles*.c/.h` (MIT, Marbles port) |
| Mutable Dreams (`ref/mutable-dreams`) | MIT (app; also offered as GPL-3.0), Plaits/Rings/stmlib MIT | Code reusable |
| MAKU, Jangada, Mini-Tone (Felucca forks) | GPL-3.0-only (MIT/Apache ports keep theirs) | Ideas only |
| TRIBE (`ref/fm1-tribe`) | GPL-3.0-only | Ideas only |
| DUB-1 (`ref/fm1-dub`) | GPL-3.0-only | Ideas only |
| AMB-1 (`ref/fm1-amb`) | **MPL-2.0** (since 0.9) | Code reusable |
| fm1_open (M-VAVE's app) | GPL-3.0 with a section-7 linking exception (the arp lives in the binary SDK) | Ideas only |
| Polly (charlesvestal/fm1-polly, cloned to scratchpad) | GPL-3.0-only; drums from the Groove MIDI Dataset (CC BY 4.0) | Ideas; drum data with attribution |
| FoMni (charlesvestal/fm1-fomni, scratchpad) | GPL-3.0-only; rhythm data and waves from Chordian (CC0) | Ideas; CC0 rhythm data |
| MIN-1 (Reytian/fm1-min1, scratchpad) | GPL-3.0-only | Ideas only |

Clones made for this survey (read as data, nothing run):
`/private/tmp/claude-501/-Users-samueloltz-Documents-fm-1/dfd13d60-fb03-409f-86fe-9daa1a019026/scratchpad/fm1-extra/{polly,fomni,min1}`.
Not cloned but listed in `awesome-fm-1` as possibly relevant: ChoralRoot, CoralFlower, UnholyUnion (rolls pad +
sequence + chord arp in a shared key), Felucca 8 Patterns ("generative arpeggiator options"), ETCH ("arpeggiator
riffs"), Melodee/Tess (algorithmic note generators with song form), GammaWave GW-1 (49-genre song generator),
IND-1 (self-arranging songs), REDACTED (a lo-fi groovebox with chance/mutate, zip only). A second round could take
UnholyUnion, ETCH, Melodee and GammaWave.

## LOF-1 (`ref/fm1-HYP1/lof1`)

Everything musical is in one file, `firmware/src/dsp/amb.c` (2,783 lines), running per 16th in `on_step`. The comp,
bass and drums run every step; the lead (player, Daydream ghost) and Answer schedule notes into an event queue
(`ev_note`, `ev_due`) with sample-accurate times, which is how rolls, swing and "lazy" timing are done.

### Harmony and chord following

- **Chords are stacked thirds on the scale's seven-note parent** (`chord_build`): root, 3rd, 5th, 7th, 9th, 11th up
  to the Jazz knob (3–6 tones). From 9ths up, **the 5th is dropped** ("as players do"). Quality is derived from the
  intervals (`chord_quality`): no chord vocabulary table, just diatonic stacking.
- **Two built-in 4-chord progressions per scale** (`PROG`): e.g. Dorian i–IV–i–VII and i–III–IV–VII; major
  ii–V–I–vi and IV–iii–ii–V. One chord a bar. At each cycle end, a 30% chance to switch to the other progression.
- **Following the player** (`follow`): each of the 7 degrees is scored. Held notes add weights by chord function
  (root 1.0, 3rd 0.9, 5th 0.7, 7th 0.6, 9th 0.3, 11th 0.25), a non-chord tone costs −0.55, the lowest held note
  matching the root +0.6. The last 8 played notes count too, decaying `exp(−0.55 × age in beats)`, ×1.4 if on a
  beat. Priors per degree favor I, IV, vi (vii° −0.25), the current chord +0.35 (stickiness), and a change needs to
  win by 0.2. Changes happen only on beats (on 1 and 3 unless two or more keys are held).
- **Push** (`on_step`, step 14): with Push > 0, comp held/stabs, and no player, the next chord is struck an 8th
  early (re-voiced from `next_auto_deg`) with probability Push × 0.6.

### Comp (`voicing`, `comp_strike`, `comp_step`)

- **Voicings**: Close = every inversion built upward from ~F3, the one whose average pitch is nearest the previous
  chord's average (plus half the distance to MIDI 62) wins: cheap voice leading by **centroid**. Open = close with
  the second from the top dropped an octave (drop 2). Shell = root low, 3rd and 7th an octave up. Spread = root, then
  the other tones an octave or two up.
- **The roll** ("like a tired pianist"): notes struck low to high, each `k × (4 ms + 45 ms × Roll)` later, plus a
  random 0–4 ms jitter each. The top note at 1.08 × velocity, the others 0.95 ×: the melody note of the chord
  speaks. Each note panned across the stereo field by Spread.
- **Patterns** (one per preset, a knob): Held (on 1 and on a chord change, length by Length; a second strike on 3
  if Busy > 0.55 with chance Busy); Stabs (2 and 4, plus 16ths 10 and 15 with chance Busy × 0.4, quieter);
  Offbeats (every "and", 2 and 10 louder); Arp (8ths ping-ponging through the current voicing, re-voiced on a chord
  change, every other one softer).
- **Base velocity** `0.5 + 0.12 × rand` per strike; Busy is the only density control.

### Bass (`bass_step`)

All in E1–D#2 (`bass_note`), so the bass never jumps more than an octave. Roots: root on 1 and on a chord change,
again on 3, an octave-up pickup on the last 16th (Busy × 0.5). Root–5th: root on 1, 5th (or 4th below) on 3, a ghost
root on the "a" of 2. **Walking**: quarters; beat 1 root, beats 2–3 the chord's 3rd and 5th (folded within a 6th of
the root), beat 4 an **approach to the next root** (50% chromatic ±1, 50% a scale step either side), using the auto
progression's next chord when nobody plays. Dub: a long root, an octave "pop" on 16th 10 (and 6, Busy × 0.5).

### Daydream: the ghost lead (`ghost_phrase`, called from `bar_start`)

After Wait bars with no key down, at each bar line, a phrase of 1–2 bars:

1. **Rhythm from one-beat cells.** `CELL[10]` holds which 16ths of a beat sound: none, 1, 1+3, 3, 1+4, 1+2+3,
   2+4, 1+3+4, 1+2+4, all four. Two weight vectors (`W_LO` sparse, `W_HI` busy) are blended by Density; the empty
   cell's weight shrinks with Fill. One cell is drawn per beat.
2. **Pitch by scale steps** from the last note: direction random but turned back at the range edge (center ±
   5–15 semitones by Range); with probability Mood × 0.6 a skip (two scale steps). **Beat 1 of every other beat
   pair snaps to the nearest chord tone**; the phrase's **last note is always a chord tone**.
3. **A breath for the answer**: the phrase's last beat rests with probability 1 − 0.9 × Fill, then 0–2 bars of rest
   follow (more when Fill is low).
4. **Motif memory**: the phrase's onset times and intervals are stored; next time, with probability Repeat, the
   motif is replayed from a chord tone near the current note, each interval nudged ±1 step with 20% chance, notes
   on beat starts re-snapped to chord tones. So a hook recurs over different chords, slightly varied.
5. **Performance**: length = gap to the next note × Legato (min 0.3 of a 16th; the last note 2–6 16ths); swing on
   odd 16ths; Lazy adds up to 30 ms random lateness; velocity 0.62 ± (0.075–0.33 by Dynamics), +0.06–0.16 on beats.

### Answer (`phrase_note`, `answer`)

Every lead note (player's *or* ghost's) is logged into a phrase buffer; a gap of more than 4 16ths starts a new
phrase. When a phrase of 2+ notes ends (3 16ths of silence, no key held, on an 8th), Answer replies with chance
0.45 + 0.55 × Chatty:

- **Imitate** (chance Echo): copy the caller's rhythm (its inter-onset gaps) and **intervals, inverted 30% of the
  time**, transposed to start on a chord tone in the answering instrument's register (`ans_centre`: sax 60,
  trumpet/vibes 70, flute/bells 76), each note snapped to the scale.
- **Invent**: 2–6 notes, gaps from `{1,2,2,2,3,4}` 16ths, mostly scale steps (70%) else a third.
- Either way, notes on beats and the last note snap to chord tones; the last note rings 4–8 16ths.

### Drift and drums

Every Drift bars (4–64): next or random drum pattern, the other progression; the bar before gets a fill
(`drift_bar`, `mutate`). Drums: fixed 16-step lanes, velocity ±10% random, hats accented on beats (1.0 / 0.8 / 0.62
for beat / 8th / 16th), ghost hats and ghost snares on 7 and 15 by Busy, fills in bar 4 of each 4 with ratchets.

### Musical vs mechanical

- *Musical*: the roll with the top note louder; centroid voice leading; walking bass approach notes; Daydream's
  chord-tone landings, phrase-end rest and motif recurrence; Answer's imitation of rhythm and contour; ghost and
  Answer conversing.
- *Mechanical*: one comp pattern, fixed for the session, with probabilistic extras only; no comp rhythm variation
  across bars; harmony is diatonic stacking only (no ii–V vocabulary, no color chords); the auto progressions are
  4 bars, one chord a bar, forever; Daydream's phrase has no arc (no question/answer, no climax); the comp doesn't
  thin when the lead is busy (Busy is a global knob); the arp is a plain 8th ping-pong. Never run on hardware.

## Tuna (`ref/tuna`)

`firmware/src/tuna_gen.c` (3,748 lines) is the generator; a style is a `tg_style` record (`tuna_style.h`, data in
`tuna_genres.inc`, `tuna_style.c`, `tuna_manners.inc`). Three pitched parts (melody, bass, "third part"/pad) and
drums, on a tick grid of `division` ticks per beat. Its own RNG streams per concern (`hrng` harmony/melody, `prng`
percussion, `hum_rng` humanization), so turning drums on changes no note elsewhere.

### The LO-FI HIP HOP style (`tuna_genres.inc`, `TG_STYLE_G_LOFI_HIP_HOP`)

Eb major, 78 BPM, 16ths, 8-bar periods, one chord a bar. Harmony: jazz vocabulary, `cycle {3,2,5,0}` =
**IVmaj7 – iii7 – ii9 – Imaj7** (the descending 4-3-2-1), `cycle2 {1,4,6,0}` = **vi7 – ii7 – V9 – Imaj7**, switching
every 2 periods. (The older survey misread these as scale degrees.) Melody: density 0.32, base G4, range 14,
step profile `{0.15, 0.6, 0.85, 0.96}` (thresholds for repeat / step / 3rd / 4th / leap), max leap 4 steps,
syncopation 0.5, appoggiatura 15%, motif 2 beats reused 70%, invert 10%, alter 30%, staccato 0.1. Swing 55% on
16ths. Pad: block chords, 4 tones from G3. Bass: ostinato `bass_rhythm 0x0401` (16ths 0 and 10) with notes
`{0, 7}` in semitones: root on 1, fifth on the "and" of 3. Orchestration: one of four trios (upright + flugelhorn
+ EP, fretless + vibes + reed EP, upright + kalimba + electric guitar, e-bass + shakuhachi + reed EP).

### Melody (`melody_tick`, `motif_segment`, `period_of`)

The most composed lead in any FM-1 firmware:

- **Period form.** The melody's period spans whole cycles of the progression, anchored so the **arrival lands on
  the tonic exactly when the harmony returns home**. At the arrival: the tonic (or, after an "open" ending, an upper
  tone of the new chord, not its root), held for `arrival_bars`, then a breath. **The beat before**: a step
  neighbor of the target (2nd above or leading tone below, whichever is nearer). **The last bar** ("homing"):
  steps toward the target only. **The half** (the question): rest on the chord's 3rd or 5th, not the tonic, held
  two beats.
- **Free notes.** Play/rest per tick from a Marbles deja-vu sequence: downbeats always, other beats with
  `(0.25 + 0.75 × density)(1 − 0.6 × syncopation)`, off-beats with `density × (1 + syncopation)`. Interval size
  from the profile thresholds; **a leap of a 3rd or more is answered by a step back** the other way; octave leaps
  rare. Downbeats snap to chord tones (moving on if that would repeat), then maybe an **appoggiatura** (lean on the
  step above, resolve half a beat later) or a **turn**. At a chord change, a non-chord melody note whose step below
  is a chord tone becomes a **suspension** resolving a beat later.
- **Motifs.** The first `motif_beats` of a period are recorded (rhythm + scale-step intervals); later segments
  replay it with chance `motif_use`, **inverted** or with the **last interval altered** by the variation odds,
  starting on the chord tone nearest the current note (a sequence over the new harmony). The segment after the half
  **restates it literally** at the original pitch. A new motif is recorded at each period start unless `repeat`
  keeps the old one.
- **Chaos knobs** (rhythm, pitch, density) move these thresholds around the style's values; 0 = exactly the style.

### Accompaniment figures

- **Pad voicing** (`pad_voice`): every inversion tried, cost = sum of per-voice movement from the last chord (or
  distance from the register center on the first); **tones common to both chords are held, not re-struck**; only
  new tones sound. Options: spread (open: root, 5th, 10th...), power chords, a polychord triad on top. **`strum_ms`**
  (4–14 ms per note in the strummed styles) delays each new note low to high.
- **Pad modes** (`pad_mode_tick`, `pad_follow`, and per-mode functions): block, rhythm (chord on a 32-bit mask over
  the groove's two bars, gated: skanks, stabs, chops), oom-pah, afterbeat, Alberti (`{0,2,1,2}`), broken chord
  (Chopin ladder `{0,2,3,4,5,4}`), arpeggio (16ths up and down a 2-octave chord ladder), tremolo, figure and
  additive figure (a pattern of chord-ladder indices, lengthened and shortened by one every few reps:
  Glass-style), **counter-melody**, parallel (Debussy), doubling (a 3rd or 6th below the melody), organum,
  tintinnabuli, canon (the melody delayed and transposed in-scale), phase (Reich).
- **Counter-melody** (`pad_counter`): 8ths, starting a 6th under the melody, moving **against** the melody's last
  direction by scale steps. On beats it searches chord tones within ±4 semitones, kept 3+ semitones under the
  melody, scored 3rds/6ths 0, unisons/5ths 2, 4ths 4, others 8, **+20 for a parallel 5th or octave**, plus distance.
  Off beats: avoid parallels or hold (oblique motion).
- **Bass** (`bass_mode_tick`, `bass_walk`, `bass_play`, `bass_808`): ostinato (rhythm mask + note list, as
  semitones over the root or chord-ladder indices), walking (root on the change, **scale steps toward the next
  root**, a chord tone mid-chord, the next root's neighbor on the last beat), march (root/5th), pedal, drone, toll,
  808 (long notes on the groove's kicks, the new note on before the old one off, so a glide synth slides), and a
  probabilistic one (root 40%, 5th 25%, octave 15%, 3rd 20%, approach step into the next chord's root on the last
  beat). `bass_root_note` keeps the root in the octave nearest the previous bass note.

### Humanization and form

- `hum_collect`: each note-on late by `r² × HUMAN_MS[part] × humanize` (bass 6 ms, melody 10, pad 8, drums 3), the
  square making most offsets tiny; each chord note gets its own delay (a natural micro-spread); velocity ±8; the
  note-off keeps the note-on's delay so lengths stay as written.
- `evolve_step`: a **sine energy arc over N periods** scales melody density (±50%), velocity (±12) and percussion
  weights, with an optional modulation to the dominant/relative and back, or up a step for the last chorus.
- `perc_groove`: two-bar lanes with accent and ghost masks, hat rolls, a snare build into each arrival and a crash
  on it.

### Musical vs mechanical

- *Musical*: period form tied to the harmony; approach notes and homing; leap recovery; suspensions and
  appoggiaturas; motif development with literal restatement after the half; voice-led, common-tone-holding pads;
  the 4-3-2-1 lo-fi progression with a vi–ii–V–I B side; the energy arc.
- *Mechanical for lo-fi*: the lo-fi pad is plain block chords struck on each change (no comp rhythm, no roll:
  `strum_ms` is 0 in that style); the bass is a fixed 2-hit ostinato; the melody is one line through the whole
  piece (no sections where it lays out); classical devices (turns, trills) are off for lo-fi but the phrase logic is
  still "classical period", which can sound too resolved for a loop-based genre.

## Mutable Dreams (`ref/mutable-dreams/src/Mutable-Dreams`)

MIT. **No Yarns, Marbles or Grids code**: `app/third_party/` holds only Plaits, Rings and stmlib DSP. The sequencer
is `tick` in `app/dsp/dreams.cpp` (~25 lines): six tracks, each a Euclidean rhythm (`md_hit`:
`((s + r) % l) × h % l < h`), hits offset by macro 1, a per-step probability, ratchets, accent on the first step of
the cycle. Pitched tracks add, with chance macro 2, a random interval from `{0,2,3,5,7,10,12}` to the track's note
and quantize to the scale (`md_quantize`, 8 scales as 12-bit masks). The seed re-rolls every 32 steps unless
locked; Mutate re-seeds and re-rotates each track; Return undoes. The "choir" model plays a fixed triad shape
(`{0,3,7}`, `{0,4,7}`, `{0,5,7}`, `{0,7,12}`) quantized to the scale.

Reusable but small: the seed-lock-or-reroll-every-N-steps idea, and `md_hit`/`md_quantize` (trivial). The vendored
`plaits/dsp/chords/chord_bank.cc` (MIT, with Jon Butler's alternative table) is a list of chord shapes for Plaits'
chord engine, not a comping algorithm.

## AMB-1 (`ref/fm1-amb`): the reusable one

MPL-2.0 file by file since 0.9 (`LICENSING.md`). Ambient, but its harmony and pad code is exactly comping plumbing.
All in `app/dsp/amb.c`:

- `en_chord_tones(pool, root, deg, pcs)`: stacked thirds from a degree of any pitch-class pool (`OFF = {0,2,4,6,1,3,5}`),
  4 tones for 6–7-note scales, 3 otherwise.
- `en_voice_lead(from, pcs, nt, to, lo, hi)`: tries all 24 assignments of the chord's tones to 4 voices, each voice
  placed nearest its previous note (within a tritone, folded into lo..hi), cost = total motion + 40 per unison. ~40
  lines, no allocation. VNL-1's own voicer is approved, so this is a reference, not a replacement.
- `chord_change`: a weighted walk over degrees (`W7 = {4,2,1,3,3,3,0.4}` for I..vii°, never the same degree), on the
  next bar; `pad_leave` holds common tones.
- **Bell loops** (`rand_offset`, `loop_pass`): 4 loops of 13, 17, 23, 29 quarters (they phase). Each slot stores a
  *scale offset above the chord's degree*, so a stored melody follows chord changes by itself. Offsets: a chord tone
  72% (an octave up 30% of those), else a passing 2nd/4th/6th. At each pass end the loop grows or prunes toward
  Density, Evolve replaces or moves a note, or the loop rests a pass. Onsets up to ~60 ms late, velocity 50–99.
- `en_euclid(k, n, rot)` (Bjorklund) and a slow **arc** (a target in −1..1 retargeted every 50–150 s, followed with
  a 35 s lag) that scales density and rests.

Mechanical for hip hop (quarter-note bells, no syncopated comp), but "notes stored as offsets above the chord degree"
is the cleanest way to make a stored lead phrase or arp follow the harmony.

## The others

- **HYP-1** (`ref/fm1-HYP1/firmware/src/dsp/amb.c`, GPL): `en_chord_kind` voicing kinds: thirds, **9ths without the
  5th** (1-3-7-9), sus, fixed dub m9 `{0,3,10,14}` and quartal `{0,5,10,15}`. Acid line `ac_roll`: gate by density
  (+0.3 on step 0), a chord tone 80% of the time, accent 30%, slides; `ac_mutate` re-rolls each step with a small
  chance at the loop end. A hat on the same 16th accents the acid note. Drum drift (`dd_mutate`): "shift" moves one
  or two hits one step but **never the kick or clap**; "morph" adds/removes a hit.
- **CTY-1** (`cty1/firmware/src/dsp/amb.c`, GPL): six degree progressions (`PROGS[6][4]`) and six bass styles in a
  few lines each (`bass_biome`): pedal, long, **sync (16ths 0, 6, 10, 14, the 5th on 10)**, root-5th, walk
  (`{0,2,4,5}` degrees on beats), standard. Arps `{0,2,4,2,4}` chord tones.
- **DUB-1** (`ref/fm1-dub/firmware/src/dsp/amb.c`, GPL): **chord memory** (`en_voicing`): one shape moved whole to
  each root (open 7th: 1-5-7-10; 9th without 5th: 1-7-10-14), root kept in MIDI 42–53, Evolve swaps the shape 10%.
  `chord_change` goes "home" 65% when away and rejects diminished-fifth roots. **Bass grooves as bitmasks**:
  `GROOVE = {0x0001, 0x4444, 0x0909, 0x4949, 0x0481}` (drone, offbeat, dotted, roller, sync) with gates
  `{15, 1.2, 2.2, 1.5, 2.6}` 16ths. Euclidean off-beat stabs (1–6 a bar, rotation 2). 8-bar phrases Full / Break /
  Groove / Thin with a fill before the kick returns.
- **MAKU** (`ref/maku/firmware/src/maku.c`, `maku_arp_step`, GPL): the most interesting **arp lead**. A hand-written
  32-place phrase in scale degrees with rests (`{11,R,14,11,9,R,7,9,11,14,16,R,...}`) played in **runs of 3–8 steps
  from a random entry point**, then a jump elsewhere (shorter runs when dense); the arp's track is 29 steps long with
  steps 24–28 silent ("the breath"), so it phases against the 16-step kick. "Loose" re-rolls degrees; Cascade stacks
  octaves and a diatonic 3rd; Scramble ratchets. **The player's notes melt in**: a fresh key sounds on the next arp
  step once; afterwards, 35% of steps draw from the last 8 played notes in proportion to decaying weights, avoid
  notes fading twice as fast.
- **MIN-1** (`fm1-extra/min1/firmware/src/dsp/min.c`, GPL): `en_cycle_roll` builds 2–6-chord progressions by weighted
  root motion (4ths/5ths ×1.4, 3rds/6ths ×1.3, steps ×0.9, tritone ×0.2; the last chord ×1.5 if it leads home); the
  bass takes the chord's 3rd 30% of the time when that walks by step. `fig_roll` makes arp figures (up, down, zigzag,
  bounce, 25% leaps, never a repeated note) from chord-tone indices, alongside a library (`FIGS`: broken
  `{0,2,1,3}`, Alberti, rocking, wave, spread); an **additive process** grows a figure from 2 notes to full length
  and back. `lead_compose`: per chord 0/1/2/4 notes by Activity, staying on a common tone (30%) or stepping to the
  next chord tone in the current direction (reversing 20%), tied across chords, seeded so it repeats each cycle.
  (`cycle_vary` substitutes relative/mediant chords: violates VNL-1's no-substitution rule.)
- **Polly** (`fm1-extra/polly`, GPL): an arranger, not a generator, but its **transposer** (`app/dsp/xpose.c`) is the
  best authoring model found. Cells are written over Cmaj7; each source pitch class maps to a degree (R, 9, 3, 11,
  5, 13, 7) plus chromatic offset and is re-mapped to the target chord's degree table (26 chord types). Track types:
  parallel, chord-to-chord, chord-to-chord **nearest the last note** (smooth pads), bass (first note after a change
  snaps to the root). Its hip hop style (`styles/09-hiphop.sty`, 90 BPM): rootless 3-5-7 Rhodes **laid back 40/480
  of a beat**, a pushed second hit; bass locked to the kick; drums from the Groove MIDI Dataset (CC BY 4.0).
- **FoMni** (`fm1-extra/fomni`, GPL; data CC0): the strum is the player's. Auto rhythms are **per-step strings**
  (`dsp/om_data.h`, `OM_RHYTHM`): bass and chord envelope commands (`T`rigger, `R`elease, `O`ne-shot, `N` cut) plus
  the bass note per step (`R`, `5`, `3`), e.g. chord `"----O-O-----O-------O-O-----O---"`. A compact, readable
  authoring format, and the data is CC0.
- **TRIBE** (`ref/fm1-tribe`, GPL): TB-3PO (`firmware/src/seq/tb3po.c`): uniform scale degrees, root 35% on beats,
  density/accent/slide, 25%-per-step mutation every N bars. Mechanical (no chord following). Its **break generator**
  (`dsp/breaks.c`, from BB Gen): an 8-slice loop walked by weighted jumps and retriggers, a fill in a phrase's last
  bar. Relevant to dusty drums, not to accompaniment.
- **Jangada** (GPL): VIVO (`firmware/src/seq.c`, `vv_decide`) plays a written pattern's *next note* on each hit of a
  seeded Euclidean/probability rhythm (with a memory term: bursts or alternation), so **rhythm is decoupled from note
  order**. Plus Felucca's standard arp modes.
- **Mini-Tone** (GPL): Felucca's player arp (UP, DN, UPDN, RND, ORD, CONV, DIVG, PINKY, THUMB, WALK, CHORD...,
  with probability, gate, swing) and one-key chords (`chord.c`). Not generative.
- **fm1_open** (M-VAVE): the stock arp (up, down, up-down, random; 1/4–1/32; 1–4 octaves) is inside the binary SDK.
  Per-step chords from an interval table. No auto-accompaniment.
- **ECO-1, GRAV-1**: phrases from a leaf's random walk / an orbit, a note or two changed per cycle. Ambient only.
  **OPEN-1**: no code.

**Nobody strums or rolls automatically except LOF-1's roll and Tuna's `strum_ms`.** No firmware does guitar up/down
strokes.

## What VNL-1 could take

### (a) A sophisticated arpeggiator lead to replace the hybrid

VNL-1's hybrid (`app/song/lead.c`: `plan_arp`, `plan_hybrid`) already has a voice-leading skeleton, six landing
types, a per-song pattern/rhythm/direction/swap, and motion thinning. What it lacks, compared with LOF-1 and Tuna:

1. **Rhythm cells drawn per beat** (LOF-1's `CELL` + `W_LO`/`W_HI`) instead of one fixed rhythm per song: the same
   arp shape over a varying but density-controlled rhythm. One density value per section gives intro-to-chorus
   growth for free.
2. **Phrase shape.** Tuna's question/answer: the half-phrase rests on the 3rd or 5th, the end lands on a stable tone
   via a step neighbor the beat before, the last bar only steps. For an arp: the arp's *top note* traces this
   contour, the arp fills below it.
3. **Motif memory with nudges** (LOF-1 Repeat, Tuna motif): record the first 1–2 bars' onsets and pattern indices,
   replay later with 20% index nudges, inversion 10%, last-note alteration 30%, and a literal restatement at the
   phrase's midpoint. This is what makes an arp sound like a *part* rather than a texture.
4. **A breath at the phrase end** (LOF-1 `rest_last`): the last beat of each 4-bar phrase empty, the hook for an
   answering voice or ear candy.
5. **Leap recovery** (Tuna): after a skip in the arp, the next move goes back by step.
6. **Deja vu** (Tuna's MIT Marbles port, `tm_seq_next_value`): one knob between "loop the last N choices" and "always
   new". Feeding the arp's per-note keep/skip and index choices through it gives loop-like repetition with slow
   mutation, exactly the lo-fi idiom. **Directly reusable** (MIT); VNL-1 would need it on its own seeded RNG stream
   to stay a pure function of the roll.

### (b) Guitar-strumming comping (logged for later)

- Onset spread per note (LOF-1: 4–49 ms per note, jitter 0–4 ms; Tuna: 4–14 ms), top note louder; for a guitar,
  add **direction**: downstrokes low-to-high on beats, upstrokes high-to-low and quieter on "ands", the upstroke
  hitting only the top 3–4 strings. Neither firmware does up/down strokes; that's VNL-1's to add.
- **Hold common tones, re-strike only new ones** (Tuna `pad_voice`) does not suit a strum (a guitarist re-strikes
  everything) but suits a Rhodes or pad.
- Rhythm from a 2-bar mask (Tuna `pad_rhythm`/`pad_gate`) with a gate per strike: a strum pattern is a mask plus a
  direction bit per hit.

### (c) Comping and bass variety

- **Push** (LOF-1): the next chord an 8th early on the last 16th pair of the bar, by chance; VNL-1 already does
  pushes in the lead; the comp could share the same per-bar decision so they push together.
- **Centroid voice leading** (LOF-1) is a cheap fallback; VNL-1's voicer is already better (approved). Skip.
- **Bass patterns as data** (Tuna ostinato: a 16-bit rhythm mask + a note list in semitones or chord-ladder steps):
  a compact way to roll many bass figures per song. LOF-1's four (roots, root–5th, walking with approach, dub with
  octave pop) are a good starting set; the walking approach (50% chromatic, 50% scale step) and Tuna's
  "neighbor of the next root on the last beat" are the musical core.
- **Counter-melody** (Tuna `pad_counter`) as a second voice in later sections: contrary motion, consonance-scored,
  no parallels. Pairs with VNL-1's no-rub rule.
- **Energy arc** (Tuna `evolve_step`): one sine over the song scaling density and velocity of every part; VNL-1's
  section plan could expose the same scalar to comp thinning and lead density.

## Top 5 takeaways for VNL-1 (ranked)

1. **Give the lead a phrase, not a pattern.** Tuna's period logic as the lead's spine: question at the half on the
   3rd/5th, a step neighbor the beat before the arrival, step-only homing bar, arrival on a stable tone as the
   progression comes home, a breath after. Keep the hybrid's arp as the fill *under* that contour line. (Ideas;
   GPL.)
2. **Motif memory with controlled variation.** Record the lead's first 1–2 bars (onsets + pattern indices or scale
   steps), replay with small nudges (LOF-1: 20% ±1 step; Tuna: invert 10%, alter the last interval 30%), restate
   literally at the midpoint, re-anchored on a chord tone of the current chord. Optionally drive the choices with the
   **MIT Marbles deja-vu sequence** from Tuna (`tm_seq_next_value`, reusable as is) for loop-vs-wander. This is the
   single biggest step from "arpeggiator" to "part".
3. **Per-beat rhythm cells weighted by density** (LOF-1 `CELL`, `W_LO`/`W_HI`) plus MAKU-style **runs from random
   entry points in a written degree table**: the arp's rhythm and its note order vary independently (Jangada's
   point), while a per-section density value grows or thins it, and the comping reads that density to make room.
4. **A second voice that answers** (LOF-1 Answer: imitate the phrase's rhythm and contour, inverted 30%, in its
   instrument's register, in the lead's breath), with Tuna's counter-melody scoring (3rds/6ths preferred, no
   parallel 5ths/octaves, contrary motion) as the guard. Fits later sections and VNL-1's no-rub rule.
5. **Comping and bass variety as data.** Bass figures as a 16-bit rhythm mask + a note list (DUB-1 grooves, Tuna
   ostinato, CTY-1 sync), walking with an approach into the next root (LOF-1, Tuna); comp/strum cells authored once
   and re-voiced per chord through a degree map (Polly), or as FoMni-style step strings (CC0 data). For the guitar
   strum: per-note onset spread (4–14 ms, Tuna; up to 49 ms, LOF-1) with the top note louder, plus the up/down
   stroke direction no firmware has yet.
