/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The seed library tables. Plain C on purpose: curate by editing entries; never reuse an id. The progressions and
   drum patterns are generated (patterns_gen.c, from patterns/). */
#include <string.h>
#include "library.h"

/* Roles are meant to sit in separate registers (curation intent). {id, name, chords, bass, lead, candy, pad, kit, skit, flavor, moods, src}.
   The pad layer: the warm synth pad under the synth sets (18, 19), strings under the rest; a song may draw the
   choir over either (lib_song_pad). candy: none where it was the vox pad, the kalimba, the music box or the bells
   (removed, 2026-10-31).
   skit: the set's sampled kit; a song plays it or the synth kit, about half each (lib_song_kit). By mood and flavor:
   the dusty Rhodes and Knight sets warm or dark, the quiet acoustic sets gentle, city pop clean, the synth and
   guitar-led sets rusty. KIT_N on the 808 and Brushes sets: no sampled equivalent, the synth kit always.
   Ids 3, 8 and 24 (the Wurli's) and 13, 20, 25 and 33 (the DX electric piano's) retired with their keys (Sam,
   2026-11-04: the Wurli dumped; the DX not told from the Rhodes in a mix): never reused. */
const lib_set_t LIB_SETS[] = {
    {1, "Rhodes dusty", INS_RHODES, INS_ELEC_BASS, INS_FLUTE, INS_NONE, INS_STRINGS, KIT_DUSTY, KIT_S_WARM, FL_LOFI, MOOD_WISTFUL | MOOD_WARM, "task brief: the classic dusty set"},
    {2, "Bossa nylon", INS_NYLON, INS_SUB, INS_VIBES, INS_NONE, INS_STRINGS, KIT_BRUSHES, KIT_N, FL_BOSSA, MOOD_WARM | MOOD_SUNNY, "task brief: bossa set"},
    {4, "Felt and vibes", INS_FELT_PIANO, INS_ELEC_BASS, INS_VIBES, INS_NONE, INS_STRINGS, KIT_DUSTY, KIT_S_DARK, FL_LOFI, MOOD_SAD | MOOD_WISTFUL, "task brief: felt piano with muted trumpet; the vibes lead (the trumpet went, Sam, 2026-10-29)"},
    {5, "Chillwave pad", INS_PAD, INS_SUB, INS_FELT_PIANO, INS_NONE, INS_STRINGS, KIT_808, KIT_N, FL_CHILLWAVE, MOOD_WARM | MOOD_SUNNY, "task brief: chillwave set; the felt piano leads (lead styles build: Sam, 2026-10-29)"},
    {6, "City pop Rhodes", INS_RHODES, INS_ELEC_BASS, INS_VIBES, INS_NONE, INS_STRINGS, KIT_SOFT, KIT_S_CLEAN, FL_CITYPOP, MOOD_WARM | MOOD_SUNNY, "task brief: bright Rhodes city pop set; the vibes lead (was the guitar lick: Sam, 2026-10-29)"},
    {7, "Felt and flute", INS_FELT_PIANO, INS_SUB, INS_FLUTE, INS_NONE, INS_STRINGS, KIT_SOFT, KIT_S_GENTLE, FL_LOFI, MOOD_SAD | MOOD_WISTFUL, "brainstorm: quiet felt piano lofi"},
    {9, "Nylon and flute", INS_NYLON, INS_ELEC_BASS, INS_FLUTE, INS_NONE, INS_STRINGS, KIT_BRUSHES, KIT_N, FL_BOSSA, MOOD_WISTFUL | MOOD_WARM, "brainstorm: late-night bossa; the flute leads (was the sax: Sam, 2026-10-29; a sustained lead, so the nylon still strums)"},
    {10, "Pad and flute", INS_PAD, INS_ELEC_BASS, INS_FLUTE, INS_NONE, INS_STRINGS, KIT_808, KIT_N, FL_CHILLWAVE, MOOD_SAD | MOOD_WISTFUL, "brainstorm: hazy chillwave; the vibes lead (was the guitar lick: Sam, 2026-10-29); the synth pluck leads (vibes rebalance: Sam, 2026-10-29); the flute leads (lead styles build: Sam, 2026-10-29)"},
    {11, "Rhodes neo-soul", INS_RHODES, INS_ELEC_BASS, INS_FELT_PIANO, INS_VIBES, INS_STRINGS, KIT_SOFT, KIT_S_WARM, FL_NEOSOUL, MOOD_WARM | MOOD_SUNNY, "brainstorm: smooth neo-soul; the felt piano leads (was the muted trumpet: Sam, 2026-10-29)"},
    /* the synth sets (synths plan Task 4; Sam's cut, 2026-10-24: only the DX EP, the FM slap bass, the triangle bass,
       the organ, the saw lead and the synth pluck kept; the DX EP retired 2026-11-04). The ear candy acoustic or none.
       Ids 12, 14-16 retired (the Juno, chip, organ and sine, Rhodes and Moog sets), 13 and 20 with the DX. The lead
       styles build (Sam, 2026-10-29): the saw lead and the synth pluck lead no set (one of the four lead instruments
       in their place), and the synth/acoustic divide is the drums' only, so no mood needs an all-synth set. */
    {18, "Organ and felt", INS_ORGAN, INS_CHIP_BASS, INS_FELT_PIANO, INS_NONE, INS_WARM_PAD, KIT_SOFT, KIT_S_RUSTY, FL_NEOSOUL, MOOD_SAD | MOOD_WISTFUL, "Sam's cut: an organ over the triangle bass under a saw lead; the felt piano leads (lead styles build: Sam, 2026-10-29)"},
    {19, "Rhodes, FM bass and felt", INS_RHODES, INS_FM_BASS, INS_FELT_PIANO, INS_NONE, INS_WARM_PAD, KIT_DUSTY, KIT_S_DARK, FL_LOFI, MOOD_SAD | MOOD_WISTFUL, "Sam's cut: a dusty Rhodes, the FM slap bass, a saw lead (was Rhodes and Moog); the felt piano leads (lead styles build: Sam, 2026-10-29)"},
    {17, "Nylon, sub and flute", INS_NYLON, INS_SUB, INS_FLUTE, INS_NONE, INS_STRINGS, KIT_BRUSHES, KIT_N, FL_BOSSA, MOOD_WISTFUL | MOOD_SUNNY, "synths spec: a nylon guitar under a synth pluck; the flute leads (lead styles build: Sam, 2026-10-29)"},
    /* the sampled sets (sampled instruments spec, Task 5): the Knight upright on the keys or the lead, never both in
       one set (Sam: the one acoustic piano is the keys or the lead in a song); the Emilyguitar on the keys only since
       the lead styles build (Sam, 2026-10-29: its sets 28 and 29 lead with the Knight and the flute). */
    {21, "Knight and flute", INS_KNIGHT, INS_ELEC_BASS, INS_FLUTE, INS_NONE, INS_STRINGS, KIT_DUSTY, KIT_S_DARK, FL_LOFI, MOOD_SAD | MOOD_WISTFUL, "sampled spec: the Knight's keys under the flute, dusty lofi"},
    {22, "Knight and vibes", INS_KNIGHT, INS_ELEC_BASS, INS_VIBES, INS_VIBES, INS_STRINGS, KIT_SOFT, KIT_S_WARM, FL_NEOSOUL, MOOD_WISTFUL | MOOD_WARM, "sampled spec: the Knight's keys under the sax, neo-soul; the felt piano leads (Sam, 2026-10-29); the leads evened out (Sam, 2026-11-06: each of the four about a quarter of songs; the synth sets keep theirs)"},
    {23, "Rhodes and Knight", INS_RHODES, INS_ELEC_BASS, INS_KNIGHT, INS_NONE, INS_STRINGS, KIT_DUSTY, KIT_S_WARM, FL_LOFI, MOOD_WARM | MOOD_SUNNY, "sampled spec: a Knight lead over the Rhodes"},
    {26, "Emilyguitar and vibes", INS_EMILY, INS_ELEC_BASS, INS_VIBES, INS_NONE, INS_STRINGS, KIT_SOFT, KIT_S_RUSTY, FL_CITYPOP, MOOD_WARM | MOOD_SUNNY, "sampled spec: the Emilyguitar comping (strums, stabs) under the vibes"},
    {27, "Emilyguitar and piano", INS_EMILY, INS_SUB, INS_KNIGHT, INS_NONE, INS_STRINGS, KIT_DUSTY, KIT_S_GENTLE, FL_LOFI, MOOD_SAD | MOOD_WISTFUL, "sampled spec: the Emilyguitar comping under the flute; the leads evened out (Sam, 2026-11-06: each of the four about a quarter of songs; the synth sets keep theirs)"},
    {28, "Rhodes and Knight, neo-soul", INS_RHODES, INS_ELEC_BASS, INS_KNIGHT, INS_VIBES, INS_STRINGS, KIT_SOFT, KIT_S_RUSTY, FL_NEOSOUL, MOOD_WARM | MOOD_SUNNY, "sampled spec: an Emilyguitar lead over the Rhodes; the Knight leads (lead styles build: Sam, 2026-10-29)"},
    {29, "Knight and flute, brushes", INS_KNIGHT, INS_ELEC_BASS, INS_FLUTE, INS_NONE, INS_STRINGS, KIT_BRUSHES, KIT_N, FL_LOFI, MOOD_SAD | MOOD_WISTFUL, "sampled spec: an Emilyguitar lead over the Knight, brushes; the flute leads (lead styles build: Sam, 2026-10-29)"},
    /* the string synth's leads' sets (sampled spec, Task 6), the warm synth pad under them (the pad layer's strings
       would have doubled the lead); the string lead and cello went (Sam, 2026-10-29): the vibes, felt or flute lead */
    {30, "Knight and vibes, warm pad", INS_KNIGHT, INS_ELEC_BASS, INS_VIBES, INS_NONE, INS_WARM_PAD, KIT_SOFT, KIT_S_GENTLE, FL_LOFI, MOOD_WISTFUL | MOOD_WARM, "sampled spec: a high string lead over the Knight (Task 5's deferred set); the vibes lead (Sam, 2026-10-29); the flute leads (vibes rebalance: Sam, 2026-10-29); the leads evened out (Sam, 2026-11-06: each of the four about a quarter of songs; the synth sets keep theirs)"},
    {31, "Rhodes and felt", INS_RHODES, INS_FM_BASS, INS_FELT_PIANO, INS_NONE, INS_WARM_PAD, KIT_DUSTY, KIT_S_DARK, FL_LOFI, MOOD_SAD | MOOD_WISTFUL, "sampled spec: the cello lead inside a dusty Rhodes, the FM slap bass under them; the felt piano leads (Sam, 2026-10-29)"},
    {32, "Knight, vibes and brushes", INS_KNIGHT, INS_SUB, INS_VIBES, INS_NONE, INS_WARM_PAD, KIT_BRUSHES, KIT_N, FL_LOFI, MOOD_SAD | MOOD_WISTFUL, "sampled spec: the cello lead inside the Knight, brushes; the flute leads (Sam, 2026-10-29); the leads evened out (Sam, 2026-11-06: each of the four about a quarter of songs; the synth sets keep theirs)"},
    /* the vibes on the keys (Sam, 2026-11-05: "vibes with the motor on definitely work"), each from a set it names, the
       keys swapped for the vibes and a lead other than the vibes (never both: the one vibraphone is the keys or the
       lead in a song), the thinner moods first (sunny and warm) */
    {34, "Vibes and piano, city pop", INS_VIBES, INS_ELEC_BASS, INS_KNIGHT, INS_NONE, INS_STRINGS, KIT_SOFT, KIT_S_CLEAN, FL_CITYPOP, MOOD_WARM | MOOD_SUNNY, "vibes on the keys: set 6 (City pop Rhodes), the flute leading; the leads evened out (Sam, 2026-11-06: each of the four about a quarter of songs; the synth sets keep theirs)"},
    {35, "Vibes and Knight", INS_VIBES, INS_ELEC_BASS, INS_KNIGHT, INS_NONE, INS_STRINGS, KIT_SOFT, KIT_S_WARM, FL_NEOSOUL, MOOD_WISTFUL | MOOD_WARM, "vibes on the keys: set 22 (Knight and felt), the Knight leading"},
    {36, "Vibes and felt", INS_VIBES, INS_FM_BASS, INS_FELT_PIANO, INS_NONE, INS_STRINGS, KIT_DUSTY, KIT_S_DARK, FL_LOFI, MOOD_SAD | MOOD_WISTFUL, "vibes on the keys: set 31 (Rhodes and felt), the felt leading"},
    {37, "Bossa vibes and piano", INS_VIBES, INS_SUB, INS_KNIGHT, INS_NONE, INS_STRINGS, KIT_BRUSHES, KIT_N, FL_BOSSA, MOOD_WARM | MOOD_SUNNY, "vibes on the keys: set 2 (Bossa nylon), the flute leading; the leads evened out (Sam, 2026-11-06: each of the four about a quarter of songs; the synth sets keep theirs)"},
    /* the jazz guitar leads (Sam approved it, 2026-11-11), a low lead over struck keys the lift raises (never a
       guitar's: they only strum), one set per flavor its styles weigh most ('60s Jazz, Modal Jazz, Jazz-Hop, Bossa
       Nova): about 1 song in 10, two moods each as every set; kits and pads as the sets they follow */
    {38, "Rhodes and jazz guitar", INS_RHODES, INS_ELEC_BASS, INS_JAZZ_GTR, INS_NONE, INS_STRINGS, KIT_SOFT, KIT_S_WARM, FL_NEOSOUL, MOOD_WISTFUL | MOOD_WARM, "Sam approved the jazz guitar (2026-11-11): set 11 (Rhodes neo-soul)'s keys, bass and kits"},
    {39, "Knight and jazz guitar", INS_KNIGHT, INS_ELEC_BASS, INS_JAZZ_GTR, INS_NONE, INS_STRINGS, KIT_DUSTY, KIT_S_DARK, FL_LOFI, MOOD_SAD | MOOD_WISTFUL, "Sam approved the jazz guitar (2026-11-11): set 21 (Knight and flute)'s keys, bass and kits"},
    {40, "Bossa Rhodes and jazz guitar", INS_RHODES, INS_SUB, INS_JAZZ_GTR, INS_NONE, INS_STRINGS, KIT_BRUSHES, KIT_N, FL_BOSSA, MOOD_WARM | MOOD_SUNNY, "Sam approved the jazz guitar (2026-11-11): set 37 (Bossa vibes and piano)'s bass and kit, the Rhodes on the keys"},
    /* the harp leads (Sam approved it, 2026-11-11): gentle and dreamy sets, the felt, the pad and the Rhodes under it;
       the sampled cello (approved the same day): sad and wistful sets, a low lead over struck keys the lift raises
       (the Knight, the Rhodes), the warm pad under it (the strings would double it). Kits and pads as the sets they
       follow */
    {41, "Felt and harp", INS_FELT_PIANO, INS_SUB, INS_HARP, INS_NONE, INS_STRINGS, KIT_SOFT, KIT_S_GENTLE, FL_LOFI, MOOD_SAD | MOOD_WISTFUL, "Sam approved the harp (2026-11-11): set 7 (Felt and flute)'s keys, bass and kits"},
    {42, "Pad and harp", INS_PAD, INS_SUB, INS_HARP, INS_NONE, INS_STRINGS, KIT_808, KIT_N, FL_CHILLWAVE, MOOD_WISTFUL | MOOD_WARM, "Sam approved the harp (2026-11-11): set 5 (Chillwave pad)'s keys, bass and kit"},
    {43, "Rhodes and harp", INS_RHODES, INS_ELEC_BASS, INS_HARP, INS_NONE, INS_STRINGS, KIT_SOFT, KIT_S_CLEAN, FL_CITYPOP, MOOD_WARM | MOOD_SUNNY, "Sam approved the harp (2026-11-11): set 6 (City pop Rhodes)'s keys, bass and kits"},
    {44, "Knight and cello", INS_KNIGHT, INS_ELEC_BASS, INS_SAMP_CELLO, INS_NONE, INS_WARM_PAD, KIT_BRUSHES, KIT_N, FL_LOFI, MOOD_SAD | MOOD_WISTFUL, "Sam approved the sampled cello (2026-11-11): set 32 (Knight, vibes and brushes)'s keys, kit and pad"},
    {45, "Rhodes and cello", INS_RHODES, INS_SUB, INS_SAMP_CELLO, INS_NONE, INS_WARM_PAD, KIT_DUSTY, KIT_S_DARK, FL_LOFI, MOOD_SAD | MOOD_WISTFUL, "Sam approved the sampled cello (2026-11-11): set 31 (Rhodes and felt)'s keys, kits and pad"},
};
const int LIB_NSETS = (int)(sizeof LIB_SETS / sizeof LIB_SETS[0]);

