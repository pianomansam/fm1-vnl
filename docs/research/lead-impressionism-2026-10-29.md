# An Impressionism lead

Question: what makes the melodic lines of French impressionism recognizable, and which of those traits can become generative rules for VNL-1's lead without breaking Sam's rules? Sam: "I'm still not always happy with the lead lines. I want you to explore a new one", in the impressionist style.

Scope: the style's slow flute and piano lines (solo flute pieces, an orchestral prelude's flute opening, slow piano pieces and preludes, a piano arabesque); its quick, ragtime-flavored pieces are not the target. This doc covers web analysis sources, a throwaway spike and A/B renders. The spike is on worktree branch `worktree-agent-aaf267bc7c3b70bee`, commits 93c14e3 and f5bca57. The renders are in `app/build/new/lead-impressionism-2026-10-29/` (renamed in the name scrub), described in that folder's `INDEX.md`.

## Summary

- The impressionist line is about **shape and time**, not exotic notes. Its traits are:
  - ornamental curls (arabesques) that land on a long note;
  - long notes that color the chord (9ths, 13ths);
  - entries behind the beat, and ties across the bar;
  - motifs repeated with small changes;
  - descending sighs;
  - silence.
- Most of that fits VNL-1's rules as they stand, and the spike keeps every one of them. All its notes pass the existing `legal()` check, and it had **0 rubs** in all renders.
- **Whole-tone material mostly conflicts** with the rules. Real whole-tone scales are chromatic and rub. The legal way in is the one whole-tone run every major scale contains: four notes, three whole steps (F G A B in C). It is legal only over a dominant-type chord, where it spells b7-1-9-3.
  - Even there it cannot run on into a landing beyond itself, because the next note in either direction is a semitone from the chord. It has to turn back onto its own third note.
  - Over the five renders it sounded in **4 of 7 tries**. Across 12 survey seeds it sounded in **4 of 15**.
- The spike sounds different from the current lead, mainly in rhythm:
  - Only 0-11% of its notes start on beat 1, against 9-42% for the current lead.
  - 53-67% of its notes start off the beat, against 9-64%.
  - It has fewer legato leaps: 0-2 per song, against 0-7.
  - Its density is the same, about 2.3-3.1 notes per lead bar.
- Sam has to listen; numbers can't settle it.

## 1. What makes the lines recognizable

Traits only: no melody, motif or figure from any piece is described note for note, reproduced or stored.

### Arabesque (the ornamental, curving line)

The style took "arabesque" from the visual arts: a line that decorates, curls and turns back on itself, a highly ornamental melody whose free unfolding slows the sense of time. In the style's best-known piano arabesque, the line descends in 8th-note triplets over the harmony; its opening cell is three notes, extended to seven on its return, and a falling four-note motif comes back in the coda with triplet ornamentation.

**Rule:** a short figure of 3-5 quick notes (16ths or 8th triplets) that turns around its goal and ends on a long note. Repeats of it are extended by a note or two.

### Pentatonic and modal lines

- The style's pentatonic preludes are built on pentatonic collections: an unaccompanied opening line in short three-note groups that alternate steps and leaps, a plagal first cadence, tonal but not functional voice leading.
- Its slow piano pieces open on melodies that are "not quite major or minor" (often heard as Dorian), reaching a plain major only later.
- Its flute writing is described as using whole-tone and pentatonic scales throughout, with an ambiguous center.

