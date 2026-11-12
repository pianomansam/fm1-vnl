# VNL-1

A lo-fi hip hop machine for the M-VAVE FM-1. VNL-1 writes and plays its own songs, one after another: every song
has its own key, tempo, chords, layout and band (drums, bass, comp, lead, pad and texture), played through a lo-fi
chain of tape wobble, dust and a plate. Press PLAY and leave it running, or steer it from the FM-1's knobs: change
the key, the tempo, the mood, any part's instrument, or jump between sections, and every change lands on the next
four-bar line, in time. VNL-1 is free software (GPL), built on [fm1-base](https://github.com/charlesvestal/fm1-base).

## Features

**Songs**

- Endless generated songs, each from a seed, with a 2-4 s silence between them; or stay on one song as long as you like.
- 37 instrument sets in five flavors: lo-fi, bossa, city pop, neo-soul and chillwave.
- 44 chord progressions in six modes (Ionian to Aeolian), any of 12 keys, 70-90 BPM, swing, and a chord color that
  makes later passes richer.
- Four moods (sad, wistful, warm, sunny) and a Motion control from calm to busy.
- Five song layouts (Classic, Slow bloom, Straight in, Breather, Drift) with intros, breaks, drum drops and endings.

**Parts**

- **Drums:** eight kits: 808 and CR-78 (modeled machines), Crunchy, Boom-bap and Jazz machine (synth drums), Soft
  brushes, and two sampled room kits (Room - big, Room - brushes). Eight patterns (boom bap, half-time, sparse,
  brushes), nine two-bar kick phrases, a ride section on the sampled kits, and a per-drum lo-fi pocket.
- **Bass:** electric, sub, triangle and FM bass, with walking lines and lines that lead into the next chord.
- **Comp:** Rhodes, piano, felt piano, vibes, organ, pad, nylon guitar and electric guitar. Eight comp rhythms (held,
  half notes, stabs, held on 2, held on 2 and 4, charleston, tresillo, late one); the guitars strum, in eight patterns.
- **Lead:** flute, whistle, piano, felt piano, vibes, jazz guitar, cello and harp, in eight styles: Impressionism,
  Romanticism, '60s Jazz, Modal Jazz, Jazz-Hop, Film Score, Bossa Nova and Ambient. The lead states a motif and loops
  it the way a producer loops a sample, with mutes, filter sweeps and drop-outs.
- **Pad:** strings, a warm synth pad, organ or a vox choir, in about two songs out of three.
- **Texture:** dust, a noise bed (vinyl or tape surface, rain) and room ambience.
- **The mix:** wow and flutter, saturation, grit, chorus, delay, a plate reverb, the kick ducking the mix, and a
  whole-mix stutter at turnarounds.

**Sounds**

- Sampled piano, nylon guitar, electric guitar, jazz guitar (re-amped through a real amp), cello, harp and two drum
  kits, all from CC0 libraries; modeled vibes; a tonewheel organ; FM and analog-style synths.
- With no sample bank on the FM-1, VNL-1 says NO SAMPLES and plays its synth instruments only.

**On the FM-1**

- A boot splash with a spinning record that flows into the now-playing screen: the song, its key, the chord playing,
  where it is, its tempo.
- A main screen with the song's progress and a tile per part; a page per part for its instrument, rhythm, register and
  re-rolls.
- A Parts screen with each part's notes scrolling over the song; a Sections screen to jump, loop a section or stay on
  the song; a Mixer with a level, mute and solo per part.
- The keys' lights show which parts are playing; the lowest white keys mute (press) or solo (hold) a part.
- A screen reader, through the [FM-1 Screen Reader](https://charlesvestal.github.io/fm1-reader/).
- USB audio in and out: record VNL-1 on a computer, or play the computer through the FM-1.
- A CPU guard that eases off reverb tails and quiet voices before the audio can glitch.

**For tinkerers**

- **The workbench:** a local web page that plays the same engine. Change any decision or re-roll one part, lock
  parts, edit drum kits and patterns, and save feedback with a snapshot of the song. It can also drive the FM-1's
  screens over USB.
- **The host simulator:** renders songs to WAV, with a song log, MIDI and MusicXML of what each part played.
- **Patterns as files:** drum, kick, strum, comp and chord patterns as MIDI and MusicXML in `patterns/`; edit them in
  a DAW or MuseScore and import them back.

## Controls

| Control | What it does |
| --- | --- |
| PLAY | Play or stop. After a stop, PLAY carries on from the bar it stopped in. |
| SEQ | A new song. On a part's page, re-rolls only that part. |
| K1-K4 | On the main screen: key, tempo, motion, mood. On a part's page: that page's settings. |
| ALGO | Picks a part. EDIT opens its page; SELECT steps its pages. |
| PRESET | The highlighted part's instrument (or kit). |
| OCT- / OCT+ | The comp's or the lead's register. |
| SEL | Sections: jump to a section (EDIT, or its key), loop it (K1), stay on the song (K2). |
| ARP | Parts: each part's notes over the song. |
| FX | The Mixer. |
| HOME | Switches between the now-playing screen and the main screen; the last one used is the home. |
| Lowest white keys (F G A B C) | Drums, bass, comp, lead, pad: press to mute, hold to solo. |

Every change to the song lands on its next four-bar line, and the screen shows it waiting until then.

## Install (FM-1 owners)

At your own risk: this replaces the FM-1's firmware. You can always go back to M-VAVE's.

**From the browser:** open [VNL-1's site](https://pianomansam.github.io/fm1-vnl/) in Chrome or Edge, connect the
FM-1 by USB and press Install. It writes the firmware, then offers the sample bank.

**From a terminal:**

1. Get the source with its submodule, and a Python with `mido` and `python-rtmidi`:

       git clone --recursive https://github.com/pianomansam/fm1-vnl
       python3 -m venv .venv && .venv/bin/pip install mido python-rtmidi

2. Connect the FM-1 by USB and install the firmware (from the release, or your own build):

       .venv/bin/python base/tools/fm1_install.py vnl-VERSION.fwsc

3. With VNL-1 running and stopped, write the sample bank (once; again only when a release says the samples changed):

       .venv/bin/python tools/fm1_bank.py

   A firmware update leaves the bank in place. Without it, VNL-1 plays its synth instruments only.

**Update mode:** hold REC while switching the FM-1 on.

**Back to stock, or a stuck FM-1:** if the FM-1 won't boot but shows up on USB as `4C4A:8057`,
`base/bsp/tools/fm1_rescue.sh` restores M-VAVE's firmware. `tools/flash.sh` does a clean install in one go: stock V15,
then VNL-1, then the sample bank (it needs M-VAVE's V15 package, which isn't in this repository).

## Build (developers)

    git submodule update --init --recursive

### Toolchain (macOS, Apple silicon)

| Piece | Where | Used for |
| --- | --- | --- |
| Docker Desktop (running) | | JieLi's toolchain is Linux x86-64 only; the build runs it in a `linux/amd64` container (Rosetta) |
| JieLi toolchain (clang 4.0.1, pi32v2) | `~/.jieli/toolchain` (`base/tools/get_toolchain.sh`) | firmware build |
| JieLi AC79 SDK `AC79NN_SDK_V1.2.1_2023-12-13` | `~/fw-AC79_AIoT_SDK` | package boot files |
| Python venv: `python3.14 -m venv .venv && .venv/bin/pip install Pillow mido python-rtmidi` | `.venv/` (`build.sh` prefers it) | fonts (Pillow needs raqm), USB-MIDI install |
| Emscripten (`brew install emscripten`) | | browser emulator, workbench |

SDK: `git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK`

### Build and run

    ./build.sh                    # app/build/vnl.fwsc (firmware + update loader), identity FM-1_2200
    ./build.sh --release 0.9.0    # app/build/vnl-0.9.0.fwsc, identity FM-1_22000900
    ./build.sh --host             # host simulator, then:
    app/build/host/vnl_host tests/scenarios/songs.vnl app/build/scenarios   # a song, the gap, the next: songs.wav
    sh tests/run_tests.sh         # unit tests, the host build, every tests/scenarios/*.vnl, the emulator build
    ./build.sh --web              # browser emulator in app/build/emu/ (serve it: python3 -m http.server -d app/build/emu)
    sh workbench/build.sh && python3 workbench/server.py   # the workbench at http://localhost:8732/
    sh base/tests/run_tests.sh    # fm1-base's own tests

The host simulator is the fast loop: scripted keys and MIDI in, audio, screenshots and screen-reader text out. Script
commands are listed at the top of `base/host/plat_host.c`. The workbench is described in `workbench/README.md`, the
pattern files in `patterns/README.md`, the samples in `tools/samples/`.

### Notes

- **Size:** the image is 381,164 B against VNL-1's own budget of 409,600 B (the FM-1's app slot is 581 KB); RAM
  `.data`+`.bss` 74,764 B of 98,304 B; buffer pool 304,956 B of 344,064 B; no external RAM. The sample bank
  (339,281 B) lives in the flash's data area, outside the image. `tools/mem-check.sh` checks the budgets.
- **No radio:** `app/fm1.toml` builds without Bluetooth (no Bluetooth MIDI or audio, and so no velocity or pressure
  from the keys); it saves about 130 KB of flash. USB MIDI, USB audio and the screen reader are in.
- **Identity:** `FM-1_22` (V is the 22nd letter): dev builds are FM-1_2200, releases FM-1_22XXYYZZ. Taken by others:
  0 (default), 6 CTL-1, 7 AMB-1, 8 dub, 11 template, 13 Radio. The installer checks it, so never reuse another
  firmware's.
- **Why the venv:** `brew install emscripten` pulls in a newer Homebrew Python and repoints `python3` to one without
  Pillow. `build.sh` uses `.venv/bin/python` when it exists; `PYTHON=...` overrides.
- **Live state:** `tools/fm1_state.py` reads the running FM-1's state over USB (CPU by stage, late blocks, the song).
- `ref/` (git-ignored) holds read-only clones of other FM-1 firmwares, for reference only.

### The site

`tools/site.sh VERSION` makes the GitHub Pages site in `app/build/site`: the release build, the browser emulator,
fm1-base's `tools/make_site.py` (the front page from `app/site/landing.md`, the web installer, the manual from
`app/docs/MANUAL.md`), and the sample bank beside the package, which the installer writes after the firmware
(`app/web/install_app.js`). Serve it with `python3 -m http.server -d app/build/site`. `tools/site.sh VERSION --publish`
also pushes it to the `gh-pages` branch, replacing what was there. The site's text is in `app/site/landing.md`; its
screenshots in `app/site/img/` and `app/docs/img/` come from the host simulator (`shot FILE.png` in a script).

### Keeping docs current

When a firmware feature is added, changed or removed, update in the same change: this README's Features and
Controls, `CHANGELOG.md`, the site's `app/site/landing.md` and the manual `app/docs/MANUAL.md` (and their
screenshots when a screen changes), and `ROADMAP.md` when an item ships or is dropped. List only approved features,
and no artist or composer names.

