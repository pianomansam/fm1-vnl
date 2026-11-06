// SPDX-License-Identifier: GPL-3.0-or-later
// ui/screens.js as plain data: workbench/ui/screens.json, which tools/screens/gen.py turns into the device's tables.
// Refuses data with problems (device.js's problems(): a draw item, action or value outside ui/vocab.json, among
// others), naming each. Keys sorted, so the file changes only when the screens do.
//   node workbench/tools/export_screens.mjs           writes screens.json
//   node workbench/tools/export_screens.mjs --check   exits 1 when screens.json is stale (tests/run_tests.sh)
import { readFileSync, writeFileSync } from "node:fs";
import { problems } from "../ui/device.js";

const OUT = new URL("../ui/screens.json", import.meta.url);
const data = (await import("../ui/screens.js")).default;
const bad = problems(data);
if (bad.length) {
  for (const p of bad) console.error(`screens.js: ${p}`);
  process.exit(1);
}
const sorted = (x) => Array.isArray(x) ? x.map(sorted)
  : x && typeof x === "object" ? Object.fromEntries(Object.keys(x).sort().map((k) => [k, sorted(x[k])])) : x;
const text = JSON.stringify(sorted(data), null, 1) + "\n";
if (process.argv.includes("--check")) {
  let was = "";
  try { was = readFileSync(OUT, "utf8"); } catch { /* missing: stale */ }
  if (was !== text) {
    console.error("workbench/ui/screens.json is stale: run node workbench/tools/export_screens.mjs");
    process.exit(1);
  }
  console.log("screens.json is up to date");
} else {
  writeFileSync(OUT, text);
  console.log(`wrote workbench/ui/screens.json (${text.length} B)`);
}
