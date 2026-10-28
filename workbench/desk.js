// SPDX-License-Identifier: GPL-3.0-or-later
// The decision desk: the rows' state (values, locks, hand edits, what moved) and how a change reaches the engine.
// Every change is staged (wb_stage_mark first), checked against the locks (a locked row that moved undoes it whole and
// says why), then committed with wb_apply: nothing staged is heard before that, so a bar line inside a change never
// lands half of it. A seed or map change regenerates the song, so every locked or hand-set row is staged again first. call(fn, ...args) reaches wb.wasm's exports, a Promise (wb.js in the page; smoke.mjs in Node).
import { ROWS, row, conflicts, pickFor, familyOf, fromEngine, kickMasks } from "./rows.js";
import { cellOf } from "./drums.js";

const SEED_TRIES = 20;   // a new seed whose song can't keep the locks is tried again (re-roll all, the seed's dice)

export class Desk {
  // probe(bytes): the lead register the engine applies to a staged state (wb_stage_bytes), from a second wb.wasm
  // instance off the audio thread (wb.js's probe); without one the lead register isn't read back
  // position(): { seconds, bar } of the song playing, for the change log (none: 0, 0)
  // samples: wb.js's Samples (the sampled kits: which are unavailable, and their files loaded on first use), or none
  constructor(call, C, opts, lib, { rnd = Math.random, probe = null, position = () => ({ seconds: 0, bar: 0 }), samples = null } = {}) {
    Object.assign(this, { call, C, opts, lib, rnd, probe, position, samples });
    this.changes = [];         // every applied change: { seconds, bar, row, from, to }
    this.values = {};
    this.locks = new Set();
    this.hand = new Set();     // rows set from their dropdown (they survive a regenerate, as locks do)
    this.moved = new Set();    // rows the last change forced (highlighted)
    this.legal = {};
    this.message = "";
  }

  get state() { return { values: this.values, locks: this.locks, legal: this.legal, unavailable: this.samples?.unavailable }; }
  kept(id) { return this.locks.has(id) || this.hand.has(id); }

  // the engine's staged state into values; the family is the page's (derive: from the keys and lead, as a new song's)
  async read(derive = false) {
    const fam = this.values.family, s = await this.call("wb_state");
    this.drums = s.drums;      // the lanes, the kit's own lanes and the grid (the drum screen's)
    this.values = fromEngine(s, this.C, this.opts);
    this.values.family = fam && !derive ? fam : familyOf(this.values, this.opts);
    const ids = (xs, f) => Promise.all(xs.map((x) => this.call("wb_roll_legal", f, x)))
      .then((ok) => xs.filter((x, i) => ok[i]));
    this.legal = {
      prog: await ids(this.lib.progs.map((p) => p.id), this.C.RF.PROG),
      mode: await ids([0, 1, 2, 3, 4, 5], this.C.RF.MODE),
    };
    return this.values;
  }

  async stage(id, v) {
    const r = row(id), { RF, V, OV } = this.C, c = this.call;
    if (r.kind === "seed") return c("wb_seed", v >>> 0);
    if (r.kind === "roll") return c("wb_roll_set", RF[r.field], v);
    if (r.kind === "role") return c("wb_role_set", V[r.role], v);
    if (r.kind === "ov" || r.kind === "reroll") return c("wb_ov_set", OV[r.key], v);
    if (id === "kit") return (await this.samples?.kit(v)) === false ? undefined : c("wb_kit_set", v);
    if (id === "kick") return v === "grid" ? undefined : c("wb_kick_phrase", ...kickMasks(v, this.opts));   // (the grid's own)
    if (id === "pattern") {
      await c("wb_roll_set", RF.PATTERN, v);
      if (this.values.kick && this.values.kick !== "grid") await c("wb_kick_phrase", ...kickMasks(this.values.kick, this.opts));
    }
    if (id === "family") this.values.family = v;
  }

  // The registers as the engine applies them (0 where the instrument can't move): staged and shown instead
  async readBack() {
    const OV = this.C.OV, said = [];
    const k = await this.call("wb_keys_oct_applied");
    if (k !== this.values.keysOct) {
      await this.call("wb_ov_set", OV.KEYS_OCT, k);
      said.push(`Keys register: the engine kept it ${k ? (k > 0 ? "up" : "down") + " an octave" : "where it was"} (the instrument's range).`);
      this.values.keysOct = k;
    }
    if (this.probe && this.values.leadOct) {
      const l = await this.probe(await this.call("wb_stage_bytes"));
      if (l !== this.values.leadOct) {
        await this.call("wb_ov_set", OV.LEAD_OCT, l);
        said.push("Lead register: the engine kept it where it was (the lead can't move there).");
        this.values.leadOct = l;
      }
    }
    this.message = said.join(" ");
  }

