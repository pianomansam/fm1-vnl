# VNL-1

A lo-fi hip hop firmware for the M-VAVE FM-1, built as an app on [fm1-base](https://github.com/charlesvestal/fm1-base)
(`base/`, a git submodule, with [felucca-bsp](https://github.com/hugelton/felucca-bsp) inside it at `base/bsp/`).
Same layout as [fm1-amb](https://github.com/charlesvestal/fm1-amb): our code lives in `app/`, started from fm1-base's
template.

    git submodule update --init --recursive

## Toolchain (macOS, Apple silicon)

| Piece | Where | Used for |
| --- | --- | --- |
| Docker Desktop (running) | | JieLi's toolchain is Linux x86-64 only; the build runs it in a `linux/amd64` container (Rosetta) |
| JieLi toolchain (clang 4.0.1, pi32v2) | `~/.jieli/toolchain` (`base/tools/get_toolchain.sh`) | firmware build |
| JieLi AC79 SDK `AC79NN_SDK_V1.2.1_2023-12-13` | `~/fw-AC79_AIoT_SDK` | package boot files (and BLE/WiFi libs, unused) |
| Python venv: `python3.14 -m venv .venv && .venv/bin/pip install Pillow mido python-rtmidi` | `.venv/` (`build.sh` prefers it) | fonts (Pillow needs raqm), USB-MIDI install |
| Emscripten (`brew install emscripten`) | | browser emulator |

SDK: `git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK`

## Build and run

    ./build.sh                    # app/build/vnl.fwsc (firmware + update loader)
    ./build.sh --host             # host simulator, then:
    app/build/host/vnl_host tests/scenarios/songs.vnl app/build/scenarios   # a song, the gap, the next: songs.wav
    sh tests/run_tests.sh         # unit tests, the host build and every tests/scenarios/*.vnl, the emulator build
    ./build.sh --web              # browser emulator in app/build/emu/ (serve it: python3 -m http.server -d app/build/emu)
    sh base/tests/run_tests.sh    # fm1-base's own tests

The host simulator is the fast loop: scripted keys/MIDI in, audio, screenshots and screen-reader text out.
Script commands are listed at the top of `base/host/plat_host.c`.

## Install on the device

    .venv/bin/python base/tools/fm1_install.py app/build/vnl.fwsc

At your own risk. Hold OCT- and OCT+ for 5 s for update mode. If the FM-1 won't boot but shows up as `4C4A:8057`
on USB, `base/bsp/tools/fm1_rescue.sh` restores stock firmware.

## Notes

- `app/fm1.toml`: Bluetooth is off (neither the lo-fi nor dub direction needs it; saves ~130 KB flash), so the
  app has no Bluetooth (no velocity either: APP_TOUCH and APP_BTDEV are gone).
- Identity `FM-1_22` (dev builds FM-1_2200; V is the 22nd letter). Taken by others: 0 (default), 6 CTL-1, 7 AMB-1,
  8 dub, 11 template, 13 Radio. The installer checks it, so never reuse another firmware's.
- Why the venv: `brew install emscripten` pulls in a newer Homebrew Python and repoints `python3` to one without
  Pillow. `build.sh` uses `.venv/bin/python` when it exists; `PYTHON=...` overrides.
- Status: VNL-1 plays generated songs, one after another, on placeholder voices (111 KB image, 31 KB RAM); it
  passes in the simulator and runs in the browser emulator. Not yet installed on a real FM-1. Minimal controls:
  PLAY stops / plays, K1-K4 key, tempo, motion, mood (each lands on the next 4-bar boundary).
- Budget: 581 KB flash app slot, 96 KB RAM, 336 KB buffer pool, no external RAM.
- `ref/` (git-ignored) holds read-only clones: `fm1_open` (M-VAVE's official V14, Windows toolchain, binary engine
  SDK, GPLv3), `fm1-base`, `felucca-bsp`, `fm1-amb` and `fm1-dub` (the two direction references).

## Credits and licenses

VNL-1 is MPL-2.0 (`LICENSE`), built on [fm1-base](https://github.com/charlesvestal/fm1-base) and
[felucca-bsp](https://github.com/hugelton/felucca-bsp), from [Felucca](https://github.com/hugelton/Felucca) by Leo
Kuroshita (Hügelton Instruments), both MPL-2.0.

### Third-party material

| What | Licence | Where |
| --- | --- | --- |
| The lo-fi chain's reverb and chorus, adapted from [Mutable Instruments Rings](https://github.com/pichenettes/eurorack) (`rings/dsp/fx/reverb.h`, `chorus.h`, `fx_engine.h`) by Emilie Gillet | MIT (below) | `app/engine/rings_fx.c`, `rings_fx.h` |
| The string synth's Ensemble, adapted from [Mutable Instruments Plaits](https://github.com/pichenettes/eurorack) (`plaits/dsp/fx/ensemble.h`) by Emilie Gillet | MIT (`CREDITS.md`) | `app/engine/strsyn.c`, `strsyn.h` |

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

The samples (piano, guitars, drums) are CC0 1.0; their sources are listed in `CREDITS.md`.

M-VAVE and FM-1 are trademarks of their owners. VNL-1 is not affiliated with M-VAVE, Mutable Instruments or Hügelton
Instruments.
