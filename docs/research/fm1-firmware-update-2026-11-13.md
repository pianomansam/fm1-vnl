# FM-1 firmware update survey (2026-11-13)

What changed in the firmwares we track since [fm1-firmware-update-2026-10-30.md](fm1-firmware-update-2026-10-30.md),
and what's new. Read with [fm1-reuse-survey-2026-10-26.md](fm1-reuse-survey-2026-10-26.md) and
[fm1-licenses-2026-10-25.md](fm1-licenses-2026-10-25.md) (not repeated here).

**New firmwares:** 427 FM-1 and SMK-37 repos that are in neither the 10-30 list nor `ref/` nor our earlier surveys are
in [new-fm1-firmwares-2026-11-13.tsv](new-fm1-firmwares-2026-11-13.tsv) (same columns; `updated` is the last push).
327 of them were created on or after 10-30. Licenses: 227 GPL-3.0, 79 none, 48 MPL-2.0, 35 MIT, 30 other, 8 more.
Sources: thefirmwarehub.com's catalog (202 firmwares; 66 new to us), 2bbb/fm1-firmware-collection (373 submodules;
120 new), fm1-base / felucca-bsp dependents (33 new), and GitHub search (the rest). Repos for other M-VAVE devices
(Tank-G, SMC-PAD, Chocolate, Pocket AMP, BlackBox) are left out.

**`ref/`:** all 40 clones fetched; 27 changed. fm1-dub, tuna and frodhes still need `git -C ref/X show FETCH_HEAD:path`,
and frasil is now history-rewritten upstream too. Eight new shallow clones, read-only: `ref/jamtrack-fm1`,
`ref/rakt-fm1`, `ref/fm1-songbox`, `ref/fm1-trio`, `ref/Strumline-fm1`, `ref/naiwan-91` (superposition/91),
`ref/fm1-moss`, `ref/riddim-roots-fm1`.

License reminder: GPL-3.0-only code is fine to copy in (the binary is already GPL-3.0-only through the drum engines).
MIT, MPL-2.0 (file by file) and our own base are free of that. Unlicensed repos are ideas only.

## Our uniqueness claims: what changed