## Credits and licenses

VNL-1 is GPL-3.0-or-later (`LICENSE`); up to the tag `mpl-final` it was MPL-2.0. It is built on
[fm1-base](https://github.com/charlesvestal/fm1-base) by Charles Vestal and
[felucca-bsp](https://github.com/hugelton/felucca-bsp), from [Felucca](https://github.com/hugelton/Felucca) by Leo
Kuroshita (Hügelton Instruments), both MPL-2.0, which allows combining with GPL code. Files adapted from fm1-base and
AMB-1 (`app/app.c`, `app/app.h`, `app/words.tsv`, `app/engine/fx.c`, `app/engine/voices.c`, `build.sh`,
`tests/run_tests.sh`) keep their MPL-2.0 notice (`LICENSES/MPL-2.0.txt`).

The firmware includes GPL-3.0-only code: the drum engines of FOMAJAM and Optimist, Optimist's CPU guard and Cut-A-Dor's
rhythm tables. So the firmware binary as a whole (and the workbench's wasm) is GPL-3.0-only; VNL-1's own source stays
GPL-3.0-or-later. The reverb and chorus are adapted from Mutable Instruments Rings by Emilie Gillet (MIT; the notice is
below). The fonts are SIL OFL 1.1, the package's JieLi boot files Apache-2.0, and every sample CC0 1.0.

Everything borrowed, with its author, license and place in the source, is in [`CREDITS.md`](CREDITS.md); the drum
engines' files in `app/drums/LICENSES.md`.

The MIT notice for the Rings code, which the firmware contains:

    Copyright 2014 Emilie Gillet.

    Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
    documentation files (the "Software"), to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and
    to permit persons to whom the Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all copies or substantial portions of
    the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
    CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.

M-VAVE and FM-1 are trademarks of their owners. VNL-1 is not affiliated with or endorsed by M-VAVE, Mutable
Instruments or Hügelton Instruments.

See also [`ROADMAP.md`](ROADMAP.md) for what may come next and [`CHANGELOG.md`](CHANGELOG.md) for releases.
