# Lo-fi MIDI arrangements: what full-song MIDI shows about form

> Source material, kept as researched. The distilled, current reading (general musicianship vs. lo-fi choices) is in [`docs/music/`](../music/README.md).

Follow-up to [lofi-hip-hop.md](lofi-hip-hop.md) (sections 6 and 7 cover the Magenta loops and the generator projects; this report doesn't repeat them). The goal was multi-track, whole-song (32+ bar) lo-fi MIDI, analyzed for arrangement: who plays when, section lengths, intros, breaks, outros, melody reuse, loop lengths, and drum grids.

**Labels.** **[measured]** means computed by the stdlib parser over the files. **[inferred]** means my interpretation. Role labels (drums, bass, chords, lead) are themselves inferred from program, register and polyphony (see Method), so every role-based number inherits that error.

## 0. Bottom line

- **There is no clean, licensed corpus of professional lo-fi arrangements in MIDI.** Dataset, vendor and transcription searches turned up only short loops (4–8 bars), audio-only datasets, or downloads behind logins or paywalls (section 1.3).
- **What I analyzed:**
  - **127 multi-track, 32+ bar lo-fi arrangements** from Online Sequencer (user-made, amateur, many covers). 24 of them form a stricter "core" set: at least 48 bars, has drums, bass, harmony and lead, and an effective tempo of 60–100 BPM.
  - **20 full-length 1990s jazz/soul-sampling hip hop transcriptions** from the Lakh MIDI "clean" subset (Us3, Fugees, Nas, Warren G, Biggie, Tupac, Groove Theory and others), as a professional-form proxy for the boom-bap lineage lo-fi comes from.
- **Strongest patterns [measured, all three sets agree]:**
  1. **The groove starts almost immediately.** Drums play from bar 1 in 58% of the Online Sequencer files and 15 of 19 Lakh files. When there is a drumless intro, it is 4 or 8 bars of chords or keys alone.
  2. **The groove is a loop. Sections are layer swaps on 4/8/16-bar boundaries**, not new material. Section lengths are a median of 8 bars, and harmony loops every 4 bars most often (then 8, 1, 2).
  3. **There is one mid-song drum dropout ("break")** in 38% of the Online Sequencer songs. Breaks of 2 bars or more cluster at **40–65% of the song**. They last 4, 8 or 16 bars, and keys or bass carry them. In 14 of 34 cases, 1-bar drum drops sit on the **last bar of an 8-bar phrase** (a turnaround).
  4. **Density is about 3.3–3.8 roles per bar.** The full band plays most of the time.
  5. **Melodies are restated more than invented.** Of the 4-bar lead phrases, 41–58% exactly repeat an earlier phrase, 14% are variants, and the rest are new. Restatement rises toward the end of the song.
  6. **Endings are usually abrupt or strip down to one layer.** In 63% of the Online Sequencer files and 14 of 19 Lakh files the drums play to the last bar. When there is an outro, it is commonly lead or keys alone for 1–8 bars.

## 1. Sources

### 1.1 Analyzed

| Source | What | Files used | License / terms |
|---|---|---|---|
| [Online Sequencer](https://onlinesequencer.net/sequences?search=lofi) (user sequences, fetched via the site's public `app/api/get_proto.php` endpoint that the player uses; decoded from protobuf with stdlib) | User-made lo-fi beats and covers (Nujabes "Feather", "Reflection Eternal", "Lady Brown", "Moon Strut", "Horizon"; "idealism"; game or anime tunes "lofi'd") | 569 sequences fetched from search terms lofi, lo-fi, lofi hip hop, lofi beat, lofi girl, chillhop, chill hop, nujabes, jazz hop, idealism, j dilla, study beat. **127** passed the filter (at least 32 bars, drums plus 2 or more other roles, mean of 2 or more roles/bar, deduped by title). **24** "core". | **No published terms found** (`/terms` 404). User-generated content, with copyright held by the authors. Many are covers of copyrighted songs. **Analyzed for learning only.** Don't redistribute or embed the note data. |
| [Lakh MIDI Dataset, clean_midi](https://colinraffel.com/projects/lmd/) (Raffel 2016) | Karaoke-style full-song transcriptions, artist/title named | **20** hip hop / R&B songs (list in Appendix A) | Dataset is distributed under CC-BY 4.0. **The songs are copyrighted compositions.** Learning only. |
| Magenta Lo-Fi Player loops | already covered in lofi-hip-hop.md section 6.1 | 0 new | Apache-2.0 |

Download location: `/private/tmp/claude-501/-Users-samueloltz-Documents-fm-1/dfd13d60-fb03-409f-86fe-9daa1a019026/scratchpad/midi2/` (`onlinesequencer/`, `lakh_clean/`).

Scripts are in `/private/tmp/claude-501/-Users-samueloltz-Documents-fm-1/dfd13d60-fb03-409f-86fe-9daa1a019026/scratchpad/tools/`. All are stdlib-only and run with `python3 -I`:
- `loaders.py`: Online Sequencer protobuf and SMF loading.
- `arr.py`: arrangement analysis.
- `agg.py`: aggregates.
- `partstats.py`: per-part stats.
- `maps.py`: map rendering.

### 1.2 Caveats on the material [inferred]

- **Online Sequencer is amateur.** It's a browser piano roll used mostly by hobbyists and teens.
- **No dynamics.** Velocities are almost all 1.0, so velocity stats from it are meaningless.
- **Note lengths are unreliable.** Note lengths are often 1 step (instruments ring out regardless), so chord "durations" are useless.
- **Tempo is often doubled.** BPM is frequently set high with a half-time drum pattern (24% of files put the snare on beat 3 only). I report an "effective tempo" that halves those.
- **Lakh hip hop files are pro-ish transcriptions of 1990s singles,** not lo-fi. Rap vocals are usually absent or reduced to a melody track. They show *professional loop-based form*, not lo-fi timbre or the "study beat" 2–3 minute shape.
- **Neither set has the exact Lofi Girl / Chillhop production style.** That style lives in DAW projects and audio, not public MIDI.

### 1.3 Searched and not usable (and why)

| Source | Result |
|---|---|
| [Kaggle "Lo-Fi Hip Hop MIDIs" (zakarii)](https://www.kaggle.com/datasets/zakarii/lofi-hip-hop-midi) | Kaggle download requires login (not done). Per [aaronjenson/AI-Lofi](https://github.com/aaronjenson/AI-Lofi), it is "mostly just the melody", with few chords and no bass. |
| GitHub lo-fi generator repos ([Melo04](https://github.com/Melo04/lofi-generator-LSTM), [Natroness/LoFi](https://github.com/Natroness/LoFi), [Yikemlone/lofi-ai](https://github.com/Yikemlone/lofi-ai), [jacek-mcp](https://github.com/jacek-mcp/LoFi-music-generator), [maheshtnt](https://github.com/maheshtnt/MusicGen-GAN), [callaunchpad/lofi-bytes](https://github.com/callaunchpad/lofi-bytes)) | All hold the same ~93 Cymatics/"MLP" chord loops (255 B–10 KB, 1–8 bars), model outputs, or 1–2 audio-to-MIDI piano files. No licenses. No arrangements. |
| Hugging Face (`lofi`, `lo-fi`, `chillhop` searches) | All audio (mp3/spectrogram parquet) or tokenized blobs of the same 93 Cymatics loops (`YuriRak/Lofi_Remi_Token_Dataset`). No MIDI arrangements. |
| BitMidi (API), MIDIWorld, freemidi | No lo-fi, Nujabes, Jinsang or Chillhop entries. |
| MuseScore (several "Aruarian Dance" arrangements, including [piano and drums](https://musescore.com/user/57343164/scores/32521661)) | MIDI download requires login. Skipped per the rules. |
| Vendor packs: [Beach Roads](https://beachroads.gumroad.com/l/lofi-piano-midi-files-pack-chill-melody-and-chord-progressions), [Clark Audio](https://clarkaudio.com/free-lofi-midi-chord-pack/), [Essential MIDI free hip hop](https://essentialmidi.com/products/free-hip-hop-midi-pack), [Boyss "MIDI Song Starters"](https://boyss-sound-e-scapes.gumroad.com/l/midi-song-starters), Loopmasters lo-fi kits | Loops or 8-bar construction kits, not arrangements. Email-gated or paid. The Boyss full version (multitrack kits) is paid. |
| GigaMIDI / MetaMIDI | Access-gated and not lo-fi-tagged. Not pursued. |
| Patchbanks WaivOps (Lo-Fi Drums, HH-LFBB) | Audio loops only. |

## 2. Method [measured pipeline, inferred role labels]

- **Normalizing to notes.** Every file becomes notes (start beat, duration, pitch, velocity, track).
  - Online Sequencer notes come from its protobuf: field 1 pitch (equals MIDI number), field 2 time in 16th steps, field 3 length, field 4 instrument, field 5 volume.
  - Drum-kit pitches are converted to General MIDI using the site's own export tables (808, 909, 8-bit and 2013 kits).
- **Roles per track:**
  - drum kit or channel 10 → **drums**
  - GM bass program, or median below C3 and mostly monophonic → **bass**
  - 1.8 or more notes per onset → **chords**
  - pad or strings program with long notes → **pad**
  - median at A3 or above → **lead** (the busiest one; others are **lead2**)
  - anything else → **other** (usually mid-register keys or arpeggios)
- **Activity.** A role is "active" in a bar if any note starts in it, or a held note covers at least half of it.
- **Sections** are runs of 4-bar blocks with the same active-role set. A role counts in a block if it is active in 2 or more of its 4 bars.
- **Entries and exits** are the first and last bars of a 2-bar-or-longer run of activity.
- **Drum dropouts** are bars with no drums between the first and last drum bars.
- **Harmonic loop period.** For non-drum, non-lead pitch-class sets per bar, the smallest P in {1, 2, 4, 8, 16} where at least 70% of bar pairs (b, b−P) have Jaccard similarity of 0.75 or more.
- **Lead phrases.** Each 4-bar lead phrase is a set of (16th position, pitch). Each is compared with every earlier phrase: **R** is identical, **v** is Jaccard of 0.5 or more, **N** is new.
- **Drum grids** are per-instrument onset histograms on the 16-step bar. X means the step is hit in at least 60% of that instrument's bars; x means at least 25%.
- **Swing** is the mean position of drum onsets that fall 0.4–0.75 into a beat (8th swing: 0.5 straight, 0.667 triplet) or 0.15–0.4 into a beat (16th swing: 0.25 straight).

## 3. Aggregate results [measured]

### 3.1 Online Sequencer, all 127 lo-fi arrangements
- Length: median **64 bars** (32–241). Stated tempo median 110. **Effective tempo median 90** (24% half-time patterns).
- Mean active roles per bar: median **3.3** (2.1–5.7).
- **Intro:**
  - Drums play from bar 1 in **74/127 (58%)**.
  - The first section (in 4-bar blocks) lasts a median of **8 bars**.
  - Most common first-section contents: chords alone (16), lead alone (10), chords+lead (9), drums+lead (9), full band (9), bass+drums+lead (8).
- **Bass entry relative to drums:** the same bar in 46 of 99 songs. Otherwise **+4 or +8 bars** (7 and 6 songs), occasionally +16 or earlier than drums.
- **Mid-song drum dropouts:** 48/127 songs have one or more.
  - **1 bar:** 34 cases. They fall on bar 8 of the 8-bar phrase (14), bar 5 (7) or bar 7 (6). That is turnaround drops.
  - **2 bars or more:** 46 cases. Lengths are 3–4 bars (15), 7–8 bars (14) and 16 bars (9).
  - Where 2-bar-or-longer dropouts start, as a fraction of the song: 0.43–0.65 (28 of 46). Most are at the midpoint.
  - What plays during 2-bar-or-longer drum breaks: bass+chords (9), chords+lead (8), chords alone (4), chords+lead+lead2 (4). That is the harmony bed, sometimes with the melody.
- **Outro:**
  - Drums play to the end in 80/127 (63%).
  - The last section is a full band (16), **lead/keys alone (14)**, bass+drums+lead (13) or drums alone (8).
  - Median last-section length is 7 bars.
- **Sections per song** (4-bar segmentation): median 5. Section length median **8 bars**.
- **Harmonic loop period** (bars): 4 → 34 songs, 8 → 16, 1 → 17, 2 → 9, 16 → 5, none found → 46. "None" usually means through-composed covers or noisy role mixing.
- **Lead 4-bar phrases** (1,577): **R 41%**, v 14%, N 45%.
- **Timing and feel:**
  - Off-16th-grid onsets: median 4.6%. Most users snap to the grid; a minority draw triplets or 32nds (up to 83%).
  - 8th-swing position: median **0.56** (0.50–0.75), so a light swing.
  - 16th-swing position: median 0.25, so straight 16ths in most files. Up to 0.37 in a few.

### 3.2 Online Sequencer "core" 24 (48+ bars, D+B+harmony+lead, 60–100 BPM effective)
- Length median **72.5 bars** (48–144). Effective tempo median **80**.
- Roles per bar median **3.8**.
- Drums from bar 1 in 18/24. Bass enters a median of 4 bars after the start.
- **Drums run to the end in only 10/24.** The median outro without drums is **2 bars**. The most common last section is **lead/keys alone (5/24)**.
- Drum dropouts: 8/24 songs. Breaks of 2 bars or more are mostly **8 bars** long (4 of 9) and start at 0.43–0.65 of the song (7 of 9).
- Mid-song "breakdown" sections (2 or more roles fewer than the fullest section): median **1.5 per song**.
- Lead phrases: **R 58%**, v 14%, N 28%. Better-made songs repeat more.
- Harmonic loop: 4 bars (8 songs), 8 (4), 2 (4), 1 (4), 16 (1).
- Part stats (medians):
  - Chord stacks are **4 notes** (IQR 3–7), spanning 12 semitones, lowest note E3 (MIDI 52).
  - Bass is 4 notes/bar (IQR 1.4–7.9), median pitch E2. Only 16% of bass notes sit on beat 1, so lines are riff-like rather than root whole notes.
  - Lead is 4.7 notes/bar, with a 20-semitone range centered around Bb4 (MIDI 70). 42% of lead intervals are steps (2 semitones or less).

### 3.3 Lakh 1990s jazz/soul hip hop, 20 songs (form proxy)
- Length median **101 bars** (36–241). Tempo median **95** (81–117, plus one file at a doubled 190).
- Roles per bar median **3.8**.
- Drums from bar 1 in 15/19. Drumless intros are 7–14 bars: Fugees "Ready or Not" 11, "Killing Me Softly" 14, Tupac "Changes" 8, "I Ain't Mad at Cha" 6. Those intros are keys or pad plus vocal hook.
- **Practically no drum breaks** (three 1-bar drops). Professional rap singles keep the beat running.
  - Variation comes from **layer swaps on 8/16-bar verse/hook boundaries**: chords drop out under verses and return for hooks (Big Poppa, Changes, Fantastic Voyage), or the lead/hook enters for 8 bars and leaves for 16.
- **Outro:** drums to the end in 14/19. Otherwise a 1–8 bar tail of keys or bass.
- Lead phrases: **R 51%**, v 14%, N 35%.
- Sections median 8.5 per song, section length **8 bars**. Breakdowns median 1 per song.
- Grid and dynamics: off-grid onsets median 7%, 8th-swing median **0.54** (Snoop "Who Am I" 0.70, Tupac "Changes" 0.62, Warren G "Regulate" 0.60). Hats sit about 0.35–0.65 velocity under kick and snare at about 0.8–1.0.

### 3.4 Drum patterns seen (16 steps per 4/4 bar, measured; X ≥60% of bars, x ≥25%)

Lakh (professional):

| Song | Kick | Snare/clap | Hat/ride |
|---|---|---|---|
| Us3 "Cantaloop" | `X.....X...X...X.` | `....X.......X...` | ride `X.X.X.X.X.X.X.X.`, shaker 16ths |
| Fugees "Fu-Gee-La" | `X......XX.X....X` | `....X.......X...` | `X.X.X.X.X.X.X.X.` |
| Fugees "Ready or Not" | `X..........X....` | `....X....X..X...` | `X.X.X.X.X.X.X.X.` |
| Biggie "Big Poppa" | `X.......X.......` | `....X.......X...` | pedal hat `..XX..X...XX..X.` |
| Biggie "One More Chance" | `X..X..X...x.....` | rim `....X.......X...` | `X.X.X.X.X.x.x.X.` |
| Groove Theory "Tell Me" | `X.......X.......` | `....X.......X...` | `X...X...X...X...` plus shaker 8ths |
| Tupac "Changes" | `X..X......X.....` | `....X.......X...` | 16ths |
| Warren G "Regulate" | `x.X....XX.....X.` | `....X.......X...` | 16ths with open hat on step 15 |

**[Inferred] template:**
- Snare or rim always on 2 and 4.
- Kick on 1 plus one to three syncopated hits: "and of 2" (step 7), "3" (step 9), "and of 3" (step 11), "a of 4" (step 16).
- Hats in straight 8ths, or 16ths at low velocity, with an occasional open hat on the last 16th.

Online Sequencer lo-fi: the most common snare is **half-time** `........X.......` (11 files) or backbeat `....X.......X...` (9). Hats are straight 8ths (10) or quarters (8). Kicks vary widely; `X...X.......X...` and `X.....X.........` recur.

## 4. Selected per-file maps

Grids: one column per bar, `|` every 8 bars, X = role active. Full maps for every file are in Appendices A and B.

**OS #2874622 "idealism"** (83 bars, 120 BPM half-time = 60 effective). This is the clearest "lo-fi song" shape in the set:
```
       |       |       |       |       |       |       |       |       |       |       |
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX........XXXXXXXXXXXXXXXXXXXXXXXX...
bass   XXXXXXXXXXXXXXXXXXXXXXXX........XXXXXXXXXXXXXXXX................XXXXXXXXXXXXXXXX...
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX................XXXXXXXXXXXXXXXX...
lead   .......XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXX
lead2  ........................................XXX.X.XX.......XX......XX..................
other  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX................XXXXXXXXXXXXXXXX...
```
Sections:

| Bars | Section | Who plays |
|---|---|---|
| 1–8 | Intro | groove with no lead |
| 9–24 | A | lead enters |
| 25–32 | A | bass out |
| 33–48 | A | full, plus a countermelody |
| 49–56 | Break | **lead alone** |
| 57–64 | Rebuild | drums and lead |
| 65–80 | A | full |
| 81–83 | Outro | lead tail |

**OS #4817025 "Reflection Eternal – Nujabes" (cover)** (101 bars, 94 BPM):
```
      |       |       |       |       |       |       |       |       |       |       |       |       |
drums ....XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
bass  ........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.............
lead  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.....
lead2 ....XXXX................XXXXXXXXXXX.XXX.XXXXXXXXXXXXXXXX........XXX.XXX.XXXXXXXXXXXXXXXXXXX.XXX......
```
This is a staggered entry: keys first, drums at +4 bars, bass at +8. The main piano drops for 16 bars mid-song (bars 41–56) while a second voice carries it. The outro strips bass, then drums.

**OS #4496747 "feather – nujabes" (cover)** (65 bars, 90 BPM): drums and piano from bar 1, bass at bar 5, a horn for 8 bars at bars 41–48, a 1-bar piano tail.

**OS #4188103 "Snowdin Town [Lofi]"** (58 bars, 85 BPM): drums and lead for 8 bars; bass enters at bar 9; a chord pad enters for 6 bars at bars 25–30 and again at 49–54 (a "B-section color"); the last 2 bars are drums alone.

**OS #3156435 "Ninjago Overture Lofi"** (66 bars, 80 BPM): chords/keys alone for 7 bars, then drums and bass enter together at bar 8, a 56-bar groove, and a 2-bar keys tail.

**Lakh "Ready or Not" (Fugees)** (85 bars, 90 BPM):
```
      |       |       |       |       |       |       |       |       |       |       |
drums ...........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead  ....XXXXXXXX................XXXXXXXX............XXXXXXXXXXXX........XXXXXXXXXXXXXXXXX
lead2 XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
An 11-bar drumless intro (bass and pad). After that the hook (lead) alternates 8 bars on with 12–16 bars off, over an unchanging bed.

**Lakh "Big Poppa" (Biggie)** (92 bars): D+B+C throughout, with the hook lead **8 on / 16 off** three times (bars 1, 25, 49), then a 20-bar final hook.

## 5. What this means for the generator [inferred]

1. **Song skeleton for about 5 minutes at 70–90 BPM (100–110 bars).** Allocate in 8-bar units: intro 4–8 → A 16 → A′ 16 → break 8 (at 40–60%) → rebuild 4–8 → A″ 16–24 → outro 4–8. That matches both corpora's 8-bar section median and the midpoint break cluster.
2. **Intro.** About 60%: start with the drums in (bar 1, or after a 1-bar pickup). About 40%: start with keys/chords alone for 4 or 8 bars. Bring bass in with drums, or 4–8 bars later. Bring the lead in at bar 5–9.
3. **Layer swaps are the main variation tool.**
   - Every 8 (sometimes 16) bars, toggle one layer: the lead or a countermelody, bass, a pad or "color" voice.
   - Keep the drums and the harmonic loop constant.
   - Aim for 3–4 roles active on average and the full band about half the time.
   - Avoid changing more than one layer per boundary, except at the break and the rebuild.
4. **One real break per song.** Drums out for 4 or 8 bars (16 is rare) near the midpoint; keep chords (plus bass or the lead). Add a **1-bar drum drop on bar 8 of some 8-bar phrases** (about 1 in 4 phrases) as a turnaround.
5. **Outro.** Either keep the full groove to a hard stop (common), or strip to keys/lead alone for 2–8 bars, removing bass, then drums. Lo-fi favors the strip-down; professional rap singles favor the hard stop or fade.
6. **Melody budget.** Write two or three 4-bar phrases (A, A′, B). Restate exactly about 50% of the time, vary about 15%, and add new material about 30%. Increase exact restatement toward the end. The lead rests in break sections or carries them alone.
7. **Harmony.** Use a 4-bar loop by default (8 bars as the second option; 1–2 bar vamps are common in covers). Voicings are 4 notes spanning about an octave, with the bottom near E3. Bass is a riff of about 4 notes per bar, not whole-note roots.
8. **Drums.** Snare or rim on 2 and 4 (or a half-time snare on 3 if the tempo is doubled). Kick on 1 plus one to three syncopations from {and-of-2, 3, and-of-3, a-of-4}. Hats in 8ths, with an 8th-swing position of about 0.54–0.60 (Lakh and Online Sequencer agree; the heaviest is 0.70). Hat velocity about 0.4–0.6 against kick and snare at about 0.8–1.0 (from Lakh; Online Sequencer has no velocity).

## 6. Still missing

- **Professional lo-fi in MIDI.** No Lofi Girl, Chillhop or College Music project files, and no multitrack MIDI of Nujabes, Jinsang or idealism outside amateur Online Sequencer covers. Hand-annotating 10–20 real tracks from audio (section timings, which layers enter and exit) would validate the skeleton better than more MIDI hunting.
- **Microtiming and velocity for lo-fi specifically.** Online Sequencer has no dynamics and is mostly grid-snapped. The Lakh numbers are for 1990s hip hop transcriptions. The MIDI found doesn't show Dilla-style feel (late snares, drifting hats). Audio analysis or a pro drum MIDI pack would be needed.
- **Unchecked access-gated sources:** the Kaggle zakarii set (login), MuseScore transcriptions (login), and the paid Boyss multitrack "song starters".

---

# Appendix A: Lakh MIDI clean, 20 hip hop / R&B transcriptions (copyrighted songs; learning only)

Files are under `midi2/lakh_clean/clean_midi/<Artist>/<Title>.mid`. "Still D.R.E." has no drum channel in this transcription.

#### Cantaloop.mid
tempo 117.0 BPM, 4/4, 122 bars, mean active roles/bar 4.02
  track 'A.PIANO 1' prog 0 -> chords (920 notes, median pitch B3)
  track 'A.PIANO 1' prog 0 -> other (691 notes, median pitch C3)
  track 'FRETLESS' prog 35 -> bass (460 notes, median pitch F1)
  track 'TRUMPET' prog 56 -> lead (213 notes, median pitch Bb4)
  track 'TENOR SAX' prog 66 -> chords (140 notes, median pitch C4)
  track 'JAZZ GTR' prog 26 -> chords (60 notes, median pitch Bb4)
  track 'DRUMS' prog 0 -> drums (3470 notes, median pitch A4)
```
       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       | 
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   XXXXXXX.XXXXXXXX..XXXXXXXXXXX.XXXXXXXXXXXXXXX.XXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords XXXXXXX.XXXXXXXX.....X...X.X..XXXXXXXXXXXXXXX..X.X.X.XXXXXXXXXXXXXXXXX...X...X.X..XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   .......XXXXXXXXXX........X....X.X.XXX..........X.X.X.XX.X.XXX.X.X.XXX............XXXXXXXXX........X.X.XXX.................
other  XXXXXXX.XXXXXXXX..XXXXXXXXXXX.XXXXXXXXXXXXXXX.XXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:8:D+B+C+O; 9:8:D+B+C+L+O; 17:8:D+B+O; 25:8:D+B+C+O; 33:4:D+B+C+L+O; 37:12:D+B+C+O; 49:20:D+B+C+L+O; 69:4:D+B+C+O; 73:4:D+B+O; 77:4:D+B+C+O; 81:12:D+B+C+L+O; 93:8:D+B+C+O; 101:4:D+B+C+L+O; 105:18:D+B+C+O
- drums enter bar 1, drum dropouts (start bar,len): [(54, 1)], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `-NNNN-NNNN-NvNRvRR--NNN-RRR----` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): cabasa `XXXXXXXXXXXXXXXX` vel 0.8±0.05; ride `X.X.X.X.X.X.X.X.` vel 0.69±0.04; kick `X.....X...X...X.` vel 0.87±0.03; snare2 `....X.......X...` vel 0.82±0.06; 61 `..X.Xx.x........` vel 0.74±0.1
- off-grid onsets 12%; swing8 0.567; swing16 0.25

#### Fu-Gee-La.mid
tempo 90.0 BPM, 4/4, 98 bars, mean active roles/bar 5.04
  track '' prog 32 -> drums (1894 notes, median pitch F#2)
  track '' prog 123 -> bass (220 notes, median pitch A2)
  track '' prog 120 -> other (55 notes, median pitch E3)
  track '' prog 35 -> bass (318 notes, median pitch Bb1)
  track '' prog 26 -> lead2 (804 notes, median pitch Eb4)
  track '' prog 4 -> lead (838 notes, median pitch Eb5)
  track '' prog 113 -> other (94 notes, median pitch G3)
  track '' prog 79 -> chords (418 notes, median pitch C4)
  track '' prog 52 -> lead2 (242 notes, median pitch F4)
  track '' prog 0 -> chords (24 notes, median pitch G4)
  track '' prog 92 -> lead2 (80 notes, median pitch G4)
  track '' prog 97 -> lead2 (56 notes, median pitch E4)
  track '' prog 33 -> bass (25 notes, median pitch Eb4)
```
       |       |       |       |       |       |       |       |       |       |       |       |       | 
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   XXXXXXXXXXXXXXXXXXXXXXXX....XXXXXXXXXXXXXXXX.XX.XXXXXXXXXXXXXXXXXXX.XXXX....XXXXXXXXXXXXXXXXXXXXXX
chords ....XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX........XXXXXXXXXXXXXXXX..
lead   ....XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX........XXXXXXXXXXXXXXXX..
lead2  ...XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..........
other  XXXXX.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.XXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:4:D+B+O; 5:20:D+B+C+L+L2+O; 25:4:D+C+L+L2+O; 29:44:D+B+C+L+L2+O; 73:4:D+L2+O; 77:4:D+B+L2+O; 81:8:D+B+C+L+L2+O; 89:8:D+B+C+L+O; 97:2:D+B+O
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 2 bars; lead 4-bar phrases: `-NRRRRRRRRRRRRRRRR--RRRv-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X.X.X.X.X.X.X.X.` vel 0.77±0.11; kick `X......XX.X....X` vel 0.72±0.19; ridebell `X...X...X...X...` vel 0.61±0.03; 28 `....X.......X...` vel 0.86±0.0; snare2 `....X.......X...` vel 0.86±0.0
- off-grid onsets 24%; swing8 0.499; swing16 None

#### Ready Or Not.mid
tempo 90.0 BPM, 4/4, 85 bars, mean active roles/bar 3.4
  track 'ooh' prog 53 -> lead2 (211 notes, median pitch C4)
  track 'Bass1' prog 4 -> bass (211 notes, median pitch F#2)
  track 'Lauryn1' prog 65 -> lead (245 notes, median pitch Eb4)
  track 'Lauryn2' prog 65 -> lead2 (124 notes, median pitch C#4)
  track 'Lauryn3' prog 65 -> lead2 (30 notes, median pitch Bb4)
  track 'Drums' prog 25 -> drums (924 notes, median pitch F#2)
  track 'Bass2' prog 94 -> bass (211 notes, median pitch F#2)
```
      |       |       |       |       |       |       |       |       |       |       |    
drums ...........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead  ....XXXXXXXX................XXXXXXXX............XXXXXXXXXXXX........XXXXXXXXXXXXXXXXX
lead2 XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:4:B+L2; 5:8:B+L+L2; 13:16:D+B+L2; 29:8:D+B+L+L2; 37:12:D+B+L2; 49:12:D+B+L+L2; 61:8:D+B+L2; 69:17:D+B+L+L2
- drums enter bar 12, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 4 bars; lead 4-bar phrases: `-NR----RR---RNv--RRRRN` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X.X.X.X.X.X.X.X.` vel 1.0±0.0; snare `....X....X..X...` vel 1.0±0.0; kick2 `X..........X....` vel 1.0±0.0
- off-grid onsets 1%; swing8 0.551; swing16 0.25

#### Killing Me Softly.mid
tempo 94.0 BPM, 4/4, 104 bars, mean active roles/bar 3.26
  track 'melodie' prog 57 -> lead2 (335 notes, median pitch E4)
  track 'organ' prog 16 -> chords (36 notes, median pitch G4)
  track 'sitar' prog 104 -> lead2 (10 notes, median pitch B3)
  track 'bas' prog 39 -> bass (213 notes, median pitch B1)
  track 'Back-Vocal' prog 52 -> lead (407 notes, median pitch G4)
  track 'drums' prog 127 -> drums (1198 notes, median pitch A4)
  track 'FX' prog 102 -> lead2 (20 notes, median pitch B4)
```
       |       |       |       |       |       |       |       |       |       |       |       |       |       
drums  X.............XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   ..........................XXXXXXXXXXXXXXX...........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX......
chords .XXXXXXXXXXXXXX.........................................................................................
lead   ...XXXXXXXXXXX.......XX....X.XXXXXXXXXXX...XX........X.XXXXXXXXXXXX....XXXXXXXXXXX.XXXXXXXXXXXXX.XXXXXXX
lead2  .XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:4:C+L2; 5:8:C+L+L2; 13:4:D+C+L+L2; 17:4:D+L2; 21:4:D+L+L2; 25:4:D+B+L2; 29:12:D+B+L+L2; 41:12:D+L2; 53:16:D+B+L+L2; 69:4:D+B+L2; 73:28:D+B+L+L2; 101:4:D+L+L2
- drums enter bar 15, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `NNNN-NNNNNNN-NNNvNNNRRRNNN-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): cabasa `X.X.X.X.X.X.X.X.` vel 0.45±0.03; snare2 `....X....X..X...` vel 0.71±0.12; kick `X.......X......X` vel 0.51±0.13; stick `X...X...X...X...` vel 0.49±0.01
- off-grid onsets 6%; swing8 0.501; swing16 0.25

#### Regulate.mid
tempo 97.0 BPM, 4/4, 86 bars, mean active roles/bar 5.92
  track '' prog 0 -> drums (2051 notes, median pitch F#2)
  track '' prog 79 -> lead (423 notes, median pitch F#6)
  track '' prog 33 -> bass (488 notes, median pitch B1)
  track '' prog 28 -> chords (1211 notes, median pitch A3)
  track '' prog 4 -> chords (592 notes, median pitch D4)
  track '' prog 48 -> pad (137 notes, median pitch G3)
  track '' prog 7 -> other (228 notes, median pitch G3)
  track '' prog 17 -> chords (280 notes, median pitch F#5)
  track '' prog 0 -> lead2 (371 notes, median pitch E4)
  track '' prog 53 -> lead2 (98 notes, median pitch E4)
  track '' prog 62 -> lead2 (40 notes, median pitch D6)
```
       |       |       |       |       |       |       |       |       |       |       |     
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   .XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords .XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
pad    XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   .XXX.XXX.XXX.XXX.XXX.XXX.XXX.XXX.XXX.XXX.XXX.XXX.XXX.XXX.XXX.XXX.XXX.XXX.XXX.XXX.XXX.X
lead2  ............XXXXX...XXXXXXXXXXXXXXXXX..XXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..XX..XXXXXXXXXX.
other  ....X...X...X...X...X...X...X...X...X...XXXXXXXXX...X...X...XXXXXXXXX...X...XXXXXXXXX.
```
sections, 4-bar blocks (start bar : length : roles): 1:12:D+B+C+P+L; 13:4:D+B+C+P+L+L2; 17:4:D+B+C+P+L; 21:20:D+B+C+P+L+L2; 41:8:D+B+C+P+L+L2+O; 49:12:D+B+C+P+L+L2; 61:8:D+B+C+P+L+L2+O; 69:8:D+B+C+P+L+L2; 77:10:D+B+C+P+L+L2+O
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 16 bars; lead 4-bar phrases: `NRRRRRRRRRRRRRRRRRRRRN` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `XXXXXXXXXXXXXX..` vel 0.36±0.12; kick2 `x.X....XX.....X.` vel 0.8±0.18; clap `....X.......X...` vel 0.85±0.0; snare `....X.......X...` vel 0.8±0.0; openhat `..............X.` vel 0.45±0.08
- off-grid onsets 7%; swing8 0.603; swing16 0.25

#### If I Ruled the World.mid
tempo 93.0 BPM, 4/4, 119 bars, mean active roles/bar 3.5
  track 'Lauryn' prog 75 -> lead (576 notes, median pitch D5)
  track 'Lauryn-L' prog 75 -> lead2 (150 notes, median pitch B4)
  track 'Lauryn-R' prog 75 -> lead2 (148 notes, median pitch F5)
  track 'Plink-L' prog 12 -> lead2 (317 notes, median pitch A6)
  track 'Plink-R' prog 12 -> lead2 (317 notes, median pitch A6)
  track 'Hornz' prog 56 -> chords (72 notes, median pitch A3)
  track 'Weird' prog 46 -> lead2 (72 notes, median pitch D5)
  track 'Stringz' prog 48 -> chords (50 notes, median pitch F4)
  track 'Whodini' prog 83 -> lead2 (58 notes, median pitch B3)
  track 'Drumz' prog 0 -> drums (2837 notes, median pitch Ab2)
  track 'Bass' prog 34 -> bass (317 notes, median pitch A1)
  track 'Mello-D' prog 2 -> lead2 (137 notes, median pitch A4)
```
       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |      
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXX.
bass   ........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..XXXXXXXX.
chords ........................XXXXXXXX........XXXX........XXXXXXXX........................XXXXXXXXXXXXXXXXXXXXXXXX...........
lead   XXXXXXXXX.......XXXXXXXXXXXXXXX.........XXXX....XXXXXXXXXXX...XX..XX...............XXXXXXXXXXXXXXXXXXXXXXXXXXX.........
lead2  ................XXXXXXXXXXXXXXXX............XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.........XXXXXXXXXXXXXXXXXXXXXXXX..XXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:8:D+L; 9:8:D+B; 17:8:D+B+L+L2; 25:8:D+B+C+L+L2; 33:8:D+B; 41:4:D+B+C+L; 45:4:D+B+L2; 49:4:D+B+L+L2; 53:8:D+B+C+L+L2; 61:8:D+B+L+L2; 69:8:D+B+L2; 77:8:D+B; 85:24:D+B+C+L+L2; 109:4:D+B+L+L2; 113:7:D+B+L2
- drums enter bar 1, drum dropouts (start bar,len): [(109, 1)], bars after drums stop: 1
- harmonic loop period: 4 bars; lead 4-bar phrases: `NvN-NvNv--N-NRRNv---NRvvvRRN--` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): cabasa `..X...X.XXxXX.X.` vel 0.61±0.06; kick `X.x...XX.XX.....` vel 0.88±0.05; hat `..X...X...X.....` vel 0.41±0.09; pedalhat `X...X...X...X...` vel 0.61±0.05; clap `....X.......X...` vel 0.82±0.05
- off-grid onsets 13%; swing8 0.54; swing16 0.249

#### Still D.R.E.mid
tempo 96.0 BPM, 4/4, 36 bars, mean active roles/bar 1.92
  track 'TabIt MIDI - Track 1' prog 28 -> chords (576 notes, median pitch Eb5)
  track 'TabIt MIDI - Track 2' prog 33 -> bass (65 notes, median pitch A1)
  track 'TabIt MIDI - Track 3' prog 28 -> chords (576 notes, median pitch Eb5)
```
       |       |       |       |       |   
bass   ...XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:4:C; 5:32:B+C
- harmonic loop period: 2 bars; lead 4-bar phrases: `n/a` (N new, R exact repeat, v variant, - silent)
- off-grid onsets 0%; swing8 None; swing16 None

#### Ain't Nuthin' but a G Thang.mid
tempo 94.0 BPM, 8/4, 47 bars, mean active roles/bar 3.83
  track 'Percussion' prog 25 -> drums (3130 notes, median pitch Bb2)
  track 'Bass' prog 35 -> bass (624 notes, median pitch B1)
  track 'High synth' prog 21 -> lead2 (150 notes, median pitch A6)
  track 'Flute' prog 73 -> lead (236 notes, median pitch A4)
  track 'Synth strings' prog 50 -> lead2 (31 notes, median pitch A5)
  track 'Jazz guitar' prog 26 -> lead2 (141 notes, median pitch F#4)
```
      |       |       |       |       |       |      
drums XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass  ..XXXXXXXXXXXXXXXXXXXXXXX..XXXX....XXXXXXXXXXXX
lead  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead2 XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:32:D+B+L+L2; 33:4:D+L+L2; 37:11:D+B+L+L2
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 1 bars; lead 4-bar phrases: `NvRRRRRRRRRv` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): tamb `XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX` vel 0.54±0.09; hat `X.X.X.X.X.X.X.X.X.X.X.X.X...X.X.` vel 0.91±0.0; kick `X.....X.X.....X.X.....X.X.X...X.` vel 0.99±0.04; 28 `....X.......X.......X.......X...` vel 0.79±0.0; stick `....X.......X.......X.......X...` vel 0.94±0.0
- off-grid onsets 12%; swing8 0.567; swing16 0.25

#### Who Am I (What's My Name).mid
tempo 190.0 BPM, 4/4, 241 bars, mean active roles/bar 4.45
  track 'copyright 1998 by' prog 0 -> drums (2722 notes, median pitch B2)
  track 'Scott Bradlee' prog 0 -> drums (2513 notes, median pitch Bb2)
  track 'rcbscb81@hunterdon.csnet.net' prog 81 -> bass (469 notes, median pitch A1)
  track '' prog 111 -> lead2 (120 notes, median pitch C#5)
  track '' prog 50 -> lead2 (323 notes, median pitch F#5)
  track '' prog 26 -> chords (2978 notes, median pitch D4)
  track '' prog 0 -> chords (2194 notes, median pitch F#4)
  track '' prog 65 -> lead2 (607 notes, median pitch A4)
  track '' prog 76 -> lead2 (25 notes, median pitch A5)
  track '' prog 0 -> lead2 (316 notes, median pitch B4)
  track '' prog 67 -> other (343 notes, median pitch F#3)
  track '' prog 56 -> lead (655 notes, median pitch E5)
  track '' prog 72 -> lead2 (166 notes, median pitch A5)
  track '' prog 55 -> chords (30 notes, median pitch F#4)
  track '' prog 61 -> lead2 (60 notes, median pitch G5)
```
       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX...XXXXXXXXXXXXXXXX...X...XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.............................................
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..XXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
lead   XXXX........................................................XXXXXX.....................................XX..XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.......XXXXXXXX..XXX.XXXXXXXX.X..XX..XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXX.
lead2  XXXXXXXXXXXXXXXXXXXXX.......XXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX........................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX...XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
other  XXXX........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX................................................XXXXXXXXX.......................................XXXXXXXXXXXXXX.....X...XX..XX.XXXXXXXXXXXXXXX.XX..........................................XX.
```
sections, 4-bar blocks (start bar : length : roles): 1:4:D+B+C+L+L2+O; 5:8:D+B+C+L2; 13:8:D+B+C+L2+O; 21:8:D+B+C+O; 29:24:D+B+C+L2+O; 53:8:D+B+C+L2; 61:8:D+B+C+L+L2; 69:16:D+B+C; 85:12:D+B+C+L2; 97:4:D+C+L2; 101:4:D+B+C+L2+O; 105:4:D+B+C+L+L2+O; 109:8:D+B+C+L+L2; 117:8:D+C+L; 125:16:D+B+C+L; 141:8:D+B+C+L+L2; 149:8:D+B+C+L2+O; 157:8:D+B+C+L+L2+O; 165:8:D+B+C+L+L2; 173:24:D+B+C+L+L2+O; 197:4:D+C+L; 201:36:D+C+L+L2; 237:4:D+C+L+L2+O; 241:1:D
- drums enter bar 1, drum dropouts (start bar,len): [(123, 1)], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `N--------------NN--------NNvNNNNNNNNNN-NNNNNNNNNNNNNNNNNNNNN-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): 59 `X...X..xX..xX..x` vel 0.86±0.14; cabasa `X...X...X..xXx.x` vel 0.77±0.18; kick2 `X...X......xx..X` vel 0.91±0.11; hat `X...x.......x..x` vel 0.81±0.18; pedalhat `....x......x....` vel 0.79±0.22
- off-grid onsets 61%; swing8 0.697; swing16 0.317

#### Big Poppa.mid
tempo 84.0 BPM, 4/4, 92 bars, mean active roles/bar 3.6
  track 'Pad1' prog 17 -> chords (955 notes, median pitch B4)
  track 'Pad2' prog 25 -> chords (1004 notes, median pitch B3)
  track 'Bass' prog 37 -> bass (373 notes, median pitch Ab1)
  track 'Drums' prog 0 -> drums (1342 notes, median pitch F#2)
  track 'Ocarina' prog 79 -> lead (238 notes, median pitch B5)
  track 'Harpsichord' prog 6 -> lead2 (49 notes, median pitch B4)
```
       |       |       |       |       |       |       |       |       |       |       |       |   
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   XXXXXXX................XXXXXXXX................XXXXXXXX................XXXXXXXXXXXXXXXXXXXXX
lead2  .......X.......X.......X.......X.......X.......X.......X.......X.......X.......X.......X....
```
sections, 4-bar blocks (start bar : length : roles): 1:8:D+B+C+L; 9:16:D+B+C; 25:8:D+B+C+L; 33:16:D+B+C; 49:8:D+B+C+L; 57:16:D+B+C; 73:20:D+B+C+L
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 8 bars; lead 4-bar phrases: `Nv---NRR---RRR---RRvRRR-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): pedalhat `..XX..X...XX..X.` vel 0.9±0.17; kick `X.......X.......` vel 0.79±0.17; hat `X.......X.......` vel 0.55±0.12; clap `....X.......X...` vel 0.95±0.18; snare `....X.......X...` vel 0.95±0.19
- off-grid onsets 4%; swing8 0.583; swing16 None

#### One More Chance.mid
tempo 91.0 BPM, 4/4, 57 bars, mean active roles/bar 4.26
  track 'Recorder' prog 74 -> lead2 (44 notes, median pitch C5)
  track 'Strings' prog 48 -> chords (118 notes, median pitch E4)
  track 'BOboe' prog 70 -> lead2 (42 notes, median pitch F5)
  track 'EPiano' prog 4 -> chords (74 notes, median pitch B3)
  track 'Piano' prog 1 -> lead (492 notes, median pitch C4)
  track 'Bass1' prog 36 -> bass (28 notes, median pitch C3)
  track 'Bass2' prog 34 -> bass (168 notes, median pitch C2)
  track 'Drumz' prog 0 -> drums (900 notes, median pitch Ab2)
```
       |       |       |       |       |       |       |       |
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   .XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords .XXXXXXXX.X.X.X.XXXXXXXXXXX.X.X.XXXXXXXXXXX.X....XXXXXXXX
lead   .XXXXXXXXX.XXX.XXXXXXXXXXX.XXX.XXXXXXXXXXX.XXX.XXXXXXXXXX
lead2  .XXXXXXXX........XXXXXXXX........XXXXXXXX....XXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:8:D+B+C+L+L2; 9:8:D+B+C+L; 17:8:D+B+C+L+L2; 25:8:D+B+C+L; 33:8:D+B+C+L+L2; 41:4:D+B+C+L; 45:4:D+B+L+L2; 49:9:D+B+C+L+L2
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 16 bars; lead 4-bar phrases: `NvvvvRRRRRRRRRN` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): pedalhat `X.X.X.X.X.x.x.X.` vel 1.0±0.0; kick `X..X..X...x.....` vel 1.0±0.0; stick `....X.......X...` vel 1.0±0.0; 84 `.........X.X....` vel 0.8±0.0; 83 `..........X.X...` vel 1.0±0.0
- off-grid onsets 0%; swing8 0.562; swing16 0.25

#### Mo Money Mo Problems.mid
tempo 104.0 BPM, 4/4, 112 bars, mean active roles/bar 3.82
  track 'Bass' prog 32 -> bass (346 notes, median pitch Ab1)
  track 'Piano' prog 1 -> chords (432 notes, median pitch D6)
  track 'Voice' prog 52 -> lead2 (118 notes, median pitch B4)
  track 'Guitar1' prog 27 -> chords (810 notes, median pitch E4)
  track 'Guitar2' prog 27 -> chords (2133 notes, median pitch E4)
  track 'Guitar3' prog 27 -> chords (1935 notes, median pitch E4)
  track 'Trumpet' prog 56 -> lead2 (28 notes, median pitch Ab4)
  track 'Drums' prog 0 -> drums (2684 notes, median pitch F#2)
  track 'Chorus' prog 54 -> lead (275 notes, median pitch Ab4)
```
       |       |       |       |       |       |       |       |       |       |       |       |       |       |       
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   X...XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..X.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   ........................XXXXXXXX................XXXXXXXX........................XXXXXXXXXXXX....XXXXXXXXXXXXXXXX
lead2  XX.XXX.XXX.XXX.XXX.X...X...X...XXX.XXX.X...X...X...X...X...X...XXX.X...X...XXX.XX..XX..XX..XXX.XX..XX..XX..XX..X
```
sections, 4-bar blocks (start bar : length : roles): 1:4:D+C+L2; 5:16:D+B+C+L2; 21:4:D+B+C; 25:8:D+B+C+L; 33:8:D+B+C+L2; 41:8:D+B+C; 49:8:D+B+C+L; 57:8:D+B+C; 65:4:D+B+C+L2; 69:4:D+B+C; 73:4:D+B; 77:4:D+B+C+L2; 81:12:D+B+C+L+L2; 93:4:D+B+C+L2; 97:16:D+B+C+L+L2
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 4 bars; lead 4-bar phrases: `------NR----RR------RRR-RRRR-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `XXXxX.XXXXXXXXXX` vel 0.55±0.09; snare `..x.Xx..xxxxx.xx` vel 0.73±0.16; kick `x.x.....X..X..X.` vel 0.84±0.12; tomL `........XXXXX.XX` vel 0.62±0.13; 81 `X.........X.....` vel 0.34±0.05
- off-grid onsets 7%; swing8 0.6; swing16 0.25

#### Gangsta's Paradise.mid
tempo 80.8 BPM, 4/4, 80 bars, mean active roles/bar 4.24
  track 'Rap drumset' prog 25 -> drums (1098 notes, median pitch Bb4)
  track 'string 1' prog 48 -> lead2 (545 notes, median pitch G5)
  track 'fretless' prog 35 -> bass (365 notes, median pitch Ab2)
  track 'choir aahs' prog 52 -> chords (420 notes, median pitch C5)
  track 'string 1' prog 48 -> lead (557 notes, median pitch G4)
  track 'fretless' prog 35 -> bass (365 notes, median pitch Ab1)
  track 'aahs' prog 52 -> chords (560 notes, median pitch F3)
  track 'aahs' prog 52 -> bass (12 notes, median pitch F#2)
```
       |       |       |       |       |       |       |       |       |       |       
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX....
bass   XXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords ................XXXXXXXX................XXXXXXXX........XXXXXXXXXXXXXXXXXXXXXXXX
lead   XXXXXXXXXXXXXXXXXXXXXXXX....XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX....
lead2  XXXXXXXXXXXXXXXXXXXXXXXX....XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX....
```
sections, 4-bar blocks (start bar : length : roles): 1:16:D+B+L+L2; 17:8:D+B+C+L+L2; 25:4:D+B; 29:12:D+B+L+L2; 41:8:D+B+C+L+L2; 49:8:D+B+L+L2; 57:20:D+B+C+L+L2; 77:4:B+C
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 4
- harmonic loop period: None bars; lead 4-bar phrases: `NNRRvv-RRRRRRRRRRRR--` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): maraca `X.X.X.X.X.X.X.X.` vel 0.79±0.13; kick2 `X.......X.X.....` vel 0.85±0.04; clap `....X.......X...` vel 0.81±0.1; snare `....X.......X...` vel 0.67±0.05; stick `X...X...X...X...` vel 0.38±0.07
- off-grid onsets 5%; swing8 0.5; swing16 None

#### Fantastic Voyage.mid
tempo 116.0 BPM, 4/4, 200 bars, mean active roles/bar 3.3
  track 'PIANO' prog 4 -> chords (1041 notes, median pitch G4)
  track 'ORGAN' prog 17 -> chords (896 notes, median pitch G5)
  track 'GUITAR 1' prog 27 -> lead (1703 notes, median pitch D4)
  track 'CELESTA' prog 8 -> lead2 (12 notes, median pitch C#5)
  track 'SYNTH' prog 102 -> lead2 (210 notes, median pitch G6)
  track 'BASS 1' prog 34 -> bass (1018 notes, median pitch D2)
  track 'BASS 2' prog 35 -> bass (927 notes, median pitch C2)
  track 'SYNTH B.1' prog 87 -> bass (472 notes, median pitch G1)
  track 'DRUMS' prog 0 -> drums (2690 notes, median pitch F#2)
  track 'PERC' prog 0 -> drums (1188 notes, median pitch Eb2)
```
       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   .XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords .....................XXXXXXXXXXXXXXXXXXXXXXXX........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX......................................................XXXXXXXXXXXXXXXX............................XXXXXXXXXXXXXXXXX
lead   ................XXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXX.......................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..
lead2  ....X.....X.....X............................XX......................................XX..............XX.........X.....X.....X..............................XX..............XX...........................
```
sections, 4-bar blocks (start bar : length : roles): 1:16:D+B; 17:4:D+B+L; 21:24:D+B+C+L; 45:4:D+B+L+L2; 49:4:D+B+L; 53:32:D+B+C+L; 85:4:D+B+L+L2; 89:12:D+B+L; 101:4:D+B+L2; 105:20:D+B; 125:16:D+B+L; 141:16:D+B+C+L; 157:28:D+B+L; 185:16:D+B+C+L
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `----NNvvRNvNNvRRRRvRRRvRRN-----vRRNNRRvvRRRRRvRRRv-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `x.X.X.XxX.X.X.X.` vel 0.81±0.0; clap `....X.......X...` vel 0.68±0.0; kick `X.....xxX..x....` vel 1.0±0.0; snare `....X.......X...` vel 1.0±0.0; cabasa `....X.X.........` vel 0.77±0.0
- off-grid onsets 6%; swing8 0.543; swing16 0.25

#### Changes.mid
tempo 111.0 BPM, 4/4, 125 bars, mean active roles/bar 3.28
  track '2pac' prog 35 -> bass (247 notes, median pitch D2)
  track 'Changes' prog 3 -> chords (621 notes, median pitch C5)
  track '' prog 53 -> bass (247 notes, median pitch D2)
  track 'Sequenced by TOPLIST TEAM' prog 0 -> lead (414 notes, median pitch D4)
  track 'By Izzet Selanik' prog 0 -> lead2 (143 notes, median pitch B4)
  track '' prog 53 -> bass (11 notes, median pitch C2)
  track 'Our website: http://www.foxmusic.nl/midi/' prog 49 -> bass (247 notes, median pitch D2)
  track 'E-mail: toplist@foxmusic.nl' prog 52 -> pad (27 notes, median pitch B3)
  track '' prog 0 -> drums (3468 notes, median pitch F#2)
  track '' prog 3 -> chords (24 notes, median pitch G3)
  track '' prog 35 -> bass (11 notes, median pitch C2)
```
       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |    
drums  .......XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX........
bass   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX........
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXX.....................XXXXXXXXXXXXXXXXXXX....................XXXXXXXXXXXXXXXXXXXX.................
pad    .............................XXXXXXXXXXXX............................XXXXXXXXXXXX............................XXXXXXXXXXXX....
lead   XXXXXXX......................XXXXXXXXXXXXXXXX........................XXXXXXXXXXXXXXXX.......................XXXXXXXXXXXXXXXXX
lead2  ..............................X.X.X.X.X.X.X.X.........................X.X.X.X.X.X.X.X.........................X.X.X.X.X.X.X.X
```
sections, 4-bar blocks (start bar : length : roles): 1:8:B+C+L; 9:20:D+B+C; 29:4:D+B+P+L; 33:8:D+B+P+L+L2; 41:4:D+B+L+L2; 45:4:D+B; 49:20:D+B+C; 69:4:D+B+P+L; 73:8:D+B+P+L+L2; 81:4:D+B+L+L2; 85:4:D+B; 89:20:D+B+C; 109:4:D+B+P+L; 113:4:D+B+P+L+L2; 117:4:P+L+L2; 121:5:L+L2
- drums enter bar 8, drum dropouts (start bar,len): [], bars after drums stop: 8
- harmonic loop period: None bars; lead 4-bar phrases: `NN-----NvvRN-----RRRvv-----RRvvv` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `XXXXXXXXXXXXXXXX` vel 0.64±0.07; cabasa `X...X...X...X...` vel 0.91±0.0; kick2 `X..X......X.....` vel 0.98±0.0; snare2 `....X.......X...` vel 0.98±0.0; kick `X...............` vel 0.99±0.01
- off-grid onsets 44%; swing8 0.62; swing16 0.245

#### I Ain't Mad at Cha.mid
tempo 120.0 BPM, 4/4, 83 bars, mean active roles/bar 2.78
  track '' prog 0 -> chords (1086 notes, median pitch Bb3)
  track '' prog 0 -> drums (1007 notes, median pitch F#2)
  track '' prog 48 -> chords (226 notes, median pitch C4)
  track '' prog 35 -> bass (170 notes, median pitch F1)
```
       |       |       |       |       |       |       |       |       |       |       |  
drums  ......XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
bass   .......XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX....XXXXXXXXXXXXXXXXXXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:4:C; 5:4:D+C; 9:48:D+B+C; 57:4:D+C; 61:23:D+B+C
- drums enter bar 7, drum dropouts (start bar,len): [], bars after drums stop: 1
- harmonic loop period: None bars; lead 4-bar phrases: `n/a` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X.X.X.X.X...X.x.` vel 0.63±0.0; kick2 `X.....X.X.......` vel 0.7±0.0; stick `....X.......X...` vel 0.67±0.0; openhat `..........X...x.` vel 0.71±0.0; crash `X...............` vel 0.47±0.0
- off-grid onsets 1%; swing8 0.5; swing16 None

#### Mr Wendal.mid
tempo 110.0 BPM, 4/4, 114 bars, mean active roles/bar 3.79
  track 'FINGERDBAS' prog 33 -> bass (391 notes, median pitch Ab1)
  track 'STEEL GTR' prog 25 -> chords (468 notes, median pitch Eb4)
  track 'CLARINET' prog 71 -> lead2 (48 notes, median pitch F#4)
  track 'CHOIR AAHS' prog 52 -> lead (313 notes, median pitch Eb4)
  track 'MUTED GTR' prog 28 -> other (160 notes, median pitch C#3)
  track 'MUTED GTR' prog 28 -> other (159 notes, median pitch G3)
  track 'KALIMBA' prog 108 -> lead2 (246 notes, median pitch Eb5)
  track 'CLEAN GTR' prog 27 -> chords (16 notes, median pitch Bb1)
  track 'DRUMS' prog 0 -> drums (1574 notes, median pitch F#2)
  track 'MELO TOM 1' prog 117 -> bass (8 notes, median pitch Eb2)
  track 'CHOIR AAHS' prog 52 -> lead2 (146 notes, median pitch Eb4)
```
       |       |       |       |       |       |       |       |       |       |       |       |       |       |       | 
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   ..XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX....XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.......X....XXXXXXXX........
chords XXXXXXXXXX........XXXXXXXXXXXXXXXX........XXXXXXXXXXXXXXXXXXXX........................XXXXXXXX....................
lead   ..XXXXXXXX................XXXXXXXX....................XXXXXXXX...........X...XXXXXXXXX............XXXXXXXXXXXXXXXX
lead2  ..XXXXXXXX................XXXXXXXXXXXXXXXXXXXX........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.........XXXXXXXXXXXXXXXX
other  ..XXXXXXXX................XXXXXXXX...XX..XX..XX.......XXXXXXXX................XXXXXXXX............XXXXXXXX........
```
sections, 4-bar blocks (start bar : length : roles): 1:12:D+B+C+L+L2+O; 13:4:D+B; 17:8:D+B+C; 25:12:D+B+C+L+L2+O; 37:4:D+B+L2+O; 41:8:D+B+C+L2+O; 49:4:D+B+C; 53:12:D+B+C+L+L2+O; 65:12:D+B+L2; 77:8:D+B+L+L2+O; 85:4:D+B+C+L+L2+O; 89:8:D+C; 97:12:D+B+L+L2+O; 109:6:D+L+L2
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 1 bars; lead 4-bar phrases: `NvN---RRR----RRR--NNvv--vRRRR` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X.X.X.X.X.X.X.Xx` vel 0.45±0.03; kick2 `X.........X.....` vel 0.91±0.06; snare2 `....X.......X...` vel 0.95±0.02; tamb `..XX..XX..XX..XX` vel 0.37±0.01; cowbell `..X...X...X...X.` vel 0.31±0.02
- off-grid onsets 0%; swing8 0.541; swing16 0.25

#### Tell Me.mid
tempo 93.1 BPM, 4/4, 96 bars, mean active roles/bar 4.0
  track 'Girl' prog 4 -> lead (628 notes, median pitch F#4)
  track 'Dude' prog 26 -> lead2 (207 notes, median pitch B3)
  track 'Backup' prog 52 -> chords (686 notes, median pitch Eb4)
  track 'Piano' prog 1 -> chords (751 notes, median pitch Eb4)
  track 'Stringz' prog 48 -> chords (48 notes, median pitch C#4)
  track 'Drumz' prog 0 -> drums (1728 notes, median pitch Ab2)
  track 'Bass' prog 34 -> bass (749 notes, median pitch Ab1)
```
       |       |       |       |       |       |       |       |       |       |       |       |       
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords ........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   .....XX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXX.......XXXX....XXXXXXXX
lead2  ......................................................XXXXXXXXXXX.XXXXXXX.......XXXXXXXX........
```
sections, 4-bar blocks (start bar : length : roles): 1:4:D+B; 5:4:D+B+L; 9:44:D+B+C+L; 53:20:D+B+C+L+L2; 73:8:D+B+C; 81:4:D+B+C+L+L2; 85:4:D+B+C+L2; 89:8:D+B+C+L
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `-NNNNNNNvNvNNNNNNNN-N-NN-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): 81 `X.x.x.x.X.x.X.X.` vel 0.61±0.06; hat `X...X...X...X...` vel 0.93±0.09; pedalhat `..X...X...x...X.` vel 0.93±0.11; kick `X.......X.......` vel 0.98±0.09; snare2 `....X.......X...` vel 0.99±0.06
- off-grid onsets 44%; swing8 0.506; swing16 None

#### Hey Lover.mid
tempo 88.0 BPM, 4/4, 105 bars, mean active roles/bar 4.36
  track '' prog 0 -> drums (2163 notes, median pitch Ab2)
  track '' prog 26 -> lead2 (208 notes, median pitch Eb4)
  track '' prog 4 -> chords (765 notes, median pitch E4)
  track '' prog 0 -> bass (224 notes, median pitch F1)
  track '' prog 27 -> chords (560 notes, median pitch F#4)
  track '' prog 52 -> lead2 (15 notes, median pitch G4)
  track '' prog 17 -> chords (11 notes, median pitch E5)
  track '' prog 115 -> lead (1083 notes, median pitch F4)
```
       |       |       |       |       |       |       |       |       |       |       |       |       |       |
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..
bass   .XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords .XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.X
lead   ........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..
lead2  .X.X.XXX.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.
```
sections, 4-bar blocks (start bar : length : roles): 1:8:D+B+C+L2; 9:96:D+B+C+L+L2; 105:1:B+C
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 2
- harmonic loop period: 2 bars; lead 4-bar phrases: `--NNvRRRRRRRRRRRRRRRRRRRRv-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): 59 `X.X.X.X.X.X.X.X.` vel 0.46±0.07; pedalhat `X.X.X.X.X.X.X.X.` vel 0.47±0.12; kick `X.......X......x` vel 0.79±0.0; snare2 `....X.......X...` vel 0.79±0.01; kick2 `X.....X.........` vel 0.79±0.0
- off-grid onsets 8%; swing8 0.5; swing16 None

#### I'll Be Missing You.mid
tempo 109.0 BPM, 4/4, 141 bars, mean active roles/bar 4.17
  track 'Bass' prog 35 -> bass (265 notes, median pitch G1)
  track 'MELODIE' prog 53 -> lead (215 notes, median pitch A4)
  track 'Guitar' prog 28 -> other (1544 notes, median pitch D3)
  track 'Synth Str' prog 50 -> lead2 (106 notes, median pitch D5)
  track 'CHOIRAAH' prog 52 -> lead2 (58 notes, median pitch D4)
  track 'PANFLUIT' prog 75 -> chords (170 notes, median pitch A4)
  track 'Drums' prog 0 -> drums (1342 notes, median pitch Ab2)
  track 'MELODIE' prog 53 -> lead2 (46 notes, median pitch A4)
```
       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |    
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords ...............................................................................XXXXXXXXXXXXXXXXX.............................................
lead   .......................XXXXXXXXXXXXXXXX................XXXXXXXXXXXXXXXX........................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead2  .......................................................................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
other  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:24:D+B+O; 25:16:D+B+L+O; 41:16:D+B+O; 57:16:D+B+L+O; 73:8:D+B+L2+O; 81:16:D+B+C+L2+O; 97:45:D+B+L+L2+O
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 8 bars; lead 4-bar phrases: `-----NNNRv---RRRRR-----RRRRRRRRRRRRv` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): pedalhat `....x.X.X.X.X...` vel 0.95±0.15; snare `....X.......X...` vel 0.97±0.14; kick `X.............X.` vel 0.92±0.13; openhat `X.............X.` vel 0.61±0.09; snare2 `.......X........` vel 0.76±0.12
- off-grid onsets 24%; swing8 0.534; swing16 None


# Appendix B: Online Sequencer, 127 lo-fi arrangements (user content, no published terms; learning only)

Sorted by sequence ID. Each title links to the sequence. The "core" 24 are: #2401362, #2785963, #2801489, #2874622, #2929064, #3156435, #3428945, #3582140, #3745767, #3746760, #3795663, #3845458, #3912419, #4155416, #4188103, #4496747, #4853823, #5208345, #5335524, #5337798, #5385286, #5401374, #5404618, #5499741.

#### OS #1372913 "Steven Universe - Change Your Mind (Lofi Hip Hop Remix)" (https://onlinesequencer.net/1372913)
tempo 55 BPM, 4/4, 67 bars, mean active roles/bar 4.18
  track 'Grand Piano (Classic)' prog 0 -> chords (391 notes, median pitch G3)
  track 'Elec. Piano (Classic)' prog 4 -> lead (350 notes, median pitch Bb4)
  track 'Bass Guitar (Classic)' prog 33 -> bass (368 notes, median pitch B1)
  track '8-Bit Sine' prog 80 -> chords (152 notes, median pitch C5)
  track 'Scifi' prog 103 -> lead2 (39 notes, median pitch G5)
  track 'Music Box' prog 10 -> chords (180 notes, median pitch Bb2)
  track 'Electric Drum Kit' prog 118 -> drums (1081 notes, median pitch F#2)
  track 'Drum Kit' prog 0 -> drums (329 notes, median pitch Ab2)
```
       |       |       |       |       |       |       |       |       |  
drums  ...XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   ....XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead2  .......XX......XX..XX..XX..XX......XX..XX..XX..XX..XX..............
```
sections, 4-bar blocks (start bar : length : roles): 1:4:C+L; 5:12:D+B+C+L; 17:12:D+B+C+L+L2; 29:8:D+B+C+L; 37:16:D+B+C+L+L2; 53:15:D+B+C+L
- drums enter bar 4, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 4 bars; lead 4-bar phrases: `NRRvNNRRRRRRRRvvN` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `XXXXXXXXXXXXXXXX` vel 1.0±0.0; pedalhat `X.X.X.X.X.X.X.X.` vel 1.0±0.0; maraca `X...X...X...X...` vel 1.0±0.0; snare2 `......X.......X.` vel 1.0±0.0; snare `......X.......X.` vel 1.0±0.0
- off-grid onsets 1%; swing8 0.562; swing16 0.25

#### OS #1282890 "Lofi hip-hop 'stoned' REMIX" (https://onlinesequencer.net/1282890)
tempo 87 BPM, 4/4, 32 bars, mean active roles/bar 3.0
  track 'Bass Guitar (Classic)' prog 33 -> bass (24 notes, median pitch F#1)
  track 'Electric Drum Kit' prog 118 -> drums (384 notes, median pitch F#2)
  track 'Jazz Guitar' prog 26 -> chords (176 notes, median pitch B3)
  track 'Grand Piano (Classic)' prog 0 -> lead2 (56 notes, median pitch F#4)
  track 'Drum Kit' prog 0 -> drums (4 notes, median pitch C3)
  track '808 Drum Kit' prog 0 -> drums (56 notes, median pitch C2)
  track 'Smooth Synth (Classic)' prog 90 -> other (143 notes, median pitch G2)
  track 'Scifi' prog 103 -> chords (103 notes, median pitch E4)
  track '8-Bit Sine' prog 80 -> lead (60 notes, median pitch E4)
```
       |       |       |       |       
drums  ....XXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   ....XXXXXXXX....................
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   ........................XXXX....
lead2  ........XXXX........XXXX........
other  ............XXXXXXXX....XXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:4:C; 5:4:D+B+C; 9:4:D+B+C+L2; 13:8:D+C+O; 21:4:D+C+L2; 25:4:D+C+L+O; 29:4:D+C+O
- drums enter bar 5, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 2 bars; lead 4-bar phrases: `------N--` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): tamb `X.X.X.X.X.X.X.X.` vel 1.0±0.0; stick `..xX.X.XX.X.X.X.` vel 1.0±0.0; kick `..X.X.......X...` vel 1.0±0.0; 28 `Xxx.xxx.x.X.xX.x` vel 1.0±0.0; hat `xxxx.X.xxx..Xxxx` vel 1.0±0.0
- off-grid onsets 0%; swing8 0.54; swing16 0.25

#### OS #1283435 "Lofi... Kinda" (https://onlinesequencer.net/1283435)
tempo 150 BPM, 4/4, 40 bars, mean active roles/bar 3.3
  track 'Vibraphone' prog 11 -> lead (226 notes, median pitch A4)
  track 'Grand Piano (Classic)' prog 0 -> lead2 (102 notes, median pitch F#5)
  track 'Bass Guitar (Classic)' prog 33 -> bass (161 notes, median pitch A1)
  track 'Electric Drum Kit' prog 118 -> drums (271 notes, median pitch C2)
```
      |       |       |       |       |       
drums ......XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass  XX......XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead2 ........XXXXXXXXXXXXXXXXXXXXXXXX........
```
sections, 4-bar blocks (start bar : length : roles): 1:4:B+L; 5:4:D+L; 9:24:D+B+L+L2; 33:8:D+B+L
- drums enter bar 7, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 4 bars; lead 4-bar phrases: `NRNNNNvvRR-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): kick `x..x...X...Xx..X` vel 1.0±0.0; 59 `....X.......X...` vel 1.0±0.0; snare2 `........X......x` vel 1.0±0.0; tamb `...............X` vel 1.0±0.0; stick `........X.......` vel 1.0±0.0
- off-grid onsets 53%; swing8 0.668; swing16 None

#### OS #1318372 "Lofi hip-hop 'stoned' added reverb" (https://onlinesequencer.net/1318372)
tempo 87 BPM, 4/4, 32 bars, mean active roles/bar 3.0
  track 'Bass Guitar (Classic)' prog 33 -> bass (24 notes, median pitch F#1)
  track 'Electric Drum Kit' prog 118 -> drums (384 notes, median pitch F#2)
  track 'Jazz Guitar' prog 26 -> chords (176 notes, median pitch B3)
  track 'Grand Piano (Classic)' prog 0 -> lead2 (56 notes, median pitch F#4)
  track 'Drum Kit' prog 0 -> drums (4 notes, median pitch C3)
  track 'Smooth Synth (Classic)' prog 90 -> other (143 notes, median pitch G2)
  track '808 Drum Kit' prog 0 -> drums (56 notes, median pitch C2)
  track 'Scifi' prog 103 -> chords (103 notes, median pitch E4)
  track '8-Bit Sine' prog 80 -> lead (60 notes, median pitch E4)
```
       |       |       |       |       
drums  ....XXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   ....XXXXXXXX....................
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   ........................XXXX....
lead2  ........XXXX........XXXX........
other  ............XXXXXXXX....XXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:4:C; 5:4:D+B+C; 9:4:D+B+C+L2; 13:8:D+C+O; 21:4:D+C+L2; 25:4:D+C+L+O; 29:4:D+C+O
- drums enter bar 5, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 2 bars; lead 4-bar phrases: `------N--` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): tamb `X.X.X.X.X.X.X.X.` vel 1.0±0.0; stick `..xX.X.XX.X.X.X.` vel 1.0±0.0; kick `..X.X.......X...` vel 1.0±0.0; 28 `Xxx.xxx.x.X.xX.x` vel 1.0±0.0; hat `xxxx.X.xxx..Xxxx` vel 1.0±0.0
- off-grid onsets 0%; swing8 0.54; swing16 0.25

#### OS #1325262 "Poliwrath River LoFi Remix" (https://onlinesequencer.net/1325262)
tempo 97 BPM, 4/4, 73 bars, mean active roles/bar 2.59
  track 'Acoustic Gtr (Classic)' prog 25 -> lead (571 notes, median pitch B3)
  track '8-Bit Triangle' prog 80 -> chords (366 notes, median pitch D4)
  track '8-Bit Sine' prog 80 -> chords (203 notes, median pitch E4)
  track 'Grand Piano (Classic)' prog 0 -> chords (194 notes, median pitch Eb4)
  track 'Electric Drum Kit' prog 118 -> drums (24 notes, median pitch Bb2)
  track 'Bass Guitar (Classic)' prog 33 -> bass (337 notes, median pitch Bb2)
  track 'Ragtime Piano' prog 3 -> chords (194 notes, median pitch Eb4)
  track 'Smooth Synth (Classic)' prog 90 -> chords (584 notes, median pitch A2)
  track '8-Bit Sawtooth' prog 81 -> lead2 (34 notes, median pitch Eb4)
```
       |       |       |       |       |       |       |       |       |       |
drums  ...................XXXXXXX.............................XXXXXXX...........
bass   ........................XXXXXXXXX...........................XXXXXXXXX....
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
lead   XXXXXXXXXXXXXXXXXXXX..XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..XXXXXXXXXXXXXXX
lead2  .........................XXXXXXXX............................XXXXXXXX....
```
sections, 4-bar blocks (start bar : length : roles): 1:20:C+L; 21:4:D+C+L; 25:4:D+B+C+L+L2; 29:4:B+C+L+L2; 33:24:C+L; 57:4:D+C+L; 61:4:D+B+C+L+L2; 65:4:B+C+L+L2; 69:4:C+L; 73:1:L
- drums enter bar 20, drum dropouts (start bar,len): [(27, 29)], bars after drums stop: 11
- harmonic loop period: None bars; lead 4-bar phrases: `NNNNNNNNNNNNNNNNNNN` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): tamb `X.......X.......` vel 1.0±0.0; snare `X.......X.......` vel 1.0±0.0
- off-grid onsets 4%; swing8 None; swing16 None

#### OS #1407055 "I accidentally made this a bit too lofi..." (https://onlinesequencer.net/1407055)
tempo 110 BPM, 4/4, 35 bars, mean active roles/bar 2.26
  track 'Elec. Piano (Classic)' prog 4 -> lead (540 notes, median pitch D4)
  track '808 Drum Kit' prog 0 -> drums (180 notes, median pitch D2)
  track 'Electric Drum Kit' prog 118 -> drums (7 notes, median pitch F3)
  track 'Drum Kit' prog 0 -> drums (7 notes, median pitch F3)
  track '8-Bit Sine' prog 80 -> chords (64 notes, median pitch A2)
  track '8-Bit Triangle' prog 80 -> chords (40 notes, median pitch E3)
  track 'Smooth Synth (Classic)' prog 90 -> bass (240 notes, median pitch Ab2)
  track 'Muted E-Guitar' prog 28 -> lead2 (16 notes, median pitch Ab4)
  track 'Steel Drums' prog 114 -> lead2 (60 notes, median pitch E4)
```
       |       |       |       |       |  
drums  ...XXXXXXXXXXXX...XXXXXXXXXX.......
bass   ......XXXXXXXXX.........XXXXXXX....
chords .....................X.............
lead   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
lead2  .........XXXXXX....................
```
sections, 4-bar blocks (start bar : length : roles): 1:4:L; 5:4:D+B+L; 9:8:D+B+L+L2; 17:8:D+L; 25:4:D+B+L; 29:4:B+L; 33:3:L
- drums enter bar 4, drum dropouts (start bar,len): [(16, 3)], bars after drums stop: 7
- harmonic loop period: None bars; lead 4-bar phrases: `NNNvRNNNN` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): snare `xxxxxx.xxxxxxx.x` vel 1.0±0.0; clap `..x...x...x...x.` vel 1.0±0.0; kick `x...x...x...x...` vel 1.0±0.0; kick2 `...x.......x....` vel 1.0±0.0; ridebell `.........X......` vel 1.0±0.0
- off-grid onsets 10%; swing8 0.637; swing16 0.255

#### OS #1529131 "Lofi hip-hop " (https://onlinesequencer.net/1529131)
tempo 90 BPM, 4/4, 32 bars, mean active roles/bar 3.0
  track 'Bass Guitar (Classic)' prog 33 -> bass (24 notes, median pitch F#1)
  track 'Electric Drum Kit' prog 118 -> drums (384 notes, median pitch F#2)
  track 'Jazz Guitar' prog 26 -> chords (176 notes, median pitch Bb3)
  track 'Grand Piano (Classic)' prog 0 -> lead2 (56 notes, median pitch F4)
  track 'Drum Kit' prog 0 -> drums (4 notes, median pitch C3)
  track '808 Drum Kit' prog 0 -> drums (56 notes, median pitch C2)
  track 'Smooth Synth (Classic)' prog 90 -> other (143 notes, median pitch G2)
  track 'Scifi' prog 103 -> chords (103 notes, median pitch E4)
  track '8-Bit Sine' prog 80 -> lead (60 notes, median pitch E4)
```
       |       |       |       |       
drums  ....XXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   ....XXXXXXXX....................
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   ........................XXXX....
lead2  ........XXXX........XXXX........
other  ............XXXXXXXX....XXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:4:C; 5:4:D+B+C; 9:4:D+B+C+L2; 13:8:D+C+O; 21:4:D+C+L2; 25:4:D+C+L+O; 29:4:D+C+O
- drums enter bar 5, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 1 bars; lead 4-bar phrases: `------N--` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): tamb `X.X.X.X.X.X.X.X.` vel 1.0±0.0; stick `..xX.X.XX.X.X.X.` vel 1.0±0.0; kick `..X.X.......X...` vel 1.0±0.0; 28 `Xxx.xxx.x.X.xX.x` vel 1.0±0.0; hat `xxxx.X.xxx..Xxxx` vel 1.0±0.0
- off-grid onsets 0%; swing8 0.54; swing16 0.25

#### OS #1597943 "darkside boom bap rap beat" (https://onlinesequencer.net/1597943)
tempo 90 BPM, 4/4, 32 bars, mean active roles/bar 4.0
  track 'Drum Kit' prog 0 -> drums (512 notes, median pitch Eb2)
  track 'Electric Drum Kit' prog 118 -> drums (128 notes, median pitch D2)
  track 'Grand Piano (Classic)' prog 0 -> lead (224 notes, median pitch B4)
  track 'Slap Bass' prog 36 -> bass (256 notes, median pitch B1)
  track 'Bass Guitar (Classic)' prog 33 -> bass (256 notes, median pitch Bb2)
  track 'Acoustic Gtr (Classic)' prog 25 -> bass (128 notes, median pitch Bb2)
  track 'Smooth Synth (Classic)' prog 90 -> bass (64 notes, median pitch Bb2)
  track 'Steel Drums' prog 114 -> lead2 (128 notes, median pitch B3)
  track 'Xylophone' prog 13 -> lead2 (96 notes, median pitch Bb3)
```
      |       |       |       |       
drums XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead2 XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:32:D+B+L+L2
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 1 bars; lead 4-bar phrases: `NNRRRRRR-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X.X.X.X.X.X.X.X.` vel 1.0±0.0; tomL2 `X.X.X.X.X.X.X.X.` vel 1.0±0.0; 30 `....x.xxxx..x...` vel 1.0±0.0; 29 `....X.X.X...X...` vel 1.0±0.0; kick2 `X.........X.X...` vel 1.0±0.0
- off-grid onsets 0%; swing8 0.536; swing16 0.25

#### OS #1650967 "my first lofi song, (Chords by notes)" (https://onlinesequencer.net/1650967)
tempo 130 BPM, 4/4, 33 bars, mean active roles/bar 3.79
  track 'Jazz Guitar' prog 26 -> other (216 notes, median pitch F#3)
  track 'Drum Kit' prog 0 -> drums (5922 notes, median pitch A1)
  track 'Pizzicato' prog 45 -> other (264 notes, median pitch C1)
  track 'Vibraphone' prog 11 -> lead (143 notes, median pitch F#4)
  track '808 Drum Kit' prog 0 -> drums (160 notes, median pitch F#2)
  track 'Acoustic Gtr (Classic)' prog 25 -> lead2 (102 notes, median pitch Ab4)
  track 'Cello (Classic)' prog 42 -> chords (404 notes, median pitch Bb2)
```
       |       |       |       |       |
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords ............................XXXXX
lead   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
lead2  XXX.XXX.XXX.XXXXXXX.X.X.XXXXX....
other  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
```
sections, 4-bar blocks (start bar : length : roles): 1:28:D+L+L2+O; 29:4:D+C+L+O; 33:1:D+C
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 8 bars; lead 4-bar phrases: `NNRvvvRv-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): 33 `XXXXXXXXXXXXXXXX` vel 1.0±0.0; pedalhat `XXXXXXXXXXXXXXXX` vel 1.0±0.0; 32 `XXXXXXXXXXXxXXXX` vel 1.0±0.0; hat `X.......X...x...` vel 1.0±0.0; kick `x...x.......x...` vel 1.0±0.0
- off-grid onsets 66%; swing8 0.599; swing16 0.285

#### OS #1723785 "Zelda's Lullaby (The Legend of Zelda: Ocarina of Time) - Lofi Attempt " (https://onlinesequencer.net/1723785)
tempo 110 BPM, 4/4, 42 bars, mean active roles/bar 2.83
  track 'Grand Piano' prog 0 -> bass (284 notes, median pitch A2)
  track 'Flute (Classic)' prog 73 -> lead (100 notes, median pitch F5)
  track 'Drum Kit' prog 0 -> drums (1386 notes, median pitch Ab1)
  track 'Electric Drum Kit' prog 118 -> drums (147 notes, median pitch Ab1)
  track '8-Bit Sine' prog 80 -> bass (21 notes, median pitch C1)
  track '8-Bit Triangle' prog 80 -> bass (21 notes, median pitch C#1)
  track '808 Drum Kit' prog 0 -> drums (551 notes, median pitch D2)
```
      |       |       |       |       |       | 
drums XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead  .....XXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXX.
```
sections, 4-bar blocks (start bar : length : roles): 1:4:D+B; 5:38:D+B+L
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `-NNNNNNNNN-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): 32 `XXXXXXXXXXXXXXXX` vel 1.0±0.0; hat `X.X.X.X.X.X.X.X.` vel 0.9±0.0; kick2 `X...X...X...X...` vel 1.0±0.0; kick `X...X...X...X...` vel 1.0±0.0; snare `x...x...x...x...` vel 0.58±0.04
- off-grid onsets 53%; swing8 0.563; swing16 0.286

#### OS #1740404 "funky chillhop attempt" (https://onlinesequencer.net/1740404)
tempo 142 BPM, 4/4, 36 bars, mean active roles/bar 4.33
  track 'Electric Drum Kit' prog 118 -> drums (767 notes, median pitch F#2)
  track '2013 Drum Kit' prog 0 -> drums (165 notes, median pitch B1)
  track '808 Drum Kit' prog 0 -> drums (333 notes, median pitch D2)
  track 'Grand Piano (Classic)' prog 0 -> chords (227 notes, median pitch G3)
  track '8-Bit Triangle' prog 80 -> lead2 (94 notes, median pitch D4)
  track '8-Bit Sawtooth' prog 81 -> lead (192 notes, median pitch D4)
  track 'Drum Kit' prog 0 -> drums (351 notes, median pitch B1)
  track 'Elec. Piano (Classic)' prog 4 -> chords (336 notes, median pitch G3)
  track '8-Bit Sine' prog 80 -> bass (164 notes, median pitch C2)
  track 'Bass Guitar (Classic)' prog 33 -> bass (164 notes, median pitch C2)
  track 'Grand Piano' prog 0 -> lead2 (175 notes, median pitch C5)
  track 'Flute (Classic)' prog 73 -> lead2 (50 notes, median pitch C5)
```
       |       |       |       |       |   
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   ......XXXXXXXXXXXX......XXXXXXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   ......XXXXXXXXXXXX......XXXXXXXXXXXX
lead2  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:4:D+C+L2; 5:16:D+B+C+L+L2; 21:4:D+C+L2; 25:12:D+B+C+L+L2
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `-NNNN-RvR-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `XXXXXXXXXXXXXxXX` vel 1.0±0.0; 32 `XXXXXXXXXXXXXXXX` vel 0.4±0.3; kick2 `XXxXX..XXXxXX..X` vel 0.95±0.15; pedalhat `xxxxxxxxxxxxxxxx` vel 1.0±0.02; snare `..Xx..X...Xx..Xx` vel 1.0±0.04
- off-grid onsets 20%; swing8 0.601; swing16 0.252

#### OS #1749762 "Bonfire Lofi (Cursed Images song)" (https://onlinesequencer.net/1749762)
tempo 80 BPM, 4/4, 35 bars, mean active roles/bar 2.94
  track 'Vibraphone' prog 11 -> chords (384 notes, median pitch C#4)
  track '8-Bit Square' prog 80 -> lead (216 notes, median pitch F#4)
  track '808 Drum Kit' prog 0 -> drums (493 notes, median pitch Eb2)
  track '8-Bit Sawtooth' prog 81 -> lead2 (216 notes, median pitch F#4)
```
       |       |       |       |       |  
drums  ........XXXXXXXXX...XXXXXXXXXXXXX..
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..
lead   ....XXXXXXXXXXXXXXXXXXXXXXXX.......
lead2  ....XXXXXXXXXXXXXXXXXXXXXXXX.......
```
sections, 4-bar blocks (start bar : length : roles): 1:4:C; 5:4:C+L+L2; 9:8:D+C+L+L2; 17:4:C+L+L2; 21:8:D+C+L+L2; 29:4:D+C; 33:3:silence
- drums enter bar 9, drum dropouts (start bar,len): [(18, 3)], bars after drums stop: 2
- harmonic loop period: 1 bars; lead 4-bar phrases: `-NRRRRR--` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X.X.X.XXXXX.X.XX` vel 1.0±0.0; kick2 `X....xX...X...XX` vel 1.0±0.0; snare `....X....X..X...` vel 1.0±0.0; kick `X.....X...X...xx` vel 1.0±0.0; ride `X.X.X.X.X.X.X.X.` vel 0.42±0.34
- off-grid onsets 0%; swing8 0.56; swing16 0.25

#### OS #1790288 "Dudu Lofi - Chill beats to study / relax to - 24/7" (https://onlinesequencer.net/1790288)
tempo 160 BPM, 4/4, 70 bars, mean active roles/bar 3.71
  track '808 Drum Kit' prog 0 -> drums (367 notes, median pitch C2)
  track 'Acoustic Gtr (Classic)' prog 25 -> bass (2046 notes, median pitch C2)
  track 'Steel Drums' prog 114 -> bass (131 notes, median pitch A2)
  track 'Jazz Guitar' prog 26 -> lead (527 notes, median pitch C4)
  track 'Grand Piano (Classic)' prog 0 -> lead2 (314 notes, median pitch D4)
  track '808 Bass' prog 38 -> bass (36 notes, median pitch C3)
  track 'Bass Guitar (Classic)' prog 33 -> bass (98 notes, median pitch F2)
  track '8-Bit Sawtooth' prog 81 -> bass (4 notes, median pitch C1)
  track 'Electric Drum Kit' prog 118 -> drums (138 notes, median pitch Ab1)
  track '8-Bit Triangle' prog 80 -> bass (20 notes, median pitch D2)
```
      |       |       |       |       |       |       |       |       |     
drums XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..XXXXXXX
bass  ....XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
lead  X.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead2 ....XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXX....XXXX.
```
sections, 4-bar blocks (start bar : length : roles): 1:4:D+L; 5:56:D+B+L+L2; 61:4:D+B+L; 65:6:D+B+L+L2
- drums enter bar 1, drum dropouts (start bar,len): [(62, 2)], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `NNvvvNNNvNvvvvRvvN` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): kick2 `X...x...x.......` vel 1.0±0.0; kick `X.......x.......` vel 1.0±0.0; snare `........X.......` vel 1.0±0.0; 32 `xXXXxXXXXxXXXXXX` vel 1.0±0.0; 33 `XxxXxxxXxxXXXXXX` vel 1.0±0.0
- off-grid onsets 55%; swing8 0.65; swing16 0.262

#### OS #1938638 "gax - without [ lofi remix ]" (https://onlinesequencer.net/1938638)
tempo 110 BPM, 3/4, 104 bars, mean active roles/bar 3.96
  track 'Concert Harp' prog 46 -> lead (429 notes, median pitch C4)
  track 'Grand Piano (Classic)' prog 0 -> lead2 (429 notes, median pitch C4)
  track 'Music Box' prog 10 -> lead2 (429 notes, median pitch C4)
  track '808 Drum Kit' prog 0 -> drums (515 notes, median pitch F#2)
  track 'Pizzicato' prog 45 -> bass (2496 notes, median pitch C1)
  track 'Electric Drum Kit' prog 118 -> drums (2644 notes, median pitch Bb4)
  track 'Drum Kit' prog 0 -> drums (3120 notes, median pitch Ab1)
  track 'Grand Piano' prog 0 -> lead2 (429 notes, median pitch C4)
  track '2013 Drum Kit' prog 0 -> drums (2644 notes, median pitch B3)
  track 'Saxophone' prog 66 -> bass (714 notes, median pitch F#2)
  track 'Flute (Classic)' prog 73 -> lead2 (178 notes, median pitch D5)
  track '8-Bit Sine' prog 80 -> bass (58 notes, median pitch F1)
```
      |       |       |       |       |       |       |       |       |       |       |       |       |       
drums XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..XXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXX
lead2 XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:104:D+B+L+L2
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `NNNNNvNvNNNvNNNNNNNNNNNNNN-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): 32 `XXXXXXXXXXXX` vel 0.99±0.07; maraca `XXXXXXXXXXXX` vel 0.99±0.08; 59 `XXXXXXXXXXXX` vel 0.99±0.08; hat `X...X...X...` vel 1.0±0.0; kick2 `x...x...x...` vel 1.0±0.0
- off-grid onsets 60%; swing8 0.595; swing16 0.281

#### OS #1991005 "Lofi type beat-Relax" (https://onlinesequencer.net/1991005)
tempo 95 BPM, 4/4, 68 bars, mean active roles/bar 2.74
  track 'Grand Piano' prog 0 -> lead (529 notes, median pitch G4)
  track 'Drum Kit' prog 0 -> drums (1384 notes, median pitch Ab1)
  track '8-Bit Triangle' prog 80 -> bass (34 notes, median pitch G1)
  track 'Electric Drum Kit' prog 118 -> drums (710 notes, median pitch F#2)
  track '2013 Drum Kit' prog 0 -> drums (100 notes, median pitch F#2)
  track 'Bass Guitar (Classic)' prog 33 -> bass (72 notes, median pitch G1)
  track 'Smooth Synth (Classic)' prog 90 -> chords (30 notes, median pitch Eb4)
  track 'Violin (Classic)' prog 40 -> lead2 (96 notes, median pitch Bb3)
```
       |       |       |       |       |       |       |       |       |   
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   ....XXXX........XXXXXXXX....XXXX.X..XXXX........XXXXXXXX....XXXX.X..
chords ........................XXXX............................XXXX........
lead   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead2  ........................XXXX............................XXXX........
```
sections, 4-bar blocks (start bar : length : roles): 1:4:D+L; 5:4:D+B+L; 9:8:D+L; 17:8:D+B+L; 25:4:D+C+L+L2; 29:4:D+B+L; 33:4:D+L; 37:4:D+B+L; 41:8:D+L; 49:8:D+B+L; 57:4:D+C+L+L2; 61:4:D+B+L; 65:4:D+L
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `NRvvRRNRRRRRRRRRR-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): 32 `XXXxXXXXXXXXXXXX` vel 1.0±0.0; hat `X.X.X.XxX.XxXxX.` vel 1.0±0.0; stick `..x.....X.x.....` vel 1.0±0.0; kick2 `x.........x...X.` vel 1.0±0.0; 52 `xxx.XXXXXXX.xxx.` vel 1.0±0.0
- off-grid onsets 46%; swing8 0.557; swing16 0.252

#### OS #2000692 "lofi beat" (https://onlinesequencer.net/2000692)
tempo 140 BPM, 4/4, 36 bars, mean active roles/bar 3.22
  track 'Grand Piano' prog 0 -> chords (346 notes, median pitch Ab4)
  track 'Acoustic Gtr (Classic)' prog 25 -> chords (173 notes, median pitch C#4)
  track 'Grand Piano (Classic)' prog 0 -> lead (161 notes, median pitch B5)
  track '8-Bit Drum Kit' prog 0 -> drums (178 notes, median pitch F#2)
  track '808 Drum Kit' prog 0 -> drums (164 notes, median pitch D2)
  track '808 Bass' prog 38 -> bass (32 notes, median pitch A2)
  track 'Bass Guitar (Classic)' prog 33 -> bass (12 notes, median pitch E1)
  track 'Music Box' prog 10 -> lead2 (16 notes, median pitch Eb5)
  track 'Trombone (Classic)' prog 57 -> bass (4 notes, median pitch Bb2)
  track '909 Drum Kit' prog 0 -> drums (16 notes, median pitch B1)
  track 'Koto' prog 107 -> lead2 (51 notes, median pitch Ab5)
  track 'Jazz Guitar' prog 26 -> lead2 (24 notes, median pitch Eb4)
```
       |       |       |       |       |   
drums  ........XXXXXXXX....XXXXXXXXXXXX....
bass   ............XXXX............XXXX....
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead2  ........XXXXXXX.X...XXXXXXXX........
```
sections, 4-bar blocks (start bar : length : roles): 1:8:C+L; 9:4:D+C+L+L2; 13:4:D+B+C+L+L2; 17:4:C+L; 21:8:D+C+L+L2; 29:4:D+B+C+L; 33:4:C+L
- drums enter bar 9, drum dropouts (start bar,len): [(17, 4)], bars after drums stop: 4
- harmonic loop period: 4 bars; lead 4-bar phrases: `NvNRvvRRR-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X.XxXXXXXxXxXxXx` vel 1.0±0.0; kick2 `X...X.x...x.X...` vel 1.0±0.0; clap `........X.......` vel 1.0±0.0; 32 `..x...........x.` vel 1.0±0.0; snare `..............X.` vel 1.0±0.0
- off-grid onsets 5%; swing8 0.548; swing16 0.257

#### OS #2003641 "Lofi WIP i guess" (https://onlinesequencer.net/2003641)
tempo 110 BPM, 4/4, 37 bars, mean active roles/bar 3.19
  track '8-Bit Triangle' prog 80 -> chords (100 notes, median pitch D3)
  track 'Electric Drum Kit' prog 118 -> drums (192 notes, median pitch G1)
  track '8-Bit Sine' prog 80 -> lead (54 notes, median pitch Eb5)
  track 'Grand Piano (Classic)' prog 0 -> lead2 (49 notes, median pitch C5)
```
       |       |       |       |       |    
drums  ....XXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
lead   ...XXXXXXXXX......XXXXXXXXXXXXXXXXXX.
lead2  ............XX.XXXXXXXXXXXXXXXXXXXXX.
```
sections, 4-bar blocks (start bar : length : roles): 1:4:C; 5:8:D+C+L; 13:4:D+C+L2; 17:20:D+C+L+L2; 37:1:D
- drums enter bar 5, drum dropouts (start bar,len): [(12, 1)], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `NNN-NNNNN-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): 31 `.x.x.x.x.x.x.x.x` vel 1.0±0.0; stick `..x...x...x...x.` vel 1.0±0.0; kick2 `x...x...x...x...` vel 1.0±0.0
- off-grid onsets 1%; swing8 0.651; swing16 0.25

#### OS #2045644 "Fear Of The Dark (original lofi trap by [ - Fl T 0 M - ])" (https://onlinesequencer.net/2045644)
tempo 128 BPM, 4/4, 32 bars, mean active roles/bar 4.94
  track 'Drum Kit' prog 0 -> drums (224 notes, median pitch F#2)
  track 'Cello (Classic)' prog 42 -> chords (22784 notes, median pitch C#4)
  track '8-Bit Sine' prog 80 -> chords (176 notes, median pitch F#1)
  track 'Vibraphone' prog 11 -> lead (40 notes, median pitch B4)
  track 'Steel Drums' prog 114 -> lead2 (40 notes, median pitch B4)
  track 'Smooth Synth (Classic)' prog 90 -> chords (1216 notes, median pitch C#5)
  track 'Music Box' prog 10 -> lead2 (40 notes, median pitch B5)
  track '808 Drum Kit' prog 0 -> drums (228 notes, median pitch F#2)
  track '808 Bass' prog 38 -> bass (96 notes, median pitch G2)
  track '8-Bit Square' prog 80 -> other (31 notes, median pitch F#3)
  track 'Harpsichord' prog 6 -> chords (192 notes, median pitch C4)
  track 'Slap Bass' prog 36 -> bass (54 notes, median pitch Bb2)
  track 'Pizzicato' prog 45 -> other (39 notes, median pitch D3)
```
       |       |       |       |       
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   .XXX............XXXXXXXXXXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead2  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
other  .XXX....................XXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:4:D+B+C+L+L2+O; 5:12:D+C+L+L2; 17:8:D+B+C+L+L2; 25:8:D+B+C+L+L2+O
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 4 bars; lead 4-bar phrases: `NRRRRRRR-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `xXx.xXxxxXxxxXxx` vel 1.0±0.0; clap `........X.......` vel 1.0±0.0; kick `X...............` vel 1.0±0.0; stick `........X.......` vel 1.0±0.0; kick2 `x...x.......x...` vel 1.0±0.0
- off-grid onsets 69%; swing8 0.595; swing16 0.207

#### OS #2112073 "m e l t e d  d r e a m s - a short lofi song (Made by: Papyrus_Spaghetti95)" (https://onlinesequencer.net/2112073)
tempo 80 BPM, 4/4, 36 bars, mean active roles/bar 2.14
  track 'Grand Piano' prog 0 -> lead (434 notes, median pitch C4)
  track '909 Drum Kit' prog 0 -> drums (78 notes, median pitch B1)
  track 'Violin (Classic)' prog 40 -> lead2 (262 notes, median pitch F5)
  track 'Cello (Classic)' prog 42 -> bass (20 notes, median pitch E2)
```
      |       |       |       |       |   
drums ....XXXXXXXXXXXX........XXXXXXXX....
bass  ....................XXXX............
lead  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead2 ........XXXXXXXX.......XXXXXXXXX....
```
sections, 4-bar blocks (start bar : length : roles): 1:4:L; 5:4:D+L; 9:8:D+L+L2; 17:4:L; 21:4:B+L; 25:8:D+L+L2; 33:4:L
- drums enter bar 5, drum dropouts (start bar,len): [(17, 8)], bars after drums stop: 4
- harmonic loop period: None bars; lead 4-bar phrases: `NvvvNNRRv-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): kick2 `X.........X..X..` vel 1.0±0.0; snare `....X...........` vel 1.0±0.0
- off-grid onsets 0%; swing8 0.5; swing16 0.25

#### OS #2130914 "LoFi melody" (https://onlinesequencer.net/2130914)
tempo 110 BPM, 4/4, 62 bars, mean active roles/bar 2.68
  track '808 Drum Kit' prog 0 -> drums (848 notes, median pitch F#2)
  track 'Jazz Guitar' prog 26 -> lead (572 notes, median pitch Eb4)
  track '8-Bit Square' prog 80 -> chords (371 notes, median pitch B3)
  track '8-Bit Sawtooth' prog 81 -> bass (32 notes, median pitch B2)
  track '8-Bit Sine' prog 80 -> lead2 (57 notes, median pitch E5)
  track '8-Bit Drum Kit' prog 0 -> drums (15 notes, median pitch E3)
  track '8-Bit Triangle' prog 80 -> lead2 (64 notes, median pitch E5)
```
       |       |       |       |       |       |       |       |     
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..
bass   ................XXXXX.X.......................................
chords ........X.X.X.X.....XXXXXXXX........X.X.X.X.X.X.X.X...........
lead   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead2  ................XXXXX.X.XXXX................XXXXXXXX..........
```
sections, 4-bar blocks (start bar : length : roles): 1:8:D+L; 9:8:D+C+L; 17:4:D+B+L+L2; 21:4:D+B+C+L+L2; 25:4:D+C+L+L2; 29:8:D+L; 37:8:D+C+L; 45:8:D+C+L+L2; 53:8:D+L; 61:2:L
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 2
- harmonic loop period: 1 bars; lead 4-bar phrases: `NNRRRvRRRvvRRRRN` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X.X.X.X...X.X.X.` vel 1.0±0.0; 32 `..X.........X.x.` vel 1.0±0.0; kick2 `X.....x.........` vel 1.0±0.0; snare `........X.......` vel 1.0±0.0; clap `........X.......` vel 1.0±0.0
- off-grid onsets 0%; swing8 0.503; swing16 None

#### OS #2143159 "Lofi Chillin' full version short" (https://onlinesequencer.net/2143159)
tempo 80 BPM, 4/4, 32 bars, mean active roles/bar 2.06
  track 'Grand Piano' prog 0 -> chords (240 notes, median pitch E4)
  track '808 Drum Kit' prog 0 -> drums (231 notes, median pitch F#2)
  track 'Electric Drum Kit' prog 118 -> drums (445 notes, median pitch A4)
  track 'Bass Guitar (Classic)' prog 33 -> bass (45 notes, median pitch Bb1)
  track 'Pizzicato' prog 45 -> chords (320 notes, median pitch E4)
  track '8-Bit Drum Kit' prog 0 -> drums (105 notes, median pitch B1)
```
       |       |       |       |       
drums  ........XXXXXXXXXXXXXXXXXXXX....
bass   ..........XXXXXXXXXXXXXX........
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:8:C; 9:16:D+B+C; 25:4:D+C; 29:4:C
- drums enter bar 9, drum dropouts (start bar,len): [], bars after drums stop: 4
- harmonic loop period: 8 bars; lead 4-bar phrases: `n/a` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X.X.X.X.X.X.X.X.` vel 0.85±0.15; 82 `X.X.X.X.X.X.X.X.` vel 0.4±0.1; kick2 `X.XX..XX..X..X.X` vel 1.0±0.0; snare `....X.......X...` vel 0.95±0.05; cabasa `X...X...X...X...` vel 0.5±0.0
- off-grid onsets 8%; swing8 0.5; swing16 0.312

#### OS #2212972 "Another Lofi Beat" (https://onlinesequencer.net/2212972)
tempo 130 BPM, 4/4, 40 bars, mean active roles/bar 3.77
  track 'Vibraphone' prog 11 -> lead (256 notes, median pitch Eb4)
  track 'Jazz Guitar' prog 26 -> chords (300 notes, median pitch Eb3)
  track 'Acoustic Gtr (Classic)' prog 25 -> lead2 (117 notes, median pitch Ab4)
  track '8-Bit Triangle' prog 80 -> lead2 (135 notes, median pitch Bb4)
  track '808 Drum Kit' prog 0 -> drums (259 notes, median pitch F#2)
```
       |       |       |       |       |       
drums  XXXXXXX.XXXXXXXXXXXXXXXXXXXXX.......XXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead2  XXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:28:D+C+L+L2; 29:8:C+L+L2; 37:4:D+C+L+L2
- drums enter bar 1, drum dropouts (start bar,len): [(8, 1), (30, 7)], bars after drums stop: 0
- harmonic loop period: 8 bars; lead 4-bar phrases: `NNvvvvRvNN-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X.......X...x...` vel 1.0±0.0; kick `X...x.......x...` vel 1.0±0.0; snare `........X.......` vel 1.0±0.0; maraca `x.......x...x...` vel 1.0±0.0; 68 `X...........x...` vel 1.0±0.0
- off-grid onsets 12%; swing8 0.578; swing16 0.263

#### OS #2264243 "LoFi Beat WIP Remix by (original by PoisonedDolls) by unnamed_official " (https://onlinesequencer.net/2264243)
tempo 70 BPM, 4/4, 48 bars, mean active roles/bar 3.21
  track '8-Bit Sine' prog 80 -> chords (750 notes, median pitch D3)
  track 'Grand Piano (Classic)' prog 0 -> chords (516 notes, median pitch E4)
  track '808 Drum Kit' prog 0 -> drums (549 notes, median pitch D2)
  track 'Synth Bass (Classic)' prog 38 -> bass (902 notes, median pitch Ab1)
  track '8-Bit Triangle' prog 80 -> lead (56 notes, median pitch G4)
  track 'Smooth Synth (Classic)' prog 90 -> chords (320 notes, median pitch E3)
  track 'Elec. Piano (Classic)' prog 4 -> lead2 (26 notes, median pitch B4)
  track 'Steel Drums' prog 114 -> chords (320 notes, median pitch E3)
  track 'Vibraphone' prog 11 -> chords (320 notes, median pitch E4)
```
       |       |       |       |       |       |       
drums  XXXXXXXX....XXXX....XXXXXXXXXXXX....XXXXXXXXXXXX
bass   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   ....XXXX................XXXX.............XXX.XXX
lead2  ....................XXXXXXXX....................
```
sections, 4-bar blocks (start bar : length : roles): 1:4:D+B+C; 5:4:D+B+C+L; 9:4:B+C; 13:4:D+B+C; 17:4:B+C; 21:4:D+B+C+L2; 25:4:D+B+C+L+L2; 29:4:D+B+C; 33:4:B+C; 37:4:D+B+C; 41:8:D+B+C+L
- drums enter bar 1, drum dropouts (start bar,len): [(9, 4), (17, 4), (33, 4)], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `-N----N---NR-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): kick2 `X...X...XxX...Xx` vel 1.0±0.0; hat `x...x...x...x.X.` vel 1.0±0.0; snare `........XX......` vel 1.0±0.0; tomL `...X.......X....` vel 1.0±0.0; 32 `...X.......X....` vel 1.0±0.0
- off-grid onsets 16%; swing8 0.587; swing16 0.264

#### OS #2347142 "Somewhere Inside Your Heart (kinda lofi)" (https://onlinesequencer.net/2347142)
tempo 150 BPM, 4/4, 89 bars, mean active roles/bar 2.54
  track 'Grand Piano' prog 0 -> chords (328 notes, median pitch Ab4)
  track 'Electric Drum Kit' prog 118 -> drums (442 notes, median pitch D2)
  track '8-Bit Sine' prog 80 -> chords (76 notes, median pitch F#3)
  track 'Music Box' prog 10 -> lead (120 notes, median pitch B4)
  track 'Vibraphone' prog 11 -> chords (80 notes, median pitch B4)
  track 'Smooth Synth (Classic)' prog 90 -> other (208 notes, median pitch Ab3)
  track 'Scifi' prog 103 -> lead2 (66 notes, median pitch B4)
```
       |       |       |       |       |       |       |       |       |       |       |       |
drums  .......XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX...XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.........
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   ........................X.X.X.XXX.X.X.XXX.X.X.XX.........................................
lead2  ...............................................................XXXXXXXXXXXXXXXXXX....XXX.
other  ................................................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.........
```
sections, 4-bar blocks (start bar : length : roles): 1:8:C; 9:16:D+C; 25:16:D+C+L; 41:4:C+L; 45:4:D+C+L; 49:16:D+C+O; 65:16:D+C+L2+O; 81:4:C; 85:4:C+L2; 89:1:C
- drums enter bar 8, drum dropouts (start bar,len): [(42, 3)], bars after drums stop: 9
- harmonic loop period: 8 bars; lead 4-bar phrases: `------NNRRRR-----------` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): 27 `X.x.....Xxx.x.x.` vel 1.0±0.0; snare2 `..x.....X.....x.` vel 1.0±0.0; kick2 `X...x...........` vel 1.0±0.0; pedalhat `x...........x...` vel 1.0±0.0; snare `............X.X.` vel 1.0±0.0
- off-grid onsets 11%; swing8 0.51; swing16 0.333

#### OS #2401362 "Groovy lofi funk" (https://onlinesequencer.net/2401362)
tempo 100 BPM, 4/4, 48 bars, mean active roles/bar 2.71
  track 'Drum Kit' prog 0 -> drums (1192 notes, median pitch Eb2)
  track 'Electric Drum Kit' prog 118 -> drums (1330 notes, median pitch E2)
  track '8-Bit Sawtooth' prog 81 -> bass (166 notes, median pitch D1)
  track 'Trombone (Classic)' prog 57 -> lead (215 notes, median pitch D4)
  track 'Bass Guitar (Classic)' prog 33 -> bass (224 notes, median pitch C3)
  track '8-Bit Sine' prog 80 -> chords (288 notes, median pitch D4)
  track 'Vibraphone' prog 11 -> lead2 (70 notes, median pitch C5)
```
       |       |       |       |       |       |       
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   ........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords ........................XXXXXXXXXXXXXXXX........
lead   ................XXXXXXXXXXXXXXXXX...............
lead2  ................................XXXXXXXXX.......
```
sections, 4-bar blocks (start bar : length : roles): 1:8:D; 9:8:D+B; 17:8:D+B+L; 25:8:D+B+C+L; 33:8:D+B+C+L2; 41:8:D+B
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 1 bars; lead 4-bar phrases: `----NRNNN----` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): kick `XXxX.xxX.xXX.x.x` vel 1.0±0.0; hat `X.X.X.X.X.X.X.Xx` vel 1.0±0.0; 33 `..X..XX..X.X.X.X` vel 1.0±0.0; stick `..X..XX..X.X.X.X` vel 1.0±0.0; cowbell `..X...X...X...X.` vel 1.0±0.0
- off-grid onsets 1%; swing8 0.584; swing16 0.25

#### OS #2449668 "lofi proyecto" (https://onlinesequencer.net/2449668)
tempo 80 BPM, 2/4, 114 bars, mean active roles/bar 2.43
  track '8-Bit Sine' prog 80 -> chords (394 notes, median pitch Ab3)
  track '8-Bit Sawtooth' prog 81 -> lead2 (208 notes, median pitch C#5)
  track '808 Drum Kit' prog 0 -> drums (22 notes, median pitch B1)
  track '8-Bit Square' prog 80 -> lead (1171 notes, median pitch B5)
  track 'Jazz Guitar' prog 26 -> chords (384 notes, median pitch Ab4)
  track '2013 Drum Kit' prog 0 -> drums (83 notes, median pitch E3)
  track '8-Bit Drum Kit' prog 0 -> drums (91 notes, median pitch B1)
  track 'Scifi' prog 103 -> other (105 notes, median pitch Eb3)
  track 'Koto' prog 107 -> other (36 notes, median pitch E3)
  track 'Smooth Synth (Classic)' prog 90 -> pad (18 notes, median pitch E3)
```
       |       |       |       |       |       |       |       |       |       |       |       |       |       |       | 
drums  ...................................XXXXXXXXXXXXXXXXXX.......X......................XXXXXXXXXXXXXXXXXX.............
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.............
pad    .....................................................................................................XXXXXXXXXXXXX
lead   .....................................XXXXXXXXXXXXXXXX.......XXXXX.X.XXXXXXXXX........XXXXXXXXXXXXXXXXXXXXXXXXXXX..
lead2  .............XXXXXXXXXXXXXXXXXXXXXXXX........................................XXXXXXXX.............................
other  .....................................X.XXX.XXXXXXX.XX................................X.XXX.XXXXXXX.XXXXXX.XXX.XXX.
```
sections, 4-bar blocks (start bar : length : roles): 1:12:C; 13:24:C+L2; 37:16:D+C+L+O; 53:8:C; 61:16:C+L; 77:8:C+L2; 85:16:D+C+L+O; 101:12:P+L+O; 113:2:P+O
- drums enter bar 36, drum dropouts (start bar,len): [(54, 7), (62, 22)], bars after drums stop: 13
- harmonic loop period: None bars; lead 4-bar phrases: `---------NvRRN-NNNNN-RRRRRRv-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): kick2 `X...x...` vel 1.0±0.0; 52 `X...x...` vel 1.0±0.0; hat `....Xx..` vel 1.0±0.0; snare `x...x...` vel 1.0±0.0; tomM2 `...X....` vel 1.0±0.0
- off-grid onsets 34%; swing8 0.703; swing16 0.25

#### OS #2458050 "Kamen Rider Build - Be the One - LoFi" (https://onlinesequencer.net/2458050)
tempo 80 BPM, 4/4, 82 bars, mean active roles/bar 2.91
  track 'Jazz Guitar' prog 26 -> lead (599 notes, median pitch F4)
  track '808 Drum Kit' prog 0 -> drums (994 notes, median pitch F#2)
  track 'Electric Drum Kit' prog 118 -> drums (1554 notes, median pitch C#3)
  track '8-Bit Drum Kit' prog 0 -> drums (574 notes, median pitch B1)
  track 'Electric Piano' prog 5 -> lead2 (302 notes, median pitch D6)
```
      |       |       |       |       |       |       |       |       |       |       | 
drums XXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXX..
lead  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.X
lead2 XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..
```
sections, 4-bar blocks (start bar : length : roles): 1:80:D+L+L2; 81:2:L
- drums enter bar 1, drum dropouts (start bar,len): [(24, 1), (64, 1)], bars after drums stop: 2
- harmonic loop period: None bars; lead 4-bar phrases: `NNNvvvNvvNRRRRRRRRRRN` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X.X.X.X.X.X.X.X.` vel 1.0±0.0; kick2 `X.xx..Xxx.X..X.x` vel 1.0±0.0; maraca `..X...X...X...X.` vel 0.7±0.0; snare `....X.......X...` vel 1.0±0.0; cabasa `X...X...X...X...` vel 0.5±0.0
- off-grid onsets 0%; swing8 0.523; swing16 0.25

#### OS #2484776 "Kirby Lofi Beat [Chill, Study, Short]" (https://onlinesequencer.net/2484776)
tempo 130 BPM, 4/4, 64 bars, mean active roles/bar 3.0
  track '8-Bit Triangle' prog 80 -> bass (1024 notes, median pitch Eb2)
  track '909 Drum Kit' prog 0 -> drums (320 notes, median pitch C2)
  track '8-Bit Drum Kit' prog 0 -> drums (704 notes, median pitch F#2)
  track 'Bass Guitar (Classic)' prog 33 -> bass (48 notes, median pitch A1)
  track '8-Bit Sine' prog 80 -> chords (448 notes, median pitch F3)
```
       |       |       |       |       |       |       |       |       
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:64:D+B+C
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 4 bars; lead 4-bar phrases: `n/a` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `xxXXX.Xxx.XXxxXX` vel 1.0±0.0; snare `........X.......` vel 1.0±0.0; kick2 `x.x.x.x.........` vel 1.0±0.0; kick `............X...` vel 1.0±0.0
- off-grid onsets 0%; swing8 0.603; swing16 0.25

#### OS #2572362 "Somewhat Lofi" (https://onlinesequencer.net/2572362)
tempo 130 BPM, 3/4, 64 bars, mean active roles/bar 3.41
  track 'Electric Piano' prog 5 -> chords (647 notes, median pitch A4)
  track 'Grand Piano' prog 0 -> bass (175 notes, median pitch G2)
  track '808 Drum Kit' prog 0 -> drums (560 notes, median pitch C#2)
  track '909 Drum Kit' prog 0 -> drums (518 notes, median pitch F#2)
  track 'Electric Drum Kit' prog 118 -> drums (56 notes, median pitch B1)
  track '8-Bit Drum Kit' prog 0 -> drums (560 notes, median pitch F#2)
  track '2013 Drum Kit' prog 0 -> drums (248 notes, median pitch F#2)
  track 'Slap Bass' prog 36 -> bass (80 notes, median pitch C#3)
  track 'Bass Guitar (Classic)' prog 33 -> bass (80 notes, median pitch C#3)
  track 'Pizzicato' prog 45 -> chords (32 notes, median pitch B5)
  track 'Violin (Classic)' prog 40 -> chords (32 notes, median pitch B4)
  track 'Trombone (Classic)' prog 57 -> lead (81 notes, median pitch D4)
  track 'Drum Kit' prog 0 -> drums (20 notes, median pitch E3)
  track '808 Bass' prog 38 -> bass (33 notes, median pitch A2)
  track 'Concert Harp' prog 46 -> lead2 (47 notes, median pitch A3)
  track 'Church Organ (Classic)' prog 19 -> other (4 notes, median pitch F3)
  track 'Scifi' prog 103 -> lead2 (9 notes, median pitch F5)
  track 'Synth Pluck (Classic)' prog 84 -> lead2 (6 notes, median pitch E4)
  track 'Cello (Classic)' prog 42 -> lead2 (18 notes, median pitch G4)
  track 'Xylophone' prog 13 -> lead2 (8 notes, median pitch C4)
  track 'Jazz Guitar' prog 26 -> bass (18 notes, median pitch Bb2)
```
       |       |       |       |       |       |       |       |       
drums  ........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   .....XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   ........................XXXXXXXXXXXXXXXX........................
lead2  ........................................XXX.XXXXXXXXXXXX....XXXX
other  ................................................X.X.X.X.........
```
sections, 4-bar blocks (start bar : length : roles): 1:4:C; 5:4:B+C; 9:16:D+B+C; 25:16:D+B+C+L; 41:8:D+B+C+L2; 49:8:D+B+C+L2+O; 57:4:D+B+C; 61:4:D+B+C+L2
- drums enter bar 9, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 8 bars; lead 4-bar phrases: `------NNNR-------` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `XX.XXX.XXX.X` vel 0.88±0.23; snare `....xx.xX...` vel 1.0±0.0; kick2 `X...X.......` vel 1.0±0.0; 32 `...x...X...X` vel 1.0±0.0; kick `X...x.......` vel 1.0±0.0
- off-grid onsets 49%; swing8 0.665; swing16 0.333

#### OS #4405314 "Lofi" (https://onlinesequencer.net/4405314)
tempo 165 BPM, 4/4, 93 bars, mean active roles/bar 3.9
  track 'Acoustic Guitar' prog 24 -> chords (863 notes, median pitch F#3)
  track 'Synthesizer' prog 90 -> lead (239 notes, median pitch F#5)
  track 'EDM Kit (E)' prog 0 -> drums (249 notes, median pitch Eb1)
  track 'Electric Drum Kit' prog 118 -> drums (551 notes, median pitch F#2)
  track '2023 Drum Kit' prog 0 -> drums (125 notes, median pitch F1)
  track 'Electric Drum Kit' prog 118 -> drums (340 notes, median pitch F#2)
  track '909 Drum Kit' prog 0 -> drums (125 notes, median pitch C2)
  track 'Bass Guitar' prog 33 -> bass (176 notes, median pitch B1)
  track 'EDM Kit (E)' prog 0 -> drums (48 notes, median pitch B5)
  track '8-Bit Square' prog 80 -> lead2 (50 notes, median pitch D4)
  track '8-Bit Square' prog 80 -> chords (126 notes, median pitch Bb4)
  track '8-Bit Square' prog 80 -> chords (116 notes, median pitch G5)
  track '8-Bit Square' prog 80 -> chords (229 notes, median pitch Eb5)
  track '8-Bit Square' prog 80 -> chords (190 notes, median pitch D5)
  track '8-Bit Square' prog 80 -> chords (220 notes, median pitch D5)
  track '8-Bit Square' prog 80 -> chords (163 notes, median pitch C3)
  track '8-Bit Square' prog 80 -> chords (147 notes, median pitch C5)
  track '8-Bit Square' prog 80 -> chords (134 notes, median pitch D3)
  track '8-Bit Square' prog 80 -> chords (172 notes, median pitch F#2)
  track '8-Bit Square' prog 80 -> chords (102 notes, median pitch Bb4)
  track '8-Bit Square' prog 80 -> chords (197 notes, median pitch Ab4)
  track '8-Bit Square' prog 80 -> chords (143 notes, median pitch C#2)
  track '8-Bit Square' prog 80 -> chords (107 notes, median pitch Eb5)
  track '8-Bit Square' prog 80 -> chords (198 notes, median pitch E3)
  track '8-Bit Square' prog 80 -> chords (134 notes, median pitch Ab2)
  track '8-Bit Square' prog 80 -> lead2 (73 notes, median pitch E4)
  track '8-Bit Square' prog 80 -> lead2 (61 notes, median pitch G4)
  track '8-Bit Square' prog 80 -> chords (91 notes, median pitch F#4)
  track '8-Bit Square' prog 80 -> chords (49 notes, median pitch B3)
  track '8-Bit Square' prog 80 -> chords (151 notes, median pitch C3)
  track '8-Bit Square' prog 80 -> chords (548 notes, median pitch F#2)
  track '8-Bit Square' prog 80 -> chords (266 notes, median pitch C#4)
  track '8-Bit Square' prog 80 -> chords (117 notes, median pitch Bb2)
  track '8-Bit Square' prog 80 -> chords (205 notes, median pitch F3)
  track '8-Bit Square' prog 80 -> chords (154 notes, median pitch B3)
  track '8-Bit Square' prog 80 -> lead2 (48 notes, median pitch A4)
  track '8-Bit Square' prog 80 -> chords (219 notes, median pitch F#3)
  track '8-Bit Square' prog 80 -> chords (145 notes, median pitch Bb3)
  track '8-Bit Square' prog 80 -> chords (129 notes, median pitch G3)
  track '8-Bit Square' prog 80 -> chords (200 notes, median pitch F3)
  track '8-Bit Square' prog 80 -> chords (166 notes, median pitch E3)
```
       |       |       |       |       |       |       |       |       |       |       |       |    
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   XX..XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXX.XXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXX.
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
lead   XXX.XXXXX.XXXXXXXXXXXXXXX.XXXXXXX.XXXXXXXXXXXXXXXXXXXXXXX.XXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXX.
lead2  XXXXXX.......................................................................................
```
sections, 4-bar blocks (start bar : length : roles): 1:8:D+B+C+L+L2; 9:84:D+B+C+L; 93:1:D
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `NNNvNRvRNNNRvRRRRvRNNRN-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X..XXX..X..XXX..` vel 1.0±0.0; 27 `X...x........x..` vel 1.0±0.0; 29 `X...x........x..` vel 1.0±0.0; kick `X...x........x..` vel 1.0±0.0; tomL2 `...x....X.......` vel 0.94±0.12
- off-grid onsets 16%; swing8 0.668; swing16 0.333

#### OS #2647959 "An Enigmatic Encounter LOFI REMIX (WIP)" (https://onlinesequencer.net/2647959)
tempo 100 BPM, 4/4, 60 bars, mean active roles/bar 3.85
  track 'Grand Piano' prog 0 -> bass (78 notes, median pitch G2)
  track 'Music Box' prog 10 -> lead (315 notes, median pitch B3)
  track '808 Drum Kit' prog 0 -> drums (342 notes, median pitch C#2)
  track 'Electric Drum Kit' prog 118 -> drums (3840 notes, median pitch Ab1)
  track 'Vibraphone' prog 11 -> lead2 (315 notes, median pitch B3)
  track 'Electric Piano' prog 5 -> lead2 (80 notes, median pitch B4)
  track 'Jazz Guitar' prog 26 -> lead2 (189 notes, median pitch C4)
  track 'Grand Piano (Classic)' prog 0 -> lead2 (54 notes, median pitch B4)
```
      |       |       |       |       |       |       |       |   
drums XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass  XXXXXXXX.XXXXXXXXXXXXXXXX..XXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXX
lead  XXXXXXXX.XXXXXXXXXXXXXXXX..XXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXX
lead2 XXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:60:D+B+L+L2
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `NNNvRRNNvRvNvRR-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): 32 `XXXXXXXXXXXXXXXX` vel 1.0±0.0; kick2 `X...x.X.......x.` vel 1.0±0.0; snare `............X.XX` vel 1.0±0.0; clap `........X.......` vel 1.0±0.0; stick `......xxX...x.x.` vel 1.0±0.0
- off-grid onsets 56%; swing8 0.592; swing16 0.281

#### OS #2666668 "Some lofi beat whatnot" (https://onlinesequencer.net/2666668)
tempo 130 BPM, 4/4, 32 bars, mean active roles/bar 3.0
  track 'Flute (Classic)' prog 73 -> chords (384 notes, median pitch C#5)
  track 'Vibraphone' prog 11 -> lead (156 notes, median pitch D6)
  track '808 Drum Kit' prog 0 -> drums (416 notes, median pitch F#2)
  track '909 Drum Kit' prog 0 -> drums (160 notes, median pitch B1)
  track 'Grand Piano' prog 0 -> chords (576 notes, median pitch C#5)
  track 'Grand Piano (Classic)' prog 0 -> lead2 (156 notes, median pitch D6)
  track 'Music Box' prog 10 -> lead2 (156 notes, median pitch D6)
```
       |       |       |       |       
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   ........XXXXXXXX........XXXXXXXX
lead2  ........XXXXXXXX........XXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:8:D+C; 9:8:D+C+L+L2; 17:8:D+C; 25:8:D+C+L+L2
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 4 bars; lead 4-bar phrases: `--NR--RR-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X.X.XxXXX.X.X.X.` vel 1.0±0.0; kick2 `x...x.x...x.x...` vel 1.0±0.0; kick `x...x.x...x.x...` vel 1.0±0.0; snare `........X.......` vel 1.0±0.0
- off-grid onsets 3%; swing8 0.554; swing16 0.333

#### OS #2712152 "Almost Monday Morning (lofi)" (https://onlinesequencer.net/2712152)
tempo 65 BPM, 4/4, 32 bars, mean active roles/bar 5.38
  track 'Elec. Piano (Classic)' prog 4 -> lead (420 notes, median pitch D5)
  track 'Grand Piano (Classic)' prog 0 -> chords (335 notes, median pitch B3)
  track 'Pizzicato' prog 45 -> bass (119 notes, median pitch Ab2)
  track 'Jazz Guitar' prog 26 -> chords (109 notes, median pitch Bb3)
  track 'Drum Kit' prog 0 -> drums (150 notes, median pitch Ab1)
  track 'Smooth Synth (Classic)' prog 90 -> chords (4751 notes, median pitch Bb2)
  track '2013 Drum Kit' prog 0 -> drums (139 notes, median pitch B3)
  track 'Electric Drum Kit' prog 118 -> drums (287 notes, median pitch Eb3)
  track '8-Bit Sine' prog 80 -> bass (97 notes, median pitch G1)
  track '808 Drum Kit' prog 0 -> drums (284 notes, median pitch B1)
  track '8-Bit Triangle' prog 80 -> lead2 (135 notes, median pitch D4)
  track 'Flute (Classic)' prog 73 -> chords (54 notes, median pitch Bb3)
  track 'Vibraphone' prog 11 -> lead2 (139 notes, median pitch D5)
  track 'Violin (Classic)' prog 40 -> chords (1922 notes, median pitch Bb4)
  track '8-Bit Square' prog 80 -> lead2 (4 notes, median pitch E5)
  track 'Concert Harp' prog 46 -> lead2 (140 notes, median pitch D4)
  track 'Bass Guitar (Classic)' prog 33 -> bass (95 notes, median pitch G1)
  track 'Steel Drums' prog 114 -> other (109 notes, median pitch Ab3)
  track 'Trombone (Classic)' prog 57 -> chords (8 notes, median pitch Bb3)
  track 'Music Box' prog 10 -> lead2 (4 notes, median pitch G6)
```
       |       |       |       |       
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   XXXXXXXXXXXXXXX.XXXXXXXXXXXXXXX.
lead2  ....XXXXXXXXXXXXXXXXXXXXXXXXXXXX
other  ....X...XXXXXXX.XXXXXXXX....XX..
```
sections, 4-bar blocks (start bar : length : roles): 1:4:D+B+C+L; 5:4:D+B+C+L+L2; 9:16:D+B+C+L+L2+O; 25:4:D+B+C+L+L2; 29:4:D+B+C+L+L2+O
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `NRNNRRNN-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): 32 `...Xx.XxxX...Xx.` vel 1.0±0.0; kick2 `X..x...XX.X..x.x` vel 1.0±0.0; 59 `X...X...X...X...` vel 1.0±0.0; snare `....X.......X...` vel 1.0±0.0; 33 `.x..x...x...x...` vel 1.0±0.0
- off-grid onsets 62%; swing8 0.61; swing16 0.268

#### OS #2739403 "song (added lofi drums)" (https://onlinesequencer.net/2739403)
tempo 88 BPM, 4/4, 32 bars, mean active roles/bar 3.88
  track 'Grand Piano' prog 0 -> chords (374 notes, median pitch G4)
  track 'Bass Guitar (Classic)' prog 33 -> bass (144 notes, median pitch C#2)
  track 'Jazz Guitar' prog 26 -> bass (144 notes, median pitch C#2)
  track 'Smooth Synth (Classic)' prog 90 -> bass (320 notes, median pitch A2)
  track 'Electric Piano' prog 5 -> lead (192 notes, median pitch A5)
  track 'Synth Bass (Classic)' prog 38 -> bass (32 notes, median pitch D3)
  track 'Pop Synth (Classic)' prog 88 -> bass (96 notes, median pitch A2)
  track 'Electric Drum Kit' prog 118 -> drums (66 notes, median pitch Bb4)
  track 'Electric Guitar' prog 27 -> chords (256 notes, median pitch E2)
  track '808 Drum Kit' prog 0 -> drums (392 notes, median pitch F#2)
```
       |       |       |       |       
drums  ....XXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:4:B+C+L; 5:28:D+B+C+L
- drums enter bar 5, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 2 bars; lead 4-bar phrases: `NRRRRRRR-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X...X.X.X.X.X.X.` vel 1.0±0.0; kick2 `..X.......X.....` vel 1.0±0.0; snare `....X.......X...` vel 1.0±0.0; kick `X.....x.......x.` vel 1.0±0.0; maraca `..X...X...Xx..X.` vel 1.0±0.0
- off-grid onsets 10%; swing8 0.541; swing16 0.25

#### OS #2785963 "sweetheart (Lofi WIP)" (https://onlinesequencer.net/2785963)
tempo 160 BPM, 4/4, 67 bars, mean active roles/bar 3.84
  track 'Jazz Guitar' prog 26 -> lead (247 notes, median pitch C4)
  track '808 Drum Kit' prog 0 -> drums (559 notes, median pitch D2)
  track 'Bass Guitar (Classic)' prog 33 -> bass (394 notes, median pitch A1)
  track 'Electric Drum Kit' prog 118 -> drums (169 notes, median pitch F#2)
  track 'Grand Piano' prog 0 -> lead2 (197 notes, median pitch C4)
  track 'Vibraphone' prog 11 -> lead2 (16 notes, median pitch Bb3)
  track '8-Bit Sine' prog 80 -> other (39 notes, median pitch G3)
  track 'Smooth Synth (Classic)' prog 90 -> lead2 (48 notes, median pitch A3)
```
      |       |       |       |       |       |       |       |       |  
drums XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass  ..XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead  .XXXXXXXXXXXXXXXXXXXXXXXXX.......XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead2 XX........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
other ..........................XXXXXXXX.................................
```
sections, 4-bar blocks (start bar : length : roles): 1:4:D+B+L+L2; 5:4:D+B+L; 9:16:D+B+L+L2; 25:4:D+B+L+L2+O; 29:4:D+B+L2+O; 33:4:D+B+L+L2+O; 37:31:D+B+L+L2
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 4 bars; lead 4-bar phrases: `NNvRNNN-RRRRNNNNN` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X.x.X.x.X...X.x.` vel 1.0±0.0; kick2 `X...x..x.......X` vel 1.0±0.0; snare `........X.......` vel 1.0±0.0; openhat `............X...` vel 1.0±0.0; ride `X...............` vel 1.0±0.0
- off-grid onsets 2%; swing8 0.614; swing16 None

#### OS #2801489 "'God Is Good!' - Cool Lofi Hip-Hop beat - (AdMC)" (https://onlinesequencer.net/2801489)
tempo 135 BPM, 4/4, 56 bars, mean active roles/bar 3.29
  track 'Grand Piano' prog 0 -> chords (494 notes, median pitch A4)
  track '808 Drum Kit' prog 0 -> drums (500 notes, median pitch F#2)
  track '909 Drum Kit' prog 0 -> drums (259 notes, median pitch B1)
  track 'Flute (Classic)' prog 73 -> chords (494 notes, median pitch A4)
  track 'Music Box' prog 10 -> lead (164 notes, median pitch E5)
  track 'Vibraphone' prog 11 -> lead2 (164 notes, median pitch E5)
  track '8-Bit Triangle' prog 80 -> bass (224 notes, median pitch Bb1)
```
       |       |       |       |       |       |       |       
drums  XXXXXXXXXXXXXXXXXXXXXXXX........XXXXXXXXXXXXXXXX........
bass   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   ................XXXXXXXX................XXXXXXXX........
lead2  ................XXXXXXXX................XXXXXXXX........
```
sections, 4-bar blocks (start bar : length : roles): 1:16:D+B+C; 17:8:D+B+C+L+L2; 25:8:B+C; 33:8:D+B+C; 41:8:D+B+C+L+L2; 49:8:B+C
- drums enter bar 1, drum dropouts (start bar,len): [(25, 8)], bars after drums stop: 8
- harmonic loop period: 4 bars; lead 4-bar phrases: `----NR----RR---` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X.X.X.XxX.X.X.X.` vel 1.0±0.0; kick2 `X...X.X...Xxx.X.` vel 1.0±0.0; snare `........X.......` vel 1.0±0.0
- off-grid onsets 1%; swing8 0.53; swing16 None

#### OS #2813988 "'Yes Lord!' - Gospel RnB/Soul/ChillHop - (AdMC)" (https://onlinesequencer.net/2813988)
tempo 135 BPM, 4/4, 40 bars, mean active roles/bar 2.8
  track '808 Drum Kit' prog 0 -> drums (464 notes, median pitch F#2)
  track 'Grand Piano' prog 0 -> chords (240 notes, median pitch G5)
  track 'Bass Guitar (Classic)' prog 33 -> bass (180 notes, median pitch D2)
  track '8-Bit Triangle' prog 80 -> bass (180 notes, median pitch D2)
  track 'Flute (Classic)' prog 73 -> chords (720 notes, median pitch G5)
  track '8-Bit Sawtooth' prog 81 -> chords (192 notes, median pitch C#4)
  track '8-Bit Sine' prog 80 -> chords (192 notes, median pitch C#4)
```
       |       |       |       |       |       
drums  ........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:8:B+C; 9:32:D+B+C
- drums enter bar 9, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 4 bars; lead 4-bar phrases: `n/a` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X.X.XXX.X.......` vel 1.0±0.0; kick2 `X.xxx.X.....x.X.` vel 1.0±0.0; snare `........X.......` vel 1.0±0.0
- off-grid onsets 3%; swing8 0.514; swing16 0.312

#### OS #2874622 "idealism" (https://onlinesequencer.net/2874622)
tempo 120 BPM, 4/4, 83 bars, mean active roles/bar 4.11
  track 'Harpsichord' prog 6 -> chords (36 notes, median pitch D4)
  track '808 Drum Kit' prog 0 -> drums (359 notes, median pitch F#2)
  track '909 Drum Kit' prog 0 -> drums (212 notes, median pitch D2)
  track '8-Bit Sawtooth' prog 81 -> chords (672 notes, median pitch Eb4)
  track '808 Bass' prog 38 -> bass (59 notes, median pitch C2)
  track '8-Bit Triangle' prog 80 -> bass (35 notes, median pitch C2)
  track 'Grand Piano' prog 0 -> lead (699 notes, median pitch F4)
  track '8-Bit Square' prog 80 -> other (576 notes, median pitch G3)
  track 'Vibraphone' prog 11 -> chords (16 notes, median pitch Eb4)
  track 'Electric Drum Kit' prog 118 -> drums (156 notes, median pitch Ab1)
  track 'Grand Piano (Classic)' prog 0 -> lead2 (23 notes, median pitch F5)
  track 'Music Box' prog 10 -> lead2 (18 notes, median pitch E5)
  track '8-Bit Sine' prog 80 -> other (156 notes, median pitch D3)
  track '8-Bit Drum Kit' prog 0 -> drums (128 notes, median pitch F#2)
```
       |       |       |       |       |       |       |       |       |       |       |  
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX........XXXXXXXXXXXXXXXXXXXXXXXX...
bass   XXXXXXXXXXXXXXXXXXXXXXXX........XXXXXXXXXXXXXXXX................XXXXXXXXXXXXXXXX...
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX................XXXXXXXXXXXXXXXX...
lead   .......XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXX
lead2  ........................................XXX.X.XX.......XX......XX..................
other  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX................XXXXXXXXXXXXXXXX...
```
sections, 4-bar blocks (start bar : length : roles): 1:8:D+B+C+O; 9:16:D+B+C+L+O; 25:8:D+C+L+O; 33:8:D+B+C+L+O; 41:8:D+B+C+L+L2+O; 49:8:L; 57:8:D+L; 65:16:D+B+C+L+O; 81:3:L
- drums enter bar 1, drum dropouts (start bar,len): [(49, 8)], bars after drums stop: 3
- harmonic loop period: 8 bars; lead 4-bar phrases: `-NNNNNRRvNRRNvRvNNvvN` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X.XxX..xX..xX.xx` vel 0.61±0.33; 31 `X.......X.......` vel 0.79±0.2; 33 `X.......X.......` vel 0.79±0.2; snare `........X.......` vel 0.97±0.12; kick2 `x.....x.........` vel 0.94±0.16
- off-grid onsets 23%; swing8 0.571; swing16 0.319

#### OS #2899063 "zephyr (lofi)" (https://onlinesequencer.net/2899063)
tempo 105 BPM, 4/4, 74 bars, mean active roles/bar 3.36
  track '808 Drum Kit' prog 0 -> drums (303 notes, median pitch F#2)
  track 'Electric Drum Kit' prog 118 -> drums (60 notes, median pitch E2)
  track 'Grand Piano' prog 0 -> chords (678 notes, median pitch E5)
  track 'Jazz Guitar' prog 26 -> lead2 (57 notes, median pitch F4)
  track '808 Bass' prog 38 -> bass (58 notes, median pitch C2)
  track 'Strings' prog 48 -> chords (1221 notes, median pitch Eb4)
  track 'Vibraphone' prog 11 -> chords (350 notes, median pitch Bb3)
  track '909 Drum Kit' prog 0 -> drums (52 notes, median pitch B3)
  track 'Synth Bass (Classic)' prog 38 -> bass (68 notes, median pitch Eb4)
  track 'Music Box' prog 10 -> other (117 notes, median pitch G3)
  track 'Violin' prog 40 -> lead2 (26 notes, median pitch Eb5)
  track 'Cello' prog 42 -> bass (36 notes, median pitch Bb2)
  track 'Grand Piano (Classic)' prog 0 -> chords (176 notes, median pitch B2)
  track '8-Bit Triangle' prog 80 -> chords (466 notes, median pitch C#5)
  track 'Elec. Piano (Classic)' prog 4 -> chords (115 notes, median pitch G2)
  track 'Pizzicato' prog 45 -> chords (682 notes, median pitch C4)
  track 'Flute (Classic)' prog 73 -> chords (1215 notes, median pitch Ab3)
  track '8-Bit Sawtooth' prog 81 -> other (120 notes, median pitch Bb1)
  track 'Steel Drums' prog 114 -> lead (80 notes, median pitch Bb3)
  track 'Synth Pluck (Classic)' prog 84 -> chords (439 notes, median pitch Eb3)
  track 'Saxophone' prog 66 -> lead2 (20 notes, median pitch A3)
  track 'Pop Synth (Classic)' prog 88 -> bass (16 notes, median pitch C#2)
  track 'Ragtime Piano' prog 3 -> chords (312 notes, median pitch D3)
  track 'Church Organ (Classic)' prog 19 -> chords (63 notes, median pitch C3)
```
       |       |       |       |       |       |       |       |       |       | 
drums  ......XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.......XXXXXXXXXXXXXXXXX..
bass   .XXX.XX.X.X.X.X.X.X.X.X.....XXXXXXXXXXXXXXXXXXXX........XXXXXXXXXXXXXXXX..
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   XXXXXX....................................................................
lead2  XX.XXXX.........XXXXXXXX....XXXXXXXX....XXXXXXXX........XXXXXXXXXXXXXXXX..
other  XXXXXXX.................................XXXXXXXX..........................
```
sections, 4-bar blocks (start bar : length : roles): 1:4:B+C+L+L2+O; 5:4:D+B+C+L+L2+O; 9:8:D+B+C; 17:8:D+B+C+L2; 25:4:D+C; 29:8:D+B+C+L2; 37:4:D+B+C; 41:8:D+B+C+L2+O; 49:8:C; 57:16:D+B+C+L2; 73:2:C
- drums enter bar 7, drum dropouts (start bar,len): [(49, 7)], bars after drums stop: 2
- harmonic loop period: None bars; lead 4-bar phrases: `NN-----------------` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X...X...X...X...` vel 0.74±0.26; kick2 `X...x.........x.` vel 1.0±0.0; snare2 `........X.......` vel 0.97±0.11; 59 `....X.......X...` vel 1.0±0.0; openhat `.......xX.......` vel 0.87±0.26
- off-grid onsets 0%; swing8 0.537; swing16 None

#### OS #2911420 "Rengoku's Theme [Lofi Remix] wip, its bad :(" (https://onlinesequencer.net/2911420)
tempo 100 BPM, 4/4, 38 bars, mean active roles/bar 2.55
  track 'Music Box' prog 10 -> lead (162 notes, median pitch F#4)
  track 'Drum Kit' prog 0 -> drums (110 notes, median pitch G1)
  track '8-Bit Sine' prog 80 -> chords (52 notes, median pitch Ab2)
  track '8-Bit Triangle' prog 80 -> lead2 (46 notes, median pitch D5)
  track 'Acoustic Gtr (Classic)' prog 25 -> lead2 (41 notes, median pitch E4)
```
       |       |       |       |       |     
drums  ......XXXXXX.XXXXXXX.XXXXXXXXXXXXXXXXX
chords ......XXXXXXX.XXXXXXX.................
lead   XXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXX
lead2  ......................XXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:4:L; 5:16:D+C+L; 21:18:D+L+L2
- drums enter bar 7, drum dropouts (start bar,len): [(13, 1), (21, 1)], bars after drums stop: 0
- harmonic loop period: 4 bars; lead 4-bar phrases: `NNvvNNvvRN` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): kick `X..x..x.........` vel 1.0±0.0; 28 `........X.......` vel 1.0±0.0; 31 `........X.......` vel 1.0±0.0; clap `........X.......` vel 1.0±0.0; kick2 `X...............` vel 1.0±0.0
- off-grid onsets 5%; swing8 0.611; swing16 None

#### OS #2923593 "Vinci (lofi remix)" (https://onlinesequencer.net/2923593)
tempo 159 BPM, 4/4, 160 bars, mean active roles/bar 2.63
  track 'Grand Piano' prog 0 -> chords (1316 notes, median pitch C4)
  track '808 Drum Kit' prog 0 -> drums (469 notes, median pitch D2)
  track 'Electric Drum Kit' prog 118 -> drums (922 notes, median pitch F#2)
  track '2013 Drum Kit' prog 0 -> drums (1024 notes, median pitch F#3)
  track '8-Bit Triangle' prog 80 -> bass (312 notes, median pitch C2)
  track '8-Bit Sine' prog 80 -> chords (946 notes, median pitch Bb4)
```
       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       
drums  ........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX................
bass   ................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XX.XXXX.XXXXXXXXXXXXXXXXXXXXXX................
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:8:C; 9:8:D+C; 17:128:D+B+C; 145:16:C
- drums enter bar 9, drum dropouts (start bar,len): [], bars after drums stop: 16
- harmonic loop period: None bars; lead 4-bar phrases: `n/a` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): tamb `X.XxXxX.X.XxXxX.` vel 0.9±0.26; hat `X...X...X...X...` vel 1.0±0.0; snare `........X.......` vel 1.0±0.0; kick2 `X...x.....x.....` vel 1.0±0.0; crash `..x.xxxXx.x..x.x` vel 0.78±0.25
- off-grid onsets 48%; swing8 0.599; swing16 0.208

#### OS #2929064 "Starry Night - Lofi Hip Hop" (https://onlinesequencer.net/2929064)
tempo 80 BPM, 2/4, 71 bars, mean active roles/bar 3.82
  track 'Flute (Classic)' prog 73 -> lead2 (90 notes, median pitch E6)
  track 'Grand Piano' prog 0 -> chords (112 notes, median pitch Bb3)
  track 'Acoustic Gtr (Classic)' prog 25 -> lead (140 notes, median pitch C#5)
  track 'Bass Guitar (Classic)' prog 33 -> bass (28 notes, median pitch Eb1)
  track '909 Drum Kit' prog 0 -> drums (80 notes, median pitch C2)
  track '808 Drum Kit' prog 0 -> drums (112 notes, median pitch F#2)
```
       |       |       |       |       |       |       |       |       |      
drums  ................XXXXXXXXXXXXXXXX................XXXXXXXXXXXXXXXX.......
bass   ........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.......
chords ........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.......
lead   ........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.......
lead2  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:8:L2; 9:8:B+C+L+L2; 17:16:D+B+C+L+L2; 33:16:B+C+L+L2; 49:16:D+B+C+L+L2; 65:7:L2
- drums enter bar 17, drum dropouts (start bar,len): [(33, 16)], bars after drums stop: 7
- harmonic loop period: 8 bars; lead 4-bar phrases: `--NNRRRRRRRRRRRR--` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `.XX....x` vel 1.0±0.0; kick `X.....x.` vel 1.0±0.0; kick2 `.xx.....` vel 1.0±0.0; stick `....X...` vel 1.0±0.0
- off-grid onsets 0%; swing8 0.55; swing16 0.25

#### OS #3156435 "Ninjago Overture Lofi" (https://onlinesequencer.net/3156435)
tempo 80 BPM, 4/4, 66 bars, mean active roles/bar 3.74
  track 'Grand Piano' prog 0 -> other (1022 notes, median pitch G3)
  track 'Flute (Classic)' prog 73 -> chords (454 notes, median pitch F5)
  track 'Elec. Piano (Classic)' prog 4 -> chords (454 notes, median pitch F4)
  track 'Ragtime Piano' prog 3 -> chords (454 notes, median pitch F5)
  track '808 Drum Kit' prog 0 -> drums (900 notes, median pitch F#2)
  track 'Acoustic Gtr (Classic)' prog 25 -> lead (4 notes, median pitch Eb5)
  track 'Grand Piano (Classic)' prog 0 -> chords (224 notes, median pitch E2)
  track 'Bass' prog 32 -> bass (224 notes, median pitch E2)
  track 'Harpsichord' prog 6 -> chords (152 notes, median pitch Bb4)
  track 'Synth Pluck (Classic)' prog 84 -> chords (152 notes, median pitch Bb4)
```
       |       |       |       |       |       |       |       |       | 
drums  .......XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..
bass   ........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   ......XX..........................................................
other  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:4:C+O; 5:4:C+L+O; 9:56:D+B+C+O; 65:2:C+O
- drums enter bar 8, drum dropouts (start bar,len): [], bars after drums stop: 2
- harmonic loop period: None bars; lead 4-bar phrases: `-N---------------` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X.X.X.X.X.X.X.X.` vel 1.0±0.0; kick2 `X.....x.x.x..xxx` vel 1.0±0.0; snare `....X.......X...` vel 1.0±0.0
- off-grid onsets 0%; swing8 0.521; swing16 0.25

#### OS #3160786 "Momo - You're Not Alone (Lofi HipHop Beat)" (https://onlinesequencer.net/3160786)
tempo 154 BPM, 4/4, 91 bars, mean active roles/bar 4.14
  track 'Music Box' prog 10 -> lead2 (102 notes, median pitch G4)
  track 'Cello' prog 42 -> chords (40 notes, median pitch C#3)
  track '808 Drum Kit' prog 0 -> drums (210 notes, median pitch B1)
  track '808 Bass' prog 38 -> bass (110 notes, median pitch F#2)
  track 'Drum Kit' prog 0 -> drums (48 notes, median pitch E2)
  track '8-Bit Drum Kit' prog 0 -> drums (125 notes, median pitch F#2)
  track 'Electric Drum Kit' prog 118 -> drums (310 notes, median pitch F#2)
  track 'Harpsichord' prog 6 -> chords (54 notes, median pitch C#4)
  track 'Jazz Guitar' prog 26 -> lead (216 notes, median pitch B3)
  track 'Smooth Synth (Classic)' prog 90 -> pad (64 notes, median pitch G3)
  track '8-Bit Triangle' prog 80 -> lead2 (76 notes, median pitch G4)
  track '2013 Drum Kit' prog 0 -> drums (73 notes, median pitch Bb2)
  track '8-Bit Sine' prog 80 -> chords (1313 notes, median pitch F#6)
  track 'Strings' prog 48 -> pad (32 notes, median pitch G4)
  track 'Bass' prog 32 -> bass (32 notes, median pitch Ab2)
  track 'Grand Piano' prog 0 -> lead2 (54 notes, median pitch C#4)
```
       |       |       |       |       |       |       |       |       |       |       |       |  
drums  ...XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.........XXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   ........X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X..XXXXXXXXXXXXXXX..XXXXXXX.XXXXXXX.XXX.X.XXXXXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX...................................................
pad    ........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.................................
lead   .......XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead2  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..................XXXXXXXX...XXXX..XXXXXXX.XXXXX.X.
```
sections, 4-bar blocks (start bar : length : roles): 1:4:C+L2; 5:4:D+C+L2; 9:32:D+B+C+P+L+L2; 41:8:B+P+L; 49:8:D+B+P+L; 57:4:D+B+P+L+L2; 61:28:D+B+L+L2; 89:3:D+B+L
- drums enter bar 4, drum dropouts (start bar,len): [(41, 9), (58, 1)], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `-NNNNvRRRNNNvRNNNRNNNRN` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X.X...X.x.X...X.` vel 1.0±0.0; kick2 `x...x.x.x.x.....` vel 1.0±0.0; pedalhat `....X.......X...` vel 1.0±0.0; openhat `....X.......X.x.` vel 1.0±0.0; snare2 `X.......x.......` vel 1.0±0.0
- off-grid onsets 39%; swing8 0.514; swing16 None

#### OS #3168171 "viva400 lofi - italianostylez x BBD/2 Collab " (https://onlinesequencer.net/3168171)
tempo 123 BPM, 2/4, 42 bars, mean active roles/bar 3.33
  track 'Violin' prog 40 -> lead (46 notes, median pitch G4)
  track 'Grand Piano' prog 0 -> chords (156 notes, median pitch F#5)
  track 'Strings' prog 48 -> chords (57 notes, median pitch C4)
  track 'Muted E-Guitar' prog 28 -> chords (480 notes, median pitch Bb3)
  track 'Bass Guitar (Classic)' prog 33 -> bass (137 notes, median pitch Eb2)
  track 'Electric Drum Kit' prog 118 -> drums (1076 notes, median pitch F#3)
  track '808 Drum Kit' prog 0 -> drums (20 notes, median pitch F#2)
  track '909 Drum Kit' prog 0 -> drums (18 notes, median pitch B1)
```
       |       |       |       |       |       | 
drums  .XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   .XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXX.
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
lead   .................XXXXXXXXX..XX...XXXXXXX.X
```
sections, 4-bar blocks (start bar : length : roles): 1:16:D+B+C; 17:26:D+B+C+L
- drums enter bar 2, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `----NNNNNNN` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): cabasa `XXXXXXXX` vel 1.0±0.0; hat `XX.XXX.X` vel 1.0±0.0; claves `xx..X..X` vel 1.0±0.0; tamb `..XX..X.` vel 1.0±0.0; kick2 `X.xx..x.` vel 1.0±0.0
- off-grid onsets 1%; swing8 0.621; swing16 0.25

#### OS #3213903 "Somewhat lofi 2 (final 11/24)" (https://onlinesequencer.net/3213903)
tempo 92 BPM, 4/4, 39 bars, mean active roles/bar 3.46
  track 'Grand Piano' prog 0 -> chords (482 notes, median pitch C#5)
  track '8-Bit Sawtooth' prog 81 -> other (317 notes, median pitch C#3)
  track '808 Drum Kit' prog 0 -> drums (120 notes, median pitch C#2)
  track 'Electric Drum Kit' prog 118 -> drums (170 notes, median pitch F#2)
  track '2013 Drum Kit' prog 0 -> drums (86 notes, median pitch F#2)
  track '909 Drum Kit' prog 0 -> drums (231 notes, median pitch F#2)
  track '8-Bit Square' prog 80 -> other (50 notes, median pitch C#3)
  track 'Flute (Classic)' prog 73 -> chords (48 notes, median pitch Bb3)
  track 'Music Box' prog 10 -> lead (82 notes, median pitch Ab4)
  track 'Ragtime Piano' prog 3 -> lead2 (32 notes, median pitch C6)
  track '8-Bit Sine' prog 80 -> lead2 (82 notes, median pitch F#4)
  track 'Synth Pluck (Classic)' prog 84 -> lead2 (6 notes, median pitch C5)
  track 'French Horn (Classic)' prog 60 -> bass (8 notes, median pitch Bb2)
  track 'Drum Kit' prog 0 -> drums (62 notes, median pitch E3)
  track 'Saxophone' prog 66 -> lead2 (29 notes, median pitch F5)
  track 'Pizzicato' prog 45 -> lead2 (14 notes, median pitch D6)
  track 'Scifi' prog 103 -> lead2 (38 notes, median pitch F4)
```
       |       |       |       |       |      
drums  .......XXXXXXXXXXXXXXXXX....XXXX.......
bass   ................X.XXX.XX....X.X........
chords XXXXXXXXXXXXXXXXXXXXXXXX....XXXXXXXXXXX
lead   ................XXXXX.XXXXXXX.XX.......
lead2  ........XXXXXX..XXXXXXXXXXXXXXXX.......
other  XXXXXXXXXXXXXXXXXXXXXXXX....XXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:8:C+O; 9:8:D+C+L2+O; 17:8:D+B+C+L+L2+O; 25:4:L+L2; 29:4:D+B+C+L+L2+O; 33:7:C+O
- drums enter bar 8, drum dropouts (start bar,len): [(25, 4)], bars after drums stop: 7
- harmonic loop period: 8 bars; lead 4-bar phrases: `----NNNR--` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `..X.X.X.x.X.X.X.` vel 1.0±0.0; snare `X.......X.......` vel 1.0±0.0; openhat `..X.X.....X.X...` vel 1.0±0.0; kick2 `X...............` vel 1.0±0.0; kick `X...............` vel 1.0±0.0
- off-grid onsets 0%; swing8 0.524; swing16 0.25

#### OS #3231257 "Deep Breath (Lofi Girl / Vlad)" (https://onlinesequencer.net/3231257)
tempo 75 BPM, 4/4, 35 bars, mean active roles/bar 5.66
  track 'Clean Guitar' prog 27 -> lead2 (172 notes, median pitch B3)
  track 'Jazz Guitar' prog 26 -> lead (352 notes, median pitch A3)
  track 'Flute (Classic)' prog 73 -> lead2 (11 notes, median pitch Eb6)
  track 'Grand Piano' prog 0 -> lead2 (40 notes, median pitch E6)
  track 'Bass' prog 32 -> bass (12 notes, median pitch Ab1)
  track 'Bass Guitar (Classic)' prog 33 -> bass (7 notes, median pitch E1)
  track 'Drum Kit' prog 0 -> drums (364 notes, median pitch G2)
  track 'Smooth Synth (Classic)' prog 90 -> chords (724 notes, median pitch C4)
  track '8-Bit Square' prog 80 -> lead2 (12 notes, median pitch Eb6)
  track 'Music Box' prog 10 -> lead2 (16 notes, median pitch F#6)
  track '2013 Drum Kit' prog 0 -> drums (40 notes, median pitch F#2)
  track 'Electric Drum Kit' prog 118 -> drums (98 notes, median pitch Ab4)
  track '808 Drum Kit' prog 0 -> drums (48 notes, median pitch C2)
  track '8-Bit Sine' prog 80 -> lead2 (129 notes, median pitch Eb4)
  track 'Pizzicato' prog 45 -> other (655 notes, median pitch E1)
  track 'French Horn (Classic)' prog 60 -> bass (420 notes, median pitch F2)
  track 'Concert Harp' prog 46 -> other (77 notes, median pitch Ab3)
  track 'Xylophone' prog 13 -> lead2 (5 notes, median pitch Bb6)
  track 'Vibraphone' prog 11 -> lead2 (5 notes, median pitch F#6)
  track 'Acoustic Gtr (Classic)' prog 25 -> other (82 notes, median pitch F#3)
  track 'Strings' prog 48 -> pad (33 notes, median pitch B6)
  track 'Trombone (Classic)' prog 57 -> bass (15 notes, median pitch Ab2)
  track 'Elec. Piano (Classic)' prog 4 -> lead2 (176 notes, median pitch A4)
  track '8-Bit Triangle' prog 80 -> chords (5747 notes, median pitch E3)
  track '8-Bit Sawtooth' prog 81 -> chords (196 notes, median pitch G6)
  track 'Electric Guitar' prog 27 -> lead2 (124 notes, median pitch Eb4)
  track 'Ragtime Piano' prog 3 -> lead2 (352 notes, median pitch G4)
  track 'Steel Drums' prog 114 -> bass (162 notes, median pitch C#1)
  track 'Koto' prog 107 -> bass (6 notes, median pitch C1)
  track 'Harpsichord' prog 6 -> chords (1824 notes, median pitch B4)
```
       |       |       |       |       |  
drums  ........XXXXXXXXXXXXXXXXXXXX.......
bass   ....XX..XXXXXXXXXXXXXXXXXXXXXXXXXXX
chords ....X...X...XXXXXXXXXXXXXXXXXXXXXXX
pad    XXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXX..
lead   XXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXX..
lead2  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..
other  ........XXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:4:P+L+L2; 5:4:B+P+L+L2; 9:4:D+B+P+L+L2+O; 13:16:D+B+C+P+L+L2+O; 29:4:B+C+P+L+L2+O; 33:3:B+C+O
- drums enter bar 9, drum dropouts (start bar,len): [], bars after drums stop: 7
- harmonic loop period: 1 bars; lead 4-bar phrases: `NRRRNvRRN` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): 28 `x.x.X.x.....X.x.` vel 0.91±0.1; hat `...X.X.....X.X.x` vel 0.38±0.3; kick `X.........X...x.` vel 1.0±0.0; tomL2 `X....XXXXXXXXXXX` vel 1.0±0.0; 76 `....X.....x.X...` vel 0.88±0.24
- off-grid onsets 61%; swing8 0.6; swing16 0.292

#### OS #3246562 "Vulm/Lofi Girl (it's fine)" (https://onlinesequencer.net/3246562)
tempo 110 BPM, 4/4, 48 bars, mean active roles/bar 2.08
  track 'Grand Piano' prog 0 -> chords (291 notes, median pitch Ab4)
  track 'Ragtime Piano' prog 3 -> chords (860 notes, median pitch Ab5)
  track '8-Bit Sine' prog 80 -> chords (240 notes, median pitch F2)
  track 'Smooth Synth (Classic)' prog 90 -> chords (1344 notes, median pitch A3)
  track '909 Drum Kit' prog 0 -> drums (80 notes, median pitch C2)
  track '2013 Drum Kit' prog 0 -> drums (111 notes, median pitch F#2)
  track 'Electric Drum Kit' prog 118 -> drums (30 notes, median pitch Eb5)
  track 'Concert Harp' prog 46 -> chords (106 notes, median pitch A4)
  track 'Electric Piano' prog 5 -> chords (102 notes, median pitch G5)
  track 'Elec. Piano (Classic)' prog 4 -> chords (102 notes, median pitch G5)
  track 'Drum Kit' prog 0 -> drums (128 notes, median pitch A1)
  track '8-Bit Sawtooth' prog 81 -> lead (36 notes, median pitch B3)
  track 'Flute (Classic)' prog 73 -> lead2 (36 notes, median pitch A3)
```
       |       |       |       |       |       |       
drums  ........XXXXXXXXXXXX....XXXXXXXX................
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   ................................XXXXXXXXXXXXXXXX
lead2  ................................XXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:8:C; 9:12:D+C; 21:4:C; 25:8:D+C; 33:16:C+L+L2
- drums enter bar 9, drum dropouts (start bar,len): [(21, 4)], bars after drums stop: 16
- harmonic loop period: 4 bars; lead 4-bar phrases: `--------NvRR-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): 33 `XXXXXXXXXXXXXXXX` vel 1.0±0.0; hat `..X...X...X..XxX` vel 1.0±0.0; kick `X...X...X...X...` vel 1.0±0.0; claves `.............XXX` vel 1.0±0.0
- off-grid onsets 5%; swing8 0.576; swing16 0.266

#### OS #3278314 "Moleskinsoft Clone Remover/Plastic Pop (Lofi Remix)" (https://onlinesequencer.net/3278314)
tempo 90 BPM, 4/4, 69 bars, mean active roles/bar 3.45
  track '808 Drum Kit' prog 0 -> drums (621 notes, median pitch D2)
  track 'Flute (Classic)' prog 73 -> chords (1441 notes, median pitch F5)
  track 'Smooth Synth (Classic)' prog 90 -> chords (6525 notes, median pitch F3)
  track '8-Bit Sine' prog 80 -> other (212 notes, median pitch D3)
  track 'Grand Piano' prog 0 -> lead (113 notes, median pitch E4)
  track '8-Bit Triangle' prog 80 -> lead2 (9 notes, median pitch E4)
```
       |       |       |       |       |       |       |       |       |    
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXX
lead   ........XXXXXXXX.............................XXXXXXXXXXXXXXXXX.......
lead2  ............................XXXXXXXX.................................
other  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:8:D+C+O; 9:8:D+C+L+O; 17:12:D+C+O; 29:8:D+C+L2+O; 37:8:D+C+O; 45:20:D+C+L+O; 65:5:D+C+O
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 4 bars; lead 4-bar phrases: `--NN-------NNvRN--` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `..X...X...X...X.` vel 1.0±0.0; kick2 `X.......X..X....` vel 1.0±0.0; snare `....X.......X...` vel 1.0±0.0
- off-grid onsets 37%; swing8 0.55; swing16 None

#### OS #3302712 "lofi hip-hop/chill house but it's too fast to be hip hop and too slow to be house" (https://onlinesequencer.net/3302712)
tempo 110 BPM, 4/4, 32 bars, mean active roles/bar 3.62
  track 'Electric Piano' prog 5 -> lead (124 notes, median pitch F4)
  track 'Scifi' prog 103 -> lead2 (72 notes, median pitch F#5)
  track 'Music Box' prog 10 -> lead2 (72 notes, median pitch F#5)
  track 'Ragtime Piano' prog 3 -> chords (318 notes, median pitch F4)
  track 'Koto' prog 107 -> lead2 (72 notes, median pitch F#4)
  track '909 Drum Kit' prog 0 -> drums (292 notes, median pitch F#2)
  track 'Synth Pluck (Classic)' prog 84 -> other (72 notes, median pitch F#3)
  track 'Strings' prog 48 -> bass (12 notes, median pitch Bb2)
  track 'Bass Guitar (Classic)' prog 33 -> bass (12 notes, median pitch Bb1)
```
       |       |       |       |       
drums  X...XXXXXXXXXXXXX...XXXXXXXXXXXX
bass   ....XXXX............XXXX........
chords ........XXXXXXXX........XXXXXXXX
lead   XXXXXXXX........XXXXXXXX........
lead2  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
other  .......XXXXXXXXX.......XXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:4:L+L2; 5:4:D+B+L+L2; 9:8:D+C+L2+O; 17:4:L+L2; 21:4:D+B+L+L2; 25:8:D+C+L2+O
- drums enter bar 5, drum dropouts (start bar,len): [(18, 3)], bars after drums stop: 0
- harmonic loop period: 4 bars; lead 4-bar phrases: `Nv--Nv---` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `x.X.xX.XX.XXxXX.` vel 1.0±0.0; kick2 `X...X...X...X...` vel 1.0±0.0; 59 `..X...X...X...X.` vel 1.0±0.0; clap `....X.......X...` vel 1.0±0.0; snare `....X.......X...` vel 1.0±0.0
- off-grid onsets 0%; swing8 0.561; swing16 0.25

#### OS #3361850 "Worst OS MegaCollab ever! Lofi Girl Part" (https://onlinesequencer.net/3361850)
tempo 170 BPM, 4/4, 168 bars, mean active roles/bar 3.71
  track 'Vibraphone' prog 11 -> lead (448 notes, median pitch G5)
  track '2013 Drum Kit' prog 0 -> drums (1541 notes, median pitch F#2)
  track 'Distortion Guitar' prog 30 -> chords (1159 notes, median pitch E4)
  track 'Synth Pluck (Classic)' prog 84 -> chords (777 notes, median pitch Ab2)
  track '8-Bit Triangle' prog 80 -> bass (200 notes, median pitch F#1)
  track '8-Bit Sine' prog 80 -> bass (258 notes, median pitch A2)
  track '8-Bit Square' prog 80 -> other (207 notes, median pitch F3)
  track 'Clean Guitar' prog 27 -> bass (160 notes, median pitch Eb2)
  track 'Electric Drum Kit' prog 118 -> drums (575 notes, median pitch Bb2)
  track 'Xylophone' prog 13 -> lead2 (222 notes, median pitch G4)
  track 'Koto' prog 107 -> other (192 notes, median pitch F#3)
  track 'Strings' prog 48 -> chords (45 notes, median pitch C#3)
  track 'Ragtime Piano' prog 3 -> lead2 (67 notes, median pitch Eb5)
  track 'Violin (Classic)' prog 40 -> lead2 (66 notes, median pitch Eb5)
  track 'Bass' prog 32 -> bass (512 notes, median pitch C#3)
  track 'Electric Guitar' prog 27 -> lead2 (364 notes, median pitch C#5)
  track 'Music Box' prog 10 -> lead2 (19 notes, median pitch A3)
  track 'Acoustic Gtr (Classic)' prog 25 -> bass (17 notes, median pitch F#2)
  track '808 Drum Kit' prog 0 -> drums (478 notes, median pitch B1)
  track 'Dist. Guitar (Classic)' prog 30 -> chords (128 notes, median pitch F2)
  track '909 Drum Kit' prog 0 -> drums (929 notes, median pitch F#2)
  track 'Steel Drums' prog 114 -> chords (1495 notes, median pitch Ab3)
  track 'Smooth Synth (Classic)' prog 90 -> chords (608 notes, median pitch Ab3)
  track '8-Bit Sawtooth' prog 81 -> other (1192 notes, median pitch Ab3)
  track 'Concert Harp' prog 46 -> lead2 (12 notes, median pitch Bb3)
  track 'Grand Piano' prog 0 -> chords (152 notes, median pitch B3)
  track 'Pizzicato' prog 45 -> chords (72 notes, median pitch Ab5)
  track 'Pop Synth (Classic)' prog 88 -> bass (166 notes, median pitch F1)
  track 'Synth Bass (Classic)' prog 38 -> bass (352 notes, median pitch C3)
  track '808 Bass' prog 38 -> bass (6 notes, median pitch G1)
  track 'Muted E-Guitar' prog 28 -> lead2 (128 notes, median pitch C5)
  track 'Elec. Piano (Classic)' prog 4 -> chords (256 notes, median pitch E5)
```
       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       
drums  ....XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   ....XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.................XXXXXXXXXXXXXXXX..XXXXXXXX.XXXX.XXXXXXXXX.XXXXXXXXXXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..XXXXXXXXX....XXXXXXXXXXXX
lead   XXXXXXXXXXXXXXXXXXXXXXXXXXXX............................................................................................................................................
lead2  ........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.................................................X..............................................................XXXXXXXXXXXX
other  ........XXXXXXXXXXXX.........................................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.................XXXXXXXXXXXXXXXX..XXXXXXXX................XXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:4:C+L; 5:4:D+B+C+L; 9:12:D+B+C+L+L2+O; 21:8:D+B+C+L+L2; 29:16:D+B+C+L2; 45:16:D+B+C; 61:32:D+B+C+O; 93:16:D+C; 109:28:D+B+C+O; 137:16:D+B+C; 153:4:D+B+O; 157:12:D+B+C+L2+O
- drums enter bar 5, drum dropouts (start bar,len): [(61, 1), (94, 1), (127, 1)], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `NRRNRRR------------------------------------` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): kick2 `X.X.x.x.x.X.x.x.` vel 0.95±0.11; hat `x.X.x.X.x.X.xxXx` vel 0.99±0.04; snare `....X..xxx..X..x` vel 0.9±0.19; 52 `X.X.X.X.X.X.X.X.` vel 1.0±0.0; ride `X.X.X.X.X.X.X.X.` vel 1.0±0.0
- off-grid onsets 10%; swing8 0.538; swing16 0.256

#### OS #3428945 "'God Is Pog!' - Cool Lofi Hip-Hop beat - (sped up)" (https://onlinesequencer.net/3428945)
tempo 175 BPM, 4/4, 56 bars, mean active roles/bar 3.29
  track 'Grand Piano' prog 0 -> chords (494 notes, median pitch A4)
  track '808 Drum Kit' prog 0 -> drums (500 notes, median pitch F#2)
  track '909 Drum Kit' prog 0 -> drums (259 notes, median pitch B1)
  track 'Flute (Classic)' prog 73 -> chords (494 notes, median pitch A4)
  track 'Music Box' prog 10 -> lead (164 notes, median pitch E5)
  track 'Vibraphone' prog 11 -> lead2 (164 notes, median pitch E5)
  track '8-Bit Triangle' prog 80 -> bass (224 notes, median pitch Bb1)
```
       |       |       |       |       |       |       |       
drums  XXXXXXXXXXXXXXXXXXXXXXXX........XXXXXXXXXXXXXXXX........
bass   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   ................XXXXXXXX................XXXXXXXX........
lead2  ................XXXXXXXX................XXXXXXXX........
```
sections, 4-bar blocks (start bar : length : roles): 1:16:D+B+C; 17:8:D+B+C+L+L2; 25:8:B+C; 33:8:D+B+C; 41:8:D+B+C+L+L2; 49:8:B+C
- drums enter bar 1, drum dropouts (start bar,len): [(25, 8)], bars after drums stop: 8
- harmonic loop period: 4 bars; lead 4-bar phrases: `----NR----RR---` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X.X.X.XxX.X.X.X.` vel 1.0±0.0; kick2 `X...X.X...Xxx.X.` vel 1.0±0.0; snare `........X.......` vel 1.0±0.0
- off-grid onsets 1%; swing8 0.53; swing16 None

#### OS #3514772 "chord edit for lofi girl" (https://onlinesequencer.net/3514772)
tempo 270 BPM, 4/4, 68 bars, mean active roles/bar 3.18
  track 'Grand Piano' prog 0 -> chords (212 notes, median pitch D5)
  track 'Smooth Synth' prog 90 -> chords (450 notes, median pitch G4)
  track 'Pop Synth (Classic)' prog 88 -> lead (384 notes, median pitch G4)
  track 'Flute (Classic)' prog 73 -> chords (48 notes, median pitch B5)
  track 'Concert Harp' prog 46 -> bass (320 notes, median pitch C2)
  track '8-Bit Drum Kit' prog 0 -> drums (160 notes, median pitch D2)
  track 'Elec. Piano (Classic)' prog 4 -> bass (320 notes, median pitch C#1)
  track 'Drum Kit' prog 0 -> drums (10 notes, median pitch Eb5)
  track 'Drum Kit' prog 0 -> drums (30 notes, median pitch Eb5)
  track '808 Drum Kit' prog 0 -> drums (184 notes, median pitch F#2)
  track '2023 Drum Kit' prog 0 -> drums (92 notes, median pitch F#2)
  track 'Bass Guitar' prog 33 -> bass (40 notes, median pitch B1)
```
       |       |       |       |       |       |       |       |       |   
drums  XXXXXXXXXXXXXXXXXXXX................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   XXXXXXXXXXXXXXXXXXXX................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords ....XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   ....................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:4:D+B; 5:16:D+B+C; 21:16:C+L; 37:32:D+B+C+L
- drums enter bar 1, drum dropouts (start bar,len): [(21, 16)], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `-----NNRNvvRvRRRv-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): snare `X.x.X.x.x.x.X.x.` vel 0.95±0.15; hat `X...Xx..x...xx..` vel 0.76±0.29; claves `X.......X.......` vel 1.0±0.0; kick2 `x.......x.......` vel 0.94±0.14; openhat `........X.......` vel 1.0±0.0
- off-grid onsets 24%; swing8 0.5; swing16 0.188

#### OS #3550041 "chord edit for lofi girl ThatLoFiKid" (https://onlinesequencer.net/3550041)
tempo 270 BPM, 4/4, 117 bars, mean active roles/bar 3.22
  track 'Grand Piano' prog 0 -> chords (374 notes, median pitch D5)
  track 'Smooth Synth' prog 90 -> chords (900 notes, median pitch G4)
  track 'Pop Synth (Classic)' prog 88 -> lead (768 notes, median pitch G4)
  track 'Flute (Classic)' prog 73 -> chords (96 notes, median pitch B5)
  track 'Concert Harp' prog 46 -> bass (320 notes, median pitch C2)
  track '8-Bit Drum Kit' prog 0 -> drums (160 notes, median pitch D2)
  track 'Elec. Piano (Classic)' prog 4 -> bass (320 notes, median pitch C#1)
  track 'Drum Kit' prog 0 -> drums (10 notes, median pitch Eb5)
  track 'Drum Kit' prog 0 -> drums (30 notes, median pitch Eb5)
  track '808 Drum Kit' prog 0 -> drums (368 notes, median pitch F#2)
  track '2023 Drum Kit' prog 0 -> drums (184 notes, median pitch F#2)
  track 'Bass Guitar' prog 33 -> bass (80 notes, median pitch B1)
```
       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |    
drums  XXXXXXXXXXXXXXXXXXXX................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
bass   XXXXXXXXXXXXXXXXXXXX................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
chords ....XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
lead   ....................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:4:D+B; 5:16:D+B+C; 21:16:C+L; 37:32:D+B+C+L; 69:16:C+L; 85:32:D+B+C+L; 117:1:L
- drums enter bar 1, drum dropouts (start bar,len): [(21, 16), (69, 16)], bars after drums stop: 1
- harmonic loop period: 16 bars; lead 4-bar phrases: `-----NNRNvvRvRRRvNNvNvvvvRRRRN` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): snare `X.X.x.x.x.x.x.x.` vel 0.92±0.18; hat `X.X.X.X.........` vel 0.76±0.29; kick2 `x.x.....x.x.....` vel 0.94±0.14; openhat `........x.x.....` vel 1.0±0.0; claves `X.......X.......` vel 1.0±0.0
- off-grid onsets 28%; swing8 0.515; swing16 0.188

#### OS #3582140 "Minecraft lofi Remix by Italianostylez " (https://onlinesequencer.net/3582140)
tempo 70 BPM, 4/4, 138 bars, mean active roles/bar 2.21
  track 'Electric Piano' prog 5 -> chords (115 notes, median pitch B3)
  track 'Grand Piano (Classic)' prog 0 -> lead (384 notes, median pitch C4)
  track 'Concert Harp' prog 46 -> lead2 (24 notes, median pitch F4)
  track 'Grand Piano' prog 0 -> lead2 (129 notes, median pitch A3)
  track 'Flute (Classic)' prog 73 -> chords (534 notes, median pitch C3)
  track 'Music Box' prog 10 -> lead2 (51 notes, median pitch G4)
  track 'Xylophone' prog 13 -> chords (186 notes, median pitch C#4)
  track 'Vibraphone' prog 11 -> lead2 (13 notes, median pitch C6)
  track 'Elec. Piano (Classic)' prog 4 -> other (155 notes, median pitch F#3)
  track 'Jazz Guitar' prog 26 -> chords (405 notes, median pitch B3)
  track 'Ragtime Piano' prog 3 -> other (154 notes, median pitch F#3)
  track 'Electric Drum Kit' prog 118 -> drums (415 notes, median pitch Ab2)
  track 'Steel Drums' prog 114 -> chords (1820 notes, median pitch Bb3)
  track 'Smooth Synth (Classic)' prog 90 -> lead2 (117 notes, median pitch B3)
  track '8-Bit Sawtooth' prog 81 -> lead2 (119 notes, median pitch B3)
  track '8-Bit Square' prog 80 -> lead2 (229 notes, median pitch G4)
  track 'Pizzicato' prog 45 -> bass (161 notes, median pitch E2)
  track 'Muted E-Guitar' prog 28 -> chords (180 notes, median pitch C#4)
  track 'Bass Guitar (Classic)' prog 33 -> bass (223 notes, median pitch D4)
```
       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       | 
drums  .........................................................................XXXXXXXXXXXXXXXXXXXXXXXXXX.......................................
bass   ..XXXX.............................................................................................XXXXXXXXXXXXXXXXXXXXX..................
chords XXXXXX..........XXXXXXXX...XXXXXXXXXXXXXX.......XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX....................X..................
lead   ......XXXXXXXXXX........................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..............................XXXXXXXXXXXXXXXXX..................
lead2  ................XXXXXXXXXXXXXXXXXXXXXXXXX................................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXX
other  ................................................XXXXXXXXXXXXXXXXXXXXXXXXX.................................................................
```
sections, 4-bar blocks (start bar : length : roles): 1:4:B+C; 5:4:B+C+L; 9:8:L; 17:8:C+L2; 25:4:L2; 29:12:C+L2; 41:8:L; 49:24:C+L+O; 73:28:D+C+L2; 101:4:B+L2; 105:16:B+L+L2; 121:18:L2
- drums enter bar 74, drum dropouts (start bar,len): [], bars after drums stop: 39
- harmonic loop period: None bars; lead 4-bar phrases: `-NNN------NNNNNNNNN------NNNNN-----` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): pedalhat `X.X.X.X.X.X.X.X.` vel 1.0±0.0; hat `.X.X.X.X.X.X.X.X` vel 1.0±0.0
- off-grid onsets 0%; swing8 0.624; swing16 0.25

#### OS #3601818 "Lofi Juice WRLD Type Beat - Skyline Views WIP" (https://onlinesequencer.net/3601818)
tempo 70 BPM, 4/4, 32 bars, mean active roles/bar 3.34
  track 'Grand Piano' prog 0 -> chords (127 notes, median pitch A3)
  track 'Strings' prog 48 -> lead (137 notes, median pitch E4)
  track '909 Drum Kit' prog 0 -> drums (462 notes, median pitch F#2)
  track '808 Drum Kit' prog 0 -> drums (72 notes, median pitch D2)
  track '8-Bit Sawtooth' prog 81 -> bass (107 notes, median pitch G1)
  track 'Scifi' prog 103 -> lead2 (24 notes, median pitch B3)
  track 'Acoustic Gtr (Classic)' prog 25 -> lead2 (112 notes, median pitch C#4)
  track 'Smooth Synth' prog 90 -> other (88 notes, median pitch G3)
```
       |       |       |       |       
drums  ........XXXXXXXXXXXXXXXXXXXXXXXX
bass   ........XXXXXXXXXXXXXXXXXXXXXXXX
chords XXXXXXXXXXXXXXXXX...X...........
lead   XXXXXXXXXXX.XXX.XXXXXXX.........
lead2  ................XXXXXXXXXXXXXXXX
other  ............................XXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:8:C+L; 9:8:D+B+C+L; 17:8:D+B+L+L2; 25:4:D+B+L2; 29:4:D+B+L2+O
- drums enter bar 9, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 4 bars; lead 4-bar phrases: `NNNRNN---` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `XXXXXXXXXXXXXXXX` vel 1.0±0.0; kick `X.x......xX....x` vel 1.0±0.0; snare `....X..x.x..X...` vel 1.0±0.0
- off-grid onsets 0%; swing8 0.616; swing16 0.25

#### OS #3652834 "Nujabes - Grand Piano" (https://onlinesequencer.net/3652834)
tempo 180 BPM, 2/4, 32 bars, mean active roles/bar 4.0
  track 'Grand Piano (Classic)' prog 0 -> lead2 (88 notes, median pitch D5)
  track 'Grand Piano' prog 0 -> lead2 (24 notes, median pitch C#4)
  track 'Violin' prog 40 -> lead (96 notes, median pitch Eb4)
  track 'Drum Kit' prog 0 -> drums (164 notes, median pitch Eb2)
  track 'Acoustic Gtr (Classic)' prog 25 -> bass (32 notes, median pitch F2)
```
      |       |       |       |       
drums XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead2 XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:32:D+B+L+L2
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 8 bars; lead 4-bar phrases: `NRRRRRRR-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): kick2 `X...x..X` vel 1.0±0.0; pedalhat `x...X...` vel 1.0±0.0; snare2 `X......x` vel 1.0±0.0; clap `X.......` vel 1.0±0.0; stick `X.......` vel 1.0±0.0
- off-grid onsets 0%; swing8 0.75; swing16 None

#### OS #3745767 "Lofi beats to Cornhole to." (https://onlinesequencer.net/3745767)
tempo 100 BPM, 3/4, 144 bars, mean active roles/bar 4.62
  track 'Grand Piano (Classic)' prog 0 -> lead (402 notes, median pitch D5)
  track '8-Bit Sine' prog 80 -> lead2 (36 notes, median pitch E4)
  track '2023 Drum Kit' prog 0 -> drums (180 notes, median pitch G2)
  track '2023 Drum Kit' prog 0 -> drums (2689 notes, median pitch F#2)
  track '2023 Drum Kit' prog 0 -> drums (36 notes, median pitch Ab3)
  track 'Strings' prog 48 -> chords (138 notes, median pitch A2)
  track 'Drum Kit' prog 0 -> drums (102 notes, median pitch E5)
  track 'Synth Pluck (Classic)' prog 84 -> lead2 (138 notes, median pitch D5)
  track 'Bass' prog 32 -> bass (24 notes, median pitch F#2)
  track 'Synth Pluck (Classic)' prog 84 -> other (54 notes, median pitch E3)
  track 'Pop Synth (Classic)' prog 88 -> chords (185 notes, median pitch C4)
```
       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   ................XXXXXXXX................XXXXXXXX................XXXXXXXX................XXXXXXXX................XXXXXXXX................XXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   XXXXXXX.XXXXXXX.XXXXXXXXXXXXXXX.XXXXXXX.XXXXXXXXXXXXXXX.XXXXXXX.XXXXXXXXXXXXXXX.XXXXXXX.XXXXXXXXXXXXXXX.XXXXXXX.XXXXXXXXXXXXXXX.XXXXXXX.XXXXXXXX
lead2  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
other  X...X.X.X...X.X.X...X.X.X...X.X.X...X.X.X...X.X.X...X.X.X...X.X.X...X.X.X...X.X.X...X.X.X...X.X.X...X.X.X...X.X.X...X.X.X...X.X.X...X.X.X...X.X.
```
sections, 4-bar blocks (start bar : length : roles): 1:4:D+C+L+L2; 5:4:D+C+L+L2+O; 9:4:D+C+L+L2; 13:4:D+C+L+L2+O; 17:4:D+B+C+L+L2; 21:4:D+B+C+L+L2+O; 25:4:D+C+L+L2; 29:4:D+C+L+L2+O; 33:4:D+C+L+L2; 37:4:D+C+L+L2+O; 41:4:D+B+C+L+L2; 45:4:D+B+C+L+L2+O; 49:4:D+C+L+L2; 53:4:D+C+L+L2+O; 57:4:D+C+L+L2; 61:4:D+C+L+L2+O; 65:4:D+B+C+L+L2; 69:4:D+B+C+L+L2+O; 73:4:D+C+L+L2; 77:4:D+C+L+L2+O; 81:4:D+C+L+L2; 85:4:D+C+L+L2+O; 89:4:D+B+C+L+L2; 93:4:D+B+C+L+L2+O; 97:4:D+C+L+L2; 101:4:D+C+L+L2+O; 105:4:D+C+L+L2; 109:4:D+C+L+L2+O; 113:4:D+B+C+L+L2; 117:4:D+B+C+L+L2+O; 121:4:D+C+L+L2; 125:4:D+C+L+L2+O; 129:4:D+C+L+L2; 133:4:D+C+L+L2+O; 137:4:D+B+C+L+L2; 141:4:D+B+C+L+L2+O
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 1 bars; lead 4-bar phrases: `NNvvvvRRRRRRRRRRRRRRRRRRRRRRRRRRRRRR-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `XXXXXxXxXxXx` vel 1.0±0.0; snare2 `......x...x.` vel 1.0±0.0; 88 `X...........` vel 1.0±0.0; 29 `X...........` vel 1.0±0.0; 67 `......X.....` vel 1.0±0.0
- off-grid onsets 45%; swing8 0.623; swing16 0.315

#### OS #3746760 "The Sound of Lofi" (https://onlinesequencer.net/3746760)
tempo 83 BPM, 4/4, 82 bars, mean active roles/bar 3.94
  track 'Flute (Classic)' prog 73 -> chords (1646 notes, median pitch Eb5)
  track 'Saxophone' prog 66 -> bass (518 notes, median pitch Ab1)
  track 'Drum Kit' prog 0 -> drums (494 notes, median pitch F3)
  track 'Electric Drum Kit' prog 118 -> drums (742 notes, median pitch B1)
  track '808 Drum Kit' prog 0 -> drums (864 notes, median pitch F#3)
  track '2013 Drum Kit' prog 0 -> drums (234 notes, median pitch E2)
  track '8-Bit Drum Kit' prog 0 -> drums (496 notes, median pitch E3)
  track 'Cello' prog 42 -> chords (466 notes, median pitch B2)
  track 'Vibraphone' prog 11 -> lead (370 notes, median pitch G4)
  track 'Jazz Guitar' prog 26 -> lead2 (259 notes, median pitch B4)
  track 'Pizzicato' prog 45 -> chords (1082 notes, median pitch E4)
  track 'Strings' prog 48 -> chords (417 notes, median pitch F#4)
  track 'Pop Synth (Classic)' prog 88 -> lead2 (97 notes, median pitch E4)
  track 'Steel Drums' prog 114 -> chords (912 notes, median pitch F#2)
  track '8-Bit Triangle' prog 80 -> chords (106 notes, median pitch E4)
  track 'Violin (Classic)' prog 40 -> chords (1381 notes, median pitch B2)
  track 'French Horn' prog 60 -> bass (13 notes, median pitch C#2)
  track 'Bass Guitar (Classic)' prog 33 -> bass (364 notes, median pitch B4)
  track 'Concert Harp' prog 46 -> bass (4 notes, median pitch F#2)
  track 'Church Organ (Classic)' prog 19 -> chords (774 notes, median pitch C4)
  track 'Koto' prog 107 -> chords (9 notes, median pitch G5)
  track '8-Bit Sine' prog 80 -> chords (41 notes, median pitch E5)
  track 'Harpsichord' prog 6 -> lead2 (7 notes, median pitch F5)
  track 'Xylophone' prog 13 -> chords (82 notes, median pitch D5)
  track 'Cello (Classic)' prog 42 -> chords (496 notes, median pitch B2)
  track 'Slap Bass' prog 36 -> bass (649 notes, median pitch B2)
  track 'French Horn (Classic)' prog 60 -> lead2 (7 notes, median pitch F4)
  track '8-Bit Sawtooth' prog 81 -> lead2 (66 notes, median pitch C5)
  track 'Synth Pluck (Classic)' prog 84 -> chords (214 notes, median pitch Eb4)
  track '909 Drum Kit' prog 0 -> drums (30 notes, median pitch E3)
  track 'Grand Piano (Classic)' prog 0 -> lead2 (5 notes, median pitch Bb4)
  track 'Elec. Piano (Classic)' prog 4 -> lead2 (6 notes, median pitch B3)
  track 'Music Box' prog 10 -> other (43 notes, median pitch C#3)
  track 'Ragtime Piano' prog 3 -> other (107 notes, median pitch G2)
  track 'Smooth Synth (Classic)' prog 90 -> chords (147 notes, median pitch D3)
  track 'Muted E-Guitar' prog 28 -> other (17 notes, median pitch E3)
```
       |       |       |       |       |       |       |       |       |       |       | 
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.X
bass   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
lead   XXXXXXX.........XXXXXXXXX.......XXXXXXXXX.......................XXXXXXXXX.........
lead2  XXXXXXX.................................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.........
other  XXXXXXX...........................................................................
```
sections, 4-bar blocks (start bar : length : roles): 1:8:D+B+C+L+L2+O; 9:8:D+B+C; 17:8:D+B+C+L; 25:8:D+B+C; 33:8:D+B+C+L; 41:24:D+B+C+L2; 65:8:D+B+C+L+L2; 73:8:D+B+C; 81:2:D+C
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 2
- harmonic loop period: 8 bars; lead 4-bar phrases: `NN--NNN-RRR-----RRR--` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): kick2 `X.x.xxx.X.X.x.x.` vel 0.59±0.34; 52 `X.xxX.X.XxX.Xxxx` vel 0.11±0.08; snare2 `....X.......X...` vel 0.21±0.1; ridebell `..X...X...X...X.` vel 0.64±0.48; ride `.X.X.X.X.X.X.X.X` vel 0.42±0.27
- off-grid onsets 1%; swing8 0.564; swing16 0.259

#### OS #3795663 "Chill lofi beat - Not Drby" (https://onlinesequencer.net/3795663)
tempo 90 BPM, 4/4, 76 bars, mean active roles/bar 3.3
  track 'Grand Piano' prog 0 -> other (415 notes, median pitch D3)
  track 'Concert Harp' prog 46 -> other (642 notes, median pitch F#3)
  track 'Concert Harp' prog 46 -> other (641 notes, median pitch F#3)
  track 'Grand Piano' prog 0 -> lead (204 notes, median pitch B4)
  track 'Cello' prog 42 -> bass (115 notes, median pitch A2)
  track 'Drum Kit' prog 0 -> drums (1091 notes, median pitch F#2)
  track '2023 Drum Kit' prog 0 -> drums (110 notes, median pitch F#1)
  track '808 Drum Kit' prog 0 -> drums (199 notes, median pitch B1)
  track '808 Drum Kit' prog 0 -> drums (111 notes, median pitch D2)
  track 'Electric Drum Kit' prog 118 -> drums (410 notes, median pitch F#2)
  track 'Electric Drum Kit' prog 118 -> drums (764 notes, median pitch E1)
  track 'Electric Drum Kit' prog 118 -> drums (5 notes, median pitch C#3)
  track 'Electric Drum Kit' prog 118 -> drums (5 notes, median pitch Ab2)
  track 'Bass' prog 32 -> bass (134 notes, median pitch A1)
  track '808 Bass' prog 38 -> bass (144 notes, median pitch A1)
  track '808 Bass' prog 38 -> bass (50 notes, median pitch Eb2)
  track 'Bass Guitar' prog 33 -> bass (147 notes, median pitch E3)
```
      |       |       |       |       |       |       |       |       |       |   
drums ........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX............
bass  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead  ....XXXX........XXXXXXXX........XXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXX............
other XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:4:B+O; 5:4:B+L+O; 9:8:D+B+O; 17:8:D+B+L+O; 25:8:D+B+O; 33:32:D+B+L+O; 65:12:B+O
- drums enter bar 9, drum dropouts (start bar,len): [], bars after drums stop: 12
- harmonic loop period: 4 bars; lead 4-bar phrases: `-N--Rv--RRNNRRRv----` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `XXXXXXXXXXXXXXXX` vel 1.0±0.0; 28 `XXXXXXXXXXXXXXXX` vel 1.0±0.0; kick2 `X.....X..XX..x..` vel 1.0±0.0; snare `....X.......X...` vel 1.0±0.0; 30 `............X...` vel 1.0±0.0
- off-grid onsets 3%; swing8 0.588; swing16 0.25

#### OS #3845458 "Lofi beat w/ whitejacket" (https://onlinesequencer.net/3845458)
tempo 100 BPM, 3/4, 135 bars, mean active roles/bar 2.86
  track 'Grand Piano (Classic)' prog 0 -> lead (225 notes, median pitch D5)
  track '8-Bit Sine' prog 80 -> lead2 (20 notes, median pitch E4)
  track '2023 Drum Kit' prog 0 -> drums (108 notes, median pitch G2)
  track '2023 Drum Kit' prog 0 -> drums (88 notes, median pitch F1)
  track '2023 Drum Kit' prog 0 -> drums (1508 notes, median pitch F#2)
  track '2023 Drum Kit' prog 0 -> drums (40 notes, median pitch F4)
  track 'Strings' prog 48 -> chords (81 notes, median pitch A2)
  track 'Drum Kit' prog 0 -> drums (57 notes, median pitch E5)
  track 'Synth Pluck (Classic)' prog 84 -> lead2 (76 notes, median pitch D5)
  track 'Bass' prog 32 -> bass (16 notes, median pitch F#2)
  track 'Synth Pluck (Classic)' prog 84 -> other (30 notes, median pitch E3)
  track 'Pop Synth (Classic)' prog 88 -> chords (185 notes, median pitch C4)
  track '808 Drum Kit' prog 0 -> drums (64 notes, median pitch B1)
  track '2013 Drum Kit' prog 0 -> drums (128 notes, median pitch F#2)
  track '909 Drum Kit' prog 0 -> drums (96 notes, median pitch D2)
```
       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |      
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..X...X...........X...X...X...X...........X...X...X...X
bass   ................XXXXXXXX................XXXXXXXXXXXXXXXX................XXXXXXXX.......................................................
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.......................................................
lead   XXXXXXX.XXXXXXX.XXXXXXXXXXXXXXX.XXXXXXX.XXXXXXXXXXXXXXXXXXXXXXX.XXXXXXX.XXXXXXXX.......................................................
lead2  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.......................................................
other  X...X.X.X...X.X.X...X.X.X...X.X.X...X.X.X...X.X.X...X.X.X...X.X.X...X.X.X...X.X........................................................
```
sections, 4-bar blocks (start bar : length : roles): 1:4:D+C+L+L2; 5:4:D+C+L+L2+O; 9:4:D+C+L+L2; 13:4:D+C+L+L2+O; 17:4:D+B+C+L+L2; 21:4:D+B+C+L+L2+O; 25:4:D+C+L+L2; 29:4:D+C+L+L2+O; 33:4:D+C+L+L2; 37:4:D+C+L+L2+O; 41:4:D+B+C+L+L2; 45:4:D+B+C+L+L2+O; 49:4:D+B+C+L+L2; 53:4:D+B+C+L+L2+O; 57:4:D+C+L+L2; 61:4:D+C+L+L2+O; 65:4:D+C+L+L2; 69:4:D+C+L+L2+O; 73:4:D+B+C+L+L2; 77:4:D+B+C+L+L2+O; 81:55:silence
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 55
- harmonic loop period: 1 bars; lead 4-bar phrases: `NNvvvvRRRRRRRRRRRRRR--------------` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `XXXXXxXxXXXx` vel 1.0±0.0; 29 `X.........X.` vel 1.0±0.0; snare `......X..x.x` vel 1.0±0.0; kick2 `x..x........` vel 1.0±0.0; snare2 `..........x.` vel 1.0±0.0
- off-grid onsets 42%; swing8 0.626; swing16 0.315

#### OS #3912419 "HOME - Resonance (Full) - Odyssey - (but with actual resonance at last) - by aesthratio (chillwave, lofi synthwave)" (https://onlinesequencer.net/3912419)
tempo 85 BPM, 4/4, 75 bars, mean active roles/bar 4.49
  track 'Scifi' prog 103 -> lead2 (293 notes, median pitch G5)
  track 'Synthesizer' prog 90 -> lead (896 notes, median pitch Bb4)
  track 'Synthesizer' prog 90 -> chords (2824 notes, median pitch C4)
  track 'Synth Bass' prog 39 -> bass (584 notes, median pitch C2)
  track '2013 Drum Kit' prog 0 -> drums (524 notes, median pitch F#3)
  track 'Synthesizer' prog 90 -> chords (1736 notes, median pitch Eb3)
  track 'Smooth Synth' prog 90 -> chords (2506 notes, median pitch C4)
  track 'Smooth Synth' prog 90 -> chords (2499 notes, median pitch C4)
  track 'Smooth Synth' prog 90 -> chords (2514 notes, median pitch C4)
  track 'Synthesizer' prog 90 -> chords (2737 notes, median pitch C4)
  track 'Synthesizer' prog 90 -> chords (1955 notes, median pitch Eb4)
  track 'Synthesizer' prog 90 -> bass (344 notes, median pitch G2)
  track 'Synthesizer' prog 90 -> bass (365 notes, median pitch G2)
  track 'Drum Kit' prog 0 -> drums (218 notes, median pitch C2)
  track '2023 Drum Kit' prog 0 -> drums (184 notes, median pitch F#1)
  track '909 Drum Kit' prog 0 -> drums (204 notes, median pitch C2)
  track '2023 Drum Kit' prog 0 -> drums (432 notes, median pitch F#2)
  track 'Synthesizer' prog 90 -> lead2 (896 notes, median pitch Bb4)
  track 'Synthesizer' prog 90 -> lead2 (896 notes, median pitch Bb4)
  track 'Scifi' prog 103 -> lead2 (293 notes, median pitch G5)
  track 'Flute (Classic)' prog 73 -> chords (411 notes, median pitch Ab3)
  track 'Strings' prog 48 -> chords (294 notes, median pitch E3)
  track 'Pizzicato' prog 45 -> chords (143 notes, median pitch E3)
  track 'Steel Drums' prog 114 -> chords (579 notes, median pitch Bb3)
  track 'Saxophone' prog 66 -> other (66 notes, median pitch C#3)
  track 'Violin (Classic)' prog 40 -> chords (163 notes, median pitch B3)
  track 'Cello (Classic)' prog 42 -> chords (212 notes, median pitch Bb3)
  track 'Church Organ (Classic)' prog 19 -> other (39 notes, median pitch Ab3)
  track 'Synth Pluck (Classic)' prog 84 -> lead2 (10 notes, median pitch Eb4)
  track '808 Drum Kit' prog 0 -> drums (18 notes, median pitch C4)
  track 'Vibraphone' prog 11 -> lead2 (8 notes, median pitch Bb3)
  track 'Electric Drum Kit' prog 118 -> drums (13 notes, median pitch Bb3)
  track 'Bass Guitar (Classic)' prog 33 -> bass (6 notes, median pitch F3)
  track '8-Bit Triangle' prog 80 -> other (4 notes, median pitch G3)
  track 'French Horn (Classic)' prog 60 -> lead2 (18 notes, median pitch Bb3)
```
       |       |       |       |       |       |       |       |       |       |  
drums  XXXX...........XXXXXXXXXXXXXXXXXXXXX....XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..XX
bass   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   ........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..........
lead2  XXXX....XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX...
other  XXXX.......................................................................
```
sections, 4-bar blocks (start bar : length : roles): 1:4:D+B+C+L2+O; 5:4:B+C; 9:8:B+C+L+L2; 17:20:D+B+C+L+L2; 37:4:B+C+L+L2; 41:24:D+B+C+L+L2; 65:8:D+B+C+L2; 73:3:D+B+C
- drums enter bar 1, drum dropouts (start bar,len): [(5, 11), (37, 4), (72, 2)], bars after drums stop: 0
- harmonic loop period: 4 bars; lead 4-bar phrases: `--NvvRRRRRRRRRRRN--` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): tamb `XXX.XXX.XXX.XXX.` vel 0.57±0.13; hat `XXX.XXX.XXX.XXX.` vel 0.57±0.13; kick `X...X...X...X...` vel 0.98±0.02; snare `....X.......X...` vel 1.0±0.0; 31 `....X.......X...` vel 1.0±0.0
- off-grid onsets 1%; swing8 0.509; swing16 0.25

#### OS #3989543 "Back and Forth ~Chillhop~ (Original Soundtrack) (In Progress)" (https://onlinesequencer.net/3989543)
tempo 110 BPM, 4/4, 69 bars, mean active roles/bar 3.0
  track 'Grand Piano' prog 0 -> other (192 notes, median pitch Ab3)
  track 'Grand Piano' prog 0 -> other (99 notes, median pitch F3)
  track 'Acoustic Gtr (Classic)' prog 25 -> lead (109 notes, median pitch C#5)
  track 'Concert Harp' prog 46 -> lead2 (39 notes, median pitch B4)
  track 'Concert Harp' prog 46 -> lead2 (55 notes, median pitch B3)
  track 'Concert Harp' prog 46 -> other (64 notes, median pitch F3)
  track 'Concert Harp' prog 46 -> lead2 (57 notes, median pitch Eb4)
  track 'Concert Harp' prog 46 -> lead2 (30 notes, median pitch B4)
  track '909 Drum Kit' prog 0 -> drums (318 notes, median pitch F#2)
  track 'Bass Guitar' prog 33 -> bass (46 notes, median pitch Bb1)
  track 'Violin' prog 40 -> lead2 (14 notes, median pitch C4)
```
      |       |       |       |       |       |       |       |       |    
drums ......XXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX................
bass  ...................XXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXX.............
lead  .............XXXXXXXXXXXX...............XXXXXXXXXXXX....XXXXXX.......
lead2 ........................................XXXXXXXXXXXXXXXXXXXXXXXX.XXX.
other XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:4:O; 5:8:D+O; 13:8:D+L+O; 21:4:D+B+L+O; 25:16:D+B+O; 41:12:D+B+L+L2+O; 53:4:B+L2+O; 57:8:L+L2+O; 65:4:L2+O; 69:1:O
- drums enter bar 7, drum dropouts (start bar,len): [(13, 1)], bars after drums stop: 16
- harmonic loop period: None bars; lead 4-bar phrases: `---NNNN---NNN-NN--` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `x.x.x.x.x.x.x.x.` vel 1.0±0.0; kick `.xx..xx..xx..x..` vel 1.0±0.0; snare `x...x...x...x...` vel 1.0±0.0
- off-grid onsets 4%; swing8 0.557; swing16 0.25

#### OS #4060604 "Lofi Testing" (https://onlinesequencer.net/4060604)
tempo 111 BPM, 4/4, 32 bars, mean active roles/bar 2.5
  track 'Electric Piano' prog 5 -> chords (195 notes, median pitch A4)
  track '808 Drum Kit' prog 0 -> drums (48 notes, median pitch B1)
  track '2023 Drum Kit' prog 0 -> drums (32 notes, median pitch F#1)
  track '909 Drum Kit' prog 0 -> drums (16 notes, median pitch D2)
  track '8-Bit Sine' prog 80 -> bass (16 notes, median pitch Bb1)
  track '8-Bit Triangle' prog 80 -> bass (16 notes, median pitch Bb1)
  track '2013 Drum Kit' prog 0 -> drums (32 notes, median pitch F#2)
  track 'EDM Kit (E)' prog 0 -> drums (128 notes, median pitch Bb6)
```
       |       |       |       |       
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   ........XXXXXXXXXXXXXXXX........
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:8:D+C; 9:16:D+B+C; 25:8:D+C
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 4 bars; lead 4-bar phrases: `n/a` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): 94 `X...X...X...X...` vel 0.4±0.0; kick2 `X.x.x.x...x.x.x.` vel 1.0±0.0; hat `X...X...X...X...` vel 1.0±0.0; 31 `........X.......` vel 1.0±0.0; 30 `........X.......` vel 1.0±0.0
- off-grid onsets 43%; swing8 0.5; swing16 None

#### OS #4143782 "Beautiful Lofi beat" (https://onlinesequencer.net/4143782)
tempo 60 BPM, 4/4, 32 bars, mean active roles/bar 2.75
  track 'Electric Piano' prog 5 -> chords (480 notes, median pitch Eb4)
  track 'Violin' prog 40 -> chords (64 notes, median pitch C6)
  track '8-Bit Triangle' prog 80 -> bass (28 notes, median pitch B1)
  track '2023 Drum Kit' prog 0 -> drums (84 notes, median pitch D4)
  track '808 Drum Kit' prog 0 -> drums (105 notes, median pitch C2)
```
       |       |       |       |       
drums  ....XXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   ....XXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:4:C; 5:28:D+B+C
- drums enter bar 5, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 4 bars; lead 4-bar phrases: `n/a` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): kick `X......XXxX.....` vel 1.0±0.0; 62 `....X..x.X..X...` vel 1.0±0.0
- off-grid onsets 4%; swing8 0.55; swing16 0.271

#### OS #4149873 "Random New EDM Drum kit Lofi Hip-Hop" (https://onlinesequencer.net/4149873)
tempo 175 BPM, 4/4, 57 bars, mean active roles/bar 2.72
  track 'EDM Kit (E)' prog 0 -> drums (27 notes, median pitch B5)
  track 'EDM Kit (E)' prog 0 -> drums (27 notes, median pitch Bb6)
  track 'Grand Piano' prog 0 -> chords (96 notes, median pitch E5)
  track 'Grand Piano' prog 0 -> chords (84 notes, median pitch C5)
  track 'EDM Kit (E)' prog 0 -> drums (79 notes, median pitch E1)
  track '909 Drum Kit' prog 0 -> drums (151 notes, median pitch F#2)
  track 'Electric Piano' prog 5 -> lead (104 notes, median pitch F#6)
```
       |       |       |       |       |       |       |       |
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XX
chords .XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.......
lead   ..XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.....
```
sections, 4-bar blocks (start bar : length : roles): 1:52:D+C+L; 53:5:D
- drums enter bar 1, drum dropouts (start bar,len): [(55, 1)], bars after drums stop: 0
- harmonic loop period: 4 bars; lead 4-bar phrases: `NvvRRRRRNNvRv--` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X...X...X...X...` vel 1.0±0.0; 27 `x...x...........` vel 1.0±0.0; tomL2 `........X.......` vel 1.0±0.0; 83 `X...............` vel 1.0±0.0; 94 `X...............` vel 1.0±0.0
- off-grid onsets 0%; swing8 0.639; swing16 None

#### OS #4155416 "Pokémon Lake Theme [ Lofi ]" (https://onlinesequencer.net/4155416)
tempo 67 BPM, 3/4, 70 bars, mean active roles/bar 4.36
  track 'Grand Piano' prog 0 -> lead2 (296 notes, median pitch G4)
  track 'Strings' prog 48 -> lead2 (164 notes, median pitch D4)
  track 'Electric Piano' prog 5 -> lead2 (296 notes, median pitch B4)
  track 'Synthesizer' prog 90 -> lead2 (66 notes, median pitch E5)
  track 'EDM Kit (E)' prog 0 -> drums (165 notes, median pitch Eb1)
  track 'Electric Drum Kit' prog 118 -> drums (403 notes, median pitch F#2)
  track '2023 Drum Kit' prog 0 -> drums (105 notes, median pitch F1)
  track 'Violin' prog 40 -> lead2 (66 notes, median pitch E5)
  track 'Bass' prog 32 -> bass (32 notes, median pitch G2)
  track 'Concert Harp' prog 46 -> lead2 (589 notes, median pitch Eb4)
  track 'EDM Kit (E)' prog 0 -> drums (34 notes, median pitch Bb6)
  track 'EDM Kit (E)' prog 0 -> drums (34 notes, median pitch B5)
  track 'EDM Kit (E)' prog 0 -> drums (34 notes, median pitch Bb6)
  track 'Synthesizer' prog 90 -> pad (32 notes, median pitch B6)
  track 'Synth Pluck (Classic)' prog 84 -> lead (595 notes, median pitch G4)
  track '8-Bit Square' prog 80 -> chords (284 notes, median pitch Ab3)
  track '8-Bit Square' prog 80 -> chords (410 notes, median pitch D3)
  track '8-Bit Square' prog 80 -> chords (144 notes, median pitch Ab3)
  track '8-Bit Square' prog 80 -> lead2 (24 notes, median pitch F#5)
  track '8-Bit Square' prog 80 -> other (30 notes, median pitch D3)
  track '8-Bit Square' prog 80 -> bass (11 notes, median pitch Bb2)
  track '8-Bit Square' prog 80 -> chords (21 notes, median pitch C5)
  track '8-Bit Square' prog 80 -> chords (405 notes, median pitch F3)
  track '8-Bit Square' prog 80 -> chords (559 notes, median pitch E4)
  track '8-Bit Square' prog 80 -> chords (202 notes, median pitch C5)
  track '8-Bit Square' prog 80 -> chords (27 notes, median pitch E2)
  track '8-Bit Square' prog 80 -> chords (51 notes, median pitch C#2)
  track '8-Bit Square' prog 80 -> chords (98 notes, median pitch Eb6)
```
       |       |       |       |       |       |       |       |       |     
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..X
bass   .X.X.XXXXX.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X......
chords ......XXXXX...........................................................
pad    .XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.....
lead   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.....
lead2  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX....
other  ......X.XX............................................................
```
sections, 4-bar blocks (start bar : length : roles): 1:4:D+B+P+L+L2; 5:4:D+B+C+P+L+L2; 9:4:D+B+C+P+L+L2+O; 13:52:D+B+P+L+L2; 65:4:D+L2; 69:2:D
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 3
- harmonic loop period: 2 bars; lead 4-bar phrases: `NNvRvRRRRRRRRRRRN-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X.X.X.X.X.X.` vel 1.0±0.0; 27 `X.........x.` vel 1.0±0.0; 29 `X.........x.` vel 1.0±0.0; 94 `X...........` vel 1.0±0.0; tomL2 `......X.....` vel 1.0±0.0
- off-grid onsets 33%; swing8 0.513; swing16 0.25

#### OS #4162279 "K.K. Cruisin' [ Lofi ]" (https://onlinesequencer.net/4162279)
tempo 165 BPM, 4/4, 93 bars, mean active roles/bar 3.9
  track 'Acoustic Guitar' prog 24 -> chords (863 notes, median pitch F#3)
  track 'Synthesizer' prog 90 -> lead (239 notes, median pitch F#5)
  track 'EDM Kit (E)' prog 0 -> drums (249 notes, median pitch Eb1)
  track 'Electric Drum Kit' prog 118 -> drums (551 notes, median pitch F#2)
  track '2023 Drum Kit' prog 0 -> drums (125 notes, median pitch F1)
  track 'Electric Drum Kit' prog 118 -> drums (340 notes, median pitch F#2)
  track '909 Drum Kit' prog 0 -> drums (125 notes, median pitch C2)
  track 'Bass Guitar' prog 33 -> bass (176 notes, median pitch B1)
  track 'EDM Kit (E)' prog 0 -> drums (48 notes, median pitch B5)
  track '8-Bit Square' prog 80 -> lead2 (50 notes, median pitch D4)
  track '8-Bit Square' prog 80 -> chords (126 notes, median pitch Bb4)
  track '8-Bit Square' prog 80 -> chords (116 notes, median pitch G5)
  track '8-Bit Square' prog 80 -> chords (229 notes, median pitch Eb5)
  track '8-Bit Square' prog 80 -> chords (190 notes, median pitch D5)
  track '8-Bit Square' prog 80 -> chords (220 notes, median pitch D5)
  track '8-Bit Square' prog 80 -> chords (163 notes, median pitch C3)
  track '8-Bit Square' prog 80 -> chords (147 notes, median pitch C5)
  track '8-Bit Square' prog 80 -> chords (134 notes, median pitch D3)
  track '8-Bit Square' prog 80 -> chords (172 notes, median pitch F#2)
  track '8-Bit Square' prog 80 -> chords (102 notes, median pitch Bb4)
  track '8-Bit Square' prog 80 -> chords (197 notes, median pitch Ab4)
  track '8-Bit Square' prog 80 -> chords (143 notes, median pitch C#2)
  track '8-Bit Square' prog 80 -> chords (107 notes, median pitch Eb5)
  track '8-Bit Square' prog 80 -> chords (198 notes, median pitch E3)
  track '8-Bit Square' prog 80 -> chords (134 notes, median pitch Ab2)
  track '8-Bit Square' prog 80 -> lead2 (73 notes, median pitch E4)
  track '8-Bit Square' prog 80 -> lead2 (61 notes, median pitch G4)
  track '8-Bit Square' prog 80 -> chords (91 notes, median pitch F#4)
  track '8-Bit Square' prog 80 -> chords (49 notes, median pitch B3)
  track '8-Bit Square' prog 80 -> chords (151 notes, median pitch C3)
  track '8-Bit Square' prog 80 -> chords (548 notes, median pitch F#2)
  track '8-Bit Square' prog 80 -> chords (266 notes, median pitch C#4)
  track '8-Bit Square' prog 80 -> chords (117 notes, median pitch Bb2)
  track '8-Bit Square' prog 80 -> chords (205 notes, median pitch F3)
  track '8-Bit Square' prog 80 -> chords (154 notes, median pitch B3)
  track '8-Bit Square' prog 80 -> lead2 (48 notes, median pitch A4)
  track '8-Bit Square' prog 80 -> chords (219 notes, median pitch F#3)
  track '8-Bit Square' prog 80 -> chords (145 notes, median pitch Bb3)
  track '8-Bit Square' prog 80 -> chords (129 notes, median pitch G3)
  track '8-Bit Square' prog 80 -> chords (200 notes, median pitch F3)
  track '8-Bit Square' prog 80 -> chords (166 notes, median pitch E3)
```
       |       |       |       |       |       |       |       |       |       |       |       |    
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   XX..XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXX.XXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXX.
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
lead   XXX.XXXXX.XXXXXXXXXXXXXXX.XXXXXXX.XXXXXXXXXXXXXXXXXXXXXXX.XXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXX.
lead2  XXXXXX.......................................................................................
```
sections, 4-bar blocks (start bar : length : roles): 1:8:D+B+C+L+L2; 9:84:D+B+C+L; 93:1:D
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `NNNvNRvRNNNRvRRRRvRNNRN-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X..XXX..X..XXX..` vel 1.0±0.0; 27 `X...x........x..` vel 1.0±0.0; 29 `X...x........x..` vel 1.0±0.0; kick `X...x........x..` vel 1.0±0.0; tomL2 `...x....X.......` vel 0.94±0.12
- off-grid onsets 16%; swing8 0.668; swing16 0.333

#### OS #4187977 "Mozart- Symphony no.40 (Lofi Beat Remix))" (https://onlinesequencer.net/4187977)
tempo 90 BPM, 2/4, 42 bars, mean active roles/bar 3.29
  track 'Smooth Synth (Classic)' prog 90 -> chords (1691 notes, median pitch Eb4)
  track 'Flute (Classic)' prog 73 -> chords (994 notes, median pitch E4)
  track 'Bass Guitar (Classic)' prog 33 -> bass (79 notes, median pitch G2)
  track 'Trombone (Classic)' prog 57 -> lead2 (9 notes, median pitch A3)
  track 'French Horn (Classic)' prog 60 -> lead (26 notes, median pitch Bb3)
  track '8-Bit Sine' prog 80 -> lead2 (13 notes, median pitch C#6)
  track '808 Drum Kit' prog 0 -> drums (428 notes, median pitch F#2)
  track '2013 Drum Kit' prog 0 -> drums (55 notes, median pitch C2)
```
       |       |       |       |       |       | 
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   XXXXXXXXXXXXXXXXXXXX.XXXXXX.......XXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   ...............XXXXX.............XXXXXXXX.
lead2  .............XXX.....................XXXX.
```
sections, 4-bar blocks (start bar : length : roles): 1:12:D+B+C; 13:4:D+B+C+L2; 17:4:D+B+C+L; 21:8:D+B+C; 29:4:D+C; 33:4:D+B+C+L; 37:6:D+B+C+L+L2
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `---NN---NNN` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X.X.XxXX` vel 1.0±0.0; kick2 `x...X.Xx` vel 1.0±0.0; kick `....x.xx` vel 1.0±0.0; snare `XxX.XXX.` vel 1.0±0.0; clap `X.......` vel 1.0±0.0
- off-grid onsets 2%; swing8 0.576; swing16 0.293

#### OS #4188103 "Snowdin Town [ Lofi ]" (https://onlinesequencer.net/4188103)
tempo 85 BPM, 4/4, 58 bars, mean active roles/bar 3.07
  track 'Grand Piano' prog 0 -> lead (732 notes, median pitch A5)
  track 'Grand Piano' prog 0 -> lead2 (4 notes, median pitch B6)
  track 'EDM Kit (E)' prog 0 -> drums (5 notes, median pitch B5)
  track 'Synthesizer' prog 90 -> chords (56 notes, median pitch G4)
  track 'EDM Kit (E)' prog 0 -> drums (192 notes, median pitch Eb1)
  track 'Electric Drum Kit' prog 118 -> drums (379 notes, median pitch F#2)
  track '909 Drum Kit' prog 0 -> drums (120 notes, median pitch C2)
  track 'Drum Kit' prog 0 -> drums (10 notes, median pitch Bb2)
  track 'EDM Kit (E)' prog 0 -> drums (50 notes, median pitch Bb6)
  track 'Bass Guitar' prog 33 -> bass (54 notes, median pitch Ab1)
```
       |       |       |       |       |       |       |       | 
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   ........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..
chords ........................XXXXXX..................XXXXXX....
lead   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..
lead2  .........................X...X...................X...X....
```
sections, 4-bar blocks (start bar : length : roles): 1:8:D+L; 9:16:D+B+L; 25:8:D+B+C+L; 33:16:D+B+L; 49:8:D+B+C+L; 57:2:D
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 4 bars; lead 4-bar phrases: `NvNvvvNNRRRRRR-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X.X.X.X.X.X.X.X.` vel 1.0±0.0; 27 `X.xx..Xx........` vel 1.0±0.0; kick `X.xx..Xx........` vel 1.0±0.0; tomL2 `......x.X.......` vel 0.94±0.12; 94 `X...............` vel 1.0±0.0
- off-grid onsets 4%; swing8 0.536; swing16 0.368

#### OS #4258654 "LoFi sans." (https://onlinesequencer.net/4258654)
tempo 120 BPM, 4/4, 32 bars, mean active roles/bar 4.41
  track 'Grand Piano' prog 0 -> other (128 notes, median pitch D3)
  track 'French Horn' prog 60 -> lead2 (254 notes, median pitch C4)
  track 'Drum Kit' prog 0 -> drums (140 notes, median pitch Ab5)
  track 'Electric Drum Kit' prog 118 -> drums (390 notes, median pitch Ab2)
  track 'Violin' prog 40 -> chords (78 notes, median pitch D5)
  track 'Ragtime Piano' prog 3 -> other (22 notes, median pitch F#3)
  track 'Trombone (Classic)' prog 57 -> lead2 (128 notes, median pitch D4)
  track 'Xylophone' prog 13 -> lead (329 notes, median pitch Bb4)
```
       |       |       |       |       
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXX....
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXX....
lead   XXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXX
lead2  XXXXXXXXXXXXXXXXXXXXXXXX.....XXX
other  XXXXXXXXXXXXXXXXXXXXXXXX.....XXX
```
sections, 4-bar blocks (start bar : length : roles): 1:24:D+C+L+L2+O; 25:4:D+C+L; 29:4:L+L2+O
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 4
- harmonic loop period: None bars; lead 4-bar phrases: `NvNRRRNN-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): pedalhat `..X...X...X...X.` vel 1.0±0.0; 80 `....X.X.....X.X.` vel 1.0±0.0; kick2 `X.....X.X.x...x.` vel 1.0±0.0; 81 `X.......X.......` vel 1.0±0.0; cowbell `....X.......X...` vel 1.0±0.0
- off-grid onsets 2%; swing8 0.5; swing16 None

#### OS #4280437 "watercrystals | [ Lofi ] Remix" (https://onlinesequencer.net/4280437)
tempo 150 BPM, 4/4, 81 bars, mean active roles/bar 2.79
  track 'Grand Piano' prog 0 -> lead (711 notes, median pitch C5)
  track 'Bass' prog 32 -> bass (41 notes, median pitch F#1)
  track '909 Drum Kit' prog 0 -> drums (223 notes, median pitch F#2)
  track 'EDM Kit (E)' prog 0 -> drums (68 notes, median pitch Eb1)
  track 'EDM Kit (E)' prog 0 -> drums (63 notes, median pitch G2)
  track '2023 Drum Kit' prog 0 -> drums (13 notes, median pitch G2)
  track 'Acoustic Guitar' prog 24 -> lead2 (11 notes, median pitch G6)
  track 'EDM Kit (E)' prog 0 -> drums (5 notes, median pitch F6)
  track 'Jazz Guitar' prog 26 -> lead2 (94 notes, median pitch Eb5)
  track 'Grand Piano' prog 0 -> lead2 (177 notes, median pitch C5)
  track 'EDM Kit (E)' prog 0 -> drums (4 notes, median pitch D6)
  track 'Flute (Classic)' prog 73 -> lead2 (160 notes, median pitch F#5)
```
      |       |       |       |       |       |       |       |       |       |       |
drums ........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.................
bass  ........XXXXXXXX..XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.................
lead  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.................
lead2 ................XXX.............XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:8:L; 9:8:D+B+L; 17:4:D+B+L+L2; 21:12:D+B+L; 33:32:D+B+L+L2; 65:17:L2
- drums enter bar 9, drum dropouts (start bar,len): [], bars after drums stop: 17
- harmonic loop period: 4 bars; lead 4-bar phrases: `NvRvRRRRRRRRRRRR-----` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X...X...X...X...` vel 0.97±0.14; tomL2 `........X.......` vel 0.94±0.2; 27 `x...x.x.........` vel 1.0±0.0; 89 `X...x...x...x...` vel 0.47±0.37; 87 `X...............` vel 1.0±0.0
- off-grid onsets 49%; swing8 0.5; swing16 None

#### OS #4367962 "꒒ꂦꌗ꓄ꃅꍟꋪꍟ - [ LoFi ]" (https://onlinesequencer.net/4367962)
tempo 130 BPM, 4/4, 52 bars, mean active roles/bar 3.31
  track 'Grand Piano' prog 0 -> lead2 (288 notes, median pitch Bb4)
  track 'Grand Piano' prog 0 -> lead (725 notes, median pitch C6)
  track '909 Drum Kit' prog 0 -> drums (256 notes, median pitch F#2)
  track 'EDM Kit (E)' prog 0 -> drums (45 notes, median pitch G2)
  track 'EDM Kit (E)' prog 0 -> drums (48 notes, median pitch Eb1)
  track 'Bass Guitar' prog 33 -> bass (40 notes, median pitch F2)
  track '2023 Drum Kit' prog 0 -> drums (48 notes, median pitch E1)
  track 'Electric Drum Kit' prog 118 -> drums (225 notes, median pitch F#2)
```
      |       |       |       |       |       |       |   
drums ........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.X
bass  ........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX............
lead  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX...
lead2 XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX....
```
sections, 4-bar blocks (start bar : length : roles): 1:8:L+L2; 9:32:D+B+L+L2; 41:8:D+L+L2; 49:4:D
- drums enter bar 9, drum dropouts (start bar,len): [], bars after drums stop: 2
- harmonic loop period: 4 bars; lead 4-bar phrases: `NvRRRRRRRRRRN-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X.X.X.X.X.X.X.X.` vel 0.82±0.24; 27 `X...x.x.......x.` vel 1.0±0.0; 28 `X...x.x.......x.` vel 1.0±0.0; tomL2 `...x....X.......` vel 0.92±0.16; 94 `X...............` vel 1.0±0.0
- off-grid onsets 23%; swing8 0.507; swing16 None

#### OS #4433303 "bird strike eric lofi beat" (https://onlinesequencer.net/4433303)
tempo 290 BPM, 4/4, 34 bars, mean active roles/bar 2.18
  track 'Grand Piano' prog 0 -> chords (231 notes, median pitch C5)
  track 'Synthesizer' prog 90 -> pad (17 notes, median pitch C5)
  track 'Synthesizer' prog 90 -> chords (5 notes, median pitch A4)
  track '909 Drum Kit' prog 0 -> drums (16 notes, median pitch C2)
  track 'EDM Kit (E)' prog 0 -> drums (8 notes, median pitch Eb2)
  track '808 Drum Kit' prog 0 -> drums (56 notes, median pitch F#2)
  track '8-Bit Triangle' prog 80 -> bass (66 notes, median pitch Bb2)
  track 'EDM Kit (E)' prog 0 -> drums (32 notes, median pitch F6)
```
       |       |       |       |       | 
drums  ................XXXXXXXXXXXXXXXXXX
bass   ................XXXXXXXXXXXXXXXX..
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
pad    .........XXXXXX...................
```
sections, 4-bar blocks (start bar : length : roles): 1:8:C; 9:8:C+P; 17:16:D+B+C; 33:2:D+C
- drums enter bar 17, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `n/a` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `x...X...XxxxX...` vel 0.82±0.32; 89 `....X.......X...` vel 1.0±0.0; kick `x...x...x...x...` vel 1.0±0.0; clap `X...............` vel 1.0±0.0
- off-grid onsets 0%; swing8 0.625; swing16 None

#### OS #4474455 "unfinished Lofi" (https://onlinesequencer.net/4474455)
tempo 110 BPM, 4/4, 61 bars, mean active roles/bar 2.2
  track 'EDM Kit (E)' prog 0 -> drums (230 notes, median pitch F#2)
  track 'Jazz Guitar' prog 26 -> lead (190 notes, median pitch Ab4)
  track 'Grand Piano' prog 0 -> chords (504 notes, median pitch E5)
```
       |       |       |       |       |       |       |       |    
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX........XXXXXXXXXXXX....
chords ............XXXXXXXXXXXX....XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   ....XXXXXXXXXXXXXXXXXXXXXXXX.............XXXXXXXXXXXXXXXX....
```
sections, 4-bar blocks (start bar : length : roles): 1:4:D; 5:8:D+L; 13:12:D+C+L; 25:4:D+L; 29:8:D+C; 37:4:C; 41:4:C+L; 45:12:D+C+L; 57:5:C
- drums enter bar 1, drum dropouts (start bar,len): [(38, 8)], bars after drums stop: 4
- harmonic loop period: None bars; lead 4-bar phrases: `-NRRRRR---NvRRN-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X.....x.........` vel 1.0±0.0; 31 `X.....x.........` vel 1.0±0.0; tomL2 `........X.......` vel 1.0±0.0; pedalhat `..........X.X.X.` vel 1.0±0.0
- off-grid onsets 8%; swing8 0.5; swing16 None

#### OS #4479268 "Lady brown - Nujabes (Loop)" (https://onlinesequencer.net/4479268)
tempo 194 BPM, 4/4, 40 bars, mean active roles/bar 2.8
  track 'Acoustic Guitar' prog 24 -> lead (258 notes, median pitch G4)
  track 'Grand Piano' prog 0 -> chords (246 notes, median pitch G4)
  track '2023 Drum Kit' prog 0 -> drums (292 notes, median pitch E2)
  track 'Electric Piano' prog 5 -> chords (96 notes, median pitch G3)
```
       |       |       |       |       |       
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords ........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:8:D+L; 9:32:D+C+L
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 16 bars; lead 4-bar phrases: `NvNNRNRRRR-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X...X.......X...` vel 1.0±0.0; clap `..x.x...X.....x.` vel 1.0±0.0; 32 `........X.......` vel 1.0±0.0; snare2 `X...x...........` vel 1.0±0.0; ride `X...............` vel 1.0±0.0
- off-grid onsets 0%; swing8 0.5; swing16 None

#### OS #4496747 "feather - nujabes" (https://onlinesequencer.net/4496747)
tempo 90 BPM, 4/4, 65 bars, mean active roles/bar 3.03
  track 'Grand Piano' prog 0 -> lead (974 notes, median pitch C#5)
  track 'Bass Guitar' prog 33 -> bass (472 notes, median pitch Eb3)
  track 'Drum Kit' prog 0 -> drums (3033 notes, median pitch C#2)
  track 'Trombone' prog 57 -> other (16 notes, median pitch C#3)
```
      |       |       |       |       |       |       |       |       |
drums XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
bass  ....XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
lead  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
other ........................................XXXXXXXX.................
```
sections, 4-bar blocks (start bar : length : roles): 1:4:D+L; 5:36:D+B+L; 41:8:D+B+L+O; 49:16:D+B+L; 65:1:L
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 1
- harmonic loop period: 2 bars; lead 4-bar phrases: `NvRRRRRRRvRRRRRRN` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): kick `X.X..X.XX.XX.XX.` vel 1.0±0.0; hat `.X.X.X.X.X.X.X.X` vel 1.0±0.0; 64 `.X...X...XX..XX.` vel 1.0±0.0; kick2 `.X...X...XX..XX.` vel 1.0±0.0; 62 `...X...X...X...X` vel 1.0±0.0
- off-grid onsets 2%; swing8 0.667; swing16 0.25

#### OS #4532025 "You Say Run - MHA LoFi" (https://onlinesequencer.net/4532025)
tempo 128 BPM, 4/4, 160 bars, mean active roles/bar 3.85
  track '8-Bit Square' prog 80 -> chords (3747 notes, median pitch C4)
  track '8-Bit Square' prog 80 -> chords (539 notes, median pitch C#3)
  track '8-Bit Square' prog 80 -> other (34 notes, median pitch F#3)
  track '8-Bit Square' prog 80 -> chords (62 notes, median pitch E2)
  track '8-Bit Square' prog 80 -> chords (122 notes, median pitch C#3)
  track '8-Bit Square' prog 80 -> lead2 (54 notes, median pitch Bb3)
  track '8-Bit Square' prog 80 -> chords (58 notes, median pitch G3)
  track '8-Bit Square' prog 80 -> chords (169 notes, median pitch F#3)
  track '8-Bit Square' prog 80 -> chords (90 notes, median pitch B3)
  track '8-Bit Square' prog 80 -> other (32 notes, median pitch F2)
  track '8-Bit Square' prog 80 -> other (27 notes, median pitch G3)
  track '8-Bit Square' prog 80 -> lead2 (30 notes, median pitch A3)
  track '8-Bit Square' prog 80 -> lead2 (10 notes, median pitch B3)
  track '8-Bit Square' prog 80 -> other (41 notes, median pitch F3)
  track '8-Bit Square' prog 80 -> chords (14 notes, median pitch E2)
  track '8-Bit Square' prog 80 -> other (21 notes, median pitch F#3)
  track 'Cello (Classic)' prog 42 -> chords (500 notes, median pitch G2)
  track 'Electric Piano' prog 5 -> bass (937 notes, median pitch E2)
  track 'Grand Piano' prog 0 -> chords (349 notes, median pitch A4)
  track '2013 Drum Kit' prog 0 -> drums (1676 notes, median pitch F2)
  track 'Strings' prog 48 -> chords (150 notes, median pitch F#4)
  track 'Violin (Classic)' prog 40 -> lead (754 notes, median pitch D5)
  track 'Violin' prog 40 -> lead2 (639 notes, median pitch E5)
  track 'Cello' prog 42 -> chords (525 notes, median pitch G2)
  track 'Distortion Guitar' prog 30 -> chords (717 notes, median pitch D2)
  track 'Concert Harp' prog 46 -> lead2 (81 notes, median pitch A4)
  track 'Acoustic Gtr (Classic)' prog 25 -> lead2 (81 notes, median pitch A4)
  track '8-Bit Sawtooth' prog 81 -> chords (86 notes, median pitch B3)
  track 'Vibraphone' prog 11 -> lead2 (16 notes, median pitch F#5)
  track 'Scifi' prog 103 -> lead2 (15 notes, median pitch A4)
  track '909 Drum Kit' prog 0 -> drums (128 notes, median pitch B3)
  track 'Electric Guitar' prog 27 -> chords (717 notes, median pitch D2)
  track 'Trombone' prog 57 -> chords (349 notes, median pitch E4)
  track 'Lucent Choir' prog 52 -> chords (162 notes, median pitch E3)
```
       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       
drums  ..............XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..............XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   ..................................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX...................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords XXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX...XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   .................XXXXXXXXXXXXXXXX.XXXXXXX.XXXXXXXX................XXXXXXXXXXXXXXXXXXXXX...X................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead2  XXXXXX............................XXXXXXX.XXXXXXXX................XXXXXXXXXXXXXXXXXXXXX....XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
other  XXXXX...........................................................................................................................................................
```
sections, 4-bar blocks (start bar : length : roles): 1:4:C+L2+O; 5:4:C+L2; 9:4:C; 13:4:D+C; 17:16:D+C+L; 33:20:D+B+C+L+L2; 53:12:D+B+C; 65:24:D+B+C+L+L2; 89:4:D; 93:16:C+L2; 109:52:D+B+C+L+L2
- drums enter bar 15, drum dropouts (start bar,len): [(94, 14)], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `----NvNNNNNNN---NNNNNNN---NNNNNNNNNNNvNN-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): kick2 `X.x.....X.X...X.` vel 1.0±0.0; 52 `X...X...X...X...` vel 1.0±0.0; snare2 `....X.......X...` vel 1.0±0.0; 59 `X.x.X.x.X.x.X.x.` vel 1.0±0.0; hat `x...x...x...X...` vel 1.0±0.0
- off-grid onsets 1%; swing8 0.526; swing16 0.27

#### OS #4817025 "Reflection Eternal - Nujabes" (https://onlinesequencer.net/4817025)
tempo 94 BPM, 4/4, 101 bars, mean active roles/bar 3.15
  track 'Grand Piano' prog 0 -> lead (710 notes, median pitch B3)
  track 'Flute (Classic)' prog 73 -> lead2 (151 notes, median pitch D4)
  track '808 Drum Kit' prog 0 -> drums (192 notes, median pitch Eb2)
  track '2023 Drum Kit' prog 0 -> drums (774 notes, median pitch G1)
  track 'Bass Guitar' prog 33 -> bass (112 notes, median pitch B1)
  track 'Grand Piano' prog 0 -> lead2 (136 notes, median pitch A3)
  track 'Flute (Classic)' prog 73 -> lead2 (43 notes, median pitch B4)
```
      |       |       |       |       |       |       |       |       |       |       |       |       |    
drums ....XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
bass  ........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.............
lead  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.....
lead2 ....XXXX................XXXXXXXXXXX.XXX.XXXXXXXXXXXXXXXX........XXX.XXX.XXXXXXXXXXXXXXXXXXX.XXX......
```
sections, 4-bar blocks (start bar : length : roles): 1:4:L; 5:4:D+L+L2; 9:16:D+B+L; 25:16:D+B+L+L2; 41:16:D+B+L2; 57:8:D+B+L; 65:24:D+B+L+L2; 89:8:D+L+L2; 97:4:D; 101:1:silence
- drums enter bar 5, drum dropouts (start bar,len): [], bars after drums stop: 1
- harmonic loop period: 4 bars; lead 4-bar phrases: `NRRRRRRvNv----RRRRRvRRRv--` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): pedalhat `..X...X...X...X.` vel 1.0±0.0; 27 `X.x.....X.X.....` vel 1.0±0.0; clap `....X.......X...` vel 1.0±0.0; 31 `....X.......X...` vel 1.0±0.0
- off-grid onsets 7%; swing8 0.5; swing16 0.325

#### OS #4823188 "Luv(sic) Part 4 - Nujabes & Shing02" (https://onlinesequencer.net/4823188)
tempo 93 BPM, 4/4, 32 bars, mean active roles/bar 2.56
  track 'Grand Piano' prog 0 -> other (206 notes, median pitch Ab3)
  track 'Bass Guitar' prog 33 -> bass (60 notes, median pitch F2)
  track 'EDM Kit (E)' prog 0 -> drums (105 notes, median pitch Eb1)
  track '2023 Drum Kit' prog 0 -> drums (233 notes, median pitch F#2)
  track '2013 Drum Kit' prog 0 -> drums (64 notes, median pitch D2)
  track '909 Drum Kit' prog 0 -> drums (256 notes, median pitch F#2)
  track 'Electric Piano' prog 5 -> lead (43 notes, median pitch Ab4)
```
      |       |       |       |       
drums XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass  ........XXXXXXXXXXXXXXXXXXXXXXXX
lead  ......XXXXXXXXXX................
other XXXXXXXXXXXXXXXX................
```
sections, 4-bar blocks (start bar : length : roles): 1:4:D+O; 5:4:D+L+O; 9:8:D+B+L+O; 17:16:D+B
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 8 bars; lead 4-bar phrases: `-NNN-----` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X.X.X.X.X.X.X.X.` vel 1.0±0.0; 27 `X.......X.X.....` vel 1.0±0.0; 32 `....X.......X...` vel 1.0±0.0; snare `....X.......X...` vel 1.0±0.0; 76 `.....X..........` vel 1.0±0.0
- off-grid onsets 7%; swing8 0.501; swing16 None

#### OS #4853823 "Title Theme - TLoZ: OoT [ Lofi ]" (https://onlinesequencer.net/4853823)
tempo 135 BPM, 4/4, 127 bars, mean active roles/bar 3.67
  track 'Grand Piano' prog 0 -> lead (488 notes, median pitch F4)
  track 'Flute (Classic)' prog 73 -> lead2 (187 notes, median pitch F#6)
  track 'Synthesizer' prog 90 -> lead2 (187 notes, median pitch B5)
  track '808 Drum Kit' prog 0 -> drums (123 notes, median pitch D2)
  track 'EDM Kit (E)' prog 0 -> drums (123 notes, median pitch G2)
  track 'Bass Guitar' prog 33 -> bass (40 notes, median pitch F2)
  track '909 Drum Kit' prog 0 -> drums (332 notes, median pitch F#2)
  track 'Violin' prog 40 -> lead2 (96 notes, median pitch B4)
  track 'EDM Kit (E)' prog 0 -> drums (32 notes, median pitch B5)
  track 'EDM Kit (E)' prog 0 -> drums (29 notes, median pitch Bb6)
  track '8-Bit Sine' prog 80 -> chords (6679 notes, median pitch B4)
  track '8-Bit Sine' prog 80 -> chords (6310 notes, median pitch C#5)
  track '8-Bit Sine' prog 80 -> chords (5798 notes, median pitch D5)
  track '8-Bit Sine' prog 80 -> chords (5716 notes, median pitch Eb5)
  track '909 Drum Kit' prog 0 -> drums (227 notes, median pitch B1)
  track '8-Bit Square' prog 80 -> chords (287 notes, median pitch F3)
  track '8-Bit Square' prog 80 -> chords (91 notes, median pitch Eb3)
  track '8-Bit Square' prog 80 -> chords (277 notes, median pitch A3)
  track '8-Bit Square' prog 80 -> other (51 notes, median pitch G3)
  track '8-Bit Square' prog 80 -> chords (100 notes, median pitch D3)
  track '8-Bit Square' prog 80 -> other (61 notes, median pitch E3)
  track '8-Bit Square' prog 80 -> other (66 notes, median pitch E3)
  track '8-Bit Square' prog 80 -> other (48 notes, median pitch E3)
  track '8-Bit Square' prog 80 -> chords (80 notes, median pitch E3)
  track '8-Bit Square' prog 80 -> other (44 notes, median pitch D3)
  track '8-Bit Square' prog 80 -> other (48 notes, median pitch D3)
```
       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |      
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   ............XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX...................
chords XXXXXXX....................................X................XXXXXXX.XXXXXX.....................................................
lead   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
lead2  ..XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX...............XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..XXXXXXXXXXXXXXXX.
other  XXXXXXX........................................................................................................................
```
sections, 4-bar blocks (start bar : length : roles): 1:8:D+C+L+L2+O; 9:4:D+L+L2; 13:48:D+B+L+L2; 61:16:D+C+L; 77:32:D+B+L+L2; 109:19:D+L+L2
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 1 bars; lead 4-bar phrases: `NvvNRRRvRRRNvvNvvRvRRRRRRRRvRvvN` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X...X...X...X...` vel 1.0±0.0; kick2 `X...x.........x.` vel 1.0±0.0; snare `........X.......` vel 1.0±0.0; tomL2 `........X.......` vel 1.0±0.0; 83 `X...............` vel 1.0±0.0
- off-grid onsets 78%; swing8 0.519; swing16 None

#### OS #4876947 "Nujabes - Lady Brown (Instrumental)" (https://onlinesequencer.net/4876947)
tempo 198 BPM, 4/4, 160 bars, mean active roles/bar 3.92
  track 'Acoustic Guitar' prog 24 -> lead (1152 notes, median pitch E4)
  track '2023 Drum Kit' prog 0 -> drums (843 notes, median pitch Eb1)
  track '909 Drum Kit' prog 0 -> drums (640 notes, median pitch F#2)
  track 'Electric Drum Kit' prog 118 -> drums (1280 notes, median pitch Eb4)
  track 'Bass Guitar' prog 33 -> bass (476 notes, median pitch A2)
  track 'Strings' prog 48 -> chords (644 notes, median pitch G4)
  track 'Scifi' prog 103 -> lead2 (56 notes, median pitch C#5)
  track 'Grand Piano' prog 0 -> lead2 (112 notes, median pitch A4)
  track 'Strings' prog 48 -> chords (644 notes, median pitch F#4)
```
       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords ................XXXXXXX.XXXXXXXXXXXXXXX.XXXXXXXX........XXXXXXX.XXXXXXXXXXXXXXX.XXXXXXXXXXXXXXX.XXXXXXXX........XXXXXXX.XXXXXXXXXXXXXXX.XXXXXXXX................
lead   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead2  ................X...XXXXX.......X...XXXXX...............X...XXXXX.......X...XXXXX.......X...XXXXX...............X...XXXXX.......X...XXXXX.......................
```
sections, 4-bar blocks (start bar : length : roles): 1:16:D+B+L; 17:4:D+B+C+L; 21:4:D+B+C+L+L2; 25:12:D+B+C+L; 37:4:D+B+C+L+L2; 41:8:D+B+C+L; 49:8:D+B+L; 57:4:D+B+C+L; 61:4:D+B+C+L+L2; 65:12:D+B+C+L; 77:4:D+B+C+L+L2; 81:12:D+B+C+L; 93:4:D+B+C+L+L2; 97:8:D+B+C+L; 105:8:D+B+L; 113:4:D+B+C+L; 117:4:D+B+C+L+L2; 121:12:D+B+C+L; 133:4:D+B+C+L+L2; 137:8:D+B+C+L; 145:16:D+B+L
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 16 bars; lead 4-bar phrases: `NRRRNNRNRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRR-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X...X...X...X...` vel 1.0±0.0; 82 `X...X...X...X...` vel 0.9±0.0; pedalhat `..X...X...X...X.` vel 1.0±0.0; 26 `x.x.X.........x.` vel 1.0±0.0; 27 `x.x.X.........x.` vel 0.7±0.0
- off-grid onsets 5%; swing8 0.52; swing16 None

#### OS #4886681 "Beware (Lofi)" (https://onlinesequencer.net/4886681)
tempo 112 BPM, 4/4, 36 bars, mean active roles/bar 4.44
  track 'Acoustic Gtr (Classic)' prog 25 -> chords (208 notes, median pitch C#4)
  track 'Jazz Guitar' prog 26 -> lead (384 notes, median pitch Bb3)
  track 'Rhodes' prog 2 -> other (116 notes, median pitch C3)
  track 'Slap Bass' prog 36 -> bass (476 notes, median pitch G2)
  track 'EDM Kit (E)' prog 0 -> drums (168 notes, median pitch F1)
```
       |       |       |       |       |   
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords ..........XXXXXXXX..........XXXXXXXX
lead   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
other  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:8:D+B+L+O; 9:12:D+B+C+L+O; 21:8:D+B+L+O; 29:8:D+B+C+L+O
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `NNNNNNNRR-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): 29 `X...X.x.....X...` vel 1.0±0.0; clap `........X.....X.` vel 1.0±0.0
- off-grid onsets 0%; swing8 0.5; swing16 None

#### OS #4909402 "Chill hop / Nujabes type beat" (https://onlinesequencer.net/4909402)
tempo 110 BPM, 4/4, 64 bars, mean active roles/bar 2.92
  track 'Grand Piano' prog 0 -> lead (448 notes, median pitch C4)
  track 'Bass Guitar' prog 33 -> bass (258 notes, median pitch Ab2)
  track '909 Drum Kit' prog 0 -> drums (452 notes, median pitch C2)
  track '2023 Drum Kit' prog 0 -> drums (1492 notes, median pitch F#2)
  track 'Vibraphone' prog 11 -> lead2 (5 notes, median pitch B4)
```
      |       |       |       |       |       |       |       |       
drums ...XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass  ....XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead2 ....XX..........................................................
```
sections, 4-bar blocks (start bar : length : roles): 1:4:L; 5:4:D+B+L+L2; 9:56:D+B+L
- drums enter bar 4, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `NNNNNNNNRRRRRRRR-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X.X.X.X.X.X.X...` vel 1.0±0.0; kick `X.X....X.XX.....` vel 1.0±0.0; 27 `X.X....X..X.....` vel 1.0±0.0; crash `X...X...X...X...` vel 1.0±0.0; snare `....X.......X...` vel 1.0±0.0
- off-grid onsets 1%; swing8 0.576; swing16 0.25

#### OS #4987958 "Another Lofi Beat Extended" (https://onlinesequencer.net/4987958)
tempo 130 BPM, 4/4, 80 bars, mean active roles/bar 3.77
  track 'Vibraphone' prog 11 -> lead (512 notes, median pitch Eb4)
  track 'Jazz Guitar' prog 26 -> chords (600 notes, median pitch Eb3)
  track 'Acoustic Gtr (Classic)' prog 25 -> lead2 (234 notes, median pitch Ab4)
  track '8-Bit Triangle' prog 80 -> lead2 (270 notes, median pitch Bb4)
  track '808 Drum Kit' prog 0 -> drums (518 notes, median pitch F#2)
```
       |       |       |       |       |       |       |       |       |       |       
drums  XXXXXXX.XXXXXXXXXXXXXXXXXXXXX.......XXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXX.......XXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead2  XXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:28:D+C+L+L2; 29:8:C+L+L2; 37:32:D+C+L+L2; 69:8:C+L+L2; 77:4:D+C+L+L2
- drums enter bar 1, drum dropouts (start bar,len): [(8, 1), (30, 7), (48, 1), (70, 7)], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `NNvvvvRvNNRRRRRRRRRR-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X.......X...x...` vel 1.0±0.0; kick `X...x.......x...` vel 1.0±0.0; snare `........X.......` vel 1.0±0.0; maraca `x.......x...x...` vel 1.0±0.0; 68 `X...........x...` vel 1.0±0.0
- off-grid onsets 12%; swing8 0.578; swing16 0.263

#### OS #5095199 "Dire, Dire Docks [ Lofi ]" (https://onlinesequencer.net/5095199)
tempo 110 BPM, 4/4, 105 bars, mean active roles/bar 5.63
  track 'Synthesizer' prog 90 -> pad (73 notes, median pitch G3)
  track 'Strings' prog 48 -> chords (124 notes, median pitch Bb3)
  track 'Electric Piano' prog 5 -> lead2 (719 notes, median pitch A3)
  track 'EDM Kit (E)' prog 0 -> drums (442 notes, median pitch Eb1)
  track 'Electric Drum Kit' prog 118 -> drums (920 notes, median pitch F#2)
  track '8-Bit Sine' prog 80 -> chords (1027 notes, median pitch C#4)
  track 'Drum Kit' prog 0 -> drums (121 notes, median pitch C#3)
  track 'EDM Kit (E)' prog 0 -> drums (6 notes, median pitch Bb6)
  track 'Distortion Guitar' prog 30 -> bass (96 notes, median pitch C#2)
  track 'EDM Kit (E)' prog 0 -> drums (46 notes, median pitch Bb6)
  track 'Grand Piano' prog 0 -> lead2 (719 notes, median pitch E5)
  track 'Concert Harp' prog 46 -> lead (721 notes, median pitch C#4)
  track 'Vibraphone' prog 11 -> chords (4268 notes, median pitch F4)
  track '8-Bit Sine' prog 80 -> other (719 notes, median pitch Ab3)
  track 'Bass Guitar' prog 33 -> bass (92 notes, median pitch F#1)
  track 'Strings' prog 48 -> chords (124 notes, median pitch Bb5)
```
       |       |       |       |       |       |       |       |       |       |       |       |       |       |
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
bass   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
chords ...........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
pad    XXXXXXXXXXXX.............................................................................................
lead   ............XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead2  ............XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
other  ............XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
```
sections, 4-bar blocks (start bar : length : roles): 1:12:D+B+P; 13:92:D+B+C+L+L2+O; 105:1:C+L
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 1
- harmonic loop period: None bars; lead 4-bar phrases: `---NNNNNvvvNNNvNvvNNNvvNNNN` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X.X.X.XxX.X.X.X.` vel 1.0±0.0; 27 `X..x..X...x...X.` vel 0.5±0.0; crash `XXXXXXXXXXXXXXXX` vel 0.36±0.25; tomL2 `........X.......` vel 1.0±0.0; 94 `X...............` vel 1.0±0.0
- off-grid onsets 12%; swing8 0.543; swing16 0.281

#### OS #5108773 "just a lofi kind of guy (EXTEND)" (https://onlinesequencer.net/5108773)
tempo 151 BPM, 4/4, 72 bars, mean active roles/bar 3.72
  track '808 Drum Kit' prog 0 -> drums (344 notes, median pitch F#2)
  track '808 Drum Kit' prog 0 -> drums (24 notes, median pitch Bb2)
  track '808 Drum Kit' prog 0 -> drums (127 notes, median pitch B1)
  track '909 Drum Kit' prog 0 -> drums (136 notes, median pitch C#2)
  track 'EDM Kit (E)' prog 0 -> drums (132 notes, median pitch G2)
  track 'Synthesizer' prog 90 -> chords (354 notes, median pitch D4)
  track 'Bass Guitar' prog 33 -> bass (137 notes, median pitch F#2)
  track 'Rhodes' prog 2 -> chords (354 notes, median pitch C#4)
  track 'Clean Guitar' prog 27 -> lead (198 notes, median pitch F#4)
  track 'Electric Drum Kit' prog 118 -> drums (219 notes, median pitch Bb4)
  track '2023 Drum Kit' prog 0 -> drums (52 notes, median pitch Ab3)
  track 'Rock Kit' prog -1 -> drums (548 notes, median pitch A3)
  track '8-Bit Sine' prog 80 -> lead2 (41 notes, median pitch A4)
```
       |       |       |       |       |       |       |       |       |       
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   .........XXXXXXXXXXXXXXXXX...........................XXXXXXXXXXXXXXXXX..
lead2  ..........................XXXXXXXXXXXXXXXXXX............................
```
sections, 4-bar blocks (start bar : length : roles): 1:8:D+B+C; 9:16:D+B+C+L; 25:4:D+B+C+L+L2; 29:16:D+B+C+L2; 45:8:D+B+C; 53:20:D+B+C+L
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 4 bars; lead 4-bar phrases: `--NNvRN------RRRRR-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): crash2 `XX.XXXXXXX.XXXXX` vel 0.41±0.18; hat `X...X...X...X...` vel 1.0±0.0; kick2 `X...x..........x` vel 1.0±0.0; clap `........X.......` vel 1.0±0.0; stick `....X.......x...` vel 1.0±0.0
- off-grid onsets 20%; swing8 0.648; swing16 0.296

#### OS #5172915 "stranger things lofi" (https://onlinesequencer.net/5172915)
tempo 150 BPM, 4/4, 34 bars, mean active roles/bar 2.21
  track '8-Bit Triangle' prog 80 -> chords (41 notes, median pitch A2)
  track 'Scifi' prog 103 -> lead (274 notes, median pitch G4)
  track '909 Drum Kit' prog 0 -> drums (67 notes, median pitch B1)
```
       |       |       |       |       | 
drums  ................XXXXXXXXXXXXXXXXXX
chords XXX.XXX.XXX.XXX.XXX.XXX.XXX.XXX...
lead   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
```
sections, 4-bar blocks (start bar : length : roles): 1:16:C+L; 17:16:D+C+L; 33:2:D+L
- drums enter bar 17, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 8 bars; lead 4-bar phrases: `NRRRvvvRN` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): kick2 `X.X.....X.X.....` vel 1.0±0.0; kick `X.X.....X.X.....` vel 1.0±0.0
- off-grid onsets 0%; swing8 0.5; swing16 None

#### OS #5175695 "Red Bread Team Fortress 2 (lofi remix attempt)" (https://onlinesequencer.net/5175695)
tempo 110 BPM, 4/4, 48 bars, mean active roles/bar 2.73
  track 'Rhodes' prog 2 -> chords (1388 notes, median pitch Eb3)
  track 'Smooth Synth' prog 90 -> chords (830 notes, median pitch C4)
  track 'Drum Kit' prog 0 -> drums (1384 notes, median pitch F#2)
  track '8-Bit Sine' prog 80 -> chords (1084 notes, median pitch D3)
  track 'Electric Drum Kit' prog 118 -> drums (1384 notes, median pitch F#2)
  track 'Bass Guitar (Classic)' prog 33 -> bass (396 notes, median pitch C2)
  track 'Scifi' prog 103 -> chords (1084 notes, median pitch D3)
  track '808 Drum Kit' prog 0 -> drums (1384 notes, median pitch Ab3)
```
       |       |       |       |       |       |       
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   ...XX.X.XXXXXXXXXXX.X.XXXXXXXXXXX.X.XXXXXXXXXXXX
chords XXXXX.X.XXXXXXXXXXXXXXXXXXXXXXXXX.X.XXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:4:D+C; 5:44:D+B+C
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `n/a` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): 60 `X.XxX.X.XXxXXxxX` vel 0.78±0.16; hat `XXxX.XxXXXxX.XxX` vel 0.78±0.18; 61 `x....X.X..xx.X..` vel 0.77±0.15; cowbell `XX.X.X.XxXxX.X.X` vel 0.79±0.17; snare2 `X.X.xX.xX....X..` vel 0.71±0.21
- off-grid onsets 77%; swing8 0.603; swing16 0.255

#### OS #5178879 "Wet hands- Minecraft soundtrack (lofi breakcore remix)" (https://onlinesequencer.net/5178879)
tempo 115 BPM, 2/4, 84 bars, mean active roles/bar 4.4
  track 'Grand Piano' prog 0 -> lead (225 notes, median pitch A3)
  track 'Smooth Synth (Classic)' prog 90 -> chords (1350 notes, median pitch D2)
  track 'Rhodes' prog 2 -> lead2 (36 notes, median pitch Ab4)
  track '8-Bit Triangle' prog 80 -> bass (32 notes, median pitch B1)
  track '2013 Drum Kit' prog 0 -> drums (308 notes, median pitch B1)
  track '808 Drum Kit' prog 0 -> drums (656 notes, median pitch F#2)
  track 'Electric Drum Kit' prog 118 -> drums (48 notes, median pitch C#3)
  track '8-Bit Sine' prog 80 -> lead2 (208 notes, median pitch F#4)
  track 'Synth Pluck (Classic)' prog 84 -> lead2 (18 notes, median pitch Ab4)
  track '8-Bit Sawtooth' prog 81 -> lead2 (104 notes, median pitch F#5)
  track 'Drum Kit' prog 0 -> drums (84 notes, median pitch F#1)
  track '8-Bit Drum Kit' prog 0 -> drums (24 notes, median pitch F#2)
```
       |       |       |       |       |       |       |       |       |       |       |   
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   XXXXX.XXXXX.XXXXX.XXXXX.XXXXX.XXXXX.XXXXX.XXXXX.XXXXX.XXXXX.XXXXX.XXXXX.XXXXX.XXXXX.
lead2  ............XXXXXXXXXXXXXXXXXXXXXXXX............XXXXXXXXXXXXXXXXXXXXXXXX............
```
sections, 4-bar blocks (start bar : length : roles): 1:12:D+B+C+L; 13:24:D+B+C+L+L2; 37:12:D+B+C+L; 49:24:D+B+C+L+L2; 73:12:D+B+C+L
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `NNNRRRRRRRRRRRRRRRRRR-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `XXXxXXXX` vel 1.0±0.0; kick2 `X...X...` vel 1.0±0.0; snare2 `.x.x.x.x` vel 1.0±0.0; 30 `X.X.x.X.` vel 1.0±0.0; 28 `xx......` vel 1.0±0.0
- off-grid onsets 18%; swing8 0.622; swing16 0.315

#### OS #5197533 "In The Snowy Willows (Lofi Girl _ Vlad)" (https://onlinesequencer.net/5197533)
tempo 91 BPM, 4/4, 43 bars, mean active roles/bar 3.53
  track 'Synthesizer' prog 90 -> bass (38 notes, median pitch G1)
  track '2023 Drum Kit' prog 0 -> drums (52 notes, median pitch E1)
  track '808 Drum Kit' prog 0 -> drums (36 notes, median pitch D2)
  track 'Electric Drum Kit' prog 118 -> drums (36 notes, median pitch D2)
  track 'Flute (Classic)' prog 73 -> lead2 (60 notes, median pitch G5)
  track 'Scifi' prog 103 -> lead2 (60 notes, median pitch G5)
  track '909 Drum Kit' prog 0 -> drums (52 notes, median pitch C2)
  track 'Bass' prog 32 -> bass (16 notes, median pitch F#2)
  track 'Synthesizer' prog 90 -> pad (5 notes, median pitch D5)
  track 'Strings' prog 48 -> chords (69 notes, median pitch G3)
  track 'Drum Kit' prog 0 -> drums (125 notes, median pitch G1)
  track 'Synthesizer' prog 90 -> other (8 notes, median pitch Ab3)
  track '8-Bit Triangle' prog 80 -> chords (97 notes, median pitch Eb4)
  track '8-Bit Drum Kit' prog 0 -> drums (64 notes, median pitch F#2)
  track 'Harpsichord' prog 6 -> lead (256 notes, median pitch A4)
  track 'Harpsichord' prog 6 -> lead2 (5 notes, median pitch D5)
  track '2023 Drum Kit' prog 0 -> drums (32 notes, median pitch D1)
  track 'Synthesizer' prog 90 -> chords (62 notes, median pitch E4)
  track 'Synthesizer' prog 90 -> chords (200 notes, median pitch C4)
  track 'Synthesizer' prog 90 -> chords (136 notes, median pitch C#4)
  track 'Bass' prog 32 -> bass (186 notes, median pitch Ab2)
  track '909 Drum Kit' prog 0 -> drums (18 notes, median pitch D2)
  track 'Synthesizer' prog 90 -> chords (92 notes, median pitch Bb3)
  track 'Synthesizer' prog 90 -> lead2 (255 notes, median pitch D4)
  track 'Synthesizer' prog 90 -> chords (166 notes, median pitch G3)
  track 'French Horn (Classic)' prog 60 -> bass (408 notes, median pitch F2)
  track 'Synthesizer' prog 90 -> chords (194 notes, median pitch G3)
```
       |       |       |       |       |       |  
drums  ........X.XXXXXXXXXXXXXXXX.................
bass   XXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords XXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
pad    ..................X........................
lead   ..................XXXXXXXX.................
lead2  XXXXXXXXX.XXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXX
other  ........X..................................
```
sections, 4-bar blocks (start bar : length : roles): 1:8:B+C+L2; 9:8:D+B+C+L2; 17:12:D+B+C+L+L2; 29:15:B+C+L2
- drums enter bar 11, drum dropouts (start bar,len): [], bars after drums stop: 17
- harmonic loop period: 4 bars; lead 4-bar phrases: `----NNN----` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): 31 `X.X.X.X.X.X.X.X.` vel 1.0±0.0; snare `....X.....X.X..X` vel 1.0±0.0; hat `XXXXXXXXXXXXXXXX` vel 1.0±0.0; 28 `X.......X.xx.xx.` vel 1.0±0.0; kick `X.......X.xx.xx.` vel 1.0±0.0
- off-grid onsets 10%; swing8 0.567; swing16 0.281

#### OS #5197567 "Cloudspire Lofi/Jazz REMIX (February Monthly Contest)" (https://onlinesequencer.net/5197567)
tempo 160 BPM, 4/4, 64 bars, mean active roles/bar 4.25
  track '909 Drum Kit' prog 0 -> drums (96 notes, median pitch B1)
  track 'EDM Kit (E)' prog 0 -> drums (64 notes, median pitch G2)
  track 'Rhodes' prog 2 -> lead2 (128 notes, median pitch Eb4)
  track '808 Drum Kit' prog 0 -> drums (480 notes, median pitch F#2)
  track 'Synthesizer' prog 90 -> lead2 (158 notes, median pitch G4)
  track 'Bass Guitar' prog 33 -> bass (188 notes, median pitch C2)
  track 'Grand Piano' prog 0 -> lead2 (158 notes, median pitch G5)
  track 'Vibraphone' prog 11 -> lead (258 notes, median pitch G4)
  track 'Brass' prog 61 -> lead2 (50 notes, median pitch A5)
  track 'Trombone (Classic)' prog 57 -> chords (160 notes, median pitch Eb4)
```
       |       |       |       |       |       |       |       |       
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords ................................................XXXXXXXXXXXXXXXX
lead   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead2  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:48:D+B+L+L2; 49:16:D+B+C+L+L2
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 4 bars; lead 4-bar phrases: `NNvRNRRRRRRRRRRR-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `x.XXX.X.X...X.X.` vel 1.0±0.0; kick2 `x...x.........x.` vel 1.0±0.0; tomL2 `........X.......` vel 1.0±0.0
- off-grid onsets 4%; swing8 0.556; swing16 None

#### OS #5208345 "How To Train Your Dragon. Forbidden Friendship.  (lofi remix V2)" (https://onlinesequencer.net/5208345)
tempo 62 BPM, 4/4, 63 bars, mean active roles/bar 4.05
  track 'Electric Drum Kit' prog 118 -> drums (633 notes, median pitch A4)
  track 'Scifi' prog 103 -> lead (626 notes, median pitch Ab4)
  track 'Slap Bass' prog 36 -> bass (351 notes, median pitch Bb3)
  track 'Music Box' prog 10 -> chords (232 notes, median pitch Bb4)
  track 'Synth Bass (Classic)' prog 38 -> bass (963 notes, median pitch Bb2)
  track '8-Bit Sine' prog 80 -> chords (384 notes, median pitch A3)
  track 'Synthesizer' prog 90 -> chords (1862 notes, median pitch Bb4)
  track 'Rhodes' prog 2 -> chords (1946 notes, median pitch C5)
  track 'Drum Kit' prog 0 -> drums (640 notes, median pitch B5)
  track 'Elec. Piano (Classic)' prog 4 -> lead2 (14 notes, median pitch F#4)
  track 'Smooth Synth (Classic)' prog 90 -> other (224 notes, median pitch F3)
  track 'Harpsichord' prog 6 -> lead2 (48 notes, median pitch Bb4)
  track 'Acoustic Gtr (Classic)' prog 25 -> bass (52 notes, median pitch C2)
  track 'Ragtime Piano' prog 3 -> lead2 (50 notes, median pitch Ab4)
  track '8-Bit Sawtooth' prog 81 -> other (49 notes, median pitch Ab3)
  track '8-Bit Square' prog 80 -> lead2 (14 notes, median pitch D4)
```
       |       |       |       |       |       |       |       |      
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX......
bass   X...XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX......
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..........................
lead2  X..........................XXXX.........XXX.XXXX...XXXXXX......
other  ....................XXXXX......XXXXXXXXXXXXXXXXX.....XXXX......
```
sections, 4-bar blocks (start bar : length : roles): 1:4:D+C+L; 5:16:D+B+C+L; 21:4:D+B+C+L+O; 25:4:D+B+C+L; 29:4:D+B+C+L+L2; 33:4:D+B+C+L+O; 37:4:D+B+C+O; 41:8:D+B+C+L2+O; 49:4:D+B+C; 53:4:D+B+C+L2+O; 57:7:C
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 6
- harmonic loop period: None bars; lead 4-bar phrases: `NvvNNNNNNN------` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): 83 `XXXXXXXXXXXXXXXX` vel 0.95±0.06; cabasa `.X.X.XXX.X.X.XXX` vel 0.94±0.07; tamb `.x.X.X.X.x.X.X.X` vel 0.96±0.06; hat `XX.X...XXX.X...X` vel 0.96±0.05; 86 `X..X............` vel 0.96±0.06
- off-grid onsets 62%; swing8 0.612; swing16 0.25

#### OS #5210092 "epic lofi beat by @RKH" (https://onlinesequencer.net/5210092)
tempo 277 BPM, 4/4, 241 bars, mean active roles/bar 3.72
  track 'Rhodes' prog 2 -> chords (869 notes, median pitch D4)
  track 'Rhodes' prog 2 -> lead2 (25 notes, median pitch F5)
  track 'Synthesizer' prog 90 -> lead2 (192 notes, median pitch D4)
  track 'Grand Piano' prog 0 -> lead (376 notes, median pitch F5)
  track 'Strings' prog 48 -> chords (43 notes, median pitch Eb4)
  track 'Electric Drum Kit' prog 118 -> drums (700 notes, median pitch Bb5)
  track 'Electric Drum Kit' prog 118 -> drums (17 notes, median pitch Ab2)
  track 'Electric Drum Kit' prog 118 -> drums (6 notes, median pitch Ab2)
  track 'EDM Kit (E)' prog 0 -> drums (51 notes, median pitch Bb6)
  track 'Electric Drum Kit' prog 118 -> drums (220 notes, median pitch Ab2)
  track 'EDM Kit (E)' prog 0 -> drums (33 notes, median pitch E1)
  track '808 Drum Kit' prog 0 -> drums (304 notes, median pitch F#2)
  track 'Synthesizer' prog 90 -> bass (21 notes, median pitch Bb1)
  track 'EDM Kit (E)' prog 0 -> drums (52 notes, median pitch E1)
  track 'Synthesizer' prog 90 -> chords (786 notes, median pitch A4)
  track 'Grand Piano' prog 0 -> lead2 (25 notes, median pitch Ab5)
  track 'Synthesizer' prog 90 -> bass (128 notes, median pitch F#2)
  track 'Synthesizer' prog 90 -> other (72 notes, median pitch D3)
  track 'Concert Harp' prog 46 -> lead2 (86 notes, median pitch G4)
  track '2023 Drum Kit' prog 0 -> drums (4 notes, median pitch B2)
  track '909 Drum Kit' prog 0 -> drums (128 notes, median pitch F#2)
  track 'Synthesizer' prog 90 -> lead2 (285 notes, median pitch F5)
  track 'EDM Kit (E)' prog 0 -> drums (58 notes, median pitch Bb1)
  track 'Strings' prog 48 -> chords (256 notes, median pitch E4)
  track 'Synthesizer' prog 90 -> chords (77 notes, median pitch E3)
  track 'EDM Kit (E)' prog 0 -> drums (36 notes, median pitch F6)
  track 'Synthesizer' prog 90 -> lead2 (50 notes, median pitch D4)
  track 'Bass Guitar' prog 33 -> bass (4 notes, median pitch Eb2)
  track 'Synthesizer' prog 90 -> lead2 (119 notes, median pitch G4)
  track 'EDM Kit (E)' prog 0 -> drums (112 notes, median pitch Ab1)
  track 'Synthesizer' prog 90 -> chords (12 notes, median pitch F4)
  track 'EDM Kit (E)' prog 0 -> drums (11 notes, median pitch Eb1)
  track 'Concert Harp' prog 46 -> lead2 (8 notes, median pitch B3)
  track 'Xylophone' prog 13 -> lead2 (114 notes, median pitch G5)
  track 'Xylophone' prog 13 -> lead2 (17 notes, median pitch A5)
  track 'EDM Kit (E)' prog 0 -> drums (45 notes, median pitch C1)
  track 'Rhodes' prog 2 -> lead2 (70 notes, median pitch A3)
  track 'Synthesizer' prog 90 -> lead2 (55 notes, median pitch F4)
  track '2023 Drum Kit' prog 0 -> drums (72 notes, median pitch Eb1)
  track '2023 Drum Kit' prog 0 -> drums (112 notes, median pitch G1)
  track 'Synthesizer' prog 90 -> lead2 (80 notes, median pitch A4)
  track '808 Drum Kit' prog 0 -> drums (33 notes, median pitch Ab3)
  track '2023 Drum Kit' prog 0 -> drums (94 notes, median pitch D3)
  track 'Electric Drum Kit' prog 118 -> drums (84 notes, median pitch G1)
  track 'Strings' prog 48 -> pad (63 notes, median pitch E5)
  track 'Cello' prog 42 -> lead2 (17 notes, median pitch Bb3)
  track 'Flute (Classic)' prog 73 -> lead2 (7 notes, median pitch F#6)
  track 'Brass' prog 61 -> chords (20 notes, median pitch Ab4)
  track '2013 Drum Kit' prog 0 -> drums (6 notes, median pitch F2)
  track '2013 Drum Kit' prog 0 -> drums (53 notes, median pitch E2)
  track 'Pop Synth' prog 90 -> chords (144 notes, median pitch Eb4)
  track 'Synthesizer' prog 90 -> chords (256 notes, median pitch Eb4)
  track '909 Drum Kit' prog 0 -> drums (53 notes, median pitch B1)
  track 'EDM Kit (E)' prog 0 -> drums (16 notes, median pitch E2)
  track '909 Drum Kit' prog 0 -> drums (68 notes, median pitch F#2)
  track 'Synthesizer' prog 90 -> chords (1889 notes, median pitch E5)
  track 'Synthesizer' prog 90 -> chords (250 notes, median pitch F4)
  track 'Synthesizer' prog 90 -> bass (52 notes, median pitch Ab1)
  track '2023 Drum Kit' prog 0 -> drums (44 notes, median pitch E6)
  track 'Pop Synth' prog 90 -> chords (16 notes, median pitch Eb3)
  track 'EDM Kit (E)' prog 0 -> drums (64 notes, median pitch G2)
  track 'Synthesizer' prog 90 -> chords (282 notes, median pitch C3)
  track 'EDM Kit (E)' prog 0 -> drums (4 notes, median pitch F#3)
  track 'EDM Kit (E)' prog 0 -> drums (136 notes, median pitch Ab1)
  track 'Electric Drum Kit' prog 118 -> drums (192 notes, median pitch F#3)
  track 'Grand Piano' prog 0 -> chords (4 notes, median pitch Ab3)
  track '2023 Drum Kit' prog 0 -> drums (26 notes, median pitch F#3)
  track 'EDM Kit (E)' prog 0 -> drums (88 notes, median pitch D1)
  track 'EDM Kit (E)' prog 0 -> drums (44 notes, median pitch Bb2)
```
       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |
drums  ...............XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX....X.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   ................................XXXXXXXXXXXX....XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX....XXXXXX..............................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX................XXXX.XXXXXXX.XXXXXXX.XXXXXXX.XXX.....................................
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.....................................
pad    ..............................................................................................................................................XXXXXXXXXXXXXXXXXX...........................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.......................
lead   ......................XXXXXXXXXXXXXXXXXXXXXXX...XXXXXXXXXXXXXXXXX.XX..XXXXXXXXXXXXXXXXXX..XX.........................XX..XX.......XXXXXXX...XXXXXXXXXXXXXXXX................................................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead2  ..............XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX......XXXXXXXXXXXX............XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXX.XXX.XXXXXXX.XXX.XXXXXXXXXXXXXXXXXXXXXXX.....................................
other  ................................................XX..............XXXXXXXXXXXXXXXX.................................................................................................................................................................
```
sections, 4-bar blocks (start bar : length : roles): 1:12:C; 13:4:C+L2; 17:4:D+C+L2; 21:12:D+C+L+L2; 33:12:D+B+C+L+L2; 45:4:D+C+L2; 49:4:D+B+C+L+L2+O; 53:12:D+B+C+L+L2; 65:16:D+B+C+L+L2+O; 81:4:D+B+C+L; 85:4:D+C+L+L2; 89:4:D+B+C+L+L2; 93:4:B+C+L2; 97:12:D+C; 109:8:D+C+L2; 117:8:D+C+L+L2; 125:4:D+B+C+L2; 129:8:D+B+C+L+L2; 137:4:D+B+C+L2; 141:16:D+B+C+P+L+L2; 157:4:D+C+P+L2; 161:12:D+C+L2; 173:16:D+B+C+L2; 189:16:D+B+C+P+L2; 205:16:D+P+L; 221:21:D+L
- drums enter bar 16, drum dropouts (start bar,len): [(94, 4), (99, 1)], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `-----NNNNvvNNNNNNvNNNNN------NN-NN-NRRR------------vNNNRvRN--` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): 82 `X...X...X...X...` vel 0.6±0.4; hat `x.x.X...X...X.x.` vel 1.0±0.0; pedalhat `x.X...X.x.x.X.x.` vel 1.0±0.0; 31 `.xx...x...x.x...` vel 0.68±0.28; 28 `X.......x.......` vel 0.99±0.07
- off-grid onsets 10%; swing8 0.52; swing16 0.24

#### OS #5215869 "lofi type beat" (https://onlinesequencer.net/5215869)
tempo 120 BPM, 4/4, 45 bars, mean active roles/bar 4.04
  track '8-Bit Sine' prog 80 -> chords (219 notes, median pitch F#3)
  track '2023 Drum Kit' prog 0 -> drums (314 notes, median pitch C#2)
  track 'Bass Guitar' prog 33 -> bass (90 notes, median pitch E2)
  track 'Rhodes' prog 2 -> lead (196 notes, median pitch E5)
  track 'Pizzicato' prog 45 -> chords (32 notes, median pitch F4)
  track 'Grand Piano' prog 0 -> lead2 (26 notes, median pitch D5)
  track 'EDM Kit (E)' prog 0 -> drums (4 notes, median pitch Bb6)
```
       |       |       |       |       |       |    
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   .XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
chords .XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
lead   .XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
lead2  ..........XX.X..X.XX.X..X....................
```
sections, 4-bar blocks (start bar : length : roles): 1:8:D+B+C+L; 9:4:D+B+C+L+L2; 13:4:D+B+C+L; 17:4:D+B+C+L+L2; 21:24:D+B+C+L; 45:1:D
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 8 bars; lead 4-bar phrases: `NNvvRRNNNNv-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X...X...X...X...` vel 1.0±0.0; crash `X...X...X...X...` vel 1.0±0.0; 27 `X.............x.` vel 1.0±0.0; 32 `........X.......` vel 1.0±0.0; 31 `......x...X.....` vel 1.0±0.0
- off-grid onsets 2%; swing8 0.504; swing16 None

#### OS #5224910 "How to Train Your Dragon - Test Drive(remake) (lofi remix)" (https://onlinesequencer.net/5224910)
tempo 111 BPM, 4/4, 70 bars, mean active roles/bar 3.06
  track 'Synth Bass' prog 39 -> bass (2357 notes, median pitch D4)
  track 'Steel Drums' prog 114 -> lead (160 notes, median pitch A4)
  track 'Rhodes' prog 2 -> chords (19775 notes, median pitch D5)
  track '2013 Drum Kit' prog 0 -> drums (475 notes, median pitch D2)
  track 'Scifi' prog 103 -> chords (593 notes, median pitch D6)
  track '8-Bit Triangle' prog 80 -> chords (442 notes, median pitch B2)
  track '909 Drum Kit' prog 0 -> drums (2474 notes, median pitch B2)
```
       |       |       |       |       |       |       |       |       |     
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX...XXX.......XXXXXXXXXXXXXXX...
bass   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   ..XXXXXXXXXXXXXX..................................................XXX.
```
sections, 4-bar blocks (start bar : length : roles): 1:16:D+B+C+L; 17:28:D+B+C; 45:8:B+C; 53:12:D+B+C; 65:4:D+B+C+L; 69:2:B+C+L
- drums enter bar 1, drum dropouts (start bar,len): [(40, 3), (46, 7)], bars after drums stop: 3
- harmonic loop period: None bars; lead 4-bar phrases: `NNvN------------NN` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): tomM2 `X..XX.XXX..X.XXX` vel 1.0±0.01; tomH2 `X..XX.XXX..X.XXX` vel 1.0±0.02; snare `X...X.X.X...X.X.` vel 1.0±0.0; crash2 `X..........xXXXX` vel 0.99±0.03; 25 `X...........XXXX` vel 0.99±0.05
- off-grid onsets 83%; swing8 0.574; swing16 0.265

#### OS #5235705 "epic lofi be like skull emoji by @RKH" (https://onlinesequencer.net/5235705)
tempo 181 BPM, 4/4, 115 bars, mean active roles/bar 4.1
  track 'Vibraphone' prog 11 -> lead (883 notes, median pitch C#5)
  track 'Synthesizer' prog 90 -> chords (3549 notes, median pitch D4)
  track 'Synthesizer' prog 90 -> lead2 (848 notes, median pitch A3)
  track 'Synthesizer' prog 90 -> lead2 (760 notes, median pitch A3)
  track 'EDM Kit (E)' prog 0 -> drums (665 notes, median pitch Eb5)
  track 'EDM Kit (E)' prog 0 -> drums (63 notes, median pitch B1)
  track '808 Drum Kit' prog 0 -> drums (479 notes, median pitch Eb2)
  track '808 Drum Kit' prog 0 -> drums (91 notes, median pitch B1)
  track 'Electric Drum Kit' prog 118 -> drums (716 notes, median pitch F#2)
  track 'Bass' prog 32 -> bass (141 notes, median pitch A1)
  track 'EDM Kit (E)' prog 0 -> drums (10 notes, median pitch Bb6)
  track '909 Drum Kit' prog 0 -> drums (59 notes, median pitch F2)
  track 'Grand Piano' prog 0 -> lead2 (87 notes, median pitch C#6)
  track 'EDM Kit (E)' prog 0 -> drums (450 notes, median pitch F1)
  track 'Grand Piano' prog 0 -> lead2 (218 notes, median pitch D5)
  track 'Vibraphone' prog 11 -> chords (256 notes, median pitch F5)
  track 'Synthesizer' prog 90 -> other (16 notes, median pitch Ab3)
  track 'Drum Kit' prog 0 -> drums (36 notes, median pitch A1)
  track '8-Bit Square' prog 80 -> chords (230 notes, median pitch Bb3)
  track '8-Bit Square' prog 80 -> chords (1172 notes, median pitch A2)
  track '8-Bit Square' prog 80 -> chords (48 notes, median pitch C4)
  track '8-Bit Square' prog 80 -> chords (55 notes, median pitch F3)
  track '8-Bit Square' prog 80 -> chords (354 notes, median pitch Bb3)
  track '8-Bit Square' prog 80 -> chords (571 notes, median pitch F#3)
  track '8-Bit Square' prog 80 -> chords (89 notes, median pitch Bb3)
  track '8-Bit Square' prog 80 -> chords (215 notes, median pitch A3)
  track '8-Bit Square' prog 80 -> chords (239 notes, median pitch A3)
  track '8-Bit Square' prog 80 -> chords (187 notes, median pitch A3)
  track '8-Bit Square' prog 80 -> chords (135 notes, median pitch E3)
  track '8-Bit Square' prog 80 -> chords (26 notes, median pitch G4)
  track '8-Bit Square' prog 80 -> chords (62 notes, median pitch F4)
  track '8-Bit Square' prog 80 -> chords (202 notes, median pitch Ab5)
  track '8-Bit Square' prog 80 -> chords (157 notes, median pitch F#3)
  track '8-Bit Square' prog 80 -> chords (151 notes, median pitch B3)
  track '8-Bit Square' prog 80 -> chords (94 notes, median pitch G3)
  track '8-Bit Square' prog 80 -> other (34 notes, median pitch F#3)
  track '8-Bit Square' prog 80 -> chords (126 notes, median pitch G3)
  track '8-Bit Square' prog 80 -> chords (221 notes, median pitch A3)
  track '8-Bit Square' prog 80 -> chords (129 notes, median pitch F3)
  track '8-Bit Square' prog 80 -> chords (96 notes, median pitch E3)
  track '8-Bit Square' prog 80 -> chords (120 notes, median pitch F#3)
  track '8-Bit Square' prog 80 -> chords (78 notes, median pitch B3)
  track '8-Bit Square' prog 80 -> lead2 (101 notes, median pitch G5)
  track '8-Bit Square' prog 80 -> chords (62 notes, median pitch A3)
  track '8-Bit Square' prog 80 -> lead2 (26 notes, median pitch B3)
  track '8-Bit Square' prog 80 -> lead2 (21 notes, median pitch C4)
  track '8-Bit Square' prog 80 -> chords (41 notes, median pitch D4)
  track '8-Bit Square' prog 80 -> lead2 (8 notes, median pitch E4)
  track 'Electric Piano' prog 5 -> lead2 (196 notes, median pitch D5)
  track 'Vibraphone' prog 11 -> lead2 (196 notes, median pitch D5)
  track 'Vibraphone' prog 11 -> lead2 (196 notes, median pitch D5)
  track 'Pop Synth' prog 90 -> lead2 (196 notes, median pitch D5)
  track 'Smooth Synth' prog 90 -> chords (132 notes, median pitch C#4)
  track 'Grand Piano' prog 0 -> lead2 (230 notes, median pitch D5)
  track 'Synthesizer' prog 90 -> other (68 notes, median pitch C#3)
```
       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |  
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.X...XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.........
bass   .XXXXXXXXXXXXXXXX................XXXXXXXX................XXXXXXXXXXXXXXXXXXXXXXXXX.................................
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX........X
lead   .................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.......XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.........
lead2  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
other  XXXXXX....XX..XX.................................................XXXXXXXXXXXXXXXX..................................
```
sections, 4-bar blocks (start bar : length : roles): 1:16:D+B+C+L2+O; 17:16:D+C+L+L2; 33:8:D+B+L+L2; 41:16:D+C+L+L2; 57:8:D+B+C+L+L2; 65:4:D+B+C+L+L2+O; 69:4:B+C+L2+O; 73:8:D+B+C+L+L2+O; 81:4:D+B+C+L+L2; 85:24:D+C+L+L2; 109:7:L2
- drums enter bar 1, drum dropouts (start bar,len): [(69, 1), (71, 3)], bars after drums stop: 9
- harmonic loop period: 2 bars; lead 4-bar phrases: `----NvvRRRNvvRNNN-vvNvvRRRN--` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): clap `X.x.X.x.X.x.X.X.` vel 0.48±0.25; hat `XX..XX..XX..XX..` vel 0.94±0.23; 81 `X.X.X...X.x.X...` vel 0.53±0.05; claves `X.X.X...X.x.X...` vel 1.0±0.0; pedalhat `..X...X...X...X.` vel 1.0±0.0
- off-grid onsets 1%; swing8 0.503; swing16 0.25

#### OS #5335524 "chill lofi jazz" (https://onlinesequencer.net/5335524)
tempo 74 BPM, 4/4, 96 bars, mean active roles/bar 4.0
  track 'Rhodes' prog 2 -> lead (1213 notes, median pitch C5)
  track '8-Bit Sine' prog 80 -> bass (623 notes, median pitch Eb2)
  track '909 Drum Kit' prog 0 -> drums (1776 notes, median pitch D2)
  track 'Saxophone' prog 66 -> chords (1867 notes, median pitch Eb5)
```
       |       |       |       |       |       |       |       |       |       |       |       |       
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:96:D+B+C+L
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 16 bars; lead 4-bar phrases: `NNNNvvvvRRRRRRRRRRRRRRRv-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X.X.X.X.x.x.X.X.` vel 0.93±0.07; kick2 `Xx.x..xX..xx..xx` vel 1.0±0.0; snare `..x.Xx...x..X..x` vel 1.0±0.0; openhat `x......xx.xx..x.` vel 0.96±0.06
- off-grid onsets 58%; swing8 0.504; swing16 0.34

#### OS #5337798 "chill jazzy lofi music (celebrating 2 years on online sequencer april 10th 2026)" (https://onlinesequencer.net/5337798)
tempo 80 BPM, 4/4, 96 bars, mean active roles/bar 4.5
  track 'French Horn' prog 60 -> chords (559 notes, median pitch Eb4)
  track 'Bass' prog 32 -> bass (623 notes, median pitch Eb2)
  track '2023 Drum Kit' prog 0 -> drums (1776 notes, median pitch Ab1)
  track 'Saxophone' prog 66 -> lead (654 notes, median pitch F5)
  track 'Trombone' prog 57 -> lead2 (654 notes, median pitch F5)
```
       |       |       |       |       |       |       |       |       |       |       |       |       
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   XXXXXXXXXXXX....XXXXXXXXXXXX....XXXXXXXXXXXX....XXXXXXXXXXXX....XXXXXXXXXXXX....XXXXXXXXXXXX....
lead2  XXXXXXXXXXXX....XXXXXXXXXXXX....XXXXXXXXXXXX....XXXXXXXXXXXX....XXXXXXXXXXXX....XXXXXXXXXXXX....
```
sections, 4-bar blocks (start bar : length : roles): 1:12:D+B+C+L+L2; 13:4:D+B+C; 17:12:D+B+C+L+L2; 29:4:D+B+C; 33:12:D+B+C+L+L2; 45:4:D+B+C; 49:12:D+B+C+L+L2; 61:4:D+B+C; 65:12:D+B+C+L+L2; 77:4:D+B+C; 81:12:D+B+C+L+L2; 93:4:D+B+C
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 8 bars; lead 4-bar phrases: `NNN-RRR-RRR-RRR-RRR-RRR--` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X.X.X.X.x.x.X.X.` vel 0.93±0.07; 29 `Xx.x..xX..xx..xx` vel 1.0±0.0; 32 `....X.......X...` vel 1.0±0.0; 30 `..x..x...x.....x` vel 1.0±0.0; pedalhat `x.......x..x....` vel 0.92±0.06
- off-grid onsets 56%; swing8 0.504; swing16 0.34

#### OS #5350270 "Slate [LoFi]" (https://onlinesequencer.net/5350270)
tempo 65 BPM, 4/4, 41 bars, mean active roles/bar 3.39
  track 'Electric Piano' prog 5 -> chords (120 notes, median pitch C3)
  track '808 Drum Kit' prog 0 -> drums (161 notes, median pitch B1)
  track '8-Bit Drum Kit' prog 0 -> drums (144 notes, median pitch F#2)
  track 'Electric Piano' prog 5 -> lead (188 notes, median pitch D4)
  track 'Synthesizer' prog 90 -> pad (16 notes, median pitch G3)
```
       |       |       |       |       |       |
drums  ..XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
pad    ........XXXXXXXXX.......XXXXXXXXX........
lead   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:8:D+C+L; 9:8:D+C+P+L; 17:8:D+C+L; 25:8:D+C+P+L; 33:9:D+C+L
- drums enter bar 3, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `NvRRRRvvvRN` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X...X...X...X..x` vel 0.93±0.09; kick2 `X.......x..X....` vel 0.94±0.09; snare `....X.......X...` vel 0.94±0.08
- off-grid onsets 74%; swing8 0.674; swing16 None

#### OS #5360955 "The Break Of Sunlight(lofi hiphop beat)" (https://onlinesequencer.net/5360955)
tempo 124 BPM, 4/4, 56 bars, mean active roles/bar 3.29
  track 'Grand Piano' prog 0 -> chords (494 notes, median pitch A4)
  track '808 Drum Kit' prog 0 -> drums (500 notes, median pitch F#2)
  track '909 Drum Kit' prog 0 -> drums (259 notes, median pitch B1)
  track 'Rhodes' prog 2 -> chords (494 notes, median pitch A4)
  track 'Music Box' prog 10 -> lead (164 notes, median pitch E5)
  track 'Vibraphone' prog 11 -> lead2 (164 notes, median pitch E5)
  track 'Bass Guitar' prog 33 -> bass (225 notes, median pitch Bb1)
  track 'Flute (Classic)' prog 73 -> chords (494 notes, median pitch A4)
```
       |       |       |       |       |       |       |       
drums  XXXXXXXXXXXXXXXXXXXXXXXX........XXXXXXXXXXXXXXXX........
bass   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   ................XXXXXXXX................XXXXXXXX........
lead2  ................XXXXXXXX................XXXXXXXX........
```
sections, 4-bar blocks (start bar : length : roles): 1:16:D+B+C; 17:8:D+B+C+L+L2; 25:8:B+C; 33:8:D+B+C; 41:8:D+B+C+L+L2; 49:8:B+C
- drums enter bar 1, drum dropouts (start bar,len): [(25, 8)], bars after drums stop: 8
- harmonic loop period: 4 bars; lead 4-bar phrases: `----NR----RR---` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `.X.X.X.XXX.X.X.X` vel 1.0±0.0; kick2 `.X...X.X...Xxx.X` vel 1.0±0.0; snare `.........X......` vel 1.0±0.0
- off-grid onsets 1%; swing8 0.75; swing16 0.25

#### OS #5373782 "LoFi - First try" (https://onlinesequencer.net/5373782)
tempo 130 BPM, 4/4, 99 bars, mean active roles/bar 2.67
  track 'Synthesizer' prog 90 -> other (352 notes, median pitch F#1)
  track 'Elec. Piano (Classic)' prog 4 -> lead (212 notes, median pitch G4)
  track '808 Drum Kit' prog 0 -> drums (800 notes, median pitch Eb2)
  track 'Drum Kit' prog 0 -> drums (8501 notes, median pitch B1)
```
      |       |       |       |       |       |       |       |       |       |       |       |       |  
drums XXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXX..XXXXXX.
lead  .XXXXXX.XXXX.XXXXXXXXXX.XX.XXXXXXXX.XXXXXXXXXX.XXXXXXXXXXXXXXX.XXXXXXXXXXXXX.XXXXXXXX.......XX.....
other .XXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXX....XXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:84:D+L+O; 85:4:D+O; 89:4:D; 93:4:D+L+O; 97:3:D+O
- drums enter bar 1, drum dropouts (start bar,len): [(24, 1), (77, 1), (91, 2)], bars after drums stop: 1
- harmonic loop period: 1 bars; lead 4-bar phrases: `NNNNNNNNNNNNNNNNNNNNNN-N-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): kick2 `XXXXXXXXXXXXXXXX` vel 0.96±0.19; clap `XXXXXXXXXXXXXXXX` vel 1.0±0.0; 32 `x.x.x.x.x.x.x.x.` vel 0.99±0.06
- off-grid onsets 1%; swing8 0.669; swing16 0.25

#### OS #5375206 "LoFi - First try (1)" (https://onlinesequencer.net/5375206)
tempo 130 BPM, 4/4, 108 bars, mean active roles/bar 2.76
  track 'Synthesizer' prog 90 -> other (346 notes, median pitch F#1)
  track 'Elec. Piano (Classic)' prog 4 -> lead (248 notes, median pitch C#5)
  track '808 Drum Kit' prog 0 -> drums (1040 notes, median pitch Eb2)
  track 'Drum Kit' prog 0 -> drums (12369 notes, median pitch B1)
```
      |       |       |       |       |       |       |       |       |       |       |       |       |       |   
drums XXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead  .XXXXXX.XXXX.XXXXXXXXXX.XX.XXXXXXXX.XXXXXXXXXX.XXXXXXXXXXXXXXX.XXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXX...........
other .XXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
```
sections, 4-bar blocks (start bar : length : roles): 1:96:D+L+O; 97:12:D+O
- drums enter bar 1, drum dropouts (start bar,len): [(24, 1), (77, 1)], bars after drums stop: 0
- harmonic loop period: 1 bars; lead 4-bar phrases: `NNNNNNNNNNNNNNNNNNNNNNNNN---` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): kick2 `XXXXXXXXXXXXXXXX` vel 0.95±0.21; clap `XXxXXXxXxxXXxxxX` vel 1.0±0.0; 32 `xxxxxxxxxxxxxxxx` vel 0.98±0.06
- off-grid onsets 0%; swing8 0.627; swing16 0.25

#### OS #5375603 "GPa/C (lofi mix)" (https://onlinesequencer.net/5375603)
tempo 80 BPM, 4/4, 185 bars, mean active roles/bar 2.97
  track 'Rhodes' prog 2 -> chords (1102 notes, median pitch C2)
  track 'Rhodes' prog 2 -> lead (1183 notes, median pitch F5)
  track 'Rhodes' prog 2 -> chords (346 notes, median pitch D3)
  track '2023 Drum Kit' prog 0 -> drums (288 notes, median pitch Ab1)
  track 'Violin' prog 40 -> chords (352 notes, median pitch F#4)
  track 'Brass' prog 61 -> other (320 notes, median pitch C3)
```
       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |
drums  ...............XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX................................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
chords XXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   ....XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
other  ................................XXXXXXXXXXXXXXXX........................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX................................................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
```
sections, 4-bar blocks (start bar : length : roles): 1:4:C; 5:12:C+L; 17:16:D+C+L; 33:16:D+C+L+O; 49:16:D+C; 65:8:D+C+L; 73:32:D+C+L+O; 105:16:D+C; 121:32:C+L; 153:32:D+C+L+O; 185:1:C+L
- drums enter bar 16, drum dropouts (start bar,len): [(121, 32)], bars after drums stop: 1
- harmonic loop period: 8 bars; lead 4-bar phrases: `-NRvNNRRRRRR----RRRRRRRRRR----RRRRRRRRRRRRRRRv-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): 32 `........X.......` vel 1.0±0.0; 27 `x...x...........` vel 1.0±0.0; snare2 `............X...` vel 1.0±0.0
- off-grid onsets 0%; swing8 0.5; swing16 None

#### OS #5380639 "LoFi - First try (1) (2)" (https://onlinesequencer.net/5380639)
tempo 130 BPM, 4/4, 102 bars, mean active roles/bar 2.81
  track 'Synthesizer' prog 90 -> other (335 notes, median pitch F#1)
  track 'Elec. Piano (Classic)' prog 4 -> lead (246 notes, median pitch F4)
  track '808 Drum Kit' prog 0 -> drums (880 notes, median pitch Eb2)
  track 'Drum Kit' prog 0 -> drums (11075 notes, median pitch B1)
```
      |       |       |       |       |       |       |       |       |       |       |       |       |     
drums XXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXX
lead  .XXXXXX.XXXX.XXXXXXXXXX.XX.XXXXXXXX.XXXXXXXXXX.XXXXXXXXXXXXXXX.XXXXXXXXXXXXX.XXXXXXXXX.XXXXXXX..XXXXX.
other .XXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXX.
```
sections, 4-bar blocks (start bar : length : roles): 1:102:D+L+O
- drums enter bar 1, drum dropouts (start bar,len): [(24, 1), (77, 1)], bars after drums stop: 0
- harmonic loop period: 1 bars; lead 4-bar phrases: `NNNNNNNNNNNNNNNNNNNNNNNNNN` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): kick2 `XXXXXXXXXXXXXXXX` vel 0.96±0.2; clap `XXXXXXXXXXXXXXXX` vel 1.0±0.0; 32 `xxxxxxxxxxxxxxxx` vel 0.99±0.06
- off-grid onsets 0%; swing8 0.639; swing16 0.25

#### OS #5383967 "LoFi - First try (1) (2) (2)" (https://onlinesequencer.net/5383967)
tempo 130 BPM, 4/4, 102 bars, mean active roles/bar 2.84
  track 'Synthesizer' prog 90 -> other (335 notes, median pitch F#1)
  track 'Elec. Piano (Classic)' prog 4 -> lead (243 notes, median pitch G4)
  track '808 Drum Kit' prog 0 -> drums (880 notes, median pitch Eb2)
  track 'Drum Kit' prog 0 -> drums (11075 notes, median pitch B1)
  track 'Electric Piano' prog 5 -> lead2 (13 notes, median pitch Bb4)
```
      |       |       |       |       |       |       |       |       |       |       |       |       |     
drums XXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXX
lead  .XXXXXX.XXXX.XXXXXXXXXX.XX.XXXXXXXX.XXXXXXXXXX.XXXXXXXXXXXXXXX.XXXXXXXXXXXXX.XXXXXXXXX.XXXXXXX..XXXXX.
lead2 ............................................................................................XX..X.....
other .XXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXX.
```
sections, 4-bar blocks (start bar : length : roles): 1:92:D+L+O; 93:4:D+L+L2+O; 97:6:D+L+O
- drums enter bar 1, drum dropouts (start bar,len): [(24, 1), (77, 1)], bars after drums stop: 0
- harmonic loop period: 1 bars; lead 4-bar phrases: `NNNNNNNNNNNNNNNNNNNNNNNNNN` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): kick2 `XXXXXXXXXXXXXXXX` vel 0.96±0.2; clap `XXXXXXXXXXXXXXXX` vel 1.0±0.0; 32 `xxxxxxxxxxxxxxxx` vel 0.99±0.06
- off-grid onsets 0%; swing8 0.639; swing16 0.25

#### OS #5385286 "Accidental Lofi" (https://onlinesequencer.net/5385286)
tempo 148 BPM, 4/4, 74 bars, mean active roles/bar 4.54
  track '808 Drum Kit' prog 0 -> drums (408 notes, median pitch D2)
  track 'Smooth Synth' prog 90 -> chords (156 notes, median pitch E4)
  track 'Bass' prog 32 -> bass (220 notes, median pitch D2)
  track 'Jazz Guitar' prog 26 -> lead2 (275 notes, median pitch B4)
  track 'Synth Pluck (Classic)' prog 84 -> other (384 notes, median pitch G3)
  track 'Jazz Guitar' prog 26 -> lead (364 notes, median pitch B4)
  track 'Rhodes' prog 2 -> bass (349 notes, median pitch B2)
  track 'Bass Guitar' prog 33 -> bass (20 notes, median pitch A1)
  track 'EDM Kit (E)' prog 0 -> drums (8 notes, median pitch Bb6)
```
       |       |       |       |       |       |       |       |       |       | 
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX........XXXXXXXXXXXXXXXX..
bass   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..
chords X.X.X.X.X.X.X.X.........X.X.X.X.X.X.X.X.........X.X.X.XXX.X.X.X.X.X.X.X...
lead   XXXXXXXXXXXXXXXXX.X.X.XXXXXXXXXXXXXXXXXXX.X.X.XXXXXXXXXXXXXXXXXXXXXXXXXX..
lead2  XXXXXXXXXXXXXXXXX.X.....XXXXXXXXXXXXXXXXXXXX.X..........XXXXXXXXXXXXXXXXXX
other  XXXXXXXXXXXXXXXX........XXXXXXXXXXXXXXXX................XXXXXXXXXXXXXXXX..
```
sections, 4-bar blocks (start bar : length : roles): 1:16:D+B+C+L+L2+O; 17:4:D+B+L+L2; 21:4:D+B+L; 25:16:D+B+C+L+L2+O; 41:4:D+B+L+L2; 45:4:D+B+L; 49:8:B+C+L; 57:16:D+B+C+L+L2+O; 73:2:L2
- drums enter bar 1, drum dropouts (start bar,len): [(49, 8)], bars after drums stop: 2
- harmonic loop period: 2 bars; lead 4-bar phrases: `NNRRNvRRRRRRNRRvRR-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X...X...X...X...` vel 1.0±0.0; kick2 `x...x.....x.x.x.` vel 1.0±0.0; snare `........X.......` vel 1.0±0.0; 94 `X...............` vel 1.0±0.0; ride `X...............` vel 1.0±0.0
- off-grid onsets 0%; swing8 0.5; swing16 None

#### OS #5388123 "LoFi - First try (1) (2) (2) (1)" (https://onlinesequencer.net/5388123)
tempo 130 BPM, 4/4, 102 bars, mean active roles/bar 2.84
  track 'Synthesizer' prog 90 -> other (335 notes, median pitch F#1)
  track 'Elec. Piano (Classic)' prog 4 -> lead (243 notes, median pitch G4)
  track '808 Drum Kit' prog 0 -> drums (880 notes, median pitch Eb2)
  track 'Drum Kit' prog 0 -> drums (11075 notes, median pitch B1)
  track 'Grand Piano (Classic)' prog 0 -> lead2 (6 notes, median pitch D5)
```
      |       |       |       |       |       |       |       |       |       |       |       |       |     
drums XXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXX
lead  .XXXXXX.XXXX.XXXXXXXXXX.XX.XXXXXXXX.XXXXXXXXXX.XXXXXXXXXXXXXXX.XXXXXXXXXXXXX.XXXXXXXXX.XXXXXXX..XXXXX.
lead2 ............................................................................................XXX.......
other .XXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXX.
```
sections, 4-bar blocks (start bar : length : roles): 1:92:D+L+O; 93:4:D+L+L2+O; 97:6:D+L+O
- drums enter bar 1, drum dropouts (start bar,len): [(24, 1), (77, 1)], bars after drums stop: 0
- harmonic loop period: 1 bars; lead 4-bar phrases: `NNNNNNNNNNNNNNNNNNNNNNNNNN` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): kick2 `XXXXXXXXXXXXXXXX` vel 0.96±0.2; clap `XXXXXXXXXXXXXXXX` vel 1.0±0.0; 32 `xxxxxxxxxxxxxxxx` vel 0.99±0.06
- off-grid onsets 0%; swing8 0.639; swing16 0.25

#### OS #5388655 "Lofi T.T.G. Mix" (https://onlinesequencer.net/5388655)
tempo 120 BPM, 4/4, 56 bars, mean active roles/bar 2.71
  track 'Electric Piano' prog 5 -> lead2 (336 notes, median pitch Ab4)
  track '2023 Drum Kit' prog 0 -> drums (741 notes, median pitch F#2)
  track 'Violin' prog 40 -> lead (384 notes, median pitch Bb4)
  track 'Pop Synth' prog 90 -> pad (8 notes, median pitch F#3)
```
      |       |       |       |       |       |       |       
drums XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
pad   ................................XXXXXXXX................
lead  ........XXXXXXXXXXXXXXXX................XXXXXXXXXXXXXXXX
lead2 XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:8:D+L2; 9:16:D+L+L2; 25:8:D+L2; 33:8:D+P+L2; 41:16:D+L+L2
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `--NRvR----RRRR-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X.X.X.X.X.X.X.X.` vel 1.0±0.0; 29 `X.....x...X..x.x` vel 1.0±0.0; 32 `....X.......X.x.` vel 1.0±0.0
- off-grid onsets 0%; swing8 0.51; swing16 0.25

#### OS #5391982 "Lofi type Riffing" (https://onlinesequencer.net/5391982)
tempo 110 BPM, 4/4, 36 bars, mean active roles/bar 4.11
  track 'Acoustic Guitar' prog 24 -> chords (280 notes, median pitch B4)
  track 'Bass Guitar' prog 33 -> bass (196 notes, median pitch A1)
  track 'Acoustic Guitar' prog 24 -> chords (280 notes, median pitch B4)
  track 'Bass Guitar' prog 33 -> bass (24 notes, median pitch C#3)
  track 'Bass Guitar' prog 33 -> bass (24 notes, median pitch C#3)
  track 'Xylophone' prog 13 -> chords (360 notes, median pitch B4)
  track 'Smooth Synth' prog 90 -> chords (360 notes, median pitch B2)
  track 'Bass' prog 32 -> bass (43 notes, median pitch F#3)
  track 'Rhodes' prog 2 -> lead2 (45 notes, median pitch F#4)
  track '808 Drum Kit' prog 0 -> drums (36 notes, median pitch B1)
  track '808 Drum Kit' prog 0 -> drums (16 notes, median pitch F#2)
  track 'Electric Drum Kit' prog 118 -> drums (16 notes, median pitch E1)
  track 'Lucent Choir' prog 52 -> lead2 (8 notes, median pitch B4)
  track '2023 Drum Kit' prog 0 -> drums (128 notes, median pitch F1)
  track '2023 Drum Kit' prog 0 -> drums (32 notes, median pitch Ab1)
  track 'Ragtime Piano' prog 3 -> lead2 (20 notes, median pitch F#5)
  track 'Electric Guitar' prog 27 -> lead (122 notes, median pitch B3)
  track 'Electric Guitar' prog 27 -> lead2 (122 notes, median pitch B3)
  track 'Pop Synth' prog 90 -> chords (72 notes, median pitch A3)
  track '8-Bit Triangle' prog 80 -> chords (72 notes, median pitch A2)
  track '8-Bit Sawtooth' prog 81 -> chords (72 notes, median pitch A2)
  track 'EDM Kit (E)' prog 0 -> drums (4 notes, median pitch Bb6)
  track 'Electric Piano' prog 5 -> chords (12 notes, median pitch D6)
  track 'Jazz Guitar' prog 26 -> bass (61 notes, median pitch B2)
```
       |       |       |       |       |   
drums  ....XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   .XX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   ....................XXXXXXXXXXXXXXXX
lead2  .XXXXXXXXXX..XX..XX.XXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:4:B+C+L2; 5:16:D+B+C+L2; 21:16:D+B+C+L+L2
- drums enter bar 5, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 1 bars; lead 4-bar phrases: `-----NNRR-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): 34 `..X...x.x.....X.` vel 1.0±0.0; 26 `X.....x...X.....` vel 1.0±0.0; 29 `X.....x...X.....` vel 1.0±0.0; kick2 `X.........x...x.` vel 0.51±0.28; 32 `....X.......X...` vel 1.0±0.0
- off-grid onsets 17%; swing8 0.5; swing16 None

#### OS #5401374 "True love - Lofi Type Beat" (https://onlinesequencer.net/5401374)
tempo 60 BPM, 4/4, 56 bars, mean active roles/bar 3.25
  track 'Grand Piano' prog 0 -> chords (320 notes, median pitch D4)
  track 'Flute (Classic)' prog 73 -> lead2 (272 notes, median pitch F5)
  track '8-Bit Triangle' prog 80 -> bass (40 notes, median pitch Ab1)
  track '2023 Drum Kit' prog 0 -> drums (160 notes, median pitch Eb1)
  track '808 Drum Kit' prog 0 -> drums (780 notes, median pitch F#2)
  track 'Acoustic Gtr (Classic)' prog 25 -> lead (349 notes, median pitch C4)
  track 'Electric Piano' prog 5 -> lead2 (26 notes, median pitch B4)
```
       |       |       |       |       |       |       |       
drums  ....XXXXXXXXXXXXXXXXXXXXXXXX............XXXXXXXXXXXXXXXX
bass   ....XXXXXXXXXXXXXXXXXXXXXXXX............XXXXXXXXXXXXXXXX
chords XXXXXXXXXXXXXXXXXXXX....................................
lead   ............................XXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead2  XXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXX.
```
sections, 4-bar blocks (start bar : length : roles): 1:4:C+L2; 5:16:D+B+C+L2; 21:8:D+B+L2; 29:12:L+L2; 41:16:D+B+L+L2
- drums enter bar 5, drum dropouts (start bar,len): [(29, 12)], bars after drums stop: 0
- harmonic loop period: 4 bars; lead 4-bar phrases: `-------NvRRRRR-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `XXXX.xx..XXXXxxx` vel 1.0±0.0; 27 `X..X.....XX.....` vel 1.0±0.0; snare `.......X.X.....x` vel 1.0±0.0; stick `....X.......X...` vel 1.0±0.0
- off-grid onsets 12%; swing8 0.63; swing16 0.25

#### OS #5404159 "lofi jazz loop" (https://onlinesequencer.net/5404159)
tempo 204 BPM, 3/4, 32 bars, mean active roles/bar 2.38
  track 'Rhodes' prog 2 -> lead (49 notes, median pitch A4)
  track 'Rhodes' prog 2 -> lead2 (18 notes, median pitch Ab5)
  track '909 Drum Kit' prog 0 -> drums (58 notes, median pitch F#2)
  track '909 Drum Kit' prog 0 -> drums (13 notes, median pitch D2)
  track '808 Drum Kit' prog 0 -> drums (15 notes, median pitch B1)
```
      |       |       |       |       
drums ...XXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead2 ...XXX......XXXX..XXXX......XXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:4:L; 5:4:D+L+L2; 9:4:D+L; 13:12:D+L+L2; 25:4:D+L; 29:4:D+L+L2
- drums enter bar 4, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `NNNNNRRN-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X.....X.....` vel 1.0±0.0; kick2 `X...........` vel 1.0±0.0; snare `X...........` vel 1.0±0.0
- off-grid onsets 22%; swing8 0.5; swing16 None

#### OS #5404618 "lofi-4-final" (https://onlinesequencer.net/5404618)
tempo 72 BPM, 4/4, 71 bars, mean active roles/bar 3.77
  track 'Grand Piano' prog 0 -> chords (320 notes, median pitch Bb2)
  track 'Grand Piano' prog 0 -> lead (239 notes, median pitch Bb3)
  track 'Rhodes' prog 2 -> lead2 (62 notes, median pitch Bb3)
  track '2023 Drum Kit' prog 0 -> drums (52 notes, median pitch F1)
  track 'Bass Guitar' prog 33 -> bass (52 notes, median pitch E1)
  track 'Music Box' prog 10 -> lead2 (8 notes, median pitch C#5)
  track 'Violin' prog 40 -> lead2 (22 notes, median pitch Eb4)
  track '2023 Drum Kit' prog 0 -> drums (64 notes, median pitch F1)
```
       |       |       |       |       |       |       |       |       |      
drums  ................X.X.X.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.X.X..........
bass   ........................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX...........
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
lead2  ............XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.......X.
```
sections, 4-bar blocks (start bar : length : roles): 1:12:C+L; 13:4:C+L+L2; 17:8:D+C+L+L2; 25:36:D+B+C+L+L2; 61:4:C+L+L2; 65:7:C+L
- drums enter bar 23, drum dropouts (start bar,len): [], bars after drums stop: 14
- harmonic loop period: 4 bars; lead 4-bar phrases: `NNvRRRRRRRNRRRNNvN` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): 29 `X...X...x...x...` vel 1.0±0.0; 28 `....x.......X...` vel 1.0±0.0; 27 `........X.......` vel 1.0±0.0
- off-grid onsets 0%; swing8 None; swing16 None

#### OS #5407583 "Chill lofi beat" (https://onlinesequencer.net/5407583)
tempo 90 BPM, 4/4, 48 bars, mean active roles/bar 3.48
  track '2023 Drum Kit' prog 0 -> drums (341 notes, median pitch F#2)
  track 'Clean Guitar' prog 27 -> chords (189 notes, median pitch F3)
  track 'Bass' prog 32 -> bass (240 notes, median pitch B1)
  track '8-Bit Sawtooth' prog 81 -> other (130 notes, median pitch F3)
  track 'Saxophone' prog 66 -> chords (288 notes, median pitch Bb3)
```
       |       |       |       |       |       |       
drums  ................XXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXX
bass   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
other  ........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:8:B+C; 9:8:B+C+O; 17:32:D+B+C+O
- drums enter bar 17, drum dropouts (start bar,len): [(24, 1)], bars after drums stop: 0
- harmonic loop period: 4 bars; lead 4-bar phrases: `n/a` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `....X..X.X..X...` vel 0.81±0.3; tomL2 `..X.....X..X..X.` vel 0.8±0.3; 28 `X.........X...X.` vel 0.81±0.3
- off-grid onsets 0%; swing8 0.583; swing16 0.25

#### OS #5452555 "Nujabes -  Moon Strut (intro)" (https://onlinesequencer.net/5452555)
tempo 92 BPM, 4/4, 44 bars, mean active roles/bar 3.77
  track '2023 Drum Kit' prog 0 -> drums (318 notes, median pitch G1)
  track '2013 Drum Kit' prog 0 -> drums (408 notes, median pitch F#2)
  track 'Drum Kit' prog 0 -> drums (28 notes, median pitch A2)
  track 'Grand Piano' prog 0 -> chords (506 notes, median pitch F#3)
  track 'Grand Piano (Classic)' prog 0 -> lead (702 notes, median pitch C#4)
  track 'Flute (Classic)' prog 73 -> lead2 (16 notes, median pitch D4)
  track 'Rhodes' prog 2 -> chords (16 notes, median pitch C4)
  track 'Pizzicato' prog 45 -> bass (396 notes, median pitch A2)
```
       |       |       |       |       |       |   
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.............
bass   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   .XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead2  ..................XX..XX....................
```
sections, 4-bar blocks (start bar : length : roles): 1:16:D+B+C+L; 17:8:D+B+C+L+L2; 25:8:D+B+C+L; 33:12:B+C+L
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 13
- harmonic loop period: 2 bars; lead 4-bar phrases: `NvvvvRRvRRv-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `XxXxXxXXXXXXxxXX` vel 1.0±0.0; 31 `....Xx.x.x.x..XX` vel 0.99±0.08; 26 `Xx.X..x...X..x..` vel 1.0±0.0; kick2 `.......X.X......` vel 1.0±0.0; openhat `............X...` vel 1.0±0.0
- off-grid onsets 18%; swing8 0.631; swing16 0.25

#### OS #5474129 "Take 5(but its lofi)" (https://onlinesequencer.net/5474129)
tempo 150 BPM, 5/4, 32 bars, mean active roles/bar 3.5
  track '2023 Drum Kit' prog 0 -> drums (382 notes, median pitch Ab2)
  track '8-Bit Sine' prog 80 -> other (336 notes, median pitch E3)
  track 'Bass Guitar' prog 33 -> bass (176 notes, median pitch C#2)
  track 'Rhodes' prog 2 -> lead (108 notes, median pitch Ab4)
```
      |       |       |       |       
drums XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead  ................XXXXXXXXXXXXXXXX
other XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:16:D+B+O; 17:16:D+B+L+O
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 16 bars; lead 4-bar phrases: `----NvNN-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): pedalhat `X...X...X...X...X...` vel 1.0±0.0; 52 `X...X...X...X.......` vel 1.0±0.0; crash `X.X...X.X...X...X...` vel 1.0±0.0; 32 `......x.........X...` vel 1.0±0.0; kick2 `X...................` vel 1.0±0.0
- off-grid onsets 4%; swing8 0.517; swing16 None

#### OS #5481351 "How To Train Your Dragon Melody(lofi)(celebrating 200k plays)" (https://onlinesequencer.net/5481351)
tempo 126 BPM, 4/4, 162 bars, mean active roles/bar 3.46
  track 'Concert Harp' prog 46 -> chords (533 notes, median pitch D4)
  track '8-Bit Triangle' prog 80 -> chords (477 notes, median pitch B2)
  track 'Pizzicato' prog 45 -> other (150 notes, median pitch B2)
  track 'Cello (Classic)' prog 42 -> chords (242 notes, median pitch Bb3)
  track 'Cello (Classic)' prog 42 -> chords (195 notes, median pitch F3)
  track 'Church Organ (Classic)' prog 19 -> chords (221 notes, median pitch C3)
  track 'Rhodes' prog 2 -> chords (24136 notes, median pitch D5)
  track '2023 Drum Kit' prog 0 -> drums (586 notes, median pitch C2)
  track '2023 Drum Kit' prog 0 -> drums (58 notes, median pitch Bb1)
  track '8-Bit Square' prog 80 -> lead2 (49 notes, median pitch D5)
  track 'Lucent Choir' prog 52 -> chords (72 notes, median pitch A3)
  track 'Scifi' prog 103 -> lead (1427 notes, median pitch Bb4)
  track '2013 Drum Kit' prog 0 -> drums (546 notes, median pitch D2)
  track 'Smooth Synth' prog 90 -> chords (652 notes, median pitch A4)
  track 'Synth Bass' prog 39 -> bass (2357 notes, median pitch D4)
  track 'Steel Drums' prog 114 -> lead2 (160 notes, median pitch A4)
  track '909 Drum Kit' prog 0 -> drums (2474 notes, median pitch B2)
  track 'Electric Drum Kit' prog 118 -> drums (633 notes, median pitch A4)
  track 'Slap Bass' prog 36 -> bass (702 notes, median pitch Bb3)
  track 'Music Box' prog 10 -> chords (232 notes, median pitch Bb4)
  track 'Synth Bass (Classic)' prog 38 -> bass (963 notes, median pitch Bb2)
  track '8-Bit Sine' prog 80 -> chords (384 notes, median pitch A3)
  track 'Drum Kit' prog 0 -> drums (640 notes, median pitch B5)
  track 'Elec. Piano (Classic)' prog 4 -> lead2 (14 notes, median pitch F#4)
  track 'Smooth Synth (Classic)' prog 90 -> other (224 notes, median pitch F3)
  track 'Harpsichord' prog 6 -> lead2 (48 notes, median pitch Bb4)
  track 'Acoustic Gtr (Classic)' prog 25 -> bass (52 notes, median pitch C2)
  track 'Ragtime Piano' prog 3 -> lead2 (50 notes, median pitch Ab4)
  track '8-Bit Sawtooth' prog 81 -> other (49 notes, median pitch Ab3)
```
       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       | 
drums  .................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX...XXX.......XXXXXXXXXXXXXXX..XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX......
bass   ..............................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX...XXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.....
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   ...............XXXXXXXXX................XXXXXXXXX.....XXX..........XXXXX...XXXXXXXXXXXX...X...XXX...XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..........................
lead2  ......................XXXXXXXX..XXXXXXXXXXXXXX..................................................XXXX...........................X.X..........XX.XXXX....XXXXX......
other  .................XXXXXX................................................................................................XXXXX......XXXXXXXXXXXXXXXXX.....XXXX......
```
sections, 4-bar blocks (start bar : length : roles): 1:16:C; 17:4:D+C+L+O; 21:4:D+C+L+L2+O; 25:4:D+C+L2; 29:12:D+B+C+L2; 41:8:D+B+C+L+L2; 49:4:D+B+C; 53:4:D+B+C+L; 57:12:D+B+C; 69:4:B+C+L; 73:4:D+B+C; 77:4:B+C+L; 81:8:D+B+C+L; 89:4:D+B+C; 93:4:D+B+C+L; 97:4:D+B+C+L2; 101:4:D+C+L; 105:16:D+B+C+L; 121:4:D+B+C+L+O; 125:4:D+B+C+L; 129:8:D+B+C+L+O; 137:4:D+B+C+O; 141:8:D+B+C+L2+O; 149:4:D+B+C; 153:4:D+B+C+L2+O; 157:6:C
- drums enter bar 18, drum dropouts (start bar,len): [(70, 3), (76, 7), (98, 2)], bars after drums stop: 6
- harmonic loop period: None bars; lead 4-bar phrases: `---NNN----NNNNN-NNNNNNNN-NvNNNNNNN-------` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): tomM2 `X.XXX..X.XXXX..X` vel 1.0±0.01; tomH2 `X.XXX..X.XXXX..X` vel 1.0±0.02; 83 `XXXXXXXXXXXXXXXX` vel 0.95±0.06; kick `XXXXXXXXXXXXXXXX` vel 0.91±0.21; snare `X.X.X...X.X.X...` vel 1.0±0.0
- off-grid onsets 72%; swing8 0.574; swing16 0.265

#### OS #5491540 "Horizon - Nujabes" (https://onlinesequencer.net/5491540)
tempo 99 BPM, 4/4, 40 bars, mean active roles/bar 3.4
  track 'Electric Drum Kit' prog 118 -> drums (79 notes, median pitch Bb2)
  track 'Electric Drum Kit' prog 118 -> drums (561 notes, median pitch F#2)
  track 'Electric Drum Kit' prog 118 -> drums (115 notes, median pitch E2)
  track 'Electric Drum Kit' prog 118 -> drums (221 notes, median pitch C2)
  track 'Grand Piano' prog 0 -> chords (816 notes, median pitch D4)
  track 'Bass Guitar' prog 33 -> bass (228 notes, median pitch E2)
  track 'Strings' prog 48 -> pad (40 notes, median pitch E3)
  track 'Violin' prog 40 -> chords (92 notes, median pitch A4)
```
       |       |       |       |       |       
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   ........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
pad    ................XXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:8:D+C; 9:8:D+B+C; 17:24:D+B+C+P
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 4 bars; lead 4-bar phrases: `n/a` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `xXXXXXXXXXXXxXXX` vel 1.0±0.0; kick `X.xx...Xxx.X...X` vel 1.0±0.0; snare2 `....X....x..X.xx` vel 1.0±0.0; openhat `x.x......x..X...` vel 1.0±0.0; snare `............X...` vel 1.0±0.0
- off-grid onsets 0%; swing8 0.653; swing16 0.25

#### OS #5499741 "Manx HD XD - Master Audience Lofi" (https://onlinesequencer.net/5499741)
tempo 149 BPM, 4/4, 132 bars, mean active roles/bar 3.39
  track '8-Bit Triangle' prog 80 -> chords (672 notes, median pitch C#3)
  track 'Smooth Synth (Classic)' prog 90 -> chords (768 notes, median pitch C#3)
  track '808 Drum Kit' prog 0 -> drums (1772 notes, median pitch F#2)
  track 'Grand Piano (Classic)' prog 0 -> chords (1200 notes, median pitch G4)
  track 'Ragtime Piano' prog 3 -> chords (1200 notes, median pitch G4)
  track 'Electric Guitar' prog 27 -> lead (576 notes, median pitch D4)
  track '808 Bass' prog 38 -> bass (352 notes, median pitch C3)
  track '2013 Drum Kit' prog 0 -> drums (16 notes, median pitch C#3)
  track 'Elec. Piano (Classic)' prog 4 -> chords (896 notes, median pitch F4)
  track 'Synth Pluck (Classic)' prog 84 -> chords (1792 notes, median pitch F4)
```
       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |   
drums  XXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   .....XXXXXXX.X.X.X.XXXXXXXXXXXXXX.....XXXXXXX.X.X.X.XXXXXXXXXXXXXX.....XXXXXXX.X.X.X.XXXXXXXXXXXXXX.....XXXXXXX.X.X.X.XXXXXXXXXXXXXX
chords XXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   XXXX.XXXXXX........XXXXXXXXXXXXXXXXXX.XXXXXX........XXXXXXXXXXXXXXXXXX.XXXXXX........XXXXXXXXXXXXXXXXXX.XXXXXX........XXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:4:D+C+L; 5:8:D+B+C+L; 13:8:D+B+C; 21:12:D+B+C+L; 33:4:D+C+L; 37:8:D+B+C+L; 45:8:D+B+C; 53:16:D+B+C+L; 69:4:D+C+L; 73:4:D+B+C+L; 77:8:D+B+C; 85:16:D+B+C+L; 101:4:D+C+L; 105:8:D+B+C+L; 113:4:D+B+C; 117:16:D+B+C+L
- drums enter bar 1, drum dropouts (start bar,len): [(5, 1), (38, 1), (71, 1), (104, 1)], bars after drums stop: 0
- harmonic loop period: 2 bars; lead 4-bar phrases: `NNv-NvRRRNR--RRRRNRN-RRRRRRv-vRRR-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `XXX.X.X.X.X.X.X.` vel 1.0±0.0; openhat `X.......X.......` vel 1.0±0.0; clap `........X.......` vel 1.0±0.0; snare `..x...........x.` vel 1.0±0.0; kick `X...............` vel 1.0±0.0
- off-grid onsets 1%; swing8 0.5; swing16 0.208

#### OS #5505369 "groovy lofi" (https://onlinesequencer.net/5505369)
tempo 130 BPM, 4/4, 112 bars, mean active roles/bar 2.36
  track 'Scifi' prog 103 -> other (316 notes, median pitch E3)
  track 'Pop Synth' prog 90 -> chords (80 notes, median pitch E3)
  track '909 Drum Kit' prog 0 -> drums (2028 notes, median pitch F#2)
  track 'Synth Bass' prog 39 -> bass (1168 notes, median pitch G3)
```
       |       |       |       |       |       |       |       |       |       |       |       |       |       |       
drums  ................................XXX.XXX.XXX.XXXXXXXXXXXXXXXXXXXX................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   ................................XXX.XXX.XXX.XXX.XXX.XXX.XXX.XXX.................XXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXX
chords ........XXXXXXXXXXXXXXXXXXXXXXX.XXX.XXX.XXX.XXX.XXX.XXX.XXX.XXX.........XXXXXX..................XXXXXXXXXXXXXXXX
other  X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.X.XXX.XXX.XXX.XXX.XXX.XXX.XXX.XXX.X.X.X.X.X.X.X.X.XXXXXXXXXXXXXXX.XXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:8:O; 9:24:C+O; 33:32:D+B+C+O; 65:8:O; 73:8:C+O; 81:16:D+B+O; 97:16:D+B+C+O
- drums enter bar 33, drum dropouts (start bar,len): [(36, 1), (40, 1), (44, 1), (65, 16)], bars after drums stop: 0
- harmonic loop period: 1 bars; lead 4-bar phrases: `n/a` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `XXXXXXXXXXXXXXXX` vel 1.0±0.0; kick2 `X...xXX.x.XxxXx.` vel 1.0±0.0; openhat `..X...X...X.x.X.` vel 1.0±0.0; clap `....X.......X...` vel 1.0±0.0; crash `X.......X..X..X.` vel 1.0±0.0
- off-grid onsets 0%; swing8 0.598; swing16 0.25

#### OS #5528187 "Dave Brubeck - Take Five(lofi remix)" (https://onlinesequencer.net/5528187)
tempo 150 BPM, 5/4, 117 bars, mean active roles/bar 4.2
  track 'Rhodes' prog 2 -> lead (465 notes, median pitch F#4)
  track '8-Bit Sine' prog 80 -> chords (1386 notes, median pitch Eb3)
  track 'Pizzicato' prog 45 -> bass (305 notes, median pitch Eb1)
  track 'Electric Drum Kit' prog 118 -> drums (1289 notes, median pitch Eb3)
  track 'Jazz Guitar' prog 26 -> chords (1386 notes, median pitch Eb3)
  track 'Brass' prog 61 -> lead2 (465 notes, median pitch F#4)
```
       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |    
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX........................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   ....XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords ....XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   ...........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..........XX..........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead2  ...........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX..........XX..........XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:4:D; 5:8:D+B+C; 13:48:D+B+C+L+L2; 61:24:B+C; 85:33:D+B+C+L+L2
- drums enter bar 1, drum dropouts (start bar,len): [(61, 24)], bars after drums stop: 0
- harmonic loop period: 1 bars; lead 4-bar phrases: `--NNNNvRvNNNNNNN-RR-RRRRRRRNN-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): ride `X...X.X.X...X.X.X...` vel 0.5±0.01; snare `X.....X.....X.....XX` vel 0.5±0.01; kick2 `X...................` vel 0.5±0.01
- off-grid onsets 31%; swing8 0.593; swing16 None

#### OS #5741356 "Take Five(lofi remix)" (https://onlinesequencer.net/5741356)
tempo 140 BPM, 5/4, 106 bars, mean active roles/bar 4.25
  track 'Electric Drum Kit' prog 118 -> drums (1218 notes, median pitch A2)
  track 'Bass Guitar' prog 33 -> bass (1460 notes, median pitch F#2)
  track 'Jazz Guitar' prog 26 -> other (309 notes, median pitch F3)
  track 'Rhodes' prog 2 -> lead (971 notes, median pitch Ab4)
  track '8-Bit Triangle' prog 80 -> other (143 notes, median pitch Ab3)
  track 'Strings' prog 48 -> bass (182 notes, median pitch Eb2)
  track 'Scifi' prog 103 -> other (119 notes, median pitch C#3)
  track 'Church Organ' prog 19 -> lead2 (166 notes, median pitch C#6)
```
      |       |       |       |       |       |       |       |       |       |       |       |       |       | 
drums XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass  XXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.X.X.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
lead  XXXX....XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.X.X.XX....XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.X.
lead2 ..XX.............XXXXXXXX........XXXX...........XXXXXXX..XXX.X.X.XX....XXXXXXXXXXXXXXXXXXXXX............X.
other XXXX.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.X.X.XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
```
sections, 4-bar blocks (start bar : length : roles): 1:4:D+B+L+L2+O; 5:4:D+B+O; 9:8:D+B+L+O; 17:8:D+B+L+L2+O; 25:8:D+B+L+O; 33:4:D+B+L+L2+O; 37:12:D+B+L+O; 49:20:D+B+L+L2+O; 69:4:D+B+O; 73:20:D+B+L+L2+O; 93:12:D+B+L+O; 105:2:D+B+L+L2+O
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `N-NvNNvvNNNNNNNNNNvvvNNNNNN` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): ride `X...X...X...X...X..x` vel 1.0±0.0; snare2 `...X...xX.......X...` vel 1.0±0.0; kick `X......X....X.......` vel 1.0±0.0; tomH `Xx.Xx..xx...........` vel 1.0±0.0; tomM `.X..Xx.Xx..X........` vel 1.0±0.0
- off-grid onsets 56%; swing8 0.659; swing16 0.305

#### OS #5742849 "Megalo-Strike Back (LoFi ver.) - Original" (https://onlinesequencer.net/5742849)
tempo 90 BPM, 4/4, 61 bars, mean active roles/bar 3.36
  track 'Grand Piano' prog 0 -> lead (1117 notes, median pitch E5)
  track '909 Drum Kit' prog 0 -> drums (268 notes, median pitch D2)
  track '808 Drum Kit' prog 0 -> drums (176 notes, median pitch B1)
  track 'Smooth Synth (Classic)' prog 90 -> lead2 (8 notes, median pitch F#4)
  track 'Flute (Classic)' prog 73 -> lead2 (126 notes, median pitch F#4)
  track '8-Bit Sine' prog 80 -> bass (48 notes, median pitch Bb1)
```
      |       |       |       |       |       |       |       |    
drums ....XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
bass  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.............
lead  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead2 .XX..XX.XXXXXXXXXXXXXXXX.XX..XX.XXXXXXXXXXXXXXXX.............
```
sections, 4-bar blocks (start bar : length : roles): 1:4:B+L+L2; 5:44:D+B+L+L2; 49:12:D+L; 61:1:L
- drums enter bar 5, drum dropouts (start bar,len): [], bars after drums stop: 1
- harmonic loop period: 4 bars; lead 4-bar phrases: `NRNvNvRRvvRRNvNN` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): kick2 `X.X.....X.X.....` vel 1.0±0.0; snare `x...X...X...X...` vel 1.0±0.0; clap `....X.......X...` vel 1.0±0.0
- off-grid onsets 2%; swing8 0.5; swing16 None

#### OS #5766696 "Lofi Highlands" (https://onlinesequencer.net/5766696)
tempo 100 BPM, 4/4, 33 bars, mean active roles/bar 3.33
  track 'EDM Kit (E)' prog 0 -> drums (64 notes, median pitch Eb2)
  track '2023 Drum Kit' prog 0 -> drums (160 notes, median pitch F1)
  track 'Rhodes' prog 2 -> lead (168 notes, median pitch A4)
  track '909 Drum Kit' prog 0 -> drums (320 notes, median pitch F#2)
  track '8-Bit Sine' prog 80 -> lead2 (132 notes, median pitch G4)
  track '8-Bit Sine' prog 80 -> other (72 notes, median pitch Eb3)
```
      |       |       |       |       |
drums XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
lead  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.
lead2 XXXXXXXXXXXXXXXXXX.XXXXXXX.XXXXX.
other ........XXXXXXXX...XX..XX..XX..XX
```
sections, 4-bar blocks (start bar : length : roles): 1:8:D+L+L2; 9:8:D+L+L2+O; 17:4:D+L+L2; 21:12:D+L+L2+O; 33:1:O
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 1
- harmonic loop period: 4 bars; lead 4-bar phrases: `NRRRRRRR-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X.XXX.X.XXX.X.X.` vel 1.0±0.0; 29 `X.....xxxxX...X.` vel 1.0±0.0; clap `....X.......X...` vel 1.0±0.0
- off-grid onsets 17%; swing8 0.5; swing16 0.333

#### OS #5831374 "My Castle Town (LOFI COVER)" (https://onlinesequencer.net/5831374)
tempo 135 BPM, 3/4, 96 bars, mean active roles/bar 2.48
  track 'Rhodes' prog 2 -> chords (828 notes, median pitch F4)
  track 'Strings' prog 48 -> pad (31 notes, median pitch F4)
  track 'Flute (Classic)' prog 73 -> lead2 (22 notes, median pitch D5)
  track 'Grand Piano (Classic)' prog 0 -> lead (38 notes, median pitch Eb4)
  track '2023 Drum Kit' prog 0 -> drums (1182 notes, median pitch G1)
```
       |       |       |       |       |       |       |       |       |       |       |       |       
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
chords XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
pad    ...............................XXXXXXXXXXXXXXXXX................................................
lead   ...............................................................................XXXXXXXXXXXXX....
lead2  ................................................................XXXXXXXXXXXXXXXX................
```
sections, 4-bar blocks (start bar : length : roles): 1:32:D+C; 33:16:D+C+P; 49:16:D+C; 65:16:D+C+L2; 81:12:D+C+L; 93:4:D+C
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `-------------------NNvv--` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): 28 `X..X..X...X.` vel 1.0±0.0; hat `X...X...X.X.` vel 1.0±0.0; 31 `....X...X...` vel 1.0±0.0; 32 `....X...X...` vel 1.0±0.0; 27 `X...........` vel 1.0±0.0
- off-grid onsets 2%; swing8 0.562; swing16 None

#### OS #5833871 "hi lofi" (https://onlinesequencer.net/5833871)
tempo 120 BPM, 3/4, 79 bars, mean active roles/bar 4.19
  track '2013 Drum Kit' prog 0 -> drums (47 notes, median pitch F2)
  track 'Rock Kit' prog -1 -> drums (47 notes, median pitch Ab1)
  track 'Rock Kit' prog -1 -> drums (190 notes, median pitch Ab2)
  track 'Rock Kit' prog -1 -> drums (430 notes, median pitch A2)
  track 'Rock Kit' prog -1 -> drums (566 notes, median pitch F#4)
  track '2013 Drum Kit' prog 0 -> drums (148 notes, median pitch D2)
  track 'EDM Kit (E)' prog 0 -> drums (249 notes, median pitch F#2)
  track 'Grand Piano' prog 0 -> chords (460 notes, median pitch F#3)
  track 'Upright Piano' prog 0 -> chords (464 notes, median pitch G3)
  track 'Release-Cut Piano' prog 0 -> chords (466 notes, median pitch G3)
  track 'Steel Drums' prog 114 -> chords (464 notes, median pitch G3)
  track 'Pizzicato' prog 45 -> bass (164 notes, median pitch C#2)
  track 'Alto Sax' prog 66 -> lead2 (120 notes, median pitch Ab4)
  track 'Alto Sax' prog 66 -> lead2 (120 notes, median pitch Ab4)
  track 'Music Box' prog 10 -> chords (466 notes, median pitch G3)
  track 'Electric Drum Kit' prog 118 -> drums (251 notes, median pitch E2)
  track '8-Bit Sawtooth' prog 81 -> chords (164 notes, median pitch F#3)
  track 'Flute' prog 73 -> other (144 notes, median pitch G2)
  track 'Lucent Choir' prog 52 -> other (140 notes, median pitch F#2)
  track 'Choir Ahhs' prog 52 -> lead2 (140 notes, median pitch F#4)
  track '808 Drum Kit' prog 0 -> drums (34 notes, median pitch B1)
  track 'Synth Pluck' prog 84 -> lead2 (180 notes, median pitch F#5)
  track 'Violin (Classic)' prog 40 -> chords (8 notes, median pitch Ab4)
  track 'Violin (Classic)' prog 40 -> chords (140 notes, median pitch F#4)
  track 'Cello' prog 42 -> bass (28 notes, median pitch Ab2)
  track 'Cello' prog 42 -> bass (30 notes, median pitch Ab2)
  track 'Trombone (Classic)' prog 57 -> bass (26 notes, median pitch Ab2)
  track 'Lucent Choir' prog 52 -> bass (34 notes, median pitch Bb1)
  track 'Electric Guitar' prog 27 -> lead2 (58 notes, median pitch B4)
  track 'Trombone (Classic)' prog 57 -> bass (8 notes, median pitch F2)
  track 'Lucent Choir' prog 52 -> bass (4 notes, median pitch Ab1)
  track 'Rhodes' prog 2 -> lead (273 notes, median pitch Bb3)
  track '2013 Drum Kit' prog 0 -> drums (32 notes, median pitch B1)
  track 'Electric Drum Kit' prog 118 -> drums (32 notes, median pitch C#2)
  track 'Electric Drum Kit' prog 118 -> drums (32 notes, median pitch B1)
  track 'Ragtime Piano' prog 3 -> lead2 (108 notes, median pitch C#4)
```
       |       |       |       |       |       |       |       |       |       |      
drums  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass   ......XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.X
chords ......XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead   .............................................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead2  ............XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX......................XXXXXXXXXXXXXX
other  ..........................XXXXXXXXXXXXXX.........................XXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:4:D; 5:8:D+B+C; 13:12:D+B+C+L2; 25:16:D+B+C+L2+O; 41:4:D+B+C+L2; 45:20:D+B+C+L; 65:15:D+B+C+L+L2+O
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: None bars; lead 4-bar phrases: `-----------NNNNNNNNN` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): pedalhat `x.xxx.xXx.xx` vel 0.36±0.24; 66 `X..xX..xX..x` vel 0.83±0.17; snare2 `...x.x.x.x.x` vel 0.27±0.18; hat `...x.x.x.x.x` vel 0.25±0.15; 65 `X...X...X...` vel 0.3±0.0
- off-grid onsets 62%; swing8 0.649; swing16 0.316

#### OS #5834718 "Agora sim um Lofi" (https://onlinesequencer.net/5834718)
tempo 70 BPM, 4/4, 72 bars, mean active roles/bar 3.0
  track 'Rhodes' prog 2 -> lead (459 notes, median pitch C5)
  track 'EDM Kit (E)' prog 0 -> drums (144 notes, median pitch Eb2)
  track '808 Drum Kit' prog 0 -> drums (848 notes, median pitch F#2)
  track 'Bass Guitar (Classic)' prog 33 -> bass (144 notes, median pitch E1)
  track 'Electric Drum Kit' prog 118 -> drums (83 notes, median pitch C#6)
```
      |       |       |       |       |       |       |       |       |       
drums XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
bass  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
lead  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
```
sections, 4-bar blocks (start bar : length : roles): 1:72:D+B+L
- drums enter bar 1, drum dropouts (start bar,len): [], bars after drums stop: 0
- harmonic loop period: 8 bars; lead 4-bar phrases: `NNRRRRRRRRRRRRRRRR-` (N new, R exact repeat, v variant, - silent)
- drum 16-step grid (X >=60% of bars, x >=25%): hat `X.X.X.X.X.X.X.X.` vel 1.0±0.0; kick2 `X.......X.X.....` vel 1.0±0.0; clap `....X.......X...` vel 1.0±0.0; 85 `....X.......X...` vel 1.0±0.0; 29 `........X...X...` vel 1.0±0.0
- off-grid onsets 18%; swing8 0.5; swing16 None

