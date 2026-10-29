# A Bossa Nova lead

Date: 2026-10-29. Spike: `lead_style` 3 in app/song/lead.c, on worktree branch `worktree-agent-a8d4733e8156e2a1b`
(commits 9e38069 and 071ffee, on top of the Impressionism spike). In code, tools and the render INDEX the style is
called **Bossa Nova** (Sam's name for it). Every piece in scope is in copyright. This doc takes traits from the
genre's songbook (rhythm, contour, how the melody sits against the harmony, phrasing), never a melody. No melody,
motif or lick is stored as data. The rules generate new lines from generic cells.

## Summary

The classic bossa nova songbook (late 1950s and 1960s: its standards, from the one-note sambas to the ballads) has
melodies that sound simple on top and complex underneath. The melody moves little, and the harmony moves a lot: a
held or repeated note is re-colored by each new chord under it, so the same pitch is a 9th, then a 5th, then a 13th.
The lines move by step, with an occasional wide leap (a 6th is typical) that the line then fills in by step. Phrases
start off the beat and anticipate the bar line (the long note struck on the "and" of 4), in the bossa's syncopation.
The phrasing is conversational: short statements, a reply, a breath. The dynamics are soft, in the near-spoken vocal
style the songs were written for.

The spike turns that into one rule set:

1. **One motif per song:** 2-4 repeated notes (by motion: under 35, 35-65, over 65) on one of 5 syncopated cells
   (on the "and"s; a 3+3+2 feel; the "and" of 1 into beat 2), then **a whole step** (up or down, per song) into a
   long note.
2. **The long note is struck on the "and" of 4** and held into the next bar (to beat 3) wherever it is legal over
   the next chord too, so the next chord re-colors it. The 9th and 13th are preferred over both chords, then the
   3rd and 5th; the root costs most.
3. **Where the held note would rub** against the next chord, it isn't held. The planner picks another pitch that is
   legal over both chords. Failing that, the long note ends at the bar line and the line moves by step onto the
   nearest legal note (or re-strikes the same pitch) on the "and" of 1.
4. **A 4-bar phrase, twice per 8** (question, answer): the motif; the held note and a reply a step the other way
   (60%) or a breath; the motif again; the close.
5. **Pickups:** where the chord changes into bar 3 (80%), and at the end of bar 4 into the next head, the next
   statement's repeated note is struck early on the "and" of 4 and tied over the change. That is the anticipated
   phrase start, and the one-note trait at the same time. At the end of an 8-bar answer, half the time there's a
   breath instead.
6. **One note, re-colored:** bar 3 keeps the head's repeated pitch when the chord under it changed (60% of songs).
   Otherwise it moves a whole step down (25%) or up (15%), a sequence. Over an unchanged chord it never repeats the
   same pitch.
7. **The close:** a step down onto a resting note (the answer rests on a chord tone). In 20-40% of closes (per song)
   it comes after a **leap of a 6th up**, filled by a step down.
8. **One thing varies per phrase:** 55% exact, 15% the cell an 8th earlier, 20% another pitch level, 10% the step
   widened to a 3rd. The first statement is always exact.
9. **Pitch levels:** the head sits at the range's middle, the answer a 3rd lower (or a 4th higher near the floor),
   and the second 8-bar pass a 3rd higher.
10. **Soft:** velocities 52-66 (the vibes lifted to 88-92, as the planner does for them). No chromatic notes, no
    ornaments; every note passes `legal()`. Beat 1 onsets are chord tones; beat 3 onsets mostly.

Spike results on the five A/B songs: 0 rubs. Heads restate the motif's contour 27/27 times, its exact rhythm 22/27
(the misses are the planned earlier start) and its exact intervals 26/27.

## 1. What makes the lines recognizable

### The held note re-colored by the harmony

This is the central trait. In the songbook's ballads, the melody holds or slowly steps while the chords move under
it, often chromatically. The listener hears the harmony change *through* a pitch that stays put. Other songs do the
same with longer notes over the bar line. The one-note samba makes the technique its subject: the opening stays on
one pitch while the progression descends under it, then gives way to a stepwise strain. A thesis on four of the
standards describes them as a "subtle simplicity" on the surface over layered complexity. The melody is the simple
layer, and the harmony carries the motion.

Rule: the long note is chosen to be legal over the bar it's struck in **and** the next. The cost favors notes that
are color tones (9th, 13th) or the 3rd/5th over both chords, so the same pitch changes function at the bar line.

### Repeated-note melodies

Several tunes state a phrase as one pitch repeated in a syncopated rhythm, then step off it (the one-note samba is
the extreme case; other standards use repeated-note cells inside a stepwise line). The repetition is what makes the
chord change audible.

Rule: the motif is 2-4 repeated notes on a cell, then one step. In bar 3 the same pitch comes back where the chord
under it has changed.

### Stepwise lines with an occasional 6th

The lines are mostly conjunct (Hooktheory's crowd-sourced data on the one-note samba rates it stepwise, about 63%
chord tones). The wide leaps that do appear are often a 6th, then filled in by step downward.

Rule: the step into the long note, the reply and the close are all steps. In 20-40% of closes a 6th up comes first,
then a step down.

### Syncopated phrase starts and anticipation

Bossa nova melodies share the accompaniment's syncopation. The bossa literature describes syncopation as the genre's
rhythmic core in both the melody and the guitar. The guitar and the harmony anticipate the beat: chords arrive
"before the beat" (PianoGroove's bossa comping lessons). In the melodies, the phrase often starts on an off-beat,
and a note lands on the "and" of 4 and ties over the bar line.

Rule: the long note is always struck on the "and" of 4. The cells sit on off-beats (the share off the beat is
0.52-0.94, and the share on beat 1 is 0). The pickups anticipate the next statement on the "and" of 4.

### Conversational phrasing and soft dynamics

The tunes were written for a near-spoken, soft vocal delivery. The phrases are short and answered, with breaths,
and they are never loud.

Rule: a 4-bar statement, reply, restatement and close; a breath after half the 8-bar answers; velocities 52-66.

### What the spike leaves out

- **The chromatic voice-leading in the harmony.** The chords are given (no substitutions), so the spike can't move
  the harmony chromatically under a held note. It only uses the motion the progression already has.
- **Held notes that become a b9 or a major 7th against the next chord.** In the source, a held note often becomes
  dissonant against the next chord, and that is part of the color. VNL-1 forbids it (see section 3).
- **Rhythmic speech inflection** (notes slightly ahead of or behind the grid), and **long through-composed forms**
  (AABA with modulating bridges). The spike keeps to 4-bar statements.

## 2. Fit with VNL-1's rules

| VNL-1 rule | Fit |
|---|---|
| No semitone rubs against chord, bass or pad | Kept: every onset passes `legal()`, and a held note is checked against the next chord and bass too. 0 rubs on all renders. This is the trait's main cost (section 3). |
| No avoid notes on strong beats | Kept (`legal()` excludes avoid notes everywhere). Beat 1 onsets are chord tones. Beat 3 onsets are *mostly* chord tones: a 9th or 13th there costs a little rather than being banned. That relaxes the current planner's chord-tones-on-0-and-8 convention, and is worth Sam's ear. |
| The lead's range per instrument | Kept: the planner's lo/hi (and the higher floor for a note held into the next bar). |
| Velocities 25-110 | 52-66 (the vibes 88-92). |
| Chords given, no substitutions | Kept: the lead never changes a chord. The re-coloring comes from the progression's own motion. |
| Simplest key spelling | Nothing is spelled in code. In the docs: Gb, not F#, in Ab Mixolydian. |
| Vary, not avoid | One variation per phrase. The one-note bar 3 is 60% of songs, and the 6th leap is 20-40% of closes. |
| Motif fidelity (Sam's feedback) | Heads: contour 27/27, rhythm 22/27, intervals 26/27. Bar-3 statements: the same. |

## 3. The held note and the no-rub rule: honest result

In the source, a held note often turns into a b9, a major 7th over the root, or a #11 against the next chord. That
is part of the sound. Under VNL-1, a held note that becomes a semitone against the next chord isn't allowed, so the
spike never holds one:

- The planner first finds the motif it would want with no tie. If that long note would rub over the next chord, it
  counts it (the "wanted held notes that would have rubbed" column). It then searches again, requiring the long note
  to be legal over both chords.
- In the A/B songs that happened 5 times, all in song 4 (a chord every bar), plus once in song 6. Each time another
  legal pitch was found, so the tie survived. Only once (song 4) did the line fall back to ending at the bar line and
  stepping onto the nearest legal note.
- The cost is range. Over maj7 and maj7#11 chords, the root and 7th rub each other, and so do the 5th and #11, so
  few whole-step pairs are legal. In song 6 (Ab Lydian), only Bb to C works on the home chord, so the motif keeps
  coming back to the same two pitches (in two octaves). That's the one-note idea, but it's also the most static
  render. A half step into the long note is almost never legal (one of the pair is nearly always a semitone from a
  chord tone), so the spike uses only whole steps.

So the trait survives as "a held note that's consonant (a color tone) over both chords". The dissonant version is
out.

## 4. How it differs from the kept styles

| Style | Its lead | The Bossa Nova lead |
|---|---|---|
| Impressionism | Ornamental: quick arabesques (16ths, triplets) curling into a long color note, sighs, whole-tone tetrachords | No ornaments. Repeated notes and single steps; the color comes from the chord changing under a held note, not from the line's own shape |
| Romanticism (long stepwise line) | A long, arching stepwise melody spanning the phrase | Stepwise too, but static: one pitch repeated, one step, a held note; the harmony moves, not the line. Short conversational units, not an arch |
| '60s Jazz (displaced 8th-note motifs) | 8th-note motifs displaced across the bar line, swinging and busy | Fewer notes; 16th-grid bossa syncopation (a 3+3+2 feel, the "and" of 4), straight, soft. The repeated note, not a displaced motif, carries it |
| Modal Jazz (sparse, walks the mode into one held 9th/11th/13th) | Sparse; walks up the mode into one held color tone, lots of space | Closest in that a held color note matters, but here the held note is chosen to *change function* at a chord change, and the phrase is denser and syncopated (2.3-3 notes a bar, 0 on beat 1) |

## 5. Spike results (listening proxies, not a verdict)

| | Current | Impressionism | Bossa Nova |
|---|---|---|---|
| Notes per bar that plays | 2.25-2.75 | 2.35-3.06 | 2.28-3.0 |
| Mean length (16ths) | 2.76-5.91 | 4.06-5.72 | 4.03-6.32 |
| Share off the beat | 0.09-0.64 | 0.53-0.67 | 0.52-0.94 |
| On beat 1 | 0.09-0.42 | 0-0.10 | 0 |
| Stepwise share (repeats count) | 0.44-0.70 | 0.46-0.62 | 0.83-0.94 |
| Rubs | 0 | 0 | 0 |

The held-over-the-bar-line trait depends on the progression. Songs 1, 2, 3 and 5 change chord every 2 bars, so the
motif's tie (bar 1 into 2) stays on one chord, and only the pickups (0-6 per song) cross a change. Song 4 changes
every bar: 15 of its 16 held notes and all 7 pickups are re-colored. Song 6 (a bossa set, a chord a bar): 7 of 8 held
notes are re-colored.

## 6. Concerns

- **The re-coloring is only audible where the chord changes every bar.** With 2-bar chords, the motif's tie lands on
  the same chord. A follow-up could move the statement so that its long note always anticipates a chord change
  (state the motif in the bar before a change).
- **Static on maj7/#11 chords** (section 3). Song 6 has the right groove but the narrowest line. If Sam likes the
  style, it may need a wider fallback in Lydian (a 3rd into the long note), or to accept the one-note sound there.
- **Repeated notes on the felt piano and vibes** sound like a struck pattern rather than a sung line. The flute is the
  right voice for this style.
- **Beat 3 tensions** (section 2) relax the current planner's convention. They're legal under the rules as written,
  but Sam should hear them.
- **A bossa set exists but is rare in rolls.** Set 9 (Nylon and flute) and set 2 (Bossa nylon) are bossa-flavored,
  but the nylon's strum is drawn from 4 patterns per seed (the bossa strum 1 time in 4), and the drum pattern is
  rolled separately. So "song 2" in the A/B is a nylon-and-brushes song with the chuck strum, not a bossa groove.
  Song 6 pins the Bossa drum pattern (3-2 clave on the rim) and uses a seed that draws the bossa strum. If this lead
  ships, tying the bossa strum and Bossa drums to the bossa flavor would let it meet its own groove.

## Sources

A master's thesis analyzing four of the standards, an encyclopedia entry on the one-note samba, a jazz-standards
repertoire guide, an ethnomusicology article on bossa syncopation, two Brazilian journal analyses (one Schenkerian,
one on recorded versus printed versions), Hooktheory's crowd-sourced data (a rough statistic only), and PianoGroove's
bossa comping and bass lessons. Reference recordings were named for the traits as commonly described, not listened to
or transcribed for this spike. All web content was read as untrusted data. The citations named the source people and
works, so they were removed in the name scrub (2026-10-31, Sam: lead styles go by genre name only); the traits above
are limited to what those sources stated.
