# Ideas from other apps, not adopted yet

What other accompaniment engines, generators and FM-1 firmwares do that VNL-1 does not. Distilled from
`docs/research/auto-accompaniment-comparison.md` (MMA, Impro-Visor, JJazzLab, Strudel/iReal, Tonal.js, the
Dias–Guedes walking-bass paper, autocomp, accompy, jacbz Lofi), `docs/research/other-firmwares.md` (LOF-1, HYP-1,
MAKU, Mutable Dreams, Tuna, Bunker Evolved, Mini-Tone, Jangada), `docs/research/amb-1-and-vnl-1.md` (DUB-1) and
`docs/research/lofi-hip-hop.md` §7 (Magenta Lo-Fi Player, lofi-engine). Licensing: ideas only, no code, except
Mutable Dreams (MIT) and Tonal.js (MIT).

**Already adopted, so left out:** the walking bass's last beat leading into the next chord's root; bass roots
placed near the previous root; the bass-vs-keys rub check; rootless voicings only when the bass plays (keys alone
keep the root); top-voice-first voice leading; per-song systematic lateness; head–solos–head; answers and trading
fours; seeds and novelty.

**Columns.** *Kind*: **general** (good musicianship in any genre) or **lo-fi** (specific to the genre). *Fit*:
**fits**, **fits with guards**, or **does not fit** (with the reason). *Cost* on the FM-1: CPU per step, RAM, flash,
and how many modules it touches. *Priority*: **P1** next, **P2** soon, **P3** later, **—** not planned.

## Top picks

1. **Color tones from the sounding voicing on strong beats** (Impro-Visor, lofi-engine): widens the lead's
   strong-beat choice with zero rub risk; the direct answer to R28's "arpeggiated lead" worry.