  // Stage every kept (locked or hand-set) row again, after a regenerate wiped the custom set, pattern and roll; a new
  // seed also returns the other overrides to the engine's. Then the family's followers, unless kept.
  async reapply(except, kept, fresh) {
    for (const r of ROWS) {
      if (r.id === except || r.kind === "seed" || r.kind === "family") continue;
      if (kept.has(r.id)) await this.stage(r.id, kept.get(r.id));
      else if (fresh && (r.kind === "ov" || r.kind === "reroll")) await this.stage(r.id, r.source === "registers" || r.kind === "reroll" ? 0 : -1);
    }
    if (kept.has("family")) {
      const fam = kept.get("family");
      await this.read();
      this.values.family = fam;
      const c = conflicts(row("family"), fam, { ...this.state, locks: [] }, this.opts, this.rnd, this.lib);
      for (const f of c.forced) if (!kept.has(f.row)) await this.stage(f.row, f.to);
    }
  }

  // Set row id to value (dice: a roll, not a hand edit). { ok, message, moved }
  // release: only the locks are kept through a regenerate (re-roll all lets hand edits go once its seed lands)
  async set(id, value, { dice = false, release = false } = {}) {
    const r = row(id);
    if (this.locks.has(id)) return this.done(false, `${r.label} is locked.`);
    if (id === "kit" && this.samples && !(await this.samples.kit(value))) {
      const k = this.opts.kits.find((x) => x.id === value);
      return this.done(false, `${k?.name ?? "That kit"} is unavailable: its sample files are missing (see workbench/samples/README.md).`);
    }
    const c = r.regenerates || r.kind === "roll" ? { forced: [], stuck: [], blocked: null }
                                                  : conflicts(r, value, this.state, this.opts, this.rnd, this.lib);
    if (c.blocked) return this.done(false, `Blocked: ${c.blocked.label} is locked (${c.blocked.why}).`);
    const before = { ...this.values }, grid = this.drums?.grid;
    const keep = (x) => (release ? this.locks.has(x) : this.kept(x));
    const kept = new Map([...ROWS].filter((x) => keep(x.id) && x.id !== id).map((x) => [x.id, before[x.id]]));
    const forced = new Set(c.forced.map((f) => f.row));
    await this.call("wb_stage_mark");
    await this.stage(id, value);
    for (const f of c.forced) await this.stage(f.row, f.to);
    if (r.regenerates) await this.reapply(id, kept, r.kind === "seed");
    else {   // a roll field can redraw the set (a mood change) or the pattern: the kept roles and drums go back
      await this.read();
      for (const x of ROWS)
        if ((x.kind === "role" || x.kind === "drum") && kept.has(x.id) && !forced.has(x.id) && String(this.values[x.id]) !== String(kept.get(x.id)))
          await this.stage(x.id, kept.get(x.id));
    }
    if (grid?.edited && kept.has("pattern") && id !== "pattern" && id !== "kick") {   // the grid goes with a kept pattern
      await this.read();
      if (JSON.stringify(this.drums.grid) !== JSON.stringify(grid)) await this.stageGrid(grid);
    }
    await this.read(r.kind === "seed" && !kept.has("family"));
    const changed = ROWS.filter((x) => x.id !== id && String(this.values[x.id]) !== String(before[x.id]));
    const lockedMoved = changed.filter((x) => this.locks.has(x.id) && !forced.has(x.id));
    if (lockedMoved.length) {
      await this.call("wb_stage_undo");
      this.values = before;
      await this.read();
      const names = lockedMoved.map((x) => x.label).join(", ");
      return this.done(false, `Blocked: ${names} ${lockedMoved.length > 1 ? "are" : "is"} locked and ${r.label} would move ${lockedMoved.length > 1 ? "them" : "it"}.`);
    }
    if (release) for (const h of [...this.hand]) if (!this.locks.has(h)) this.hand.delete(h);
    if (dice) this.hand.delete(id); else this.hand.add(id);
    const lost = r.regenerates ? [...kept.keys()].filter((k) => this.hand.has(k) && String(this.values[k]) !== String(kept.get(k))) : [];
    lost.forEach((k) => this.hand.delete(k));
    this.moved = new Set(r.regenerates ? [] : changed.map((x) => x.id));
    for (const x of changed) if (!r.regenerates) this.hand.delete(x.id);
    this.message = "";
    await this.readBack();
    await this.call("wb_apply");
    const at = this.position();
    for (const x of [r, ...changed.filter((x) => x.id !== id)])
      this.changes.push({ seconds: at.seconds, bar: at.bar, row: x.id, from: before[x.id], to: this.values[x.id] });
    const notes = [
      ...c.stuck.map((s) => `${row(s.row).label} stays: no approved ${value} option yet.`),
      ...lost.map((k) => `${row(k).label} couldn't be kept in the new song.`),
    ];
    if (this.message) notes.push(this.message);
    return this.done(true, notes.join(" "));
  }

