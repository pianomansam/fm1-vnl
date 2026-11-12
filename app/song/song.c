/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The sequencer. One tick = one 16th: the block covering the bar (plan_at), the bar's chord, then groove, harmony
   and the lead for that step, sorted by delay. The sample position of every step is tracked as an integer plus a
   float fraction, advanced by song_step_samples() after each tick, so a caller that accumulates the same way sees
   the same boundary samples. The pad layer (pads.h) is planned a 4-bar group at a time, after the lead's bar.

   Every 4-bar boundary (and the song's end) is a note-off line: a pitched note that would ring past it is clipped
   to end on it. With the library's chords (at most 2 bars each, starting on their bar) and the pedal bass (one chord long),
   that only trims swing and rounding spill, and it means a key or harmony change never overlaps the old key's
   notes, even when the request came after those notes were emitted.

   Requests: one slot per field, landing on the next 4-bar boundary, applied at that boundary's first tick (Mood
   first, then by field). What each field rebuilds:
     RF_KEY      the roll's key (the lead's style plans in it from the boundary on)
     RF_TEMPO    a linear glide of step samples over 2 bars; RF_BARS is kept
     RF_MOTION,  plan_build again; blocks before the boundary stay, the new plan's blocks from it on
     RF_MAP
     RF_MOOD     roll_set (re-picks an illegal mode/progression/set), one roll_dice of the progression toward the
                 mood
   A boundary rebuilds at most one plan (a bounded loop).
     RF_BARS     ignored: the bar count never changes
     the rest    roll_set only (set, pattern, swing, dust, noise, ambience take effect from the boundary) */
#include <stdint.h>

#include <string.h>
#include "song.h"
#include "harmony.h"
#include "override.h"
#include "library.h"
#include "theory.h"
#include "rng.h"
#include "pads.h"
#include "leads/leads.h"

static int loop_bars(const song_roll_t *r);

/* SPIKE (throwaway): bass rhythm (Sam: the tones are good, more rhythm). The groove's tones, in a rhythm cell per
   song: the kick's (with beat 3 sat out now and then), half notes on 1 and 3, or beats 1 and 2. Then anticipation
   (the next chord's note an 8th or 16th early, ringing over the bar line), long and short notes, and ghost notes (a
   soft, muted 16th before a strike). Roots songs only; pedal and walk play as they did. VNL_BASS: 1 a
   cell per song, 2 the kick's, 3 half notes, 4 beats 1 and 2, 5 beats 1, 2 and 4. */
static int pending_any(const song_t *s)
{
    int i;
    for (i = 0; i < RF_N; i++)
        if (s->pend[i]) return 1;
    return 0;
}

static int bass_spike, sb_tied;
static int sb_on[16], sb_len[16], sb_vel[16];              /* the bar's bass onsets: MIDI (0 none), 16ths, velocity */
static uint32_t sb_tie_mask;                               /* onsets that ring past the bar line */
void song_set_bass_spike(int mode) { bass_spike = mode; sb_tied = 0; }
static int pads_double;
void song_set_pads_double(int on) { pads_double = on; }

static uint32_t sb_hash(uint32_t seed, int bar, int lane)
{
    uint32_t h = seed * 0x9E3779B1u ^ (uint32_t)(bar + 1) * 0x85EBCA6Bu ^ (uint32_t)(lane + 1) * 0xC2B2AE35u;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 13; h *= 0x297A2D39u; h ^= h >> 15;
    return h;
}

static int sb_anticipates(const song_t *s, int bar)      /* 0 none, 2 an 8th early, 1 a 16th early, into `bar` */
{
    uint32_t h = sb_hash(s->roll.seed, bar, 3);
    if (h % 100u >= 35u) return 0;                     /* about a third of chord changes */
    return (h >> 8) % 3u ? 2 : 1;
}

static void sb_plan(song_t *s, const plan_block_t *b, int bar, int bpc, const int *pcs, int np)
{
    rng_t g;
    int tone[16], i, cell, legato, ghosts, last = 0, nn, npcs[5];
    uint32_t on = 0;
    const plan_block_t *nb = plan_at(&s->plan, bar + 1);
    rng_seed(&g, s->roll.seed, 150);
    cell = rng_int(&g, 100);
    cell = cell < 34 ? 0 : cell < 56 ? 1 : cell < 78 ? 2 : 3;
    if (bass_spike >= 2 && bass_spike <= 5) cell = bass_spike - 2;
    legato = rng_int(&g, 2);
    ghosts = rng_int(&g, 100) < 60;
    for (i = 0; i < 16; i++) {
        tone[i] = groove_bass_note(&s->roll, b, bar, i, pcs, np);
        if (tone[i]) on |= 1u << i;
        sb_on[i] = 0;
    }
    if (cell == 1) on = 0x0101u;                         /* half notes on 1 and 3 */
    if (cell == 2) on = 0x0011u;                         /* beats 1 and 2 */
    if (cell == 3) on = 0x1011u;                         /* beats 1, 2 and 4 */
    if (cell == 0 && sb_hash(s->roll.seed, bar, 1) % 4u == 0) on &= ~0x0F00u;   /* beat 3 sat out */
    if (sb_tied) on &= ~1u;                             /* the note pushed into this bar still rings */
    nn = (bar + 1) % bpc == 0 && nb && (nb->layers & L_BASS) ? harmony_chord_at(&s->roll, bar + 1,
             (bar + 1) / loop_bars(&s->roll) + 1, 0, npcs) : 0;
    if (nn > 0 && sb_anticipates(s, bar + 1)) {
        int at = 16 - sb_anticipates(s, bar + 1);
        on &= (1u << at) - 1u;
        on |= 1u << at;
        tone[at] = groove_bass_note(&s->roll, nb, bar + 1, 0, npcs, nn);
        if (!tone[at]) on &= ~(1u << at);
    }
    sb_tie_mask = 0;
    for (i = 0; i < 16; i++) {
        int d;
        if (!((on >> i) & 1)) continue;
        if (!tone[i]) tone[i] = last ? last : groove_bass_note(&s->roll, b, bar, 0, pcs, np);
        if (!tone[i]) continue;
        last = tone[i];
        for (d = 1; i + d < 16 && !((on >> (i + d)) & 1); d++)
            ;
        sb_on[i] = tone[i];
        if (i >= 14 && nn > 0 && i == 16 - sb_anticipates(s, bar + 1)) {
            sb_len[i] = d + 4;                           /* rings through the next beat 1 */
            sb_tie_mask |= 1u << i;
        } else {                                        /* beat 1 held; the rest long or short per song */
            sb_len[i] = i == 0 || legato ? d : (d + 1) / 2;
        }
        sb_vel[i] = (i & 3) == 0 ? 84 : 76;
        if (ghosts && i > 1 && !((on >> (i - 1)) & 1) && sb_hash(s->roll.seed, bar, 10 + i) % 100u < 30u) {
            sb_on[i - 1] = tone[i];                      /* a ghost: soft and muted, a 16th before */
            sb_len[i - 1] = -1;
            sb_vel[i - 1] = 34;
        }
    }
    sb_tied = sb_tie_mask != 0;
}

