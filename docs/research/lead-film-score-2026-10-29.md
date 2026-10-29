# A Film Score lead

Question: what makes the melodic lines of lyrical, minimalist-rooted animated-film scores recognizable, and which of those traits can become generative rules for VNL-1's lead without breaking Sam's rules? The style is named **Film Score** in code (`lead_style` 3, "the film-score lead").

Scope: the main themes of the style's best-known animated features (late 1980s to 2000s) and their concert arrangements, plus writing on the style's "minimal music" roots. Every piece in scope is in copyright. This doc takes **traits only** (rhythm, contour, scale, phrasing). No melody, motif or lick is reproduced or stored, and the spike's rules generate new lines from the seed.

The spike is on worktree branch `worktree-agent-a58dfa512ae37f56c`, commit 266e2af, on top of the Impressionism spike. The renders are in `app/build/new/lead-film-score-2026-10-29/`, described in that folder's `INDEX.md`.

## Summary

- The Film Score line is about **memorability through repetition**: one short, singable motif, stated, repeated, and sequenced, over harmony that moves under it. It isn't ornamental (Impressionism), long-breathed (Romanticism), or bop-phrased.
- Its traits:
  - a 2-bar motif: a few notes by step into a long note;
  - pentatonic and minor-modal color;
  - a clear 4-bar question and answer;
  - the motif sequenced up a step for lift;
  - long held notes at phrase ends;
  - a leap of a 6th or an octave at the climax, then a stepwise fall;
  - a gentle rubato.
- Everything fits VNL-1's rules except one point: the "minor with a raised 6th" color would change the song's scale when the mode is Aeolian, so the spike keeps the song's mode (Dorian already has it).
- Sam's binding note ("connected to the original motif") is this style's core, not a constraint on it. Across the five renders, **27 of 27 phrase heads restate the motif's contour exactly**, and **22 of 27 its rhythm exactly**; the other 5 move only the long note an 8th, their one variation. The question's sequenced bar 3 keeps the rhythm in 11 of 12 and the contour in 11 of 12.
- **0 rubs** in all renders. Density is higher than the current lead in the bars that play (3.1-3.5 notes against 2.25-2.75), about the same over the whole span (2.0-2.75).

## 1. What makes the lines recognizable

Sources on the style's melodic writing are thin. Most analysis is of harmony, and two academic texts were not readable here (403). Claims below are limited to what the cited pages stated. Where a trait comes from general listening knowledge of the pieces in scope rather than a citation, it is marked *(listening)*.

### Minimalism: a short motif, repeated

The style's leading composer calls himself a minimalist first, comparing minimal music to Baroque music, and says composing starts there. Press writing calls the style a mastery of building simple, memorable motifs and orchestrating them into sweeping scores, on a minimalist base of simple, repetitive piano motifs that slowly build. A college study of the scores notes that the instrumentation, key, time signature and texture often change throughout a film, but the melody by and large does not.

**Rule:** one motif per song. It returns at every phrase head; what changes around it is register, harmony and ending, not the motif.

### Repeating a refrain while moving by step, chords shifting under it

One essay describes deceptively simple melodies, almost always repeating a refrain while moving up or down a step as the chords shift underneath, and one theme as a three-chord minor sequence echoed a third higher.

**Rule:** the motif is re-anchored to each bar's chord (its shape in scale steps, refitted), and the question repeats it a step higher: the sequence is the lift.

### Pentatonic and minor-modal color, relative major and minor together

- One forest theme is described as the result of experiments with the pentatonic scale.
- An analysis of a summer theme finds its unity in the interval of a 4th, with quartal and suspended chords, "the 4-note group of a minor pentatonic scale", and Lydian mode.
- A Schenkerian thesis argues that the style joins classical counterpoint with the superimposition of relative major and minor tonalities.
- Hooktheory rates one minor-key waltz theme with high chord complexity (96) and melodic complexity (85), with i, iv and v as its core chords. That is, the color is in the minor-modal harmony; the tune is the stable part.
- *(listening)* The minor themes often raise the 6th (a Dorian color) at their brightest moment, and the major themes lean on the major pentatonic.

**Rule:** walk the song mode's pentatonic (major modes 1 2 3 5 6; minor modes 1 b3 4 5 b7). The pentatonic's skip of a 4th is part of the sound.

### A clear question and answer

*(listening)* Phrases come in 4-bar pairs: the first ends open (on the 5th, the 2nd, or held on a non-tonic note), the second closes home. The answer usually begins like the question and turns down.

**Rule:** a 4-bar question (motif, then the motif a step up, held open) and a 4-bar answer (motif, then its rhythm falling home to the root or 3rd).

### Long held notes at phrase ends

