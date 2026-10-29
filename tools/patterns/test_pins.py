#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""pins.py: after an import that moves the songs, the suite's pin failures are told apart from real ones, and
   --repin rewrites the pins with a comment naming why (over copies of song_test.c and fake suite logs).
   python3 -I tools/patterns/test_pins.py   (test_patterns.py runs these too)"""
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import pins  # noqa: E402

PASS = "  ok   song\n  ok   clash\n  ok   candidates\nall tests passed\n"
NEW = "strum same: voicer {v}, songs 11111111, with set changes 22222222\n"


class Pins(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        t = Path(self.tmp.name)
        self.logs, self.song_test, self.expected = t / "logs", t / "song_test.c", t / "logs" / "pins-expected"
        self.logs.mkdir()
        shutil.copy(pins.SONG_TEST, self.song_test)
        self.old = pins.pins(self.song_test.read_text())

    def tearDown(self):
        self.tmp.cleanup()

    def suite(self, failed, logs):
        """a suite's output with `failed` runs, and their logs"""
        out = "".join(f"  FAIL {n} (see app/build/tests/{n}.log)\n" for n in failed) + "  ok   clash\nTESTS FAILED\n"
        for name, text in logs.items():
            (self.logs / f"{name}.log").write_text(text)
        return out

    def pin_fails(self):
        o = self.old
        return (NEW.format(v=o["v"]) +
                f"FAIL tests/host/song_test.c:8963: strum same: the strummed songs hash 11111111, wanted {o['a']}\n"
                f"FAIL tests/host/song_test.c:9098: wb off: with set changes 22222222, wanted {o['b']}\n"
                "FAIL tests/host/song_test.c:9401: ov part salts: the pinned hashes moved\n")

    def after(self, out):
        return pins.after(out, self.logs, self.song_test, self.expected)

    def test_a_pass_is_a_pass(self):
        self.assertEqual(self.after(PASS)[0], 0)

    def test_only_the_pins_failing_after_an_approval_is_expected(self):
        self.expected.write_text("approved drums 9\n")
        out = self.suite(["song", "candidates"], {"song": self.pin_fails(), "candidates": self.pin_fails()})
        code, say = self.after(out)
        self.assertEqual(code, 3, say)
        self.assertIn("as expected (approved drums 9)", say)
        self.assertIn(f"the strummed songs {self.old['a']} -> 11111111", say)
        self.assertIn(f"with set changes {self.old['b']} -> 22222222", say)
        self.assertIn("--repin", say)

    def test_anything_else_failing_is_a_real_failure(self):
        self.expected.write_text("approved drums 9\n")
        for failed, logs in ((["song"], {"song": self.pin_fails() + "FAIL tests/host/song_test.c:12: strum: x\n"}),
                             (["song", "clash"], {"song": self.pin_fails(), "clash": "FAIL clash_test.c:3: a rub\n"}),
                             (["song"], {"song": "Segmentation fault\n"})):
            with self.subTest(failed=failed):
                code, say = self.after(self.suite(failed, logs))
                self.assertEqual(code, 1, say)
                self.assertIn("REAL FAILURE", say)
                self.assertIn("approved drums 9", say)
                self.assertIn("Nothing was re-pinned", say)

    def test_pins_failing_with_no_import_to_explain_them_are_real(self):
        code, say = self.after(self.suite(["song"], {"song": self.pin_fails()}))
        self.assertEqual(code, 1, say)

    def test_repin_rewrites_the_pins_with_why(self):
        self.expected.write_text("approved drums 9; retired (was approved) strum 2\n")
        (self.logs / "song.log").write_text(self.pin_fails())
        before = self.song_test.read_text()
        code, say = pins.repin(self.song_test, self.logs, self.expected, today="2026-10-30")
        self.assertEqual(code, 0, say)
        text = self.song_test.read_text()
        self.assertEqual(pins.pins(text), {"v": self.old["v"], "a": "11111111", "b": "22222222"})
        self.assertNotIn(f"0x{self.old['a']}u", text)
        self.assertNotIn(f"wanted {self.old['b']}", text)
        self.assertIn("/* re-pinned (tools/patterns/import.sh --repin, 2026-10-30: approved drums 9; retired (was "
                      f"approved) strum 2): the strummed songs was {self.old['a']}, with set changes was "
                      f"{self.old['b']} */\n    CHECK(a == 0x11111111u", text)
        self.assertEqual(len(text.splitlines()), len(before.splitlines()) + 1)
        self.assertEqual(pins.repin(self.song_test, self.logs, self.expected)[0], 0)      # again: they hold
        self.assertEqual(self.song_test.read_text(), text)

    def test_repin_needs_an_import_that_moved_them(self):
        (self.logs / "song.log").write_text(self.pin_fails())
        before = self.song_test.read_text()
        code, say = pins.repin(self.song_test, self.logs, self.expected)
        self.assertEqual(code, 1)
        self.assertIn("no import has moved the pins", say)
        self.assertEqual(self.song_test.read_text(), before)


if __name__ == "__main__":
    unittest.main()