  done(ok, message) {
    this.message = message;
    if (!ok) this.moved = new Set();
    return { ok, message, moved: [...this.moved] };
  }

  // 🎲: the engine's own dice for a roll field, a new seed (tried until the locks fit), a new salt, or pickFor
  async dice(id, { release = false } = {}) {
    const r = row(id);
    if (this.locks.has(id)) return this.done(false, `${r.label} is locked.`);
    if (r.kind === "seed") {
      let res;
      for (let i = 0; i < SEED_TRIES; i++) {
        res = await this.set("seed", 1 + Math.floor(this.rnd() * 999999), { dice: true, release });
        if (res.ok) return res;
      }
      return res;
    }
    if (r.kind === "reroll") return this.set(id, 1 + Math.floor(this.rnd() * 0x7ffffffe), { dice: true });
    if (r.kind === "roll") {
      const { RF } = this.C, before = this.values[id];
      await this.call("wb_stage_mark");
      const v = await this.call("wb_roll_dice", RF[r.field]);
      await this.call("wb_stage_undo");          // set() stages it again, with the lock checks
      if (v < 0 || v === before) return this.done(false, `${r.label}: no other value fits.`);
      return this.set(id, v, { dice: true });
    }
    const v = pickFor(r, this.state, this.opts, this.rnd, this.lib);
    if (v === null) return this.done(false, `${r.label}: no other approved option${r.kind === "role" || id === "kit" ? ` for a ${this.values.family} song` : ""}.`);
    return this.set(id, v, { dice: true });
  }

  // Re-roll everything unlocked: a new seed (hand edits let go) when the seed isn't locked, else each row's dice
  async rerollAll() {
    if (!this.locks.has("seed")) return this.dice("seed", { release: true });
    for (const id of [...this.hand]) if (!this.locks.has(id)) this.hand.delete(id);
    const notes = [];
    for (const r of ROWS) {
      if (this.locks.has(r.id) || r.kind === "seed") continue;
      const res = await this.dice(r.id);
      if (!res.ok && res.message.startsWith("Blocked")) notes.push(res.message);
    }
    this.moved = new Set();
    return this.done(true, notes.join(" "));
  }

  // Everything a replay needs (version 1; later tasks add to it): the rows of the song being HEARD (the engine's live
  // state: a change still waiting for its bar line isn't in it), those of the waiting commit (pending, or null), the
  // locks and hand edits, the seed. "drums" is the drum state the rows don't hold (the kit, pattern and kick are rows
  // already): the heard song's drum lanes and grid, { preset, lanes: [[kind, a, b, dB], ...], kit_lanes, grid: { base,
  // edited, lanes } } (wb_state's; restore stages the grid if it was edited).
  async snapshot() {
    const pending = (await this.call("wb_pending")) ? { ...this.values } : null;
    let rows = { ...this.values }, heard = await this.call(pending ? "wb_live_state" : "wb_state");
    if (pending) {
      rows = fromEngine(heard, this.C, this.opts);
      rows.family = familyOf(rows, this.opts);
    }
    return { version: 1, rows, pending, locks: [...this.locks], hand: [...this.hand], seed: rows.seed, drums: heard.drums };
  }

  // Put a snapshot's state on the engine (staged, then committed) as the page's own stagings would; the rows that
  // came out different (the engine kept another value) are returned.
  async restore(snap) {
    if (snap.version !== 1) throw new Error(`snapshot version ${snap.version}: this page reads version 1`);
    await this.call("wb_stage_mark");
    await this.stage("seed", snap.rows.seed);
    for (const r of ROWS) if (r.kind !== "seed" && r.kind !== "family") await this.stage(r.id, snap.rows[r.id]);
    const missing = await this.stageDrums(snap.drums);
    await this.call("wb_apply");
    this.values = { ...snap.rows };
    this.locks = new Set(snap.locks);
    this.hand = new Set(snap.hand);
    this.moved = new Set();
    await this.read();
    return [...ROWS.filter((r) => String(this.values[r.id]) !== String(snap.rows[r.id])).map((r) => r.id), ...missing];
  }

