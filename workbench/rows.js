// SPDX-License-Identifier: GPL-3.0-or-later
// The main page's decision rows (layout A): what each row is, the options its dropdown offers, its dice, and what a
// new value forces elsewhere. Pure: no DOM, no engine (desk.js drives the engine; smoke.mjs tests this directly).
//   opts: options.json; state: { values: {row id: value}, locks: Set of row ids, legal: {prog, mode} (optional),
//   unavailable: Set of kit ids whose samples are missing (optional: offered marked, never chosen) };
//   lib: wb_library()'s maps and progressions.

const SONG = "Song", INS = "Instruments", VP = "Voicing and phrase", DRUMS = "Drums", TEX = "Texture";

// kind: seed | roll (a roll field) | family (the page's own) | role (an instrument in the custom set) | ov (an override)
// | reroll (an override's salt) | drum (kit, pattern, kick phrase). field: RF_*; role: V_*; key: WB_OV_*.
export const ROWS = [
  { id: "seed", group: SONG, label: "Seed", kind: "seed", regenerates: true },
  { id: "key", group: SONG, label: "Key", kind: "roll", field: "KEY", source: "keys" },
  { id: "mode", group: SONG, label: "Mode", kind: "roll", field: "MODE", source: "modes" },
  { id: "tempo", group: SONG, label: "Tempo", kind: "roll", field: "TEMPO", source: "tempo" },
  { id: "swing", group: SONG, label: "Swing", kind: "roll", field: "SWING", source: "swing" },
  { id: "mood", group: SONG, label: "Mood", kind: "roll", field: "MOOD", source: "moods" },
  { id: "motion", group: SONG, label: "Motion", kind: "roll", field: "MOTION", source: "motion" },
  { id: "map", group: SONG, label: "Map", kind: "roll", field: "MAP", source: "maps", regenerates: true },
  { id: "prog", group: SONG, label: "Progression", kind: "roll", field: "PROG", source: "progs" },
  { id: "color", group: SONG, label: "Chord color", kind: "roll", field: "COLOR", source: "colors" },
  { id: "family", group: INS, label: "Family", kind: "family", source: "families" },
  { id: "keys", group: INS, label: "Keys", kind: "role", role: "CHORD", source: "instruments" },
  { id: "comp", group: INS, label: "Comping", kind: "ov", key: "COMP", source: "comping" },
  { id: "lead", group: INS, label: "Lead", kind: "role", role: "LEAD", source: "instruments" },
  { id: "bass", group: INS, label: "Bass", kind: "role", role: "BASS", source: "instruments" },
  { id: "pad", group: INS, label: "Pad", kind: "role", role: "PAD", source: "instruments" },
  { id: "padOn", group: INS, label: "Pad on/off", kind: "ov", key: "PAD_ON", source: "padOn" },
  { id: "phrase", group: VP, label: "Lead phrase", kind: "reroll", key: "PHRASE_SALT" },
  { id: "leadStyle", group: VP, label: "Lead style", kind: "ov", key: "LEAD_STYLE", source: "leadStyles" },
  { id: "leadOct", group: VP, label: "Lead register", kind: "ov", key: "LEAD_OCT", source: "registers" },
  { id: "voicings", group: VP, label: "Keys voicings", kind: "reroll", key: "VOICING_SALT" },
  { id: "keysOct", group: VP, label: "Keys register", kind: "ov", key: "KEYS_OCT", source: "registers" },
  { id: "padPlace", group: VP, label: "Pad place", kind: "ov", key: "PAD_PLACE", source: "padPlaces" },
  { id: "kit", group: DRUMS, label: "Kit", kind: "drum", key: "kit", source: "kits" },
  { id: "pattern", group: DRUMS, label: "Pattern", kind: "drum", key: "pattern", source: "patterns" },
  { id: "kick", group: DRUMS, label: "Kick phrase", kind: "drum", key: "kick", source: "kickPhrases" },
  { id: "ride", group: DRUMS, label: "Ride section", kind: "ov", key: "RIDE", source: "rides" },
  { id: "dust", group: TEX, label: "Dust", kind: "roll", field: "DUST", source: "dust" },
  { id: "noise", group: TEX, label: "Noise", kind: "roll", field: "NOISE", source: "noise" },
  { id: "ambience", group: TEX, label: "Ambience", kind: "roll", field: "AMBIENCE", source: "ambience" },
];
export const GROUPS = [SONG, INS, VP, DRUMS, TEX];
export const row = (id) => ROWS.find((r) => r.id === id);

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
const SAMPLED_ONE = new Set([25, 26]);  // INS_KNIGHT, INS_EMILY: one per song, keys or lead (Sam)

