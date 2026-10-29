#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""The pattern import: export then import regenerates the tables byte for byte; both formats (and MuseScore's and a
   DAW's rewrites of them) import the same; each engine rule refuses with file, bar and rule and writes nothing;
   either file of a pattern may be edited, or a new pattern added as either file alone.
   python3 -I tools/patterns/test_import.py   (test_patterns.py runs these too)"""
import copy
import importlib
import json
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
import export  # noqa: E402
import midi  # noqa: E402

imp = importlib.import_module("import")
GEN = ("patterns_gen.c", "patterns_gen.h")


def entry(model, fam, k, **changes):
    e = copy.deepcopy(model[fam][k])
    e.update(changes)
    if fam == "progressions":                      # the symbols the file's text events carry
        mode = export.MODES.index(e["symbols_mode"])
        e["symbols"] = [export.ROOT_NAME[r] + s for r, s in (export.chord_symbol(c, mode) for c in e["chords"])]
    return e


def xml_variant(path, out, divisions=None, tie_beats=False, extra_staff=False, rest_bar=False, drop_meta=False):
    """MuseScore-style rewrites of an exported MusicXML file"""
    tree = ET.parse(path)
    root = tree.getroot()
    if drop_meta:
        for misc in root.iter("miscellaneous"):
            misc.clear()
    f = 1
    if divisions:
        f = divisions // 4
        for d in root.iter("divisions"):
            d.text = str(divisions)
        for d in root.iter("duration"):
            d.text = str(int(d.text) * f)
    if tie_beats:                                   # every whole note as four quarters tied across the beats
        for m in root.iter("measure"):
            kids = list(m)
            groups, cur = [], None
            for el in kids:
                if el.tag == "note" and el.find("chord") is not None and cur is not None:
                    cur.append(el)
                elif el.tag == "note":
                    cur = [el]
                    groups.append(cur)
            for g in groups:
                if g[0].find("pitch") is None or int(g[0].find("duration").text) != 16 * f:
                    continue
                at = list(m).index(g[0])
                for el in g:
                    m.remove(el)
                pieces = []
                for i in range(4):
                    for el in g:
                        n = copy.deepcopy(el)
                        stop = i > 0 or any(t.get("type") == "stop" for t in el.findall("tie"))
                        start = i < 3 or any(t.get("type") == "start" for t in el.findall("tie"))
                        for t in n.findall("tie"):
                            n.remove(t)
                        for nt in n.findall("notations"):
                            n.remove(nt)
                        n.find("duration").text = str(4 * f)
                        n.find("type").text = "quarter"
                        pos = list(n).index(n.find("duration")) + 1
                        for kind, on in (("start", start), ("stop", stop)):
                            if on:
                                t = ET.Element("tie", type=kind)
                                n.insert(pos, t)
                        pieces.append(n)
                for i, n in enumerate(pieces):
                    m.insert(at + i, n)
    parts = root.findall("part")
    if extra_staff:
        pl = root.find("part-list")
        sp = ET.SubElement(pl, "score-part", id="P9")
        ET.SubElement(sp, "part-name").text = "Piano"
        part = ET.SubElement(root, "part", id="P9")
        for i, _ in enumerate(parts[0].findall("measure")):
            m = ET.SubElement(part, "measure", number=str(i + 1))
            if i == 0:
                a = ET.SubElement(m, "attributes")
                ET.SubElement(a, "divisions").text = str(divisions or 4)
            n = ET.SubElement(m, "note")
            ET.SubElement(n, "rest", measure="yes")
            ET.SubElement(n, "duration").text = str(16 * f)
    if rest_bar:
        for part in root.findall("part"):
            m = ET.SubElement(part, "measure", number=str(len(part.findall("measure")) + 1))
            n = ET.SubElement(m, "note")
            ET.SubElement(n, "rest", measure="yes")
            ET.SubElement(n, "duration").text = str(16 * f)
    tree.write(out, encoding="UTF-8", xml_declaration=True)


class Import(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.model = json.loads((ROOT / "patterns/model.json").read_text())

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        t = Path(self.tmp.name)
        self.pat, self.gen, self.opts = t / "patterns", t / "gen", t / "options.json"
        self.pat.mkdir()
        for fam in imp.FAMS:
            shutil.copytree(ROOT / "patterns" / fam, self.pat / fam, ignore=shutil.ignore_patterns(".*"))
        for name in ("model.json", ".manifest.json"):
            shutil.copy(ROOT / "patterns" / name, self.pat / name)
        self.gen.mkdir()
        for name in GEN:
            shutil.copy(ROOT / "app/song" / name, self.gen / name)
        shutil.copy(ROOT / "workbench/options.json", self.opts)

    def tearDown(self):
        self.tmp.cleanup()

    def run_import(self, *args, pat=None):
        r = subprocess.run([sys.executable, "-I", str(HERE / "import.py"), "--patterns", str(pat or self.pat), "--gen",
                            str(self.gen), "--options", str(self.opts), *args], capture_output=True, text=True)
        self.assertEqual(r.stderr, "")
        return r.returncode, r.stdout

    def snapshot(self):
        files = [self.gen / g for g in GEN] + [self.opts, self.pat / "model.json", self.pat / ".manifest.json"]
        return {str(p): p.read_bytes() for p in files}

    def write(self, fam, k, e, ext):
        export.write_file(self.pat, fam, k, e, ext)
        return self.pat / (e["file"] + ext)

    def decode(self, path, fam, k, meta=True):
        sc = imp.score_of(Path(path), fam)
        if not meta:
            sc["meta"] = None
        return imp.content(imp.decode(fam, k, sc, imp.with_meta(fam, self.model[fam][k], sc["meta"])))

    # ------------------------------------------------------------------------------------------ the round trip
    def test_export_then_import_regenerates_the_tables_byte_for_byte(self):
        out = Path(self.tmp.name) / "exported"
        r = subprocess.run(["sh", str(HERE / "export.sh"), str(out)], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        code, so = self.run_import(pat=out)
        self.assertEqual(code, 0, so)
        for name in GEN:
            self.assertEqual((self.gen / name).read_bytes(), (ROOT / "app/song" / name).read_bytes(), name)
        self.assertEqual(self.opts.read_bytes(), (ROOT / "workbench/options.json").read_bytes())
        self.assertIn("0 pattern(s) imported", so)
        self.assertNotIn("will move", so)

    def test_every_file_in_both_formats_imports_to_its_entry(self):
        for fam in imp.FAMS:
            for k, e in self.model[fam].items():
                for ext in imp.EXTS:
                    for meta in (True, False):
                        got = self.decode(self.pat / (e["file"] + ext), fam, k, meta)
                        self.assertEqual(got, imp.content(e), (fam, k, ext, meta))

    def test_musescore_style_variants_import_the_same(self):
        picks = [("drums", "5"), ("drums", "1"), ("progressions", "1"), ("progressions", "19"),
                 ("comp-rhythms", "3"), ("comp-rhythms", "2"), ("strum", "3"), ("lead-arp", "rhythm-2"),
                 ("lead-arp", "pattern-3"), ("melody-cells", "7"), ("kick-phrases", "k6")]
        variants = {"divisions 480": dict(divisions=480), "ties across beats": dict(tie_beats=True),
                    "an extra empty staff": dict(extra_staff=True), "a rests-only bar": dict(rest_bar=True),
                    "all of them, metadata dropped": dict(divisions=480, tie_beats=True, extra_staff=True,
                                                          rest_bar=True, drop_meta=True)}
        for fam, k in picks:
            e = self.model[fam][k]
            for name, kw in variants.items():
                out = Path(self.tmp.name) / f"v-{fam}-{k}.musicxml"
                xml_variant(self.pat / (e["file"] + ".musicxml"), out, **kw)
                if kw.get("tie_beats") and fam == "progressions":
                    self.assertGreater(out.read_text().count('<tie type="start"'), 8)
                self.assertEqual(self.decode(out, fam, k), imp.content(e), (fam, k, name))

    def test_daw_style_midi_imports_the_same(self):
        """PPQ 960, notes a tick short, MuseScore's chord-symbol playback on its own channel, no metadata"""
        for fam, k in [("drums", "3"), ("progressions", "19"), ("progressions", "1"), ("strum", "2"),
                       ("lead-arp", "rhythm-4"), ("comp-rhythms", "1")]:
            e = self.model[fam][k]
            _, _, tracks, _, _, texts = export.render(fam, k, e)
            for tr in tracks:
                tr["notes"] = [(t * 2, n * 2 - 1, p, v) for t, n, p, v in tr["notes"]]
            if fam == "progressions":
                tracks.append({"name": "Chord symbols", "channel": 1, "program": None,
                               "notes": [(t, 1919, 36, 80) for t, _, _, _ in tracks[0]["notes"]]})
            out = Path(self.tmp.name) / f"daw-{fam}-{k}.mid"
            midi.write(str(out), tracks, ppq=960, title="Chords", meta=None, texts=[])
            self.assertEqual(self.decode(out, fam, k, meta=False), imp.content(e), (fam, k))

    # ------------------------------------------------------------------------------------------------ the rules
    def refused(self, path, bar, words):
        before = self.snapshot()
        code, so = self.run_import()
        self.assertEqual(code, 1, so)
        rel = "patterns/" + path.relative_to(self.pat).as_posix()
        self.assertRegex(so, rf"refused: {rel}: bar {bar}: [^\n]*{words}", so)
        self.assertIn("nothing written", so)
        self.assertEqual(self.snapshot(), before)          # patterns_gen.c and the rest untouched

    def test_a_drum_pattern_without_a_snare_is_refused(self):
        e = self.model["drums"]["1"]
        ghost = list(e["ghost"])
        hit = list(e["hit"])
        hit[1] = ghost[1] = "0x0000"
        self.refused(self.write("drums", "1", {**e, "hit": hit, "ghost": ghost}, ".mid"), 1, "snare")

    def test_a_substitution_is_refused(self):
        chords = copy.deepcopy(self.model["progressions"]["15"]["chords"])
        chords[3] = {"degree": 1, "alter": -1, "quality": "Q_DOM7"}          # VI7 -> bII7 (Db7): a tritone sub
        e = entry(self.model, "progressions", "15", chords=chords)
        self.refused(self.write("progressions", "15", e, ".musicxml"), 4, "tritone substitution")

    def test_a_borrowed_chord_in_a_loop_is_refused(self):
        chords = copy.deepcopy(self.model["progressions"]["7"]["chords"])
        chords[1] = {"degree": 2, "alter": -1, "quality": "Q_MAJ7"}          # iii7 -> bIIImaj7 in an Ionian loop
        e = entry(self.model, "progressions", "7", chords=chords)
        self.refused(self.write("progressions", "7", e, ".mid"), 2, "only a jazzy progression borrows")

    def test_a_dominant_below_the_9th_is_refused(self):
        e = self.model["progressions"]["1"]                                  # Dm7 G7, two bars each
        title, _, tracks, _, _, texts = export.render("progressions", "1", e)
        tracks[0]["notes"].append((32 * 120, 32 * 120, 68, 60))             # G7 with an Ab: a b9
        path = self.pat / (e["file"] + ".mid")
        midi.write(str(path), tracks, title=title, meta=None, texts=texts)
        self.refused(path, 3, "dominant below the 9th")

    def test_an_off_grid_note_is_refused(self):
        e = self.model["drums"]["2"]
        title, _, tracks, _, _, _ = export.render("drums", "2", e)
        tracks[0]["notes"].append((16 * 120 + 60, 60, 42, 90))              # a 32nd off, in bar 2
        path = self.pat / (e["file"] + ".mid")
        midi.write(str(path), tracks, title=title)
        self.refused(path, 2, "off the 16th grid")

    def test_an_off_grid_note_in_musicxml_is_refused(self):
        e = self.model["drums"]["2"]
        path = self.pat / (e["file"] + ".musicxml")
        xml_variant(path, path, divisions=8)
        tree = ET.parse(path)
        m2 = tree.getroot().find("part").findall("measure")[1]
        fwd = ET.Element("forward")
        ET.SubElement(fwd, "duration").text = "1"
        m2.insert(0, fwd)
        tree.write(path, encoding="UTF-8", xml_declaration=True)
        self.refused(path, 2, "off the 16th grid")

    def test_an_up_stroke_off_an_and_is_refused(self):
        e = self.model["strum"]["1"]
        self.refused(self.write("strum", "1", {**e, "steps": "D...d.u..U..d.u."}, ".musicxml"), 1, "up stroke")

    def test_a_chord_tone_index_over_4_is_refused(self):
        e = self.model["lead-arp"]["pattern-1"]
        title, _, tracks, _, _, _ = export.render("lead-arp", "pattern-1", e)
        tracks[0]["notes"][-1] = (6 * 120, 240, 72, 80)                     # C5: the 5th tone up
        path = self.pat / (e["file"] + ".mid")
        midi.write(str(path), tracks, title=title)
        self.refused(path, 1, r"chord-tone index 5 > 4")

    def test_untrusted_files_are_refused_not_crashed_on(self):
        e = self.model["drums"]["3"]
        mid = self.pat / (e["file"] + ".mid")
        mid.write_bytes(mid.read_bytes()[:40])                               # truncated
        code, so = self.run_import()
        self.assertEqual(code, 1)
        self.assertIn("patterns/drums/3-boombap.mid: unreadable", so)
        shutil.copy(ROOT / "patterns" / (e["file"] + ".mid"), mid)
        xml = self.pat / (e["file"] + ".musicxml")
        xml.write_text('<?xml version="1.0"?><!DOCTYPE x [<!ENTITY a "aaaa">]><score-partwise>&a;</score-partwise>')
        code, so = self.run_import()
        self.assertEqual(code, 1)
        self.assertIn("patterns/drums/3-boombap.musicxml: unreadable", so)

    # ------------------------------------------------------------------------------- either file may be edited
    def imported(self, args=()):
        code, so = self.run_import(*args)
        self.assertEqual(code, 0, so)
        return so, json.loads((self.pat / "model.json").read_text()), json.loads(
            (self.pat / ".manifest.json").read_text())

    def assert_in_step(self, fam, k, model, manifest):
        e = model[fam][k]
        for ext in imp.EXTS:
            p = self.pat / (e["file"] + ext)
            self.assertEqual(manifest[e["file"] + ext], imp.sha(p), ext)
            sc = imp.score_of(p, fam)
            got = imp.decode(fam, k, sc, imp.with_meta(fam, e, sc["meta"]))
            self.assertEqual(imp.content(got), imp.content(e), ext)

    def test_only_the_mid_changed_imports_from_it_and_rewrites_the_musicxml(self):
        e = self.model["drums"]["1"]
        hit = list(e["hit"])
        hit[0] = "0x0101"
        mid = self.write("drums", "1", {**e, "hit": hit}, ".mid")
        sam = mid.read_bytes()
        so, model, man = self.imported()
        self.assertEqual(model["drums"]["1"]["hit"][0], "0x0101")
        self.assertEqual(mid.read_bytes(), sam)                              # Sam's file is never rewritten
        self.assertIn("wrote patterns/drums/1-boombap.musicxml from patterns/drums/1-boombap.mid", so)
        self.assertIn("approved drums 1 changed", so)
        self.assertIn("will move", so)
        self.assertIn("{1, PF_BOOMBAP, {0x0101,", (self.gen / "patterns_gen.c").read_text())
        self.assert_in_step("drums", "1", model, man)
        code, so = self.run_import()                                         # and again: nothing left to do
        self.assertIn("0 pattern(s) imported", so)

    def test_only_the_musicxml_changed_imports_from_it_and_rewrites_the_mid(self):
        e = self.model["melody-cells"]["1"]
        xml = self.write("melody-cells", "1", {**e, "mask": "0x3", "onsets": [0, 1]}, ".musicxml")
        sam = xml.read_bytes()
        so, model, man = self.imported()
        self.assertEqual(model["melody-cells"]["1"]["mask"], "0x3")
        self.assertEqual(xml.read_bytes(), sam)
        self.assertIn("wrote patterns/melody-cells/1-1.mid from patterns/melody-cells/1-1.musicxml", so)
        self.assertIn("MELODY_CELLS[MELODY_NCELLS] = {0x3, 0x4", (self.gen / "patterns_gen.c").read_text())
        self.assert_in_step("melody-cells", "1", model, man)

    def test_both_changed_and_disagreeing_is_refused_naming_both(self):
        e = self.model["strum"]["2"]
        self.write("strum", "2", {**e, "steps": "D.u.X.U.d.u.X..."}, ".mid")
        self.write("strum", "2", {**e, "steps": "D.u.X...d.u.X.U."}, ".musicxml")
        before = self.snapshot()
        code, so = self.run_import()
        self.assertEqual(code, 1)
        self.assertIn("refused: patterns/strum/2-chuck.mid and patterns/strum/2-chuck.musicxml: both files changed "
                      "and they disagree (steps)", so)
        self.assertEqual(self.snapshot(), before)

    def test_both_changed_and_agreeing_imports_and_keeps_both(self):
        e = self.model["strum"]["2"]
        e2 = {**e, "steps": "D.u.X.U.d.u.X..."}
        mid, xml = self.write("strum", "2", e2, ".mid"), self.write("strum", "2", e2, ".musicxml")
        xml.write_bytes(xml.read_bytes().replace(b"<work-title>", b"<work-title>Sam: "))
        sam = mid.read_bytes(), xml.read_bytes()
        so, model, man = self.imported()
        self.assertEqual(model["strum"]["2"]["steps"], "D.u.X.U.d.u.X...")
        self.assertEqual((mid.read_bytes(), xml.read_bytes()), sam)
        self.assertNotIn("wrote patterns/strum", so)
        self.assert_in_step("strum", "2", model, man)

    def test_a_new_pattern_as_a_mid_alone_is_a_candidate(self):
        e = {**self.model["drums"]["7"], "file": "drums/sam-groove", "src": "x"}
        hit = list(e["hit"])
        hit[0] = "0x0111"
        title, _, tracks, _, _, _ = export.render("drums", "7", {**e, "hit": hit})
        midi.write(str(self.pat / "drums/sam-groove.mid"), tracks, title="My groove")    # no metadata
        so, model, man = self.imported()
        new = model["drums"]["9"]
        self.assertEqual((new["status"], new["file"], new["hit"][0], new["src"]),
                         ("candidate", "drums/sam-groove", "0x0111", "imported from patterns/drums/sam-groove"))
        self.assertTrue((self.pat / "drums/sam-groove.musicxml").is_file())
        self.assertIn("new drums 9 (drums/sam-groove), a candidate", so)
        c = (self.gen / "patterns_gen.c").read_text()
        self.assertIn("{9, PF_BOOMBAP, {0x0111,", c)
        self.assertIn("LIB_PATTERNS_STATUS[] = {1, 1, 1, 1, 1, 1, 1, 1, 0};", c)
        self.assert_in_step("drums", "9", model, man)

    def test_a_new_pattern_as_a_musicxml_alone_takes_its_modes_from_the_title(self):
        e = entry(self.model, "progressions", "17", file="progressions/sam-vamp")
        path = self.write("progressions", "17", e, ".musicxml")
        xml_variant(path, path, drop_meta=True)
        text = path.read_text().replace("(vamp; wistful, warm, sunny; Dorian)", "(loop; warm; Dorian, Aeolian)")
        path.write_text(text)
        so, model, man = self.imported()
        new = model["progressions"]["45"]
        self.assertEqual((new["status"], new["kind"], new["moods"], new["modes"], new["symbols"]),
                         ("candidate", "PK_LOOP", ["MOOD_WARM"], ["MODE_DORIAN", "MODE_AEOLIAN"], ["Cm7", "F7"]))
        self.assertTrue((self.pat / "progressions/sam-vamp.mid").is_file())
        self.assert_in_step("progressions", "45", model, man)

    def test_a_kick_phrase_edit_regenerates_options_json(self):
        e = self.model["kick-phrases"]["k3"]
        self.write("kick-phrases", "k3", {**e, "b": [0, 7, 11]}, ".mid")
        old = self.opts.read_text()
        so, model, _ = self.imported()
        new = self.opts.read_text()
        self.assertIn('{ "id": "k3", "a": [0, 7, 10], "b": [0, 7, 11] }', new)
        self.assertEqual(new.replace('"b": [0, 7, 11]', '"b": [0, 7, 10]', 1), old)
        self.assertNotIn("will move", so)                                    # the workbench's, not the firmware's

    def test_check_writes_nothing(self):
        e = self.model["drums"]["1"]
        hit = list(e["hit"])
        hit[0] = "0x0101"
        self.write("drums", "1", {**e, "hit": hit}, ".mid")
        before = self.snapshot()
        xml = (self.pat / (e["file"] + ".musicxml")).read_bytes()
        code, so = self.run_import("--check")
        self.assertEqual(code, 0, so)
        self.assertIn("approved drums 1 changed", so)
        self.assertIn("nothing written", so)
        self.assertEqual(self.snapshot(), before)
        self.assertEqual((self.pat / (e["file"] + ".musicxml")).read_bytes(), xml)


if __name__ == "__main__":
    unittest.main()