static int spike_bass(song_t *s, const plan_block_t *b, int bar, int step, float gs, const int *pcs, int np,
                      song_ev_t *buf, int n)
{
    const lib_prog_t *pr = lib_prog_by_id((uint8_t)s->roll.v[RF_PROG]);
    int bpc = pr && pr->bars_per_chord ? pr->bars_per_chord : 1, style = groove_bass_style(&s->roll), i, k = 0;
    uint32_t swing = 0;
    if (style != BS_ROOTS) return n;
    for (i = 0; i < n; i++)
        if (buf[i].voice != V_BASS) buf[k++] = buf[i];
    n = k;
    if (!(b->layers & L_BASS) || np <= 0) { sb_tied = 0; return n; }
    if (step == 0) sb_plan(s, b, bar, bpc, pcs, np);
    if (!sb_on[step] || n >= 60) return n;
    if (step & 1) {
        float sw = ((float)s->roll.v[RF_SWING] * 0.001f - 0.5f) * 2.0f * gs;
        swing = sw > 0.0f ? (uint32_t)(sw + 0.5f) : 0u;
    }
    buf[n].voice = V_BASS; buf[n].note = (uint8_t)sb_on[step]; buf[n].vel = (uint8_t)sb_vel[step];
    buf[n].pad = (uint8_t)((sb_tie_mask >> step) & 1);   /* 1: rings past the chord change */
    buf[n].delay = swing;
    buf[n].len = sb_len[step] < 0 ? (uint32_t)(gs * 0.4f) : (uint32_t)((float)sb_len[step] * gs);
    return n + 1;
}

#define BUF 64

static float ss_of(int bpm) { return 44100.0f * 60.0f / ((float)bpm * 4.0f); }

/* step samples of song step idx (bar * 16 + step), following a glide */
static float ss_at(const song_t *s, int idx)
{
    int g = idx - s->glide_t0;
    if (g >= 0 && g < 32) return s->glide_from + (s->glide_to - s->glide_from) * (float)(g + 1) / 32.0f;
    return s->glide_to;
}

static void advance(uint32_t *pos, float *frac, float ss)
{
    float adv = ss + *frac;
    uint32_t ip = (uint32_t)adv;
    *frac = adv - (float)ip;
    *pos += ip;
}

/* the sample position of the next 4-bar boundary (or the end), from the step idx that starts a 4-bar group */
static void find_edge(song_t *s, int idx)
{
    uint32_t pos = s->pos;
    float frac = s->frac;
    int k, end = idx + 64, last = s->roll.v[RF_BARS] * 16;
    if (end > last) end = last;
    for (k = idx; k < end; k++) advance(&pos, &frac, ss_at(s, k));
    s->edge = pos;
}

static int loop_bars(const song_roll_t *r)
{
    const lib_prog_t *p = lib_prog_by_id((uint8_t)r->v[RF_PROG]);
    int n = p ? p->n * p->bars_per_chord : 4;
    return n > 0 ? n : 4;
}

/* ---- the plan from a boundary on ---- */

static void merge_plan(song_t *s, int B)
{
    song_plan_t np, m;
    int i;
    plan_build(&s->roll, &np);
    memset(&m, 0, sizeof m);
    m.bars = s->plan.bars;
    for (i = 0; i < s->plan.n && s->plan.b[i].start < B; i++) {
        plan_block_t b = s->plan.b[i];
        if (b.start + b.bars > B) b.bars = (uint8_t)(B - b.start);
        m.b[m.n++] = b;
    }
    for (i = 0; i < np.n && m.n < 24; i++) {
        plan_block_t b = np.b[i];
        if (b.start + b.bars <= B) continue;
        if (b.start < B) {
            b.bars = (uint8_t)(b.start + b.bars - B);
            b.start = (uint8_t)B;
        }
        m.b[m.n++] = b;
    }
    s->plan = m;
}

/* ---- the ride (groove_ride): the bars whose hats go to it ---- */

static void ride_for(song_t *s, const song_plan_t *own)
{
    int blk, ride = groove_ride(&s->roll, &blk);
    s->ride_lo = s->ride_hi = 0;
    if (ride == RIDE_ALL) {
        s->ride_hi = s->roll.v[RF_BARS];
    } else if (ride == RIDE_SECTION) {
        song_plan_t p;                              /* groove_ride's plan: the roll's own (after a merge, from B on;
                                                       own: already built from the roll, at the start) */
        if (!own) {
            plan_build(&s->roll, &p);
            own = &p;
        }
        s->ride_lo = own->b[blk].start;
        s->ride_hi = (int16_t)(own->b[blk].start + own->b[blk].bars);
    }
}

/* ---- the keys make room for the lead: their window under its band ---- */

/* once per set: worked out at the start, and again where a set request or the workbench's lead register or style
   moves the band (the voicings, cached by their window, follow) */
