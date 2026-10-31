#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""The pattern export: every table entry gets a .mid and a .musicxml, the MIDI reads back to the model's hits, the
   MusicXML is MuseScore-shaped, the committed patterns/ matches a fresh export, the README covers every family. The
   import's tests (test_import.py) run here too.
   python3 -I tools/patterns/test_patterns.py"""
import hashlib
import json
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(HERE))
import midi  # noqa: E402
from test_import import Import, ImportWithCandidates, imp  # noqa: E402,F401  (run with these)
from test_pins import Pins  # noqa: E402,F401

FAMILIES = {"drums": 8, "kick-phrases": 5, "progressions": 44, "comp-rhythms": 5, "strum": 5}
GM = {"kick": 36, "snare": 38, "hat": 42, "ohat": 46, "shaker": 70, "rim": 37}
LANES = ["kick", "snare", "hat", "ohat", "shaker", "rim"]
STROKE = {"T": 64, "B": 65, "D": 67, "d": 69, "X": 71, "U": 72, "u": 74, "P": 76}
TONE = [60, 64, 67, 71]                       # the reference Cmaj7, C4 E4 G4 B4: chord tones 1-4
SIXTEENTH = 120                               # ticks at PPQ 480


def steps(mask):
    return [i for i in range(16) if (mask >> i) & 1]


def expected(family, e):
    """the (step, pitch) onsets a file must hold, over all its bars, from the model entry alone"""
    out = []
    if family == "drums":
        for bar in range(2):
            b = bar == 1 and e["two_bar"]
            for lane, name in enumerate(LANES):
                if e["poly3"] and name == "shaker":
                    out += [(bar * 16 + s, GM[name]) for s in range(16) if (bar * 16 + s) % 3 == 0]
                    continue
                hit = int((e["hit_b"] if b else e["hit"])[lane], 16)
                ghost = int((e["ghost_b"] if b else e["ghost"])[lane], 16) & ~hit
                out += [(bar * 16 + s, GM[name]) for s in steps(hit | ghost)]
    elif family == "kick-phrases":
        out = [(s, 36) for s in e["a"]] + [(16 + s, 36) for s in e["b"]]
    elif family == "strum":
        out = [(i, STROKE[c]) for i, c in enumerate(e["steps"]) if c != "."]
    elif family == "comp-rhythms":
        if e["name"] == "stabs":
            for bar, s2 in enumerate(e["second"]):
                out += [(bar * 16 + s, p) for s in (e["first"], s2) for p in TONE]
        else:
            out = [(k["step"], p) for k in e["strikes"] for p in TONE]
    elif family == "progressions":
        pcs = {"C": 0, "Db": 1, "D": 2, "Eb": 3, "E": 4, "F": 5, "F#": 6, "G": 7, "Ab": 8, "A": 9, "Bb": 10, "B": 11}
        shape = {"maj7": [0, 4, 7, 11], "7": [0, 4, 7, 10], "m7": [0, 3, 7, 10], "m7b5": [0, 3, 6, 10]}
        for i, sym in enumerate(e["symbols"]):
            name, suffix = re.fullmatch(r"([A-G][b#]?)(maj7|m7b5|m7|7)", sym).groups()
            root = pcs[name] + (48 if pcs[name] >= 5 else 60)
            out += [(i * e["bars_per_chord"] * 16, root + x) for x in shape[suffix]]
    return sorted(out)


def xml_onsets(path):
    """(step, pitch) for every sounding note: unpitched by its MIDI unpitched number, pitched by step/alter/octave"""
    root = ET.parse(path).getroot()
    unp = {}
    for si in root.iter("midi-instrument"):
        unp[si.get("id")] = int(si.find("midi-unpitched").text) - 1 if si.find("midi-unpitched") is not None else None
    names = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}
    out = []
    for part in root.iter("part"):
        t = 0
        for m in part.iter("measure"):
            start = t
            for n in m:
                if n.tag != "note":
                    continue
                if n.find("chord") is None:
                    start = t
                    t += int(n.find("duration").text)
                if n.find("rest") is not None:
                    continue
                if any(x.get("type") == "stop" for x in n.findall("tie")):
                    continue
                if n.find("unpitched") is not None:
                    out.append((start, unp[n.find("instrument").get("id")]))
                else:
                    p = n.find("pitch")
                    alt = int(p.find("alter").text) if p.find("alter") is not None else 0
                    out.append((start, 12 * (int(p.find("octave").text) + 1) + names[p.find("step").text] + alt))
    return sorted(out)


class T(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.out = Path(cls.tmp.name) / "patterns"
        cls.out.mkdir()
        shutil.copy(ROOT / "patterns/model.json", cls.out / "model.json")     # the entries' file names, as committed
        r = subprocess.run(["sh", str(HERE / "export.sh"), str(cls.out)], capture_output=True, text=True)
        if r.returncode:
            raise RuntimeError("export.sh failed:\n" + r.stdout + r.stderr)
        cls.model = json.loads((cls.out / "model.json").read_text())

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def entries(self):
        for fam in FAMILIES:
            for k, e in self.model[fam].items():
                yield fam, k, e

    def test_counts_and_status(self):
        for fam, n in FAMILIES.items():
            self.assertGreaterEqual(sum(e["status"] == "approved" for e in self.model[fam].values()), n, fam)
        for fam, k, e in self.entries():
            self.assertIn(e["status"], ("approved", "candidate"), (fam, k))

    def test_model_matches_sources(self):
        lib = (ROOT / "app/song/patterns_gen.c").read_text()
        d = self.model["drums"]
        self.assertEqual(d["1"]["hit"][0], "0x4441")
        self.assertEqual((d["5"]["poly3"], d["5"]["hit"][4]), (1, "0x9249"))
        self.assertEqual(self.model["progressions"]["19"]["chords"][2], {"degree": 5, "alter": -1, "quality": "Q_MAJ7"})
        self.assertEqual(len(self.model["progressions"]), len(re.findall(r"^    \{\d+, PK_", lib, re.M)))
        self.assertEqual(self.model["strum"]["1"]["steps"], "D...d.u...U.d.u.")
        self.assertEqual(self.model["strum"]["5"], {**self.model["strum"]["5"], "kind": "fill", "fill_of": 4,
                                                    "steps": "D.....u.d......."})
        self.assertEqual(self.model["kick-phrases"]["k6"]["b"], [0, 3, 10])
        stabs = self.model["comp-rhythms"]["3"]
        self.assertEqual((stabs["second"], stabs["busy_steps"], stabs["busy_over"]), ([6, 10, 4, 12], [0, 6], 3))
        cr = self.model["comp-rhythms"]
        h2, l8, h24 = cr["4"], cr["5"], cr["6"]   # held on 2, light eighths, held on 2 and 4
        self.assertEqual((h2["file"], h2["strikes"], "second_vel_pct" in h2),
                         ("comp-rhythms/4-held-on-2", [{"step": 4, "len": 12}], False))
        self.assertEqual((h24["file"], h24["strikes"], h24["second_vel_pct"]),
                         ("comp-rhythms/6-held-on-2-and-4", [{"step": 4, "len": 8}, {"step": 12, "len": 4}], 70))
        self.assertEqual((l8["file"], [s["step"] for s in l8["strikes"]]),
                         ("comp-rhythms/5-light-eighths", list(range(0, 16, 2))))
        for e in cr.values():   # no busy-lead rule for the new rhythms; opt_pct is gone
            self.assertNotIn("opt_pct", e)
            if e["name"] != "stabs":
                self.assertNotIn("busy_steps", e)
        self.assertLess(l8["vel_range"][1], self.model["comp-rhythms"]["2"]["vel_range"][0])
        # the lead-arp and melody-cells families went with the hybrid lead and the struck melody (lead styles Task 6)
        self.assertFalse({"lead-arp", "melody-cells"} & set(self.model))
        self.assertFalse((ROOT / "patterns/lead-arp").exists() or (ROOT / "patterns/melody-cells").exists())

    def test_every_entry_has_both_files(self):
        n = 0
        for fam, k, e in self.entries():
            stem = e["file"]                                     # a new pattern keeps the name Sam gave its file
            self.assertTrue(stem.startswith(fam + "/" + (k + "-" if e["status"] == "approved" else "")), stem)
            for ext in (".mid", ".musicxml"):
                self.assertTrue((self.out / (stem + ext)).is_file(), stem + ext)
                n += 1
        files = {p.relative_to(self.out).as_posix() for p in self.out.rglob("*") if p.suffix in (".mid", ".musicxml")}
        self.assertEqual(len(files), n)

    def test_midi_reads_back_to_the_hits(self):
        for fam, k, e in self.entries():
            m = midi.read(self.out / (e["file"] + ".mid"))
            self.assertEqual(m["ppq"], 480)
            self.assertEqual(m["tempo_bpm"], 85)
            self.assertEqual(m["time_sig"], (4, 4))
            got = sorted((n["tick"] // SIXTEENTH, n["note"]) for n in m["notes"])
            self.assertTrue(all(n["tick"] % SIXTEENTH == 0 for n in m["notes"]), (fam, k))
            self.assertEqual(got, expected(fam, e), (fam, k))
            self.assertTrue(all(25 <= n["vel"] <= 110 for n in m["notes"]), (fam, k))
            self.assertEqual(json.loads(m["meta"])["id"], k)

    def test_drum_hits_and_ghosts_by_velocity(self):
        e = self.model["drums"]["1"]
        m = midi.read(self.out / (e["file"] + ".mid"))
        hats = [(n["tick"] // SIXTEENTH, n["vel"] > 60) for n in m["notes"] if n["note"] == 42 and n["tick"] < 1920]
        self.assertEqual(hats, [(s, s % 2 == 0) for s in range(16)])
        self.assertTrue(all(n["channel"] == 9 for n in m["notes"]))

    def test_progression_chord_symbols(self):
        e = self.model["progressions"]["19"]
        self.assertEqual(e["symbols"], ["Cm7", "Bb7", "Abmaj7", "G7"])
        root = ET.parse(self.out / (e["file"] + ".musicxml")).getroot()
        h = root.findall(".//harmony")
        self.assertEqual(len(h), 4)
        self.assertEqual((h[2].find("root/root-step").text, h[2].find("root/root-alter").text,
                          h[2].find("kind").text), ("A", "-1", "major-seventh"))
        m = midi.read(self.out / (e["file"] + ".mid"))
        self.assertEqual(m["texts"], e["symbols"])
        two = self.model["progressions"]["1"]                       # 2 bars a chord: tied across the bar line
        root = ET.parse(self.out / (two["file"] + ".musicxml")).getroot()
        self.assertEqual(len(root.findall(".//measure")), 4)
        self.assertTrue(root.findall(".//tie[@type='start']"))

    def test_musicxml_is_musescore_shaped(self):
        for fam, k, e in self.entries():
            path = self.out / (e["file"] + ".musicxml")
            root = ET.parse(path).getroot()
            self.assertEqual((root.tag, root.get("version")), ("score-partwise", "4.0"))
            self.assertIsNotNone(root.find("part-list/score-part"))
            self.assertIn("<!DOCTYPE score-partwise", path.read_text())
            self.assertEqual(json.loads(root.find(".//miscellaneous-field[@name='vnl1']").text)["id"], k)
            for part in root.iter("part"):
                ms = part.findall("measure")
                a = ms[0].find("attributes")
                self.assertEqual(a.find("divisions").text, "4")
                self.assertEqual((a.find("time/beats").text, a.find("time/beat-type").text), ("4", "4"))
                self.assertIsNotNone(a.find("clef/sign"))
                for m in ms:
                    total = sum(int(n.find("duration").text) for n in m.findall("note") if n.find("chord") is None)
                    self.assertEqual(total, 16, (fam, k, m.get("number")))
            self.assertEqual(xml_onsets(path), expected(fam, e), (fam, k))

    def test_manifest(self):
        man = json.loads((self.out / ".manifest.json").read_text())
        self.assertEqual(len(man), 2 * sum(len(self.model[f]) for f in FAMILIES))
        for rel, sha in man.items():
            self.assertEqual(hashlib.sha256((self.out / rel).read_bytes()).hexdigest(), sha, rel)

    def pattern_files(self, d):
        return {p.relative_to(d).as_posix() for p in d.rglob("*") if p.suffix in (".mid", ".musicxml")}

    def reexport(self, tables, out):
        tj = Path(self.tmp.name) / "tables.json"
        tj.write_text(json.dumps(tables))
        r = subprocess.run([sys.executable, "-I", str(HERE / "export.py"), str(tj), str(out)], capture_output=True,
                           text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        return r.stdout

    def copy(self, name):
        d = Path(self.tmp.name) / name
        shutil.copytree(self.out, d)
        return d

    def test_committed_file_set_equals_a_fresh_export(self):
        """every entry's files are committed; any other file is a new one Sam has not imported yet (no manifest
           entry)"""
        committed = self.pattern_files(ROOT / "patterns")
        fresh = self.pattern_files(self.out)
        self.assertEqual(fresh - committed, set())
        man = json.loads((ROOT / "patterns/.manifest.json").read_text())
        self.assertEqual({f for f in committed - fresh if f in man}, set())

    def test_a_content_change_keeps_the_file_names(self):
        d = self.copy("changed")
        before = self.pattern_files(d)
        tables = json.loads((ROOT / "app/build/patterns/tables.json").read_text())
        tables["progs"][0]["c"][0] = [3, 0, 3]                  # prog 1: Dm7 -> Fmaj7, its slug says dm7
        old = (d / "progressions/1-vamp-dm7-g7.mid").read_bytes()
        self.reexport(tables, d)
        self.assertEqual(self.pattern_files(d), before)
        m = json.loads((d / "model.json").read_text())
        self.assertEqual(m["progressions"]["1"]["file"], "progressions/1-vamp-dm7-g7")
        self.assertEqual(m["progressions"]["1"]["symbols"], ["Fmaj7", "G7"])
        self.assertNotEqual((d / "progressions/1-vamp-dm7-g7.mid").read_bytes(), old)
        man = json.loads((d / ".manifest.json").read_text())
        self.assertEqual(man["progressions/1-vamp-dm7-g7.mid"],
                         hashlib.sha256((d / "progressions/1-vamp-dm7-g7.mid").read_bytes()).hexdigest())

    def test_an_edited_file_is_left_untouched(self):
        d = self.copy("edited")
        rel = "drums/1-boombap.musicxml"
        edited = (d / rel).read_bytes().replace(b"<work-title>", b"<work-title>Sam's ")
        (d / rel).write_bytes(edited)
        before_sha = json.loads((d / ".manifest.json").read_text())[rel]
        tables = json.loads((ROOT / "app/build/patterns/tables.json").read_text())
        tables["patterns"][0]["hit"][0] = 0x0101                # the table changed too: still not overwritten
        out = self.reexport(tables, d)
        self.assertEqual((d / rel).read_bytes(), edited)
        self.assertIn("skipped " + rel, out)
        self.assertEqual(json.loads((d / ".manifest.json").read_text())[rel], before_sha)
        self.assertNotEqual((d / "drums/1-boombap.mid").read_bytes(), (self.out / "drums/1-boombap.mid").read_bytes())

    def test_committed_patterns_match_a_fresh_export(self):
        """model.json byte for byte; each pattern file byte for byte, or (a file Sam edited and imported, kept as he
           saved it) reading back to its entry"""
        committed = ROOT / "patterns"
        self.assertEqual((committed / "model.json").read_bytes(), (self.out / "model.json").read_bytes())
        for fam, k, e in self.entries():
            for ext in (".mid", ".musicxml"):
                rel = e["file"] + ext
                if (committed / rel).read_bytes() == (self.out / rel).read_bytes():
                    continue
                sc = imp.score_of(committed / rel, fam)
                got = imp.decode(fam, k, sc, imp.with_meta(fam, e, sc["meta"]))
                self.assertEqual(imp.content(got), imp.content(e), rel)

    def test_readme_lists_each_family(self):
        text = (ROOT / "patterns/README.md").read_text()
        for fam in FAMILIES:
            self.assertRegex(text, r"(?m)^## " + re.escape(fam) + r"\b", fam)
        for word in ("GM", "ghost", "poly3", "chord symbols", "Cmaj7", "16th", "85", ".manifest.json", "model.json"):
            self.assertIn(word, text)


if __name__ == "__main__":
    unittest.main()
