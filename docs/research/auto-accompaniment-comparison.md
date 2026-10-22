# Auto-accompaniment engines vs. VNL-1

> Source material, kept as researched. The distilled, current reading (general musicianship vs. lo-fi choices) is in [`docs/music/`](../music/README.md).

Research note, 2026-10-22. How open-source "Band-in-a-Box / iReal Pro"-style engines generate drums, bass, comping
and melody; how VNL-1's song engine (`app/song/`) compares; what to change.

**Marking.** **[src]** = read in the project's source or data files (shallow clones, October 2026). **[doc]** = read
in its docs or paper. **[inf]** = my inference (from code reading, not from listening or a test run). Every idea below
is described in my own words; no code was copied. Most sources are GPL/AGPL/LGPL; VNL-1 is MPL-2.0, so only ideas
may cross over.

## 1. Summary

- VNL-1 is already ahead of every open project on **song form** (planned maps, layer blocks, novelty) and on
  **clash safety** (pitch-class legality masks). Its **voicing search** is more general than the dictionary
  approach most engines use, and it matches good practice (common tones held, top voice first, rootless with
  bass).
- It is behind on **variation inside a song**. Every engine studied varies its patterns per bar or per chord with
  weighted choices: MMA through sequences, random skips and per-bar random choice, Impro-Visor through weighted
  patterns, JJazzLab through phrase tiling. VNL-1 fixes one comp style and one drum pattern per song, and each
  plays the same every bar. In lo-fi part of that is right, because a loop should feel like a sample, but it needs
  slow, Motion-scaled variation.
- I found **three defects** in reading our code: the walking bass "approach" note targets the current root, not
  the next chord's; bass roots are folded into a fixed octave window, so the line leaps; and the lead and ear candy
  are not swung while hats and kick are. There is also a **gap**: rootless voicings in keys-only intros and outros
  have no bass to supply the root.

## 2. Sources

| Project | What it is | URL | Licence | How studied |
| --- | --- | --- | --- | --- |
| MMA – Musical MIDI Accompaniment (Bob van der Poel) | Text-file-driven auto-accompaniment, 1000+ grooves | https://www.mellowood.ca/mma/ ; source via Debian: https://sources.debian.org/src/mma/21.09-1/ | GPL | [src] `MMA/patWalk.py`, `patChord.py`, `chords.py`, `patAria.py`, `seqrnd.py`, `pat.py`; `lib/stdlib/swing.mma` |
| Impro-Visor (Bob Keller, Harvey Mudd) | Lead-sheet notation, style-based accompaniment, grammar-based solos | https://github.com/Impro-Visor/Impro-Visor | GPL-2.0 (+ LGPL-3 for bundled libs) | [src] `src/imp/style/stylePatterns/BassPattern.java`, `ChordPattern.java`, `src/imp/style/Style.java`, `src/imp/lickgen/LickGen.java`, `vocab/My.voc`, `styles/*.sty`, `grammars/*.grammar`, `transforms/*.transform` |
| JJazzLab (Jérôme Lelasseux), incl. JJSwing and YamJJazz | Desktop BIAB alternative: Yamaha style engine + recorded-phrase jazz engine | https://github.com/jjazzboss/JJazzLab | LGPL (LICENSE file is LGPL-2.1; file headers say LGPLv3) | [src] `plugins/JJSwing/.../bass/*` (BassGenerator, TilerLongestFirstNoRepeat, WbpsaScorer, db/RootProfile), `drums/DrumsGenerator.java`, `NOTES.TXT`, `core/RhythmParametersImpl/.../RP_SYS_Intensity.java`, `RP_SYS_Fill.java` |
| Strudel (TidalCycles in JS): `voicing()`, iReal dictionaries | Live-coding; chord voicing from dictionaries scraped from iReal Pro's MIDI output | https://codeberg.org/uzu/strudel (GitHub repo is archived) | AGPL-3.0 | [src] `packages/tonal/voicings.mjs`, `tonleiter.mjs` (`renderVoicing`), `ireal.mjs` |
| Tonal.js `voicing`, `voice-leading`, `voicing-dictionary` | Music-theory library | https://github.com/tonaljs/tonal | MIT | [src] `packages/voicing/index.ts`, `packages/voice-leading/index.ts` |
| Dias & Guedes, "A contour-based walking-bass algorithm" (SMC 2013; from the GimmeDaBlues app) | Paper | https://repositorio.ipcb.pt/handle/10400.11/4690 | CC BY 3.0 (paper) | [doc] full text |
| accompy (Thor Whalen) | Python: chord chart → backing track; own pattern engine or MMA | https://github.com/thorwhalen/accompy | MIT | [src] skimmed: mostly a pipeline; its pattern engine is simple, and it can hand off to MMA |
| autocomp (agallant) | TypeScript PWA "iReal UX + BIAB styles" | https://github.com/agallant/autocomp | **none** (no licence file: all rights reserved) | [src] `src/style/swing/walkingBass.ts`, `pianoComp.ts`, `src/variation/*` |
| Lofi (jacbz) | ML lo-fi generator: a VAE writes chords and melody, a rule "producer" arranges | https://github.com/jacbz/Lofi | Apache-2.0 | [src] `client/src/producer.ts`, `producer_presets.ts` |
| Band-in-a-Box / iReal Pro | Closed source; reference only | pgmusic.com, irealpro.com | proprietary | [doc]/[inf] from public docs and the Dias–Guedes survey: transposed and chained pre-recorded phrases per chord type; BIAB's chart "pushes/shots/holds" |

