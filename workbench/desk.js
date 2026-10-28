// SPDX-License-Identifier: GPL-3.0-or-later
// The decision desk: the rows' state (values, locks, hand edits, what moved) and how a change reaches the engine.
// Every change is staged (wb_stage_mark first), checked against the locks (a locked row that moved undoes it whole and
// says why), then committed with wb_apply: nothing staged is heard before that, so a bar line inside a change never
// lands half of it. A seed or map change regenerates the song, so every locked or hand-set row is staged again first. call(fn, ...args) reaches wb.wasm's exports, a Promise (wb.js in the page; smoke.mjs in Node).
import { ROWS, row, conflicts, pickFor, familyOf, fromEngine, kickMasks } from "./rows.js";

const SEED_TRIES = 20;   // a new seed whose song can't keep the locks is tried again (re-roll all, the seed's dice)

export class Desk {
  // probe(bytes): the lead register the engine applies to a staged state (wb_stage_bytes), from a second wb.wasm
  // instance off the audio thread (wb.js's probe); without one the lead register isn't read back
  constructor(call, C, opts, lib, { rnd = Math.random, probe = null } = {}) {
    Object.assign(this, { call, C, opts, lib, rnd, probe });
    this.values = {};
    this.locks = new Set();
    this.hand = new Set();     // rows set from their dropdown (they survive a regenerate, as locks do)
    this.moved = new Set();    // rows the last change forced (highlighted)
    this.legal = {};
    this.message = "";
  }

  get state() { return { values: this.values, locks: this.locks, legal: this.legal }; }
  kept(id) { return this.locks.has(id) || this.hand.has(id); }

  // the engine's staged state into values; the family is the page's (derive: from the keys and lead, as a new song's)
  async read(derive = false) {
    const fam = this.values.family;
    this.values = fromEngine(await this.call("wb_state"), this.C, this.opts);
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
    if (id === "kit") return c("wb_kit_set", v);
    if (id === "kick") return c("wb_kick_phrase", ...kickMasks(v, this.opts));
    if (id === "pattern") {
      await c("wb_roll_set", RF.PATTERN, v);
      if (this.values.kick) await c("wb_kick_phrase", ...kickMasks(this.values.kick, this.opts));
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
    const c = r.regenerates || r.kind === "roll" ? { forced: [], stuck: [], blocked: null }
                                                  : conflicts(r, value, this.state, this.opts, this.rnd, this.lib);
    if (c.blocked) return this.done(false, `Blocked: ${c.blocked.label} is locked (${c.blocked.why}).`);
    const before = { ...this.values };
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

  lock(id, on) {
    if (on) this.locks.add(id); else this.locks.delete(id);
  }
}
