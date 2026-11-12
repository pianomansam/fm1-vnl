// SPDX-License-Identifier: GPL-3.0-or-later
// The device's screens against the mock's, pixel for pixel: the host simulator draws a screen with the UI runtime
// (app/ui: the generated tables, the value bag, render.c) and prints the bag it drew from (uistate); the mock
// (ui/device.js's drawScreen) draws the same screen from that bag. Every pixel must match. Steps of a script:
//   { press: CONTROL } | { hold: [BUTTON, MS] } | { turn: [CONTROL, DETENTS] } | { key: [N, MS] } | { wait: MS } | { screen: ID } |
//   { row: [ROW, VALUE] } (a row's value forced in the snapshot shown: the longest names) | { spin: [ENC, N] } (N detents
//   at once) | { keydown: K } | { keyup: K } | { fade: N } (the text redrawn alone at {fade} N) | { reboot: true } |
//   { check: NAME, expect: (state) => "" or what is wrong }
// Every screen: the main and part screens, Sections, the Parts roll, the Mixer, Settings.
//   ./build.sh --host && node workbench/tools/parity.mjs [OUTDIR]
import { execFileSync } from "node:child_process";
import { mkdtempSync, readFileSync, writeFileSync, existsSync, mkdirSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { deflateSync } from "node:zlib";
import { Lcd, W, H, colors, drawScreen } from "../ui/device.js";
import data from "../ui/screens.js";
import { ROWS } from "../rows.js";

const root = new URL("../../", import.meta.url).pathname;
const HOST = join(root, "app/build/host/vnl-1_host");
if (!existsSync(HOST)) { console.log(`parity: no ${HOST} (./build.sh --host first)`); process.exit(1); }
const outDir = process.argv[2];
if (outDir) mkdirSync(outDir, { recursive: true });
const opts = JSON.parse(readFileSync(join(root, "workbench/options.json"), "utf8"));

const PHASE1 = Object.keys(data.screens);   // (every screen now)

const CONTROL = { press: "tap", turn: "turn", wait: "wait" };
// a script on the simulator: its checks' frames and bags, in order
function run(seed, steps) {
  const dir = mkdtempSync(join(tmpdir(), "vnl-parity-"));
  const lines = [], checks = [];
  for (const s of steps) {
    if (s.press) lines.push(`tap ${s.press}`);
    else if (s.turn) lines.push(`turn ${s.turn[0]} ${s.turn[1]}`);
    else if (s.spin) lines.push(`spin ${s.spin[0]} ${s.spin[1]}`);
    else if (s.keydown !== undefined) lines.push(`key ${s.keydown} down`);
    else if (s.keyup !== undefined) lines.push(`key ${s.keyup} up`);
    else if (s.hold) lines.push(`press ${s.hold[0]}`, `wait ${s.hold[1]}`, `release ${s.hold[0]}`);
    else if (s.wait) lines.push(`wait ${s.wait}`);
    else if (s.screen) lines.push(`uiscreen ${s.screen}`, "wait 20");
    else if (s.row) lines.push(`uirow ${s.row[0]} ${s.row[1]}`, "wait 20");
    else if (s.fade !== undefined) lines.push(`uifade ${s.fade}`);
    else if (s.reboot) lines.push("reboot");
    else if (s.check) { lines.push("wait 20", "uistate", `fbdump ${checks.length}.raw`); checks.push(s); }
  }
  writeFileSync(join(dir, "s.vnl"), lines.join("\n") + "\n");
  let out = "";
  try { out = execFileSync(HOST, [join(dir, "s.vnl"), dir], { env: { ...process.env, VNL_SEED: String(seed) }, encoding: "utf8", maxBuffer: 1 << 26 }); }
  catch (e) { out = e.stdout ?? ""; console.log(e.stderr ?? ""); }
  const states = [...out.matchAll(/^uistate (.*)$/gm)].map((m) => JSON.parse(m[1]));
  return checks.map((c, i) => {
    const b = readFileSync(join(dir, `${i}.raw`)), px = new Uint16Array(W * H);
    for (let k = 0; k < W * H; k++) px[k] = (b[2 * k] << 8) | b[2 * k + 1];
    return { name: c.check, expect: c.expect, fb: px, state: states[i] };
  });
}

// the mock's screen from the device's bag
function mock(state) {
  const lcd = new Lcd();
  lcd.fill(colors(state.values.theme).BLACK);   // (the theme's background: Settings' THEME)
  const screen = data.screens[state.screen];
  drawScreen(lcd, screen, state.values, data);
  return lcd.fb;
}

const diff = (a, b) => a.map((v, i) => (v === b[i] ? (v >> 2) & 0x39e7 : 0xf800));
function png(file, screens) {
  const gap = 8, w = screens.length * (W + gap) - gap, raw = Buffer.alloc((w * 3 + 1) * H);
  for (let y = 0; y < H; y++)
    screens.forEach((s, k) => {
      for (let x = 0; x < W; x++) {
        const v = s[y * W + x], o = y * (w * 3 + 1) + 1 + (k * (W + gap) + x) * 3, r = v >> 11, g = (v >> 5) & 63, b = v & 31;
        raw[o] = (r << 3) | (r >> 2); raw[o + 1] = (g << 2) | (g >> 4); raw[o + 2] = (b << 3) | (b >> 2);
      }
    });
  const crc = (buf) => { let c = ~0; for (const x of buf) { c ^= x; for (let k = 0; k < 8; k++) c = (c >>> 1) ^ (0xedb88320 & -(c & 1)); } return ~c >>> 0; };
  const chunk = (type, body) => {
    const b = Buffer.concat([Buffer.from(type), body]), len = Buffer.alloc(4), c = Buffer.alloc(4);
    len.writeUInt32BE(body.length); c.writeUInt32BE(crc(b));
    return Buffer.concat([len, b, c]);
  };
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(H, 4); ihdr[8] = 8; ihdr[9] = 2;
  writeFileSync(file, Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk("IHDR", ihdr),
                                     chunk("IDAT", deflateSync(raw)), chunk("IEND", Buffer.alloc(0))]));
}