Not studied in depth: LinuxBand (a GUI front end for MMA), Sonic Pi and Overtone (no accompaniment engine, only
user examples), and MuseScore plugins (I found no substantive accompaniment generator). The ML projects
(accompaniment-generator, Ultimate-Accompaniment-Transformer) are not runnable on an MCU.

## 3. Per-project analysis

### 3.1 MMA

- **Voicing [src].** Chord tracks use a named mode (`Voicing Mode=...`, `patChord.py: setVoicing/chordVoicing`).
  The modes are OPTIMAL, KEY/KEY2/ROOTKEY (rotate inversions toward the key's tonic), COMPRESSED (fold into an
  octave), INVERT, and DROP2/DROP3/DROP23. OPTIMAL (`chords.py: center1/center2`) moves each note by octaves to
  the nearest note of the last chord, then folds everything into a window around a "center". Two extras:
  `RMOVE` (a percentage chance per bar to shift the center up or down) and `MOVE`/`DIR` (shift the center every
  *n* bars). Those make the register drift on purpose, so a long vamp does not sit still. There is no top-voice
  rule and no rootless logic: extensions come from the chord name.
- **Comping rhythm [src].** Patterns are text lists of (offset, length, velocity per note), for example
  `{1 2 90 ; 3+81 4. 85}`, where `3+81` means "beat 3 plus a swung 8th". `SeqSize 4` makes multi-bar sequences,
  and `SeqRnd On` picks a random bar of the sequence each bar (`seqrnd.py`, weighted with `SeqRndWeight`). Each
  track can also humanize: `RSkip` (percentage chance to drop a note, optionally only on given beats), `RTime`,
  `RVolume`, `Articulate` (gate) and `Strum`.
- **Bass [src].** `patWalk.py: trackBar` walks a pointer up and down the chord's scale list. Direction is UP,
  DOWN, RANDOM or BOTH; BOTH has a weighted bias to step up. Beat 1 resets to the root about 50% of the time.
  There is **no look-ahead to the next chord** and no approach tones: the line is plausible but aimless. The
  plain Bass track instead plays pattern strings that name chord degrees (`1 4 1 90; 2+81 16 1 80; 3 4 5 90`).
- **Drums [src].** One track per drum sound, each a pattern plus `RSkip`/`RVolume`/`RTime`. Grooves come in
  families: `Swing`, `SwingWalk`, `SwingPlus`, `SwingSus`, `SwingFill`, `SwingIntro`, `SwingEnd`. Fills, intros and
  endings are separate grooves that the **user** places in the song file.
- **Form [src].** No automatic form: the song file switches grooves by hand.
- **Melody [src].** `Aria` (`patAria.py`) makes an automatic melodic line. It walks a pointer over chord, scale or
  key notes with a direction list (`Direction 1 1 -1 r ...`, where `r` means random) and an optional "deplete"
  (no repeated notes). This is a rhythm-pattern-plus-pointer walk with no phrase or motif.
- **Data model [src].** Plain-text, line-based DSL: `Begin Drum-Kick ... End` blocks, named pattern definitions
  (`Begin Chord Define`), `DefGroove name description`, includes, macros. It is very readable and diff-friendly,
  so it is a good model for a curation bench's files.

### 3.2 Impro-Visor

- **Voicing [src].** A **vocabulary** (`vocab/My.voc`) gives every chord type a spelling, a set of **color**
  tones (allowed tensions), a **priority** order (which tones to keep first when thinning), **approach** tones
  per chord tone, and a list of named voicing shapes: `left-hand-A/B/C` (the rootless 3-5-7-9 and 7-9-3-5 kind),
  `drop-2-*`, `drop-3-*`, `two-hand-*`, `shout-*`. For example, CM9 has left-hand-A E G B D. A style picks a
  `voicing-type` (closed, open, shout or quartal) and a register (`chord-low`/`chord-high`).
  `ChordPattern.getVoicingAndExtensionList` and `chooseVoicings` place each candidate shape next to the last chord
  (`placeVoicing`, a whole-shape octave shift), drop shapes that leave the range, and keep the ones with the
  **smallest average leap**. The final pick is random among ties. A newer optional "custom voicing" generator
  (`src/imp/voicing/VoicingGenerator.java`, `HandManager`) models two hands with priority and color notes. There
  is no explicit top-voice rule in the default path [src].
