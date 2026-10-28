# Hip hop kick patterns: acoustic vs synth families (2026-10-28)

Question: how do kick patterns work in hip hop, how do they differ between acoustic-drum and machine-drum families, and what should VNL-1's kick lane do? Builds on `lofi-midi-arrangements.md` (3.4 and the per-song grids), `lofi-hip-hop.md` (3.1, 6) and `auto-accompaniment-comparison.md`; those are not repeated here.

**Step convention:** 16th steps 0-15 in one bar. 0 = beat 1, 4 = beat 2, 8 = beat 3, 12 = beat 4. Step 3 = "a" of 1, 6 = "and" of 2, 7 = "a" of 2, 10 = "and" of 3, 14 = "and" of 4, 15 = "a" of 4.

**Evidence honesty:** the web has very few machine-readable kick transcriptions. Most grids are images or paywalled. Tags: **[src]** stated in a fetched page; **[repo]** from VNL-1's own Lakh/Online Sequencer stats; **[recall]** from my general knowledge, not verified, treat as a starting point; **[inferred]** my reasoning. Where a source used 1-indexed slots I converted to 0-indexed.

## 1. Summary table

| Family | Kicks/bar | Common grids (steps) | Feel / velocity / timing | Bass relation |
|---|---|---|---|---|
| Sampled break / boom bap (acoustic kick sample) | 3-4 (2-5) | 0,8 / 0,7,10 / 0,3,6,10 / 0,6,10,14 | Kick on grid or slightly early, snare late; MPC 8th swing ~54-60%; main kicks 0.85-1.0, syncopated ones 0.6-0.8 | Separate bass locks to kick; roots on kick hits |
| Live / acoustic-kit lo-fi, jazz-hop | 2-3 (up to 4) | 0,10 / 0,6,10 / 0,8,10 / 0,3,10 | Soft (0.5-0.8), inconsistent velocity; bar 1/3 kicks a little late, 2/4 a little early; swing 54-62% | Upright/electric bass follows kick; kick often one hit shorter than bass phrase |
| 808 / 909 / CR-78 machines (electro, 80s rap) | 3-5 | 0,10 / 0,3,6,10 / 0,3,8,10 (recall) | Dead-straight grid or fixed swing %, nearly constant velocity (machines have accent only) | 808 kick can be the bass; otherwise bass doubles kick |
| SP-1200 / MPC machine-kit boom bap | 3-5 | 0,7,10,14 / 0,7,8,10,15 | Swing 54-66% on 16ths (SP/MPC), kick often swung less than hats; velocity layers | Sample bass + kick, kick leads |
| Synth lo-fi / chillhop with 808 | 2-4 | 0,6,10 / 0,10 / 0,3,10 / 0,6,8,14 | Soft, sidechain to everything else; 52-60% swing; velocity 0.5-0.9 | 808 sub follows the kick hits, 2-4 notes/bar |
| Trap / drill-adjacent 808 | 2-5, follows 808 | 0 + two or three syncopated steps, snare on 8 | Straight or light swing, hard velocity; hats supply the busy part | Kick and 808 are one instrument |

## 2. Acoustic family

Characteristics: the kick is a sampled or live acoustic bass drum on a one-bar to four-bar loop. The kick is busier than rock (3-4 vs 2-3) because of added syncopations before or after beat 3, but each added hit is quieter than the anchor. Backbeat snare lands on 4 and 12 (or 8 in half-time).

Grids (0-indexed):

