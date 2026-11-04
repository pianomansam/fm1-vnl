// SPDX-License-Identifier: GPL-3.0-or-later
// The decision desk: the rows' state (values, locks, hand edits, what moved) and how a change reaches the engine.
// Every change is staged (wb_stage_mark first), checked against the locks (a locked row that moved undoes it whole and
// says why), then committed with wb_apply: nothing staged is heard before that, so a bar line inside a change never
// lands half of it. A seed or map change regenerates the song, so every locked or hand-set row is staged again first. call(fn, ...args) reaches wb.wasm's exports, a Promise (wb.js in the page; smoke.mjs in Node).
import { ROWS, row, conflicts, pickFor, familyOf, fromEngine, kickMasks, rolled, compFits } from "./rows.js";
import { cellOf, LANES } from "./drums.js";
import { byPart } from "./roll.js";

// the parts a listener can mute or solo (the page's names) and the engine's role for each (the drums' bus on V_KICK)
export const PARTS = { keys: "CHORD", bass: "BASS", lead: "LEAD", pad: "PAD", drums: "KICK" };

// each part's salt row (rows.js's part: its 🎲 sets it), and its name in messages
export const PART_ROW = Object.fromEntries(ROWS.filter((r) => r.part).map((r) => [r.part, r.id]));
const NAME = { keys: "Keys", bass: "Bass", lead: "Lead", pad: "Pad", drums: "Drums" };

// What part p's events moving says, after a re-roll of part by (moved: every part that moved): "<part> moved", and
// why where the engine's rules say (a part re-roll moves only its own streams; these are its known reactions)
const WHY = {
  keys: (moved, by) => (moved.includes("lead") || by === "lead" ? "they make room for the lead" : ""),
  lead: (moved, by) => (by === "bass" ? "it keeps clear of the bass" : ""),
};
// what a part's events sound as: each (bar, step, role, note, length), sorted. Velocity and the order within a step
// don't count (a salt that moves only those isn't heard as a new roll)
export const notesOf = (evs) => evs.map((e) => `${e.bar}.${e.step}.${e.role}.${e.note}.${e.len}`).sort().join(" ");

// a part whose 🎲 found no other roll (no salt of ROLL_TRIES changed its notes): why, by part
const ROLL_TRIES = 8, CS_STRUM = 5;
const DEAD = {
  bass: "Bass: no other bass line fits this song.",
  lead: "Lead: no other phrase fits this song.",
  pad: "Pad: no other pad part fits this song.",
  drums: "Drums: no other variation fits this pattern.",
};
export const reaction = (p, by, moved) => { const w = WHY[p]?.(moved, by); return `${NAME[p]} moved${w ? `: ${w}` : ""}.`; };

// the section (wb_sections' { start, bars, name }) holding bar (from 1: wb_bar's), or null
export const sectionAt = (secs, bar) => secs?.find((x) => bar > x.start && bar <= x.start + x.bars) ?? null;

// looping a section: the bar (from 1) its last bar starts → the section's start to jump back to (landing at the next bar
// line, as the section ends), else null
export const loopAt = (secs, bar) => { const s = sectionAt(secs, bar); return s && bar === s.start + s.bars ? s.start : null; };

// where the song heard is: { seconds, bar, section } (the section's name in the song playing, or null). live: the playing
// song's sections (wb_live_sections), not the committed ones (a change may be waiting)
export const positionOf = (pos, live) => ({ seconds: pos.seconds, bar: pos.bar, section: sectionAt(live, pos.bar)?.name ?? null });

const SEED_TRIES = 20;   // a new seed whose song can't keep the locks is tried again (re-roll all, the seed's dice)

// a die from a seed (the same song lands on the same kit every time)
const seeded = (seed) => { let x = (seed >>> 0) || 1; return () => ((x = (Math.imul(x, 1103515245) + 12345) >>> 0) / 4294967296); };

