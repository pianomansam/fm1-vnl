#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""The pattern export: every table entry gets a .mid and a .musicxml, the MIDI reads back to the model's hits, the
   MusicXML is MuseScore-shaped, the committed patterns/ matches a fresh export, the README covers every family.
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

FAMILIES = {"drums": 8, "kick-phrases": 5, "progressions": 44, "comp-rhythms": 3, "strum": 5, "lead-arp": 18,
            "melody-cells": 7}
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
    elif family == "lead-arp":
        if e["kind"] == "rhythm":
            out = [(at, TONE[0]) for at, _ in e["notes"]]
        elif e["kind"] == "pattern":
            out = [(i * 2, TONE[t]) for i, t in enumerate(e["tones"])]
        else:
            out = [(e["at"], TONE[0])]
    elif family == "progressions":
        pcs = {"C": 0, "Db": 1, "D": 2, "Eb": 3, "E": 4, "F": 5, "F#": 6, "G": 7, "Ab": 8, "A": 9, "Bb": 10, "B": 11}
        shape = {"maj7": [0, 4, 7, 11], "7": [0, 4, 7, 10], "m7": [0, 3, 7, 10], "m7b5": [0, 3, 6, 10]}
        for i, sym in enumerate(e["symbols"]):
            name, suffix = re.fullmatch(r"([A-G][b#]?)(maj7|m7b5|m7|7)", sym).groups()
            root = pcs[name] + (48 if pcs[name] >= 5 else 60)
            out += [(i * e["bars_per_chord"] * 16, root + x) for x in shape[suffix]]
    elif family == "melody-cells":
        out = [(s, TONE[0]) for s in range(4) if (int(e["mask"], 16) >> s) & 1]
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
            self.assertEqual(len(self.model[fam]), n, fam)
        for fam, k, e in self.entries():
            self.assertEqual(e["status"], "approved", (fam, k))

    def test_model_matches_sources(self):
        lib = (ROOT / "app/song/library.c").read_text()
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
        la = self.model["lead-arp"]
        self.assertEqual(la["rhythm-2"]["notes"], [[0, 3], [3, 1], [4, 3], [7, 1], [8, 3], [11, 1], [12, 4]])
        self.assertEqual(la["pattern-3"]["tones"], [0, 1, 2, 3, 2, 1])
        self.assertEqual([la[f"landing-{i}"]["at"] for i in range(1, 7)], [0, 0, 2, 3, 2, 4])
        self.assertEqual([la[f"landing-{i}"]["weight"] for i in range(1, 7)], [25, 30, 10, 5, 20, 10])
        self.assertEqual([c["mask"] for c in self.model["melody-cells"].values()],
                         ["0x1", "0x4", "0x5", "0x9", "0x8", "0x6", "0xD"])

    def test_every_entry_has_both_files(self):
        for fam, k, e in self.entries():
            stem = e["file"]
            self.assertTrue(stem.startswith(fam + "/" + k + "-"), stem)
            for ext in (".mid", ".musicxml"):
                self.assertTrue((self.out / (stem + ext)).is_file(), stem + ext)
        files = {p.relative_to(self.out).as_posix() for p in self.out.rglob("*") if p.suffix in (".mid", ".musicxml")}
        self.assertEqual(len(files), 2 * sum(FAMILIES.values()))

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
        self.assertEqual(len(man), 2 * sum(FAMILIES.values()))
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
        self.assertEqual(self.pattern_files(ROOT / "patterns"), self.pattern_files(self.out))

    def test_a_content_change_keeps_the_file_names(self):
        d = self.copy("changed")
        before = self.pattern_files(d)
        tables = json.loads((ROOT / "app/build/patterns/tables.json").read_text())
        tables["progs"][0]["c"][0] = [3, 0, 3]                  # prog 1: Dm7 -> Fmaj7, its slug says dm7
        tables["arp_patterns"][0] = [3, 2, 1, 0]
        tables["melody_cells"][0][0] = 3
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
        committed = ROOT / "patterns"
        for p in self.out.rglob("*"):
            if p.is_file():
                rel = p.relative_to(self.out)
                self.assertTrue((committed / rel).is_file(), rel)
                self.assertEqual((committed / rel).read_bytes(), p.read_bytes(), rel)

    def test_readme_lists_each_family(self):
        text = (ROOT / "patterns/README.md").read_text()
        for fam in FAMILIES:
            self.assertRegex(text, r"(?m)^## " + re.escape(fam) + r"\b", fam)
        for word in ("GM", "ghost", "poly3", "chord symbols", "Cmaj7", "16th", "85", ".manifest.json", "model.json"):
            self.assertIn(word, text)


if __name__ == "__main__":
    unittest.main()
