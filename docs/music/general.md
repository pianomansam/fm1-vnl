# General musicianship (genre-independent)

Principles that make music sound played by a good musician in any style. If VNL-1 were a jazz trio, a pop band or
a film cue, these would still hold. Genre and taste settings (how much swing, which chords, how long a song is) live
in [lofi.md](lofi.md); ideas not adopted yet live in [ideas-from-other-apps.md](ideas-from-other-apps.md).

**Refreshed 2026-10-30 against the code** for the rule-based parts: harmony, keys voicing, bass, drums and the pad
layer. Each rule is written **rule → its numbers**. **Not refreshed:** the melody and lead rows (section 6 and the
lead rows of section 7). The lead engines are being rewritten as lead styles; those rows are kept as of 2026-10-24
and marked. Where the code and the ledger disagree, the case is listed under "Open questions for Sam" at the end,
not fixed.

Status: **Done** (file: function), **Partial**, **Not yet**, **Removed** (was in the 10-24 doc, no longer true).
Sources: `docs/research/` (short names below), the plan ledger's rulings (R*n*), commits (short SHA), or **(Sam)**
for decisions Sam made after listening.

Research short names: *genre* = `lofi-hip-hop.md`, *audio* = `lofi-audio-arrangements.md`, *midi* =
`lofi-midi-arrangements.md`, *accomp* = `auto-accompaniment-comparison.md`, *firmwares* = `other-firmwares.md`.

Code paths are under `app/song/`. "Step" means a 16th note; a bar is steps 0–15 (0 = beat 1, 4 = beat 2, 8 = beat
3, 12 = beat 4; 2 = "and of 1", 6 = "and of 2", and so on).

## 1. Harmony