const lib_map_t LIB_MAPS[] = {
    {1, "Classic", 4, EN_TOGETHER, MID_BREAK, END_KEYS_OUTRO, 0, "spec: the five maps; lofi-audio-arrangements.md 4/7"},
    {2, "Slow bloom", 8, EN_SNARE_FIRST, MID_NONE, END_FADE, 0, "spec: the five maps; lofi-audio-arrangements.md 4/7"},
    {3, "Straight in", 0, EN_FROM_TOP, MID_HATS_ONLY, END_RING, 0, "spec: the five maps; lofi-audio-arrangements.md 4/7"},
    {4, "Breather", 4, EN_TOGETHER, MID_KICK_OUT, END_KEYS_OUTRO, 0, "spec: the five maps; lofi-audio-arrangements.md 4/7"},
    {5, "Drift", 4, EN_TOGETHER, MID_NONE, END_FADE, 1, "spec: the five maps; lofi-audio-arrangements.md 4/7"},
};
const int LIB_NMAPS = (int)(sizeof LIB_MAPS / sizeof LIB_MAPS[0]);

/* mode_w: ionian dorian phrygian lydian mixolydian aeolian; color_w: 7, 9, rich. */
const lib_mood_t LIB_MOODS[4] = {
    {{1, 4, 0, 0, 1, 6}, {1, 4, 5}, -2, 20},   /* sad */
    {{1, 5, 0, 0, 2, 5}, {2, 5, 3}, -1, 30},   /* wistful */
    {{5, 2, 0, 1, 5, 1}, {3, 5, 2}, 0, 35},    /* warm */
    {{6, 1, 0, 4, 3, 0}, {3, 5, 2}, 0, 50},    /* sunny */
};