**Rule:** lean on each mode's pentatonic subset, and let the mode's own color note sound (Dorian 6, Lydian #4, Mixolydian b7, Aeolian b6, Phrygian b2).

### Whole-tone fragments

At the extreme, a whole prelude lives on one whole-tone scale apart from a short pentatonic passage. The style's typical use of the whole-tone scale, though, is as a contrast: whole-tone passages alternating with modal or tonal ones.

The style's most famous flute opening falls mostly by semitones through a tritone and climbs back. A tritone is the span of a whole-tone tetrachord: two notes three whole steps apart.

**Rule:** brief whole-tone gestures as passing color, never a whole-tone section (see section 3).

### Planing and parallelism, as it bears on one line

Planing moves a whole chord in parallel. A single line can't plane. Its melodic counterpart is **the same contour restated over the next chord at the same chord function**: a figure landing on a 9th lands on the next chord's 9th.

**Rule:** the motif's shape is fixed in scale steps and re-anchored to each bar's chord. The landing note is chosen by its function over the chord (9th, 13th, 11th), not by a fixed pitch.

### Rhythmic fluidity

- In the style's best-known 9/8 nocturne, each melodic figure enters an 8th behind the beat, with syncopation inside the figures as well; its rubato is written into the notation, through tuplets and syncopations that bend the meter.
- The solo flute writing is meant to be played with a great deal of rubato.

**Rule:** start figures off the beat, land long notes behind it (on the "and" of 2 or the "and" of 3), tie them over the bar, and use triplets inside the 16th grid. Rubato is a small lateness on some landings, never a drift.

### Motif repetition with small variation

An arabesque's three notes become seven. A prelude restates its opening with harmony added. A solo flute piece keeps its themes returning in an ABA' shape.

**Rule:** one motif per song, restated at every phrase head. Every restatement after the first is varied in exactly one way: re-timed, extended, mirrored, or turned whole-tone.

### Long held notes that decorate the harmony

Lyrical melodies ride over arpeggios that carry the modal color, and held melody notes float over the chord rather than outline it. The long note is where the color lives.

**Rule:** the long note is a 9th, 13th or (over minor chords) 11th wherever it is legal, the root last.

### Descending sighs, and contours that rise then float

- A fall and a return, ending where it began.
- Phrases that start high and drift down.
- Falling triplet cascades.
- A solo line that descends to its last low note.

**Rule:** the phrase rises to a peak in its first half, floats on held notes, then falls by step in pairs (sighs) to a low resting note. The answer phrase ends lower than the question.

### Register and silence

- The flute solos sit in the flute's warm low-middle register.
- Solo lines whose rests are part of their form.
- Pieces that open on an unaccompanied line.

**Rule:** center the line in the instrument's middle (C5-D5 for the flute), never on its top. Every phrase ends with at least 6 16ths of silence.

### Existing algorithmic work

No published generator was found that models impressionist melody specifically.

- **General melody models:** recurrent networks trained on a melody corpus, and Markov models with phrase-contour context, which listeners preferred over plain Markov output.
- **Corpus work:** an annotated piano corpus of the style (MuseScore files, CC BY-NC-SA 4.0) and a study built on it, which measures a shift away from diatonicity toward octatonicity across the period, with more fragmentation.

The corpus is a possible future source for measuring interval and duration statistics. Its non-commercial license rules out shipping anything derived from it in a GPL firmware without care, so it is reference only.

## 2. Fit with VNL-1's rules

| Trait | Fit | How it's kept legal |
|---|---|---|
| Arabesque into a long note | Fits | Every note passes `legal()` (in the chord-scale, no semitone against a chord tone or a struck bass note, no avoid note). An onset on step 0 or 8 must be a chord tone. Notes that would rub are stepped past, not leapt over. A run is cut at its first gap wider than a major 3rd. |
| Pentatonic and modal color | Fits | The pentatonic is a subset of the mode, so nothing is new. The color note is in the mode. |
| Whole-tone fragments | **Mostly conflicts** | Only the scale's own whole-tone tetrachord, only where all four notes are legal over the chord, turning back onto its third note. See section 3. |
| Planing (as contour) | Fits | Contour in scale steps, re-anchored per chord. The chords are never changed (no substitution). |
| Ties over the bar, entries behind the beat | Fits | A tie is allowed only when the held note is legal over both chords (the push rule the phrase planner already uses). Else the note is cut at the bar line. |
| Triplets, rubato | Fits, with a mechanism | The lead's 16th grid gains a lateness in thirds of a 16th (`dl[]`, applied as the event's sample delay), so 8th triplets are 0, +1⅓, +2⅔ 16ths. "Rubato" is a ⅓-16th late landing about a quarter of the time. |
| Long colored notes (9, 11, 13) | Fits partly | A 9th over a minor chord is a semitone from the minor 3rd, so `legal()` refuses it (R25). Over minor chords the color comes from the 11th instead. Over major chords from the 9th and 13th. |
| Sighs | Fits | A non-chord tone falling a step happens only on weak 16ths, so no avoid note lands on a strong beat. |
| Register | Fits | Inside the bar's lead range (a 4th over percussive keys, a pad's place), centered under the top. |
| Velocities 25-110 | Fits | 56-84 in the spike (vibes lifted to 90-99, as the phrase planner lifts them). |
| Simplest spelling | n/a in code | Docs use Bb, Gb (not A#, F#). |

**Conflicts besides whole-tone.** Beat 1 is almost never an onset (0-11%). The impressionist line ties over it or enters after it. Sam's standing note on the phrase planner was "beat 1 is the norm, varied, not avoided". In this lead, beat 1 is usually *sounding* (the tied note) but not *struck*. That needs Sam's ear: is a tie through beat 1 enough, or should 30-40% of phrase heads strike beat 1?

## 3. Whole-tone: where it can live (honest result)

The real whole-tone scale (C D E F# G# A#) has three notes outside any diatonic mode. Against diatonic chords those notes produce exactly the semitones Sam's rule forbids. So whole-tone material is restricted to the scale's one whole-tone tetrachord.

| Mode (on C) | Tetrachord | Legal over |
|---|---|---|
| Ionian | F G A B | G7 / G9 (V): b7-1-9-3. Also Bm7b5. |
| Dorian | Eb F G A | the IV7 (F7) of Dorian vamps |
| Mixolydian | Bb C D E | C7 (I7) itself |
| Lydian | C D E F# | D7 (II7) only, not the tonic (F# rubs the 5th) |
| Aeolian | Ab Bb C D | Bb7 (bVII7) |

Two findings bound how it can be used:

1. **In Ionian, over the tonic, ii, iii, IV and vi it never fits.** At least one of the four notes is a semitone from a chord tone.
2. **Even where it fits, it can't continue.** The scale notes just past the tetrachord (C above B, E below F, in C) are a semitone from the dominant's 3rd or 7th. So a run of three whole steps can't land on a fourth note beyond itself.

The spike's version therefore runs through the four notes and turns back a whole step onto the third (for example, Gb Ab Bb C, then Bb). In Ab Mixolydian over Ab7, that turn sounds in render 4.

Hit rate:

- In the renders it sounded in 4 of 7 tries: 2 of 2 in Ab Mixolydian, 1 of 1 in A Ionian, 1 of 3 in B Dorian, and 0 in G Aeolian and Db Lydian.
- Across 12 survey seeds it sounded in 4 of 15.
- It never sounded in Aeolian or Lydian songs, whose progressions rarely carry the needed dominant.

The other possibility, a non-scale whole-tone passing note on a weak 16th, would break the chord-scale rule. I didn't try it. If Sam wants more whole-tone color, the honest options are:

- (a) accept that it lives on dominants only (current spike);
- (b) a ruled exception: a chromatic whole-tone passing note on a weak 16th, never against a chord tone or the bass a semitone away. It would need a ruling and an audible A/B.

## 4. Proposed rule set: the Impressionism lead

Numbers are the spike's (commits 93c14e3 and f5bca57), to be tuned by ear.

**Phrase.** 4 bars, twice per 8 (question, answer), anchored like the phrase planner: lead runs, 8-bar passes, a 4-bar remainder plays the answer.

1. **Arabesque.** 3-5 quick notes:
   - 16ths 60% of songs, 8th triplets 40%;
   - capped at 3 under motion 40, plus one over motion 70.
   - The shape is drawn once per song: turn 30%, rise 25%, fall 25%, curl 20%. A shape mirrors where more of it fits in range.
   - Walked over the mode's pentatonic (65% of songs; or pentatonic plus the color note, 35%), restricted to legal notes. It falls back to the full legal scale when that is smoother.
   - Cut at its first gap wider than a major 3rd.
2. **Landing.** On the "and" of 2 (35%), beat 2 (20%), beat 3 (25%) or the "and" of 3 (20%), drawn per song.
   - A chord tone if on beat 3, else a color tone. Cost: 9th, 13th or (minor) 11th −4; root +8; 5th +2.
   - Late by ⅓ of a 16th 25% of the time.
   - Tied into the next bar 60% of the time if legal over both chords: held 6 16ths into it, or 12 when bar 2 floats.
3. **Float.** Then one of:
   - a sigh: up a step on the "and" of 3, down on beat 4 (50%);
   - a rising pickup into bar 3 (30%);
   - nothing (20%).
   - Not tied: a long chord tone on 1 first.
4. **Bar 3.** The arabesque again, varied by one draw:
   - same 15%; extended +2 notes 25%; landing moved an 8th 20%; 16ths and triplets swapped 15%; whole-tone tetrachord 25% (where legal).
   - Lands 2 scale steps above the anchor (question peak) or 1 below (answer).
5. **Fall.** From the peak, step down through the pentatonic in 8ths from the "and" of 1 (up to 3 notes) to a resting note.
   - Question: about a step under the anchor. Answer: a chord tone about 3 steps under.
   - Then at least 6 16ths of silence.
6. **Motif variation.** Phrase heads after the first take the same variation draw (except whole-tone): re-timed, +1 note, swapped unit, or the next shape.
7. **Density by motion.** About 2.3-3.1 onsets per lead bar measured (the current lead is 2.0-2.75). Motion scales the arabesque length (3 to 5-6 notes) more than the count of long notes.
8. **Register.** The phrase anchor is the key's tonic nearest D5 (or 7 semitones under the bar's ceiling), inside the bar's range.
9. **Velocity.** Arabesque 56→65 rising, landing 80, sigh 70→60, resting note 56-62. Vibes are lifted to 88-99.
10. **Legality.** Every note passes `legal()`. Strong-beat onsets are chord tones. A tie needs legality over both chords. Chords are never changed.

**Not implemented, proposed next:**

- Register arcs across a whole song: the second half of the lead blocks a 3rd higher.
- A once-per-song unaccompanied pickup into the first lead block (the unaccompanied opening).
- A per-mode table of color-note weights tuned by ear.

## 5. Spike results (listening proxies, not a verdict)

Five songs × {current, Impressionism}, plus two with the bed. The full table is in `app/build/new/lead-impressionism-2026-10-29/INDEX.md`. In brief:

| | Current lead | Impressionism lead |
|---|---|---|
| Notes per lead bar | 2.25-2.75 | 2.35-3.06 |
| Range (MIDI) | 68-88 | 71-87 |
| Onsets on beat 1 | 9-42% | 0-11% |
| Onsets off the beat | 9-64% | 53-67% |
| Legato leaps of a 4th+ | 0-7 per song | 0-2 per song |
| Rubs (lead vs keys, bass, pad) | 0 | 0 |
| Velocity | 44-104 (felt hybrid 106-110) | 56-82 (vibes 90-98) |

## 6. Concerns

- **Repetition.** The motif is restated at every phrase head, and VNL-1's chords loop every 4 bars, so bar 1 of each phrase can sound like a loop more than a development. One variation per restatement may not be enough over 20+ lead bars.
- **Beat 1** (section 2): rarely struck. Sam's "vary, not avoid" may apply.
- **The felt piano.** Its current hybrid plays at 106-110. The Impressionism lead plays it at 56-80, so it may sit under the Rhodes. It is a percussive lead, and long ties decay rather than sing: a fit for flute and vibes more than the felt.
- **The narrow range over percussive keys.** With keys topping out near G4, the lead's floor is about C5, so the flute's low register is not reachable. The line lives at about B4-Eb6.
- **Triplet lateness** is applied as sample delay on the 16th grid, so the song's swing does not apply to it. That is probably right for triplets, but unheard so far.
- **Whole-tone** is rare and dominant-only by construction (section 3).
- **Spike quality.** The code has no host tests. Its fallbacks (a note that can't be placed is dropped) can leave a bar sparse. It reuses `phrase_at`, so the same block and pass structure applies.

## Sources

Web analyses of the style's representative works (encyclopedia entries, program and teaching notes, published theory articles and a corpus study), and two papers on algorithmic melody (recurrent networks; Markov models with phrase context). All were read as untrusted data. The citations named the source person and works, so they were removed in the name scrub (2026-10-31, Sam: lead styles go by genre name only); the traits above are limited to what those sources stated.
