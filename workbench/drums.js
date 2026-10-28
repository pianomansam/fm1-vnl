// SPDX-License-Identifier: GPL-3.0-or-later
// The drum screen: each lane's sound (from any kit), its 🎲, an audition and a level; the two-bar grid (a click cycles
// empty → hit → ghost); the kit, pattern and kick-phrase rows; saving kits and patterns (POST /kit, /pattern) and
// loading them back. The catalog, the grid's cells and the files' shapes are pure (smoke.mjs tests them); DrumScreen
// draws the page's. Every edit goes through the desk (desk.js): staged, then landed at the next bar line.
import { row, optionsFor } from "./rows.js";

// the lanes (wb_drums.h: V_KICK's order); the pattern's are the first six (library.h's D_*)
export const LANES = ["Kick", "Snare", "Closed hat", "Open hat", "Shaker", "Rim", "Ride"];
export const GRID_LANES = 6, RIDE = 6;

// a grid lane's step from its masks [A hits, A ghosts, B hits, B ghosts]: 0 empty, 1 hit, 2 ghost
export const cellOf = (m, bar, step) => ((m[bar * 2] >> step) & 1 ? 1 : (m[bar * 2 + 1] >> step) & 1 ? 2 : 0);
export const cycle = (state) => (state + 1) % 3;

// Each kit's sounds: [name, voice or lane, the lane it plays as shipped (-1: an extra)]. poketrack's are the machines'
// voices (C.PT); Optimist's the LO-FI kit's lanes (drum_synth.c DS_LANES: lane 12's sound is named BOOMBAP COWBELL
// upstream, a removed kit's name: here it's the LO-FI cowbell); the sampled kits' their pieces (C.SMP; Gogodze has no
// ride, the brushes' rim is the brush snare played lightly); the engine's synth kits their six lanes.
const PT_808 = [["kick", "BD", 0], ["snare", "SD", 1], ["closed hat", "CH", 2], ["open hat", "OH", 3], ["maracas", "MA", 4],
  ["rimshot", "RS", 5], ["clap", "CP", -1], ["low tom", "LT", -1], ["mid tom", "MT", -1], ["high tom", "HT", -1],
  ["cymbal", "CY", -1], ["cowbell", "CB", -1], ["low conga", "LC", -1], ["mid conga", "MC", -1], ["high conga", "HC", -1],
  ["claves", "CL", -1]];
const PT_CR78 = [["kick", "BD", 0], ["snare", "SD", 1], ["closed hat", "CH", 2], ["open hat", "OH", 3], ["maracas", "MA", 4],
  ["rimshot", "RS", 5], ["cymbal", "CY", -1], ["cowbell", "CB", -1], ["tambourine", "TB", -1], ["low bongo", "MC", -1],
  ["high bongo", "HC", -1], ["claves", "CL", -1]];
const OPT_LOFI = [["kick", 0, 0], ["snare", 1, 1], ["clap", 2, -1], ["closed hat", 3, 2], ["open hat", 4, 3], ["low tom", 5, -1],
  ["high tom", 6, -1], ["crash", 7, -1], ["ride", 8, RIDE], ["shaker", 9, 4], ["conga", 10, -1], ["rim", 11, 5],
  ["cowbell", 12, -1], ["clave", 13, -1], ["kick 2", 14, -1], ["snare 2", 15, -1]];
const SMP = { 0: [["kick", "KICK", 0], ["snare", "SNARE", 1], ["sidestick", "RIM", 5], ["closed hat", "HAT", 2], ["open hat", "OHAT", 3]],
  1: [["kick", "KICK", 0], ["snare", "SNARE", 1], ["sidestick", "RIM", 5], ["closed hat", "HAT", 2], ["open hat", "OHAT", 3], ["ride", "RIDE", RIDE]],
  2: [["kick", "KICK", 0], ["snare", "SNARE", 1], ["snare, light (rim)", "RIM", 5], ["closed hat", "HAT", 2], ["open hat", "OHAT", 3], ["ride", "RIDE", RIDE]] };
