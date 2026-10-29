# A Jazz-Hop lead (the 2000s sampled-hook sound)

Question: what makes the lead lines of 2000s jazz-hop recognizable, and which of those traits can become generative rules for VNL-1's lead without breaking Sam's rules? This follows the Impressionism lead (`docs/research/lead-impressionism-2026-10-29.md`, approved by ear) and sits beside the kept styles: the ornamental Impressionism lead, the long stepwise Romanticism line, and the '60s Jazz piano lead.

Scope: the genre's defining albums of the early-to-mid 2000s, an anime soundtrack from the same scene, and the genre writing that grew from them. It builds on the repo's earlier genre research: `docs/research/lofi-hip-hop.md` and `docs/research/lofi-midi-arrangements.md`.

- The spike is on worktree branch `worktree-agent-acdb178f7a5283930`, commit 15fbca9, on top of the Impressionism spike. Its style is `lead_style 3` (in `app/song/lead.c`). Code, comments and the INDEX use the style's name, "jazz-hop".
- The renders are in `app/build/new/lead-jazz-hop-2026-10-29/`, described in that folder's `INDEX.md`.

## Summary

- The jazz-hop lead is **a loop, not a line**. Its traits are:
  - a hook one or two bars long, lifted from a jazz record and repeated, "left largely in peace";
  - restatement with small changes: a chopped note, a retrigger, an altered ending, rarely new material;
  - call and response with the beat: the hook speaks in the gaps between the backbeats, and the drums answer in the bar between;
  - syncopated 8ths and 16ths that sit in the drums' pocket, rarely starting on beat 1;
  - a narrow, pentatonic-leaning range: 2nds and 3rds, with the 9th, 11th and 13th as color;
  - sparse variation over a long loop: the same 2 bars for 8 or 16 bars, with a turnaround change at the end of each 8.