| # | Grid | Source / note |
|---|---|---|
| A1 | 0, 8 | Boom bap starting point: kick on 1 and 3 (Violet Recording [src]). Plain; needs a pickup to avoid "plodding" (NI [src]). |
| A2 | 0, 7, 8 | NI hip hop drums 101: kick on 1 and 3 plus one kick on the last 16th before beat 3 (step 7), nudged late halfway to the next 16th so it merges toward step 8 [src]. |
| A3 | 0, 6, 10 | Hein/Violet: kick on 1, "anticipates" the beat-3 kick by landing at 7 or 8 (1-indexed; 6 or 7 here), or is delayed to 10 or 11 (9 or 10 here), or the beat-3 kick is omitted [src, conversion mine]. |
| A4 | 0, 3, 6, 10 | Soul II Soul "Back to Life": kicks at 4, 7, 11 (1-indexed) = 3, 6, 10, plus no kick on beat 3 itself; the step-3 kick "pre-empts the snare on beat 2"; Faderpro suggests 16th swing and snares pushed slightly late [src]. Step 0 not stated; assumed. |
| A5 | 0, 10 | Cold Sweat (Hein table: 1, 9, 11 1-indexed = 0, 8, 10) [src]. Strictly 0, 8, 10. |
| A6 | 0, 7, 8, 14 | Impeach the President per Hein's table (1, 8, 9, 15 1-indexed). Hein's slot count is ambiguous (bar vs 8ths); treat as unverified [src, caveat]. Soundfly calls the Impeach kick "heavily syncopated" [src]. |
| A7 | 0, 2, 6, 10, 13 | Funky Drummer per the same Hein table (1, 3, 7, 11, 14). Same caveat; real break has varied kicks across the loop [src, caveat]. |
| A8 | 0, 1, 7, 10, 11 | When the Levee Breaks (Hein table 1, 2, 8, 11, 12): a 16th-pair "double kick" on 0-1 and 10-11, the busiest acoustic example, rock-heavy [src]. |
| A9 | 0, 6, 10, 14 | Repo stats: kick `X.....X...X...X.` in a lofi-midi song, same as LIB pattern 1 (Cantaloop) [repo]. |
| A10 | 0, 8, 10 / 0, 8, 15 / 0, 10 | Repo stats: `X.......X.X.....`, `X.......X......X`, `X.........X.....` [repo]. |
| A11 | 0, 9, 15 | MusicRadar: downbeat, extra kick at 16th #10 counting from 0 = step 9 (as quoted in `lofi-hip-hop.md` 3.1), grace kick on the last 16th [repo, secondhand]. |

**Amen break (4-bar kick phrase).** Faderpro: bars 1 and 3 identical; bar 2 adds a kick on the 3rd 16th of beat 1 (step 2); bar 4 drops the beat-1 kick and adds one halfway through beat 3 (step 10) [src]. Hein: kicks sit on the eighth-note offbeats, beat-3 kick omitted, "and" of 3 doubled in bars 1-2, bar 4 omits the downbeat kick and doubles the "and" of 1; he describes the whole as displaced tresillos over an R&B backbeat [src]. Lesson: the 4-bar kick phrase is bar A, bar A', bar A, bar A'' where bar 4 drops the downbeat and adds a pickup, not a new pattern.

**Phrase structure [inferred from the above plus repo]:**
- One bar loop (boom bap sample loops, OS lo-fi): same kick every bar, variation only in the fill bar.
- Two-bar: bar 2 is the same, with one kick moved or added: "question" (kick at 0, 10) then "answer" (kick at 0, 6 or 0, 14). This matches MusicRadar's note that bars 1/3 and 2/4 differ in timing [repo].
- Four-bar: bars 1-3 near-identical, bar 4 drops the downbeat kick or adds a 16th pickup into the next phrase (Amen).

**Feel (acoustic):**
- Attack Magazine 90s boom bap: kick uses MPC 3000 8th swing 57, hats 16th swing 74; Vol>Vel on the kick; two layered kick samples [src].
- DJ Premier: drums "basic and reserved" with very subtle swing, chops carry the interest [src, secondary].
- Dilla (Hein, Chameleon): downbeat kick in odd-numbered bars sits on the grid; offbeat kicks after beats 3 and 4 get lighter or wider swing than the snare, and not constant; the anticipated snare is late; hats drag [src]. MPC swing runs 50-75%, 66 = triplet; Dilla mixed quantized and unquantized tracks [src, secondary]. No published ms numbers exist for kicks.
- Takeaway: keep the downbeat kick on the grid; give syncopated kicks their own timing, not the snare's.

## 3. Synth / drum-machine family

Characteristics: sampled or synthesized kick (808, 909, CR-78, SP-1200 or MPC kit). Straight grids, constant velocity (machines have only accent), kick often the same sound as the bass. Hip hop drum-machine kicks are busier and more syncopated than the 4-on-floor house default (Hein table: generic hip hop kick 0, 7, 10, 14 vs house 0, 4, 8, 12) [src].

Grids:

| # | Grid | Source / note |
|---|---|---|
| S1 | 0, 7, 10, 14 | "Generic hip-hop" in Hein's drum machine table (1, 8, 11, 15 1-indexed) [src]. |
| S2 | 0, 7, 8, 10, 15 | LIB pattern 2, Fu-Gee-La grid [repo]; MPC-style busy boom bap. |
| S3 | 0, 6, 10 | Chillhop-with-808 default [inferred, matches OS lo-fi stats]. |
| S4 | 0, 10 | Sparse lo-fi 808: two hits and a long 808 tail [repo OS stats, inferred]. |
| S5 | 0, 3, 10 | LIB 3 (Tupac Changes) [repo]. |
| S6 | 0, 3, 8, 10 | Classic 808 electro skip: pair 0-3, then 8-10 [recall, unverified]. |
| S7 | 0, 6, 8, 14 | Bossa-ish / dotted pattern; also clean 808-lo-fi [recall]. |
| S8 | 0, 4, 12 | Half-time snare on 8 with kicks on 2 and 4 (OS recurring `X...X.......X...`) [repo]. |
| S9 | 0, 8 (reggaeton: dembow 0, 4, 8, 12 is a different family) | Reggaeton 1 and 3 + 16th backbeat (Faderpro) [src]; kept out of VNL-1. |
| S10 | 0 + 2-3 syncopations, snare on 8 | Trap: "short, punchy kick on beat 1 plus a couple of syncopated spots", 808 on the same hits, 2-4 808 notes/bar [src]. |

