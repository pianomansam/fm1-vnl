#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""prep_kits.py on a tiny SFZ kit (fixtures/tinykit): the files it writes and the index.
   python3 -I workbench/tools/test_prep_kits.py"""
import array
import importlib.util
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("prep_kits", HERE / "prep_kits.py")
prep = importlib.util.module_from_spec(spec)
spec.loader.exec_module(prep)

ROOT = HERE / "fixtures" / "tinykit"
KIT = {"id": 0, "name": "tiny", "title": "Tiny", "kit": 19, "program": "Programs/kit.sfz", "cc": {}, "swap": {},
       "pieces": [{"id": 0, "name": "kick", "key": 36}, {"id": 1, "name": "snare", "key": 38},
                  {"id": 2, "name": "rim", "same": 1, "vel": 0.45}, {"id": 3, "name": "hat", "key": 42},
                  {"id": 4, "name": "ohat", "key": 46, "cc": {4: 127}}],
       "lanes": [0, 1, 3, 4, None, 2, 3]}
BRUSH = dict(KIT, id=2, name="tinybrush", swap={"mappings/snare.sfz": "mappings/snare_brush.sfz"})


def f32(path):
    a = array.array("f")
    a.frombytes(path.read_bytes())
    if sys.byteorder == "big":
        a.byteswap()
    return a


class T(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.out = Path(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def test_files_and_index(self):
        e = prep.build(KIT, ROOT, self.out)
        kick = e["pieces"][0]
        # two layers (tops 63, 127), two round robins each; the default curve (vel squared)
        self.assertEqual([l["hi"] for l in kick["layers"]], [63, 127])
        self.assertEqual(kick["curve"], 2)
        self.assertEqual([len(l["files"]) for l in kick["layers"]], [2, 2])
        # mixed to mono at the layer's top velocity: k1 at (63/127)^2; k3 plus the second mic at cc101 64/127
        v1r1, v2r1, v2r2 = (f32(self.out / kick["layers"][a]["files"][b]) for a, b in ((0, 0), (1, 0), (1, 1)))
        self.assertAlmostEqual(v1r1[0], 0.25 * (63 / 127) ** 2, places=5)
        self.assertAlmostEqual(v2r1[0], 0.5 + 0.5 * 64 / 127, places=5)
        self.assertAlmostEqual(v2r2[0], 0.625, places=5)
        self.assertEqual(len(v2r1), 64)
        # six hat layers: at most four kept (the softest, the loudest, two between), one round robin, amp_velcurve: linear
        hat = e["pieces"][3]
        self.assertEqual([l["hi"] for l in hat["layers"]], [20, 60, 80, 127])
        self.assertEqual(hat["curve"], 1)
        self.assertTrue(all(len(l["files"]) == 1 for l in hat["layers"]))
        self.assertAlmostEqual(f32(self.out / hat["layers"][1]["files"][0])[0], 0.375 * 60 / 127, places=5)
        # the open hat at cc4 127 plays the open sample
        self.assertAlmostEqual(f32(self.out / e["pieces"][4]["layers"][0]["files"][0])[0], 0.75, places=5)
        # the rim is the snare at 45%: no files of its own
        self.assertEqual({k: e["pieces"][2][k] for k in ("same", "vel")}, {"same": 1, "vel": 45})
        self.assertNotIn("layers", e["pieces"][2])
        self.assertEqual(e["lanes"], [0, 1, 3, 4, None, 2, 3])
        # every file the index names is there, and nothing else
        named = {f for p in e["pieces"] for l in p.get("layers", []) for f in l["files"]}
        self.assertEqual(named, {p.name for p in self.out.glob("*.f32")})
        self.assertTrue(all(f.startswith("tiny-") for f in named))

    def test_swap_and_index_file(self):
        prep.write_index([prep.build(KIT, ROOT, self.out), prep.build(BRUSH, ROOT, self.out)], self.out)
        idx = json.loads((self.out / "kits.json").read_text())
        self.assertEqual([k["name"] for k in idx["kits"]], ["tiny", "tinybrush"])
        sn = idx["kits"][1]["pieces"][1]["layers"][0]["files"][0]
        self.assertAlmostEqual(f32(self.out / sn)[0], 0.25, places=5)   # the brush map's snare

    def test_needed_and_missing(self):
        need = prep.needed(KIT, ROOT)
        self.assertIn("Samples/km.wav", need)
        self.assertNotIn("Samples/sb.wav", need)
        broken = Path(self.tmp.name) / "broken"
        shutil.copytree(ROOT, broken)
        (broken / "Samples" / "km.wav").unlink()
        with self.assertRaisesRegex(FileNotFoundError, "km.wav"):
            prep.build(KIT, broken, self.out / "x")


if __name__ == "__main__":
    unittest.main()