let ok = true;
function compare(c, extra = "") {
  if (!c.state) { console.log(`  FAIL ${c.name}: no uistate`); ok = false; return; }
  const mk = mock(c.state);
  let same = 0;
  for (let i = 0; i < W * H; i++) same += c.fb[i] === mk[i];
  // nothing drawn outside the screen's bands (the device clips at a band, as the mock does)
  const screen = data.screens[c.state.screen], inBand = new Uint8Array(H);
  for (const b of screen.bands) for (let y = b.y; y < b.y + b.h; y++) inBand[y] = 1;
  let outside = 0;
  const bg = colors(c.state.values.theme).BLACK;
  for (let y = 0; y < H; y++) if (!inBand[y]) for (let x = 0; x < W; x++) outside += c.fb[y * W + x] !== bg;
  const wrong = c.expect ? c.expect(c.state) : "";
  const good = same === W * H && !outside && !wrong;
  console.log(`  ${good ? "ok  " : "FAIL"} ${c.name}: ${((100 * same) / (W * H)).toFixed(3)}% of pixels match` +
              `${same === W * H ? "" : ` (${W * H - same} differ)`}${outside ? `, ${outside} drawn outside the bands` : ""}${wrong ? `; ${wrong}` : ""}${extra}`);
  if (outDir) png(join(outDir, `${c.name}.png`), [c.fb, mk, diff(c.fb, mk)]);
  ok = ok && good;
}