const lib_prog_t *lib_prog_by_id(uint8_t id)
{
    int i;
    for (i = 0; i < LIB_NPROGS; i++) if (LIB_PROGS[i].id == id) return &LIB_PROGS[i];
    return NULL;
}

static const lib_set_t *wb_set;
static uint8_t wb_base;
static const lib_pattern_t *wb_pattern;
void lib_set_wb(const lib_set_t *s, uint8_t base_id) { wb_set = s; wb_base = base_id; }
static int set_key(const lib_set_t *s) { return s == wb_set ? wb_base : s->id; }
void lib_pattern_wb(const lib_pattern_t *p) { wb_pattern = p; }

unsigned lib_lane_hits(const lib_pattern_t *p, int lane, int bar)
{
    return p->two_bar && (bar & 1) ? p->hit_b[lane] : p->hit[lane];
}

unsigned lib_lane_ghosts(const lib_pattern_t *p, int lane, int bar)
{
    return p->two_bar && (bar & 1) ? p->ghost_b[lane] : p->ghost[lane];
}

const lib_set_t *lib_set_by_id(uint8_t id)
{
    int i;
    if (id == LIB_SET_WB) return wb_set;
    for (i = 0; i < LIB_NSETS; i++) if (LIB_SETS[i].id == id) return &LIB_SETS[i];
    return NULL;
}