const BUILTIN = [["kick", 0, 0], ["snare", 1, 1], ["closed hat", 2, 2], ["open hat", 3, 3], ["shaker", 4, 4], ["rim", 5, 5]];
const SYNTH_KITS = ["KIT_DUSTY", "KIT_SOFT", "KIT_808", "KIT_BRUSHES"];   // the engine's (BUILTIN: a is the kit id)
const SET_NAME = ["Gogodze", "Big Rusty", "Big Rusty brushes"];   // the sample sets' (WB_SMP_*)

export const soundKey = (kind, a, b) => `${kind}:${a}:${b}`;

// Every lane source: { key, kind, a, b, name, kit (the kit row's id), kitName, lane, status, family, unavailable }.
// The mixed sampled kits have none (their hits aren't sources). unavailable: kit ids whose samples are missing.
export function sounds(C, opts, unavailable = new Set()) {
  const { SRC, DRUMS, PT, SMP: P } = C, out = [];
  const add = (k, kind, a, b, name, lane) => out.push({ key: soundKey(kind, a, b), kind, a, b, name, kit: k.id, kitName: k.name,
    lane, status: k.status, family: k.family, unavailable: unavailable.has(k.id) });
  for (const k of opts.kits) {
    const preset = k.id - DRUMS.PRESET;
    if (preset === DRUMS.PT808) for (const [n, v, l] of PT_808) add(k, SRC.PT, PT.M808, PT[v], `808 ${n}`, l);
    else if (preset === DRUMS.PTCR78) for (const [n, v, l] of PT_CR78) add(k, SRC.PT, PT.MCR78, PT[v], `CR-78 ${n}`, l);
    else if (preset === DRUMS.OPTLOFI) for (const [n, v, l] of OPT_LOFI) add(k, SRC.OPT, 0, v, `LO-FI ${n}`, l);
    else if (k.set !== undefined) for (const [n, p, l] of SMP[k.set]) add(k, SRC.SAMPLE, k.set, P[p], `${SET_NAME[k.set]} ${n}`, l);
    else if (SYNTH_KITS.includes(k.sym)) for (const [n, b, l] of BUILTIN) add(k, SRC.BUILTIN, k.id, b, `${k.name.replace(/ \(synth\)$/, "")} ${n}`, l);
  }
  return out;
}

// A lane's dropdown: approved sounds (retired ones with showRetired, marked), the current one always
export function soundOptions(all, cur, showRetired = false) {
  const out = all.filter((s) => showRetired || s.status !== "retired");
  if (cur && !out.some((s) => s.key === cur)) {
    const s = all.find((x) => x.key === cur);
    out.push(s ? { ...s, current: true } : { key: cur, name: "(not a source)", current: true, status: "retired" });
  }
  return out;
}

// A lane's 🎲: an approved, available sound that kit ships on this lane, of the song's family (electric goes with
// either), other than the current one; the family's own kits first. null when none.
export function pickSound(all, lane, cur, family, rnd = Math.random) {
  let xs = all.filter((s) => s.lane === lane && s.status === "approved" && !s.unavailable && s.key !== cur &&
                             (s.family === family || s.family === "electric"));
  const own = xs.filter((s) => s.family === family);
  if (own.length) xs = own;
  return xs.length ? xs[Math.floor(rnd() * xs.length) % xs.length] : null;
}

// the lanes as they play: the lane sources, or the staged kit's own when none is set (null: a mixed sampled kit)
export function laneSources(drums, C) {
  if (drums.lanes.some((l) => l[0] !== C.SRC.NONE)) return drums.lanes;
  return drums.kit_lanes ? drums.kit_lanes.map((l) => [...l, 0]) : null;
}

// a name for the files: lowercase letters, digits and hyphens (the server's rule), or "" if nothing's left
export const slug = (s) => String(s).toLowerCase().replace(/[^a-z0-9-]+/g, "-").replace(/-+/g, "-").replace(/^-|-$/g, "").slice(0, 40);