Planet Rock, Sexual Healing, "Just Be Good To Me": GearNews describes the 808 patterns in prose only; no grid is published [src]. I could not verify exact 808-era kick grids and have not invented them; S6 is from memory.

**Feel (synth):**
- NI: kicks locked rigidly to the grid sound "plodding"; swung kick should be lowered in velocity [src].
- Machine swing is a global percent on 16ths (MPC 50-75%); SP-1200 and MPC-60/3000 are the canonical boom-bap swing boxes [src, secondary].
- Lo-fi synth drums: soft, muffled, inconsistent velocity (Violet, [src]); repo Lakh velocity median kick 0.8-0.9 [repo].

## 4. Kick vs bass; density by section

- **808 as the bass:** kick and 808 are triggered by the same hits; 2-4 808 notes per bar, "the space between them is what makes the hits feel heavy"; shorten notes so two do not overlap [src, Violet trap]. Kick transient leads, the 808 ducks under it (SongChecks) [src].
- **Kick + separate bass (acoustic/MPC family, jazz-hop):** the bass sits with the kick; roots on kick hits (already VNL-1 behavior, `auto-accompaniment-comparison.md`), but bass may add 1-2 notes the kick does not play (pickup, passing note).
- **Layering:** Hi-Tek layers 808 under sampled kicks [src, forum, low reliability]. Attack Magazine layers two kick samples [src].
- **Density by section [inferred]:**
  - Intro: no kick, or step 0 only.
  - Groove/verse: full pattern, 2-4 kicks.
  - Break/MID_KICK_OUT: kick out entirely (existing map) or just step 0.
  - Last bar of a block (fill): pickup kick on 14 or 15, or drop beat-1 of the next bar like Amen bar 4.
  - Rule: more kicks in later bars of a phrase, never more than 5 in a bar.

## 5. Anti-patterns (what reads wrong)

1. **Four on the floor** (0, 4, 8, 12): house/disco, Faderpro calls it "the most rooted" pattern [src]. Not hip hop unless it is deliberate half-time gating.
2. **March: 0, 8 (or 0, 4, 8, 12) with no syncopation, identical every bar**: "plodding" [src, NI]. Needs a pickup or an answer bar.
3. **Kick on the snare's step (4 or 12) in a backbeat pattern:** rock stomp, hides the backbeat. OK in half-time with snare on 8, not otherwise [inferred].
4. **Too busy:** more than 4-5 kicks per bar in lo-fi, or two adjacent-16th pairs in every bar. 0, 7, 8, 10, 15 (LIB 2) is already the ceiling; keep such bars rare [inferred; repo stats show 3-4 typical].
5. **Every syncopation at full velocity and straight timing**: sounds like a MIDI file. Ghost kicks should sit 0.2-0.3 lower [src NI, inferred numbers].
6. **Syncopated kick placed after the snare for no reason (e.g. 5, 13, 9 together)**: crowded around the backbeat; prefer kick before the snare ("pre-empts") [src Faderpro].
7. **Overlapping 808 notes / kick and 808 on different rhythms** in synth lo-fi: mud [src].
8. **Kick on 15 every bar**: constant pickup flattens the fill; reserve for phrase ends [inferred].
9. **Dembow/reggaeton 16th-backbeat kicks** and trap-style 808 rolls: wrong genre for lo-fi.

## 6. For VNL-1 (current LIB_PATTERNS kicks, decoded from the hex)

