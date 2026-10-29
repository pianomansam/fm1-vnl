# A Romanticism lead

Question: what makes the melodic lines of late French Romanticism recognizable, and which of those traits can become generative rules for VNL-1's lead without breaking Sam's rules? This is one of several source studies after the Impressionism lead, which Sam approved by ear.

Scope: late-Romantic French songs and instrumental pieces whose lines glide by small steps across changing chords: art songs (mélodies), a 6/8 siciliana, a slow processional dance, a cello elegy, a lullaby song and the gentle movements of a requiem. This doc covers web analysis sources, a throwaway spike and renders. The spike is on worktree branch `worktree-agent-aeec8cac90861a25b`, commit 4100d01 (`lead_style` 3 in `app/song/lead.c`). The renders are in `app/build/new/lead-romanticism-2026-10-29/` (renamed in the name scrub), described in that folder's `INDEX.md`. They are the same five songs as the Impressionism A/B.

## Summary

- The Romanticism line is about **continuity**. Impressionism's is about shape and time. Its traits are:
  - one long, mostly stepwise line that keeps moving while the chords change under it (the "long line");
  - melody built from a small cell restated in sequence, a step higher or lower;
  - an even, repetitive rhythm with few ornaments;
  - phrases that each hold one defining leap (a rising 4th, a 6th), filled back by step;
  - triplets near phrase ends, and appoggiaturas at cadences.
- Most of it fits VNL-1's rules. The spike keeps all of them: **0 rubs** in all renders, every note passes `legal()`, and notes on beats 1 and 3 are chord tones.
- **Two traits conflict:**
  - **Enharmonic gliding** is a harmonic trait in this style: the chords slide, and the melody follows. VNL-1's chords are given and almost all diatonic, so the lead can't glide chromatically. What remains is the glide's melodic half: crossing every chord change by a held common tone or a step.
  - **"Stepwise."** Over minor 7th and major 7th chords, VNL-1's semitone rule removes the scale's neighbor notes. For example, over Gm7 it removes A (against Bb) and Eb (against D), leaving G Bb C D F. Over these chords "by step" becomes steps in a pentatonic-like set, so the line moves in 2nds and minor 3rds.
- The spike's line moves by a 3rd or less 89-96% of the time (current lead 79-98%, Impressionism 76-97%). It crosses 14-26 bar lines a song without a breath (current 0-15, Impressionism 3-23), and strikes beat 1 21-32% of the time (current 9-42%, Impressionism 0-11%).
- Best approved lead for it: **the flute** (the siciliana's and the processional's own instrument; a sustained voice that can hold a line across the bar). The saw lead is second. The felt piano and vibes decay, so the held notes that carry the line over the bar line die away.
- Sam has to listen; numbers can't settle it.

## 1. What makes the lines recognizable

Traits only: no melody, motif or figure from any piece is described note for note, reproduced or stored.

### The long line

Scholarship on the style names a distinctive trait, the so-called "long line": voices that move across a long span through conjunct motion in a single direction, sometimes a single, comprehensive, stepwise ascending gesture. Melody is described as inseparable from the harmony, "a sort of emanation" of it, and the style as having learned tonal freedom and long melodic lines from the early Romantic piano.

**Rule:** a phrase is one line. It climbs (question) or falls (answer), or arches, across its four bars. It crosses every bar line moving, and breathes only at the phrase's end.

### The melody carries the harmonic motion

In the style's best-known art song, the harmony moves about one chord a bar, often a long circle of fifths, and the voice moves mainly conjunct over it. Teaching notes stress that the melody is less chromatic than the harmony: the voice's last bars are diatonic while the piano passes through altered chords. The later style is described as full of enharmonic shifts. Those shifts live in the chords; the line glides over them.

**Rule:** the melody note before a chord change moves to the new chord by a held common tone or a step. Where it can, it is held over the bar line and resolves an 8th or a quarter later (the suspension). The line stays diatonic; the chords are VNL-1's.

### Sequence from a small cell

