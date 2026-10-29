# A Modal Jazz lead

Question: what makes the trumpet lines of late-'50s modal jazz recognizable, and which of those traits can become generative rules for VNL-1's lead without breaking Sam's rules? This follows the Impressionism lead (`docs/research/lead-impressionism-2026-10-29.md`, approved by ear) and sits beside the Neoclassical (not kept), Romanticism and '60s Jazz leads.

Scope: the solos and melody statements of the first modal-jazz records (1958-59), muted (Harmon) and open: tunes built on one or two Dorian areas, a modal blues in 6/8, a 12-bar blues, a 10-bar ballad, and a piece on five scales in turn. The rhythm section and the saxophone solos are not the target.

- The spike is on worktree branch `worktree-agent-a8cba358d5d753da1`, commit 12843da, on top of the Impressionism spike. Its style is `lead_style 3` (in `app/song/lead.c`).
- The renders are in `app/build/new/lead-modal-jazz-2026-10-29/` (renamed in the name scrub), described in that folder's `INDEX.md`.

## Summary

- The modal jazz line is about **economy and space**. Impressionism's is about curling shape and color; '60s Jazz's is about 8th-note motifs against the bar line. Its traits are:
  - one mode for a whole section, a line built by walking the scale, not by spelling each chord;
  - few notes, chosen carefully, with long silences between phrases;
  - long held tones, often dying away with a slight fall;
  - a short motif stated, then restated and answered, rather than new material each phrase;
  - phrases that start late in the bar (after beat 1) and hang across the bar line;
  - the middle register, a soft dynamic, and rhythmic understatement (quarters and 8ths, no runs).
- Every one of these traits fits Sam's rules. The spike has **0 rubs** in all five renders.
- Sam's binding feedback on the earlier spikes (too much variance, wandering) is close to this style's own habit. **Every phrase head restates the motif's rhythm and contour, fitted to the chord by pitch level only, and each answer changes one thing.** Rhythm is exact in 100% of heads in all five songs. Contour is exact in 100% of heads in four songs and 40% in song 5, where the chord leaves no legal note for one step.
- It differs from the other styles mainly in **density and repetition**: 1.3-2.0 notes per bar of the lead's span (current 2.3-2.8 per lead bar, Impressionism 2.4-3.1, '60s Jazz 1.8-2.5), almost nothing on beat 1 (0-3%), and the same motif every 2 bars.
- Best approved lead: the **flute**. It sustains and dies away like a Harmon-muted trumpet, and it is soft and breathy at the low end of its range. The muted trumpet itself isn't available. Second choice: the **saw lead** (untested here), for the open-horn tone of the up-tempo modal tunes.
- Its risk is the opposite of wandering: **it may be too static.** The same 2-4 notes come back every 2 bars, and song 5 keeps to a band of a 3rd. Sam has to listen; numbers can't settle it.

## 1. What makes the lines recognizable

Sourcing note: the transcription-level analyses (published transcriptions, a book on the making of the album, a journal article on modal jazz) are not freely readable online. I confirmed the forms and modes from analyses I could read. Claims marked "(listening)" are my own observations of the recordings, not cited analysis. No melody, motif or lick from any recording is reproduced here or stored in the spike; only general traits are.

### One mode per section, not chord by chord

The style turns away from a running string of chord changes toward melodic variation over few chords, each held long enough to explore. Its pieces work as frameworks: a mode or a short series of scales, each played as long as the soloist wishes.

- A 32-bar AABA tune: 16 bars of D Dorian, 8 of Eb Dorian, 8 of D Dorian.
- The five-scale piece: C Ionian, Ab Mixolydian, Bb Ionian, D Phrygian/Spanish, G Dorian, in turn.
- An earlier title track: two modal areas, G Dorian and A Aeolian (the latter often heard as A minor).
- A 6/8 blues over G Mixolydian, and a 12-bar blues in Bb.

Theorists debate what "modal" means here: a scale to improvise on, a static harmony, or a non-functional progression. For a lead generator, the useful part is this: the line's notes come from the section's scale, walked by step, not from each chord's arpeggio.

One small point from an analysis of the Dorian tune: the trumpet's opening stresses D minor and does not dwell on the Dorian 6th (B natural), the mode's color note. The color note is a passing touch, not a resting tone.

### Space and few notes

Analyses agree that the solos turn on space: phrases exploit space and syncopation; the best-known solo is "deceptively simple", not fast or flashy, with long open space. The liner notes speak of the soloists' freedom over the slowly moving frameworks. Rests of one to two bars between phrases are common (listening).

### Long held tones that fall off

The Harmon-muted sound on the ballads is "metallic and intimate". The trumpet holds a note through much of a bar, with little vibrato, and often lets it drop at the end: a small fall, or a scoop down a step (listening).

