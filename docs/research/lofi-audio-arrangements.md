# Lo-fi hip hop arrangements measured from audio

> Source material, kept as researched. The distilled, current reading (general musicianship vs. lo-fi choices) is in [`docs/music/`](../music/README.md).

Companion to [lofi-hip-hop.md](lofi-hip-hop.md), which found no published bar-by-bar arrangement maps. This report measures them from the audio of 19 freely licensed lo-fi tracks: when the drums, bass, keys and top line come in and drop out, how intros, breaks and outros work, and how long each section runs in bars.

Labels used throughout:
- **[measured]**: comes straight from the analysis output.
- **[inferred]**: my interpretation of the measurements, or a design recommendation.
- **[uncertain]**: the measurement exists but the method is weak at that point (see section 6).

## 1. Summary

**The typical track [measured, n = 19].**
- **Size:** 80–90 BPM (median 85), about 2:40 long (median 158 s), and about 53 bars of music. The range is 28–88 bars.
- **Harmony:** one chord loop runs the entire track. It is 4 bars long in 11 of the 17 tracks where loop length could be measured, 8 bars in 3, and 1–2 bars (a vamp) in 3. **No track has a B section with a new progression.** The only chroma that differs from the loop is in intros and outros, where the voicing is different or filtered.
- **Intro:** the median is 4 bars. Exactly 4 bars in 8 tracks, about 8 bars in 5, 0–1 bar in 4, and 11–12 bars in 2. The intro is almost always the keys loop alone, often darker (low-passed) than the main section. Four tracks open with a bar of noise or a fade-in first.
- **Drum entry:** the drums usually arrive as a full kit on a phrase boundary, and the bass arrives in the same bar (10 tracks), or 1–2 bars later (3 tracks). A hat or shaker layer is often held back and added 4–8 bars after the kick and snare (6 tracks). Three tracks bring in snare and hats without a kick for 4–8 bars before the full kit.
- **Main body:** drums play in about 80% of bars (median per track). Drums + bass + keys is the steady state: 68% of all audible bars carry exactly these three roles.
- **Variation without new harmony:**
  - The hat layer drops out for 4–8 bars (8 tracks).
  - Kick and bass drop out for about 8 bars while the snare and hats keep going (3 tracks).
  - One full drum break of about 4 bars, with the keys continuing, falls 16–28 bars after the drums enter (5 of 18 tracks).
  - Short 3-beat to 1-bar drum gaps fall at the ends of phrases (about 5 tracks).
  - **7 of 18 tracks have no drum drop at all** between drum entry and outro.
- **Outro:** in 12 of 19 tracks the drums stop before the music ends, leaving a tonal tail of about 4–10 bars (median about 6), usually the keys alone, which mirrors the intro. In the other 7 the drums play to the end. Six tracks fade over 3 or more bars. Most of the rest ring out within 1–2 bars, and one stops hard.
- **Tape stop:** none found, either at endings or before breaks. The detector was checked against a synthetic tape stop, and it caught it.
- **Phrase lengths:** layer changes cluster at 4, 8 and 16 bars. On the bar grid they measure as 3–4, 7–8 and 15 bars, because the drums usually hit beat 1 of the first "off" bar before stopping. That beat makes the detector count the bar as "on".

**Confidence.** Tempo is solid for 17 of 19 tracks (integer BPM, beat grid stable to about 7 ms). Drum, bass and keys presence per bar is reasonably reliable. Bar *phase* (which beat is "1") is uncertain in about half the tracks, so section boundaries can be off by ±1 bar. Lead detection is weak. Bass and the piano left hand cannot be separated. Two tracks are low-confidence throughout (see §6).

## 2. Sources

All files were downloaded only to the session scratchpad, converted to 22.05 kHz mono WAV with `afconvert`, and never executed. FMA's "download" button needs a login, so I fetched each file through the site's public stream URL (`/track/<handle>/stream/`). That returns the same MP3 file the player uses, and the licenses below permit copying.