int lib_kit_sampled(int kit) { return kit >= KIT_S_WARM && kit <= KIT_S_CLEAN; }

const lib_pattern_t *lib_pattern_by_id(uint8_t id)
{
    int i;
    if (id == LIB_PATTERN_WB) return wb_pattern;
    for (i = 0; i < LIB_NPATTERNS; i++) if (LIB_PATTERNS[i].id == id) return &LIB_PATTERNS[i];
    return NULL;
}

const lib_map_t *lib_map_by_id(uint8_t id)
{
    int i;
    for (i = 0; i < LIB_NMAPS; i++) if (LIB_MAPS[i].id == id) return &LIB_MAPS[i];
    return NULL;
}

/* the keys' octave shift for a set: the Chillwave pad an octave lower (Sam, 2026-10-24: "only the chillwave pad needs
   to come down to that lower octave") */
int lib_set_keys_shift(const lib_set_t *s)
{
    return s && set_key(s) == 5 ? -12 : 0;
}

/* the sets whose bass may walk: the ones that had the upright (Sam, 2026-10-24: "I don't like the upright's sound";
   their bass is now the electric; set 8 retired with the Wurli, 2026-11-04) */
int lib_set_walks(const lib_set_t *s)
{
    return s && (set_key(s) == 1 || set_key(s) == 4 || set_key(s) == 9);
}