| Principle | Rule → numbers | Status | Source |
| --- | --- | --- | --- |
| **Every chord is built from the mode.** | Root, 3rd, 5th and 7th stacked in thirds from the scale degree. A progression can force a quality (m7, dominant 7, maj7) where the mode would give another; then the 5th is perfect and the 9th natural. | **Done.** `theory.c: th_chord_pcs` | Task 2 ruling |
| **Extensions by color.** | Color 7: root, 3rd, 5th, 7th. Color 9: adds the 9th. Color "rich": the 9th plus one characteristic tone (13th on a dominant, 11th on a minor 7th, #11 on a maj7 only in Lydian) and drops a perfect 5th. At most 5 tones. | **Done.** `theory.c: th_chord_pcs` | Task 2 ruling; *genre* §2.1 |
| **No avoid notes as colors.** | A color tone a semitone above another chord tone is left out (for example the natural 11th on a major chord, the b9 on a iii7). | **Done.** `theory.c: th_chord_pcs` | Task 2 ruling |
| **A dominant always carries an extension.** | Any chord with a major 3rd and a minor 7th is colored at least 9. If the 9th is an avoid note, the 13th is tried. If neither fits, the chord drops its 7th and plays as a major triad. The voicer never drops the dominant's last extension. | **Done.** `harmony.c: dominant_colors`; `theory.c: th_voice_range` | cd9e5aa **(Sam)** |
| **No substitutions.** | Reharmonizing never changes a root, 3rd or 7th. On loop passes 2 and later, in songs below "rich" color: each pass has a per-song chance of 25–50% to color one chord of that pass one step richer. Rich songs never reharmonize. | **Done.** `harmony.c: spec_at` | 167b256, aa0ce3f **(Sam)** |
| **The intro and outro use the same chords.** | "Dark" blocks change nothing in the harmony. They only play softer and filtered. | **Done.** `harmony.c: harmony_chord_at` | **(Sam)** |
| **No semitone rubs between parts.** | Bass against keys: no keys note a minor 2nd above the bass, nor a major 7th above a bass note that is not the root. Pad: no pad note a semitone (or minor 9th) from a keys, lead or other pad note, and none a minor 2nd over the bass's root. Keys inside the voicing: see section 2 (a penalty, not a ban). The clash test allows 0 rubs. | **Done** (bass, pad, strum); **Partial** (keys voicer, see Open questions) | R20, R25, 8dde786 **(Sam)** |
| **The bass plays the 5th only when the chord has a perfect 5th.** | Else the root. So the bass never adds a note the chord lacks. | **Done.** `groove.c: chord_fifth_interval` | R20, 0996e03 |

## 2. Voice leading (the keys)

Pianist rules **(Sam, 03c8cdb)**, in `theory.c: th_voice_range`, called from `harmony.c: voice_chord` and
`loop_voicings`. The voicer scores every 4-note placement of the chord tones and keeps the cheapest. The costs are
the numbers below.

| Principle | Rule → numbers | Status | Source |
| --- | --- | --- | --- |
| **Four voices, in the comping register.** | Notes from C3 (48) to C5 (72), or lower when the keys make room for the lead (section 7). The bottom note at most C4 (60). No unisons. | **Done** | 03c8cdb |
| **Preferred register E3–A4.** | Each note under E3 (52) or over A4 (69) costs 4 per semitone. A top note under C4 (60) costs 1 per semitone. | **Done** | 03c8cdb |
| **Top voice first.** | The top voice costs 8 per semitone it moves, plus 6 for a leap over 2 semitones. So it stays when it can, else it steps. | **Done** | 03c8cdb; *accomp* §3.4 |
| **Hold common tones.** | A common tone that moves to another octave or voice costs 6. | **Done** | 03c8cdb |
| **Smallest moves for the inner voices.** | Each lower voice costs 1 per semitone it moves. | **Done** | 03c8cdb |
| **The first chord sits near a reference.** | With nothing before it, the voicing is pulled toward a top of F4 (65) and a bottom of G3 (55), 1 per semitone off each. (The workbench's voicing re-roll moves the reference to a top of D4–Bb4 and a bottom of C3–Bb3; every other rule holds.) | **Done** | 03c8cdb |
| **Rootless where it helps.** | Five-tone chords drop a tone. The root goes first (cost 0), then the 5th (cost 1), an extension last (cost 3). Never the 3rd or 7th. A dominant keeps one extension. Four-tone chords may drop the root and double the 5th (else the 3rd) at a cost of 2; keeping the root costs 0. So 9th and rich chords are rootless; 7th-color chords keep the root unless leaving it saves more than 2 semitones of motion. Sam (2026-10-30) kept this over strict rootless: forcing the root out cost the voice leading (common tones held 96% → 86%, top kept 96% → 76%). | **Done** | 8dde786 **(Sam)** |
| **No minor 2nds or 9ths, no muddy seconds.** | Any two voices a minor 2nd or a minor 9th apart cost 10000 each (a ban: used only if nothing else fits). A whole step between adjacent voices below G3 (55) costs 3. | **Done** (Sam, 2026-10-30) | 03c8cdb |
| **One voicing per chord, voiced as a cycle.** | The loop's chords (up to 4) are voiced 8 times around, each from the one before and the first from the last. The engine keeps the pass with the smallest total cost around the loop, wrap included, and plays it every pass. A chord recolored on a later pass is voiced on its own, from the last voicing. Half-note restrikes, stabs and the held chord's restrikes reuse the chord's voicing. Switch: `VNL_LOOPLOCK=0`. | **Done.** `harmony.c: loop_cycle`, `loop_voicings`, `voice_chord` | 8dde786 **(Sam)** |
| **One voicing through the song.** | Intros and outros voice exactly like the rest; they play 6 velocity softer and low-passed. | **Done.** `harmony.c: emit_chord` | 03c8cdb **(Sam)** |
| **Guitar shapes (strummed keys).** | A separate voicer for the nylon and the Emilyguitar when they strum: 4–6 strings, E2–E5 (40–76), the root on the low E, A or D string at C3–B3 (48–59), fretted notes within 4 frets. Chord tones only. No note a minor 2nd or 9th over another (a ban). A missing 3rd costs 100, a missing 7th 60. The top note aims at E4 (64), costs 3 per semitone over G4 (67), and costs 4 per semitone it moves from the last shape (40 more for a jump over 5). One shape per chord through the song. | **Done.** `strum.c: strum_voicing` | ee807a4, 887e370 **(Sam)** |
| **A loop's voicings should breathe** (a pianist re-voices on repeats). | Not built: the same voicings every pass, by Sam's one-voicing rule. | **Not yet** | *accomp* §6.5 |

## 3. Bass lines

`groove.c`. The bass range is E1–B2 (MIDI 28–47).

| Principle | Rule → numbers | Status | Source |
| --- | --- | --- | --- |
| **Lock the bass to the kick.** | "Roots": the root on every kick hit. No root-fifth line (Sam, 2026-11-04: the 1-to-5 bass is cheesy). | **Done.** `groove.c: bass_note` | *genre* §3.3 |
| **Place each root near the last.** | The tonic sits in A1–Ab2 (33–44). Every other root lands within 5 below or 6 above it, folded into E1–B2. A root more than 6 semitones from the previous root moves an octave toward it when the range allows. | **Done.** `groove.c: bass_place`, `root_place` | 8dde786; *accomp* §6.2 |
| **A walking line arrives.** | Quarter notes: root, 3rd, 5th (the root if the chord lacks a perfect 5th), then an approach. The approach is the scale tone 1–2 semitones from the next chord's root, on the side the line comes from, placed where that root will sound. It must not rub the chord still sounding; failing both sides, the 5th. Inside a 2-bar chord, the approach targets the same root. A 3rd or 5th that rubs the chord's colors gives way to the root. | **Done.** `groove.c: approach`, `next_root_of` | 8dde786; *accomp* §6.1 |
| **The walk is diatonic.** | Approach tones come from the mode's scale only, never chromatic. | **Done.** `groove.c: approach` | 8dde786 **(Sam)** |
| **A pedal restrikes per chord.** | The root at each chord change, held to the next change. | **Done.** `groove.c` (R21) | R21 |
| **No holes and no overhang.** | Root notes last to the next bass note, at most a beat. Walk notes last a quarter. A bass note always ends at the chord change. | **Done.** `groove.c`; `song.c` cut (R22) | R21, R22 |
| **Bass touch.** | Velocity 78–89, a little different every note. Odd 16ths swing with the drums. | **Done.** `groove.c: groove_step` | — |
| **The bass never copies the keys' rhythm.** | When the keys play more than a note on beat 1 (stabs, or half notes on 1 and 3) and the drawn bass style would match that rhythm exactly, the next style in a fixed order is tried. | **Done.** `groove.c: bass_style_of` | **(Sam)** |
| **Shape the bar as a contour toward the target,** not the same root-3rd-5th climb every bar; think in two-bar phrases. | Not built. | **Not yet** | *accomp* §3.5 |

Which songs get which style (and the walk's window) is a taste setting: [lofi.md, Bass](lofi.md#bass).

## 4. Rhythm and humanization

| Principle | Rule → numbers | Status | Source |
| --- | --- | --- | --- |
| **Systematic microtiming beats random jitter.** | The kick sits on the grid. The snare and rim are late by a per-song base of 13–27 ms, ±3 ms a hit (always 10–30 ms). Hats, open hat and shaker by a per-song base of 0–40 ms, ±8 ms a hit, capped at 70 ms. Swing delays every odd 16th by (swing − 50%) × 2 of a 16th, on drums, bass and strums alike. | **Done.** `groove.c: groove_step` | *genre* §3.3; R13 |
| **Velocity shapes the groove.** | Kick 94–107 (ghost 42–51). Snare 88–103 (ghost 25–34). Rim 58–69 (ghost 30–37). Closed hat 46–55, +4 on the beat (ghost 36–43). Open hat 50–59 (ghost 34–41). Shaker 42–51 (ghost 28–35). All clamped to 25–110. Every hit draws its own value. | **Done.** `groove.c: groove_step` | *genre* §3.4; *midi* §5 |
| **Chords are rolled, not struck as a block.** | Electric and synth keys: bottom up, 10–60 ms between notes, drawn per chord and bar. The Knight (acoustic piano): all notes within 0–8 ms, no roll. Strums: 7–12 ms between strings (6–14 after a stroke's factor), 40–70 ms on a slow ballad. | **Done.** `harmony.c: harmony_step`; `strum.c` | *genre* §2.4; 95f1797 **(Sam)** |
| **Fills mark structure,** at boundaries only. | A fill bar ends a block before the kick, snare or bass changes: steps 12–15 become snare 16ths at velocity 60, 70, 80, 90, with a kick on step 14 at 96. No crashes, no tom fills. | **Partial.** `plan.c`, `groove.c`. No phrase-end fills. | R13 |
| **Everything that swings swings together.** | Drums, bass and strums share the swing. The lead and the melody are straight (R16). | **Partial** | R16; *accomp* §6.3 |
| **Small variation inside a repeating part.** | Not built: one drum pattern and one comping style per song, the same every bar (velocity jitter aside). | **Not yet** | *accomp* §1, §6.6–6.7 |
| **Phrase starts vary.** | Lead rule; not refreshed (lead rewrite). | — | ledger |

## 5. Form and variation

Not refreshed in detail; the rules below still match `plan.c`.

| Principle | Status in VNL-1 | Source |
| --- | --- | --- |
| **Change on phrase boundaries,** not mid-phrase. | **Done.** All plan moves and listener requests land on 4/8-bar lines (`plan.c`, `song.c: song_request`, R17). | spec; R17 |
| **Change gradually:** few parts move at once; entries and exits are soft. | **Partial.** At most 2 roles change per boundary except map-mandated moves (R9). The pad fades in and out over a bar at a section line (lofi.md, Pad layer). | R9; spec anti-goals |
| **Every piece has a shape:** a beginning, a middle event, an ending. | **Done.** Map skeletons (`plan.c: skeleton`). | spec Song maps |
| **Intensity rises and falls** over the piece (density, velocity), not only on/off layers. | **Not yet.** | *accomp* §6.8 |

## 6. Melody (not refreshed: the lead is being rewritten)

The rows below describe the 2026-10-24 engines. Since then the ear candy is **Removed** (b4e00ff), and the two old
lead engines (the phrase planner, the hybrid) and the struck melody path are **Removed** (lead styles Task 6): every
song's lead plays one of the eight lead styles (`app/song/leads/`). Read these rows as history until the lead styles
are documented.

| Principle | Status on 2026-10-24 | Source |
| --- | --- | --- |
| **One motif, developed.** | `melody.c: melody_motif`, `vary`, `lead_phrase`. | spec |
| **Repetition dominates, variation keeps interest.** | Restate / variant / new 50 / 15 / 35 (`melody.c: melody_plan`). | *midi* §0, §5 |
| **Question and answer.** | Ear-candy answers and trading fours. **Removed:** the ear candy is gone. | R10, R16 |
| **Sparseness leaves room.** | About a third of beats played. | spec |
| **Chord-scale and strong beats on chord tones.** | `melody.c: bar_masks`; `theory.c: th_is_avoid`. | R20, R25 |

## 7. Register and mix (the accompaniment)

| Principle | Rule → numbers | Status | Source |
| --- | --- | --- | --- |
| **Each role has its own register band.** | Bass E1–B2 (28–47). Keys C3–C5 (48–72), E3–A4 preferred; the Chillwave pad set an octave lower. Guitar shapes E2–E5 (40–76), top near E4. Pad chord part G3–C5 (55–72). Pad solo note C5–C6 (72–84), the strings C6–C7 (84–96). | **Done.** `groove.c`, `theory.c`, `strum.c`, `pads.c` | spec The band |
| **The keys make room for the lead.** | Each song's lead claims a band: its style's center ±6 semitones. Percussive keys then voice with their top at most one semitone under the band's floor, by the same voice-leading rules. Sustained keys do too, unless the pad sits with or above the lead. Strummed guitar shapes keep their top at or under the band's floor (when that floor is E4-Eb5). The window is ignored (the lead moves up instead) when it would push the top under C4 (guitar: E4), when any loop chord can't fit under it, or for the workbench's keys octave. | **Done.** `harmony.c: harmony_keys_window`, `voice_hi`, `strum_hi`; `song.c: keys_win_for` | 54e2723, 2026-10-30 **(Sam)** |
| **The lead leads: a busy lead thins the comping.** | In a bar with 4 or more lead onsets: stabs strike on beat 1 only; half notes strike on beat 1 only, held the whole bar. Held chords, strums and sustained keys are unchanged. | **Done.** `harmony.c: harmony_step` (`busy`) | lead design **(Sam)** |
| **The keys duck under the lead.** | While a lead note sounds, the keys' bus dips 3 dB (about 25 ms in, 400 ms out). The pad layer dips 1 dB. | **Done.** `app/engine/voices.c` | **(Sam)** |
| **A sustained accompaniment frees the register.** | Sustained keys (pad, vox pad, organ) sit below, with or above the lead, one placement per song, equally likely. With or above: the keys an octave up. | **Done.** `harmony.c: harmony_pad_place` | lead design **(Sam)** |
| **Melody above percussive accompaniment.** | Lead rule; not refreshed (lead rewrite). | — | lead design |
| **Steady levels.** | Engine soft clip and about −6 dBFS peak; section and song level checks in the tests. | **Partial** | spec; R23 |

## 8. Communication and process

| Principle | Status | Source |
| --- | --- | --- |
| **Simplest spelling** of keys and notes in all text: Bb not A#, Db Lydian not C# Lydian, no double sharps. | **Done** in the song log and MusicXML (`app/host/score.c`); applies to docs and screen text too. | af0521c **(Sam)** |
| **Explain the logic with measurements:** before/after counts over many rolled songs. | **Done** as practice (ledger, commit bodies). | **(Sam)** |
| **Let the ear see the music:** every render has a song log, MIDI and MusicXML beside it. | **Done.** `VNL_SONGLOG`, `VNL_SCORE` in the host simulator. | 876a1c4, af0521c **(Sam)** |
| **Patterns as files Sam can edit.** | **Done.** Drum patterns, progressions, comp rhythms and strum patterns live in `patterns/` as MIDI and MusicXML; `tools/patterns/import.sh` turns them into `patterns_gen.c` with checks (no substitutions, no bare dominant, kick and snare in every drum bar). New files are candidates until Sam approves them. | 8af0acf, 5c500f4 **(Sam)** |

## Open questions for Sam

Cases where the code and the ledger (or Sam's rules) disagree. Nothing here was changed.

1. **Settled (Sam, 2026-10-30): keep the voice leading; rootless stays a preference.** Was: **Rootless voicings are a preference, not a rule.** The ledger's approved voicing is "rootless". In the code,
   only 5-tone chords (9th and rich colors) drop the root by default. A 7th-color chord (10–30% of songs, by mood;
   also any chord whose 9th is an avoid note) keeps its root unless dropping it saves more than 2 semitones of
   motion. The strummed guitar shapes always put the root on the lowest string. Should rootless be a hard rule for
   the keys? For the guitars?
2. **Settled (Sam, 2026-10-30): the keys now ban minor 2nds and 9ths too.** Was: **The keys' own semitone check is weaker than the pad's and the strum's.** "No semitone rubs" holds as a ban for
   the bass, the pad and the guitar shapes, including minor 9ths. The keys voicer only penalizes a semitone between
   adjacent voices (cost 100, allowed if nothing else fits) and does not look at minor 9ths between non-adjacent
   voices. Should the keys match the others?