static void keys_win_for(song_t *s)
{
    const lib_set_t *set = lib_set_by_id((uint8_t)s->roll.v[RF_SET]);
    int lo, hi;
    leads_band_for(&s->roll, s->lead.style - 1, &lo, &hi);
    /* (a low lead's band, an octave or two down, would squeeze the voicer under C4: none, the keys lift over it in
       its blocks; one they don't lift over, the soft synth lead, voiced as under the flute: its band undropped) */
    s->keys_win = set && lib_lead_lift(set->lead) ? 0 : lo - 1 + (set ? lib_lead_drop(set->lead) : 0);
    s->win_oct = song_ov()->lead_oct;
    s->win_style = (int8_t)s->lead.style;
}

/* The style the lead plays: the workbench's forced one, else the song's rolled one, kept (leads_style_keep: rolled
   again only for a set whose lead can't play it), so clearing the override gives it back. A change of style, or of
   lead instrument (fresh: its band moved), starts the style clean: its own state (the st union, Bossa Nova's fields)
   zeroed */
static void style_for(song_t *s, int fresh)
{
    int o = song_ov()->lead_style, st;
    s->lead.rolled = leads_style_keep(&s->roll, s->lead.rolled - 1) + 1;
    st = o >= 2 && o < 2 + LS_N ? o - 1 : s->lead.rolled;
    if (st == s->lead.style && !fresh) return;
    memset(&s->lead.st, 0, sizeof s->lead.st);
    s->lead.anchor = s->lead.akey = s->lead.ctrun = 0;
    s->lead.loop.have = 0;                          /* the lead loop records anew (its counters kept: the song's) */
    s->lead.style = st;
}

/* ---- requests landing at B ---- */

/* 1 where the plan is rebuilt from B (Motion, Map) */
static int apply_one(song_t *s, int f, int B)
{
    song_roll_t before = s->roll;
    int v = s->pend_val[f];
    s->pend[f] = 0;
    switch (f) {
    case RF_KEY:
        roll_set(&s->roll, RF_KEY, v);
        return 0;
    case RF_TEMPO: {
        float to;
        roll_set(&s->roll, RF_TEMPO, v);
        s->roll.v[RF_BARS] = before.v[RF_BARS];                           /* the bar count never changes */
        to = ss_of(s->roll.v[RF_TEMPO]);
        if (to != s->glide_to) {
            s->glide_from = s->glide_to;
            s->glide_to = to;
            s->glide_t0 = B * 16;
        }
        return 0;
    }
    case RF_MOOD:
        roll_set(&s->roll, RF_MOOD, v);
        if (s->roll.v[RF_MOOD] == before.v[RF_MOOD]) return 0;
        roll_dice(&s->roll, RF_PROG, (uint32_t)B);
        return 0;
    case RF_BARS:
        return 0;
    default:
        roll_set(&s->roll, f, v);
        s->roll.v[RF_BARS] = before.v[RF_BARS];
        if (memcmp(&before, &s->roll, sizeof before) == 0) return 0;
        if (f == RF_MOTION)              /* the plan follows; the per-song draws (the keys' comping) read the rolled
                                            motion and stay (Sam, 2026-11-05: "It should stay") */
            s->roll.motion_moved = (int16_t)(s->roll.motion_moved + s->roll.v[RF_MOTION] - before.v[RF_MOTION]);
        return f == RF_MOTION || f == RF_MAP;
    }
}

/* the song's own set, copied from its library set on the first role or kit request (NULL: the set is unknown) */
static lib_set_t *own_set(song_t *s)
{
    if (!(s->own & SONG_OWN_SET)) {
        const lib_set_t *from = lib_set_by_id((uint8_t)s->roll.v[RF_SET]);
        if (!from) return 0;
        s->set = *from;
        if (s->roll.v[RF_SET] != LIB_SET_WB) {
            s->set.pad = (uint8_t)lib_song_pad(&s->roll);   /* the song's pad layer: the set's, or the choir drawn */
            s->set_base = from->id;
        }
        s->set.id = LIB_SET_WB;
        s->roll.v[RF_SET] = LIB_SET_WB;
        s->own |= SONG_OWN_SET;
        lib_set_wb(&s->set, s->set_base);
    }
    return &s->set;
}

/* the song's own overrides and set, where it has them, in force while it ticks */
static void own_install(const song_t *s)
{
    if (s->own & SONG_OWN_OV) song_override(&s->ov);
    if (s->own & SONG_OWN_SET) lib_set_wb(&s->set, s->set_base);
}

/* what song_request_row had in force before the song first made its own (restored when it starts a new song) */
static const song_t *own_by;
static song_ov_t own_ov_before;

static void apply_row(song_t *s, int row, int v)
{
    lib_set_t *set;
    if (row >= SR_OV && row < SR_OV + OV_N) {
        if (!(s->own & SONG_OWN_OV)) {
            if (own_by != s) {
                own_ov_before = *song_ov();
                own_by = s;
            }
            s->ov = *song_ov();
            s->own |= SONG_OWN_OV;
        }
        song_ov_set(&s->ov, row - SR_OV, v);
        song_override(&s->ov);
    } else if (!lib_samples() &&
               (row == SR_KIT ? lib_kit_sampled(v) || SONG_KIT_IS_SAMPLED(v - SONG_KIT_PRESET)
                              : row >= SR_ROLE && row < SR_OV && lib_ins_sampled(v))) {
        /* (no sample bank: a sampled instrument or kit refused) */
    } else if ((set = own_set(s)) != 0) {
        switch (row) {
        case SR_ROLE + V_CHORD: set->chords = (uint8_t)lib_keys_approved(v); break;
        case SR_ROLE + V_BASS: set->bass = (uint8_t)lib_bass_approved(v); break;
        case SR_ROLE + V_LEAD: set->lead = (uint8_t)lib_lead_approved(v); break;
        case SR_ROLE + V_PAD: set->pad = (uint8_t)v; break;
        case SR_KIT:
            s->kit_preset = (int8_t)(v >= SONG_KIT_PRESET ? v - SONG_KIT_PRESET : -1);
            if (SONG_KIT_IS_SAMPLED(s->kit_preset))
                v = KIT_S_RUSTY;          /* (under a shipped kit: a sampled kit's song, the ride section, the hats) */
            else if (s->kit_preset >= 0)
                v = KIT_BRUSHES;          /* (a synth kit's song: no ride section, its compressor) */
            set->kit = lib_kit_sampled(v) ? KIT_N : (uint8_t)v;
            set->skit = lib_kit_sampled(v) ? (uint8_t)v : KIT_N;
            break;
        default: break;
        }
    }
    s->changes++;
}