// The files Claude reads (and the load buttons read back): a pattern's grid, with each lane drawn (x hit, o ghost, .
// empty; bar A | bar B) and the library pattern it started from; a kit's lanes [kind, a, b, dB] with their sounds' names
export function patternFile(name, grid, opts) {
  const draw = (m) => [0, 1].map((b) => [...Array(16)].map((_, s) => ".xo"[cellOf(m, b, s)]).join("")).join(" | ");
  return { name, from: opts.patterns.find((p) => p.id === grid.base)?.name ?? `pattern ${grid.base}`,
           grid: { base: grid.base, edited: 1, lanes: grid.lanes },
           drawn: Object.fromEntries(grid.lanes.map((m, i) => [LANES[i], draw(m)])) };
}
export function kitFile(name, lanes, all, kitName) {
  return { name, from: kitName, lanes,
           sounds: Object.fromEntries(lanes.map((l, i) => [LANES[i], all.find((s) => s.key === soundKey(l[0], l[1], l[2]))?.name ?? "none"])) };
}

// The server's side: kind "kit" or "pattern". f: fetch (the page's; a test's)
export async function save(kind, body, f = globalThis.fetch) {
  const r = await f(`/${kind}`, { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(body) });
  const j = await r.json().catch(() => ({}));
  return { ok: r.ok, error: j.error };
}
export async function listSaved(kind, f = globalThis.fetch) {
  const r = await f(`/list/${kind}s`);
  return r.ok ? (await r.json()).names : [];
}
export async function getSaved(kind, name, f = globalThis.fetch) {
  const r = await f(`/${kind}s/${encodeURIComponent(name)}.json`);
  if (!r.ok) throw new Error(`${kind} ${name}: ${r.status}`);
  return r.json();
}

const el = (tag, props = {}, ...kids) => {
  const e = Object.assign(document.createElement(tag), props);
  e.append(...kids);
  return e;
};
const STEP = ["empty", "hit", "ghost"];

// The screen, drawn into root. ctx: { desk, call (wb.js's), run (the page's queue: a change, then render), retired()
// (the "show retired" switch), audio() (the AudioContext resumed: an audition is a click), say(message), back() }
export class DrumScreen {
  constructor(root, ctx) {
    Object.assign(this, { root, ...ctx, lanes: [], heads: {} });
    this.all = sounds(this.desk.C, this.desk.opts, this.desk.samples?.unavailable);
    this.build();
  }

  build() {
    const { desk, run } = this;
    const head = el("div", { className: "dhead" });
    for (const id of ["kit", "pattern", "kick"]) {
      const sel = el("select", { id: `dr-${id}`, onchange: () => {
        const o = this.heads[id].options.find((x) => String(x.value) === sel.value);
        run(() => desk.set(id, o ? o.value : sel.value));
      } });
      const dice = el("button", { textContent: "🎲", title: `Re-roll the ${row(id).label.toLowerCase()}`, onclick: () => run(() => desk.dice(id)) });
      head.append(el("label", { htmlFor: sel.id, textContent: row(id).label }), sel, dice);
      this.heads[id] = { sel, dice, options: [] };
    }
    const saveKit = el("button", { textContent: "Save as new kit", onclick: () => this.save("kit") });
    const savePat = el("button", { textContent: "Save as new pattern", onclick: () => this.save("pattern") });
    this.loadKit = el("select", { id: "dr-loadkit", onfocus: () => this.refreshSaved(), onchange: () => this.load("kit") });
    this.loadPat = el("select", { id: "dr-loadpattern", onfocus: () => this.refreshSaved(), onchange: () => this.load("pattern") });
    const files = el("div", { className: "dhead" }, saveKit, savePat,
                     el("label", { htmlFor: "dr-loadkit", textContent: "Load kit" }), this.loadKit,
                     el("label", { htmlFor: "dr-loadpattern", textContent: "Load pattern" }), this.loadPat);
    const steps = (b) => el("th", { className: "bars", textContent: `Bar ${b + 1}` });
    const thead = el("tr", {}, el("th", { textContent: "Lane" }), el("th", { textContent: "Sound (from any kit)" }), el("th"),
                     el("th", { textContent: "Level (dB)" }), steps(0), steps(1));
    this.bars = [thead.children[4], thead.children[5]];
    const body = el("tbody");
    for (const [i, name] of LANES.entries()) body.append(this.laneRow(i, name));
    body.append(el("tr", { className: "dnote" }, el("td", { textContent: "Toms / crash" }),
                   el("td", { colSpan: 5, textContent: "Fills and section starts: the drummer layer, not built yet." })));
    const table = el("div", { className: "dscroll" }, el("table", { className: "dgrid" }, el("thead", {}, thead), body));
    const key = el("p", { className: "dkey", textContent: "■ hit  ▪ ghost (soft)  □ empty: a click cycles empty → hit → ghost, heard from the next bar. ▶ plays the lane's sound alone." });
    this.root.append(el("div", { className: "dtop" }, el("button", { textContent: "← The desk", onclick: () => this.back() }),
                        el("h2", { textContent: "Drums" })), head, files, table, key);
    this.refreshSaved();
  }

