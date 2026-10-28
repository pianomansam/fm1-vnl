#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""The workbench's local server: python3 workbench/server.py [port] (default 8732), on 127.0.0.1 only.
Serves the build (app/build/workbench/: the page and wb.wasm) and, behind it, workbench/ (kits, patterns, samples,
options.json)."""
import sys
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
ROOTS = (REPO / "app" / "build" / "workbench", REPO / "workbench")


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
