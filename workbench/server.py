#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""The workbench's local server: python3 workbench/server.py [port] (default 8732), on 127.0.0.1 only.
Serves the build (app/build/workbench/: the page and wb.wasm) and, behind it, workbench/ (kits, patterns, samples,
options.json). It writes only under workbench/: POST /feedback appends a line to feedback.jsonl, POST /kit and
POST /pattern ({"name": ..., ...}) write kits/<name>.json and patterns/<name>.json (a name already saved: 409, unless
the body has "overwrite": true, which isn't written). POST /approve ({"family": ..., "id": ...}, a candidate
of patterns/model.json; anything else is refused) marks it approved in patterns/status.json, the one file it writes outside
workbench/; tools/patterns/import.sh then makes it one the roll draws. Only this page's requests are answered (the Host is
127.0.0.1 or localhost with the port; a POST's Origin too). GET /list/kits and /list/patterns
give the saved names ({"names": [...]}), each served at /kits/<name>.json and /patterns/<name>.json. POST /state (the
device mock's live state, about once a second) keeps only the latest in app/build/live/workbench.json (git-ignored), for
Claude to read when Sam asks a live question."""
import json
import os
import re
import sys
import threading
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
DATA = REPO / "workbench"          # where it writes (the tests point it elsewhere)
PATTERNS = REPO / "patterns"       # model.json (read) and status.json (written by /approve); the tests point it elsewhere
BUILD = REPO / "app" / "build" / "workbench"      # the page and wb.wasm (served first, then DATA)
LIVE = REPO / "app" / "build" / "live"            # POST /state's latest (the tests point it elsewhere)
MAX_BODY = 1 << 20                 # a request body over 1 MB is refused
NAME = re.compile(r"[a-z0-9-]{1,40}")
WRITE = threading.Lock()
SEEN = {}                          # feedback.jsonl's entry ids (read once), so a resent entry isn't written twice


class Handler(SimpleHTTPRequestHandler):
    extensions_map = {**SimpleHTTPRequestHandler.extensions_map, ".wasm": "application/wasm",
                      ".js": "text/javascript", ".mjs": "text/javascript", ".json": "application/json"}

    def translate_path(self, path):
        """the first root that has the file (the build's page over workbench/'s sources)"""
        rel = Path(super().translate_path(path)).relative_to(self.directory)   # (it drops any "..")
        for root in (Path(self.directory).resolve(), DATA.resolve()):
            p = (root / rel).resolve()
            if p.is_relative_to(root) and p.exists():
                return str(p)
        return str(Path(self.directory) / rel)

    def reply(self, code, body):
        data = json.dumps(body).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def ours(self):
        """the Host is this server's (a page elsewhere reaching us by DNS rebinding sends its own name)"""
        port = self.server.server_address[1]
        return self.headers.get("Host") in (f"127.0.0.1:{port}", f"localhost:{port}")

    def do_HEAD(self):
        if not self.ours():
            return self.reply(403, {"ok": False, "error": "not this page"})
        return super().do_HEAD()

    def do_GET(self):
        if not self.ours():
            return self.reply(403, {"ok": False, "error": "not this page"})
        route = self.path.split("?")[0]
        if route in ("/list/kits", "/list/patterns"):
            folder = DATA / route[len("/list/"):]
            names = sorted(p.stem for p in folder.glob("*.json") if NAME.fullmatch(p.stem)) if folder.is_dir() else []
            return self.reply(200, {"names": names})
        return super().do_GET()

    def do_POST(self):
        route = self.path.split("?")[0]
        if route not in ("/feedback", "/kit", "/pattern", "/approve", "/state"):
            return self.reply(404, {"ok": False, "error": "no such route"})
        # only this page: a web page elsewhere can POST here too (a simple request, or DNS rebinding), so the Host, the
        # Origin (if sent) and a JSON content type (which a cross-site form can't send) must all be ours
        port = self.server.server_address[1]
        hosts = (f"127.0.0.1:{port}", f"localhost:{port}")
        origin = self.headers.get("Origin")
        if not self.ours() or (origin is not None and origin not in [f"http://{h}" for h in hosts]):
            return self.reply(403, {"ok": False, "error": "not this page"})
        if self.headers.get("Content-Type", "").split(";")[0].strip().lower() != "application/json":
            return self.reply(415, {"ok": False, "error": "Content-Type must be application/json"})
        if self.headers.get("Content-Length") is None:
            return self.reply(411, {"ok": False, "error": "Content-Length required"})
        try:
            size = int(self.headers["Content-Length"])
        except ValueError:
            size = -1
        if size < 0:
            return self.reply(400, {"ok": False, "error": "bad Content-Length"})
        if size > MAX_BODY:
            return self.reply(413, {"ok": False, "error": "body over 1 MB"})
        try:
            body = json.loads(self.rfile.read(size))
        except ValueError:
            return self.reply(400, {"ok": False, "error": "not JSON"})
        if not isinstance(body, dict):
            return self.reply(400, {"ok": False, "error": "expected a JSON object"})
        if route == "/approve":
            return self.approve(body)
        if route == "/state":   # the latest only: written whole, then renamed into place
            with WRITE:
                LIVE.mkdir(parents=True, exist_ok=True)
                tmp = LIVE / ".workbench.tmp"
                tmp.write_text(json.dumps(body, indent=1) + "\n", encoding="utf-8")
                os.replace(tmp, LIVE / "workbench.json")
            return self.reply(200, {"ok": True})
        if route == "/feedback":
            path = DATA / "feedback.jsonl"
            with WRITE:
                if path not in SEEN:
                    SEEN[path] = set()
                    if path.exists():
                        for line in path.read_text(encoding="utf-8").splitlines():
                            try:
                                SEEN[path].add(json.loads(line).get("id"))
                            except ValueError:
                                pass
                eid = body.get("id")
                if eid is None or eid not in SEEN[path]:
                    with open(path, "a", encoding="utf-8") as f:
                        f.write(json.dumps(body, separators=(",", ":")) + "\n")
                    SEEN[path].add(eid)
            return self.reply(200, {"ok": True})
        name = body.get("name")
        if not isinstance(name, str) or not NAME.fullmatch(name):
            return self.reply(400, {"ok": False, "error": "name must be [a-z0-9-]{1,40}"})
        folder = DATA / (route[1:] + "s")
        overwrite = body.pop("overwrite", False) is True
        with WRITE:
            if (folder / f"{name}.json").exists() and not overwrite:
                return self.reply(409, {"ok": False, "error": f"{name} is already saved", "exists": True})
            folder.mkdir(exist_ok=True)
            tmp = folder / f".{name}.tmp"
            tmp.write_text(json.dumps(body, indent=1) + "\n", encoding="utf-8")
            os.replace(tmp, folder / f"{name}.json")
        self.reply(200, {"ok": True})

    def approve(self, body):
        """an entry of patterns/model.json (its family and id, never a path) approved in patterns/status.json"""
        fam, eid = body.get("family"), body.get("id")
        if isinstance(eid, int) and not isinstance(eid, bool):
            eid = str(eid)
        try:
            model = json.loads((PATTERNS / "model.json").read_text(encoding="utf-8"))
        except (OSError, ValueError):
            return self.reply(500, {"ok": False, "error": "patterns/model.json is missing or unreadable"})
        if not isinstance(fam, str) or fam.startswith("_") or not isinstance(model.get(fam), dict):
            return self.reply(400, {"ok": False, "error": "family must be one of patterns/model.json's"})
        if not isinstance(eid, str) or eid not in model[fam]:
            return self.reply(400, {"ok": False, "error": f"no such {fam} pattern"})
        was = model[fam][eid].get("status") if isinstance(model[fam][eid], dict) else None
        if was != "candidate":                       # only a candidate is Sam's to approve here
            return self.reply(409, {"ok": False, "error": f"{fam} {eid} is not a candidate ({was})"})
        path = PATTERNS / "status.json"
        with WRITE:
            try:
                status = json.loads(path.read_text(encoding="utf-8")) if path.exists() else {}
            except ValueError:
                status = None
            if not isinstance(status, dict) or not isinstance(status.get(fam, {}), dict):
                return self.reply(500, {"ok": False, "error": "patterns/status.json is not {family: {id: status}}: "
                                                              "fix or remove it"})
            status.setdefault(fam, {})[eid] = "approved"
            tmp = PATTERNS / ".status.json.tmp"
            tmp.write_text(json.dumps(status, indent=2, sort_keys=True) + "\n", encoding="utf-8")
            os.replace(tmp, path)
        self.reply(200, {"ok": True, "family": fam, "id": eid, "status": "approved",
                         "then": "run tools/patterns/import.sh so the firmware (and, rebuilt, the workbench) draws it"})

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")   # a rebuild shows on reload
        super().end_headers()


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8732
    if not (BUILD / "wb.wasm").exists():
        sys.exit("server: no build; run sh workbench/build.sh first")
    srv = ThreadingHTTPServer(("127.0.0.1", port), partial(Handler, directory=str(BUILD)))
    print(f"workbench  http://localhost:{port}/")
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