2. **Phrase starts off the beat, with pickups** (Impro-Visor, Magenta's melody): the ledger's "To revisit" item,
   backed by every grammar-driven soloist studied.
3. **A slow top line in the keys** (MMA, Strudel, Tonal.js): the loop stops repeating identical voicings and gains
   a quiet countermelody; extends Sam's top-voice-first rule.
4. **Melodic contour templates** (Impro-Visor): phrases that arch and land on a long chord tone instead of a
   weighted walk.
5. **Slow, Motion-scaled variation of comp and drums** (MMA, Impro-Visor, JJazzLab): per pass or block, never per
   bar, so the loop still feels sampled.

## Melody

From listening (Sam, not from another app), to start the lead rework with: the **slow 8th-note arpeggio** of the
song's voicings as lead material (as comping it sounded like a melody), and **simple whole-note voice-leading lines**
(a chord tone to the nearest one, a note per chord) built on the keys' voice leading. See the ledger's "Leads".


| Idea | From | Kind | Fit | Cost | Priority |
| --- | --- | --- | --- | --- | --- |
| **Color tones on strong beats:** allow an extension the current voicing actually contains (the 9th of a sounding maj9 or m9). Grammars weight chord / color / scale tones about 0.7 / 0.2 / 0.1. | Impro-Visor grammars; lofi-engine weights | general | fits (a voiced tone cannot rub the voicing) | a `color` mask per bar in `melody.c: bar_masks`; trivial | **P1** |
| **Rest-led phrase starts and pickups:** rhythm templates with explicit leading rests (`r8 C8 …`); 1–3 pickup notes before the bar, ending before the chord change. Proposed mix: 15% beat 1, 25% other beats, 35% 8th offbeats, 10% 16ths, 15% pickups. | Impro-Visor bricks; Dias–Guedes ornaments; Magenta's melody (starts on beat 2) | general need; lo-fi amount | fits | small table in `melody.c` (`CELLS`, `gen_figure`) | **P1** |
| **Contour templates:** 4–6 shapes (arch; descent to a guide tone; rise then hold; stepwise answer) bias each move's direction; end phrases on a long chord tone. | Impro-Visor `slope` segments | general | fits | small table; `melody.c: next_pitch` | **P1** |
| **Chord-tone weights by role:** 3rd 0.30, 7th 0.20, 5th 0.15, root 0.10, extensions 0.25. | lofi-engine | general | fits | trivial | **P2** (with color tones) |
| **Motif transforms:** invert (about 10%), alter (30%), mirror, orbit, bloom as named development moves under guard conditions. | Tuna; Impro-Visor transforms; Bunker Evolved EVOLVE | general | fits (melody masks keep them legal) | small; `melody.c: vary` | **P2** |
| **Swing the lead and candy** like the drums (odd 16ths), keeping the small lateness. Reverses R16: A/B by ear. | every engine studied | general | fits | trivial; `melody.c: melody_step` | **P2** (listen first) |
| **"Deja vu" dial** between repeating the hook and wandering. | Tuna (Marbles) | general | fits | small | **P3** (the 50/15/35 plan covers most of it) |

## Keys and comping

| Idea | From | Kind | Fit | Cost | Priority |
| --- | --- | --- | --- | --- | --- |
| **Slow top line:** a target top note per pass or 8-bar block (from guide and color tones, a gentle path like 5 → 9 → 3), as a cost term; drift scaled by Motion; keys' top stays under the lead's floor while the lead plays. | MMA `RMOVE`/`MOVE`; Strudel top-note anchor; Tonal `topNoteDiff` | general | fits (extends Sam's top-voice rule) | one term per candidate (~500 per change); `theory.c: th_voice_range`, `harmony.c: voice_chord` | **P1** |
| **Weighted comp cells varied per pass:** a small library of 1-bar cells per style with a strong repeat bias. | Impro-Visor `chord-pattern`; MMA `SeqRnd` | general | fits with guards (per pass or block, never per bar) | a few hundred bytes; `library.c`, `harmony.c` | **P2** |
| **Lazy late changes:** the whole chord an 8th behind the bar on some changes (about 40% in the Magenta MIDI). | Magenta Lo-Fi Player MIDI | lo-fi | fits with guards (melody bound and bass must agree) | small; touches harmony, melody masks, bass | **P2** |
| **Push (anticipation):** strike the next chord an 8th early on the loop's last bar. | Impro-Visor `push`; JJazzLab anticipations | general (jazz) | fits with guards: only when the bass pushes too, groove blocks only, never sad mood; lo-fi leans late more than early | medium (three modules) | **P3** (taste call) |
| **Preferred rootless shapes** (left-hand A/B: 3-5-7-9, 7-9-3-5) as a small bonus. | Impro-Visor vocab; Strudel `lefthand` | general | fits | trivial | **P3** (the search reaches them already) |
| **Legato gate 95–100% with a short release before changes.** | JJazzLab notes | general | fits | trivial | **P3** |

## Bass

| Idea | From | Kind | Fit | Cost | Priority |
| --- | --- | --- | --- | --- | --- |
| **Contour walking bass:** fill from the current note to the next target strongest beat first; "openness" between direct and arpeggiated paths; think in two-bar phrases instead of root-3rd-5th every bar. | Dias–Guedes paper | general | fits | small; `groove.c: bass_note` | **P2** |
| **Bass rhythm cells per bar:** whole; two halves; 3 + 1; 3.5 + 0.5 (a push into the next bar); tresillo 1.5 + 1.5 + 1, all roots. | jacbz Lofi | lo-fi | fits with guards (the push must not rub the chord still sounding) | trivial table | **P2** |
| **Bass rule strings with weights** (root, chord tone, scale step, approach, next root, rest), held roots weighted far above walking figures. | Impro-Visor `bass-pattern` | general | fits (approach diatonic) | medium; best as bench data | **P3** |
| **Phrase database tiling:** 1–2 bar recorded walking phrases, longest first, no repeats, target-note continuity. | JJazzLab JJSwing | general | fits, but curation-heavy | ~2.4 KB flash for 100 phrases | **P3** (only if rules still sound mechanical) |

## Drums and feel

| Idea | From | Kind | Fit | Cost | Priority |
| --- | --- | --- | --- | --- | --- |
| **Per-hit skip chance** for ghosts and hats (a per-hit hash, no state), so hats breathe. | MMA `RSkip`; Mutable Dreams chance per step | general | fits | trivial; `groove.c` | **P1** |
| **A/B bar variants:** an optional variant bar per pattern (a different kick on bar 4 of each 4). | MMA `SeqSize`; Impro-Visor weighted drum patterns | general | fits | a few bytes per pattern | **P2** |
| **Rare phrase-end turnarounds:** a soft open hat on the and-of-4, an extra ghost, a kick pickup at the end of 8-bar phrases. | JJazzLab fill parameter (`random_rare`); autocomp section-end fills | general | fits with guards (subtle; no crashes or rolls) | small; `plan.c` flag, `groove.c` | **P2** |
| **Per-step nudges in a pattern:** kicks on beats 1 and 3 a little late, on 2 and 4 a little early. | Mini-Tone NUDGE; MusicRadar (in *genre* §3.1) | lo-fi | fits | a few bytes per pattern | **P3** |
| **Tempo-linked swing:** more swing at 70 BPM, the median at 90. | JJazzLab notes | general | fits | trivial; `roll.c` | **P3** |
| **Slight accents on beats 1 and 3 at slow tempos.** | JJazzLab notes | general | fits | trivial | **P3** |

## Form and dynamics

| Idea | From | Kind | Fit | Cost | Priority |
| --- | --- | --- | --- | --- | --- |
| **Per-block intensity** (about −3…+3): velocity offset (±6–10), comp-cell density, hat ghosts, lead density; rises through the middle, dips in breaks, falls in the outro; scaled by Motion. | JJazzLab Intensity; autocomp density modes | general | fits with guards (small steps: no loudness jumps) | one byte per block; read in groove, harmony, melody | **P2** |
| **Filter arc per section:** breaks go a little darker, then open over a few bars. | DUB-1 | lo-fi (filtered intros are the genre's) | fits with guards (slow, gentle) | Plan 3 lo-fi chain | **P2** (with Plan 3) |
| **A style as one data record with ranges** (tempo, swing, density, register, motif reuse, bass rhythm, pad mode): the genre split made explicit in code; sub-styles (Nujabes jazz, dusty Dilla, sleepy ambient) as more records. | Tuna | general architecture | fits | refactor of `library.c` moods and sets | **P3** |

## Sound and texture

| Idea | From | Kind | Fit | Cost | Priority |
| --- | --- | --- | --- | --- | --- |
| **Dust as a medium:** vinyl, cassette, reel, radio (static, band-limit). Radio is new to VNL-1's list. | LOF-1 | lo-fi | fits | Plan 3 chain | **P2** (Plan 3) |
| **DX7 E.PIANO 1 for the Rhodes;** Dexed-compatible patches for FM voices. | Jangada | lo-fi timbre | fits | a 6-op FM voice; CPU to measure | **P2** (Plan 3) |
| **Granular smear** from a recent-audio buffer. | Mutable Dreams | lo-fi texture | fits with guards (shares the pool with reverb) | 256 KB pool | **P3** (with self-sampling chops) |

## Product and tooling

| Idea | From | Kind | Fit | Cost | Priority |
| --- | --- | --- | --- | --- | --- |
| **Readable bench data:** drum lanes as 16-character grids, comp cells, bass rule strings, melody rhythm templates, each with a weight and a source; keep/fix/drop ratings map onto weights. | MMA text DSL; Impro-Visor weighted s-expressions | general | fits | a compiler script to C tables | **P2** (with the bench) |
| **Likes steer future rolls,** with about 8% mutation per part. | MAKU | product | fits | flash writes only on save | **P3** (spec: later, maybe) |
| **"Another like this" with undo** for the re-roller. | Mutable Dreams, Bunker Evolved | product | fits | small | **P3** |
| **The song as an object on screen** (a record label or cassette J-card). | ECO-1, GRAV-1 | product | fits | screen work | **P3** |

## Does not fit lo-fi (or VNL-1)

| Idea | From | Kind | Why not |
| --- | --- | --- | --- |
| **Chromatic approach notes** (bass and melody `A`) | Impro-Visor, autocomp, Dias–Guedes | general (jazz) | Standard in jazz walking bass, but conflicts with Sam's diatonic-only rule and the no-semitone-rub rule. |
| **Chord substitutions, tritone subs, substitution styles** | Impro-Visor `swing-substitute.sty`; research's own early proposal | general (jazz) | Sam heard a tritone-sub D7 in Db Lydian and a borrowed Bm7 in Gb major as wrong: stay in key. |
| **A second progression, key changes, an evolution timer to a pivot key** | Tuna `cycle2`, evolution timer; research's "two movements" | general form device | Measured lo-fi tracks keep one loop and one key; Sam chose 2–3.5 minute songs with one loop. |
| **Next-chord Markov tables** for progressions | lofi-engine | generic generator | Loops are curated by ear **(Sam)**; a free chain wanders. |
| **Random 2-bar drum toggles** (off p 0.2, on p 0.95 every 2 bars); drums out on the last bar of every pass | Magenta Lo-Fi Player; jacbz | lo-fi generators | Too frequent and unplanned: about 40% of real tracks never drop the drums; maps place drops deliberately. |
| **Phrase-to-phrase Markov form** | DUB-1 | dub/ambient | Looks one phrase ahead; VNL-1 plans whole songs. |
| **Build-then-drop sections, risers** | HYP-1 | techno/EDM | Startling; excluded by the anti-goals. |
| **Record skip, tape stop, backspin, stutter, half-time switch** | LOF-1 performance FX | lo-fi production FX | None in the 19 measured tracks; startling for a study radio. |
| **Euclidean drum patterns** | Mutable Dreams, AMB-1 | ambient/techno | Boom-bap wants written patterns (chance per step does carry over, see above). |
| **Random per-note timing jitter as "swing"** (up to 300 ms on melody) | Magenta Lo-Fi Player | lo-fi generator | Systematic lateness sounds played; large random jitter sounds sloppy (JJazzLab notes agree). |
| **Learned bebop grammars, Markov clusters from transcriptions** | Impro-Visor | jazz | Far too dense for a sparse lo-fi lead; trained off-device. The format (rhythm + note category + contour) is kept above. |
| **ML generators** (VAE, transformers) | jacbz Lofi, others | any | No tensor runtime or RAM on the AC79. |
| **Large recorded databases** (Yamaha styles, BIAB RealTracks, JJazzLab MIDI DBs at scale) | JJazzLab YamJJazz, BIAB | any | Flash and curation cost; a tiny bass DB is the only candidate (above). |
| **Root-position dictionary voicings under a bass** | Strudel `ireal` set | solo piano | Doubles the bass's root; VNL-1 already keeps the root only when the keys are alone. |
| **Tempo-adaptive swing for fast tempos** | JJazzLab | jazz | VNL-1 lives at 70–90 BPM. |