The style is described (secondhand) as a master of unfolding a melody: from a harmonic and rhythmic cell, chains of sequences. One paper analyzes the melodic sequences of an art song (modulations that repeat the melody at a higher pitch). In the processional dance, the central section runs four-bar sequences over bass pedals falling by whole tones, a favorite device of the style.

**Rule:** one cell per song (rhythm and contour). Each bar restates it a scale step or two higher or lower, so the line climbs or falls by sequence, not by running scales.

### Even, repetitive rhythm; little ornament

The style's rhythms are called subtle and repetitive, with discreet syncopations. In the art song, the piano keeps a single rhythmic pattern throughout while the voice avoids repetitive rhythms. The siciliana is in 6/8 at a dotted quarter of 50. The processional's theme returns with only small, barely perceptible changes, under new harmonizations. Ornament is sparse: appoggiaturas, and melismas only on the triplet groups at phrase ends.

**Rule:** a fixed rhythm cell per song, legato (each note held to the next). No runs or grace notes. The only ornaments are an appoggiatura at the cadence and a closing triplet.

### Phrase shape: the defining leap, the triplet ending, the pickup

Teaching notes on the art song give the most precise phrase traits:

- Every phrase includes a defining leap or outlines a significant interval: the 6th, a rising 4th at critical moments, and falling 5ths as punctuation marks.
- Triplet 8ths towards the ends of all the phrases, the first of the triplet usually tied over from the previous note.
- Phrases in the first verse start on beat 1. Later ones start with an anacrusis.
- Phrase lengths are uneven: 3+4 bars, then 3+2+2 bars to build tension.
- The vocal range is an 11th.

In the requiem, one movement is a six-bar modal melody whose second call reaches higher and whose middle alternates two notes; another opens with a simple modal melody of little ambitus; the last is a rising expressive melody over fast broken triads.

**Rule:**

- Some phrase heads leap a 4th to a 6th, then fill back down by step.
- Bar 3 may end in an 8th-note triplet, its first note tied.
- The answer may start with an 8th-note pickup: a rising 4th or a step.
- The question ends higher than it began, and the answer comes home.

### Register and arc

The lullaby song spans a 13th, its middle section unusually dramatic for the style. In the cello elegy, the opening melody returns an octave higher in the cello's upper register. A phrase climbs or falls about a 5th to an octave.

**Rule:** the line starts low in the lead's range (about a minor 3rd over its floor) so the question has room to climb. Where it meets the range's edge, the rest of the bar is mirrored.

### What I didn't find

No source found gives score-level analysis of the siciliana's or the processional's melodies, or of the requiem's last movement. Statements about those pieces above are limited to what the cited pages said. No published generator models this style specifically.

## 2. How it differs from the Impressionism lead

| | Impressionism lead (style 2) | Romanticism lead (style 3) |
|---|---|---|
| Unit | an arabesque (quick notes) curling into a long color note | an even cell restated in sequence, one step on |
| Breath | inside the phrase (the float bar, sighs, silence) | only at the phrase's end: the line crosses every bar line |
| Beat 1 | rarely struck (0-11%) | struck about a quarter of the time (21-32%), often held over and resolved instead |
| Ornament | arabesques, whole-tone tetrachords | none but a cadence appoggiatura and a closing triplet |
| Color | long 9ths, 13ths and 11ths, off the beat | chord tones on 1 and 3; color comes from passing notes and suspensions |
| Rhythm | triplets and lateness, behind the beat | steady (half + quarters, dotted quarters, even quarters, a dotted-8th lilt), triplet only at phrase end |
| Contour | rise, float, sigh down | one long climb or fall a phrase (or an arch), by 2nds and 3rds |

## 3. Fit with VNL-1's rules

