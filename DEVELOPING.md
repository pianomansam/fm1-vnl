# Developing VNL-1

How to build, test and run VNL-1 from source. For what VNL-1 does and how to install it, see the
[README](README.md).

## Get the source

    git submodule update --init --recursive

## Toolchain (macOS, Apple silicon)

| Piece | Where | Used for |
| --- | --- | --- |
| Docker Desktop (running) | | JieLi's toolchain is Linux x86-64 only; the build runs it in a `linux/amd64` container (Rosetta) |
| JieLi toolchain (clang 4.0.1, pi32v2) | `~/.jieli/toolchain` (`base/tools/get_toolchain.sh`) | firmware build |
| JieLi AC79 SDK `AC79NN_SDK_V1.2.1_2023-12-13` | `~/fw-AC79_AIoT_SDK` | package boot files |
| Python venv: `python3.14 -m venv .venv && .venv/bin/pip install Pillow mido python-rtmidi` | `.venv/` (`build.sh` prefers it) | fonts (Pillow needs raqm), USB-MIDI install |
| Emscripten (`brew install emscripten`) | | browser emulator, workbench |

SDK: `git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK`

## Build and run

    ./build.sh                    # app/build/vnl-1.fwsc (firmware + update loader), identity FM-1_2200
    ./build.sh --release 0.9.0    # app/build/vnl-1-0.9.0.fwsc, identity FM-1_22000900
    ./build.sh --host             # host simulator, then:
    app/build/host/vnl-1_host tests/scenarios/songs.vnl app/build/scenarios   # a song, the gap, the next: songs.wav
    sh tests/run_tests.sh         # unit tests, the host build, every tests/scenarios/*.vnl, the emulator build
    ./build.sh --web              # browser emulator in app/build/emu/ (serve it: python3 -m http.server -d app/build/emu)
    sh workbench/build.sh && python3 workbench/server.py   # the workbench at http://localhost:8732/
    sh base/tests/run_tests.sh    # fm1-base's own tests

The host simulator is the fast loop: scripted keys and MIDI in, audio, screenshots and screen-reader text out. Script
commands are listed at the top of `base/host/plat_host.c`. The workbench is described in `workbench/README.md`, the
pattern files in `patterns/README.md`, the samples in `tools/samples/`.

## Notes

- **Size:** the image is 406,252 B against VNL-1's own budget of 409,600 B (the FM-1's app slot is 581 KB); RAM
  `.data`+`.bss` 79,660 B of 81,920 B (98,304 B on the chip); buffer pool 305,220 B of 307,200 B (344,064 B); no
  external RAM. The sample bank (339,281 B) lives in the flash's data area, outside the image.
  `tools/mem-check.sh` checks the budgets; `python3 tools/size_report.py [--files]` breaks flash, RAM and the bank
  down by part (song planner, lead, engines, UI, platform…) from the last build.
- **No radio:** `app/fm1.toml` builds without Bluetooth (no Bluetooth MIDI or audio, and so no velocity or pressure
  from the keys); it saves about 130 KB of flash. USB MIDI, USB audio and the screen reader are in.
- **Identity:** `FM-1_22` (V is the 22nd letter): dev builds are FM-1_2200, releases FM-1_22XXYYZZ. Taken by others:
  0 (default), 6 CTL-1, 7 AMB-1, 8 dub, 11 template, 13 Radio. The installer checks it, so never reuse another
  firmware's.
- **Why the venv:** `brew install emscripten` pulls in a newer Homebrew Python and repoints `python3` to one without
  Pillow. `build.sh` uses `.venv/bin/python` when it exists; `PYTHON=...` overrides.
- **Live state:** `tools/fm1_state.py` reads the running FM-1's state over USB (CPU by stage, late blocks, the song).
- **Saved songs:** `tools/fm1_songs.py` lists, exports and writes the FM-1's saved songs over USB (read-only without
  `--write`); `put SLOT DUMP.json` saves a song from a live-state dump.
- `ref/` (git-ignored) holds read-only clones of other FM-1 firmwares, for reference only.

## The site

`tools/site.sh VERSION` makes the GitHub Pages site in `app/build/site`: the release build, the browser emulator,
fm1-base's `tools/make_site.py` (the front page from `app/site/landing.md`, the web installer, the manual from
`app/docs/MANUAL.md`), and the sample bank beside the package, which the installer writes after the firmware
(`app/web/install_app.js`). Serve it with `python3 -m http.server -d app/build/site`. `tools/site.sh VERSION --publish`
also pushes it to the `gh-pages` branch, replacing what was there. The site's text is in `app/site/landing.md`; its
screenshots in `app/site/img/` and `app/docs/img/` come from the host simulator (`shot FILE.png` in a script).

## Keeping docs current

When a firmware feature is added, changed or removed, update in the same change: the README's Features and
Controls, `CHANGELOG.md`, the site's `app/site/landing.md` and the manual `app/docs/MANUAL.md` (and their
screenshots when a screen changes), and `ROADMAP.md` when an item ships or is dropped. List only approved features,
and no artist or composer names.