static void apply_requests(song_t *s, int B)
{
    int f, i, re = 0, set = s->roll.v[RF_SET], motion = s->roll.v[RF_MOTION], map = s->roll.v[RF_MAP], rows = 0;
    if (s->pend[RF_MOOD] && s->pend_bar[RF_MOOD] == B) re |= apply_one(s, RF_MOOD, B);
    for (f = 0; f < RF_N; f++)
        if (s->pend[f] && s->pend_bar[f] == B) re |= apply_one(s, f, B);
    for (i = 0; i < s->rq_n;)                         /* the rows' requests, in the order they came */
        if (s->rq[i].bar == B) {
            apply_row(s, s->rq[i].row, s->rq[i].val);
            s->rq[i] = s->rq[--s->rq_n];
            rows = 1;
        } else
            i++;
    if (rows)
        set = -1;                                     /* an instrument, the kit or an override: as a set change (the
                                                         lead's style and band, the keys' window, the ride) */
    if (re) merge_plan(s, B);
    if (set != s->roll.v[RF_SET] || motion != s->roll.v[RF_MOTION] || map != s->roll.v[RF_MAP]) ride_for(s, 0);
    if (set != s->roll.v[RF_SET]) {                  /* another lead instrument: its style (kept where it plays it), its
                                                       band */
        style_for(s, 1);
        keys_win_for(s);
        harmony_keys_window(0, s->keys_win);
        harmony_keys_plan(&s->plan);
    }
}

/* ---- the pad layer (pads design) ---- */

/* The pad layer's part in plan block i (pads_section_part): 0 where it is silent, as in a song without a pad
   (song_pad_on), a block without it (song_pad_in) or one without the keys */
static int pad_part(const song_t *s, int i)
{
    const lib_set_t *set = lib_set_by_id((uint8_t)s->roll.v[RF_SET]);
    if (i < 0 || i >= s->plan.n || !(s->plan.b[i].layers & L_KEYS) || !song_pad_on(&s->roll) ||
        !song_pad_in(&s->roll, &s->plan, i))
        return 0;
    return pads_section_part(&s->roll, i, s->plan.b[i].section, !set || lib_ins_sustained(set->chords));
}

/* The pad layer for the 4 bars from B, a 4-bar boundary (roll and plan hold until the next, and a block covers the
   whole group): only where the keys play, the block's part (pads_section_part) alone: the chord part at each chord
   change over percussive keys, voice-led from the last, or the solo note in each bar, in its voice's range. The keys' voicings are followed as they will
   strike. The lead's notes are known for bar B only; the chord part's notes are chord tones and the lead keeps to the
   chord-scale, so the later bars' lead never rubs them. */
static void pads_group(song_t *s, const plan_block_t *b, int B)
{
    const lib_prog_t *pr = lib_prog_by_id((uint8_t)s->roll.v[RF_PROG]);
    int bpc = pr && pr->bars_per_chord ? pr->bars_per_chord : 1, k, i, lead[16], nlead = 0, bi = (int)(b - s->plan.b);
    int up = harmony_pad_place(&s->roll) == PL_BELOW ? 0 : 12, keys_on = (b->layers & L_KEYS) != 0;
    int part = pad_part(s, bi);
    const lib_set_t *set = lib_set_by_id((uint8_t)s->roll.v[RF_SET]);
    int vibes = set && set->chords == INS_VIBES;
    int chord_on = part == 1, solo_on = part == 2, lo = pads_solo_lo(pads_solo_voice(&s->roll));
    harmony_voicing_t vs = s->voicing;
    for (i = 0; i < 16; i++)
        if (s->lead.on[i] > 0) lead[nlead++] = s->lead.on[i];
    memset(s->pads.note, 0, sizeof s->pads.note);
    memset(s->pads.fade, part ? PF_ON : PF_SILENT, sizeof s->pads.fade);
    if (!chord_on) memset(s->pads.chord, 0, sizeof s->pads.chord);
    if (part) {   /* the fade over a section line: in over the first bar back after a block without the pad, out over
                     the last bar before one */
        if (B == b->start && bi > 0 && !pad_part(s, bi - 1)) s->pads.fade[0] = PF_IN;
        if (b->start + b->bars - 1 - B < 4 && bi + 1 < s->plan.n && !pad_part(s, bi + 1))
            s->pads.fade[b->start + b->bars - 1 - B] = PF_OUT;
    }
    for (k = 0; keys_on && k < 4 && B + k < s->roll.v[RF_BARS]; k++) {
        int bar = B + k, pass = bar / loop_bars(&s->roll) + 1, pcs[5], np, spcs[5], nsp, keys[4], all[6], top = 0,
            raw[4] = {0, 0, 0, 0};
        np = harmony_chord_at(&s->roll, bar, pass, 0, pcs);
        memcpy(spcs, pcs, sizeof pcs);                   /* the chord part sounds a dominant as the keys do: no 7th */
        nsp = harmony_sound_pcs(spcs, np);
        harmony_voicing_raw_at(&s->roll, bar, pass, &vs, keys, raw);
        harmony_keys_notes_at(&s->roll, bar, pass, &vs, all);   /* (what the pad must not rub: a strum's low strings
                                                                   too) */
        if (bar % bpc == 0) {
            int cs = harmony_style(&s->roll);
            for (i = 0; i < 4; i++)                      /* the keys strike it (the vibes' as voiced: place_keys
                                                            lifts their low notes alone) */
                vs.last[i] = vibes ? raw[i] : keys[i] ? keys[i] - up - harmony_keys_oct(&s->roll) -
                                                      harmony_keys_lift(&s->roll, bar) : 0;
            if (chord_on && pads_double && (cs == CS_HELD || cs == CS_STRUM))   /* a strum sustains as held does:
                                                                                  its guitar shape's top doubled */
                pads_double_keys(keys, s->pads.chord);
            else if (chord_on)
                pads_chord(&s->roll, all, spcs, nsp, (unsigned)pcs[0], lead, k ? 0 : nlead, s->pads.chord,
                           harmony_pad_lift(&s->roll, bar), s->pads.chord);
        }
        for (i = 0; i < 3; i++) {
            s->pads.note[k][i] = s->pads.chord[i];
            if (s->pads.chord[i] > top) top = s->pads.chord[i];
        }
        if (solo_on) {
            int bass[16], nb = 0, st, n;
            for (st = 0; st < 16; st++) {                /* the bar's bass notes, as the groove strikes them */
                int x = groove_bass_note(&s->roll, b, bar, st, pcs, np);
                for (i = 0; i < nb && bass[i] != x; i++) {}
                if (x && i == nb) bass[nb++] = x;
            }
            n = pads_solo(&s->roll, s->roll.v[RF_KEY], s->roll.v[RF_MODE], pcs, np, all, bass, nb, top, lo,
                          s->pads.solo);
            if (n) s->pads.solo = n;
            s->pads.note[k][3] = n;
        }
    }
    /* a strummed song's shapes found here are the ones its bars would find (in the same order, from the same last
       shape): kept, so the voicer never runs twice for a chord */
    memcpy(s->voicing.gtab, vs.gtab, sizeof vs.gtab);
    memcpy(s->voicing.gsig, vs.gsig, sizeof vs.gsig);
    s->voicing.gn = vs.gn;
}

