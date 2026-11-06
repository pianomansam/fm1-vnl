// SPDX-License-Identifier: GPL-3.0-or-later
// The main page's decision rows (layout A): what each row is, the options its dropdown offers, its dice, and what a
// new value forces elsewhere. Pure: no DOM, no engine (desk.js drives the engine; smoke.mjs tests this directly).
//   opts: options.json; state: { values: {row id: value}, locks: Set of row ids, legal: {prog, mode} (optional),
//   unavailable: Set of kit ids whose samples are missing (optional: offered marked, never chosen), strums: the comp
//   strums in the staged song (optional), comp: the comping the engine applies to it, a Comping value (optional),
//   padless: the committed song has no pad (optional), grid: the drum grid holds hand edits (optional) };
//   lib: wb_library()'s maps and progressions.

// The rows, the sections and the comp's fit rules are data (rows.json), which the device's tables are generated from
// too (tools/screens/gen.py). A row's kind: seed | roll (a roll field) | family (the page's own) | role (an instrument
// in the custom set) | ov (an override) | reroll (an override's salt) | drum (kit, pattern, kick phrase). field: RF_*;
// role: V_*; key: WB_OV_*. part: the part a salt re-rolls (desk.js's parts: the part header's 🎲 sets it). label: the
// row's name in messages and tooltips; short: its name inside its section (SECTIONS), which names the part. The keys
// rows are the Comp part's in the page's text; their ids (keys, keysOct) and the engine's names (V_CHORD, KEYS_OCT)
// stay. The mode row (labeled Scale) is the six modes (major or minor from 2026-11-05 to 06: desk.js maps those
// snapshots). One place to pick comping (Sam 2026-11-05): the comp row lists the styles and the comp rhythms in one
// list; desk.js stages a comp rhythm as the COMP_PAT override, anything else as COMP.
import DATA from "./rows.json" with { type: "json" };
export const ROWS = DATA.rows;
export const row = (id) => ROWS.find((r) => r.id === id);

// The page's sections, top to bottom, built up like a track: the song, each part, the texture (ROWS keeps its own
// order: the desk stages and rolls in it). part: desk.js's PARTS key (its header carries the part's mute, solo, rolls,
// lock and feedback, and summary()). main: the rows always shown (a part's instrument and its rhythm or style; the
// song's are the device's home knobs and the seed); more: those folded under the section's "more" toggle.
export const SECTIONS = DATA.sections;
export const sectionOf = (id) => SECTIONS.find((s) => s.main.includes(id) || s.more.includes(id));

// The Comp part's one Rhythm row (Sam, 2026-11-05): the comp rhythms (the comp row) on keys, the strum patterns (the
// strumPat row) on the guitars, which only strum. The page shows the row this names and hides the other; both stay
// rows of their own (their overrides, locks and snapshot values unchanged). A comp rhythm a guitar can't play, left by a
// snapshot or the engine, keeps the comp row in view (greyed, open to change).
const RR = DATA.fits.rhythmRow;   // { strumSym, compRow, strumRow }
export const RHYTHM_ROWS = [RR.compRow, RR.strumRow];
export function rhythmRow(state, opts) {
  const strum = opts.comping.find((c) => c.sym === RR.strumSym), v = state.values;
  if (!strum || !compFits(strum, v.keys)) return RR.compRow;
  const c = v.comp >= 0 ? compOption(opts, v.comp) : null;
  return c && c.sym !== RR.strumSym && !compFits(c, v.keys) ? RR.compRow : RR.strumRow;
}

// Each section's "more" open or folded, per viewer (localStorage, when there is one: a private window or blocked
// storage throws, and the page works without it, folded). storage(): the Storage, or a test's stand-in
export const FOLD_KEY = "wb-more";
export function folds(storage = () => globalThis.localStorage) {
  let open = {};
  try { open = JSON.parse(storage()?.getItem(FOLD_KEY) || "{}") || {}; } catch { open = {}; }
  return {
    isOpen: (name) => open[name] === true,
    set(name, on) {
      open = { ...open, [name]: !!on };
      try { storage()?.setItem(FOLD_KEY, JSON.stringify(open)); } catch { /* kept for this visit only */ }
    },
  };
}