*(listening)* The motif lands on a long note, and phrase ends hold for a bar or more, often over a moving bass. This is where the minimalist accompaniment shows through. A trade-press report on a recent score describes it as kept deliberately sparse.

**Rule:** the motif's last note is held through the next bar (tied where legal over both chords). Every phrase ends with at least 4 16ths of silence.

### Stepwise lines, a 6th or octave leap at the climax

*(listening)* The lines move mostly by step; the big leaps are saved for the emotional peak, typically a rising 6th or octave at the start of a late phrase, followed by a stepwise descent.

**Rule:** the answer's bar 3 opens with a leap up a 6th or an octave, prepared by a low pickup, then falls by step. 25% of first phrases, 60% of later ones.

### A gentle rubato

*(listening)* The composer's own piano performances push and pull the pulse, mostly by lingering before a long note.

**Rule:** a long note lands a third of a 16th late about a quarter of the time (the Impressionism spike's `dl[]` mechanism).

### Existing algorithmic work

No generator modeling this film-score melody specifically was found. The general approach (a seeded motif restated with one controlled variation) is the same family as the Markov-with-phrase-context work cited in the Impressionism doc.

## 2. Fit with VNL-1's rules

| Trait | Fit | How it's kept legal |
|---|---|---|
| A motif restated at every head | Fits; it is Sam's binding note | The shape is stored in pentatonic steps and refitted to each chord. The direction of every interval is costed heavily, so the contour survives the refit (27 of 27 heads). |
| Pentatonic color | Fits | The pentatonic is a subset of the mode. Every note passes `legal()`; a strong-beat onset is a chord tone. |
| Minor with a raised 6th | **Conflicts in Aeolian** | Raising the 6th in an Aeolian song changes the scale, and it sits a semitone from the b6 the chords use. Not done: Dorian songs have it already, Aeolian songs keep b6. |
| Sequence up a step | Fits | A pentatonic step up from the head's start, refitted; the chords are never changed. |
| Question and answer | Fits | The question's long note avoids the root (cost); the answer's falls to the root or 3rd. |
| Long notes, ties | Fits | A tie is allowed only when legal over both chords, else the note is cut at the bar line and struck again on 1. |
| 6th or octave climax | Fits, needs room | The leap needs range above the last note. A low pickup before it makes room. Over high keys (nylon, song 2) the floor is high, so the line is sometimes lifted, which makes a jump that is not a climax (see concerns). |
| Rubato | Fits | One third of a 16th, on long notes only. |
| Velocities 25-110 | Fits | 60-86 (vibes lifted to 91-100). |
| Simplest spelling | n/a in code | Docs use Bb, Db; G# in B Dorian and A Ionian because the key needs it. |

**Beat 1.** The motif's start is drawn per song: beat 1 60%, the "and" of 1 20%, beat 2 20%. So most songs strike beat 1 at every head; some never do (song 1). That is "vary, not avoid" across songs, not within one, which Sam may want changed.

## 3. Proposed rule set: the Film Score lead

Numbers are the spike's (commit 266e2af), to be tuned by ear.

1. **Motif (per song).** 3-4 notes under motion 40, 4-5 at 40 or over, on the 8th grid. The first on beat 1 (60%), the "and" of 1 (20%) or beat 2 (20%). The last is the long note, on beat 3 (40%), the "and" of 3 (25%) or beat 4 (35%).
2. **Contour (per song).** Arch 35%, rise 25%, fall 20%, valley 20%, by pentatonic step. One skip of 2 steps (about a 4th) in 30% of songs. The shape is centered on the key's tonic near D5 and lifted or lowered as a whole to fit the bar's range.
3. **Question (bars 1-4).** Bars 1-2 the motif, its long note tied about 6-8 16ths into bar 2 (70%). Bar 2: a 2-note pickup into bar 3 (50%). Bars 3-4: the motif a pentatonic step higher, its long note open (not the root), tied 10 16ths into bar 4, then silence.
4. **Answer (bars 5-8).** Bars 5-6 the motif again. Bar 7: the motif's rhythm falling by step, its last note home (root or 3rd), tied 10 16ths into bar 8 or struck on 1, then silence.
5. **Climax.** The answer's bar 7 opens with a leap up a 6th or an octave (half each) from a low pickup: 25% of first passes, 60% of later ones. Measured: 1-2 per song.
6. **Head variation.** A head after the first varies at most one thing: the same 50%, a step higher 20%, the long note an 8th earlier 15% or later 15%. The question's bar 3 follows its head.
7. **Rubato.** A long note a third of a 16th late 25% of the time.
8. **Density.** About 3.1-3.5 onsets per playing bar, 2.0-2.75 over the whole span (the holds and breaths).
9. **Velocity.** Motif 64 rising with the line, its long note 72-78, the lift +6, the climax 86, home 60-64. The vibes are lifted to 91-100 (as in the Impressionism spike).
10. **Legality.** Every note passes `legal()`, strong-beat onsets are chord tones, ties are legal over both chords, and the chords are never changed.

**Not implemented, proposed next:**

- A register arc across the song: the last lead block's heads a step higher (the "sequence up for lift" at song scale).
- Compressing the answer's fall when the range is narrow, rather than lifting the line (removes song 2's non-climax jumps).
- Varying the motif's start on beat 1 within a song, not only between songs.