/* The pad layer's events on step 0 of bar: a lane's note strikes where it changes (or the group begins) and lasts
   to the end of its run of bars with the same note (a tie), ending exactly on that bar line */
static int pads_step(const song_t *s, int bar, song_ev_t *out, int max)
{
    int k = bar % 4, l, j, n = 0, dark;
    const plan_block_t *b = plan_at(&s->plan, bar);
    dark = b && (b->flags & PB_DARK);
    for (l = 0; l < 4 && n < max; l++) {
        int note = s->pads.note[k][l], idx;
        uint32_t pos = s->pos;
        float frac = s->frac;
        if (!note || (k > 0 && s->pads.note[k - 1][l] == note)) continue;
        for (j = k + 1; j < 4 && s->pads.note[j][l] == note; j++) {}
        for (idx = bar * 16; idx < (bar - k + j) * 16; idx++) advance(&pos, &frac, ss_at(s, idx));
        out[n].voice = V_PAD;
        out[n].note = (uint8_t)note;
        out[n].vel = (uint8_t)((l < 3 ? 30 : 58) - (dark ? 8 : 0) < 25 ? 25 : (l < 3 ? 30 : 58) - (dark ? 8 : 0));   /* the chord part about 8.5 dB under the keys, down 2 dB with them (30, was 29: +0.3 dB
                                                     where a dominant lost its 7th, a held common tone, 2026-11-04) */
        out[n].pad = (uint8_t)(l + 1);
        out[n].delay = 0;
        out[n].len = pos - s->pos;
        n++;
    }
    return n;
}

/* ---- the public face ---- */

static void stage_voice(song_t *s, int B);
static void stage_lead(song_t *s, int B);
static void stage_pads(song_t *s, int B);
static int stage_off;                    /* (song_start_kit's first start: its song is started again) */
static int stage_later;                  /* song_first_bar_later */

void song_start(song_t *s, const song_roll_t *r)
{
    if (own_by == s) {                               /* a new song: as rolled, as before this song made its own */
        song_override(&own_ov_before);
        own_by = 0;
    }
    memset(s, 0, sizeof *s);
    s->kit_preset = -1;
    s->stg_bar = -1;
    s->roll = *r;
    if (stage_off)
        return;                                      /* (only what own_set and apply_row read: started again) */
    plan_build(&s->roll, &s->plan);
    s->glide_to = s->glide_from = s->ss = ss_of(s->roll.v[RF_TEMPO] > 0 ? s->roll.v[RF_TEMPO] : 80);
    s->glide_t0 = -64;
    s->state = s->roll.v[RF_BARS] > 0 ? SS_PLAYING : SS_ENDED;
    ride_for(s, &s->plan);
    s->lead.rolled = leads_style_roll(&s->roll) + 1;
    s->lead.style = leads_style(&s->roll) + 1;
    keys_win_for(s);
    if (!stage_later) {                              /* the first bar's planning now too (the start's block, not its
                                                        first downbeat's) */
        int part;
        for (part = 0; part < SONG_FIRST_BAR_PARTS; part++)
            song_first_bar(s, part);
    }
}

void song_first_bar_later(int on) { stage_later = on; }

void song_first_bar(song_t *s, int part)
{
    if (s->state != SS_PLAYING || s->bar || s->step)
        return;
    own_install(s);                                  /* (under what its first tick would have in force) */
    style_for(s, 0);
    if (song_ov()->lead_oct != s->win_oct || s->lead.style != s->win_style)
        keys_win_for(s);
    harmony_keys_window(0, s->keys_win);
    harmony_keys_plan(&s->plan);
    if (part == 0) {                                 /* the loop's voicings worked out (stage_voice's first voicing,
                                                        then a lookup: the same, in its cache) */
        int keys[4];
        harmony_voicing_at(&s->roll, 0, 1, &s->voicing, keys);
    } else if (part == 1) {
        stage_voice(s, 0);
    } else if (part == 2) {
        stage_lead(s, 0);
    } else {
        stage_pads(s, 0);
    }
    harmony_keys_window(0, 0);
    harmony_keys_plan(0);
}

void song_start_kit(song_t *s, const song_roll_t *r, int kit) { song_start_rows(s, r, kit, 0, 0, 0); }