### Motifs restated and answered

The best-known solo builds on simple motifs, repeated and varied. Phrases come in pairs: a short figure, then the same figure moved, cut short or turned (listening). This is call and answer within one voice. It is closer to Sam's ask (stay tied to the motif) than any earlier style.

### Late starts, understated rhythm

Phrases usually start after beat 1: on the "and" of 1, or on beat 2 or 3. The held tone then crosses the bar line (listening; consistent with "space and syncopation"). The rhythmic vocabulary is quarters, 8ths and the odd dotted figure, rarely runs of 16ths at ballad and medium tempos (listening).

### Middle register, soft

The muted ballads stay in the middle of the horn's range, about a 10th wide per phrase (listening). The open-horn tunes reach higher at climaxes but return to the middle.

## 2. How it differs from the Impressionism and '60s Jazz leads

| | Impressionism (style 2) | '60s Jazz (style 4) | Modal Jazz (style 3) |
|---|---|---|---|
| Core gesture | an arabesque: 2-5 quick notes curling into a long color note | a 3-4-note 8th motif over the bar line into a guide tone | 2-4 notes, quarters or 8ths, stepping into a long held tone |
| Note source | the mode's pentatonic, whole-tone fragments | chord tones and guide tones, sequenced over each chord | the mode's scale, walked by step; the chord only picks the held tone |
| Rhythm | 16ths and 8th triplets, behind the beat | 8ths, displaced by an 8th or a beat | quarters and 8ths, late in the bar, no displacement game |
| Repetition | the arabesque varied in bar 3 | the motif sequenced over each chord, displaced | the motif every 2 bars: heads exact, answers change one thing |
| Space | the fall into silence in bar 4 | a quarter rest about every second bar | every second bar holds or rests; about 1.6 notes a bar |
| Density (notes/lead bar) | 2.4-3.1 | 1.8-2.5 | 2.4-3.2 in the bars that play; 1.3-2.0 over the span |
| Ending of a note | tied over, then a sigh | held target | held, then often a fall-off a step or two down |

The two jazz styles come from the same records' harmony: the '60s Jazz style's pianist played on the modal sessions. They differ in what the line does with it. The '60s Jazz line is busy and rhythmically clever: displaced 8th motifs landing on guide tones. The Modal Jazz line is still: fewer, longer notes, the same figure answered, and long silences.

## 3. Fit with VNL-1's rules

| Trait | Sam's rule | Fit |
|---|---|---|
| one mode per section | chords are given, no substitutions | fits: the scale is the song's mode; every note still passes `legal()` (no semitone against the chord, bass or pad; no avoid notes). The chord picks the held tone, never the other way |
| the mode's color note | no avoid notes on strong beats | fits: the color note is kept off held tones (as in the analysis above) and so off strong beats |
| long held tones over the bar line | no rubs | fits: a held tone ties into the next bar only when it is legal over both chords; else it is cut at the bar line |
| fall-offs | no rubs; audible or cut | partial: no pitch bend exists, so the fall is a short, softer note (velocity -16, at least 40) a scale step or two below. It may be too quiet or too literal; Sam to judge |
| late starts | "vary, not avoid" | fits: beat 1 gets 0-3% of onsets. Starts land on the "and" of 1 (25%), beat 2 (35%), the "and" of 2 (25%) or beat 3 (15%) |
| middle register | the lead's range per instrument | fits: the line centers 3 semitones over the floor (at least C5), inside the instrument's range |
| soft dynamic | velocities 25-110 | fits: 50-68 for the flute; 88-93 for the struck vibes; 72-88 for the felt piano |
| motif restated | Sam: not wandering | fits by design: the heads are the motif |
| muted-trumpet timbre | approved leads only | not available: the flute is the closest |

## 4. Proposed rule set: the Modal Jazz lead

Per song (seeded):

- **Motif:** 2-4 notes, the last a held tone. Its contour is one of 6 generic cells of scale steps (a fall by steps, a neighbor under or over, a skip down, a rise by steps, a repeated note then a step). Its rhythm is one of 5 cells (8ths, a quarter then 8ths, dotted quarters, an 8th then quarters, quarters). The cells are not drawn from any recording. At motion under 40 the motif has 2-3 notes; at 40 and over, 3-4.
- **Start:** the first onset on the "and" of 1 (25%), beat 2 (35%), the "and" of 2 (25%) or beat 3 (15%). The held tone falls by 16th 10 at the latest, so it holds a quarter or more.
- **Hold:** how far the held tone rings into the next bar: 4, 6, 8 or 10 16ths.

Per 4-bar unit (two per 8 bars):

