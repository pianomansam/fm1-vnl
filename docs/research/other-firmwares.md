# Other FM-1 firmwares: ideas to borrow

> Source material, kept as researched. The distilled, current reading (general musicianship vs. lo-fi choices) is in [`docs/music/`](../music/README.md).

## RedtailWorks (HYP-1, LOF-1, ECO-1, GRAV-1)

Source: `ref/fm1-HYP1` (github.com/rustypawz/fm1-HYP1), one repo holding all four, each in its own directory.
All are AMB-1 forks from before AMB-1's MPL move: **GPL-3.0-only**, own copy of the old platform (not fm1-base).
Ideas only; no code. LOF-1 and the others are untested on a real FM-1 (simulator and emulator only).

## LOF-1 (lo-fi groovebox, `lof1/`)

A player-driven instrument, not a generator: you play the lead, a "tired jazz band" follows the chord your notes
imply. Different goal from VNL-1, but several parts transfer directly.

| Idea | How LOF-1 does it | Use in VNL-1 |
| --- | --- | --- |
| **Real instrument samples on the chip** | 18 instruments + a drum kit from Versilian Studios' **CC0** libraries (VCSL, VSCO-2 CE), at 16 kHz, 4-bit IMA ADPCM: **318 KB total**. Multi-zone (3–4 zones per instrument), looped. A Python tool picks, resamples and packs them into a C header. | The biggest takeaway. Upright piano, vibes, kalimba, nylon strum, upright bass, muted trumpet, tenor sax, flute and a kit are exactly lo-fi's palette, and CC0 means no license strings. ADPCM's grit is a lo-fi feature. Budget: fm1-base's data area beyond the store holds big data (`plat_data*`). |
| **Synth + sample split** | Rhodes, Wurli, bell, music box, pad, organ, sub, Moog are synthesized; acoustic instruments are sampled. | Same split: FM for EP/bells/bass, samples for acoustic. |
| **DUST as a medium** | Four characters: Vinyl (crackle, pops), Cassette (hiss), Reel (haze), Radio (static, band-limit), plus a granular smear of the mix. | A per-song roll: "this song is on cassette, the last was on vinyl". Strong, cheap novelty. |
| **Comp patterns** | Held, stabs, offbeats, arpeggio; four voicings; "rolled like a tired pianist". | Per-song or per-section comp style; the rolled strum matches the research (0.05–0.3 beat rolls). |
| **Bass styles** | Plain roots, root+fifth, walking, dub. | Per-song roll. |
| **Ghost lead (Daydream) and Answer** | When keys rest, a generated lead plays phrases; Answer replies to phrases (call and response). | The melody generator: a lead that plays phrases, and an answering voice in later sections. |
| **Performance FX** | Record skip, tape stop, backspin, half time, stutter, muffle, radio, dropout, crush, fill. | The **transition vocabulary** for the song plan: tape stop into the break, a record skip before the drop, half time in the outro, muffle on the intro. |
| **Drift** | Drums and progression change by themselves every few bars. | Already covered by sections. |

## HYP-1 (techno, the main firmware)

- **Sections as commands**: Groove, Break, Build (four bars, then the drop), switched on the next bar. VNL-1's
  song plan can issue the same kind of commands on a schedule instead of from a player.
- **Pattern changes wait for the bar** (queued, applied on the downbeat): needed everywhere in VNL-1.
- **Sound palettes per voice** (16 sounds per voice, 8 per drum) instead of fixed "styles": an instrument is a
  separate roll from the pattern. Same idea as the novelty vector.
- **Kick ducks everything**: matches the research's light ducking.

## ECO-1 and GRAV-1