void song_start_rows(song_t *s, const song_roll_t *r, int kit, const int16_t *rows, const int16_t *vals, int n)
{
    static lib_set_t set;                /* (the set the plan reads while song_start clears s) */
    static song_ov_t ov, before;
    song_roll_t own;
    int base = 0, preset, i, mine, need_set = kit >= 0;
    for (i = 0; i < n; i++)              /* (overrides alone leave the song its library set: a saved song recalled
                                            plays as it was rolled, its set's id and all) */
        need_set |= rows[i] >= SR_ROLE && rows[i] < SR_OV;
    stage_off = kit >= 0 || n > 0;       /* (started again below: planned then) */
    song_start(s, r);
    stage_off = 0;
    if (kit < 0 && n <= 0)
        return;
    if (need_set && !own_set(s)) {
        song_start(s, r);                /* (no set to make its own: as rolled, planned now) */
        return;
    }
    if (kit >= 0)
        apply_row(s, SR_KIT, kit);
    for (i = 0; i < n; i++)              /* (a row the device refuses as a request: refused here too) */
        if (song_request_row(s, rows[i], vals[i])) {
            s->rq_n = 0;
            apply_row(s, rows[i], vals[i]);
        }
    set = s->set;
    base = s->set_base;
    own = s->roll;
    preset = s->kit_preset;
    mine = s->own & SONG_OWN_OV;
    if (mine) {                          /* the overrides the song made its own: in force while it is planned (not the
                                            ones before it, which song_start would put back) */
        ov = s->ov;
        before = own_ov_before;
        own_by = 0;
        song_override(&ov);
    }
    if (need_set)
        lib_set_wb(&set, base);
    song_start(s, &own);                 /* (as the workbench's: its stage's set and roll, its song from them) */
    if (need_set) {
        s->set = set;
        s->set_base = (uint8_t)base;
        lib_set_wb(&s->set, base);
    }
    s->own = (uint8_t)((need_set ? SONG_OWN_SET : 0) | mine);
    s->kit_preset = (int8_t)preset;
    if (mine) {
        s->ov = ov;
        own_ov_before = before;
        own_by = s;
    }
}

/* what the next bar's planning depends on that a tick between could change: the requests waiting and what has landed
   (the roll, the rows' changes), the keys' window */
uint32_t song_key(const song_t *s)
{
    uint32_t h = 2166136261u ^ s->roll.seed;
    int i;
    for (i = 0; i < RF_N; i++)
        h = (h ^ (uint32_t)(s->pend[i] ? 1 + s->pend_bar[i] * 31 + s->pend_val[i] : 0)) * 16777619u;
    for (i = 0; i < s->rq_n; i++)
        h = (h ^ (uint32_t)(s->rq[i].row * 65599 + s->rq[i].val * 31 + s->rq[i].bar)) * 16777619u;
    h = (h ^ (uint32_t)s->rq_n) * 16777619u;
    for (i = 0; i < RF_N; i++)                       /* (and what landed since: a request is cleared as it lands) */
        h = (h ^ (uint32_t)s->roll.v[i]) * 16777619u;
    h = (h ^ (uint32_t)s->changes) * 16777619u;
    {                                                /* the overrides in force (the workbench may set them itself) */
        const unsigned char *o = (const unsigned char *)song_ov();
        size_t j;
        for (j = 0; j < sizeof(song_ov_t); j++)
            h = (h ^ o[j]) * 16777619u;
    }
    return h;
}
static uint32_t stage_key(const song_t *s) { return (song_key(s) ^ (uint32_t)s->keys_win) * 16777619u; }

/* a request lands on bar B (its planning waits for the downbeat, after it) */
static int lands_on(const song_t *s, int B)
{
    int i;
    for (i = 0; i < RF_N; i++)
        if (s->pend[i] && s->pend_bar[i] == B) return 1;
    for (i = 0; i < s->rq_n; i++)
        if (s->rq[i].bar == B) return 1;
    return 0;
}

/* bar B's voicings, from the states now, into the stage (the downbeat's own work, ahead) */
static void stage_voice(song_t *s, int B)
{
    const plan_block_t *b = plan_at(&s->plan, B);
    int pass;
    s->stg_bar = -1;
    if (!b || B >= s->roll.v[RF_BARS] || (B % 4 == 0 && lands_on(s, B)))
        return;
    pass = B / loop_bars(&s->roll) + 1;
    s->stg_np = (int8_t)harmony_chord_at(&s->roll, B, pass, (b->flags & PB_DARK) != 0, s->stg_pcs);
    s->stg_v0 = s->voicing;
    s->stg_v = s->voicing;
    harmony_voicing_at(&s->roll, B, pass, &s->stg_v, s->stg_keys);
    harmony_voicing_at(&s->roll, B + 1, (B + 1) / loop_bars(&s->roll) + 1, &s->stg_v, s->stg_nkeys);
    s->stg_key = stage_key(s);
    s->stg_bar = (int16_t)B;
    s->stg_stage = 0;
}

/* then the lead's plan of bar B, on those voicings' keys */
static void stage_lead(song_t *s, int B)
{
    const plan_block_t *b = plan_at(&s->plan, B);
    int i, k;
    if (s->stg_bar != B || s->stg_stage != 0 || !b || s->stg_key != stage_key(s)) {
        s->stg_bar = -1;
        return;
    }
    for (i = 0, k = B + 1 < s->plan.bars; i < RF_N; i++)
        if (s->pend[i] && s->pend_bar[i] == B + 1) k = 0;
    s->stg_l0 = s->lead;
    s->stg_l = s->lead;
    lead_plan_bar(&s->stg_l, &s->roll, b, B, s->stg_keys, s->stg_nkeys, s->stg_pcs, s->stg_np, k);
    s->stg_stage = 1;
}

/* the pad group bar B begins, on the stage's voicings and lead (pads_group reads them, and the voicings' gn moves) */
static void stage_pads(song_t *s, int B)
{
    harmony_voicing_t v = s->voicing;
    lead_state_t l = s->lead;
    pads_state_t p = s->pads;
    const plan_block_t *b = plan_at(&s->plan, B);
    if (s->stg_bar != B || s->stg_stage != 1 || !b)
        return;
    s->voicing = s->stg_v;
    s->lead = s->stg_l;
    pads_group(s, b, B);
    s->stg_v = s->voicing;
    s->stg_p = s->pads;
    s->voicing = v;
    s->lead = l;
    s->pads = p;
    s->stg_stage = 2;
}