- **Comping rhythm [src].** Each style lists weighted `chord-pattern`s, for example
  `(rules X2+4+8 R8)(weight 50)` and `(rules X1+1+1+1)(weight 10)(push 8/3)`. `X` is a strike, `R` a rest and `V` a
  velocity. `push` **anticipates** the next chord by the given duration (here a swung 8th).
  `Style.getPattern` picks, **for each chord span**, the longest patterns that fit its duration, weighted at
  random. So comping varies bar to bar but stays within the style's vocabulary.
- **Bass [src].** Weighted `bass-pattern`s use a small rule language (`BassPattern.applyRules`). The rules are
  `B` (chord's bass note), `C` (random chord tone), `S` (scale tone a 2nd or 3rd from the last note), `A`
  (**approach**: a semitone above or below the **next** chord's bass), `N` (next chord's bass, early), `=`
  (repeat), `R` (rest), and scale degrees with b/#. `U`/`D` force up or down. Each note is placed **near the
  previous bass note** (`placePitchNear`), then pulled toward the style's `bass-base` with soft and hard margins
  (`pressure`). For example, swing.sty weights `B4 S4 C4 A4` at 10 and `B4+8` / `B2+4` at 100. Long held roots
  dominate and walking figures are spice.
- **Drums [src].** Weighted multi-lane `drum-pattern`s per style (`X` hit, `R` rest, `V` velocity, triplets),
  chosen per chord span like the comping. Ten or more variants per style, several of them single-snare-comment
  variants.
- **Form [doc/inf].** The user writes the leadsheet sections. Styles carry no intro/fill/ending logic; variety
  comes from the per-span weighted picks.
- **Melody [src].** Probabilistic grammars (`grammars/*.grammar`) generate **abstract melodies**: rhythm plus a
  note category, where `C` is a chord tone, `L` a color tone, `A` an approach, `S` a scale tone, `X` any note and
  `R` a rest. These are grouped in `(slope min max ...)` contour segments that bound the interval between
  successive notes. `LickGen` turns each category into a pitch within the slope's interval window; an `A` is
  placed a semitone from the next note (`LickGen.java` around line 2040). Grammar parameters weight chord, color
  and scale tones (Red Garland: 0.7 / 0.2 / 0.1) and set `rest-prob`, `leap-prob`, `max-interval` and the pitch
  range. Learned grammars (`BillEvans.grammar`) are **Markov chains over clusters** of abstract measures,
  extracted from transcriptions. Many phrases begin with a rest (`R8 C8 ...`, `R4 ...`), so starts are rarely on
  the downbeat [src]. **Transforms** (`transforms/*.transform`) rewrite motifs under guard conditions; for
  example, two chord tones at relative pitch 5 become a figure moved diatonically. ThemeWeaver interleaves a
  theme with generated material [inf from class names and docs].
- **Data model [src].** Lisp-style s-expression text files (`.sty`, `.grammar`, `.voc`) with weights on every
  alternative, editable in a style editor GUI. This is a strong model for "a list of weighted alternatives per
  role".

### 3.3 JJazzLab (JJSwing, YamJJazz)

- **YamJJazz [src/doc].** It plays Yamaha `.sty` styles: recorded MIDI per section (Intro, Main A–D, Fill-ins,
  Ending), and the chord table re-maps the recorded notes to each chord. Song parts choose a variation and a fill
  through "rhythm parameters".
- **JJSwing bass [src].** A **database of recorded walking-bass phrases** (`WalkingBassMidiDB.mid` plus 2-feel
  databases), each 1–4 bars with metadata (`WbpSource`, `RootProfile` = chord positions plus root intervals). A
  **tiler** covers the song's chord sequence with transposed phrases, longest first, never reusing a source
  (`TilerLongestFirstNoRepeat`). The scorer (`WbpsaScorer`) rates chord-type match, transposition and tempo, and
  whether the previous phrase's "target note" matches this phrase's first note: the next-chord continuity rule
  again. After tiling, a post-process adds pedal bass, slash chords, **accents and chord anticipations**
  (`processAccentAndChordAnticipation`) and a tempo-dependent swing (`tempoadapter/*`).
- **JJSwing drums [src].** A database of recorded patterns (`drums44DB.mid`) in "standard" and "fill" sets. Each
  song part is tiled with randomly picked standard phrases. If the part's **Fill** parameter says so (`always`,
  `random`, `random_rare`, `break`, `fade_out`), the last bar becomes a fill phrase (`DrumsGenerator.fillPhrases`,
  `isFillRequired`).
- **Intensity [src].** `RP_SYS_Intensity` (−10…+10) per song part shifts velocities
  (`BassGenerator.processIntensity`).
