// SPDX-License-Identifier: GPL-3.0-or-later
// What an "as rolled" row plays, by row (the device mock writes "AS ROLLED (HELD)"): from wb_rolled (the engine's
// own choices, x), the page's state and its options. Every row with an "as rolled" choice must name it here or be
// in NO_NAME with its reason: smoke.mjs checks all of them, so a new row can't slip by unnamed.
import { compOption, rhythmRow, COMP_PAT_BASE } from "../rows.js";

const NOTE = ["C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"];
const note = (m) => `${NOTE[m % 12]}${Math.floor(m / 12) - 1}`;   // MIDI 60 = C4

// re-rolls with nothing to name: a fresh draw of the same choices (which notes, which hits), not a kind of thing;
// the mock writes them ORIGINAL / VARIANT n
export const NO_NAME = {
  voicings: "a re-draw of the comp's voicings",
  padNotes: "a re-draw of the pad's notes",
  phrase: "a re-draw of the lead's phrase",
  drumsVar: "a re-draw of the drums' variation",
};

export function rolledNames(x, state, opts) {
  if (!x) return {};
  const up = (s) => (s == null ? undefined : String(s).toUpperCase());
  const comp = up(compOption(opts, x.comp_pat >= 0 ? COMP_PAT_BASE + x.comp_pat : x.comp)?.name);
  const strum = up(opts.strumPats?.find((p) => p.index === x.strum_pat)?.name);
  return {
    comp, strumPat: strum, rhythm: rhythmRow(state, opts) === "comp" ? comp : strum,
    leadStyle: up(opts.leadStyles?.find((s) => s.id === x.lead_style + 2)?.name),
    padOn: ["OFF", "ON"][x.pad_on],
    padPlace: ["BELOW THE LEAD", "WITH THE LEAD", "ABOVE THE LEAD"][x.pad_place],
    strumFill: ["STEP BACK", "DRIVE"][x.strum_fill],
    leadLoop: ["OFF", "ON"][x.lead_loop],
    bassLine: ["ROOTS", "WALKING", "PEDAL"][x.bass_style],
    ride: ["NONE", "THROUGHOUT", "ONE SECTION"][x.ride],
    keysOct: x.keys_high === undefined ? undefined : x.keys_high ? "HIGH" : "UNDER THE LEAD",
    keysWith: ["LIFT OVER IT", "WITH THE LEAD"][x.keys_with],
    leadOct: x.lead_lo > 0 && x.lead_hi > 0 ? `${note(x.lead_lo)}-${note(x.lead_hi)}` : undefined,
  };
}
