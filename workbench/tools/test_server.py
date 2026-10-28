#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""server.py's writes: feedback lines, kits, patterns (into a temp folder, never workbench/).
   python3 workbench/tools/test_server.py"""
import http.client
import importlib.util
import json
import tempfile
import threading
import unittest
from functools import partial
from http.server import ThreadingHTTPServer
from pathlib import Path

spec = importlib.util.spec_from_file_location("wbserver", Path(__file__).resolve().parent.parent / "server.py")
server = importlib.util.module_from_spec(spec)
spec.loader.exec_module(server)


class T(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        server.DATA = Path(cls.tmp.name)
        server.Handler.log_message = lambda *a: None
        cls.srv = ThreadingHTTPServer(("127.0.0.1", 0), partial(server.Handler, directory=cls.tmp.name))
        threading.Thread(target=cls.srv.serve_forever, daemon=True).start()

    @classmethod
    def tearDownClass(cls):
        cls.srv.shutdown()
        cls.srv.server_close()
        cls.tmp.cleanup()

    def post(self, path, body, raw=None, headers=None):
        c = http.client.HTTPConnection("127.0.0.1", self.srv.server_address[1])
        c.request("POST", path, raw if raw is not None else json.dumps(body), {"Content-Type": "application/json", **(headers or {})})
        r = c.getresponse()
        out = r.status, json.loads(r.read())
        c.close()
        return out

    def test_feedback_appends_lines(self):
        self.assertEqual(self.post("/feedback", {"rating": "up", "note": "x"}), (200, {"ok": True}))
        self.assertEqual(self.post("/feedback", {"rating": "down", "note": "é"})[0], 200)
        lines = (server.DATA / "feedback.jsonl").read_text(encoding="utf-8").splitlines()
        self.assertEqual([json.loads(x)["rating"] for x in lines], ["up", "down"])

    def test_kit_and_pattern_names(self):
        self.assertEqual(self.post("/kit", {"name": "my-kit-2", "lanes": []})[0], 200)
        self.assertEqual(json.loads((server.DATA / "kits" / "my-kit-2.json").read_text())["lanes"], [])
        self.assertEqual(self.post("/pattern", {"name": "p1"})[0], 200)
        self.assertTrue((server.DATA / "patterns" / "p1.json").exists())
        for bad in ("../x", "Kit", "a b", "", "a" * 41, "x.json", 5, None):
            self.assertEqual(self.post("/kit", {"name": bad})[0], 400, bad)
        self.assertEqual(self.post("/kit", {}, )[0], 400)
        self.assertEqual([p.name for p in (server.DATA / "kits").iterdir()], ["my-kit-2.json"])

    def test_same_id_written_once(self):
        for _ in range(2):
            self.assertEqual(self.post("/feedback", {"id": "x-1", "note": "once"})[0], 200)
        lines = (server.DATA / "feedback.jsonl").read_text(encoding="utf-8").splitlines()
        self.assertEqual(sum('"x-1"' in x for x in lines), 1)

    def test_only_this_page(self):
        port = self.srv.server_address[1]
        self.assertEqual(self.post("/feedback", None, raw="{}", headers={"Content-Type": "text/plain"})[0], 415)
        self.assertEqual(self.post("/feedback", {}, headers={"Origin": "https://evil.example"})[0], 403)
        self.assertEqual(self.post("/feedback", {}, headers={"Origin": "null"})[0], 403)
        self.assertEqual(self.post("/feedback", {}, headers={"Host": "evil.example:%d" % port})[0], 403)
        self.assertEqual(self.post("/feedback", {}, headers={"Host": "127.0.0.1:1"})[0], 403)
        self.assertEqual(self.post("/feedback", {}, headers={"Origin": f"http://localhost:{port}", "Host": f"localhost:{port}"})[0], 200)
        self.assertEqual(self.post("/kit", {"name": "k"}, headers={"Origin": "http://evil.example"})[0], 403)

    def test_bad_requests(self):
        self.assertEqual(self.post("/feedback", None, raw="nope")[0], 400)
        self.assertEqual(self.post("/feedback", None, raw="[1]")[0], 400)
        c = http.client.HTTPConnection("127.0.0.1", self.srv.server_address[1])   # the claim alone is refused
        c.putrequest("POST", "/feedback")
        c.putheader("Content-Type", "application/json")
        c.putheader("Content-Length", str(server.MAX_BODY + 1))
        c.endheaders()
        r = c.getresponse()
        self.assertEqual(r.status, 413)
        r.read()
        c.close()
        self.assertEqual(self.post("/other", {})[0], 404)

    def test_bad_content_length(self):
        before = sorted(p.name for p in server.DATA.rglob("*") if p.is_file())
        for path in ("/feedback", "/kit"):
            for length, want in (("-1", 400), ("abc", 400), (None, 411)):
                c = http.client.HTTPConnection("127.0.0.1", self.srv.server_address[1])
                c.putrequest("POST", path)
                c.putheader("Content-Type", "application/json")
                if length is not None:
                    c.putheader("Content-Length", length)
                c.endheaders()
                r = c.getresponse()
                self.assertEqual(r.status, want, (path, length))
                r.read()
                c.close()
        self.assertEqual(sorted(p.name for p in server.DATA.rglob("*") if p.is_file()), before)

    def get(self, path):
        c = http.client.HTTPConnection("127.0.0.1", self.srv.server_address[1])
        c.request("GET", path)
        r = c.getresponse()
        out = r.status, r.read()
        c.close()
        return out

    def test_saved_listed_and_served(self):
        self.assertEqual(self.post("/pattern", {"name": "listed-1", "grid": {"base": 3}})[0], 200)
        (server.DATA / "patterns" / "Not A Name.json").write_text("{}")
        status, body = self.get("/list/patterns")
        self.assertEqual(status, 200)
        self.assertIn("listed-1", json.loads(body)["names"])
        self.assertNotIn("Not A Name", json.loads(body)["names"])
        status, body = self.get("/patterns/listed-1.json")
        self.assertEqual((status, json.loads(body)["grid"]), (200, {"base": 3}))
        self.assertEqual(json.loads(self.get("/list/kits")[1])["names"], sorted(json.loads(self.get("/list/kits")[1])["names"]))


if __name__ == "__main__":
    unittest.main()