- **Timing notes [src: NOTES.TXT].** The swing ratio falls as tempo rises (about 2.6–3:1 slow to 1.4–1.7:1 fast).
  Systematic micro-timing beats random jitter, with jitter capped at ±10–20 ms at slow tempos. Accent beats 1 and
  3 slightly at slow tempos. Gate 95–100% legato with a short release before chord changes.
- **Data model.** Recorded MIDI with metadata, browsed in a database explorer dialog. That is high quality but
  costly to author.

### 3.4 Strudel (iReal dictionaries) and Tonal.js

- **Voicing [src].** Dictionaries map a chord symbol to several interval stacks. Strudel's `ireal` and
  `ireal-ext` sets were scraped from iReal Pro's MIDI output. Many keep the root at the bottom; for example, `13`
  includes `'7m 9M 10M 13M 15P'` and `'1P 7m 9M 10M 13M'`. `lefthand` gives two rootless forms per type (m7:
  `3m 5P 7m 9M` and `7m 9M 10m 12P`), and `guidetones` gives 3–7 shells. `renderVoicing` (`tonleiter.mjs`) is
  **top-note anchored**: in `below` mode it picks the shape whose top note lands closest *under* an anchor
  (default `c5`, or `a4` for lefthand) and transposes it there. In `above` and `root` modes it anchors the bottom
  instead. Voice leading is therefore implicit: the top voice hovers just under a fixed anchor, and moving the
  anchor moves the line.
- **Tonal [src].** `Voicing.search` lists every dictionary shape at every start note inside a range.
  `VoiceLeading.topNoteDiff` picks the candidate whose **top note** is closest to the last voicing's top note. It
  is the simplest explicit top-voice-first rule.
- No rhythm, bass or drums: those are patterns the user writes.

### 3.5 Dias & Guedes contour walking bass [doc]

There are three stages:

1. **Target note.** Pick a tone of the *next* chord; in the paper this is 100% the root. Choose its octave with a
   **direction** setting (lowest, down, nearest, up or highest).
2. **Trajectory.** Fill the passing notes between the current note and the target recursively, strongest beat
   first. An **openness** setting chooses between a direct path and a wide arpeggiated path. Strong beats take
   chord tones, weak beats scale tones, and the last beat may be a chromatic approach.
3. **Ornaments.** Optional swung-8th "anticipation" pickups.

The paper also notes that good players think in **two-bar** phrases rather than bar by bar. It critiques phrase
databases: a small one sounds repetitive, a large one is inconsistent, and the phrases must be neutral to chain
smoothly.

### 3.6 accompy, autocomp, jacbz Lofi

- **autocomp [src].** The walking bass (`walkingBass.ts`) puts the root on 1 and nearest chord tones in the
  middle. The last beat is a chromatic approach to the next bar's root, falling back to a chord tone if the
  approach would leap more than a 5th. The piano plays 3–7 shells on beats 2 and 4 (`pianoComp.ts`).
  `variation/` adds density modes (solo/sparse/normal/dense), section-end fills, endings, count-in, and seeded
  humanize (timing jitter capped inside the bar). It is clean and minimal, a reference for how little is needed.
  It has no licence, so ideas only.
- **accompy [src].** A pipeline around chord parsing and rendering. Its own patterns are basic "rhythmic
  skeletons", and it can hand off to MMA for real styles.
- **jacbz Lofi [src].** A VAE outputs the chord degrees and melody. The producer plays a closed triad, inverted
  (`producer.ts`), with the root on beat 1 in one of five bass rhythms chosen per bar (`BassPatterns`). Drums are
  sample loops chosen by tempo and energy. There is a one-bar silent intro, an outro that replays the first two
  chords, and a preset chosen by valence and energy (`selectPreset`). It confirms that "lo-fi" generators rely on
  loops plus mood presets. Its arrangement is much simpler than VNL-1's.

## 4. VNL-1 today (what the comparison is against)

- **Voicing** (`theory.c: th_voice_range`): a search over octave placements of 4 voices, dropping one tone from
  five. The order is root, then the 5th, never the 3rd or 7th, and a dominant keeps an extension. Costs are
  per-voice motion, the **top voice weighted 8×** with a penalty for leaps over 2, held common tones, register
  E3–A4 preferred (C3–C5 allowed), top ≥ C4, no semitone clusters, and no tight 2nds low. This is the most
  pianistic rule set of all the projects studied [src].
- **Chord colors** (`theory.c: th_chord_pcs`, `harmony.c: dominant_colors`): extensions stacked from the mode,
  avoid tones removed, dominants forced to 9/13. Reharmonizing only raises the color of one chord per pass
  (`spec_at`).
- **Comping** (`harmony.c: harmony_step`): one style per song (held, stabs on steps 0 and 6, offbeats 2/6/10/14,
  or a 6-step arpeggio), a rolled spread of 10–60 ms, and a darker voicing in intros and outros. Every bar plays
  the same.