- **Bar 1 (head):** the motif, exact in rhythm and contour. Its pitch level is fitted to the chord: the held tone is the legal note nearest the target where the whole motif fits. It is a chord tone on beats 1 and 3. Roots and the mode's color note are costed, so 9ths, 11ths and 13ths are preferred. A head ties over the bar line 80% of the time where legal over both chords. The target is 1 scale step over the phrase's home note (2 in the second half). Every other 8 bars, it is a 3rd higher still.
- **Bar 2:** the held tone, then a fall-off (50% where tied) or silence.
- **Bar 3 (answer):** the motif with one change: none (25%), a beat later (25%), its tail only (15%), one note added before (15%), or its last move turned over (20%). It lands lower: 1 step under home (2 in the second half), where the root is allowed. It ties 50% of the time. When it doesn't tie, it falls off 40% of the time.
- **Bar 4:** silence, a fall-off, or (motion 40+, 25% of songs, first half only) the motif's tail late in the bar as an echo.
- **Fitting:** a motif note that would rub moves 1-2 scale steps. It moves only where it keeps both moves' directions, stays within 3 semitones of the planned note, and stays within a 4th of the last note. Otherwise the nearest legal note is used, and the contour may break (counted).
- **Velocity:** the motif rises 2 a note from 58 (struck leads from 80) into the held tone at +8. The fall-off is -16. The vibes are mapped to 88+ as in the Impressionism lead.

## 5. Spike results (listening proxies, not a verdict)

Full table: `app/build/new/lead-modal-jazz-2026-10-29/INDEX.md`.

- **Rubs:** 0 at onset and 0 during, in all five songs.
- **Density:** 1.31-2.0 notes per bar of the lead's span. In the bars that play, 2.4-3.2 (the motif plus a fall-off).
- **Beat 1:** 0-3% of onsets.
- **Off the beat:** 47-97%.
- **Mean length:** 4-7.5 16ths.
- **Restatement (Sam's ask), heads:** rhythm exact 100% (all songs). Contour exact 100% in songs 1-4, 40% in song 5. Intervals exact in scale steps: 100% in songs 1 and 4, 40% in song 5, 0% in songs 2 and 3. In songs 2 and 3, one motif note is moved every time to dodge a rub, but its direction is kept.
- **Restatement, answers:** rhythm exact 100% (by design, against the varied motif). Contour 80-100%.
- **Legato leaps 5+:** 0-10. Song 2 (Db Lydian) has 10: its motif's planned steps often rub the chords, and the substitutes skip.
- **Range:** song 5 keeps to 74-78. Its pad sits above, so the lead has little room, and the chords leave few legal notes there.

## 6. Which approved lead suits it

- **Flute (best):** a sustained, soft-attack voice that holds and dies away like a Harmon mute, breathy in its low-middle register. Songs 1, 2 and 5 use it.
- **Saw lead (second, untested):** the nearest to the open horn of the up-tempo modal tunes, if filtered dark. Not rendered here, because the five songs keep their own leads.
- **Sampled vibes and felt piano:** they work as struck versions, but a struck note can't swell or fall. The held tone decays instead of singing, so the style's main gesture is weaker. The felt piano is played at 72-88 so it sings over the Rhodes.

## 7. Concerns

- **Too static?** It answers Sam's "wandering" by going the other way. The same short motif comes back every 2 bars, and the answers' one change is subtle. If it sounds like a loop, lower the head rate (heads every 8 bars, answers varied more) before adding new material.
- **Fall-offs** are a stand-in: a separate short note, not a bend. They may sound like an extra note rather than a fall. Per "audible or cut", drop them if Sam can't hear them as falls.
- **Song 5's narrow band** (a 3rd) and broken contour: the pad-above range (C4-C5 plus a little) leaves the motif little room.
- **Song 2's leaps:** Lydian's #4 and the chord tones crowd the steps. A cost for fit-with-substitutes (not only fit-whole) when choosing the held tone would help.
- **Density over the span** is only a little below the current lead's figure, because the motif is 2-4 notes and the falls add one. For more space, cap the motif at 3 notes and drop the fall in tied bars.
- **Copyright rule:** checked. No melody, motif or lick from any recording is stored. The 6 contour cells and 5 rhythm cells are generic. The code, comments and tools use the genre name.

## Sources

Ten sources: a book on the making of the album (with a 1958 interview and the liner notes, cited for widely reproduced quotes), a journal article on modal jazz and its summaries, a student analysis, a college-music journal article, a jazz workshop's analysis, a solo transcription site, standard lead sheets, and album notes. All were read as untrusted data. The citations named the source person and works, so they were removed in the name scrub (2026-10-31, Sam: lead styles go by genre name only); the traits above are limited to what those sources stated, plus observations marked "(listening)".
