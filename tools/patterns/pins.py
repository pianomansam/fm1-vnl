#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""The pinned song hashes (tests/host/song_test.c's strum_same: the strummed songs, with set changes, and the voicer's
   shapes) after an import that moves them on purpose: approving, retiring or changing an approved pattern moves the
   songs. tools/patterns/import.sh uses this; Sam never needs to.

   python3 -I tools/patterns/pins.py after SUITE_OUTPUT
     the engine suite's output (tests/run_tests.sh): exit 0 it passed; 3 only the pins failed and the import said they
     would move (app/build/tests/pins-expected: why), with their new values; 1 any other failure (a real one)
   python3 -I tools/patterns/pins.py repin
     rewrites the pins in tests/host/song_test.c to the values the last suite run printed, with a comment naming why
     (pins-expected); never run by itself: import.sh --repin runs it, then the suite again"""
import datetime
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
SONG_TEST = ROOT / "tests/host/song_test.c"
LOGS = ROOT / "app/build/tests"                 # run_tests.sh's logs (song.log, candidates.log, ...)
EXPECTED = LOGS / "pins-expected"               # import.sh: why the pins move (import.py's "pins move:" line)
PIN_RE = re.compile(r"CHECK\(([vab]) == 0x([0-9a-f]{8})u, \"strum same:")
NEW_RE = re.compile(r"^strum same: voicer ([0-9a-f]{8}), songs ([0-9a-f]{8}), with set changes ([0-9a-f]{8})$", re.M)
PIN_RUNS = {"song", "candidates"}               # the runs that check the pins (candidates.sh runs strum_same)
NAMES = {"v": "the voicer", "a": "the strummed songs", "b": "with set changes"}


def pins(text):
    """song_test.c's text -> {v, a, b: the pinned value}"""
    got = dict(PIN_RE.findall(text))
    if set(got) != set(NAMES):
        raise SystemExit("pins: tests/host/song_test.c's strum_same pins not found")
    return got


def new_values(log):
    """a song_test output -> {v, a, b} as it computed them, or None"""
    m = NEW_RE.search(log)
    return dict(zip("vab", m.groups())) if m else None


def is_pin_failure(line, old):
    return any(f"wanted {h}" in line for h in old.values()) or "the pinned hashes moved" in line


def after(output, logs, song_test, expected):
    """-> (exit code, what to say)"""
    failed = re.findall(r"^  FAIL (\S+)", output, re.M)
    if not failed and "all tests passed" in output:
        return 0, "import: the engine suite passed"
    why = expected.read_text().strip() if expected.exists() else ""
    old = pins(song_test.read_text())
    fails = {}
    for name in failed:
        log = logs / f"{name}.log"
        fails[name] = [x for x in log.read_text().splitlines() if x.startswith("FAIL")] if log.exists() else []
    pin_only = (failed and set(failed) <= PIN_RUNS and all(fails.values()) and
                all(is_pin_failure(x, old) for xs in fails.values() for x in xs))
    new = new_values((logs / "song.log").read_text()) if (logs / "song.log").exists() else None
    if pin_only and why and new:
        moved = ", ".join(f"{NAMES[k]} {old[k]} -> {new[k]}" for k in "abv" if new[k] != old[k])
        return 3, (f"import: the pins moved, as expected ({why}): {moved}. Nothing else failed. Listen to a few "
                   "songs, then run tools/patterns/import.sh --repin to re-pin them (it reruns the suite).")
    real = ", ".join(f"{n} (app/build/tests/{n}.log)" for n in failed) or "the suite didn't finish"
    return 1, (f"import: REAL FAILURE: {real}" +
               (f"; the pins were also expected to move ({why})" if why else "") + ". Nothing was re-pinned.")


def repin(song_test, logs, expected, today=None):
    """-> (exit code, what to say); rewrites song_test.c"""
    if not expected.exists():
        return 1, "import: --repin: no import has moved the pins (run tools/patterns/import.sh first)"
    why = expected.read_text().strip().replace("*/", "* /")
    text = song_test.read_text()
    old = pins(text)
    new = new_values((logs / "song.log").read_text()) if (logs / "song.log").exists() else None
    if not new:
        return 1, "import: --repin: the last suite run printed no new values (app/build/tests/song.log)"
    changed = [k for k in "abv" if new[k] != old[k]]
    if not changed:
        return 0, "import: --repin: the pins already hold"
    for k in changed:
        text = text.replace(f"0x{old[k]}u", f"0x{new[k]}u").replace(f"wanted {old[k]}", f"wanted {new[k]}")
    at = text.index('    CHECK(a == 0x', text.index("static void test_strum_same"))
    note = (f"    /* re-pinned (tools/patterns/import.sh --repin, {today or datetime.date.today().isoformat()}: {why}): "
            + ", ".join(f"{NAMES[k]} was {old[k]}" for k in changed) + " */\n")
    song_test.write_text(text[:at] + note + text[at:])
    return 0, "import: re-pinned " + ", ".join(f"{NAMES[k]} {old[k]} -> {new[k]}" for k in changed) + f" ({why})"


def main(argv):
    if len(argv) == 3 and argv[1] == "after":
        code, say = after(Path(argv[2]).read_text(), LOGS, SONG_TEST, EXPECTED)
    elif len(argv) == 2 and argv[1] == "repin":
        code, say = repin(SONG_TEST, LOGS, EXPECTED)
    else:
        raise SystemExit(__doc__)
    print(say)
    return code


if __name__ == "__main__":
    sys.exit(main(sys.argv))