- **Bass** (`groove.c: bass_note`): roots on kick hits; root-fifth (the 5th only if the chord has a perfect 5th);
  pedal; and walk (root, 3rd, 5th, approach) in one 8-bar window out of four, upright sets only.
- **Drums** (`groove.c: groove_step`, `library.c: LIB_PATTERNS`): one 16-step pattern per song with fixed ghost
  bits, 16th swing on odd steps, per-song snare and hat lateness plus small jitter, velocity jitter, a snare-roll
  fill only before kick/snare/bass layer changes, and 1-bar drops.
- **Form** (`plan.c`, `library.c: LIB_MAPS`): map skeletons plus Motion moves on a 4/8-bar grid, intro/outro dark,
  swell before drums, fade.
- **Melody** (`melody.c`): a 1–2 bar motif from rhythm cells, its first note a guide tone, then stepwise
  pentatonic-leaning moves. The head–solos–head plan targets 50/15/35 restate/variant/new. Legality comes from
  per-bar pitch-class masks (chord-scale, strong steps on chord tones, never a semitone from any sounding pitch
  class), and the motif is legal under every lead phrase.

## 5. Comparison table

| Dimension | MMA | Impro-Visor | JJazzLab | Strudel / Tonal | Dias–Guedes / autocomp | VNL-1 |
| --- | --- | --- | --- | --- | --- | --- |
| Voicing source | chord-name tones + mode (drop-2, optimal) | dictionary of named shapes (left-hand A/B, drop-2/3) | recorded MIDI re-mapped (YamJJazz) | dictionary of interval stacks | 3–7 shells (autocomp) | **search** over octave placements of rolled colors |
| Voice leading | nearest-note per voice, center window | min average leap, random tie-break | from recordings | top note under anchor (Strudel); min top-note diff (Tonal) | none | weighted motion, **top voice 8×**, common tones held |
| Top-voice priority | no | no (default path) | n/a | **yes (anchor)** | no | **yes** |
| Register drift over time | **yes (RMOVE/MOVE)** | no | n/a | via anchor | no | **no**: same voicings each pass [inf] |
| Rootless when bass plays | no | left-hand forms | n/a | lefthand set | shells | yes, **also when no bass plays** (gap) |
| Comp rhythm | multi-bar sequences, per-bar random pick, RSkip | weighted patterns per chord span, **push** | recorded + anticipations post-process | user-written | fixed beats 2 and 4 | **one fixed style per song** |
| Anticipation (push) | via pattern offsets | **yes** | **yes** | no | no | no |
| Bass logic | scale-pointer walk (aimless) | rule strings B/C/S/A/N, **near previous note + range pressure** | phrase DB tiling, target-note continuity | none | target note → trajectory → approach | root/fifth on kick, pedal, walk window |
| Approach targets next chord | no | **yes** | **yes** | n/a | **yes** | **no (targets current root)** [src] |
| Bass octave placement | octave setting | near last note + pressure | from DB, closest acceptable pitch | n/a | nearest / direction | **fixed fold to 28–39** (leaps) [src] |
| Drum patterns | per-instrument tracks, RSkip/RVolume/RTime | weighted multi-lane patterns per span | recorded std + fill sets | user-written | per style + section fill | one pattern per song, fixed ghosts |
| Fills | separate fill groove (user places) | none automatic | **per-part parameter incl. random_rare** | n/a | section-end fill | only before layer changes; 1-bar drops |
| Intensity / density | groove variants (Plus/Sus) | none | **Intensity −10…+10** | n/a | density modes | layers only (plus dark/swell) |
| Humanization | uniform random per track | swing value per style | tempo-aware swing + accents | n/a | seeded jitter | **per-song systematic lateness + small jitter** (good) |
| Swing applies to | all tracks | style + comp-swing | all (tempo adapter) | n/a | all | drums, bass; **not lead/candy** [src] |
| Form | user-placed grooves | user leadsheet | song parts + rhythm params | n/a | chart sections | **planned maps, novelty** (best) |
| Melody | Aria pointer walk | **grammar: C/L/A/S/X/R + slope**, Markov clusters, transforms | none | none | guide melody (autocomp) | motif + development, legality masks |
| Phrase starts | n/a | often rest-led (R8, R4) | n/a | n/a | n/a | **60% beat 1** (ledger) |
| Authoring | text DSL | s-expression text + GUI | MIDI databases | JS objects | code | C tables (bench to come) |

## 6. Ranked improvements for VNL-1

Ordered by musical value for cost, and filtered by Sam's preferences: no substitutions, dominants never bare,
top-voice voice leading, simplest spellings, soothing, no clashes.

### 1. Walking bass: approach the *next* chord, diatonically by default