int lib_lead_drop(int ins)
{
    return ins == INS_JAZZ_GTR ? 12 : ins == INS_SAMP_CELLO ? 24 : 0;
}
int lib_lead_low(int ins) { return lib_lead_drop(ins) > 0; }
int lib_lead_lift(int ins) { return lib_lead_low(ins); }

int lib_ins_sampled_struck(int ins)
{
    return ins == INS_KNIGHT || ins == INS_NYLON || ins == INS_EMILY || ins == INS_JAZZ_GTR;
}

int lib_ins_sampled(int ins) { return lib_ins_sampled_struck(ins) || ins == INS_SAMP_CELLO || ins == INS_HARP; }

static uint8_t samples_on = 1;
void lib_set_samples(int on) { samples_on = on != 0; }
int lib_samples(void) { return samples_on; }
int lib_set_playable(const lib_set_t *s)
{
    return samples_on || (!lib_ins_sampled(s->chords) && !lib_ins_sampled(s->bass) && !lib_ins_sampled(s->lead) &&
                          s->kit < KIT_N);
}

int lib_ins_sustained(int ins)
{
    return ins == INS_PAD || ins == INS_FLUTE || ins == INS_ORGAN || ins == INS_STRINGS || ins == INS_WARM_PAD ||
           ins == INS_CHOIR || ins == INS_SAMP_CELLO;
}