| # | Artist | Title | License | Source URL | BPM [measured] | Length |
|---|---|---|---|---|---|---|
| 1 | Purrple Cat | Bookworm | CC BY 4.0 | https://freemusicarchive.org/music/purrple-cat/a-good-book/bookworm/ | 85 | 3:13 |
| 2 | Purrple Cat | Coffee Jelly | CC BY 4.0 | https://freemusicarchive.org/music/purrple-cat/a-good-book/coffee-jelly/ | 90 | 2:09 |
| 3 | Purrple Cat | Another Dimension | CC BY 4.0 | https://freemusicarchive.org/music/purrple-cat/a-good-book/another-dimension/ | 65 | 1:59 |
| 4 | Purrple Cat | Summer Crush | CC BY 4.0 | https://freemusicarchive.org/music/purrple-cat/a-good-book/summer-crush-1/ | 70 | 2:21 |
| 5 | Purrple Cat | A Short Walk With You | CC BY-SA 4.0 | https://freemusicarchive.org/music/purrple-cat/up-all-night/a-short-walk-with-you/ | 116 (or 58 half-time) | 1:31 |
| 6 | Purrple Cat | Cold & Rainy | CC BY-SA 4.0 | https://freemusicarchive.org/music/purrple-cat/up-all-night/cold-rainy/ | 90 | 2:43 |
| 7 | Purrple Cat | Up All Night | CC BY-SA 4.0 | https://freemusicarchive.org/music/purrple-cat/up-all-night/up-all-night/ | 100 | 1:08 |
| 8 | HoliznaCC0 | Roof Tops | CC0 1.0 | https://freemusicarchive.org/music/holiznacc0/summer-air-lo-fi/roof-tops/ | 80 | 3:34 |
| 9 | HoliznaCC0 | Summer Felt Longer Then | CC0 1.0 | https://freemusicarchive.org/music/holiznacc0/summer-air-lo-fi/summer-felt-longer-then/ | 80 | 3:48 |
| 10 | HoliznaCC0 | August Never Lasts | CC0 1.0 | https://freemusicarchive.org/music/holiznacc0/summer-air-lo-fi/august-never-lasts/ | 85 | 2:38 |
| 11 | HoliznaCC0 | Ocean Memory | CC0 1.0 | https://freemusicarchive.org/music/holiznacc0/ocean-memory-lo-fi-chill/ocean-memory/ | 67 | 2:31 |
| 12 | HoliznaCC0 | Washed Up | CC0 1.0 | https://freemusicarchive.org/music/holiznacc0/ocean-memory-lo-fi-chill/washed-up/ | 90 | 3:55 |
| 13 | HoliznaCC0 | Still Life (Lofi, Chill, Nostalgic) | CC0 1.0 | https://freemusicarchive.org/music/holiznacc0/public-domain-lofi/still-life-lofi-chill-nostalgic/ | 76.5 | 2:15 |
| 14 | HoliznaCC0 | Tokyo Sunset (Lofi, Peaceful, Soft) | CC0 1.0 | https://freemusicarchive.org/music/holiznacc0/public-domain-lofi/tokyo-sunset-lofi-peaceful-soft/ | 73.5 [uncertain] | 2:19 |
| 15 | Grumpynora | Combo Platter | CC BY-NC-ND 3.0 | https://archive.org/details/jamendo-367882 | 70 | 3:09 |
| 16 | Grumpynora | Sunday Driver | CC BY-NC-ND 3.0 | https://archive.org/details/jamendo-370054 | 82 | 4:20 |
| 17 | Kolarbeatz | Sun | CC BY-NC-ND 3.0 | https://archive.org/details/jamendo-385396 | 90 | 2:43 |
| 18 | lesfm | Chilling Ego | CC BY-NC-ND 3.0 | https://archive.org/details/jamendo-460429 | 94 | 2:48 |
| 19 | M.R.K | No Stress | CC BY-SA 3.0 | https://archive.org/details/jamendo-292516 | 88 | 2:16 |

Album licenses were read from the FMA album pages (Purrple Cat *A Good Book*: CC BY 4.0; *Up All Night*: CC BY-SA 4.0; all three HoliznaCC0 albums: CC0) and from the `licenseurl` metadata of the archive.org items, which are Jamendo mirrors.

**Selection and representativeness [inferred].** Purrple Cat and HoliznaCC0 are prolific lo-fi producers who publish under Creative Commons. Their tracks are tagged Lo-fi Hip-Hop on FMA, sit at 65–100 BPM, and analyze as keys loop + boom-bap drums + bass + vinyl, which matches the Lofi Girl and Chillhop style. The five Jamendo tracks are tagged hiphop + lofi/chillhop. Some Jamendo producers write library or "vlog" music, so their arrangement habits may differ from label releases. I did not analyze any Lofi Girl or Chillhop label releases, because they aren't freely licensed. Treat these results as "how freely licensed lo-fi is built". The style match is close, but the sample is small and comes from only 7 producers (Purrple Cat contributes 7 tracks and HoliznaCC0 7).

## 3. Method

Scripts are in `/private/tmp/claude-501/-Users-samueloltz-Documents-fm-1/dfd13d60-fb03-409f-86fe-9daa1a019026/scratchpad/audiotools/` (`features.py`, `classify.py`, `aggregate.py`, `beatzoom.py`, `tapestop.py`, `gridcheck.py`, `pattern.py`). They run in a Python 3.14 venv with numpy, scipy and librosa 0.11, and were invoked with `python -I`. The scratchpad is temporary. Copy the scripts into the repo if you want to rerun the analysis.

1. **Separation.** STFT (2048 points, hop 512 at 22.05 kHz), then harmonic/percussive separation (HPSS) so that tonal roles and drums are measured separately.
2. **Tempo and beat grid.**
   - librosa's beat tracker on the percussive onset envelope gives the initial estimate.
   - A brute-force search over period (±3%) and phase then finds the constant grid that maximizes onset strength on the beats. An integer BPM is used when it scores within 3% of the best.
   - The grid was verified per 15 s window. 17 of 19 tracks snapped to integer BPMs (65, 67, 70, 80, 82, 85, 88, 90, 94, 100), with median beat-tracker residuals of 6–10 ms, which is consistent with DAW-produced, fixed-tempo tracks.