| Trait | Fit | How it's kept legal |
|---|---|---|
| Long stepwise line | **Partly conflicts** | The walk steps through the notes `legal()` allows over the bar's chord. Over min7 and maj7 chords that set is pentatonic-like (see Summary), so "steps" include minor 3rds. Where the next legal note is a 4th or more away, the last note holds instead of leaping. |
| Melody moving over changing chords | Fits | The note before the bar line steps into the next chord's chord tone, or is held over (only when legal over both chords, the push rule the phrase planner uses) and resolves by step an 8th or a quarter later. |
| Enharmonic gliding | **Conflicts** | Not attempted. Chords are given and diatonic (no substitutions), and a chromatic line note would leave the chord-scale. The glide is kept only as common-tone and step motion across chord changes. |
| Sequence | Fits | Contour in legal-note steps, re-taken from wherever the line is. The chords are never changed. |
| Defining leap | Fits | A 4th to a 6th (5-9 semitones) up from the head, chord tone if it lands on 1 or 3, then fills down by step. |
| Triplet ending | Fits | The Impressionism spike's thirds-of-a-16th lateness (`dl[]`). |
| Appoggiatura | **Partly conflicts** | The style's appoggiaturas fall on the strong beat. VNL-1's lead rule is chord tones on steps 0 and 8, so the spike plays the cadence appoggiatura on 1 only when it is a chord tone (the chord's own 9th or 13th). Otherwise the cadence is the resting note alone. |
| Beat 1 | Fits "vary, not avoid" | Struck 21-32%. Otherwise held over and resolved. |
| Velocities | Fits | 58-79 (vibes 90-97). |
| Simplest spelling | n/a in code | Docs use Bb, Gb, Db Lydian. |

## 4. Proposed rule set: the Romanticism lead

Numbers are the spike's, to be tuned by ear.

**Phrase.** Four bars, twice per 8 (question, answer), anchored like the phrase planner and the Impressionism lead: lead runs, 8-bar passes, a 4-bar remainder plays the answer.

1. **Line shape.** One per song:
   - rise 50%: the question climbs, the answer falls;
   - fall 30%: both fall, the answer lower;
   - arch 20%.
   Each pass keeps the song's shape 70% of the time, else takes another.
2. **Cell.** One rhythm cell per song, by motion (onsets in 16ths):

   | Cell | Onsets | Motion under 40 | 40-60 | Over 60 |
   |---|---|---|---|---|
   | long | 0 8 12 | 40% | 10% | - |
   | dotted | 0 6 12 | 40% | 20% | - |
   | dotted pair | 0 6 8 12 | 20% | 40% | 30% |
   | even | 0 4 8 12 | - | 30% | 35% |
   | lilt | 0 3 4 8 12 | - | - | 35% |

   Bar 2 drops one onset 35% of the time. Every note is held to the next.
3. **Spine (beat 1).** A chord tone.
   - **Mid-phrase:** one legal step from the last note, the way the last bar planned (else 2 steps, else back). This is the glide.
   - **Phrase head:** the chord tone nearest the line's start: the anchor, a minor 3rd over the range's floor, plus the shape's start, ±1 step 40% of the time.
4. **Sequence walk.** The bar's k moves (its k-1 notes, plus the step over the bar line) all go the bar's way except one back step. The back step's place is drawn per song (first 30%, over the bar line 50%, middle 20%) and varied per pass 40% of the time. So the line climbs or falls about a 3rd a bar.
   - Notes on beat 3 must be chord tones (else one step on, else the note holds).
   - A move of a 4th or more holds instead. The exception is the first move, which may be a 4th.
   - The range's edge mirrors the rest of the bar.
5. **Suspension.** The bar's last note is held over the bar line 35% of the time (bars 1-3), if it is legal over both chords. It resolves by step, an 8th (50%) or a quarter (50%) into the next bar, onto a chord tone.
6. **Defining leap.** 40% of phrase heads leap up 3 or 4 legal steps, capped at 5-9 semitones (a 4th to a 6th), then fill down by step.
7. **Triplet ending.** 35% of bar 3s end with an 8th-note triplet after beat 3, its first note tied.
8. **Cadence (bar 4).**
   - The chord tone nearest the line (glide-weighted), held 11-12 16ths.
   - 45% of the time it is preceded by an appoggiatura a step above, but only when that note is a chord tone.
   - Then a breath of 4-5 16ths.
9. **Answer pickup.** 50% of questions end with an 8th pickup on the "and" of 4 into the answer's head: a rising 4th 40% of the time where legal, else a step below.
10. **Velocity.** A swell to bar 3: 64 → 76, plus half the pitch above the anchor. Cadences are 60-66, pickups 58. The vibes are lifted to 88-99, as the other planners lift them.
11. **Legality.** Every note passes `legal()`. Notes on beats 1 and 3 are chord tones. A tie needs legality over both chords. The chords are never changed.

**Not implemented, proposed next:**

- Uneven phrase lengths (3+4) where the plan allows. They would need `phrase_at` changes.
- A phrase-end falling 5th as punctuation.
- Once per song, the answer's line taken up a register for its climax (the octave-higher return).

## 5. Spike results (listening proxies, not a verdict)

Five songs × Romanticism, plus two with the bed. The full table, with where the lead plays in each render, is in `app/build/new/lead-romanticism-2026-10-29/INDEX.md`. Current and Impressionism figures are from `app/build/new/lead-impressionism-2026-10-29/INDEX.md`, and the long-line columns from the spike's survey script.

| | Current lead | Impressionism lead | Romanticism lead |
|---|---|---|---|
| Notes per lead bar | 2.25-2.75 | 2.35-3.06 | 2.35-3.25 |
| Range (MIDI) | 68-88 | 71-87 | 71-88 |
| Share held a quarter+ | 0.18-0.91 | 0.33-0.45 | 0.71-0.90 |
| Onsets on beat 1 | 9-42% | 0-11% | 21-32% |
| Onsets off the beat | 9-64% | 53-67% | 13-40% |
| Moves of a 3rd or less | 79-98% | 76-97% | 89-96% |
| Bar lines crossed without a breath | 0-15 | 3-23 | 14-26 |
| Legato leaps of a 4th+ | 0-7 per song | 0-2 per song | 1-3 per song |
| Rubs (lead vs keys, bass, pad) | 0 | 0 | 0 |
| Velocity | 44-104 (felt hybrid 106-110) | 56-82 (vibes 90-98) | 58-79 (vibes 90-97) |

## 6. Concerns

- **Density.** Songs 1, 4 and 5 run at 3.05-3.25 notes per lead bar, above the current lead's 2.25-2.75. With the line rarely breathing, it may feel busy for lo-fi. Under motion 40 the cells are already the long ones. Dropping the triplet or the bar-2 thinning by motion is the next knob.
- **"Stepwise" is pentatonic-like over 7th chords** (section 3). Only 48-61% of moves are 2nds; the rest are mostly minor 3rds. It may sound more like a pentatonic sequence than the style's scalar legato.
- **The glide is weaker than hoped.** The engine counts 42-60% of chord changes crossed by a held note or a step of 2 semitones or less. The rest move a 3rd, because the legal sets are sparse.
- **Sequence can sound mechanical.** Each bar restates its cell. Per-pass variation (shape, back-step place, head lift) keeps whole phrases from repeating literally, but within a phrase the restatement is deliberate.
- **The felt piano and vibes.** Ties over the bar line decay on struck instruments, so the long line breaks audibly. The flute is the fit.
- **The appoggiatura** is mostly gone, because of the chord-tone-on-1 rule. If Sam wants it, a ruled exception (a legal, non-avoid color tone on beat 1 of the cadence bar only, resolving down by step) needs his ruling and an audible A/B.
- **Spike quality.**
  - There are no host tests.
  - A note that can't be placed holds the last one, so some bars are sparse (one held note).
  - The question and the answer draw their shapes separately, so they can disagree.
  - Triplets use sample delay, so the song's swing doesn't apply to them (as in the Impressionism spike).
  - The cadence can repeat the last note where only one chord tone is in range.

## Sources

Ten web sources on the style: a music-theory journal article on its nocturnes (with the scholarship it cites), exam-board teaching notes on its best-known art song, encyclopedia entries on the style and on individual works, a mathematics-and-art conference paper on melodic sequence, and two secondhand summaries. All were read as untrusted data. The citations named the source person and works, so they were removed in the name scrub (2026-10-31, Sam: lead styles go by genre name only); the traits above are limited to what those sources stated, and the secondhand ones are marked as such.