  // A snapshot's drum lanes over the kit row's (a preset's, or the song's own kit's): the lanes with a source first (the
  // first one set on a song with none fills the others from its kit), then the silent ones; each lane's level. Returns
  // what couldn't be restored: ["drums (<kit>: samples missing)"] for a sample set that didn't load (its lanes keep the
  // kit row's sources)
  async stageDrums(d) {
    const lanes = d?.lanes || [], out = [];
    if (d?.grid?.edited) await this.stageGrid(d.grid);
    if (!lanes.some((l) => l[0] !== this.C.SRC.NONE)) return out;
    for (const a of new Set(lanes.filter((l) => l[0] === this.C.SRC.SAMPLE).map((l) => l[1])))
      if (!this.samples || !(await this.samples.load(a))) {
        const k = this.opts.kits.find((x) => x.set === a);
        out.push(`drums (${k?.name ?? `sample set ${a}`}: samples missing)`);
      }
    for (const silent of [false, true])
      for (const [i, [kind, a, b]] of lanes.entries())
        if ((kind === this.C.SRC.NONE) === silent) await this.call("wb_lane_src", i, kind, a, b);
    for (const [i, l] of lanes.entries()) await this.call("wb_lane_db", i, l[3]);
    return out;
  }

  // Lane i plays a source (WB_SRC_*), staged and committed (the drum screen's). The first on a song with none copies
  // its kit to the others; a retired mixed sampled kit refuses: its hits aren't lane sources. { ok, message }
  async lane(i, kind, a, b) {
    if (kind === this.C.SRC.SAMPLE && this.samples && !(await this.samples.load(a)))
      return this.done(false, "Those samples are missing (see workbench/samples/README.md).");
    if (!(await this.call("wb_lane_src", i, kind, a, b))) {
      const k = this.opts.kits.find((x) => x.id === this.values.kit);
      return this.done(false, `${k?.name ?? "This kit"} can't be edited lane by lane (its hits are mixed copies, not sources): choose a shipped kit first.`);
    }
    await this.call("wb_apply");
    await this.read();
    const at = this.position();
    this.changes.push({ seconds: at.seconds, bar: at.bar, row: "drums", lane: i, source: [kind, a, b] });
    return this.done(true, "");
  }

  // A grid (wb_state's drums.grid: its library base and each lane's masks) as the custom pattern: the base loaded,
  // then the steps that differ (sent together: they run between the same two quanta). Staged only.
  async stageGrid(g) {
    await this.call("wb_pat_load", g.base);
    const have = (await this.call("wb_state")).drums.grid.lanes, jobs = [];
    for (const [lane, m] of g.lanes.entries())
      for (let bar = 0; bar < 2; bar++)
        for (let step = 0; step < 16; step++) {
          const want = cellOf(m, bar, step);
          if (want !== cellOf(have[lane], bar, step)) jobs.push(this.call("wb_pat_step", lane, bar, step, want));
        }
    await Promise.all(jobs);
  }

  // A drum screen edit: staged, checked against the locks (a locked row it would move undoes it whole), committed
  // (it lands at the next bar line) and logged. { ok, message }
  async drumEdit(what, stage, detail = {}) {
    const before = { ...this.values };
    await this.call("wb_stage_mark");
    const res = await stage();
    await this.read();
    const moved = ROWS.filter((x) => this.locks.has(x.id) && String(this.values[x.id]) !== String(before[x.id]));
    if (moved.length) {
      await this.call("wb_stage_undo");
      await this.read();
      return this.done(false, `Blocked: ${moved.map((x) => x.label).join(", ")} ${moved.length > 1 ? "are" : "is"} locked and this edit would move ${moved.length > 1 ? "them" : "it"}.`);
    }
    if (what === "grid") this.hand.add("pattern");   // a drawn pattern is a hand edit: it survives a new seed
    await this.call("wb_apply");
    const at = this.position();
    this.changes.push({ seconds: at.seconds, bar: at.bar, row: what, ...detail });
    this.moved = new Set();
    return this.done(true, Array.isArray(res) && res.length ? `Not restored: ${res.join(", ")}.` : "");
  }

  // The grid: lane (the pattern's: kick, snare, hat, open hat, shaker, rim), bar 0 or 1, step 0-15 to state (0 empty,
  // 1 hit, 2 ghost)
  step(lane, bar, step, state) {
    if (this.locks.has("pattern")) return this.done(false, "Pattern is locked: unlock it to edit the grid.");
    return this.drumEdit("grid", () => this.call("wb_pat_step", lane, bar, step, state), { lane, bar, step, state });
  }

  // a saved pattern's grid (workbench/patterns/*.json) or kit's lanes (workbench/kits/*.json)
  loadPattern(p) {
    if (this.locks.has("pattern")) return this.done(false, "Pattern is locked: unlock it to load one.");
    return this.drumEdit("grid", () => this.stageGrid(p.grid), { loaded: p.name });
  }
  loadKit(k) {
    return this.drumEdit("drums", () => this.stageDrums({ lanes: k.lanes }), { loaded: k.name });
  }

  // lane's level in dB (a song with no lane sources gets its kit's on every lane first)
  level(i, db) {
    return this.drumEdit("drums", () => this.call("wb_lane_db", i, db), { lane: i, db });
  }

  lock(id, on) {
    if (on) this.locks.add(id); else this.locks.delete(id);
  }
}