- **What.** Beat 4 of a bar that precedes a chord change should lead into the **next** chord's root: Impro-Visor's
  `A`, Dias–Guedes's target note, autocomp's `approachMidi`. Today `bass_note` (BS_WALK) plays the current root
  +2 or −1 [src]. That lands correctly only for 2-bar chords, where "the next bar" is the same chord. For
  1-bar loops it approaches nothing. On m7 and dominant chords its −1 version (for example C# under Dm7) is a
  semitone from the voiced b7 (C): a bass-against-keys rub that `clash_test` does not check (it checks only lead
  and candy). On maj7 chords root−1 is the 7th itself, so it is harmless there [src; rub inferred].
- **Rule for Sam's ear.** Prefer a **diatonic** approach: the scale step above or below the next root, or the
  next chord's 5th. Allow a chromatic approach only from below, and only when it is not a semitone from any
  pitch class sounding in the current voicing. Keep beat 3 on a chord tone. Shape the bar as a short contour
  toward the target (Dias–Guedes's "nearest" direction) instead of always root → 3rd → 5th, which climbs the same
  way every bar.
- **Where.** `groove.c: bass_note` and `groove_bass_note`, which need the next chord's root (the caller has
  `harmony_chord_at(bar+1)`). Melody's beat-4 mask (`melody.c: bar_masks`) already reads `groove_bass_note`, so
  it stays consistent. Add **bass vs. chord** to `clash_test`.
- **Effect.** Walking lines that "arrive"; no low rubs.
- **Cost.** A few integer ops per beat; no RAM.
- **Fit.** Very high: no clashes, pianist logic.

### 2. Bass octave: nearest to the previous root, pulled toward a center

- **What.** `bass_pitch` folds every pitch class into 28–39 the same way [src], so Eb sits at 39 and E at 28: an
  11-semitone leap between neighbors, and roots jump octaves across the loop. Impro-Visor places each note near
  the last one, then applies "pressure" toward `bass-base` with soft and hard margins (`placePitchNear`,
  `pressure`) [src].
- **Where.** `groove.c: bass_pitch` and `bass_note`. Stateless version: place this bar's root nearest to the
  previous bar's placed root, where the previous root is recomputed from the previous chord with a bounded
  look-back to the loop start, which is deterministic. Break ties toward a center around D2–F2 and clamp to
  28–47.
- **Effect.** Smoother, more "played" bass (4ths and 5ths instead of 7ths and octave jumps); better weight in the
  register.
- **Cost.** Trivial.
- **Fit.** High (soothing).

### 3. Swing the lead and ear candy; fix square phrase starts (ledger "To revisit")

- **What.** `melody_step` adds a 5–25 ms random delay but no swing [src]. Odd-16th lead notes (cells 0x6, 0x9,
  0xD) therefore play straight against swung hats, a soft flam [inf]. Apply the same swing offset as
  `groove_step` to odd steps. Ledger R16 chose "no swing on lead", so this reverses a recorded decision: Sam
  should hear both.
- **Phrase starts.** Every grammar-driven soloist studied starts phrases off the downbeat often. Impro-Visor
  bricks are frequently rest-led (`R8 C8 ...`, `R4 ...`, `R2 ...`) [src], and Dias–Guedes treats pickups as an
  ornament. This supports the ledger's proposal: about 15% beat 1, 25% other beats, 35% 8th offbeats, 10% 16ths,
  15% pickups. Implement it as **rhythm templates with explicit leading rests** in `melody.c` (CELLS and the
  first-onset draw in `gen_figure`). Pickups end before the change and land on a chord tone.
- **Cost.** Trivial CPU; small table.
- **Fit.** High: more relaxed, less "sequenced".

### 4. Keys alone keep a root

- **What.** `th_voice_range` drops the root first from five-tone chords and may go rootless with four
  [src]. That suits a band, because the bass has the root. In intros and outros the map plays **keys alone**,
  and the dark voicing is also rootless, so the harmony can float, with a maj7 heard as its iii chord [inf].
  Every engine without a bass player (Strudel's iReal sets, jacbz) keeps the root.
- **Where.** Pass "bass present" from `harmony_step` (`b->layers & L_BASS`) into `voice_chord` → `th_voice_range`.
  When the bass is absent, drop the 5th first and keep the root, preferably in the lowest voice.
- **Effect.** Clear harmony in intros and outros, and the bass's entry no longer re-defines the chord.
- **Cost.** One flag.
- **Fit.** High (no ambiguity, soothing).

### 5. A slow top line for the keys (anchored top voice per pass)

- **What.** The voicing is deterministic given the previous one, so a 2–4 chord loop settles into the **same
  voicings on every pass** for the whole song [inf from `th_voice_range`]. Pianists and both reference systems
  move the top: MMA shifts the voicing center by chance or every *n* bars (`RMOVE`, `MOVE`), and Strudel anchors
  the top note under a movable anchor. Add a cost term pulling the top voice toward a **target top note per pass,
  or per 8-bar block**, chosen from the chord's guide and color tones along a gentle path (for example 5 → 9 →
  3 across passes, or a rise then fall over the song). Keep the 8× top-voice rule for chord-to-chord moves inside
  a pass.
- **Where.** `theory.c: th_voice_range` (add an optional `top_target` and weight); `harmony.c: voice_chord`
  chooses the target from `pass` and the seed, with drift amount scaled by Motion.
- **Guard.** Keep the keys' top under the lead's floor while the lead plays (lead_lo is about E4–G4 by mood).
  Lift the top only in keys-only or candy-only blocks, so the lead and keys don't collide.
- **Effect.** The loop breathes like a pianist re-voicing a sample; a quiet countermelody in the keys.
- **Cost.** One extra term per candidate (about 500 candidates per chord change today).
- **Fit.** Very high: top-voice priority is Sam's explicit preference.

### 6. Comp cells with slow, weighted variation and rare, soft anticipations

- **What.** Replace the four hard-coded step patterns with a small **library of 1-bar comp cells per style**,
  each weighted. This is Impro-Visor's weighted `chord-pattern`s and MMA's `SeqRnd`, but for lo-fi the choice
  should be **per loop pass** (or per 4-bar unit) with a strong repeat bias, not per bar. The loop should still
  feel sampled. Allow an optional **push**: strike the next chord one 8th (or swung 16th) early on the last bar
  of the loop (Impro-Visor `push`, JJazzLab chord anticipations).
- **Guards for "no clashes".** A pushed chord sounds over the old bass and the lead's last notes. Push only when
  the bass pushes with it (root-fifth and roots styles) and the lead mask for that 8th is the *next* chord's.
  Allow it only in groove blocks, never in intros and outros, and keep it off in sad mood.
- **Where.** `library.c` (a new `lib_comp_t`: steps, lengths, velocities, weight, push flag), `harmony.c:
  harmony_step`. A push also changes `melody.c: bar_masks` (the bound moves one 8th earlier) and
  `groove.c: bass_note`.
- **Effect.** Comping that sounds played, not stamped.
- **Cost.** A few hundred bytes of tables; trivial CPU. Medium complexity because the push touches three modules.
- **Fit.** Good for the cells; pushes are a taste call for Sam, so default them rare.

### 7. Drum micro-variation and phrase-end turnarounds

- **What.** Three parts:
  - **(a) Per-lane skip chance** for ghosts and hats (MMA `RSkip`, as a per-hit hash, not state), so hats breathe.
  - **(b) A/B bar variants**: each pattern gets an optional variant bar, for example a different kick on bar 4 of
    each 4 (MMA's `SeqSize` sequences, Impro-Visor's weighted patterns). Play it A A A B, or by weighted pick per
    pass.
  - **(c) Phrase-end "turnaround" figures**, the ledger's deferred "no phrase-end fills": a soft open hat on the
    and-of-4, one extra ghost, or a kick pickup at the end of 8-bar phrases. Use them with JJazzLab's
    `random_rare` logic, and keep the snare roll only for layer changes.
- **Where.** `library.c: lib_pattern_t` (variant bitmasks, skip percentages), `groove.c: groove_step`, `plan.c`
  (a `PB_TURN` flag).
- **Cost.** A few bytes per pattern; per-hit hash already exists.
- **Fit.** High if kept subtle. No crashes, no rolls: "nothing startling".

### 8. Per-block intensity

- **What.** JJazzLab's Intensity parameter (velocity shift per part) and autocomp's density modes. Give each plan
  block a small intensity value, about −3…+3. It drives velocity offset, comp-cell choice (sparser or denser),
  hat ghost density and lead note density. It rises slowly through the middle, dips in breaks and falls in the
  outro, scaled by Motion. This complements the planned filter and reverb automation arcs.
- **Where.** `plan.h: plan_block_t` (one int8), `plan.c`; read in `groove_step`, `harmony_step` and
  `melody_step`.
- **Cost.** One byte per block.
- **Fit.** Good, as long as the steps are small (±6–10 velocity); the anti-goal bans loudness jumps.

### 9. Melody: contour templates and color tones on strong beats

- **What.** Impro-Visor separates **rhythm plus note category** (C/L/S/R) from **contour** (`slope` segments).
  VNL-1's `next_pitch` is a weighted random walk with center gravity, which gives no phrase arch [src]. Add 4–6
  contour templates (arch; descent to a guide tone; rise then hold; stepwise answer) that bias the move direction
  per note. End phrases on a long chord tone.
- **Second part.** Allow a **color tone that the current voicing actually contains** (the 9th in a maj9 or m9) on
  strong steps. This loosens R28's narrow strong-step set (often 2 pitch classes) **without any rub**, because a
  voiced tone cannot rub with itself or its neighbors (R20/R25 already exclude semitones). Impro-Visor's grammars
  give color tones about 20% weight.
- **Where.** `melody.c: next_pitch`, `gen_figure`, `bar_masks` (a `color` mask from the sounding voicing).
- **Cost.** A small table; trivial CPU.
- **Fit.** High: Sam's "arpeggiated/dull lead" risk (ledger R28/R30) is addressed with no clash risk.

### 10. Tempo-linked swing range

- **What.** JJazzLab's notes: slower tempos take more swing. Within 70–90 BPM, tilt the swing roll toward the
  upper end at 70 and the median at 90. Keep lateness systematic per song, which is already right and is what
  the JJazzLab notes recommend over random jitter.
- **Where.** `roll.c` (swing roll).
- **Cost.** Trivial.
- **Fit.** Fine.

### 11. Curation bench data format

- **What.** Borrow MMA's readable block DSL and Impro-Visor's weighted alternatives:
  - **Drum patterns:** lanes as 16-char grids (`x...x...` for hits, `g` for ghost), weight, per-lane skip
    percentage, an optional variant bar, and a source note.
  - **Comp cells:** a step grid with lengths and velocities, weight, push flag.
  - **Bass figures:** rule strings in Impro-Visor style but our own letters, for example
    `R(root)4 C4 S4 A4`, where `A` means approach the next chord (diatonic by default).
  - **Melody rhythm templates:** strings like `r8 C8 S16 S16 C4 r4`.

  A script compiles them to C tables, as the spec already plans. Weighted lists make the bench's
  keep/fix/drop ratings map directly onto weights.
- **Fit.** Directly serves the planned bench; text files diff well.

### 12. Later, maybe: preferred rootless shapes, a tiny bass-phrase database

- **Rootless shapes.** Add a small bonus in `th_voice_range` for the classic left-hand A/B shapes (3-5-7-9 and
  7-9-3-5), as in Impro-Visor's vocab and Strudel's `lefthand`. Our search already reaches these often [inf], so
  this is low value.
- **Phrase database.** A JJazzLab-style database of 1–2 bar bass phrases (100 phrases × about 8 notes × 3 bytes
  ≈ 2.4 KB of flash) would sound more human. It costs curation time, though, and Dias–Guedes warns about
  repetition and neutral-sounding phrases. Revisit only if rule-based bass still sounds mechanical after items 1
  and 2.

## 7. What does not apply (lo-fi or MCU)

- **Chord substitutions and reharmonization styles** (Impro-Visor `swing-substitute.sty`, its substitution
  files, tritone subs): excluded by Sam.
- **Chromatic approach tones as a default** (bass `A`, melody `A`, Impro-Visor color/approach lists): they conflict
  with the no-semitone rule. Use diatonic approaches; chromatic only under the guard in item 1.
- **Learned grammars and Markov clusters from bebop transcriptions**: far too dense and jazzy for a sparse lo-fi
  lead, and the training pipeline is off-device. The *format* (rhythm plus category plus contour) is the useful
  part.
- **ML generators** (jacbz VAE, transformers): no float tensor runtime or RAM budget on an AC79 for this.
- **Large recorded databases** (JJazzLab MIDI DBs, Yamaha styles, BIAB RealTracks): flash and curation cost; see
  item 12.
- **Tempo-adaptive swing for fast tempos** (JJazzLab above 160 BPM): VNL-1 lives at 70–90.
- **Live-coding dictionaries' root-position voicings** (Strudel's `ireal` set keeps roots): right for a solo
  piano, wrong under a bass. Item 4 takes only the "keys alone" case.