  laneRow(i, name) {
    const { desk, run } = this, C = desk.C;
    const sel = el("select", { id: `dr-sound-${i}`, onchange: () => {
      const s = this.all.find((x) => x.key === sel.value);
      if (s) run(() => desk.lane(i, s.kind, s.a, s.b));
    } });
    const dice = el("button", { textContent: "🎲", title: `Another ${name.toLowerCase()} sound`, onclick: () => run(async () => {
      const cur = laneSources(desk.drums, C)?.[i];
      const s = pickSound(this.all, i, cur && soundKey(cur[0], cur[1], cur[2]), desk.values.family);
      return s ? desk.lane(i, s.kind, s.a, s.b) : desk.done(false, `${name}: no other approved sound for a ${desk.values.family} song.`);
    }) });
    const play = el("button", { textContent: "▶", title: `${name} alone`, onclick: async () => { await this.audio(); this.call("wb_audition", i, 110); } });
    const lvl = el("input", { type: "number", min: -40, max: 12, step: 1, id: `dr-level-${i}`, className: "lvl",
                              onchange: () => run(() => desk.level(i, Number(lvl.value) || 0)) });
    lvl.setAttribute("aria-label", `${name} level in dB`);
    const tr = el("tr", {}, el("td", {}, el("label", { htmlFor: sel.id, textContent: name })), el("td", {}, sel),
                  el("td", {}, el("div", { className: "dbtns" }, dice, play)), el("td", {}, lvl));
    const cells = [];
    if (i === RIDE) {
      tr.append(el("td", { colSpan: 2, className: "dnote", textContent: "Plays the hat lane in the ride section (quarters at most)." }));
    } else {
      for (let b = 0; b < 2; b++) {
        const td = el("div", { className: "steps" });
        for (let s = 0; s < 16; s++) {
          const btn = el("button", { className: "step", onclick: () => {
            const cur = cellOf(desk.drums.grid.lanes[i], b, s);
            run(() => desk.step(i, b, s, cycle(cur)));
          } });
          if (s % 4 === 0) btn.classList.add("beat");
          cells.push(btn);
          td.append(btn);
        }
        tr.append(el("td", {}, td));
      }
    }
    this.lanes[i] = { sel, dice, play, lvl, cells, name };
    return tr;
  }