export class Desk {
  // probe(bytes): the lead register the engine applies to a staged state (wb_stage_bytes), from a second wb.wasm
  // instance off the audio thread (wb.js's probe); without one the lead register isn't read back
  // position(): { seconds, bar } of the song playing, for the change log (none: 0, 0)
  // samples: wb.js's Samples (the sampled kits: which are unavailable, and their files loaded on first use), or none
  // events(bytes): the committed song's { events } (wb.js's events, on the probe instance), for the parts' reactions and
  // whether the song has a pad; without it neither is known
  constructor(call, C, opts, lib, { rnd = Math.random, probe = null, position = () => ({ seconds: 0, bar: 0 }), samples = null,
                                    events = null } = {}) {
    Object.assign(this, { call, C, opts, lib, rnd, probe, position, samples, events });
    this.changes = [];         // every applied change: { seconds, bar, row, from, to }
    this.values = {};
    this.locks = new Set();
    this.hand = new Set();     // rows set from their dropdown (they survive a regenerate, as locks do)
    this.moved = new Set();    // rows the last change forced (highlighted)
    this.listen = { mute: new Set(), solo: null };   // parts muted, the part soloed: listening only, kept across regenerates
    this.legal = {};
    this.message = "";
    this.rolls = {};           // each part's rolls: { history: [{ salt } (the drums': { salt, kick })], at }
    this.partEv = null;        // the committed song's events by part (refresh())
    this.padless = false;      // the committed song has no pad (its pad 🎲 and ◀ ▶ wait for one)
    this.reactions = [];       // what else the last part roll moved ("Lead moved: ...")
    this.dead = {};            // parts whose 🎲 found no other roll in the song as it stands: why (cleared by any change)
    this.approved = new Set(); // candidates approved this session ("family/id"): approved once import.sh has run
    this.strums = undefined;   // the staged song's keys strum (the strum pattern row is heard only then)
  }