3. **Bar phase (downbeat).** The phase is set from the beat on which the drums first enter after a drumless intro (12 tracks). Producers almost always bring the kit in on the "1". Where there is no such entry (7 tracks), it comes from a combined score: kick on 1 and 3 versus snare on 2 and 4, chroma change at bar starts, and how sharp the bar-to-bar energy steps are.
4. **Per-bar features.** Band energies, all in dB relative to the track's own 90th percentile:
   - Percussive: sub/kick 30–100 Hz, snare 1.2–5 kHz, hats/air 6–11 kHz.
   - Harmonic: sub 30–100 Hz, bass 60–250 Hz, keys 250–1500 Hz, presence 1.5–5 kHz, air 6–11 kHz.
   - Also onset count, spectral flux, flatness, harmonic roll-off, chroma (CQT on the harmonic part), and the beat-periodicity of hat and snare onsets.
5. **Role rules (per bar).**
   - **Kick:** percussive sub energy within 12 dB of the track's reference.
   - **Snare:** percussive mid energy within 10 dB of reference and periodic at 2 beats.
   - **Hats:** air-band transients within 10 dB of reference and periodic at the beat.
   - **Drums:** kick or snare.
   - **Bass:** harmonic 30–100 Hz energy within 12 dB of reference.
   - **Keys:** harmonic 250–1500 Hz within 12 dB of reference.
   - **Dark:** keys present but presence-to-keys balance at least 6 dB below the main section, a proxy for low-pass filtering.
   - **Lead/top line:** presence-band brightness at least 4 dB above both the main-section norm and the same bar position in other 8-bar cycles. This removes chord-voicing patterns from the loop.
   - **Noise:** audible, but no drums, bass or keys.
6. **Beat-resolution events.** Drum entries, exits and breaks were also measured per beat, which makes them independent of bar phase. Break lengths are given in beats/4.
7. **Repeat structure.**
   - **Loop length:** the smallest lag (1, 2, 4, 8 or 16 bars) at which bar chroma repeats, measured over drummed bars. Similarity at the chosen lag is 0.94–0.997.
   - **"New material":** a bar whose chroma matches no bar 1 or 2 loops earlier or later (cosine < 0.88).
8. **Tape stops.** I tracked the shift of the log-frequency spectrum frame by frame over the last 4 s before each ending and each drum break, looking for a downward glide. A synthetic tape stop applied to a Bookworm excerpt read −2.3 semitones; normal music reads 0.

## 4. Results

### 4.1 Length and tempo [measured]
| | Median | Range |
|---|---|---|
| Duration | 2:38 | 1:08–4:20 |
| Music length (beats/4) | 53 bars | 28–88 |
| Tempo | 85 BPM | 65–100 (plus one at 116 that is probably a 58 BPM half-time feel) |

Seventeen of the 19 tempos are integers. Still Life (76.5) and Tokyo Sunset (73.5) aren't, which points to sample-based or unquantized material [inferred].

### 4.2 Intro [measured unless marked]
| Intro length (beats/4 before drum entry) | Tracks |
|---|---|
| 0–1 bar (drums from the start) | Coffee Jelly, Another Dimension, Combo Platter (1 bar of drums before bass), Sunday Driver |
| 2–3 bars | Still Life (2.25), Summer Crush (3), Ocean Memory (3) |
| 4 bars | A Short Walk, Up All Night, Roof Tops, Summer Felt Longer Then, No Stress, Tokyo Sunset (4.5) |
| ≈8 bars | Bookworm (7.75), August Never Lasts (7.75), Cold & Rainy (8), Sun (8.5) |
| 11–12 bars | Washed Up (4 keys + 8 hats/snare without kick), Chilling Ego (8 keys + 4 drums without kick) |