  render(pos) {
    const { desk } = this, C = desk.C, d = desk.drums, retired = this.retired();
    if (!d) return;
    for (const [id, h] of Object.entries(this.heads)) {
      h.options = optionsFor(row(id), desk.state, desk.opts, { showRetired: retired });
      const cur = desk.values[id], locked = desk.locks.has(id);
      h.sel.replaceChildren(...h.options.map((o) => {
        const tag = (o.status === "retired" ? " (retired)" : "") + (o.unavailable ? " (unavailable: samples missing)" : "");
        const opt = new Option(o.name + tag, String(o.value), false, String(o.value) === String(cur));
        opt.disabled = !!o.unavailable;
        return opt;
      }));
      h.sel.disabled = h.dice.disabled = locked;
      h.sel.title = locked ? `${row(id).label} is locked (on the desk)` : "";
    }
    const src = laneSources(d, C), patLocked = desk.locks.has("pattern");
    for (const [i, L] of this.lanes.entries()) {
      const cur = src?.[i], key = cur && cur[0] !== C.SRC.NONE ? soundKey(cur[0], cur[1], cur[2]) : "";
      const opts = soundOptions(this.all, key, retired);
      const groups = new Map();
      for (const o of opts) {
        if (!groups.has(o.kitName ?? "")) groups.set(o.kitName ?? "", el("optgroup", { label: o.kitName ?? "Other" }));
        const tag = (o.status === "retired" ? " (retired)" : "") + (o.unavailable ? " (unavailable)" : "");
        const opt = new Option(o.name + tag, o.key, false, o.key === key);
        opt.disabled = !!o.unavailable;
        groups.get(o.kitName ?? "").append(opt);
      }
      L.sel.replaceChildren(...(key ? [] : [new Option(src ? "none" : "the mixed kit's own", "", false, true)]), ...groups.values());
      L.sel.disabled = L.dice.disabled = L.lvl.disabled = !src;
      L.sel.title = src ? "" : "A mixed sampled kit (retired) can't be edited lane by lane: choose a shipped kit first.";
      if (document.activeElement !== L.lvl) L.lvl.value = String(src ? cur[3] ?? 0 : 0);
      for (const [n, btn] of L.cells.entries()) {
        const b = n >> 4, s = n & 15, st = cellOf(d.grid.lanes[i], b, s);
        btn.textContent = "□■▪"[st];
        btn.dataset.state = STEP[st];
        btn.disabled = patLocked;
        btn.setAttribute("aria-label", `${L.name}, bar ${b + 1}, step ${s + 1}: ${STEP[st]}`);
      }
    }
    const sounding = pos?.bar ? (pos.bar - 1) & 1 : -1;
    this.bars.forEach((th, b) => th.classList.toggle("now", b === sounding));
  }

  async save(kind) {
    const { desk } = this, C = desk.C;
    const name = slug(prompt(`A name for the new ${kind} (letters, digits, hyphens):`) ?? "");
    if (!name) return;
    let body;
    if (kind === "kit") {
      const src = laneSources(desk.drums, C);
      if (!src) return this.say("A mixed sampled kit (retired) has no lanes to save: choose a shipped kit first.");
      body = kitFile(name, src, this.all, desk.opts.kits.find((k) => k.id === desk.values.kit)?.name);
    } else {
      body = patternFile(name, desk.drums.grid, desk.opts);
    }
    const r = await save(kind, body).catch((e) => ({ ok: false, error: String(e) }));
    this.say(r.ok ? `Saved workbench/${kind}s/${name}.json.` : `Not saved: ${r.error ?? "the server isn't answering"}.`);
    this.refreshSaved();
  }

  async refreshSaved() {
    for (const [kind, sel] of [["kit", this.loadKit], ["pattern", this.loadPat]]) {
      const names = await listSaved(kind).catch(() => []);
      sel.replaceChildren(new Option(names.length ? "Choose…" : "(none saved)", ""), ...names.map((n) => new Option(n, n)));
    }
  }

  load(kind) {
    const sel = kind === "kit" ? this.loadKit : this.loadPat, name = sel.value;
    sel.value = "";
    if (!name) return;
    this.run(async () => {
      const file = await getSaved(kind, name);
      const res = await (kind === "kit" ? this.desk.loadKit(file) : this.desk.loadPattern(file));
      return res.ok && !res.message ? { message: `Loaded ${kind} ${name}: heard from the next bar.` } : res;
    });
  }
}
