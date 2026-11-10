#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""The screens generator (gen.py): the main screen's tables round-trip to screens.json, anything outside vocab.json is
   refused by name, the choice names are options.json's as the device writes them (through a small C harness), and
   --check catches a stale file.
   python3 -I tools/screens/test_gen.py"""
import json
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
GEN = HERE / "gen.py"
SCREENS = ROOT / "workbench/ui/screens.json"
UI = ROOT / "app/ui"


def gen(*args):
    return subprocess.run([sys.executable, "-I", str(GEN), *map(str, args)], capture_output=True, text=True)


def c_array(src, name):
    """the brace-enclosed initializer of `name` in generated C"""
    m = re.search(r"\b" + re.escape(name) + r"\[[^\]]*\]\s*=\s*\{(.*?)\n\};", src, re.S)
    assert m, name
    return m.group(1)


class Gen(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.out = Path(cls.tmp.name) / "ui"
        r = gen("--out", cls.out)
        assert r.returncode == 0, r.stderr
        cls.data = json.loads(SCREENS.read_text())

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def harness(self, body):
        """a C program over the generated tables: its stdout"""
        prog = Path(self.tmp.name) / "h.c"
        prog.write_text('#include <stdio.h>\n#include "screens_gen.h"\n#include "choices_gen.h"\n#include "song.h"\n'
                        "int main(void) {\n" + body + "\nreturn 0;\n}\n")
        exe = Path(self.tmp.name) / "h"
        r = subprocess.run(["cc", "-O1", "-std=c99", "-DPLAT_HOST", "-Wall", "-Wextra", "-Werror", f"-I{self.out}", f"-I{ROOT}/app/song", f"-I{ROOT}/app/engine", f"-I{ROOT}/base/include", "-o", str(exe),
                            str(prog), str(self.out / "screens_gen.c"), str(self.out / "choices_gen.c")],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        return subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout

    def test_round_trip_main(self):
        """the main screen as generated: its bands, each item's kind and each text's template, as screens.json has
           them (a template's values decoded back to {name})"""
        vocab = json.loads((ROOT / "workbench/ui/vocab.json").read_text())
        kinds = list(vocab["items"])
        out = self.harness(r"""
            const ui_screen_t *s = &UI_SCREENS[UI_HOME];
            printf("%d\n", s->n_bands);
            for (int b = 0; b < s->n_bands; b++) {
                const ui_band_t *band = &UI_BANDS[s->bands + b];
                printf("band %d %d %d\n", band->y, band->h, band->n_items);
                for (int i = 0; i < band->n_items; i++) {
                    const ui_item_t *it = &UI_ITEMS[band->first_item + i];
                    printf("item %d ", it->kind);
                    if (it->kind == UI_TEXT)
                        for (const char *p = UI_STR(it->ref); *p; p++) {
                            if (*p == UI_TPL_VAL) { printf("{%s}", UI_VALUE_STR(UI_TPL_ID(p + 1))); p += 2; }
                            else putchar(*p);
                        }
                    printf("\n");
                }
            }""").splitlines()
        main = self.data["screens"][self.data["home"]]
        want = [str(len(main["bands"]))]
        for b in main["bands"]:
            want.append(f"band {b['y']} {b['h']} {len(b['draw'])}")
            for it in b["draw"]:
                k = next(k for k in kinds if k in it)
                want.append(f"item {kinds.index(k)} " + (it["text"] if k == "text" else ""))
        self.assertEqual([x.rstrip() for x in out], [x.rstrip() for x in want])

    def test_unknown_kind_refused(self):
        """an item outside vocab.json's draw items: refused, named, nothing written"""
        bad = json.loads(SCREENS.read_text())
        bad["screens"]["main"]["bands"][0]["draw"].append({"spark": 1})
        f = Path(self.tmp.name) / "bad.json"
        f.write_text(json.dumps(bad))
        out = Path(self.tmp.name) / "bad"
        r = gen("--screens", f, "--out", out)
        self.assertEqual(r.returncode, 1)
        self.assertIn("main.bands[0]: no draw item spark", r.stderr)
        self.assertFalse((out / "screens_gen.c").exists())
        bad["screens"]["main"]["bands"][0]["draw"].pop()
        bad["defaults"]["PLAY"] = {"do": "warp"}
        f.write_text(json.dumps(bad))
        r = gen("--screens", f, "--out", out)
        self.assertEqual(r.returncode, 1)
        self.assertIn("defaults.PLAY: unknown action warp", r.stderr)

    def test_choice_names(self):
        """ch_name: options.json's name upper-cased, its short where it has one (a bass); NULL for no such value"""
        opts = json.loads((ROOT / "workbench/options.json").read_text())
        out = self.harness(r"""
            printf("%s\n", ch_name(ROW_KIT, 6));
            printf("%s\n", ch_name(ROW_BASS, 7));
            printf("%s\n", ch_name(ROW_COMP, 101));
            printf("%s\n", ch_name(ROW_KIT, 99) ? "named" : "NULL");
            printf("%d %d\n", ch_is_retired(ROW_KIT, 6), ch_is_retired(ROW_KIT, 20));
            printf("%d %d %d\n", ch_fits(ROW_COMP, 5, 3), ch_fits(ROW_COMP, 5, 0), ch_fits(ROW_COMP, 0, 26));
            int n = ch_count(ROW_KIT);
            printf("%d %d\n", n, ch_value(ROW_KIT, n - 1));""").splitlines()
        kit6 = next(k for k in opts["kits"] if k["id"] == 6)
        sub = next(i for i in opts["instruments"] if i["id"] == 7)
        self.assertEqual(out[0], (kit6.get("short") or kit6["name"]).upper())
        self.assertEqual(out[1], sub["short"].upper())
        self.assertEqual(out[2], "TRESILLO")
        self.assertEqual(out[3], "NULL")
        self.assertEqual(out[4], "1 0")         # kit 6 retired, 20 (Room - big) approved (19, Dusty, retired 2026-11-10)
        self.assertEqual(out[5], "1 0 0")       # strum: the guitars only; held: never the guitars
        self.assertEqual(out[6], f"{len(opts['kits'])} {opts['kits'][-1]['id']}")

    def test_song_rows(self):
        """each row's request on the song: a role's instrument, an override, the kit; none for the seed"""
        out = self.harness(r"""
            printf("%d %d\n", CH_SONG_ROW[ROW_KEYS] == SR_ROLE + V_CHORD, CH_SONG_ROW[ROW_KEYS_OCT] == SR_OV + OV_KEYS_OCT);
            printf("%d %d %d\n", CH_SONG_ROW[ROW_KIT] == SR_KIT, CH_SONG_ROW[ROW_TEMPO] == SR_ROLL + RF_TEMPO,
                   CH_SONG_ROW[ROW_SEED]);""").splitlines()
        self.assertEqual(out, ["1 1", "1 1 -1"])

    def test_check_stale(self):
        """--check: 0 on the committed files; 1 when screens.json has moved on"""
        r = gen("--check")
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        moved = json.loads(SCREENS.read_text())
        moved["screens"]["main"]["title"] = "Main, moved"
        f = Path(self.tmp.name) / "moved.json"
        f.write_text(json.dumps(moved))
        r = gen("--screens", f, "--check")
        self.assertEqual(r.returncode, 1)
        self.assertIn("stale", r.stderr)


if __name__ == "__main__":
    unittest.main()