**What plays in the intro.**
- **Keys alone:** the chord loop alone is the intro in 12 of the 15 tracks that have one.
- **Keys + low register:** 3 tracks (Bookworm, August Never Lasts, No Stress) [uncertain: this may be the piano's left hand rather than a separate bass].
- **Dark keys:** flagged in 9 intros. The presence band is weak relative to the keys band compared with the main section.
- **Not the main loop:** in Cold & Rainy, Summer Felt Longer Then, Washed Up, Ocean Memory and Roof Tops, the intro's chroma doesn't match the main loop. A different voicing or a filtered version is likely [inferred].
- **Noise or fade-in first:** Bookworm (about 3 bars of rising noise and keys), Ocean Memory, Washed Up and Combo Platter have a near-silent or noise-only bar before the keys.
- **No intros of noise alone longer than 1–3 bars.**

### 4.3 Drum and bass entry [measured]
- **Full kit at once (kick + snare in the same bar):** 15 of 19 tracks.
- **Kit without the kick first** (snare, rim and hats; the kick follows 4–8 bars later): Washed Up (8 bars), Chilling Ego (4), August Never Lasts (about 2).
- **Hat or shaker layer added after the kick and snare:** Coffee Jelly (+8), Cold & Rainy (+8), Bookworm (+8), Combo Platter (+5), Another Dimension (+4), Still Life (+4).
- **Bass entry relative to drums:**
  - Same bar: 10 tracks.
  - +1 to +2 bars: 3.
  - +4 to +8 bars: 3 (Still Life +4, Chilling Ego +8 with the kick, Washed Up +8 with the kick).
  - Before the drums: 3 [uncertain, probably the piano's low register].
- **Odd one out:** Combo Platter builds bottom-up. Drums come first, then bass (+2), hats (+5) and keys (+8).

### 4.4 Drum drop-outs and breaks [measured; lengths at beat resolution]

Tokyo Sunset is excluded; its drums are too sparse to classify.

| Type | Tracks | Length | Where |
|---|---|---|---|
| Full drum break, keys continue | Roof Tops, No Stress, Chilling Ego, Summer Crush, Sunday Driver | 2.75–3.75 bars measured ⇒ a 4-bar slot where the drums hit beat 1, then stop [inferred]. Sunday Driver about 2 bars. | 16.25, 20.25, 28.25 and 24.25 bars after drum entry; 30–70% of the way through the track |
| Bass during that break | stays in No Stress and Summer Crush; out in Roof Tops and Chilling Ego | | |
| Kick and bass out, snare/hats keep playing | August Never Lasts (bars 25–31), Summer Felt Longer Then (34–40), Sunday Driver (10–16) | 7–8 bars | mid-track |
| Hat layer out | Bookworm (2 × 8 bars), Sun (2 × 8), Combo Platter (2 × 4), Summer Felt Longer Then (several, 4–15), Still Life (4–6), Roof Tops (5), No Stress (4), Cold & Rainy | 4–8 bars | alternating with hat-on blocks of 8–16 bars |
| Short gaps (3 beats to 1 bar) | Summer Felt Longer Then (end of bar 16 and bar 56 after entry), Sun (bar 25), Ocean Memory, Still Life, Summer Crush | ≤1 bar | at the end of a 16-bar phrase where measurable |
| No drum drop between entry and outro | Coffee Jelly, Another Dimension, A Short Walk, Cold & Rainy, Up All Night, Washed Up (64 straight bars), Bookworm (full drums never stop; only the hats do) | | |

No track has more than one full multi-bar break.

### 4.5 Phrase grid [measured, then inferred]
- On the bar grid, layer-change lengths cluster at 3–4, 7–8 and 15 bars (36 events: 11 at 3–4, 6 at 7–8, 2 at 15; the rest are 1–2-bar fills and gaps).
- Beat-level zooms on the Roof Tops, No Stress and Chilling Ego breaks show the drums playing beat 1 (and sometimes beat 2) of the first break bar, then stopping, and coming back with a pickup on the last beat before the downbeat. So these are **4-bar breaks on a 4-bar grid**. The "−1 bar" is a detector artifact.
- Intros of 4 and 8 bars, tails of 4 and 8 bars, and hat blocks of 8 and 16 bars all point the same way: **sections are multiples of 4 bars, usually 8**. Where the bar phase is known, the starts of drum entries and breaks sit at multiples of 4 bars from the drum entry, with a one-beat pickup or a hit on the 1.

### 4.6 Outro [measured]
- **Drums stop before the end in 12 of 19 tracks.** Tonal tail at beat resolution: 0.5 (Ocean Memory), 2.75, 3.75, 5.5, 6.25, 7, 8, 8.25, 8.25, 8.75, 10, 10 bars. Median about 7. Values cluster near 4 and 8.
- **The tail is mostly the keys loop alone,** often darker, as in the intro. Bookworm keeps bass/low keys and a top line. Washed Up has a top line over the keys.
- **Drums play to the end in 7 of 19** (Coffee Jelly, Another Dimension, Combo Platter, Sunday Driver, No Stress, Roof Tops, Summer Crush).
- **How the level ends:**
  - Fade over 3 or more bars: 6 tracks (A Short Walk 7, Coffee Jelly 7, August Never Lasts 4, Another Dimension 3, Chilling Ego 3, Summer Felt Longer Then 3).
  - Short ring-out of 1–2 bars: 12.
  - Hard stop: 1 (Bookworm drops 41 dB within one bar after an 8-bar keys/bass tail).
- **Trailing near-silence after the music:** 0–9 s (median 3 s).
- **Tape stop:** none detected (0 of 19 endings, 0 of 6 pre-break points). **Filter close:** the "dark" flag appears in the last bars of Cold & Rainy, Up All Night, Tokyo Sunset and Sun's tail. That suggests a low-passed tail in about 4 tracks [uncertain].

### 4.7 How many roles play at once [measured]
Over all 1,046 audible bars (roles counted: drums, bass, keys, lead):

| Roles | Share of bars | Typical content |
|---|---|---|
| 0 | 2% | noise or silence lead-ins |
| 1 | 12% | keys alone (intros, tails, breaks) |
| 2 | 15% | keys + drums, or keys + bass |
| 3 | 68% | drums + bass + keys |
| 4 | 3% | + detected top line |

The per-track mean is 2.6 roles (range 1.7–3.0). Drums are present in a median 81% of a track's audible bars (range 38–98%).

### 4.8 Repeat structure [measured]
- **Loop length** (bar-chroma self-similarity): 4 bars in 11 tracks, 8 in 3 (August Never Lasts, Cold & Rainy, Washed Up), 2 in 1 (Another Dimension), and a static 1-bar vamp in 2 (Combo Platter, Summer Crush). Still Life matched best at 16 bars, a 4/16 tie. Up All Night and Tokyo Sunset couldn't be measured (too few drummed bars).
- **No A/B form.** In no track does the main section contain a run of 2 or more bars of new harmonic material. "New material" runs show up only in intros (Cold & Rainy, Washed Up, Summer Felt Longer Then, Summer Crush, Ocean Memory, Roof Tops), outros (Cold & Rainy, Washed Up, Summer Felt Longer Then), and Chilling Ego's 3-bar break.
- **Every track is one progression looped for its whole length. Variation comes from layers, not harmony.**

## 5. Sanity check: what a listener would hear [inferred from the features; I could not listen]

**Bookworm (Purrple Cat, 85 BPM).**
- **Bars 1–4:** near-silence that rises into hiss or vinyl-like high-frequency noise. The keys fade in.
- **Bars 5–8 (approximately):** the 4-bar keys loop with low notes, either piano left hand or soft bass.
- **About bar 9:** kick and snare enter together, with no busy hats yet.
- **8 bars later:** a hat or shaker layer arrives, and onset density doubles from about 5 to about 12 per bar.
- **Middle:** after 16 more bars the hat layer drops out for 8 bars while a brighter top line (a melody or a brighter voicing) appears every other bar. The hats then return for about 16 bars.
- **End:** the drums stop and the keys, low end and top line carry on for about 8 bars, then the track stops abruptly, followed by about 9 s of near-silence.
- **Doubts:** whether the "bass" is a separate instrument, whether the top line is a melody, and the exact downbeat (a ±1-beat error would move every boundary by a fraction of a bar).

**Roof Tops (HoliznaCC0, 80 BPM).**
- **Bars 1–4:** keys alone, with a soft attack on each downbeat, probably chord stabs.
- **Bar 5:** drums, bass and hats enter together for 16 bars.
- **First break:** the drums hit beat 1 of the next bar and drop out for 4 bars. The bass drops too, leaving the keys alone.
- **Second section:** everything returns for about 20 bars. Then comes an 8-bar stretch where the bass is gone and the keys sound darker, probably a low-pass filter, while the drums keep going.
- **Final section:** about 16 more bars of the full groove.
- **End:** the drums play almost to the end, and the keys ring out for 1–2 bars. This reads like a textbook "4 intro / 16 / 4 break / 20 / 8 filtered / 16 / 2 out" form.
- **Main doubt:** whether the dark section is a filter or a different instrument.

## 6. Limitations and confidence
- **Bar phase is the weakest link.**
  - The downbeat came from the first drum entry in 12 tracks. A beat-level look at three of them confirmed the entries and breaks land on 4-bar boundaries from there.
  - For the other 7 the structural score was weak; margins were small in about half the tracks.
  - A wrong phase moves section boundaries by a fraction of a bar and can turn a 4-bar event into "3" or "5" on the bar grid. Beat-level lengths (§4.4) avoid this.
- **Tempo octave.** A Short Walk With You measured 116 BPM. It may be felt at 58 (half-time). If so, its bar counts halve: intro 2 bars, about 21 bars total. Bookworm's snare-band onsets peak once per bar (on beat 3 of my grid), which could mean a half-time snare at 85 or a 170 BPM count. I kept 85.
- **Role separation is approximate.**
  - "Bass" means harmonic energy at 30–100 Hz. It can't separate a bass instrument from the piano's left hand, and it can miss a bass that sits mostly above 100 Hz.
  - "Kick" can be triggered by bass plucks (HPSS leaks attacks into the percussive part).
  - "Lead" only catches a top line that is brighter than the chords and not present all the time. A melody that plays through the whole track is invisible to it. Lead counts are therefore lower bounds.
- **The noise bed is only detectable where the music is quiet** (lead-ins, tails). I could not tell whether vinyl or rain runs under the full mix.
- **Two tracks are low-confidence.**
  - Tokyo Sunset: the grid matched in only 44% of windows, the drums are sparse, and snare and hats were never detected.
  - Sunday Driver: large level swings and a drum pattern that drops in and out bar by bar, possibly live-played or heavily sidechained.
  - Their grids are shown but excluded from the break statistics.
- **Fade versus "short release" is a threshold call** (a decline of 3 or more bars at more than 1 dB per bar counts as a fade). Tape-stop detection would miss stops shorter than about 0.3 s or buried under drums.
- **Sample bias.** Seven producers, 14 of 19 tracks from two of them, and no label releases. The "no B section" result in particular should be checked against Lofi Girl or Chillhop releases by ear before treating it as a rule.

## 7. Design implications for the 5-minute firmware [inferred]
This revises §8.2 of [lofi-hip-hop.md](lofi-hip-hop.md).

1. **One progression per "track arc".** Real tracks never change progression mid-song. A 5-minute song of 88–112 bars is about two real tracks back to back. So use **two arcs, each with its own loop** (4 bars, or 8 bars ≈ 25% of the time), joined by a transition. Don't use an A/B song form inside one arc.
2. **Arc template (about 50 bars), built from the measured medians:**
   - Intro: 4 bars of keys alone, darker than the main section (8 bars about 30% of the time; skip it about 20% of the time).
   - Drums + bass enter together on a 4-bar boundary. The hat layer comes in 4–8 bars later.
   - Main body: 32–40 bars with **layer toggles in 8-bar blocks**:
     - hats out for 8 (p ≈ 0.4 per arc);
     - kick + bass out for 8 while hats and snare keep going (p ≈ 0.15);
     - one full 4-bar drum break with the keys continuing, placed 16–28 bars after entry (p ≈ 0.3). Mute after beat 1 of the first break bar and come back with a 1-beat pickup;
     - a 3-beat drum gap at the end of a 16-bar phrase (p ≈ 0.3).
   - Tail: drums stop and the keys play alone for 4 or 8 bars (p ≈ 0.6), otherwise the drums run to the end. Then either a 3–7-bar fade (≈ 30%) or a 1–2-bar ring-out.
3. **Keep three layers on most of the time.** Drums + bass + keys covers about 70% of bars. Adding a 4th layer (lead) is rare in this sample.
4. **Tape stop and filter close are optional color, not the norm.** Neither appeared in this sample. A darker (low-passed) keys sound in intros and tails, however, is common.
5. **Transition between arcs:** the end of arc 1 (keys alone, 4–8 bars) leads into the intro of arc 2 (new keys loop alone, 4 bars). This reproduces what a listener hears between two tracks on a lo-fi playlist.

## 8. Per-track maps [measured]

**Grid legend.**
- One character per bar. The `bar` ruler marks the start of every 4-bar group with `|` and every 10th bar with its tens digit.
- `D` drums, `k` kick, `h` hats/shaker (periodic air-band transients), `B` low-register harmonic (bass or piano left hand), `C` keys/chords, `f` keys darker than the main section (filter proxy), `L` possible top line, `*` harmony that doesn't match the main loop.
- `level`: 0–9, in 3 dB steps below the track's peak bar.
- `.` means audible but the role is absent. A blank means silence.
- The first column can be a partial pickup bar.

### Purrple Cat – Bookworm (85 BPM)
```
bar   |   |   |1  |   |  2|   |   |3  |   |  4|   |   |5  |   |  6|   |   |7
drums  ........DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD.........    
kick   ........kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk.........    
hats   ................hhhhhhhhhhhhhhhh........hhhhhhhhhhhhhhhh.........    
bass   ...BBB.BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB    
keys   ...CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC    
dark   .......f...f...f.................................................    
lead?  .L..............................L.LLL..L................L.LLL..L.    
new    ................................................................*    
level  34566778888898888888888888889888988888888888988898889888888888886    
```

### Purrple Cat – Coffee Jelly (90 BPM)
```
bar   |   |   |1  |   |  2|   |   |3  |   |  4|   |   |5
drums DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD.  
kick  kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk.  
hats  ........hhhhhhhhhhhhhhhh..hhhhhhhhhhhhhhhhhhh...  
bass  .BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB...  
keys  .CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC...  
dark  ................................................  
lead? ................................................  
new   ................................................  
level 688888888888889888889888889888888888988888766430  
```

### Purrple Cat – Another Dimension (65 BPM)
```
bar   |   |   |1  |   |  2|   |   |3  |
drums DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD.
kick  kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk.
hats  ....hhhhhhhhhhhhhhhhhhhhhhhhhhhh.
bass  .BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB.
keys  .CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC.
dark  .ffff.f.f.f...........f.f.f.f.f..
lead? .........L.L..L...L......L.L.....
new   .................................
level 388888888889888889888888888988762
```

### Purrple Cat – Summer Crush (70 BPM)
```
bar   |   |   |1  |   |  2|   |   |3  |   |  4| 
drums  ...DDDDDDDDDDDDDDDDDDDDDDDD...DDDD.DD.D. 
kick   ...kkkkkkkkkkkkkkkkkkkkkkkk...kkkk...... 
hats   ...hhhhhhhhhhhhhhhhhhhhhhhh...hhhhhhhhh. 
bass   ...BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB. 
keys   CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC. 
dark   ffff...........f...........f.f.......... 
lead?  ....L...........L.........L............. 
new    ***........*..*........*..*........*..*. 
level  5678888998888888899888888888778888778873 
```

### Purrple Cat – A Short Walk With You (116 BPM grid; maybe 58 half-time)
```
bar   |   |   |1  |   |  2|   |   |3  |   |  4|   |
drums ....DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD....  
kick  ....kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk....  
hats  ....hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh....  
bass  ....BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB...  
keys  CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC...  
dark  ffffffffffff...............................  
lead? ...........................................  
new   ...*.......................................  
level 8777888988898888888888898888888888888765430  
```

### Purrple Cat – Cold & Rainy (90 BPM)
```
bar   |   |   |1  |   |  2|   |   |3  |   |  4|   |   |5  |   |  6| 
drums   ........DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD........    
kick    ........kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk........    
hats    ...............hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh........    
bass    ........BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB........    
keys    CCCCCCC.CCCCCCC.CCCCCCC.CCCCCCC.CCCCCCC.CCCCCCC.CCCCCCC.    
dark    fffffff.f.f.f...f.f.f...f.f.f...f.f.f...f.f.f...f.f.f...    
lead?   ........................................................    
new     ******..........................................*******.    
level   76757674988888888888888888889888988898889888888876767675    
```

### Purrple Cat – Up All Night (100 BPM)
```
bar   |   |   |1  |   |  2|   |   |
drums ....DDDDDDDDDDDDDDDD.........
kick  ....kkkkkkkkkkkkkkkk.........
hats  ....hhhhhhhhhhhhhhhh.........
bass  ....BBBBBBBBBBBBBBBB.........
keys  CCCCCCCCCCCCCCCCCCCCCCCCCCCCC
dark  ffff......................f.f
lead? .............................
new   .............................
level 76768889888888898888767776772
```

### HoliznaCC0 – Roof Tops (80 BPM)
```
bar   |   |   |1  |   |  2|   |   |3  |   |  4|   |   |5  |   |  6|   |   |7  
drums ....DDDDDDDDDDDDDDDDD...DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD.  
kick  ....kkkkkkkkkkkkkkkkk...kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk.  
hats  ....hhhhhhhhhhhhhhhh....hhhhhhhhhhhhhhhhhhhh.....hhhhhhhhhhhhhhhhhhh..  
bass  ....BBBBBBBBBBBBBBBBB...BBBBBBBBBBBBBBBBBBBBB.......BBBBBBBBBBBBBBBBB.  
keys  CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC  
dark  .............................................fffffff..................  
lead? ......................................................................  
new   **.*.................*.*.............................................*  
level 6667888888888888888977667888888888888889888977777777888888898889888974  
```

### HoliznaCC0 – Summer Felt Longer Then (80 BPM)
```
bar   |   |   |1  |   |  2|   |   |3  |   |  4|   |   |5  |   |  6|   |   |7  |   |
drums ....DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD... 
kick  ....kkkkkkkkkkkkkkkkkkkkkkkkkkkkk.......kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk... 
hats  .......h...h...............hh...h..........h...h...hh..........h...h....h... 
bass  ....BBBBBBBBBBBBBBBBBBBBBBBBBBBBB.......BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB.. 
keys  CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC.. 
dark  fffff...............f.......................................f............... 
lead? .......................................................................LLLL. 
new   ****................................*................................***.... 
level 7777788888888888889888989888888888888888888888888888889889888888888888988630 
```

### HoliznaCC0 – August Never Lasts (85 BPM)
```
bar   |   |   |1  |   |  2|   |   |3  |   |  4|   |   |5  |   
drums ......DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD....... 
kick  ........kkkkkkkkkkkkkkkk.......kkkkkkkkkkkkkkkkk....... 
hats  .......hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh....... 
bass  BBBBBBBBBBBBBBBBBBBBBBBBBB......BBBBBBBBBBBBBBBBB...... 
keys  CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC.. 
dark  ffffffff................ffffffff................fff.f.. 
lead? ....................................................... 
new   ................................................*...... 
level 7777777788888888888888888998889988888888888888887665531 
```

### HoliznaCC0 – Ocean Memory (67 BPM)
```
bar   |   |   |1  |   |  2|   |   |3  |   |  4|  
drums ....DDDDDDDDDDD.DDDDDDDDDDDDDDDDDDDDDDD... 
kick  ....kkkkkkkkk...kkkkkkkkk...kkkkkkkkk..... 
hats  ....hhhhhhhhhhhhhhhhhhhhh...hhhhhhhhhhhhh. 
bass  ....BBBBBBBBB...BBBBBBBBBBBBBBBBBBBBB..... 
keys  .CCCCCCCCCCCCCCCCCCCCCCCCC.CCCCCCCCCCCCCC. 
dark  .ff....................................... 
lead? .......................................... 
new   .**..........***.....................***.. 
level 255788898889877788898889888888898889877781 
```

### HoliznaCC0 – Washed Up (90 BPM)
```
bar   |   |   |1  |   |  2|   |   |3  |   |  4|   |   |5  |   |  6|   |   |7  |   |  8|   |   |
drums ....DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD..........   
kick  ............kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk..........   
hats  ....hhhhhhhhhhhhh.hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh.hhhhhhhhhhhhhhhhhhhhhhhhhh..........   
bass  ............BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB.........   
keys  .CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC.   
dark  ........f.f.fffffff.....f.f.................fffffff.....f.f...........................   
lead? .............................................................................LLLLLL..L   
new   .....***......................................................................****....   
level 05557777777788898888888988888889888888898888888988888888888888898889888988887767675633   
```

### HoliznaCC0 – Still Life (76.5 BPM)
```
bar   |   |   |1  |   |  2|   |   |3  |   |  4|  
drums DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD.D..
kick  kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk.k..
hats  ....hhhhhhhhhhh.....hhhhhhhhhhhhh..........
bass  ....BBBBBBBBBBBB....BBBBBBBBBBBB...........
keys  CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC.
dark  f.f.....f......f........f................f.
lead? ..........LL....LLL.......LL....LL.........
new   .........................................*.
level 7786898888888988878789888888898878878777860
```

### HoliznaCC0 – Tokyo Sunset (73.5 BPM, low confidence)
```
bar   |   |   |1  |   |  2|   |   |3  |   |  4|   
drums .....DDDDD.DD..........DDDDDDDDD..........  
kick  .....kkkkk.kk..........kkkkkkkkk..........  
hats  ..........................................  
bass  .....BBBBBBBBB............................  
keys  CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC  
dark  ff................................fff.ffff  
lead? .........L....L...L....L..................  
new   ..........................................  
level 556558899888974556556557888788885667747766  
```

### Grumpynora – Combo Platter (70 BPM)
```
bar   |   |   |1  |   |  2|   |   |3  |   |  4|   |   |5  |  
drums .DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD. 
kick  .kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk.... 
hats  ......hhhhhhhhhhhhhhhhhh....hhhhhhhhhhhhhhhhhhhhh..... 
bass  ...BBBBBBBBBBBBBBBBBBBB.BB.BBBBBBBBBBBBBBBBBBBBBBB.... 
keys  .........CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC 
dark  ...................................................... 
lead? ........L............................................. 
new   ..................................................*.*. 
level 034878888888888898888887886888889889988998899888885555 
```

### Grumpynora – Sunday Driver (82 BPM, low confidence)
```
bar   |   |   |1  |   |  2|   |   |3  |   |  4|   |   |5  |   |  6|   |   |7  |   |  8|   |   |
drums DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD...DDDDDDDDDDDDDDD.DDDDDDDDDDDDDDDDDDDDDDDDDDDDD.
kick  kkkkkkkkk.......kk.kkk.kkkkkkkkk.kkkkkkk....kkkkkkkkkkkkk...kkkkkkkkkkkkkkkkkkkkkkkkkkkk.
hats  ......................................h.....................hhhhhhhhhhhhhhhhhhhhhhhhhhhh.
bass  B.B.............B.BBBBBBBBB.BBB.BBBBBBBBB.....................BB..B...BBBBBBBB.BBB.BBBBB.
keys  ....CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC.CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC.
dark  .................f..f......f..f......f......f.............f................f.........f...
lead? LLLL.L...L....L..................L.L....LLLL.LL..LL...LL....L......L....................L
new   .........................................................................................
level 54556576547456547688769678867676967787686433755565666565556456777787779778778877977887794
```

### Kolarbeatz – Sun (90 BPM)
```
bar   |   |   |1  |   |  2|   |   |3  |   |  4|   |   |5  |   |  6| 
drums .........DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD.... 
kick  .........kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk.... 
hats  ........hhhhhhhh........hhhhhhhhhhhhhhhh........hhhhhhhh..... 
bass  ........BBBBB.BBBBBBB.BBBBBBB.BBBBBBB.BBBBBBB.BBBBBBB.BBB.... 
keys  CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC 
dark  ffff.............ffff....................ffff............ffff 
lead? ........................L.......................L............ 
new   ............................................................. 
level 7666666678888888988888898888888888888888988888898888888886663 
```

### lesfm – Chilling Ego (94 BPM)
```
bar   |   |   |1  |   |  2|   |   |3  |   |  4|   |   |5  |   |  6|   |  
drums ........DD..DDDDDDDDDDDDDDDDDDDDDDDDDDDDD...DDDDDDDDDDDDD.D..DD..  
kick  ............kkkkkkkkkkkkkkkkkkkkkkkkkkkkk...kkkkkkkkkkkkk........  
hats  ............hhhhhhhhhhhhhhhhhhhhhhhhhhhh....hhhhhhhhhhhh.........  
bass  ................BBBBBBBBBBBBBBBBBBBBBBBBB...BBBBBBBBBBBBB........  
keys  CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC.  
dark  .................................................................  
lead? .................................LLL..LL................L.LL..L..  
new   .........................................***.....................  
level 77767776777688888988898888888888898889888666888888888988877677762  
```

### M.R.K – No Stress (88 BPM)
```
bar   |   |   |1  |   |  2|   |   |3  |   |  4|   |   |5 
drums ....DDDDDDDDDDDDDDDDDDDDD...DDDDDDDDDDDDDDDDDDDDD  
kick  ....kkkkkkkkkkkkkkkkkkkkk...kkkkkkkkkkkkkkkkkkkkk  
hats  ....hhhhhhhhhhhh....hhhh....hhhhhhhhhhhhhhhhhhhh.  
bass  BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB.B..  
keys  CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC....  
dark  .................................................  
lead? ............................................L.L.L  
new   .................................................  
level 8777888888889888888898888777888888888888988887876  
```
