#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""The workbench's local server: python3 workbench/server.py [port] (default 8732), on 127.0.0.1 only.
Serves the build (app/build/workbench/: the page and wb.wasm) and, behind it, workbench/ (kits, patterns, samples,
options.json). It writes only under workbench/: POST /feedback appends a line to feedback.jsonl, POST /kit and
POST /pattern ({"name": ..., ...}) write kits/<name>.json and patterns/<name>.json."""
import json
import re
import sys
import threading
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
DATA = REPO / "workbench"          # where it writes (the tests point it elsewhere)
ROOTS = (REPO / "app" / "build" / "workbench", DATA)
MAX_BODY = 1 << 20                 # a request body over 1 MB is refused
NAME = re.compile(r"[a-z0-9-]{1,40}")
WRITE = threading.Lock()


class Handler(SimpleHTTPRequestHandler):
    extensions_map = {**SimpleHTTPRequestHandler.extensions_map, ".wasm": "application/wasm",
                      ".js": "text/javascript", ".mjs": "text/javascript", ".json": "application/json"}

    def translate_path(self, path):
        """the first root that has the file (the build's page over workbench/'s sources)"""
        rel = Path(super().translate_path(path)).relative_to(ROOTS[0])   # (it drops any "..")
        for root in ROOTS:
            p = (root / rel).resolve()
            if p.is_relative_to(root) and p.exists():
                return str(p)
        return str(ROOTS[0] / rel)

    def reply(self, code, body):
        data = json.dumps(body).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_POST(self):
        route = self.path.split("?")[0]
        if route not in ("/feedback", "/kit", "/pattern"):
            return self.reply(404, {"ok": False, "error": "no such route"})
        try:
            size = int(self.headers.get("Content-Length", ""))
        except ValueError:
            return self.reply(411, {"ok": False, "error": "Content-Length required"})
        if size > MAX_BODY:
            return self.reply(413, {"ok": False, "error": "body over 1 MB"})
        try:
            body = json.loads(self.rfile.read(size))
        except ValueError:
            return self.reply(400, {"ok": False, "error": "not JSON"})
        if not isinstance(body, dict):
            return self.reply(400, {"ok": False, "error": "expected a JSON object"})
        if route == "/feedback":
            with WRITE, open(DATA / "feedback.jsonl", "a", encoding="utf-8") as f:
                f.write(json.dumps(body, separators=(",", ":")) + "\n")
            return self.reply(200, {"ok": True})
        name = body.get("name")
        if not isinstance(name, str) or not NAME.fullmatch(name):
            return self.reply(400, {"ok": False, "error": "name must be [a-z0-9-]{1,40}"})
        folder = DATA / (route[1:] + "s")
        with WRITE:
            folder.mkdir(exist_ok=True)
            (folder / f"{name}.json").write_text(json.dumps(body, indent=1) + "\n", encoding="utf-8")
        self.reply(200, {"ok": True})

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")   # a rebuild shows on reload
        super().end_headers()


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8732
    if not (ROOTS[0] / "wb.wasm").exists():
        sys.exit("server: no build; run sh workbench/build.sh first")
    srv = ThreadingHTTPServer(("127.0.0.1", port), partial(Handler, directory=str(ROOTS[0])))
    print(f"workbench  http://localhost:{port}/")
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