  get state() { return { values: this.values, locks: this.locks, legal: this.legal, unavailable: this.samples?.unavailable,
                         strums: this.strums }; }
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
    this.strums = (await this.call("wb_comp_applied")) === CS_STRUM;
    return this.values;
  }

  // POST /approve (f: fetch, the page's; a test's): the candidate t (rows.js's approval: { family, id, name }) approved
  // in patterns/status.json. The roll draws it once tools/patterns/import.sh has run (and the workbench is rebuilt);
  // until then it plays here as before, when picked.
  async approve(t, f = globalThis.fetch) {
    const r = await f("/approve", { method: "POST", headers: { "Content-Type": "application/json" },
                                    body: JSON.stringify({ family: t.family, id: t.id }) });
    const j = await r.json().catch(() => ({}));
    if (!r.ok || !j.ok) return this.done(false, `${t.name} wasn't approved: ${j.error || `the server said ${r.status}`}.`);
    this.approved.add(`${t.family}/${t.id}`);
    return this.done(true, `${t.name} approved (patterns/status.json). Run tools/patterns/import.sh so the firmware ` +
                           "draws it; sh workbench/build.sh (import.sh runs it) lets the workbench roll it too.");
  }

  async stage(id, v) {
    const r = row(id), { RF, V, OV } = this.C, c = this.call;
    if (r.kind === "seed") return c("wb_seed", v >>> 0);
    if (r.kind === "roll") return c("wb_roll_set", RF[r.field], v);
    if (r.kind === "role") return c("wb_role_set", V[r.role], v);
    if (r.kind === "ov" || r.kind === "reroll") return c("wb_ov_set", OV[r.key], v);
    if (id === "kit") return (await this.samples?.kit(v)) === false ? false : c("wb_kit_set", v);   // false: unavailable
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
  // seed also returns the other overrides to the engine's (a new map, the parts' salts). Then the family's kit, unless kept.
  async reapply(except, kept, fresh) {
    for (const r of ROWS) {
      if (r.id === except || r.kind === "seed" || r.kind === "family") continue;
      if (kept.has(r.id)) await this.stage(r.id, kept.get(r.id));
      else if ((fresh || r.part) && (r.kind === "ov" || r.kind === "reroll")) await this.stage(r.id, rolled(r));
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
  // step: a part's salt set from its history (not a new roll); also: [[row, value]] staged with it (the drums' kick)
  async set(id, value, { dice = false, release = false, step = false, also = [] } = {}) {
    const r = row(id);
    if (this.locks.has(id)) return this.done(false, `${r.label} is locked.`);
    if (id === "kick" && this.locks.has(PART_ROW.drums)) return this.done(false, "Kick phrase: the drums part is locked (its roll holds the kick).");
    const comp = id === "comp" ? this.opts.comping.find((c) => c.id === value) : null;
    if (comp && !compFits(comp, this.values.keys)) {   // a style these keys can't play (rows.js offers it not)
      const name = (k) => this.opts.instruments.find((i) => i.id === k)?.name ?? `instrument ${k}`;
      return this.done(false, `${comp.name} comping doesn't suit the ${name(this.values.keys)}: ` +
                              (comp.keys ? `it plays on the ${comp.keys.map(name).join(", ")} only.`
                                         : "the guitars only strum."));
    }
    for (const p of Object.keys(PARTS)) this.rollOf(p);   // each part's history starts at the roll before this change
    const prevEv = this.partEv;
    if (id === "kit" && this.samples && !(await this.samples.kit(value))) {
      const k = this.opts.kits.find((x) => x.id === value);
      return this.done(false, `${k?.name ?? "That kit"} is unavailable: its sample files are missing (see workbench/samples/README.md).`);
    }
    const c = r.regenerates || r.kind === "roll" ? { forced: [], stuck: [], blocked: null }
                                                  : conflicts(r, value, this.state, this.opts, this.rnd, this.lib);
    if (c.blocked) return this.done(false, `Blocked: ${c.blocked.label} is locked (${c.blocked.why}).`);
    const before = { ...this.values }, grid = this.drums?.grid, lanes = this.drums?.lanes, dropped = [];
    // a part's salt is kept only when locked (a salt means something else in a new song)
    // (and a locked drums part keeps its kick phrase: the roll is its salt and kick)
    const keep = (x) => (x === "kick" && this.locks.has(PART_ROW.drums)) || (release || row(x).part ? this.locks.has(x) : this.kept(x));
    const kept = new Map([...ROWS].filter((x) => keep(x.id) && x.id !== id).map((x) => [x.id, before[x.id]]));
    const forced = new Set([...c.forced.map((f) => f.row), ...also.map(([x]) => x)]);
    await this.call("wb_stage_mark");
    await this.stage(id, value);
    for (const [x, v] of also) await this.stage(x, v);
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
      if (JSON.stringify(this.drums.grid) !== JSON.stringify(grid) && !(await this.stageGrid(grid)))
        dropped.push("The drawn pattern couldn't be kept: its base pattern isn't legal in this song.");
    }
    // the edited lanes (sounds, levels) go with a kept kit: staging the kit again resets them to the kit as shipped
    if (lanes?.some((l) => l[0] !== this.C.SRC.NONE) && kept.has("kit") && id !== "kit" && !forced.has("kit")) {
      await this.read();
      if (JSON.stringify(this.drums.lanes) !== JSON.stringify(lanes)) {
        const d = await this.stageDrums({ lanes });
        if (d.refused) dropped.push(`The edited drum lanes couldn't be kept: ${d.refused}`);
        else if (d.missing.length) dropped.push(`Not kept: ${d.missing.join(", ")}.`);
      }
    }
    await this.read(r.kind === "seed" && !kept.has("family"));
    const replaced = id !== "kit" && !kept.has("kit") && (r.regenerates || this.values.kit !== before.kit) ? await this.approvedKit() : null;
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
    this.moved = new Set(r.regenerates ? (replaced ? ["kit"] : []) : changed.map((x) => x.id));
    for (const x of changed) if (!r.regenerates) this.hand.delete(x.id);
    this.message = "";
    await this.readBack();
    await this.call("wb_apply");
    if (r.regenerates)   // a new song: the unlocked parts start over, as rolled
      for (const p of Object.keys(PARTS)) if (!this.locks.has(PART_ROW[p])) delete this.rolls[p];
    this.dead = {};
    await this.refresh();
    // a new roll goes on the end of its history, if it changed the part's notes (unknown without events: it goes on);
    // the kick row changed on its own is a drums roll too
    const rolled_ = r.part ?? (id === "kick" ? "drums" : null);
    if (rolled_ && !step && !(prevEv && this.partEv && notesOf(prevEv[rolled_]) === notesOf(this.partEv[rolled_]))) {
      const h = this.rollOf(rolled_);
      h.history.push(this.entry(rolled_));
      h.at = h.history.length - 1;
    }
    const at = this.position();
    for (const x of [r, ...changed.filter((x) => x.id !== id)])
      this.changes.push({ seconds: at.seconds, bar: at.bar, row: x.id, from: before[x.id], to: this.values[x.id] });
    const notes = [
      ...c.stuck.map((s) => `${row(s.row).label} stays: no approved ${value} option yet.`),
      ...lost.map((k) => `${row(k).label} couldn't be kept in the new song.`),
      ...dropped,
      ...(replaced ? [replaced] : []),
    ];
    if (this.message) notes.push(this.message);
    return this.done(true, notes.join(" "));
  }

  // the committed song's sections: [{ start (bar from 0), bars, name }]
  sections() { return this.call("wb_sections"); }

  // Play from a section's first bar (start, from 0): playing, at the next bar line (with any change waiting); stopped,
  // Play starts there
  async jumpTo(start, name = "that section") {
    await this.call("wb_jump", start);
    this.moved = new Set();
    return this.done(true, `Jumping to ${name} (bar ${start + 1}) at the next bar line.`);
  }

  // mute and solo, heard at once (listening only: the song is unchanged)
  async hear() {
    const { V } = this.C, { mute, solo } = this.listen;
    await this.call("wb_mute", [...mute].reduce((m, p) => m | (1 << V[PARTS[p]]), 0));
    await this.call("wb_solo", solo ? V[PARTS[solo]] : -1);
  }
  async mute(part, on) {
    if (on) this.listen.mute.add(part); else this.listen.mute.delete(part);
    await this.hear();
    return this.done(true, "");
  }
  async solo(part) {
    this.listen.solo = part;
    await this.hear();
    return this.done(true, "");
  }

  done(ok, message) {
    this.message = message;
    if (!ok) this.moved = new Set();
    return { ok, message, moved: [...this.moved] };
  }

  // A song that landed on a retired kit (most seeds roll one) gets an approved kit of its family instead, the seed's
  // own pick. Staged only; the note saying so, or null when the kit is approved (or nothing fits).
  async approvedKit() {
    const was = this.opts.kits.find((k) => k.id === this.values.kit);
    if (was?.status !== "retired") return null;
    const no = new Set(this.samples?.unavailable);   // and a kit the engine didn't take (a sampled one not loaded)
    for (;;) {
      const v = pickFor(row("kit"), { ...this.state, unavailable: no }, this.opts, seeded(this.values.seed), this.lib);
      if (v === null) return null;
      if ((await this.stage("kit", v)) !== false && (await this.read()).kit === v)
        return `Kit: ${was.name} is retired; the song plays ${this.opts.kits.find((k) => k.id === v)?.name} instead.`;
      no.add(v);
    }
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
    if (r.part) return this.reroll(r.part);
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
    if (v === null) return this.done(false, `${r.label}: no other approved option${id === "kit" ? ` for a ${this.values.family} song` : ""}.`);
    return this.set(id, v, { dice: true });
  }

  // ---- the parts (keys, bass, lead, pad, drums): a 🎲 each (a new salt for that part's own streams), its history of
  // rolls (◀ ▶), a lock (its salt row's), and what else moved ----

  // { part: { salt, history, at, locked } }
  get parts() {
    return Object.fromEntries(Object.keys(PARTS).map((p) => {
      const h = this.rollOf(p);
      return [p, { salt: this.values[PART_ROW[p]] ?? 0, history: h.history.map((e) => ({ ...e })), at: h.at, locked: this.locks.has(PART_ROW[p]) }];
    }));
  }
  // the part's roll as it stands (a history entry); its history (a new one: this roll alone)
  entry(p) { return p === "drums" ? { salt: this.values.drumsVar ?? 0, kick: this.values.kick ?? "" } : { salt: this.values[PART_ROW[p]] ?? 0 }; }
  rollOf(p) { return (this.rolls[p] ??= { history: [this.entry(p)], at: 0 }); }

  // why part p can't take a new roll (step: or step through its history), or null: it can
  cantRoll(p, step = false) {
    if (this.locks.has(PART_ROW[p])) return `${NAME[p]} is locked.`;
    if (p === "pad" && this.padless) return "No pad in this song: its rolls are kept for when it has one (Pad on/off).";
    if (p === "drums" && this.drums?.grid?.edited) return "Drums: the pattern grid holds hand edits; reset the drawn pattern to re-roll.";
    return (!step && this.dead[p]) || null;
  }

  // the song's events by part with changes ([[row, value]]) staged on the committed state, nothing kept staged
  async trial(changes) {
    await this.call("wb_stage_mark");
    for (const [id, v] of changes) await this.stage(id, v);
    const bytes = await this.call("wb_stage_bytes");
    await this.call("wb_stage_undo");
    return byPart((await this.events(bytes)).events, this.C.V);
  }

  // why part p has one roll only (its 🎲 found no other)
  async deadWhy(p) {
    if (p !== "keys") return DEAD[p];
    const cs = await this.call("wb_comp_applied");   // (the staged state is the committed one between changes)
    return cs === CS_STRUM ? "Keys: the strummed guitar keeps its shapes (one voicing path)." : "Keys: only one voicing fits this progression.";
  }

  // the committed song's events by part (null without events()); kept as partEv, with padless
  async refresh() {
    if (!this.events) return null;
    this.partEv = byPart((await this.events(await this.call("wb_commit_bytes"))).events, this.C.V);
    this.padless = !this.partEv.pad.length;
    return this.partEv;
  }

  // f() changes part's salt: its result, the message saying which other parts' events moved (the committed song before
  // and after) and whether the part's own did
  async reacting(part, f) {
    const before = this.events ? await this.refresh() : null;
    const res = await f();
    if (!res.ok || !before) return res;
    const after = this.partEv, same = (p) => notesOf(before[p]) === notesOf(after[p]);
    const moved = Object.keys(PARTS).filter((p) => p !== part && !same(p));
    this.reactions = moved.map((p) => reaction(p, part, moved));
    const notes = [res.message, ...(same(part) ? [`${NAME[part]}: this roll plays the same notes in this song.`] : []), ...this.reactions];
    return this.done(true, notes.filter(Boolean).join(" "));
  }

  // 🎲 for a part: a new nonzero salt (the drums: a kick phrase from options.json's too, unless the kick is locked);
  // it lands at the next bar line and goes on the end of the part's history. With events, salts are tried on the probe
  // first (up to ROLL_TRIES): the first that changes the part's notes is the one committed; none: the part has one
  // roll in this song, and its 🎲 says why and waits for a change
  async reroll(p) {
    const why = this.cantRoll(p);
    if (why) return this.done(false, why);
    const also = [], kick = this.values.kick, handKick = this.hand.has("kick") && kick && kick !== "grid", salt = () => 1 + Math.floor(this.rnd() * 0x7ffffffe);
    if (p === "drums" && !this.locks.has("kick")) {
      const ks = this.opts.kickPhrases.filter((k) => (k.status ?? "approved") === "approved" && k.id !== kick);
      if (ks.length) also.push(["kick", ks[Math.floor(this.rnd() * ks.length) % ks.length].id]);
    }
    let v = salt();
    if (this.events) {
      const was = notesOf((await this.refresh())[p]);
      for (let i = 0; notesOf((await this.trial([[PART_ROW[p], v], ...also]))[p]) === was; i++) {
        if (i + 1 >= ROLL_TRIES) return this.done(false, (this.dead[p] = await this.deadWhy(p)));
        v = salt();
      }
    }
    const res = await this.reacting(p, () => this.set(PART_ROW[p], v, { dice: true, also }));
    // a kick phrase set by hand that the drums' roll replaced: said
    if (res.ok && also.length && handKick)
      return this.done(true, [`Kick phrase: ${this.values.kick} (was your ${kick}).`, res.message].filter(Boolean).join(" "));
    return res;
  }

  // ◀ (d -1) or ▶ (d 1) through part p's history: that roll exactly (the drums' kick too). Past either end: nothing
  async stepPart(p, d) {
    const h = this.rollOf(p), to = h.at + d;
    if (to < 0 || to >= h.history.length) return this.done(false, `${NAME[p]}: no ${d < 0 ? "earlier" : "later"} roll.`);
    const why = this.cantRoll(p, true);
    if (why) return this.done(false, why);
    const e = h.history[to];
    const also = p === "drums" && e.kick !== this.values.kick && !this.locks.has("kick") ? [["kick", e.kick]] : [];
    const res = await this.reacting(p, () => this.set(PART_ROW[p], e.salt, { dice: true, step: true, also }));
    if (res.ok) h.at = to;
    return res;
  }

  // Re-roll everything unlocked: a new seed (hand edits let go) when the seed isn't locked, else each row's dice
  async rerollAll() {
    if (!this.locks.has("seed")) return this.dice("seed", { release: true });
    for (const id of [...this.hand]) if (!this.locks.has(id)) this.hand.delete(id);
    const notes = [];
    let drums = false;   // the drums' roll drew a kick phrase: the kick row's dice doesn't draw another over it
    for (const r of ROWS) {
      if (this.locks.has(r.id) || r.kind === "seed" || (r.id === "kick" && (drums || this.locks.has(PART_ROW.drums)))) continue;
      const res = await this.dice(r.id);
      if (r.part === "drums") drums = res.ok;
      if (!res.ok && (res.message.startsWith("Blocked") || r.part)) notes.push(res.message);   // (a part: why it didn't roll)
    }
    this.moved = new Set();
    return this.done(true, notes.join(" "));
  }

  // Everything a replay needs (version 1; later tasks add to it): the rows of the song being HEARD (the engine's live
  // state: a change still waiting for its bar line isn't in it), those of the waiting commit (pending, or null), the
  // locks and hand edits, the seed. "drums" is the drum state the rows don't hold (the kit, pattern and kick are rows
  // already): the heard song's drum lanes and grid, { preset, lanes: [[kind, a, b, dB], ...], kit_lanes, grid: { base,
  // edited, lanes } } (wb_state's; restore stages the grid if it was edited). "listen": { mute: [parts], solo: part|null }
  // (absent in older snapshots: none). "parts": { part: { salt, history, at, locked } } (the desk's: a roll still waiting
  // for its bar line is in it; the salts heard are the rows'; absent in older snapshots: every part as rolled).
  async snapshot() {
    const pending = (await this.call("wb_pending")) ? { ...this.values } : null;
    let rows = { ...this.values }, heard = await this.call(pending ? "wb_live_state" : "wb_state");
    if (pending) {
      rows = fromEngine(heard, this.C, this.opts);
      rows.family = familyOf(rows, this.opts);
    }
    return { version: 1, rows, pending, locks: [...this.locks], hand: [...this.hand], seed: rows.seed, drums: heard.drums,
             listen: { mute: [...this.listen.mute], solo: this.listen.solo }, parts: this.parts };
  }

  // Put a snapshot's state on the engine (staged, then committed) as the page's own stagings would; the rows that
  // came out different (the engine kept another value) are returned.
  async restore(snap) {
    if (snap.version !== 1) throw new Error(`snapshot version ${snap.version}: this page reads version 1`);
    // a row newer than the snapshot (an override: the pad on/off) as rolled; a row it names that is gone (the lead
    // doubling switch, the arpeggio rows: lead styles, 2026-10-31) is ignored
    const older = ROWS.filter((r) => (r.kind === "ov" || r.kind === "reroll") && !(r.id in snap.rows));
    snap = { ...snap, rows: { ...snap.rows, ...Object.fromEntries(older.map((r) => [r.id, rolled(r)])) } };
    // a retired lead style (the phrase planner, the hybrid: lead styles, 2026-10-31) restores as rolled: said, not a
    // mismatch
    const style = this.opts.leadStyles.find((s) => s.id === snap.rows.leadStyle);
    const styleGone = style?.status === "retired";
    if (styleGone) snap = { ...snap, rows: { ...snap.rows, leadStyle: rolled(row("leadStyle")) } };
    await this.call("wb_stage_mark");
    await this.stage("seed", snap.rows.seed);
    for (const r of ROWS) if (r.kind !== "seed" && r.kind !== "family") await this.stage(r.id, snap.rows[r.id]);
    const d = await this.stageDrums(snap.drums), missing = d.refused ? [...d.missing, "drums (the kit row's kit can't take lanes)"] : d.missing;
    await this.call("wb_apply");
    this.listen = { mute: new Set(snap.listen?.mute), solo: snap.listen?.solo ?? null };   // an older snapshot: none
    await this.hear();
    this.values = { ...snap.rows };
    this.locks = new Set((snap.locks ?? []).filter((id) => row(id)));   // (a gone row's lock: dropped)
    this.hand = new Set((snap.hand ?? []).filter((id) => row(id)));
    this.moved = new Set();
    await this.read();
    this.rolls = {};   // each part's history, at the roll the rows hold (a snapshot without parts: a history of that roll)
    for (const [p, x] of Object.entries(snap.parts ?? {})) {
      if (!(p in PARTS) || !x.history?.length) continue;
      if (x.locked) this.locks.add(PART_ROW[p]);
      const cur = JSON.stringify(this.entry(p)), history = x.history.map((e) => ({ ...e }));
      let at = history.findIndex((e, i) => i === x.at && JSON.stringify(e) === cur);
      if (at < 0) at = history.findIndex((e) => JSON.stringify(e) === cur);
      if (at < 0) at = history.push(this.entry(p)) - 1;
      this.rolls[p] = { history, at };
    }
    await this.refresh();
    // a retired lead (the muted trumpet, sax, guitar lick, string lead, cello: Sam, 2026-10-29; the kalimba, saw lead,
    // synth pluck, and the Emilyguitar as a lead: the lead styles build) lands as the lead the engine stands in for it
    // (library.c's lib_lead_approved): said, not a mismatch
    const was = this.opts.instruments.find((i) => i.id === snap.rows.lead);
    const asLead = was?.roles.includes("lead");
    const swapped = was && !(was.status === "approved" && asLead) && this.values.lead !== snap.rows.lead;
    // a removed bass (the upright, 2026-10-31) likewise, as the electric bass (library.c's lib_bass_approved)
    const wasBass = this.opts.instruments.find((i) => i.id === snap.rows.bass);
    const bassSwapped = wasBass?.status === "retired" && this.values.bass !== snap.rows.bass;
    // removed keys (the Wurli, the DX electric piano: 2026-11-04) likewise, as the Rhodes (library.c's
    // lib_keys_approved)
    const wasKeys = this.opts.instruments.find((i) => i.id === snap.rows.keys);
    const keysSwapped = wasKeys?.status === "retired" && this.values.keys !== snap.rows.keys;
    const name = (id) => this.opts.instruments.find((i) => i.id === id)?.name;
    this.message = [swapped ? `Lead: ${was.name} is retired${was.status === "approved" ? " as a lead" : ""}; the song plays ${name(this.values.lead)} instead.` : "",
                    bassSwapped ? `Bass: ${wasBass.name} is retired; the song plays ${name(this.values.bass)} instead.` : "",
                    keysSwapped ? `Keys: ${wasKeys.name} is retired; the song plays ${name(this.values.keys)} instead.` : "",
                    styleGone ? `Lead style: ${style.name} is retired; the song plays a rolled style instead.` : ""]
      .filter(Boolean).join(" ");
    return [...ROWS.filter((r) => !(swapped && r.id === "lead") && !(bassSwapped && r.id === "bass") &&
                                  !(keysSwapped && r.id === "keys") &&
                                  String(this.values[r.id]) !== String(snap.rows[r.id])).map((r) => r.id), ...missing];
  }

  // A snapshot's drum lanes over the kit row's (a preset's, or the song's own kit's): the lanes with a source first (the
  // first one set on a song with none fills the others from its kit), then the silent ones; each lane's level.
  // { missing, refused }: missing, what couldn't be restored (["drums (<kit>: samples missing)"] for a sample set that
  // didn't load: its lanes keep the kit row's sources); refused, why the engine refused a lane (a retired mixed sampled
  // kit's hits aren't lane sources), or null: the caller undoes it (no level is staged)
  async stageDrums(d) {
    const lanes = d?.lanes || [], missing = [];
    if (d?.grid?.edited && !(await this.stageGrid(d.grid))) missing.push("the drawn pattern (its base pattern isn't legal here)");
    if (!lanes.some((l) => l[0] !== this.C.SRC.NONE)) return { missing, refused: null };
    for (const a of new Set(lanes.filter((l) => l[0] === this.C.SRC.SAMPLE).map((l) => l[1])))
      if (!this.samples || !(await this.samples.load(a))) {
        const k = this.opts.kits.find((x) => x.set === a);
        missing.push(`drums (${k?.name ?? `sample set ${a}`}: samples missing)`);
      }
    const no = [];
    for (const silent of [false, true])
      for (const [i, [kind, a, b]] of lanes.entries())
        if ((kind === this.C.SRC.NONE) === silent && !(await this.call("wb_lane_src", i, kind, a, b))) no.push(i);
    if (no.length) {
      const k = this.opts.kits.find((x) => x.id === this.values.kit);
      return { missing, refused: (await this.call("wb_state")).drums.kit_lanes
        ? `the engine has no such sound for ${no.map((i) => LANES[i].toLowerCase()).join(", ")}.`
        : `${k?.name ?? "This kit"} can't take lanes (its hits are mixed copies, not sources): choose a shipped kit first.` };
    }
    for (const [i, l] of lanes.entries()) await this.call("wb_lane_db", i, l[3]);
    return { missing, refused: null };
  }

  // Lane i plays a source (WB_SRC_*), staged and committed (the drum screen's). The first on a song with none copies
  // its kit to the others; a retired mixed sampled kit refuses: its hits aren't lane sources. { ok, message }
  async lane(i, kind, a, b) {
    if (kind === this.C.SRC.SAMPLE && this.samples && !(await this.samples.load(a)))
      return this.done(false, "Those samples are missing (see workbench/samples/README.md).");
    const k = this.opts.kits.find((x) => x.id === this.values.kit);
    const refused = this.drums && !this.drums.kit_lanes
      ? `${k?.name ?? "This kit"} can't be edited lane by lane (its hits are mixed copies, not sources): choose a shipped kit first.`
      : "That sound isn't one the engine has.";
    return this.drumEdit("drums", async () => ((await this.call("wb_lane_src", i, kind, a, b)) ? null : { refused }),
                         { lane: i, source: [kind, a, b] });
  }

  // A grid (wb_state's drums.grid: its library base and each lane's masks) as the custom pattern: the base loaded,
  // then the steps that differ (sent together: they run between the same two quanta). Staged only; false (nothing
  // staged) if the base pattern isn't legal for the staged song.
  async stageGrid(g) {
    if (!(await this.call("wb_pat_load", g.base))) return false;
    const have = (await this.call("wb_state")).drums.grid.lanes, jobs = [];
    for (const [lane, m] of g.lanes.entries())
      for (let bar = 0; bar < 2; bar++)
        for (let step = 0; step < 16; step++) {
          const want = cellOf(m, bar, step);
          if (want !== cellOf(have[lane], bar, step)) jobs.push(this.call("wb_pat_step", lane, bar, step, want));
        }
    await Promise.all(jobs);
    return true;
  }

  // A drum screen edit: staged, checked against the locks (a locked row it would move undoes it whole), committed
  // (it lands at the next bar line) and logged. stage() may refuse ({ refused: why }: undone) or return what it
  // couldn't restore (a list). { ok, message }
  async drumEdit(what, stage, detail = {}) {
    const before = { ...this.values };
    await this.call("wb_stage_mark");
    const res = await stage();
    if (res?.refused) {
      await this.call("wb_stage_undo");
      await this.read();
      return this.done(false, res.refused);
    }
    await this.read();
    const moved = ROWS.filter((x) => this.locks.has(x.id) && String(this.values[x.id]) !== String(before[x.id]));
    if (moved.length) {
      await this.call("wb_stage_undo");
      await this.read();
      return this.done(false, `Blocked: ${moved.map((x) => x.label).join(", ")} ${moved.length > 1 ? "are" : "is"} locked and this edit would move ${moved.length > 1 ? "them" : "it"}.`);
    }
    this.hand.add(what === "grid" ? "pattern" : "kit");   // a hand edit of the pattern or kit: it survives a new seed
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
    const from = this.opts.patterns.find((x) => x.id === p.grid?.base)?.name ?? `pattern ${p.grid?.base}`;
    return this.drumEdit("grid", async () => ((await this.stageGrid(p.grid)) ? null
      : { refused: `${p.name ?? "That pattern"} can't load: it's drawn over ${from}, which this song can't take.` }), { loaded: p.name });
  }
  // a kit file names the kit it was saved from: that kit goes on the kit row first (so the family logic sees it)
  loadKit(k) {
    const from = this.opts.kits.find((x) => x.name === k.from);
    return this.drumEdit("drums", async () => {
      if (from && from.id !== this.values.kit) {
        if ((await this.stage("kit", from.id)) === false) return { refused: `${from.name} is unavailable: its sample files are missing.` };
        await this.read();
      }
      const d = await this.stageDrums({ lanes: k.lanes });
      return d.refused ? { refused: `${k.name ?? "That kit"} can't load: ${d.refused}` } : d.missing;
    }, { loaded: k.name });
  }

  // lane's level in dB (a song with no lane sources gets its kit's on every lane first)
  level(i, db) {
    return this.drumEdit("drums", () => this.call("wb_lane_db", i, db), { lane: i, db });
  }

  lock(id, on) {
    if (on) this.locks.add(id); else this.locks.delete(id);
  }
}
