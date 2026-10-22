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
    app/build/host/vnl_host app/tests/basic.txt app/build/host
    ./build.sh --web              # browser emulator in app/build/emu/ (serve it: python3 -m http.server -d app/build/emu)
    sh base/tests/run_tests.sh    # fm1-base's own tests

The host simulator is the fast loop: scripted keys/MIDI in, audio, screenshots and screen-reader text out.
Script commands are listed at the top of `base/host/plat_host.c`.

## Install on the device

    .venv/bin/python base/tools/fm1_install.py app/build/vnl.fwsc

At your own risk. Hold OCT- and OCT+ for 5 s for update mode. If the FM-1 won't boot but shows up as `4C4A:8057`
on USB, `base/bsp/tools/fm1_rescue.sh` restores stock firmware.

## Notes

- `app/fm1.toml`: Bluetooth is off (neither the lo-fi nor dub direction needs it; saves ~130 KB flash).
  Identity `FM-1_22` (dev builds FM-1_2200).
- Budget: 581 KB flash app slot, 96 KB RAM, 336 KB buffer pool, no external RAM.
- `ref/` (git-ignored) holds read-only clones: `fm1_open` (M-VAVE's official V14, Windows toolchain, binary engine
  SDK, GPLv3), `fm1-base`, `felucca-bsp`, `fm1-amb` and `fm1-dub` (the two direction references).