| ID | Kick steps | Verdict |
|---|---|---|
| 1 | 0, 6, 10, 14 | Acoustic fits (sampled/live boom bap) and synth fits. Keep. 14 is a pickup; consider using it only on alternate bars. |
| 2 | 0, 7, 8, 10, 15 | Synth/MPC fit, acoustic too busy for live-kit lo-fi. Keep for sampled boom bap, restrict by section; use the 0-1 or 7-8 pair only in alternate bars. |
| 3 | 0, 3, 10 | Fits both; the best generic pattern. Keep. |
| 4 | 0, 6 | Half-time, both fit; a bit plain (two kicks). Add 10 as a ghost (it already is). Keep. |
| 5 | 0, 4, 12 | Synth only (half-time, OS-style). Reads "march/stomp" on acoustic kit since kicks fall on beats 2 and 4. Keep for synth lo-fi, exclude from acoustic kits. |
| 6 | 0, 6 | Same as 4 (sparse); merge or vary: give 6 a different kick like 0, 10. |
| 7 | 0, 8 | Brushes/jazz-hop only. The plain 1+3 is a march outside jazz-hop; add a ghost pickup (e.g. 14) on alternate bars, or replace with 0, 10. |
| 8 | 0, 3, 8, 11 | Neither: not a bossa kick; 3 and 11 are "a" hits with 8 beside them, so it sounds like an electro skip. Replace with the bossa-style 0, 6, 8, 14 [recall], or keep as electro and move out of the bossa slot. |

Suggested additions (acoustic): `0, 7, 8` (NI), `0, 3, 6, 10` (Back to Life), `0, 10`, `0, 8, 10`, 4-bar Amen-style set: [0, 10], [0, 2, 10], [0, 10], [10, 14] (kicks per bar; bar 4 drops the downbeat) [adapted from src, my steps].

Suggested additions (synth, 808): `0, 10` plus 808 follows; `0, 6, 10`; `0, 3, 8, 10` (electro skip, [recall]); 2-3 syncopated variants following trap rule "beat 1 + a couple syncopations".

Suggested rule changes (all [inferred], to test before adopting):
- Two-bar kick phrases: bar 2 moves or adds exactly one kick (steps 6, 7, 10 or 14).
- Fill bar: drop step 0 and add a pickup on 14/15 (Amen bar 4).
- Timing: step 0 on the grid, syncopated kicks have their own late/early offset (0 to +10 ms) separate from the snare's late 13-27 ms. Per family: acoustic swing 54-60%, synth 52-58%.
- Velocity: main (0, 8) 0.85-1.0; syncopated 0.6-0.8; ghosts 0.4-0.55; machine family narrower spread (±0.05) than acoustic (±0.15).
- Synth family: tie bass notes to kick hits and cap at 2-4 per bar; acoustic family: bass roots on kick plus 1 extra note.
- Do not use the kick lane to raise density beyond 5.

## Sources

- Hein, drum machine programming (kick slot tables for hip hop, Funky Drummer, Impeach, Cold Sweat, Levee): https://ethanhein.substack.com/2010/drum-machine-programming
- Violet Recording, hip hop patterns: https://violetrecording.com/best-drum-patterns-for-hip-hop/
- Violet Recording, trap patterns: https://violetrecording.com/best-drum-patterns-for-trap/
- Faderpro, 5 kick and snare patterns: https://blog.faderpro.com/techniques/5-kick-and-snare-patterns-you-need-to-know/
- Hein, building the Amen break: https://ethanhein.substack.com/p/building-the-amen-break
- Hein, Amen listening guide: https://www.ethanhein.com/wp/2025/amen-break-listening-guide/
- Hein, Dilla time in Chameleon: https://ethanhein.com/wp/2022/dilla-time-in-chameleon/
- Hein, natural history of the Funky Drummer break: https://ethanhein.substack.com/p/the-natural-history-of-the-funky-drummer-break
- Native Instruments, hip hop drums 101: https://www.native-instruments.com/blogs/blog/hip-hop-drums-101
- Native Instruments, best drum breaks: https://www.native-instruments.com/blogs/blog/best-drum-breaks
- Attack Magazine, 90s boom bap: https://www.attackmagazine.com/technique/beat-dissected/90s-boom-bap-hip-hop/
- SongChecks, mixing bass and kick in hip hop: https://www.songchecks.com/blog/how-mix-bass-and-kick-in-hip-hop
- GearNews, famous 808 patterns (no grids): https://www.gearnews.com/the-rhythm-composer-famous-808-patterns
- Soundfly, famous hip hop rhythms (not retrievable, 403): https://flypaper.soundfly.com/produce/a-compendium-of-historys-most-famous-hip-hop-rhythms/
- Peterson, Dilla microtiming (abstract only): https://uoregon.academia.edu/SPeterson
- Big Noise Radio, Dilla Time / MPC swing: https://bignoiseradio.com/dilla-time-simple-complex-music-production-techniques/
- PocketVAE (hip hop 2-bar corpus, no kick stats): https://arxiv.org/pdf/2107.05009
- Groove MIDI Dataset: https://magenta.tensorflow.org/datasets/groove

## Open items

- No verified transcription of Funky Drummer, Impeach or 808-era records at 16th resolution; the best path is MIDI from the NI Rudiments break pack or measuring the audio. Hein's table slot meaning needs checking.
- No published ms numbers for Dilla kick offsets; the best measurement would be our own onset analysis of a few records.