/* the stage for bar B is what the downbeat would make now: made from the same states, under the same requests */
static int staged(const song_t *s, int B)
{
    return s->stg_bar == B && s->stg_stage >= 1 && (B % 4 != 0 || s->stg_stage == 2) && s->stg_key == stage_key(s) &&
           !memcmp(&s->voicing, &s->stg_v0, sizeof s->voicing) && !memcmp(&s->lead, &s->stg_l0, sizeof s->lead);
}

int song_tick(song_t *s, song_ev_t *out, int max)
{
    song_ev_t buf[BUF];
    const plan_block_t *b;
    int bar, step, idx, n = 0, i, j, k;
    uint32_t salt;
    float gs;
    if (!s || s->state != SS_PLAYING) return 0;
    own_install(s);                                    /* this song's own overrides and set (song_request_row) */
    style_for(s, 0);                                   /* the workbench may force another style */
    if (song_ov()->lead_oct != s->win_oct || s->lead.style != s->win_style)
        keys_win_for(s);                            /* the workbench moved the lead's band */
    harmony_keys_window(0, s->keys_win);           /* this song's, while it ticks (cleared below) */
    harmony_keys_plan(&s->plan);                   /* (the lift over a low lead: its lead blocks) */
    bar = s->bar;
    step = s->step;
    idx = bar * 16 + step;
    if (step == 0 && bar % 4 == 0) {
        for (i = 0; i < RF_N; i++)                  /* a request lands: a push into this bar was cut at the line */
            if (s->pend[i] && s->pend_bar[i] == bar) s->lead.tied = 0;
        apply_requests(s, bar);
        find_edge(s, idx);
    }
    s->ss = ss_at(s, idx);
    /* the generators size held notes from one step's samples; during a speed-up glide later steps are shorter, so
       they get the glide's target and held notes end early rather than late */
    gs = s->ss < s->glide_to ? s->ss : s->glide_to;
    salt = s->roll.seed;
    b = plan_at(&s->plan, bar);
    if (b) {
        int pcs[5], np, pass = bar / loop_bars(&s->roll) + 1, dark = (b->flags & PB_DARK) != 0;
        np = harmony_chord_at(&s->roll, bar, pass, dark, pcs);
        if (step == 0 && staged(s, bar)) {          /* planned ahead (stage_plan, stage_pads): the same, taken */
            s->voicing = s->stg_v;
            s->lead = s->stg_l;
            if (bar % 4 == 0) s->pads = s->stg_p;
        } else if (step == 0) {                     /* the lead plans its bar first: the comping makes room for it */
            int keys[4], nkeys[4];
            harmony_voicing_at(&s->roll, bar, pass, &s->voicing, keys);
            harmony_voicing_at(&s->roll, bar + 1, (bar + 1) / loop_bars(&s->roll) + 1, &s->voicing, nkeys);
            for (i = 0, k = bar + 1 < s->plan.bars; i < RF_N; i++)   /* no push past the end, or into a change */
                if (s->pend[i] && s->pend_bar[i] == bar + 1) k = 0;
            lead_plan_bar(&s->lead, &s->roll, b, bar, keys, nkeys, pcs, np, k);
            if (bar % 4 == 0) pads_group(s, b, bar);
        }
        k = n;
        n += groove_step(&s->roll, b, bar, step, gs, pcs, np, salt, buf + n, BUF - n);
        if (bar >= s->ride_lo && bar < s->ride_hi) {    /* a ride block: the hats to the ride on the beat (quarter
                                                           notes at most), open hats its accents */
            for (i = j = k; i < n; i++) {
                if (buf[i].voice == V_HAT || buf[i].voice == V_OHAT) {
                    if (step % 4) continue;
                    if (buf[i].voice == V_OHAT) buf[i].vel = (uint8_t)(buf[i].vel + 14 > 110 ? 110 : buf[i].vel + 14);
                    buf[i].voice = V_RIDE;
                }
                buf[j++] = buf[i];
            }
            n = j;
        }
        if (roll_motion(&s->roll) < 25) {   /* the calmest songs (Sam, 2026-11-05): no hats, the ride kept */
            for (i = j = k; i < n; i++)
                if (buf[i].voice != V_HAT && buf[i].voice != V_OHAT) buf[j++] = buf[i];
            n = j;
        }
        n += harmony_step(&s->roll, b, bar, step, pass, gs, &s->voicing,
                          (s->lead.onsets >= 4 ? HS_BUSY : 0) | (s->lead.active ? HS_LEAD : 0), salt, buf + n,
                          BUF - n);
        n += lead_step(&s->lead, step, gs, buf + n, BUF - n);
        if (step == 0) n += pads_step(s, bar, buf + n, BUF - n);
        {   /* the layer's level this step (song_pad_level) */
            int f = s->pads.fade[bar % 4];
            float up = (float)(step + 1) * (1.0f / 16.0f);
            s->pad_level = f == PF_SILENT ? 0.0f : f == PF_IN ? up : f == PF_OUT ? 1.0f - up : 1.0f;
        }
        if (bass_spike)
            n = spike_bass(s, b, bar, step, gs, pcs, np, buf, n);
    }
    {   /* a bass note (a swung one runs a little long) ends by the next chord change: the lead keeps clear of
           the chord it plays over, not of the one before */
        const lib_prog_t *pr = lib_prog_by_id((uint8_t)s->roll.v[RF_PROG]);
        int bpc = pr && pr->bars_per_chord ? pr->bars_per_chord : 1;
        if ((bar + 1) % bpc == 0) {
            float left = (float)(16 - step) * gs;   /* gs: never longer than the steps to come */
            uint32_t line = s->pos + (left > 1.0f ? (uint32_t)left - 1u : 0u);
            for (i = 0; i < n; i++)
                if (buf[i].voice == V_BASS && !buf[i].pad && s->pos + buf[i].delay + buf[i].len > line)
                    buf[i].len = line > s->pos + buf[i].delay + 1u ? line - s->pos - buf[i].delay : 1u;
        }
    }
    for (i = k = 0; i < n; i++) {                   /* the 4-bar note-off line */
        uint32_t at = s->pos + buf[i].delay;
        if (buf[i].voice <= V_PAD && buf[i].len) {     /* the pitched roles */
            if (at >= s->edge) continue;            /* would start past the line: dropped, not clicked */
            if (at + buf[i].len > s->edge &&
                !((buf[i].voice == V_BASS || buf[i].voice == V_LEAD) && buf[i].pad && !pending_any(s)))
                buf[i].len = s->edge - at;          /* a push (lead, or the bass spike) rings on unless a change lands */
        }
        buf[k++] = buf[i];
    }
    n = k;
    for (i = 1; i < n; i++)                         /* stable sort by delay */
        for (j = i; j > 0 && buf[j].delay < buf[j - 1].delay; j--) {
            song_ev_t t = buf[j];
            buf[j] = buf[j - 1];
            buf[j - 1] = t;
        }
    if (n > max) n = max < 0 ? 0 : max;
    if (n) memcpy(out, buf, sizeof(song_ev_t) * (size_t)n);
    if (step == 0)
        s->stg_bar = -1;
    else if (step == 12)                            /* the next bar's planning ahead, a part a 16th: its voicings, the */
        stage_voice(s, bar + 1);                    /* lead's plan, the pad group (each a tick of its own) */
    else if (step == 13)
        stage_lead(s, bar + 1);
    else if (step == 14 && (bar + 1) % 4 == 0)
        stage_pads(s, bar + 1);
    advance(&s->pos, &s->frac, s->ss);
    if (++s->step == 16) {
        s->step = 0;
        if (++s->bar >= s->roll.v[RF_BARS]) s->state = SS_ENDED;
    }
    harmony_keys_window(0, 0);
    harmony_keys_plan(0);
    return n;
}

