# VNL-1

A generative lo-fi DJ groovebox for the M-VAVE FM-1. Let it play in auto mode to provide a backdrop to your life, or be the DJ and shape your own songs.

VNL-1 writes and plays its own songs, one after another. Each one different than the last. Every song
has its own key, tempo, chords, layout and band, played through a lo-fi
chain effects. Press PLAY and leave it running, or steer it from the FM-1's knobs. Change
the key, the tempo, the mood, any part's instrument, or jump between sections, and every change lands on the next
four-bar line.

Written by pianomansam, a classically-trained jazz pianist turned web developer.

VNL-1

VNL-1 is free software (GPL), built on [fm1-base](https://github.com/charlesvestal/fm1-base).

## Features

- **[Endless songs](#songs):** each one generated from a seed, with its own key, tempo, chords, layout and band; 40 instrument
  sets in five flavors, 44 chord progressions, four moods and five song layouts.
- **[A six-part band](#parts):** drums (8 kits), bass (4 instruments), accompaniment (9 instruments), lead (8 instruments in 8 styles),
  pad and texture.
- **[An accompaniment engine](#parts):** voice-led chords in eight rhythms, guitars strummed string by string in
  eight patterns, and a harp that strums or rolls its chords
- **[Real and synth sounds](#sounds):** sampled piano, guitars, cello, harp and drum kits from CC0 libraries, beside modeled and
  synth instruments.
- **[Ten sound engines](#sounds):** a sampler, FM electric piano, FM bass, a wind voice (flute, whistle), an analog-style saw
  pad, an ensemble for strings and choir, a tonewheel organ, a modeled vibraphone, a chip triangle and the drum
  synths.
- **[Ten effects](#effects)** in a lo-fi chain.
- **[Hands on](#controls):** change the key, tempo, mood or any part from the knobs, re-roll or lock a part, jump to or loop a
  section, mute and solo, and save up to 16 songs; every change lands in time.
- **[Fifteen themes](#on-the-fm-1):** color schemes for every screen, picked in Settings.
- **[On the FM-1](#on-the-fm-1):** a now-playing screen, a detailed main screen, a page per part, a screen reader and
  USB audio in and out.
- **[For tinkerers](#for-tinkerers):** a workbench web page, a host simulator that renders WAV, MIDI and MusicXML, and patterns as
  editable files.

See [Features in detail](#features-in-detail) for everything each part can do.

## Unique to VNL-1

Music theory drives VNL-1's decisions, from the key of the next song to the last note of a phrase.

- **Endless songs:** complete songs with real endings, written on the FM-1 itself, one after another, forever, each with its own key, mode, tempo,
  progression, band and arrangement, a real ending, then a short silence, and each different from the one before.
  Each new song starts in a related key: a fifth away, or the relative major or minor.
- **Chords voiced like a jazz player's:** the bass takes the root, the keys keep the 3rd and 7th, and each chord moves
  to the next by the smallest step, with the top note moving least, even from the loop's last chord back to its
  first. Dominants always carry a 9th or 13th, notes that clash with the chord are left out, and later passes add
  color one chord at a time.
- **Parts that listen to each other:** no part rubs against another. The pad, the lead and the walking bass skip any
  note a half step or a minor 9th from what the others play, and the accompaniment makes room for the lead, thinning
  under it or lifting over a low one.
- **Leads built from real melodies:** eight lead styles drawn from studies of how melodies work: a motif stated and
  then sequenced, chord tones on the strong beats, a bossa line held across the chord change, and phrases that never
  end on a dominant's 7th.
- **A guitarist's strumming:** VNL-1 writes its own strummed guitar part inside each song it generates. It
  voices each chord as a real hand shape on six strings (the root on a low string, fretted notes within four frets,
  open strings where they fit) and plays it with down and up strokes, muted chucks and a bossa thumb and pinch.
- **Deep song planning:** each song is laid out before it plays: a layout, sections, where each part enters and
  drops out, fills, swells, breaks and an ending. No two sections are alike. The seeds and patterns are a starting
  point and the engine reshapes them so every song is its own.
- **Parts planned ahead:** each part's next bar is planned before it plays, so a change from the knobs lands on the
  next four-bar line, in time, with that bar already planned in the new setting.
- **Scores, not just sound:** every part can be written out as MusicXML (and MIDI) for a score editor, and the drum,
  strum, accompaniment and chord patterns live as MIDI and MusicXML files that build into the firmware.

## Controls

| Control | What it does |
| --- | --- |
| PLAY | Play or stop. After a stop, PLAY carries on from the bar it stopped in. |
| SEQ | A new song. On a part's page, re-rolls only that part; hold it there (0.6 s) to lock the part (a padlock by its name and on its main-screen tile; hold again to unlock); on the main screen, hold it to lock the highlighted part, or on the song tile (or the Song page) the song line: key and scale, tempo, motion and mood. A locked part plays on unchanged in every new song (its instrument, rhythm or style, register and variations; the drums' kit, pattern, kick, ride and variation; the texture's dust, noise and room) while the rest rolls. Locks last until unlocked or power-off. |
| K1-K4 | On the main screen: key, tempo, motion, mood. On a part's page: that page's settings. |
| ALGO | Picks a part. EDIT opens its page; SELECT steps its pages. |
| PRESET | The highlighted part's instrument (or kit). |
| OCT- / OCT+ | The accompaniment's or the lead's register. |
| SEL | Sections: jump to a section (EDIT, or its key), loop it (K1), stay on the song (K2). |
| ARP | Parts: each part's notes over the song. |
| FX | The Mixer. |
| HOME | The home screen, set in Settings (GLO): HOME SCREEN SIMPLE, the now-playing screen (the default), or DETAILED, the main screen. On either of those two, HOME switches to the other. |
| SAVE | On the Song page, saves the song into the first free slot (SAVED TO 3); held there, or anywhere else, a line of measures for a bug report. |
| REC | On the Song page, the Saved list: EDIT loads the highlighted song (the next song), SAVE saves into that slot, SAVE held deletes it. A save made while music plays is written to the flash when it stops. |
| Lowest white keys (F G A B C) | Drums, bass, accompaniment, lead, pad: press to mute, hold to solo. |

Every change to the song lands on its next four-bar line, and the screen shows it waiting until then.

## Features in detail

### Songs

- Endless generated songs, each from a seed, with a 2-4 s silence between them; or stay on one song as long as you like.
- 40 instrument sets in five flavors: lo-fi, bossa, city pop, neo-soul and chillwave.
- 44 chord progressions in six modes (Ionian to Aeolian), any of 12 keys, 70-90 BPM, swing, and a chord color that
  makes later passes richer.
- Four moods (sad, wistful, warm, sunny) and a Motion control from calm to busy.
- Five song layouts (Classic, Slow bloom, Straight in, Breather, Drift) with intros, breaks, drum drops and endings.

### Parts

- **Drums:** eight kits: 808 and CR-78 (modeled machines, from FOMAJAM), Crunchy, Boom-bap and Jazz machine (synth
  drums adapted from Optimist), Soft brushes, and two sampled room kits (Room - big, and Room - brushes for sunny songs, from
  Karoryfer's Big Rusty Drums). Eight patterns (boom bap,
  half-time, sparse, brushes), nine two-bar kick phrases, a ride section on the sampled kits, and a per-drum lo-fi
  pocket (the pocket and four kick phrases adapted from Cut-A-Dor).
- **Bass:** electric, sub, triangle and FM bass, with walking lines and lines that lead into the next chord. Every
  bass but the sub is gently saturated, so its line carries on small speakers (Settings' BASS SATURATION turns it
  off for good speakers).
- **Accompaniment:** Rhodes, piano, felt piano, vibes, organ, pad, nylon guitar, electric guitar and harp. Eight rhythms
  (held, half notes, stabs, held on 2, held on 2 and 4, charleston, tresillo, late one); the guitars strum, in eight
  patterns; the harp strums (ballad, bossa, charleston) or rolls its chords, centered on the beat.
  Over a low lead (jazz guitar, cello) the accompaniment shares the lead's register, thinning where it plays, or lifts over it.
- **Lead:** flute, whistle, piano, felt piano, vibes, jazz guitar, cello and harp, in eight styles: Impressionism,
  Romanticism, '60s Jazz, Modal Jazz, Jazz-Hop, Film Score, Bossa Nova and Ambient. The lead states a motif and loops
  it the way a producer loops a sample, with mutes, filter sweeps and drop-outs.
- **Pad:** strings, a warm synth pad, organ or a vox choir, in about two songs out of three.
- **Texture:** dust, a noise bed (vinyl or tape surface, rain) and room ambience.

### Effects

Ten, in a lo-fi chain: tape wow and flutter, dust (saturation and a darkening low-pass), grit, a chorus and plate
reverb (adapted from Mutable Instruments Rings), a tape delay, bass saturation, kick ducking, filter sweeps on section
changes, and a whole-mix stutter on the last bar of a 4- or 8-bar group (inspired by Cut-A-Dor). The host simulator's
plate is adapted from AMB-1, inspired by Dattorro.

### Sounds

- Sampled piano, nylon guitar, electric guitar, jazz guitar (re-amped through a real amp), cello, harp and two drum
  kits, all from CC0 libraries (the Versilian Community Sample Library, VSCO-2 CE, Karoryfer Samples and FreePats);
  modeled vibes (inspired by Ravel_FM1); a tonewheel organ (inspired by SLOOP); FM and analog-style synths.
- Ten sound engines: a sampler, FM electric piano, FM bass, a wind voice (flute, whistle), an analog-style saw
  pad, an ensemble for strings and choir, a tonewheel organ, a modeled vibraphone, a chip triangle and the drum
  synths.
- With no sample bank on the FM-1, VNL-1 says NO SAMPLES and plays its synth instruments only.

### On the FM-1

- A boot splash with a spinning record that flows into the now-playing screen: the song, its key, the chord playing,
  where it is, its tempo.
- A main screen with the song's progress and a tile per part; a page per part for its instrument, rhythm, register and
  re-rolls.
- A Parts screen with each part's notes scrolling over the song; a Sections screen to jump, loop a section or stay on
  the song; a Mixer with a level, mute and solo per part.
- The keys' lights show which parts are playing; the lowest white keys mute (press) or solo (hold) a part.
- Saved songs: SAVE on the Song page keeps the song (16 slots, through power-off); REC lists them, EDIT loads one.
  A backup over USB holds them.
- Fifteen color themes (Settings, GLO): GARNET by default, plus GARNET+, BLACK CHERRY, OXBLOOD, OXBLOOD+, PLUM,
  PLUM+, ESPRESSO, TEAL, OLIVE and fm1-base's black GREEN, AMBER, CYAN, RED and MONO.
- A screen reader, through the [FM-1 Screen Reader](https://charlesvestal.github.io/fm1-reader/).
- USB audio in and out: record VNL-1 on a computer, or play the computer through the FM-1.
- A CPU guard that eases off reverb tails and quiet voices before the audio can glitch (adapted from Optimist,
  inspired by Flowstate).
- Built on fm1-base (with Felucca's board support), in the Barlow font.

### For tinkerers

- **The workbench:** a local web page that plays the same engine. Change any decision or re-roll one part, lock
  parts, edit drum kits and patterns, and save feedback with a snapshot of the song. It can also drive the FM-1's
  screens over USB.
- **The host simulator:** renders songs to WAV, with a song log, MIDI and MusicXML of what each part played.
- **Patterns as files:** drum, kick, strum, accompaniment and chord patterns as MIDI and MusicXML in `patterns/`; edit them in
  a DAW or MuseScore and import them back.

## Install (FM-1 owners)

VNL-1 runs on both FM-1 versions: the original, and the units sold since autumn 2026 with a different flash chip
(the UC25HQ80), where saving, the sample bank and updates all work.

Install at your own risk. This replaces the FM-1's firmware. You can always go back to M-VAVE's.

**From the browser:** open [VNL-1's site](https://pianomansam.github.io/fm1-vnl/) in Chrome or Edge, connect the
FM-1 by USB and press Install. It writes the firmware, then offers the sample bank.

**From a terminal:**

1. Get the source with its submodule, and a Python with `mido` and `python-rtmidi`:

       git clone --recursive https://github.com/pianomansam/fm1-vnl
       python3 -m venv .venv && .venv/bin/pip install mido python-rtmidi

2. Connect the FM-1 by USB and install the firmware (from the release, or your own build):

       .venv/bin/python base/tools/fm1_install.py vnl-1-VERSION.fwsc

3. With VNL-1 running and stopped, write the sample bank (once; again only when a release says the samples changed):

       .venv/bin/python tools/fm1_bank.py

   A firmware update leaves the bank in place. Without it, VNL-1 plays its synth instruments only.

**Update mode:** hold REC while switching the FM-1 on.

**Back to stock, or a stuck FM-1:** if the FM-1 won't boot but shows up on USB as `4C4A:8057`,
`base/bsp/tools/fm1_rescue.sh` restores M-VAVE's firmware. `tools/flash.sh` does a clean install in one go: stock V15,
then VNL-1, then the sample bank (it needs M-VAVE's V15 package, which isn't in this repository). Stock erases the
saved songs, so flash.sh exports them first (into `backups/`) and writes them back at the end.

## Build (developers)

Clone with the submodule (`git submodule update --init --recursive`). The firmware builds on macOS (Apple silicon)
with Docker and JieLi's toolchain; the host simulator, the browser emulator and the workbench need only a C compiler,
Python and Emscripten.

    ./build.sh              # the firmware: app/build/vnl-1.fwsc
    ./build.sh --host       # the host simulator: renders songs to WAV, with screenshots and the screen reader's text
    sh tests/run_tests.sh   # every test
    sh workbench/build.sh && python3 workbench/server.py   # the workbench at http://localhost:8732/

The toolchain, the budgets, the site and the rule for keeping the docs current are in [DEVELOPING.md](DEVELOPING.md).

## Credits and licenses

VNL-1 is Copyright (C) 2026 pianomansam and GPL-3.0-or-later (`LICENSE`, `NOTICE`); up to the tag `mpl-final` it was
MPL-2.0. It is built on [fm1-base](https://github.com/charlesvestal/fm1-base) by Charles Vestal and
[felucca-bsp](https://github.com/hugelton/felucca-bsp), from [Felucca](https://github.com/hugelton/Felucca) by Leo
Kuroshita (Hügelton Instruments), both MPL-2.0, which allows combining with GPL code. Files adapted from fm1-base and
AMB-1 (`app/app.c`, `app/app.h`, `app/words.tsv`, `app/engine/fx.c`, `app/engine/voices.c`, `build.sh`,
`tests/run_tests.sh`) keep their MPL-2.0 notice and Charles Vestal's copyright (`LICENSES/MPL-2.0.txt`); VNL-1's
changes to them are Copyright (C) 2026 pianomansam, under the same MPL-2.0.

The firmware includes GPL-3.0-only code: the drum engines of FOMAJAM and Optimist, Optimist's CPU guard and Cut-A-Dor's
rhythm tables. So the firmware binary as a whole (and the workbench's wasm) is GPL-3.0-only; VNL-1's own source stays
GPL-3.0-or-later. The other licenses, each text in `LICENSES/`:

- The reverb and chorus, adapted from Mutable Instruments Rings by Emilie Gillet: MIT (`LICENSES/MIT.txt`, and in
  `app/engine/rings_fx.c`).
- The Barlow font, packed into the firmware: SIL OFL 1.1 (`LICENSES/OFL-1.1-Barlow.txt`). fm1-base also ships the
  Terminus font (`LICENSES/OFL-1.1-Terminus.txt`); VNL-1's build doesn't use it.
- The JieLi AC79 SDK's boot files in the firmware package: Apache-2.0 (`LICENSES/Apache-2.0.txt`). No JieLi library is
  linked into the firmware: VNL-1 builds without Bluetooth.
- The samples: CC0 1.0, but for the jazz guitar's re-amp recording, which is Copyright (C) 2026 pianomansam and
  GPL-3.0-or-later.

Everything borrowed, with its author, license and place in the source, is in [`CREDITS.md`](CREDITS.md); the drum
engines' files in `app/drums/LICENSES.md`.

M-VAVE and FM-1 are trademarks of their owners. VNL-1 is not affiliated with or endorsed by M-VAVE, Mutable
Instruments or Hügelton Instruments.

See also [`ROADMAP.md`](ROADMAP.md) for what may come next and [`CHANGELOG.md`](CHANGELOG.md) for releases.