| Claim (README "Unique to VNL-1") | Now | Holds? |
|---|---|---|
| **Endless songs**, one after another, with a real ending, a short silence and a related key | **Tuna 1.2 "Radio"** (on the FM-1, GPL): another generated piece every 1 to 30 minutes, coming in at a cadence. No endings or sections yet (Tuna's roadmap phase 12: "longer form"). **FM-1 Song Box** (raspberrypi66/fm1-songbox, MIT): an ESP32-S3 box that drives an FM-1 over Bluetooth MIDI with an endless chain of complete **lo-fi hip hop** pieces, about 3 minutes each, with intro, hooks, a break, a final hook lifted a step, an outro that slows down, a gap, then the next piece in its own key and tempo (`main/lofi_gen.c` `plan_piece` 1080, `gen_outro` 1580). **PERF-1 0.16.3 Smart DJ**: endless sets, mixed piece to piece. **DARK-1 Wander** and **RIDDIM ROOTS** also move on by themselves | **Yes, narrowly.** No FM-1 firmware plays complete songs with endings, silence between them and related keys. But "endless" alone no longer sets us apart, and the Song Box comes close on another device. Reword: lead with "complete songs, each with its own ending, written on the FM-1 itself" |
| **MusicXML** of what played | None found (GitHub code search). Others export MIDI (Tuna, RAKT) | Yes |
| **Strummed guitar with real hand shapes** ("the only FM-1 firmware we found that writes its own strummed guitar part") | **Strumline** (Mighty-stack444, 2026-11-03, GPL) works out a six-string hand shape for any chord in any tuning: a root on a low string, the rest within a four-fret window or open, scored by coverage, mutes and fingers (`firmware/src/app/ui.c` 138-217, nearly our README's words). It strums 14 patterns over a progression the player loads or records, hands free. **JAMTRACK** (MIT) generates a jam and comps it with strums, fingerpicking or Travis picking on a spread "guitar" voicing (no frets). **fm1-guitar** (xMagElonx): real voicings, a strum model, played by hand | **Partly.** Ours is still the only one that writes the part itself inside a generated song, but the hand shapes are no longer unique. Drop "the only"; keep the generated part, the chucks and the bossa thumb and pinch |
| **Accompaniment thins or lifts around the lead** | None found. RAKT does the reverse: its melody "Layer" shortens its reply to fit the gaps between chord hits | Yes |

## Ranked: what to borrow

| # | What | Source (file:line) | License | VNL-1 need | Cost | Verdict |
|---|---|---|---|---|---|---|
| 1 | **Render a block ahead** (`audio_ahead = true`) | fm1-base, already in our `base/`: `device/main_fm1.c` 132-155, `tools/build.py` 172 | MPL (ours) | Audio drops on the device: a render may run up to two halves long (the spike at a chord's start) without a dropout; two late in a row still drop | One line in `app/fm1.toml`; 2 KiB RAM; 5.8 ms more latency (nothing for a player) | **Reuse first**, then the CPU guard bench on the FM-1 |
| 2 | **Slower input scan** (`input_hz = 5000` or `2000`) | our `base/tools/build.py` 176-190 | MPL (ours) | CPU headroom: the scan is about 7.5% of core 0 at 10 kHz; 5000 frees ~4%, 2000 ~6.5% (estimated). VNL-1 isn't played from the keys | One line. At 2000, fast knob flicks lose steps and the LEDs lose their breathing and dim levels (check key lights) | **Reuse** at 5000; try 2000 |
| 3 | **CPU at 396 MHz** (`cpu_clock = 396`) | our `base/docs/SECOND-CORE.md` 135, `tools/build.py` 168 | MPL (ours) | 10% more clock for the worst block | One line; past JieLi's rating; safe mode falls back to 360 | **Reuse after a long soak** on Sam's unit; keep it out of releases until then |
| 4 | **Load ceiling**: after an emergency shed, admit no more voices than sounded then (a new one replaces the oldest released tail); one back every ~250 ms under 60% load | Tessera `firmware/src/voice.c` 149-210, `audio.c` 90, 151 (#586, 11-14) | GPL-3.0-only | Our guard (from Optimist) can loop admit, overload, shed, admit: clicks and late halves anyway | ~40 lines, 2 bytes of state | **Adapt** into `vx_guard` |
| 5 | **Steady sample loops**: each STFT bin held to its mean level over the loop, then a spectral crossfade that keeps every partial's magnitude (no beating or "wah" at short loops) | erik37 1.3 `tools/build_bank.py` `spectral_crossfade` 133, `steady_loop` 177 | MPL-2.0 | Sampled horns parked for sample space: shorter loops that don't pulse mean smaller zones. Also our piano, cello and harp loops | Build time only (numpy, scipy); zero firmware cost | **Adapt** as an MPL file beside `tools/samples/pack.py`; A/B a sax zone at half its loop length |
| 6 | **Broken-chord comp as voice-index patterns** on a voice-led chord: `ARPEGGIO` "1.2.3.4.5.4.3.2.", `RIPPLE`, `FINGERPICK`, `TRAVIS`; one picked voice rings to the chord's end | JAMTRACK `app/dsp/styles.h` 232-244, `app/dsp/seq.h` `play_chords` 516 (picked voices 548), `voice_chord` in `theory.h` 278 | MIT | Broken-chord piano comp (near-term). The 10-26 survey found the climb in no firmware; this is it, MIT | ~60 lines over our voicing | **Reuse** the mechanism; write our own climbs; credit |
| 7 | **Drums by energy 0-4 and fills**: the barest level keeps kick and snare on the beats; ghosts and "?" hits by probability; a fill on the bar before a change of part, snare crescendo or toms, the beat dropping out for hip hop (`FILL_DROP`); a crash and kick after it | JAMTRACK `seq.h` `drum_vel` 211, `build_bar` 237 (fills 282, crash 329), `bar_start` 848; `styles.h` `GROOVES` 34 (`BOOM BAP` 100, `LO-FI` 105, `BOSSA` 55) | MIT | Drummer layer (tom fills, crash), song energy over time, a second bossa groove, vary-not-avoid | Control rate; tables a few KB | **Adapt** (fills and energy rules); grooves as reference |
| 8 | **A and B parts**: switch on the round's end with a fill; each part restarts its voicing, bass and line from home "so A sounds like A again after B" | JAMTRACK `seq.h` `part_switch` 833, `bar_start` 848 | MIT | The B section (later) | Small | **Adapt** the rules |
| 9 | **Bass approach, passing and dead notes**: the last note before a change leans a half step into the next root; walking steps through the scale toward it; `d` muted notes | JAMTRACK `seq.h` `play_bass` 391 (approach 425, passing 464); `styles.h` `BSTYLES` 186 | MIT | Bass details (anticipations, ghost notes, contour-led walking) | Small | **Adapt**; check each against "no substitutions" (approach tones only) |
| 10 | **Gate and stutter patterns per part**: 16 masks of live and repeated steps; STUT loops the last live step with a windowed crossfade | Felucca `firmware/src/slicer.c` `SL_PAT` 28-45, repeat 92-152 | GPL-3.0-only | Subtler stutters (a repeated 16th up to the whole mix), a stutter where no lead plays (the comp alone), the on-demand stutter | Per-part delay line; a 2 KB-class buffer per stuttering part | **Adapt**, A/B soloed on a dense song |
| 11 | **Battery in the header**: three bars, a bolt on USB power, nothing until the first reading; the screen reader hears it | AMB-1 `app/ui.c` 625-640 (`plat_battery`) | MPL-2.0 | Settings screen and home status (we show none) | ~0.5 KB flash | **Adapt** |
| 12 | **Static audio budget from the disassembly**: hot loops' instructions per sample, failing the build over a limit | erik37 `app/tests/target_budget.py` | MPL-2.0 | Device checks, `docs/cost.md`, beside `tools/cost-gate.sh` | Tool only | **Adapt** (the counting; our own loops) |
| 13 | **Intro and outro loudness**, measured from reference recordings: intros' first 4 bars -11.5 dB, then -6.8, then -3.8; outros -2.4 dB halfway, -6.5 at 3/4, -20 in the last 4 bars; the kick leaves last | DUB-1 1.4.3 `firmware/src/dsp/amb.c` 2185-2190 | GPL-3.0-only (numbers) | Darker intro, endings, bed alone | Numbers only | **Idea**: check our intro and ending against them |
| 14 | **The ending slows down**: a ritardando over the outro's 4 bars, the last chord rolled and left ringing into the gap | FM-1 Song Box `main/lofi_gen.c` `gen_outro` 1580 | MIT | A real ending, varied (vary, not avoid) | Tempo ramps at control rate | **Idea**, A/B on a few songs |

## What changed in `ref/` that matters

- **fm1-base** (153 commits): our fork already has all of it but c8e0599 (the package now carries the resource region,
  and felucca-bsp's loader writes it, so Hub and M-UPGRADE installs bring an app's resources). Items 1-3 above, a
  second-core guide (`docs/SECOND-CORE.md`), USB audio stems (`usb_audio_channels`), the UC25HQ80 flash of newer units.
- **Tessera 3.5.2**: the load ceiling (item 4), an honest CPU meter (the USB service counted, a recent peak).
- **erik37 1.3** (MPL): steady loops (item 5), a pad layer under the piano, the target budget (item 12).
- **Tuna 1.2**: the player: Radio, favourites, share codes (a piece as a URL), MOOD (one knob from calm to busy).
- **DUB-1 1.4.3**: fuller intros and outros, a segue from the next piece's second phrase (item 13).
- **ChoralRoot 0.41**: drums on the second core; a second resource region freeing 12 KB of the app slot.
- **frodhes 0.9.11**: a two-core latency build; core 1's work cut ~700 µs a block by keeping it off the flash cache.
- **FMEL-1 1.3**: the sax as 8-bit PCM with lossless prediction (136 to 106 KB); sprites run-length packed (31 to 22 KB).
- **Felucca 1.8.2**: pending notes cleared when a song section changes (#316); a target-cost check that refuses
  incomplete estimates.
- **Skiff 2.5-2.6**, **Shine On 3.5**, **Espada**, **Flyfish**, **Estampie**, **FoMni**, **FiMba**, **Polly**, **CTL-1**,
  **AMB-1**: the new flash chip, hub cards, mirrors, installers; AMB-1's battery (item 11). Nothing musical for us.
- **FOMAJAM**: no drum changes since our 138c5d9 (MIDI, Bluetooth, a screen over USB). **fm1-HYP1**: BIT-1, a
  generative 8-bit tracker. **lunar-modulator**: notes on JieLi's USB updater only.

## Ideas only / low priority

- **PERF-1 Smart DJ** energy shapes (arc, climb, peak, chill) and "break when the groove goes stale, not on a
  count": for song energy over time. **RAKT**: the bass locking to the kick, per-part "Hold" while the rest evolves.
- **TRIO** (MPL): a walking bass and a swing drummer as small networks on the chip, and the bass fine-tuned on a line
  the player walks. Heavy; a far idea for "learning from likes".
- **Tuna's share codes and favourites**: a song as a short code and a link; fits "another like this".
- **NAIWAN 91** (GPL, a PERF-1 fork): city-pop genre settings, for "a straight city-pop groove".
- **JAMTRACK chord push**: the comp lands up to two steps early. Our roadmap wants changes that land a little late;
  the same switch, the other way.
- **Tuna's flute and sax as FM patches**, which freed 62 KB of samples: a synth stand-in while sampled horns are
  parked (for the synth sets, not in place of samples).
- **USB audio stems** (`usb_audio_channels`, fm1-base): the parts on their own channels, for Sam's recordings.
- **The resource region in the package**: if the sample bank ever fits a resource region, Hub installs would bring
  it. Check sizes first; today the bank is written by `tools/fm1_bank.py` and the installer.
- **Second core**: ChoralRoot's drums on core 1 and frodhes' flash-cache lesson. Headroom only (some units misread).
- **Strumline's shape scorer** (GPL): a cross-check for ours across tunings.

## Skip

RIDDIM ROOTS' stutter (one repeated step; ours does more), FMEL-1's lossless 8-bit packing (our 4-bit ADPCM is
smaller), DARK-1, SATIE-1, HADAL-1, MOSS-1, Rumiboard (docs only so far), the game, tutor and companion firmwares,
mirrors and recovery tools, and the many synth and groovebox firmwares with nothing generative. No new firmware does
a screen cross-fade we could use for the home-screen fade.

## Housekeeping found

- Our fork of fm1-base is one commit behind upstream (c8e0599, the resource region in the package). Nothing needed
  unless the bank moves there.
- The drum engines and guard we borrowed are current: FOMAJAM's drums unchanged since 138c5d9; Optimist's
  `drum_synth.c` unchanged; our guard pin 8a91efd (11-05) is past its 10-29 to 11-02 fixes. Cut-A-Dor: no new release.
- SLOOP 2.8.4 (91f3018) and FMEL-1 1.2 both fixed distortion and DC at the ends of notes caused by rounding the mix
  down. Check where our mix becomes 16-bit: round to nearest, never floor.
- Felucca #316: notes still pending when a section changes. Check our section change and stutter paths for the same.
- README "Unique to VNL-1": reword the endless-songs and strumming bullets (see the table above).
- `ref/` now holds 48 clones (plus `mutable-dreams`, our own repo).