float song_step_samples(const song_t *s) { return s->ss; }

float song_pad_level(const song_t *s) { return s->pad_level; }

float song_lead_lp_at(const song_t *s, int bar, int st)
{
    if (!s || bar < 0 || bar >= s->roll.v[RF_BARS]) return 0.0f;   /* the song's last tick has ended it: still played */
    return leads_loop_lp(&s->roll, &s->lead, plan_at(&s->plan, bar), bar, st,
                         s->lead.style ? s->lead.style - 1 : leads_style(&s->roll));
}

void song_request(song_t *s, int field, int value)
{
    int B;
    if (!s || s->state != SS_PLAYING || field < 0 || field >= RF_N || field == RF_BARS) return;
    B = (s->step == 0 && s->bar % 4 == 0) ? s->bar : (s->bar / 4 + 1) * 4;
    if (B >= s->roll.v[RF_BARS]) return;            /* the song ends first */
    s->pend[field] = 1;
    s->pend_val[field] = (int16_t)value;
    s->pend_bar[field] = (int16_t)B;
}

int song_request_row(song_t *s, int row, int value)
{
    int B, i;
    if (!s || s->state != SS_PLAYING) return 0;
    if (row >= SR_ROLL && row < SR_ROLL + RF_N) {
        if (row == RF_BARS) return 0;
        B = (s->step == 0 && s->bar % 4 == 0) ? s->bar : (s->bar / 4 + 1) * 4;
        if (B >= s->roll.v[RF_BARS]) return 0;
        song_request(s, row, value);
        return 1;
    }
    if (!(row >= SR_OV && row < SR_OV + OV_N) && row != SR_ROLE + V_CHORD && row != SR_ROLE + V_BASS &&
        row != SR_ROLE + V_LEAD && row != SR_ROLE + V_PAD && row != SR_KIT)
        return 0;
    if (row == SR_KIT && (value < 0 || (value >= KIT_N && (value < SONG_KIT_PRESET ||
                                                         value >= SONG_KIT_PRESET + SONG_KIT_PRESETS)))) return 0;
    if (row >= SR_ROLE && row < SR_OV && (value < 0 || value > 255)) return 0;
    B = (s->step == 0 && s->bar % 4 == 0) ? s->bar : (s->bar / 4 + 1) * 4;
    if (B >= s->roll.v[RF_BARS]) return 0;            /* the song ends first */
    for (i = 0; i < s->rq_n && s->rq[i].row != row; i++) {}
    if (i == s->rq_n) {
        if (s->rq_n == SONG_ROW_REQS) return 0;
        s->rq_n++;
    }
    s->rq[i].row = (int16_t)row;
    s->rq[i].val = (int16_t)value;
    s->rq[i].bar = (int16_t)B;
    return 1;
}

void song_carry_requests(song_t *s, const song_t *later)
{
    int f;
    if (!s || !later) return;
    for (f = 0; f < later->rq_n; f++) {               /* the rows' too (one a row) */
        int i;
        for (i = 0; i < s->rq_n && s->rq[i].row != later->rq[f].row; i++) {}
        if (i == s->rq_n) {
            if (s->rq_n == SONG_ROW_REQS) continue;
            s->rq_n++;
        }
        s->rq[i] = later->rq[f];
    }
    for (f = 0; f < RF_N; f++)
        if (later->pend[f]) {
            s->pend[f] = 1;
            s->pend_val[f] = later->pend_val[f];
            s->pend_bar[f] = later->pend_bar[f];
        }
}

int song_pending(const song_t *s, int *field, int *bars_left)
{
    int f, best = -1;
    if (!s || s->state != SS_PLAYING) return 0;
    for (f = 0; f < RF_N; f++)
        if (s->pend[f] && (best < 0 || s->pend_bar[f] < s->pend_bar[best])) best = f;
    if (best < 0) return 0;
    if (field) *field = best;
    if (bars_left) *bars_left = s->pend_bar[best] - s->bar;
    return 1;
}