## 8. Where the sources contradict how VNL-1 is built

1. **Next-chord targeting.** Every bass engine with look-ahead (Impro-Visor, JJazzLab, Dias–Guedes, autocomp)
   aims the last beat at the next chord. VNL-1's walk does not (item 1). This looks like a defect, not a choice.
2. **Bass register.** Every engine places bass notes relative to the previous note. VNL-1 folds by pitch class
   (item 2).
3. **Swing scope.** All engines swing every swung-genre part. VNL-1 exempts the lead and candy by ruling R16
   (item 3), so this reverses a recorded decision; let Sam hear both.
4. **Variation granularity.** All engines vary comping and drums per bar or chord from weighted lists; VNL-1 is
   fixed per song. Lo-fi's sampled-loop aesthetic partly justifies VNL-1, so the answer is slower variation (per
   pass or block, Motion-scaled), not per-bar randomness (items 6–8).
5. **Rootless everywhere.** Engines that go rootless do so because a bass plays. VNL-1 also goes rootless when
   the keys are alone (item 4).

What VNL-1 does better than the sources, and should keep: explicit top-voice priority with common tones held (no
open engine combines both); dominants always extended; avoid-tone removal by rule; per-song systematic lateness
instead of uniform jitter; legality masks that make clashes impossible by construction; planned song form with
novelty between songs.