const KEYS = ["C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"];
const MODES = ["Ionian", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Aeolian"];
const list = (names, from = 0) => names.map((name, i) => ({ value: i + from, name }));
const range = (lo, hi, step, fmt = String) => {
  const out = [];
  for (let v = lo; v <= hi; v += step) out.push({ value: v, name: fmt(v) });
  return out;
};
const ROLLED = { value: -1, name: "As rolled", rolled: true };   // the engine's own choice: never a die's pick

// the role a row fills in options.json's instruments
const ROLE = { CHORD: "keys", LEAD: "lead", BASS: "bass", PAD: "pad" };
// INS_KNIGHT, INS_FELT_PIANO: one per song, keys or lead (Sam; the felt never leads over itself, 2026-10-29). (The
// Emilyguitar plays the keys only since the lead styles build.)
// INS_VIBES too (on the keys from 2026-11-05: never the vibes on the keys and the lead in one song)
const ONE_PART = new Set([25, 2, 11]);

// a comping style the keys can play: options.json's keys (only these) and notKeys (never these: the guitars only strum,
// Sam 2026-11-04)
// (rows.json's fits.comping names the two lists)
const FIT = DATA.fits.comping;
export const compFits = (c, keys) => (!c[FIT.keysOf] || c[FIT.keysOf].includes(keys)) && !c[FIT.notKeysOf]?.includes(keys);

// The Comping row's value: -1 as rolled; a comping rhythm's (options.json's compRhythms, generated from
// patterns/comp-rhythms, every keys rhythm in one list, Sam 2026-11-05): a fixed one's CS_* id (staged as COMP), an
// added one's COMP_PAT_BASE + its table index (the engine's CS_PAT with that rhythm forced, the comp_pat override); or
// another style's id (options.json's comping: strum, the retired, the removed, and CS_PAT alone, an old snapshot's)
export const COMP_PAT_BASE = 100;
export const isCompPat = (v) => Number.isInteger(v) && v >= COMP_PAT_BASE;
// the comping choice a Comping value names: a rhythm's (key: its patterns/comp-rhythms id), a style's, or undefined
export function compOption(opts, v) {
  const x = (opts.compRhythms || []).find((c) => c.value === v);
  return x ? { ...x, id: v, key: x.id } : opts.comping.find((c) => c.id === v);
}

// a progression's note without its source ("lofi-hip-hop.md 2, ii9-V13 vamp" -> "ii9-V13 vamp"); an imported one's file
// ("imported from patterns/progressions/sam-vamp" -> "sam-vamp")
const progName = (src) => src.startsWith("imported from ") ? src.replace(/^.*\//, "")
  : src.replace(/^[^:,]*[:,]\s*(\d+,\s*)?/, "").replace(/;.*$/, "");

// Every option a row can take: { value, name, status, family }. Not yet filtered for retired ones or legality.
function allOptions(r, state, opts, lib) {
  const v = state.values;
  switch (r.source) {
    case "keys": return list(KEYS);
    case "modes": return list(MODES).filter((o) => !state.legal?.mode || state.legal.mode.includes(o.value));
    case "tempo": return range(70, 90, 1, (t) => `${t} BPM`);
    case "swing": return range(520, 620, 1, (s) => `${(s / 10).toFixed(1)}%`);
    case "moods": return list(["Sad", "Wistful", "Warm", "Sunny"]);
    case "motion": return range(0, 100, 5);
    case "maps": return (lib?.maps || []).map((m) => ({ value: m.id, name: m.name }));
    case "progs":
      return (lib?.progs || []).filter((p) => !state.legal?.prog || state.legal.prog.includes(p.id))
        .map((p) => ({ value: p.id, name: `#${p.id} ${progName(p.src)}`, status: p.status, key: String(p.id) }));
    case "colors": return list(["7ths", "9ths", "Rich (9/11/13)"]);
    case "families": return ["acoustic", "synth"].map((f) => ({ value: f, name: opts.families[f].name }));
    case "instruments":
      return opts.instruments.filter((i) => i.roles.includes(ROLE[r.role]))
        .map((i) => ({ value: i.id, name: (r.role === "BASS" && i.short) || i.name, status: i.status, family: i.family }));
    case "comping": {   // every rhythm (on the keys its data names), then strum and the retired styles. CS_PAT alone
                        // (an old snapshot's "a drawn comp rhythm") is never offered: shown only as the current value;
                        // a removed style (light eighths) never
      const rhythms = (opts.compRhythms || []).map((c) => compOption(opts, c.value));
      return [ROLLED, ...[...rhythms, ...opts.comping].filter((c) => c.status !== "removed" && compFits(c, v.keys))
        .map((c) => ({ value: c.id, name: c.name, status: c.status, ...(c.sym === "CS_PAT" ? { hidden: true } : {}),
                       ...(c.key ? { key: c.key } : {}) }))];
    }
    case "leadStyles":   // 2 + leads.h's LS_*; the retired engines 0 and 1 behind "show retired"
      return [ROLLED, ...opts.leadStyles.map((s) => ({ value: s.id, name: s.name, status: s.status }))];
    case "registers": return [{ value: -1, name: "Down an octave" }, { value: 0, name: "As rolled", rolled: true },
                              { value: 1, name: "Up an octave" }];
    case "leadLoop": return [{ ...ROLLED, name: "Default" }, ...list(["Off", "On"])];   // -1 follows the firmware's constant
    case "padOn": return [ROLLED, ...list(["Off", "On"])];
    case "onOffInverted": return [{ value: 0, name: "On" }, { value: 1, name: "Off" }];   // the engine's flag turns it off
    case "padPlaces": return [ROLLED, ...list(["Below the lead", "With the lead", "Above the lead"])];
    case "rides": return [ROLLED, ...list(["None", "Throughout", "One section"])];
    case "kits": return opts.kits.map((k) => ({ value: k.id, name: k.name, status: k.status, family: k.family,
                                                unavailable: !!state.unavailable?.has(k.id) }));
    case "patterns": return opts.patterns.map((p) => ({ value: p.id, name: p.name, status: p.status, key: String(p.id) }));
    case "strumPats":   // a table index (the override forces it)
      return [ROLLED, ...(opts[r.source] || []).map((p) => ({ value: p.index, name: p.name, status: p.status, key: p.id }))];
    case "strumFills":   // the fill bars' figure (the song's draw by default; the ballad keeps its own fill)
      return [{ ...ROLLED, name: "Default" }, ...list(["Step back", "Drive"])];
    case "kickPhrases": return [{ value: "", name: "The pattern's own" },
                                ...opts.kickPhrases.map((k) => ({ value: k.id, name: kickName(k), status: k.status ?? "approved",
                                                                  key: k.id })),
                                ...(v.kick === "grid" ? [{ value: "grid", name: "Drawn in the grid", rolled: true }] : [])];
    case "dust": return list(["None", "Light", "Medium", "Heavy"]);
    case "noise": return list(["None", "Vinyl", "Tape"]);
    case "ambience": return list(["None", "Rain"]);
    default: return [];
  }
}

const same = (a, b) => String(a) === String(b);

// The dropdown's options: retired ones only with showRetired (marked), the current value always (marked if retired
// or otherwise not offered). A reroll row offers "as rolled" and the current variant.
export function optionsFor(r, state, opts, { showRetired = false, lib } = {}) {
  const cur = state.values[r.id];
  if (r.kind === "seed") return [{ value: cur, name: String(cur) }];
  if (r.kind === "reroll")
    return [{ value: 0, name: "As rolled" }, ...(cur ? [{ value: cur, name: `Variant ${cur}` }] : [])];
  const all = allOptions(r, state, opts, lib);
  const out = all.filter((o) => (showRetired || o.status !== "retired") && !o.hidden);
  if (cur !== undefined && !out.some((o) => same(o.value, cur))) {
    const o = all.find((x) => same(x.value, cur)) ?? (r.source === "comping" ? compOption(opts, cur) : null);
    out.push(o ? { ...o, value: o.value ?? o.id, current: true } : { value: cur, name: String(cur), current: true });
  }
  return out;
}

// the song's family (it chooses the kit: Sam, 2026-10-29, the synth/acoustic divide applies to drums only): a new
// song's from its keys, else its lead (electric goes with either), else acoustic
export function familyOf(values, opts) {
  const fam = (id) => opts.instruments.find((i) => i.id === id)?.family;
  for (const f of [fam(values.keys), fam(values.lead)]) if (f === "acoustic" || f === "synth") return f;
  return "acoustic";
}

// an option fits a family: its own, electric, or none ("any")
const fits = (o, family) => !o.family || o.family === family || o.family === "electric" || o.family === "any";

// approved options of a row, other than the current value (and a kit whose samples are missing); a kit's of the
// family (the instruments mix freely across families)
function approvedFits(r, state, opts, family, lib) {
  return allOptions(r, state, opts, lib)
    .filter((o) => (o.status ?? "approved") === "approved" && !o.rolled && !o.unavailable && !o.hidden &&
                   !same(o.value, state.values[r.id]))
    .filter((o) => r.source !== "kits" || fits(o, family));
}

const choose = (xs, rnd) => (xs.length ? xs[Math.floor(rnd() * xs.length) % xs.length].value : null);

// The dice for a row the page rolls itself (family, role, ov, drum): a random approved option (a kit of the song's
// family; for the family, the other one), never the current value. null when none exists.
export function pickFor(r, state, opts, rnd = Math.random, lib) {
  const family = state.values.family ?? familyOf(state.values, opts);
  let xs = approvedFits(r, state, opts, family, lib);
  if (r.source === "kits") {   // the family's own first, electric when it has none
    const own = xs.filter((o) => o.family === family);
    if (own.length) xs = own;
  }
  return choose(xs, rnd);
}

// What setting row r to value forces elsewhere: { forced: [{ row, to }], stuck: [{ row, why }], blocked } where
// blocked ({ row, label, why }) names a locked row the value would have to move. stuck: a row that should follow
// but has no approved option yet (it keeps its value).
export function conflicts(r, value, state, opts, rnd = Math.random, lib) {
  const v = state.values, locks = new Set(state.locks || []);
  const out = { forced: [], stuck: [], blocked: null };
  const force = (id, to, why) => {
    if (locks.has(id)) { out.blocked ??= { row: id, label: row(id).label, why }; return; }
    if (to === null) out.stuck.push({ row: id, why });
    else out.forced.push({ row: id, to });
  };
  const optOf = (id, x) => allOptions(row(id), state, opts, lib).find((o) => same(o.value, x));
  if (r.kind === "family") {
    const next = { ...state, values: { ...v, family: value } };
    for (const id of opts.families.follow) {
      const cur = optOf(id, v[id]);
      if (cur && !fits(cur, value)) {
        const xs = approvedFits(row(id), next, opts, value, lib), own = xs.filter((o) => o.family === value);
        force(id, choose(own.length ? own : xs, rnd), `${row(id).label} is ${cur.name} (${cur.family}); the song goes ${value}`);
      }
    }
  } else if (r.id === "keys") {   // (a comp rhythm too: the percussive keys only)
    const comp = compOption(opts, v.comp);
    if (comp && !compFits(comp, value))
      force("comp", -1, `${comp.name} comping doesn't suit the new comp instrument`);
    if (ONE_PART.has(value) && v.lead === value) {
      const next = { ...state, values: { ...v, keys: value } };
      force("lead", pickFor(row("lead"), next, opts, rnd, lib), `the ${optOf("lead", value).name} plays comp or lead, not both`);
    }
  } else if (r.id === "lead") {
    if (ONE_PART.has(value) && v.keys === value) {
      const next = { ...state, values: { ...v, lead: value } };
      force("keys", pickFor(row("keys"), next, opts, rnd, lib), `the ${optOf("keys", value).name} plays comp or lead, not both`);
    }
  }
  return out;
}

// an override row's "as rolled" value (the registers', the switches' and the re-rolls' 0, the others' -1)
export const rolled = (r) => (r.source === "registers" || r.source === "onOffInverted" ||
                               r.kind === "reroll" ? 0 : -1);

// The page's values from wb_state() (C: wb_consts()); the family is the page's, so it isn't here. A shorter ov list
// (an older engine's state) leaves its newer overrides as rolled.
export function fromEngine(s, C, opts) {
  const v = { seed: s.seed };
  const mask = (steps) => steps.reduce((m, i) => m | (1 << i), 0);
  for (const r of ROWS) {
    if (r.kind === "roll") v[r.id] = s.roll[C.RF[r.field]];
    else if (r.kind === "role") v[r.id] = s.roles[C.V[r.role]];
    else if (r.kind === "ov" || r.kind === "reroll") v[r.id] = s.ov[C.OV[r.key]] ?? rolled(r);
  }
  const pat = s.ov[C.OV.COMP_PAT] ?? -1;   // a comp rhythm forced: the Comping row's value names it
  if (pat >= 0) v.comp = COMP_PAT_BASE + pat;
  v.kit = s.kit;
  v.pattern = s.pattern;
  // the kick lane: the pattern's own (""), a kick phrase, or drawn in the grid ("grid": no phrase, or ghosts)
  const [a, b, k] = s.kick;
  v.kick = !k ? "" : (k === 1 && opts.kickPhrases.find((p) => mask(p.a) === a && mask(p.b) === b)?.id) || "grid";
  return v;
}

// the pattern family (patterns/model.json's) each row's options come from: a candidate there can be approved
const FAMILY = { patterns: "drums", progs: "progressions", strumPats: "strum", kickPhrases: "kick-phrases" };

// The row's current value when it is a candidate pattern: { family, id, name } for POST /approve (patterns/status.json),
// else null
export function approval(r, state, opts, lib) {
  if (r.id === "comp") {   // a comping rhythm (patterns/comp-rhythms)
    const o = compOption(opts, state.values.comp);
    return o?.key && o.status === "candidate" ? { family: "comp-rhythms", id: o.key, name: o.name } : null;
  }
  if (!FAMILY[r.source]) return null;
  const o = allOptions(r, state, opts, lib).find((x) => same(x.value, state.values[r.id]));
  return o?.status === "candidate" ? { family: FAMILY[r.source], id: o.key, name: o.name } : null;
}

// why a row can't be heard now (greyed, with this as its tooltip), or null
export function inactive(r, state, opts) {
  const style = r.id === "leadStyle" && opts.leadStyles?.find((s) => s.id === state.values.leadStyle);
  if (style?.status === "retired")   // (picked under "show retired": the engine plays it as rolled; restore says the same)
    return `Lead style: ${style.name} is retired; the song plays a rolled style instead.`;
  if (r.id === "strumPat" && state.strums === false)
    return "The comp doesn't strum in this song: a strum pattern plays with strummed comping (the guitars).";
  if (r.id === "comp") {   // a rhythm left on a comp instrument its data doesn't name (a snapshot's): it holds
    const c = compOption(opts, state.values.comp);
    if (c?.key && !compFits(c, state.values.keys)) {
      const name = (k) => opts.instruments.find((i) => i.id === k)?.name ?? `instrument ${k}`;
      return `This comp instrument doesn't play ${c.name}: ` +
             (c.keys ? `it plays on the ${c.keys.map(name).join(", ")} only.` : "the guitars only strum.");
    }
  }
  if (r.id === "strumFill" && state.strums === false)
    return "The comp doesn't strum in this song: a strum fill plays in a strummed song's drum-fill bars.";
  if (r.id === "ride") {
    const kit = opts.kits.find((k) => k.id === state.values.kit);
    if (!kit?.ride) return `${kit ? kit.name : "This kit"} has no ride: the ride section plays on the sampled kits only.`;
  }
  return null;
}

// A part's values in one line, for its section header ("Rhodes · Tresillo · mid register"): its instrument (the drums:
// kit and pattern), its rhythm or style and its register or place. A value left as rolled is named where the page
// knows what the engine plays (the comping style: state.comp), else left out.
export function summary(part, state, opts) {
  const v = state.values, ins = (id) => opts.instruments.find((i) => i.id === id)?.name;
  const reg = (o) => (o < 0 ? "low register" : o > 0 ? "high register" : "mid register");
  const out = [];
  if (part === "drums") {
    const kit = opts.kits.find((k) => k.id === v.kit);
    out.push(kit?.name, state.grid ? "drawn pattern" : opts.patterns.find((p) => p.id === v.pattern)?.name);
    if (v.kick && v.kick !== "grid") out.push(`kick ${v.kick}`);
    if (kit?.ride) out.push(["no ride", "ride throughout", "ride in one section"][v.ride]);
  } else if (part === "bass") {
    out.push(opts.instruments.find((i) => i.id === v.bass)?.short ?? ins(v.bass));
  } else if (part === "keys") {
    out.push(ins(v.keys));
    const style = compOption(opts, v.comp >= 0 ? v.comp : state.comp);   // (as rolled: the style the engine applies)
    const pat = style?.sym === "CS_STRUM" && v.strumPat >= 0 ? opts.strumPats?.find((p) => p.index === v.strumPat) : null;
    out.push(pat ? `strum ${pat.name}` : style?.sym === "CS_PAT" && !isCompPat(style.id) ? "a comp rhythm" : style?.name);
    out.push(reg(v.keysOct));
  } else if (part === "pad") {
    if (state.padless || v.padOn === 0) return v.padOn === 0 ? `${ins(v.pad)} · off` : "no pad in this song";
    out.push(ins(v.pad), ["below the lead", "with the lead", "above the lead"][v.padPlace]);
  } else if (part === "lead") {
    out.push(ins(v.lead), opts.leadStyles.find((s) => s.id === v.leadStyle)?.name, reg(v.leadOct));
  }
  return out.filter(Boolean).join(" · ");
}

// a kick phrase's shown name: "Kick 2" for k2 (Sam, 2026-11-06: "k2" read like a knob, K1-K4)
export const kickName = (k) => k.name ?? `Kick ${String(k.id).replace(/^k/i, "")}`;

// a kick phrase's step masks (bars A and B); [0, 0] for the pattern's own
export function kickMasks(id, opts) {
  const k = opts.kickPhrases.find((x) => x.id === id);
  const mask = (steps) => steps.reduce((m, i) => m | (1 << i), 0);
  return k ? [mask(k.a), mask(k.b)] : [0, 0];
}