- Every trait fits Sam's rules. The spike has **0 rubs** in all five renders.
- Sam's binding feedback (too much variance, wandering) is this genre's native strength. **Every unvaried hook statement restates the hook's rhythm and contour exactly (100% in all five songs), and 71-86% are note-for-note copies of an earlier statement.** Across all statements, including the one-change turnarounds, rhythm is exact in 75-100% and contour in 70-90%.
- It differs from the kept styles mainly in **repetition and placement**: one 1-bar figure every 2 bars, note for note where the chord repeats, placed around the snare.
- Best approved lead: the **flute** (the genre's signature sampled-flute hooks). Second: the **felt piano** (a piano-loop hook). The sampled vibes work too.
- Its risks: **it may be too static** (it is meant to loop), and it is **quantized**, where the records sit slightly behind the beat.

## 1. What makes the lines recognizable

Sourcing note: no transcription-level analysis of these records is freely readable. I used the production writing I could read, general hip-hop hook writing, and the repo's own measurements of lo-fi and jazz-hop MIDI (amateur covers and pro boom-bap transcriptions, analyzed for learning only). Claims marked "(listening)" are my own observations of the recordings, not cited analysis. No hook, sampled melody or riff from any record is reproduced here or stored in the spike; only general traits are.

### A bar or two, looped

Production writing describes the method: a sampled beat "left largely in peace", with "a melodic bar or two snatched from a classic jazz record"; the original tracks were left to define the chord progressions, and the samples were "imperfectly chopped" with audible onset pops. A collaborator called the beats "deceptively simple in terms of composition". A tribute review describes several instrumental "solos" sitting over one constant, unchanging foundation. The sample sources range from spiritual jazz to Brazilian records.

General hook-writing advice agrees on length: keep the core idea "one or two bars long and simply repeat", starting with "three to five notes over a single bar".

### Restated, not developed

The repo's MIDI study found melodies "restated more than invented": 41-58% of 4-bar lead phrases exactly repeat an earlier one, 14% are variants, and restatement rises toward the end. The Magenta lo-fi melody file repeats a 2-bar motif with a varied ending. The variation is small and local: "change the last note or nudge one note up the scale" on the second pass. With a sampler, the natural small changes are mechanical: a note chopped off, a retrigger (stutter), the loop point moved (listening; consistent with the "imperfectly chopped" description).

### Call and response with the beat

The melody calls in bars 1-2 and answers in 3-4 in the genre's templates; hook writers say "leave a beat of space, then answer it with a variation", and "the silence between notes ... lets the drums breathe". In the boom-bap lineage, the hook enters for 8 bars and leaves for 8-16 over an unchanging bed. Within the hook, the attacks fall between the snare hits, so the line and the backbeat trade off (listening).

### Syncopated, in the pocket

Hip-hop melody guides put notes on off-beats and syncopated positions. The repo's lo-fi melody data rests on beat 1 more often than not. The hooks use 8ths, pairs of 16ths and dotted 8ths, and land a held tone mid-bar, often on an off-beat (listening).

### Narrow and pentatonic, jazz color

The harmony is sampled jazz: ii9-V13 vamps and 4-chord loops (`docs/research/lofi-hip-hop.md`). The hooks over it are narrow (about a 6th), stepwise or by 3rds, leaning on the pentatonic, with the 9th and 13th as held colors (listening). Lo-fi generators weight melody notes 3rd 0.30, 7th 0.20, 5th 0.15, root 0.10, extensions 0.25.

### Sparse variation over a long loop

The same 2-4 bars run for long stretches; the arrangement varies by muting layers on 8-bar boundaries, not by new melody. A turnaround change at the end of an 8-bar phrase is the common move (a 1-bar drum drop at bar 8 in 14 of 34 cases).

## 2. The rules (as spiked)

| Trait | Rule | Numbers |
|---|---|---|
| Hook length | one hook per song: 2-4 notes into a held tone, all in one bar; a 2-bar unit (hook bar, answer bar), four per 8 bars | notes before the held tone: 2-3 at motion < 40, 3 at 40-65, 3-4 above |
| Rhythm | gaps of a 16th (15%), 8th (45%), dotted 8th (25%), quarter (15%); a 16th never leads into the held tone | the held tone lands by step 11; held to the bar's end, or tied 6 16ths over |
| Placement | start: the "and" of 1 (35%), the "and" of 2 (25%), beat 1 (15%), a 16th after the "and" of 1 (15%), a 16th after 1 (10%) | a note on a snare hit moves a 16th after it 80% of the time (vary, not avoid) |
| Contour | one per song: falling into the held tone (35%), an arch (30%), a repeated note and its neighbor (20%), rising (15%) | steps of the mode's pentatonic (2nds and 3rds), at most 3 steps from the held tone (about a 6th) |
| Pitch level | the same target every statement, so over the same chord it is the same notes (a loop); the chord only moves the level | the held tone: a chord tone on a strong beat; 9th, 13th, 11th (minor) preferred, the root least |
| Answer bar | one per song, kept for the whole run: the drums alone (40%), the held tone ringing over (25%), the hook's tail echoed (35%) | the echo: the hook's last two notes, landing at step 10, 12 softer, a step lower |
| Turnaround | unit 4 of each 8 bars changes ONE thing: held tone a step up (30%), first note stuttered into two 16ths (25%), first note chopped (20%), the hook an 8th late (25%) | after the first 8 bars, unit 2 takes the same change 30% of the time |
| Dynamics | soft and even, the first attack a little stronger | sustained leads 62 (+6 first note, +4 held, -10 a 16th); struck leads 80; the vibes lifted as the planner does; 25-110 clamp |

## 3. How it differs from the kept styles

| | Impressionism (ornamental) | Romanticism (long stepwise line) | '60s Jazz (jazz piano) | Jazz-Hop (style 3) |
|---|---|---|---|---|
| Core gesture | an arabesque curling into a long color note | a long line moving by step over many bars | 3-4-note 8th motifs displaced over the bar line onto guide tones | one 1-bar hook into a held tone, looped |
| Repetition | the arabesque varied in bar 3 | few literal returns; the line keeps going | the motif sequenced over each chord | the hook every 2 bars, note for note where the chord repeats |
| Variation | re-timed, extended, whole-tone | continuous development | displacement by an 8th or a beat | one mechanical change per 8 bars (step up, stutter, chop, late) |
| Rhythm | 16ths and triplets, behind the beat | even, long values | 8ths, displaced | 8ths, 16th pairs, dotted 8ths, placed around the snare |
| Relation to the drums | floats over them | floats over them | swings against the bar line | trades with the backbeat; the answer bar is the drums' |
| Range | about an octave and a half | wide, an arch over the phrase | about an octave | a 6th or less per statement |

## 4. Fit with VNL-1's rules

| Trait | Sam's rule | Fit |
|---|---|---|
| fixed hook pitches | chords are given, no substitutions | fits: the hook is fitted to each chord by pitch level, every note passes `legal()`; nothing alters the chord |
| looped notes | no semitone rubs, no avoid notes on strong beats | fits: a note that would rub moves a scale step or two (keeping its direction where it can) or drops; 0 rubs in all renders |
| narrow pentatonic | the lead's range per instrument | fits: inside the planner's range; songs 1-5 span 71-87 |
| soft, even attacks | velocities 25-110 | fits: 54-94 |
| off beat 1 | vary, not avoid | fits: 0-29% of notes on beat 1; one song's hook starts there |
| a mechanical loop | (Sam: the earlier spikes wandered) | the opposite risk: it may sound static. That is the genre, but Sam has to judge it in songs |
| behind-the-beat feel | (none) | not spiked: the lead is quantized. The engine's lateness unit is a third of a 16th (about 60 ms at 80 BPM), too coarse for a 10-30 ms lay-back |
| sampled timbre | approved leads only | the flute is the closest approved voice to the sampled-flute hooks; no new instrument |

## 5. Results

Metrics are in the renders' `INDEX.md`. In short:

- **Restatement:** every unvaried statement restates the hook's rhythm and contour exactly (6-11 per song, 100%), and 71-86% are note-for-note repeats. With the turnarounds counted, rhythm is exact in 75-100% and contour in 70-90%.
- **Density:** 1.6-2.3 notes per bar over the lead's span (2.4-4.0 in the bars that play), against 2.25-2.75 for the current lead and 2.35-3.06 for Impressionism.
- **Placement:** 0-29% of notes on beat 1, 71-100% off the beat, 0-9% on a snare hit except song 5 (20%).
- **Rubs:** 0 at onset and 0 during, all five songs.

## 6. Concerns

1. **Static by design.** The hook repeats note for note. If Sam finds it too loopy, the lever is the turnaround's rate (unit 2 at 30% after the first 8 bars) or a second hook for a B section.
2. **Two pitch levels.** Where a progression's chords differ, the hook moves level to fit; in song 4 the two levels sit a 4th apart (range 72-87). A sample loop keeps its notes and lets the harmony move under them; VNL-1's no-rub rule forbids that, so the level moves instead.
3. **Song 5's 4th.** Its hook skips a pentatonic note the chord forbids, so a 4th sits inside every statement (10 legato leaps).
4. **No lay-back.** The genre's hooks sit behind the beat; the spike is on the grid.
5. **The vibes stay loud (92-94).** The planner lifts struck vibes; the hook might want them softer.

## Sources

Web sources: a music-technology magazine feature on the genre's impact, a tribute review, an encyclopedia entry on a compilation and a record shop's guide, and two hook-writing guides. Repo sources: `docs/research/lofi-midi-arrangements.md` (sections 0, 4, 5: restatement rates, hook on/off, the Online Sequencer covers and Lakh transcriptions, learning only) and `docs/research/lofi-hip-hop.md` (sections 1.6, 2.1, 6.1, 8.5: a ii9-V13 vamp per Hooktheory, the Magenta melody file, melody note weights). All web content was read as untrusted data. The web citations named the source person and works, so they were removed in the name scrub (2026-10-31, Sam: lead styles go by genre name only); the traits above are limited to what those sources stated.