// a progression's note without its source ("lofi-hip-hop.md 2, ii9-V13 vamp" -> "ii9-V13 vamp")
const progName = (src) => src.replace(/^[^:,]*[:,]\s*(\d+,\s*)?/, "").replace(/;.*$/, "");

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
        .map((p) => ({ value: p.id, name: `#${p.id} ${progName(p.src)}` }));
    case "colors": return list(["7ths", "9ths", "Rich (9/11/13)"]);
    case "families": return ["acoustic", "synth"].map((f) => ({ value: f, name: opts.families[f].name }));
    case "instruments":
      return opts.instruments.filter((i) => i.roles.includes(ROLE[r.role]))
        .map((i) => ({ value: i.id, name: i.name, status: i.status, family: i.family }));
    case "comping":
      return [ROLLED, ...opts.comping.filter((c) => !c.keys || c.keys.includes(v.keys))
        .map((c) => ({ value: c.id, name: c.name, status: c.status }))];
    case "leadStyles": return [ROLLED, { value: 0, name: "Phrase" }, { value: 1, name: "Hybrid" }];
    case "registers": return [{ value: -1, name: "Down an octave" }, { value: 0, name: "As rolled", rolled: true },
                              { value: 1, name: "Up an octave" }];
    case "padOn": return [ROLLED, ...list(["Off", "On"])];
    case "padPlaces": return [ROLLED, ...list(["Below the lead", "With the lead", "Above the lead"])];
    case "rides": return [ROLLED, ...list(["None", "Throughout", "One section"])];
    case "kits": return opts.kits.map((k) => ({ value: k.id, name: k.name, status: k.status, family: k.family,
                                                unavailable: !!state.unavailable?.has(k.id) }));
    case "patterns": return opts.patterns.map((p) => ({ value: p.id, name: p.name, status: p.status }));
    case "kickPhrases": return [{ value: "", name: "The pattern's own" },
                                ...opts.kickPhrases.map((k) => ({ value: k.id, name: k.id })),
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
  const out = all.filter((o) => showRetired || o.status !== "retired");
  if (cur !== undefined && !out.some((o) => same(o.value, cur))) {
    const o = all.find((x) => same(x.value, cur));
    out.push(o ? { ...o, current: true } : { value: cur, name: String(cur), current: true });
  }
  return out;
}

// the song's family: its keys', else its lead's (electric goes with either), else acoustic
export function familyOf(values, opts) {
  const fam = (id) => opts.instruments.find((i) => i.id === id)?.family;
  for (const f of [fam(values.keys), fam(values.lead)]) if (f === "acoustic" || f === "synth") return f;
  return "acoustic";
}

// an option fits a family: its own, electric, or none ("any")
const fits = (o, family) => !o.family || o.family === family || o.family === "electric" || o.family === "any";

// approved options of a row that fit the family, other than the current value (and a kit whose samples are missing)
function approvedFits(r, state, opts, family, lib) {
  return allOptions(r, state, opts, lib)
    .filter((o) => (o.status ?? "approved") === "approved" && !o.rolled && !o.unavailable && !same(o.value, state.values[r.id]))
    .filter((o) => r.kind === "family" || fits(o, family));
}

const choose = (xs, rnd) => (xs.length ? xs[Math.floor(rnd() * xs.length) % xs.length].value : null);

// The dice for a row the page rolls itself (family, role, ov, drum): a random approved option of the song's family
// (or, for the family, the other one), never the current value. null when none exists.
export function pickFor(r, state, opts, rnd = Math.random, lib) {
  const family = state.values.family ?? familyOf(state.values, opts);
  let xs = approvedFits(r, state, opts, family, lib);
  if (r.source === "instruments" || r.source === "kits") {   // the family's own first, electric when it has none
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
  } else if (r.id === "keys") {
    const comp = opts.comping.find((c) => c.id === v.comp);
    if (comp?.keys && !comp.keys.includes(value))
      force("comp", -1, `${comp.name} comping doesn't suit the new keys`);
    if (SAMPLED_ONE.has(value) && v.lead === value) {
      const next = { ...state, values: { ...v, keys: value } };
      force("lead", pickFor(row("lead"), next, opts, rnd, lib), `the ${optOf("lead", value).name} plays keys or lead, not both`);
    }
  } else if (r.id === "lead") {
    if (SAMPLED_ONE.has(value) && v.keys === value) {
      const next = { ...state, values: { ...v, lead: value } };
      force("keys", pickFor(row("keys"), next, opts, rnd, lib), `the ${optOf("keys", value).name} plays keys or lead, not both`);
    }
  }
  return out;
}

// an override row's "as rolled" value (the registers' and the re-rolls' 0, the others' -1)
export const rolled = (r) => (r.source === "registers" || r.kind === "reroll" ? 0 : -1);

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
  v.kit = s.kit;
  v.pattern = s.pattern;
  // the kick lane: the pattern's own (""), a kick phrase, or drawn in the grid ("grid": no phrase, or ghosts)
  const [a, b, k] = s.kick;
  v.kick = !k ? "" : (k === 1 && opts.kickPhrases.find((p) => mask(p.a) === a && mask(p.b) === b)?.id) || "grid";
  return v;
}

// why a row can't be heard now (greyed, with this as its tooltip), or null
export function inactive(r, state, opts) {
  if (r.id === "ride") {
    const kit = opts.kits.find((k) => k.id === state.values.kit);
    if (!kit?.ride) return `${kit ? kit.name : "This kit"} has no ride: the ride section plays on the sampled kits only.`;
  }
  return null;
}

// a kick phrase's step masks (bars A and B); [0, 0] for the pattern's own
export function kickMasks(id, opts) {
  const k = opts.kickPhrases.find((x) => x.id === id);
  const mask = (steps) => steps.reduce((m, i) => m | (1 << i), 0);
  return k ? [mask(k.a), mask(k.b)] : [0, 0];
}