console.log(`parity: the device's screens (the simulator, app/ui) against the mock's: ${PHASE1.join(", ")}`);
const SEED = 72;
{   // every phase-1 screen at bar 13 or so, playing; then stopped (held); then between songs; and at boot (ready)
  const steps = [];
  for (const id of PHASE1) steps.push({ screen: id }, { check: `boot-${id}` });
  steps.push({ press: "PLAY" }, { wait: 30000 });
  for (const id of PHASE1) steps.push({ screen: id }, { check: `playing-${id}` });
  steps.push({ press: "PLAY" }, { wait: 1500 });
  for (const id of PHASE1) steps.push({ screen: id }, { check: `stopped-${id}` });
  steps.push({ press: "PLAY" }, { press: "SEQ" }, { wait: 1500 });
  for (const id of ["main", "pComp"]) steps.push({ screen: id }, { check: `between-${id}` });
  for (const c of run(SEED, steps)) compare(c);
}
{   // Review Focus 1: each options row at its longest choice name, on its part screen: clipped at the band, as the mock
  const steps = [{ press: "PLAY" }, { wait: 4000 }];
  const fields = Object.entries(data.fields).filter(([, d]) => d.options && !["loop", "endless"].includes(d.row));
  const screenOf = (k) => PHASE1.find((id) => (data.screens[id].knobs ?? []).some((kn) => kn.field === k));
  for (const [k, d] of fields) {
    const id = screenOf(k);
    if (!id) continue;
    const row = d.row === "rhythm" ? "comp" : d.row;
    const src = ROWS.find((r) => r.id === row)?.source;
    const role = ROWS.find((r) => r.id === row)?.role, ROLE = { CHORD: "keys", LEAD: "lead", BASS: "bass", PAD: "pad" };
    const names = { instruments: opts.instruments.filter((i) => i.roles.includes(ROLE[role])), kits: opts.kits.filter((x) => x.id < 16),
                    patterns: opts.patterns, leadStyles: opts.leadStyles,
                    comping: [...opts.compRhythms.map((c) => ({ ...c, id: c.value })), ...opts.comping] }[src];
    if (!names) continue;
    const best = names.reduce((a, x) => ((x.short ?? x.name).length > (a.short ?? a.name).length ? x : a));
    steps.push({ screen: id }, { row: [row, best.id] }, { check: `longest-${k}` });
  }
  for (const c of run(SEED, steps)) compare(c);
}
{   // the navigator (nav.c) on scripted presses, as the mock's Device moves (the plan's nav_* scripts): every step drawn
  // and compared, and what it should have done checked
  const v = (st) => st.values, is = (what, got, want) => (got === want ? "" : `${what} ${JSON.stringify(got)}, not ${JSON.stringify(want)}`);
  const steps = [
    { press: "PLAY" }, { wait: 5000 }, { screen: "main" },
    // nav_main_to_comp: ALGO 3, EDIT, K1 +1, OCT+, SELECT, ALGO 1, HOME
    { spin: ["ALGO", 3] }, { check: "nav-algo3", expect: (st) => is("sel3", v(st).sel3, true) },
    { press: "EDIT" }, { check: "nav-edit", expect: (st) => is("screen", st.screen, "pComp") || is("stack", st.stack.join(), "main,pComp") ||
                                                       (st.said.startsWith("COMP, PAGE 1/2") ? "" : `said ${st.said}`) },
    { spin: ["K1", 1] }, { check: "nav-k1", expect: (st) => is("compInsPend", v(st).compInsPend, true) || (v(st).compIns.startsWith("*") ? "" : `compIns ${v(st).compIns}`) },
    { press: "OCT+" }, { check: "nav-oct", expect: (st) => is("compRegPend", v(st).compRegPend, true) || is("msg", st.msg, "REGISTER") },
    { spin: ["SELECT", 1] }, { check: "nav-select", expect: (st) => is("screen", st.screen, "pCompMore") },
    { spin: ["ALGO", 1] }, { check: "nav-algo1", expect: (st) => is("screen", st.screen, "pLead") },
    { press: "HOME" }, { check: "nav-home", expect: (st) => is("stack", st.stack.join(), "main") },
    // nav_pending: K2 +2 waits, marked, then lands (nothing waits)
    // (the comp's change, made first, is still the line's: the earliest boundary, a tie the first made)
    { spin: ["K2", 2] }, { check: "nav-pending", expect: (st) => (/^INSTRUMENT .* IN \d BARS?$/.test(v(st).pending) ? "" : `pending ${v(st).pending}`) ||
                                                                 is("tempoPend", !!v(st).tempoPend, true) || (v(st).tempo.startsWith("*") ? "" : `tempo ${v(st).tempo}`) },
    { wait: 14000 }, { check: "nav-landed", expect: (st) => is("pending", v(st).pending, "") },
    // nav_preset: PRESET on Drums steps its kit; nav_seq_part: SEQ on Bass re-rolls it
    { spin: ["ALGO", -2] }, { press: "EDIT" },
    // (the device plays one approved kit, Soft brushes: PRESET on Drums has nowhere to go; on Bass it steps)
    { spin: ["PRESET", -1] }, { check: "nav-preset-drums", expect: (st) => is("screen", st.screen, "pDrums") },
    { spin: ["ALGO", 1] }, { press: "SEQ" }, { check: "nav-seq-part", expect: (st) => is("screen", st.screen, "pBass") ||
                                                                         (/^BASS: NEW ROLL IN \d BARS?$/.test(v(st).pending) ? "" : `pending ${v(st).pending}`) },
    { spin: ["PRESET", 1] }, { check: "nav-preset", expect: (st) => is("bassInsPend", !!v(st).bassInsPend, true) },
    // nav_fast (Review Focus 4): SELECT +12 at once on the main screen; F and C down together; C held while K1 turns
    { press: "HOME" }, { spin: ["SELECT", 12] }, { check: "nav-spin", expect: (st) => is("sel6", v(st).sel6, true) },   // (1 + 12) % 7
    { keydown: 0 }, { keydown: 6 }, { wait: 100 }, { keyup: 0 }, { spin: ["K1", 1] }, { wait: 600 },
    { check: "nav-keys", expect: (st) => (st.said === "LEAD SOLO" ? "" : `said ${st.said}`) },
    { keyup: 6 }, { keydown: 6 }, { wait: 50 }, { keyup: 6 }, { check: "nav-unsolo", expect: (st) => is("said", st.said, "LEAD SOLO OFF") },
    // nav_stopped (Review Focus 3): stopped, K2 asks nothing
    { press: "PLAY" }, { wait: 1500 }, { spin: ["K2", 1] }, { check: "nav-stopped", expect: (st) => is("pending", v(st).pending, "") || is("song", v(st).song, false) },
    // nav_new_song: on Comp the song ends, the next starts: the main screen
    { press: "PLAY" }, { spin: ["ALGO", 4] }, { press: "EDIT" }, { check: "nav-comp", expect: (st) => is("screen", st.screen, "pComp") },
    { wait: 230000 }, { check: "nav-new-song", expect: (st) => is("stack", st.stack.join(), "main") },
  ];
  for (const c of run(SEED, steps)) compare(c);
}
{   // the screens after the main and part screens (Sections, the Parts roll, the Mixer, Settings) on scripted presses
  const v = (st) => st.values, is = (what, got, want) => (got === want ? "" : `${what} ${JSON.stringify(got)}, not ${JSON.stringify(want)}`);
  const notes = (st) => Object.values(v(st).activity?.notes ?? {}).reduce((n, x) => n + x.length, 0);
  const steps = [
    { press: "PLAY" }, { wait: 9000 }, { screen: "main" },
    // the Parts roll (ARP): a lane a part, the bar playing highlighted, the notes around it; K1 mutes a lane, K2 solos
    // (its look-ahead a few 16ths a block: the window's notes a moment after it opens)
    { press: "ARP" }, { wait: 400 }, { check: "parts", expect: (st) => is("screen", st.screen, "mixer") || (notes(st) > 20 ? "" : `${notes(st)} notes`) },
    { spin: ["ALGO", 1] }, { spin: ["K1", 1] }, { check: "parts-mute", expect: (st) => is("mute", v(st).activity.mute.join(), "bass") },
    { spin: ["K2", 1] }, { check: "parts-solo", expect: (st) => is("solo", v(st).activity.solo, "bass") },
    { spin: ["K2", 1] }, { spin: ["K1", 1] }, { wait: 3400 }, { check: "parts-next-bar" },
    // the Mixer (FX): the knobs layout, K1-K4 a part's level each (K3 the comp's), -12..+6 dB
    { press: "FX" }, { spin: ["K3", -4] },
    { check: "mixer", expect: (st) => is("screen", st.screen, "levelsKnobs") || is("keys level", v(st).levels.keys, -4) },
    { spin: ["K3", 20] }, { check: "mixer-top", expect: (st) => is("keys level", v(st).levels.keys, 6) },
    { spin: ["K3", -6] },
    // the faders layout (the mock's variant): K1 the highlighted part's level
    { screen: "levels" }, { spin: ["ALGO", 2] }, { spin: ["K1", -4] },
    { check: "mixer-faders", expect: (st) => is("keys level", v(st).levels.keys, -4) },
    { spin: ["K1", 4] },
    // the knobs layout's pages: SELECT the other page
    { screen: "levelsKnobs" }, { spin: ["K2", -3] }, { keydown: 4 }, { keyup: 4 },
    { check: "mixer-knobs", expect: (st) => is("bass level", v(st).levels.bass, -3) || is("mute", v(st).activity.mute.join(), "keys") },
    { spin: ["SELECT", 1] }, { spin: ["K1", 2] },
    { check: "mixer-knobs-2", expect: (st) => is("screen", st.screen, "levelsKnobs2") || is("pad level", v(st).levels.pad, 2) },
    { keydown: 4 }, { keyup: 4 }, { screen: "levels" },
    // the instrument rules on a part change (rows.js's conflicts, on the device's requests): seed 72's comp plays the
    // nylon and its lead the whistle (the vibes and the piano until the whistle's sets, 2026-11-11): the lead to the
    // piano first; then the comp to the piano too moves the lead to another instrument, landing together
    { screen: "pLead" }, { spin: ["K1", -4] },
    { check: "rules-lead", expect: (st) => is("leadIns", v(st).leadIns, "*PIANO") },
    { wait: 16000 },
    { screen: "pComp" }, { spin: ["K1", 4] },
    { check: "rules-asked", expect: (st) => is("compIns", v(st).compIns, "*PIANO") },
    { wait: 16000 }, { check: "rules-landed", expect: (st) => is("compIns", v(st).compIns, "PIANO") ||
                                                              (v(st).lead !== "PIANO" ? "" : "the lead still plays the piano") },
    { screen: "levels" },
    // Settings (GLO): the placeholder; GLO back
    { press: "GLO" }, { check: "settings", expect: (st) => is("screen", st.screen, "prefs") },
    { press: "GLO" }, { check: "settings-back", expect: (st) => is("screen", st.screen, "levels") },
    // Sections (SEL): the list, the playing one marked; SELECT, EDIT: a jump waiting for the 4-bar line, then landed
    { press: "HOME" }, { press: "SEL" }, { check: "sections", expect: (st) => is("screen", st.screen, "sections") },
    { spin: ["SELECT", 2] }, { press: "EDIT" },
    { check: "sections-jump", expect: (st) => (/^GO TO .* IN \d BARS?$/.test(v(st).pending) ? "" : `pending ${v(st).pending}`) },
    { wait: 14000 }, { check: "sections-landed", expect: (st) => is("pending", v(st).pending, "") || is("sec2Now", v(st).sec2Now, true) },
    // a white key jumps too (the second: G, the second section)
    { keydown: 2 }, { keyup: 2 }, { check: "sections-key", expect: (st) => (/^GO TO /.test(v(st).pending) ? "" : `pending ${v(st).pending}`) },
    // K1 Loop, K2 Stay on song: switches, at once
    { spin: ["K1", 1] }, { check: "sections-loop", expect: (st) => is("looping", v(st).looping, true) },
    { spin: ["K2", 1] }, { check: "sections-stay", expect: (st) => is("endless", v(st).endless, "ON") },
    { wait: 20000 }, { check: "sections-looping", expect: (st) => is("looping", v(st).looping, true) },
    { spin: ["K1", 1] }, { spin: ["K2", 1] }, { check: "sections-off", expect: (st) => is("looping", v(st).looping, false) },
  ];
  for (const c of run(SEED, steps)) compare(c);
}
{   // the main screen's PRESET wraps: the bass's K1 turned down to its list's first (it stops there), then PRESET
  // back one on the main screen: its last
  const v = (st) => st.values, is = (what, got, want) => (got === want ? "" : `${what} ${JSON.stringify(got)}, not ${JSON.stringify(want)}`);
  const steps = [
    { press: "PLAY" }, { wait: 5000 }, { screen: "main" }, { spin: ["ALGO", 2] }, { press: "EDIT" }, { spin: ["K1", -12] },
    { check: "preset-first", expect: (st) => is("bassInsFirst", !!v(st).bassInsFirst, true) },
    { press: "HOME" }, { spin: ["PRESET", -1] },
    { check: "preset-wrap", expect: (st) => is("bassInsLast", !!v(st).bassInsLast, true) || is("bassInsFirst", !!v(st).bassInsFirst, false) },
  ];
  for (const c of run(SEED, steps)) compare(c);
}
{   // a part's lock: SEQ held on its screen (the padlock by its name), the padlock on its main-screen tile, a short SEQ
  // there refused; every part locked: each tile marked
  const v = (st) => st.values, is = (what, got, want) => (got === want ? "" : `${what} ${JSON.stringify(got)}, not ${JSON.stringify(want)}`);
  const steps = [
    { press: "PLAY" }, { wait: 5000 }, { screen: "main" }, { spin: ["ALGO", 1] }, { press: "EDIT" }, { hold: ["SEQ", 700] },
    { check: "lock-drums", expect: (st) => is("lockDrums", v(st).lockDrums, true) || is("said", st.said, "DRUMS LOCKED") ||
                                           is("pending", v(st).pending, "") },
    { press: "SEQ" }, { check: "lock-seq", expect: (st) => is("said", st.said, "DRUMS IS LOCKED") || is("pending", v(st).pending, "") },
    { spin: ["SELECT", 1] }, { check: "lock-drums-more", expect: (st) => is("screen", st.screen, "pDrumsMore") },
    { press: "HOME" }, { check: "lock-main", expect: (st) => is("lockDrums", v(st).lockDrums, true) || is("lockBass", v(st).lockBass, false) },
    ...[1, 2, 3, 4, 5].flatMap((i) => [{ spin: ["ALGO", 1] }, { press: "EDIT" }, { hold: ["SEQ", 700] }, { check: `lock-part-${i}` },
                                       { press: "HOME" }]),
    { check: "lock-main-all", expect: (st) => ["lockDrums", "lockBass", "lockComp", "lockLead", "lockPad", "lockTexture"]
                                                .map((k) => is(k, v(st)[k], true)).join("") },
    // on the main screen SEQ held unlocks (and locks) the highlighted tile's part, with no new song
    { hold: ["SEQ", 700] }, { check: "lock-main-hold", expect: (st) => is("lockTexture", v(st).lockTexture, false) ||
                                                                     is("said", st.said, "TEXTURE UNLOCKED") || is("songs", v(st).song, true) },
    { spin: ["ALGO", -2] }, { hold: ["SEQ", 700] }, { check: "lock-main-hold-lead", expect: (st) => is("lockLead", v(st).lockLead, false) ||
                                                                                                  is("said", st.said, "LEAD UNLOCKED") },
    // the song line: SEQ held on its line of the main screen, and on the Song screen
    { spin: ["ALGO", -4] }, { hold: ["SEQ", 700] }, { check: "lock-main-song", expect: (st) => is("lockSong", v(st).lockSong, true) ||
                                                                                             is("said", st.said, "SONG LOCKED") },
    { press: "EDIT" }, { check: "lock-song", expect: (st) => is("screen", st.screen, "pSong") || is("lockSong", v(st).lockSong, true) },
    { spin: ["SELECT", 1] }, { check: "lock-song-more", expect: (st) => is("screen", st.screen, "pSongMore") },
    { hold: ["SEQ", 700] }, { check: "lock-song-off", expect: (st) => is("lockSong", v(st).lockSong, false) || is("said", st.said, "SONG UNLOCKED") },
  ];
  for (const c of run(SEED, steps)) compare(c);
}
{   // the saved songs: SAVE on the Song screen ("SAVED TO 1" in the header, the save waiting for silence while the song
  // plays), SAVE held there the measures; REC the Saved list (the waiting one in the pending color), SAVE on another
  // line refused while one waits; stopped, written, saved straight; the list scrolled to its last slots; one deleted
  const v = (st) => st.values, is = (what, got, want) => (got === want ? "" : `${what} ${JSON.stringify(got)}, not ${JSON.stringify(want)}`);
  const steps = [
    { press: "PLAY" }, { wait: 5000 }, { screen: "main" }, { press: "EDIT" }, { press: "SAVE" },
    { check: "saved-msg", expect: (st) => is("screen", st.screen, "pSong") || is("msg0", v(st).msg0, "SAVED TO 1") },
    { hold: ["SAVE", 700] }, { check: "saved-readout", expect: (st) => (/^REDRAW /.test(v(st).msg0) ? "" : `msg0 ${v(st).msg0}`) },
    { press: "REC" }, { check: "saved-list", expect: (st) => is("screen", st.screen, "saved") || is("slot0Pend", v(st).slot0Pend, true) ||
                                                                is("slot1Empty", v(st).slot1Empty, true) },
    { spin: ["ALGO", 1] }, { press: "SAVE" }, { check: "saved-busy", expect: (st) => is("msg0", v(st).msg0, "A SAVE WAITS") },
    { press: "PLAY" }, { wait: 3000 }, { press: "SAVE" },
    { check: "saved-kept", expect: (st) => is("slot0Pend", v(st).slot0Pend, false) || is("slot1Empty", v(st).slot1Empty, false) ||
                                           is("msg0", v(st).msg0, "SAVED TO 2") },
    { spin: ["SELECT", -2] }, { check: "saved-scrolled", expect: (st) => is("slotRange", v(st).slotRange, "10-16 OF 16") },
    { spin: ["ALGO", 2] }, { hold: ["SAVE", 700] },
    { check: "saved-deleted", expect: (st) => is("slot0Empty", v(st).slot0Empty, true) || is("msg0", v(st).msg0, "SLOT 2") },
  ];
  for (const c of run(SEED, steps)) compare(c);
}
{   // the splash into now playing (the home): its text fading out, the song's in, the record as it is; a step's text
  // redrawn alone (uifade) and the boot's own steps (the device's timing: 1150 ms turning, 150 out, 150 in)
  const v = (st) => st.values, is = (what, got, want) => (got === want ? "" : `${what} ${JSON.stringify(got)}, not ${JSON.stringify(want)}`);
  const steps = [
    { press: "PLAY" }, { wait: 5000 }, { screen: "now" },
    { fade: 8 }, { check: "fade-now-8", expect: (st) => is("fade", v(st).fade, 8) },
    { fade: 16 }, { check: "fade-now-16" }, { fade: 0 }, { check: "fade-now-0" },
    { screen: "splash" }, { fade: 4 }, { check: "fade-splash-4" }, { fade: 12 }, { check: "fade-splash-12" },
    { screen: "main" }, { wait: 1000 }, { press: "HOME" }, { wait: 100 },
    { press: "PLAY" }, { wait: 2000 }, { reboot: true },   // (stopped: the home is kept once all is silent, app.c set_keep)
    { wait: 1190 }, { check: "boot-fade-out", expect: (st) => is("screen", st.screen, "splash") || (v(st).fade > 0 && v(st).fade < 16 ? "" : `fade ${v(st).fade}`) },
    { wait: 120 }, { check: "boot-fade-in", expect: (st) => is("screen", st.screen, "now") || (v(st).fade > 0 && v(st).fade < 16 ? "" : `fade ${v(st).fade}`) },
    { wait: 300 }, { check: "boot-faded-in", expect: (st) => is("screen", st.screen, "now") || is("fade", v(st).fade, 0) },
  ];
  for (const c of run(SEED, steps)) compare(c);
}
{   // the themes (Settings' THEME: SELECT steps through gfx.c's palettes, at once): the splash, now playing, the main
  // screen, Settings and a part screen in each of the plum, oxblood, garnet and black cherry ones; a reboot keeps it
  // (stopped)
  const v = (st) => st.values, is = (what, got, want) => (got === want ? "" : `${what} ${JSON.stringify(got)}, not ${JSON.stringify(want)}`);
  const steps = [{ press: "PLAY" }, { wait: 5000 }];
  for (const [d, name] of [[-1, "PLUM"], [1, "PLUM+"], [1, "OXBLOOD"], [1, "OXBLOOD+"], [1, "GARNET"], [1, "GARNET+"],
                           [1, "BLACK CHERRY"]]) {
    steps.push({ screen: "prefs" }, { spin: ["SELECT", d] }, { check: `theme-${name}-prefs`, expect: (st) => is("theme", v(st).theme, name) });
    for (const id of ["splash", "now", "main", "pComp"]) steps.push({ screen: id }, { check: `theme-${name}-${id}` });
  }
  steps.push({ press: "PLAY" }, { wait: 2000 }, { reboot: true }, { wait: 1500 },
             { check: "theme-kept", expect: (st) => is("theme", v(st).theme, "BLACK CHERRY") });
  for (const c of run(SEED, steps)) compare(c);
}
{   // bass saturation (Settings' BASS SATURATION: EDIT or PRESET flips it): Settings on, off, on again by PRESET; a
  // reboot keeps it (stopped)
  const v = (st) => st.values, is = (what, got, want) => (got === want ? "" : `${what} ${JSON.stringify(got)}, not ${JSON.stringify(want)}`);
  const steps = [{ press: "PLAY" }, { wait: 3000 }, { screen: "prefs" },
                 { check: "bassSat-on", expect: (st) => is("bassSat", v(st).bassSat, "ON") },
                 { press: "EDIT" }, { check: "bassSat-off", expect: (st) => is("bassSat", v(st).bassSat, "OFF") },
                 { spin: ["PRESET", 1] }, { check: "bassSat-preset", expect: (st) => is("bassSat", v(st).bassSat, "ON") },
                 { press: "EDIT" }, { press: "PLAY" }, { wait: 2000 }, { reboot: true }, { wait: 1500 }, { screen: "prefs" },
                 { check: "bassSat-kept", expect: (st) => is("bassSat", v(st).bassSat, "OFF") }];
  for (const c of run(SEED, steps)) compare(c);
}
console.log(ok ? "parity: all match" : "parity: FAILED");
process.exit(ok ? 0 : 1);
