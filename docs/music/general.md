# General musicianship (genre-independent)

Principles that make music sound played by a good musician in any style. If VNL-1 were a jazz trio, a pop band or
a film cue, these would still hold. Genre and taste settings (how much swing, which chords, how long a song is) live
in [lofi.md](lofi.md); ideas not adopted yet live in [ideas-from-other-apps.md](ideas-from-other-apps.md).

Status: **Done** (file: function), **Partial**, **Not yet**. Sources: `docs/research/` (short names below), the
plan ledger's rulings (R*n*), commits (short SHA), or **(Sam)** for decisions Sam made after listening.

Research short names: *genre* = `lofi-hip-hop.md`, *audio* = `lofi-audio-arrangements.md`, *midi* =
`lofi-midi-arrangements.md`, *accomp* = `auto-accompaniment-comparison.md`, *firmwares* = `other-firmwares.md`.

## 1. Harmony

| Principle | Status in VNL-1 | Source |
| --- | --- | --- |
| **Chord-scale.** Over each chord, melody notes come from the scale with each note a semitone above a chord tone replaced by that chord tone, so no avoid note is left. | **Done.** `melody.c: bar_masks` (the `weak` mask); `theory.c: th_is_avoid`. | R25, 1aeaa20; *accomp* §4 |
| **Strong beats on chord tones,** weak beats free within the chord-scale. Guide tones (3rd, 7th) are the most defining. | **Done.** `strong` mask on steps 0 and 8; the motif starts on a guide tone (`melody.c: gen_figure`, `theory.c: th_guide_tones`). | spec Melody; R20 |
| **Extensions come from the scale, never an avoid tone.** A color tone a semitone above another chord tone (natural 11 on a major chord) is left out; the 5th is the first tone to drop. | **Done.** `theory.c: th_chord_pcs` (colors 7 / 9 / rich: 13 on dominants, 11 on minor 7ths, #11 on Lydian maj7). | Task 2 ruling; *genre* §2.1 |
| **A dominant 7th is extended, and the extension sounds.** Jazz practice: a bare V7 sounds thin and dated; at least a 9th, a 13th in richer settings, and the voicing keeps 3rd, 7th and the extension (the 5th goes). | **Done.** `harmony.c: dominant_colors`, `theory.c: th_voice_range` (dominants keep an extension). The stricter "never bare, else a triad" rule is a taste call: see [lofi.md](lofi.md#harmony). | cd9e5aa **(Sam)** |
| **No semitone rubs between parts.** Lead and ear candy never sound a semitone from any sounding chord or bass note, at any step (not only strong beats); within the keys no two voices a semitone apart. | **Done.** `melody.c: bar_masks` (`safe`, held notes checked on every step they sound); `theory.c: th_voice_range` (no clusters); checked by `tests/host/clash_test.c` (0 allowed). Note: jazz tolerates a passing rub on a weak beat; VNL-1's zero tolerance leans toward soothing, so the *strictness* is partly taste. | R20, R25, 0996e03, 1aeaa20 **(Sam)** |
| **Bass against keys: no minor 2nd above the bass, no major 7th above a non-root bass note** (root under a maj7 is the chord itself, so it stays). | **Done.** `groove.c: rubs_chord`, used by the walk's approach and the fifth; `clash_test` checks bass vs keys at every step (830 rubs → 0). | 8dde786 **(Sam)** |
| **The bass's 5th only when the chord has a perfect 5th** (else the root), so the bass never adds a note the chord lacks. | **Done.** `groove.c: chord_fifth_interval`. | R20, 0996e03 |
| **Narrow strong-beat choice is a cost.** When strong beats allow only 2 pitch classes, a lead sounds arpeggiated. Voiced color tones (the 9th in a sounding maj9) can be added without any rub. | **Not yet.** R28 deferred; see ideas file (color tones on strong beats). | R28, R30; *accomp* §6.9 |

## 2. Voice leading (keys)

Pianist rules **(Sam, 03c8cdb)**, all in `theory.c: th_voice_range`, called from `harmony.c: voice_chord`.

| Principle | Status in VNL-1 | Source |
| --- | --- | --- |
| **Hold common tones.** | **Done.** Held 76% → 96% of possible cases (300 songs). | 03c8cdb |
| **Move the other voices to the nearest chord tone.** | **Done.** Per-voice motion cost. | 03c8cdb |
| **Top voice first:** it stays if the new chord has it, else it steps (no leaps). | **Done.** Top voice weighted 8×; kept 69% → 100% when a clean voicing allows; top leaps 10% → 0%. | 03c8cdb; *accomp* §3.4 (Tonal's top-note rule) |
| **Rootless voicings only when the bass plays the root.** Five-tone chords drop the root first, then the 5th, never the 3rd or 7th. Keys alone (intros, outros) voice the root themselves. | **Done.** `rootless` flag = bass layer present (`harmony.c: voice_chord`); keys-only root 60% → 100%. | 03c8cdb, 8dde786 |
| **No semitone clusters; no tight 2nds low** (muddy below about C4). | **Done.** Inside sounding chords 2,830 → 0 rubs (of 18,224 strikes). | 03c8cdb |
| **Keep the comp in its register:** E3–A4 preferred, C3–C5 allowed (room so no voicing is forced into a rub), bottom at most C4, top pulled back up when it drifts low. | **Done.** | 03c8cdb |
| **Keep one voicing through a song:** an intro or outro is darkened by its sound (filter, softer playing), not by different voicings, so voice leading never breaks at a section seam. | **Done.** `harmony.c: voice_chord` voices dark blocks exactly as the rest; they play softer (`emit_chord`). An earlier lower-ceiling dark voicing was removed (Sam). | 03c8cdb, this change |
| **A loop's voicings should breathe:** a pianist re-voices on repeats (the top line moves over passes). | **Not yet.** Same voicings every pass; see ideas file (slow top line). | *accomp* §6.5 |

## 3. Bass lines

| Principle | Status in VNL-1 | Source |
| --- | --- | --- |
| **Lock the bass to the kick;** roots on strong beats. | **Done.** `groove.c: bass_note` (roots / root-fifth on kick hits). | *genre* §3.3; spec |
| **A walking line arrives:** the last beat before a chord change leads by a step into the **next** chord's root, placed exactly where that root will sound, never on a note that rubs the chord still sounding (else the 5th). | **Done.** `groove.c: approach`, `next_root_of`; 9 of 147 → all approaches lead in. (Diatonic-only is a taste rule: [lofi.md](lofi.md#bass).) | 8dde786; *accomp* §6.1 (Impro-Visor, JJazzLab, Dias–Guedes, autocomp) |
| **Place each root near the previous one,** inside a window centered on the tonic, so a step in the key sounds as a step, not nearly an octave. | **Done.** `groove.c: bass_place`, `root_place` (window E1–B2, 28–47); step-wise root moves sounding a step 83% → 97%. | 8dde786; *accomp* §6.2 |
| **No holes and no overhang:** a held bass note ends at the chord change; a pedal restrikes the new root at every change. | **Done.** `song.c` cut at changes (R22); `groove.c` pedal (R21). | R21, R22, acf29e4 |
| **Shape the bar as a contour toward the target,** not the same climb (root, 3rd, 5th) every bar; think in two-bar phrases. | **Not yet.** See ideas file (contour walking bass). | *accomp* §3.5 |

## 4. Rhythm and humanization

| Principle | Status in VNL-1 | Source |
| --- | --- | --- |
| **Systematic microtiming beats random jitter:** each part has its own consistent lateness (a "feel"), plus a little jitter. | **Done.** `groove.c: groove_step` (per-song snare and hat lateness, small jitter); `melody.c: melody_step` (lead 5–25 ms late). The amounts are lo-fi. | *genre* §3.3; *accomp* §3.3 (JJazzLab notes) |
| **Velocity shapes the groove:** accents, ghost notes well below the backbeat, hats softer than kick and snare, every hit a little different. | **Done.** `groove.c` velocity bands (about 25–110) and jitter; `library.c: LIB_PATTERNS` ghost bits. | *genre* §3.4; *midi* §5 |
| **Chords are rolled, not struck as a block,** bottom up. | **Done.** `harmony.c: emit_chord` (10–60 ms apart). | *genre* §2.4 |
| **Everything that swings swings together.** A straight part against swung hats sounds like a flam. | **Not yet.** The lead and candy are not swung (R16); Sam should hear both. See ideas file. | R16; *accomp* §6.3 |
| **Phrase starts vary:** off the beat, with pickups, not mostly on beat 1. | **Not yet.** 35% of lead gestures start on beat 1, no pickups. Ledger "To revisit". The amount of syncopation is lo-fi. | ledger; *accomp* §6.3 |
| **Fills mark structure,** at boundaries only, never startling. | **Partial.** Fills before kick/snare/bass layer changes and 1-bar drops (`plan.c`, `groove.c`); no phrase-end turnarounds yet. | R13; Task 5 minor |
| **Small variation inside a repeating part** (a skipped hat, a variant bar) keeps a loop alive. | **Not yet.** One comp style and one drum pattern per song, the same every bar. See ideas file. | *accomp* §1, §6.6–6.7 |

## 5. Form and variation

| Principle | Status in VNL-1 | Source |
| --- | --- | --- |
| **Change on phrase boundaries,** not mid-phrase. | **Done.** All plan moves and listener requests land on 4/8-bar lines (`plan.c`, `song.c: song_request`, R17). The grid size is lo-fi. | spec; R17 |
| **Change gradually:** few parts move at once; entries and exits are soft (no jumps in loudness). | **Partial.** ≤ 2 roles per boundary except map-mandated moves (R9); fades and swells flagged (`PB_FADE`, `PB_SWELL`); level checks between sections not built. | R9; spec anti-goals |
| **Every piece has a shape:** a beginning, a middle event, an ending. | **Done.** Map skeletons with intro, middle event, outro (`plan.c: skeleton`). Which shapes is lo-fi. | spec Song maps |
| **Intensity rises and falls** over the piece (density, velocity), not only on/off layers. | **Not yet.** See ideas file (per-block intensity). | *accomp* §6.8 |

## 6. Melody

| Principle | Status in VNL-1 | Source |
| --- | --- | --- |
| **One motif, developed:** stated, moved onto the next chord with the same shape, given a new ending, restated. | **Done.** `melody.c: melody_motif`, `vary`, `lead_phrase`. | spec; Tuna (*firmwares*) |
| **Repetition dominates, variation keeps interest;** restatement grows toward the end. | **Done.** Head–solos–head plan, restate / variant / new targets 50 / 15 / 35 (`melody.c: melody_plan`). The exact ratio is measured from lo-fi. | *midi* §0, §5 |
| **Question and answer:** a phrase is answered, by itself or another voice. | **Done.** Ear-candy answers and trading fours (`melody.c: answer_phrase`, `candy_figure`); lead and candy never on the same step. | spec; R10, R16 |
| **Sparseness leaves room;** rests are part of the line. | **Done.** About a third of beats played. The density is lo-fi. | spec |
| **A phrase has a contour** (an arch, a descent to a guide tone) and ends on a long chord tone. | **Not yet.** `melody.c: next_pitch` is a weighted walk with center gravity, no arch. See ideas file. | *accomp* §6.9 |
| **Avoid stutters:** no repeated same-pitch 16ths, no restatement clusters back to back. | **Not yet.** Task 8 deferred minors. | ledger Task 8 |

## 7. Register and mix

| Principle | Status in VNL-1 | Source |
| --- | --- | --- |
| **Each role has its own register band,** so parts don't collide: bass E1–B2 (28–47), keys C3–C5 (48–72, E3–A4 preferred), lead 64–84 (shifted by mood), ear candy 72–96. | **Done.** `groove.c`, `theory.c`, `melody.h`. | spec The band |
| **Melody above the accompaniment:** the keys' top stays under the lead's floor while the lead plays. | **Not yet.** Bands overlap (keys up to 72, lead from about 64); no clash is possible (pitch-class masks) but the lines can cross. | *accomp* §6.5 guard |
| **Steady levels:** no loudness jumps between sections or songs; headroom kept. | **Partial.** Engine soft clip and about −6 dBFS peak (`app/engine`, bed46a3); section and song level checks planned (spec Testing). | spec; R23 |

## 8. Communication and process

| Principle | Status | Source |
| --- | --- | --- |
| **Simplest spelling** of keys and notes in all text: Bb not A#, Db Lydian not C# Lydian, no double sharps. | **Done** in the song log and MusicXML (`app/host/score.c`); applies to docs and screen text too. | af0521c **(Sam)** |
| **Explain the logic with measurements:** before/after counts over many rolled songs, as in the commits. | **Done** as practice (ledger, commit bodies). | **(Sam)** |
| **Let the ear see the music:** every render has a song log, MIDI and MusicXML beside it. | **Done.** `VNL_SONGLOG`, `VNL_SCORE` in the host simulator: `app/build/scenarios/songs.log`, `songs-N.mid`, `songs-N.musicxml`. | 876a1c4, af0521c **(Sam)** |