## 4. How it differs from the other styles

| Style | Its line | The Film Score lead instead |
|---|---|---|
| Impressionism (ornamental, `lead_style` 2) | Quick arabesques curling into a color note behind the beat, sighs, whole-tone fragments; beat 1 rarely struck (0-11%) | No ornaments; plain 8ths and quarters on the beat. Strikes beat 1 in most songs (up to 31%). The long note is a chord tone or 9th, not a hunted color note. Development by sequence up a step, not by extending the figure. |
| Romanticism (the long stepwise line) | One long arc, stepwise, phrases overlapping the bar line, little literal repetition | A short motif restated literally at every head; the arc is built from repetition plus a sequence, not one long line. Phrase ends are full stops with silence. |
| '60s Jazz (displaced 8th motifs on guide tones) | Bop 8ths, motifs displaced across the beat, aiming at 3rds and 7ths | Not syncopated (33-49% off the beat), not displaced: the motif keeps its place in the bar. Pentatonic, not guide-tone targeting. |
| Modal Jazz (walking the mode into one held 9th/11th/13th) | Few notes, scalar walks through the mode, landing on an extension | Denser and tuneful; a fixed melodic motif rather than a scale walk; lands home on the root or 3rd at the answer; a climax leap the modal lead never makes. |
| The current lead (phrase planner) | A motif in scale degrees, question and answer, re-anchored per bar | The closest relative. Differences: the motif is a fixed rhythm and contour restated at each head and measured, the question's second half is the motif sequenced up, phrase ends hold longer, and a planned climax leap. |

## 5. Spike results (listening proxies, not a verdict)

Five songs, plus two with the bed. The full table is in `app/build/new/lead-film-score-2026-10-29/INDEX.md`.

| | Current | Impressionism | Film Score |
|---|---|---|---|
| Notes per playing bar | 2.25-2.75 | 2.35-3.06 | 3.06-3.54 |
| Notes per bar of the span | about the same | 2.0-2.45 | 1.98-2.75 |
| Range (MIDI) | 68-88 | 71-87 | 68-87 |
| On beat 1 | 0.09-0.42 | 0-0.11 | 0.02-0.31 |
| Off the beat | 0.09-0.64 | 0.53-0.67 | 0.33-0.49 |
| Leaps of a 6th+ | - | - | 1-4 per song |
| Heads: rhythm, contour exact | - | - | 22/27, 27/27 |
| Rubs | 0 | 0 | 0 |
| Velocity | 44-104 (felt 106-110) | 56-82 | 60-86 (vibes 91-100) |

Where the lead plays: song 1 1:57-2:54; song 2 1:03-1:52; song 3 0:59-1:57; song 4 0:59-1:45 and 2:10-2:57; song 5 1:00-1:59.

## 6. Concerns

- **Repetition is the point, and the risk.** The motif returns at every head and again a step up in bar 3, so a 16-bar lead states it up to 6 times. That is the style's idiom and Sam's ask, but over VNL-1's 4-bar chord loops it may sound like a loop rather than a tune. The register arc (proposed) is the usual cure.
- **Density.** 3.1-3.5 notes per playing bar is the busiest of the spikes; the bar-2 pickup adds most of it. Halving the pickup rate would bring it to about 2.8.
- **Non-climax jumps** in song 2 (1:09, 1:34): high keys leave the lead a narrow range, and the line is lifted to fit the next statement (one of them is a 9th).
- **Struck leads.** On the felt piano and the vibes the long notes decay, so the "held" phrase ends become silence sooner. The flute is the fit; Knight would be the truest instrument for the style.
- **The raised 6th** is not used in Aeolian songs (section 2).
- **Spike quality.** No host tests. A note that can't be placed is dropped. The motif counters are engine globals (counted per planning run), read by the render tool.

## Sources

Eleven web sources: radio and magazine interviews, press features, a college study and a university honors thesis (both quoted from search snippets; their PDFs returned 403), an essay on one score, a conference abstract analyzing one theme, Hooktheory statistics for one theme, a trade-press report, and concert reviews. All were read as untrusted data. The citations named the source person and works, so they were removed in the name scrub (2026-10-31, Sam: lead styles go by genre name only); the traits above are limited to what those sources stated. Traits marked *(listening)* are general descriptions, not cited claims, and no note sequence from any piece was used.