/* The leads Sam removed (2026-10-29: "i only liked the felt piano and vibraphones"; the lead styles build: the
   kalimba, saw lead, synth pluck and Emilyguitar no longer lead, the Emilyguitar stays on the keys): one of the four
   lead instruments in their place, the flute or the vibes (an old snapshot's keys never the vibes, which joined the
   keys 2026-11-05, so never a lead over itself): the
   breathy and plucked ones the flute, the struck and synth ones the vibes; any other instrument is itself */
int lib_lead_approved(int ins)
{
    switch (ins) {
    case INS_MUTED_TPT: case INS_SAX: case INS_CELLO: case INS_KALIMBA: case INS_EMILY: case INS_SYNTH_PLUCK:
        return INS_FLUTE;
    case INS_GUITAR_LICK: case INS_STR_LEAD: case INS_SAW_LEAD: return INS_VIBES;
    default: return ins;
    }
}

/* The upright (retired 2026-10-24, removed 2026-10-31: no patch): the electric bass, which took its sets, in its
   place; any other instrument is itself */
int lib_bass_approved(int ins) { return ins == INS_UPRIGHT ? INS_ELEC_BASS : ins; }

/* The keys Sam retired (2026-11-04, removed with their patches: the Wurli; the DX electric piano, not told from the
   Rhodes in a mix): the Rhodes, the electric piano they stood beside, in their place; any other instrument is itself */
int lib_keys_approved(int ins) { return ins == INS_WURLI || ins == INS_DX_EP ? INS_RHODES : ins; }