Not genre-relevant. Shared trait worth noting: each "piece" is a little world with its own visual on screen
(a plant, a planetary system). VNL-1 could show the song as something visual (a record label, a cassette
J-card with the rolled song's name, key and tempo).

## Why LOF-1 disappoints (likely)

Not judged by ear here. Structurally: it's an instrument you have to play, there's no song form (only drift),
and it's never run on hardware. VNL-1 inverts that: it plays itself, songs have form, and we test on the FM-1.

## MAKU (okumurari2/maku): a new world every boot, grown from your likes

Source: `ref/maku`, <https://okumurari2.github.io/maku/>. On Felucca (not fm1-base), **GPL-3.0-only**, verified on
hardware by its author. Ambient, but its **generator** is the closest thing yet to VNL-1's song roll.

| Idea | How MAKU does it | Use in VNL-1 |
| --- | --- | --- |
| **A world is one seeded roll** | Boot (or HOME held) draws everything from one seed: sounds, root, scale, tempo 70–90, every macro. The re-roll runs on the main loop, never the audio interrupt; same seed, same world, bit for bit. | A song is a seed. Reproducible songs come free: save the seed, replay the song. |
| **Likes steer the roll** | REC = "like": stores the world's choices (up to 16, a ring). A new world comes from a like with chance min(90%, 15% × likes), picked weighted by recency, else fresh random. | Pairs with our novelty rule: "different from the last song" *and* "near what you liked". One button, the machine learns your taste. |
| **Mutation** | A world from a like redraws each part (each voice, root, scale, each macro) with ~8% chance; tempo kept. | A liked song comes back as a cousin, not a copy. |
| **Catalog with stable ids and weights** | Every sound is a row in an append-only catalog with a fixed id and a rating weight; changing a sound = a new row, old one retired at weight 0 (still plays from old likes). | How to store likes/seeds so firmware updates don't break saved songs. Weights = tuned odds per instrument. |
| **Background bed per world** | One computed texture or none: surf, shower, breeze, campfire, vinyl, crowd, on its own random stream (changing it doesn't change the rest). | Same as LOF-1's DUST: rain/vinyl/cassette per song. Separate RNG streams per feature is a good rule generally. |
| **Verbs** | Upper-row buttons are timed gestures (sink, cascade, swell, wobble, lift, scramble): rise, hold, fade, in beats. | Optional player nudges on top of the autopilot, later. |
| **Flash writes only on REC** | Likes are the only thing saved; the erase is done ahead of time so the press never stalls audio. | Same rule for saving liked songs. |

## Mutable Dreams (kidbee85-beep/mutable-dreams v1.0): the one we can take code from

Source: `ref/mutable-dreams/src/` (the release's source zip). Generative ambient with six assignable tracks.
**Built on fm1-base, like VNL-1**, and **MIT-licensed** (the app; also offered as GPL-3.0, with the MIT grant kept).
Validated on hardware: install, six-voice stress, ten-minute soak, return to stock.

| Idea | How it does it | Use in VNL-1 |
| --- | --- | --- |
| **Mutable Instruments DSP on the FM-1** | Vendored (MIT) parts of Plaits: FM, virtual analog, additive, and the **analog bass drum, snare and hi-hat** models; Rings' **modal resonator**; stmlib. Wired in via `fm1.toml`'s `o2`, `include`, `vendored`. | Directly reusable. Plaits' drum models are a synthesized lo-fi kit (no sample budget); Rings' modal resonator does kalimba, marimba, struck bells and plucked tones; Plaits FM covers the EP. A proven way to vendor C++ into an fm1-base app. |
| **Measured CPU tiers** | ECO (2 grains, 8 Rings modes, half-rate melodic engines) and FULL (4 grains, 24 modes, full rate); dense combinations drop to ECO automatically. | Real numbers for what fits. Our song generator can respect a CPU budget when it rolls instruments. |
| **Granular tape** | 256 KB pool buffer, 2.97 s of mono 44.1 kHz audio, grains with Hann windows, bounded feedback. | A "dust"/smear texture, or the tape-stop/record-skip transitions (they need a buffer of recent audio). |
| **Variation and undo** | REC makes a variation of the patch; ENV restores the one before; ARP locks or evolves the seed. | Same family as MAKU's mutation: "another like this", with undo. |
| **Biomes from a generator script** | 64 factory scenes generated by `tools/generate_presets.py` into a C include. | Author instrument and form tables in Python, generate C. Easier to tune than hand-written tables. |
| **Euclidean + chance + swing to 65:35, trigger ducking** | Per track. | Boom-bap needs written patterns, not Euclid, but chance per step and trigger ducking carry over. |

Identity note: it uses `FM-1_710`; ours is `FM-1_22`, no clash.

## Tuna (delirioux/tuna): generative styles as data, including LO-FI HIP HOP

Source: `ref/tuna`. On Felucca, **GPL-3.0-only** (its Marbles port and Felucca-Engines submodule are MIT upstream).
Runs on the FM-1. Generates endlessly in the manner of 89 composers and 86 genres. **The most relevant generator
so far**, because it defines a style as one data record and has a lo-fi hip hop entry
(`firmware/src/tuna_genres.inc`, `TG_STYLE_G_LOFI_HIP_HOP`).

| Idea | How Tuna does it | Use in VNL-1 |
| --- | --- | --- |
| **A style is one struct** | Tempo, meter, period (8 bars), chord length, melody density/register/range/leap profile, syncopation, motif length and reuse, motif variation odds (invert 10%, alter 30%), swing 55% on 16ths, bass rhythm bitmask and notes, pad mode, two chord cycles with a switch, jazz vocabulary, reverb type. | VNL-1's "genre" as data, with **ranges** instead of fixed values: the song roll picks inside them. Lo-fi sub-styles (Nujabes jazz, dusty Dilla, sleepy ambient) are just more records. |
| **Orchestration sets** | `orch` = 4 sets of {bass, lead, keys}, e.g. upright + flugelhorn + EP, fretless + vibes + reed EP, upright + kalimba + guitar, e-bass + shakuhachi + reed EP; one picked at random. | Exactly the per-song instrument roll, as curated trios (so combinations always sound right). |
| **Two progressions, switched** | `cycle` (degrees 3-2-5-1 = iii-ii-V-I-ish) and `cycle2` (1-4-6), `cycle_switch = 2`. | The A/B "movements" from the research, built in. |
| **Melody: periods and motifs** | Question-and-answer phrases resolving to the tonic by step; a 2-beat motif reused 70% with variations. | Lead lines that sound composed, not random. |
| **Marbles-style randomness with deja vu** | Mutable's Marbles port (bit-exact): random streams with a "deja vu" knob that loops recent choices; REC locks the phrase. | A dial between "repeat the hook" and "keep wandering"; the original Marbles is MIT, so a clean-room port is possible. |
| **Seeds and slots** | Same seed plays the same music; 16 slots save the whole state in ~300 bytes, CRC-checked, A/B sectors. | Saving liked songs: a seed plus a few overrides is tiny. |
| **Evolution timer** | An energy arc over time, modulating to a pivot key and back. | A within-song energy curve; key change for the B movement. |

## Bunker Evolved (bunker-evolved.sugarypixie10.chatgpt.site)

On Felucca, GPL-3.0-only. Ambient, four layers (AIR, MELODY, ROOT, SPARK), 24 preset "worlds".
- **EVOLVE operators** on a phrase: WALK, ANSWER, ORBIT, MIRROR, BLOOM, BREATH, VOICE, with a chance of change and
  a number of loops between changes; a liked phrase can be locked. A good vocabulary for varying a lead between
  sections (A2 = A1's melody, mirrored).
- **GROW with undo**: TUNE / SOFT / FLOW / NEW variations of a phrase, OCT− undoes. Same family as MAKU and Mutable Dreams.

## The groovebox firmwares (less relevant)

All on Felucca, GPL-3.0-only, player-sequenced rather than generative.
- **Mini-Tone** (eldontyrellcorp): per-step **NUDGE** (1/16-step resolution, half a step early to 7/16 late),
  recorded from live playing. Confirms per-step microtiming fits on the FM-1; Dilla-style lateness is cheap.
  Also ports Braids and a TB-303.
- **Jangada** (zednaked): a **6-operator FM engine that reads Dexed/DX7 patches**, and worn-tape character. A
  real DX7 Rhodes patch (the classic E.PIANO 1) is the lo-fi EP; Dexed compatibility means existing patch
  libraries could be used for the FM voices.
- **TRIBE** (apiel): 808/909 voices, clips launched at the end of the pattern, scatter effects. Clip launching on the
  boundary is the same rule as our section changes.
