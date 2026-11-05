/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdint.h>
#include <string.h>
#include "harmony.h"
#include "rng.h"
#include "theory.h"
#include "library.h"
#include "strum.h"
#include "override.h"
#include "patterns_gen.h"

static uint32_t mix(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 1) * 0x85EBCA6Bu ^ (c + 1) * 0xC2B2AE35u ^ (d + 1) * 0x27D4EB2Fu;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
    return h;
}

uint32_t harmony_mix(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { return mix(a, b, c, d); }

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static const lib_prog_t *prog_of(const song_roll_t *r)
{
    const lib_prog_t *p = lib_prog_by_id((uint8_t)r->v[RF_PROG]);
    return p ? p : &LIB_PROGS[0];
}

/* the chord of this bar after any reharmonization; color is the song color (before dark). Reharmonizing only
 * colors one chord of a later pass richer: no substitutions (Sam, after songs.wav: a tritone sub and a borrowed iv
 * both sounded foreign to the key). */
static th_chord_spec_t spec_at(const song_roll_t *r, const lib_prog_t *p, int bar, int pass, int *color)
{
    int idx = (bar / p->bars_per_chord) % p->n;
    th_chord_spec_t c = p->c[idx];
    rng_t g;
    int chance;
    *color = r->v[RF_COLOR];
    if (pass < 2 || *color >= COLOR_RICH) return c;
    rng_seed(&g, r->seed, 120);
    chance = 25 + rng_int(&g, 26);
    if ((int)(mix(r->seed, (uint32_t)pass, 1, 120) % 100u) >= chance) return c;
    if ((int)(mix(r->seed, (uint32_t)pass, 2, 120) % (uint32_t)p->n) == idx)
        (*color)++;                                      /* one chord of the pass, a richer color */
    return c;
}

static int has_iv(const int *pcs, int n, int iv)
{
    int i;
    for (i = 1; i < n; i++)
        if ((pcs[i] - pcs[0] + 12) % 12 == iv)
            return 1;
    return 0;
}

static int drop_iv(int *pcs, int n, int iv)
{
    int i, m = 0;
    for (i = 0; i < n; i++)
        if (i == 0 || (pcs[i] - pcs[0] + 12) % 12 != iv)
            pcs[m++] = pcs[i];
    return m;
}

/* Dominants (major 3rd, minor 7th) never sound bare (Sam: a plain Eb7 sounded bad): at least a 9th, a 13th in a
 * rich song; th_voice keeps 3rd, 7th and the extension and drops the root (the bass has it). Where the mode gives no
 * extension (each would clash), the chord drops its 7th: a plain major triad. The keys and the pad then sound it
 * without its 7th anyway (harmony_sound_pcs, Sam 2026-11-04); the lead and the bass still read it here. */
static int dominant_colors(const song_roll_t *r, th_chord_spec_t c, int color, int *pcs)
{
    int key = r->v[RF_KEY], mode = r->v[RF_MODE], n, k, ext = 0;
    if (color < COLOR_9) color = COLOR_9;
    n = th_chord_pcs(key, mode, c, color, pcs);
    if (n == 4 && color < COLOR_RICH)                    /* the 9th was a clash: try the 13th */
        n = th_chord_pcs(key, mode, c, COLOR_RICH, pcs);
    for (k = 1; k < n; k++) {
        int iv = (pcs[k] - pcs[0] + 12) % 12;
        ext |= iv != 4 && iv != 7 && iv != 10;
    }
    if (ext)
        return n;                                        /* th_voice drops the root first, never the extension */
    n = th_chord_pcs(key, mode, c, COLOR_7, pcs);
    return drop_iv(pcs, n, 10);
}

int harmony_sound_pcs(int *pcs, int n)
{
    return th_dom_b7(pcs, n) >= 0 ? drop_iv(pcs, n, 10) : n;
}

/* the chord as the keys voice it: a dominant without its 7th */
static int keys_chord_at(const song_roll_t *r, int bar, int pass, int dark, int *pcs)
{
    return harmony_sound_pcs(pcs, harmony_chord_at(r, bar, pass, dark, pcs));
}

int harmony_chord_at(const song_roll_t *r, int bar, int pass, int dark, int *pcs)
{
    int color, n;
    th_chord_spec_t c = spec_at(r, prog_of(r), bar < 0 ? 0 : bar, pass, &color);
    (void)dark;                          /* dark is a sound (filter, softer), not a different chord (Sam) */
    n = th_chord_pcs(r->v[RF_KEY], r->v[RF_MODE], c, COLOR_7, pcs);
    if (has_iv(pcs, n, 4) && has_iv(pcs, n, 10))
        return dominant_colors(r, c, color, pcs);
    return th_chord_pcs(r->v[RF_KEY], r->v[RF_MODE], c, color, pcs);
}

static int is_guitar(int ins) { return ins == INS_NYLON || ins == INS_EMILY; }

#if COMP_NPAT
/* CS_PAT's weight in the keys' draw (beside held 44 and half notes 36; the Knight's 35, 25, 25) once a comp rhythm is
   approved, on the keys that play it: a first guess, about 1 song in 5 of those keys, the approved rhythms equally
   (Sam sets it by ear) */
#define COMP_PAT_WEIGHT 20
/* the approved comp rhythms (COMP_PAT): the roll draws CS_PAT only once there is one */
static int comp_pat_napproved(void)
{
    int i, n = 0;
    for (i = 0; i < COMP_NPAT; i++) n += COMP_PAT_APPROVED(i);
    return n;
}
#endif

/* per song (Sam, after hearing them side by side): the synth keys' weights held 44, arpeggio 20, half notes 36, so as
   heard held about 55%, half notes 25%, arpeggio 20% (half notes hold over sustained keys and a kick on 1 and 3 alone:
   half_held); offbeats retired (Sam: the "and" 8ths liked "less and less"), stabs retired on the synth keys ("just
   not working") and back on the Knight (below). The guitars only strum, under any lead (Sam, 2026-11-04: "since we
   have a strumming engine drop the comping capabilities for guitars"): no draw. */
static int style_draw(int ins, uint32_t seed)
{
    /* held, stabs, offbeats (retired: 0 everywhere), arpeggio, half notes, strum (the guitars' alone). No arpeggio on
       any keys for now (Sam, 2026-10-28: set 23's Rhodes arpeggio "sounds very synthetic"; it may come back later).
       Was SYNTH arpeggio 20, KNIGHT 15. Held on 2 15 and held on 2 and 4 10 (Sam, 2026-11-05: approved; first-guess
       weights); light eighths 0 (not approved). */
    static const uint8_t SYNTH[CS_N] = {44, 0, 0, 0, 36, 0, 15, 0, 10, 0},
                         KNIGHT[CS_N] = {35, 25, 0, 0, 25, 0, 15, 0, 10, 0};
    uint8_t w[CS_N];
    int k;
    rng_t g;
    if (is_guitar(ins)) return CS_STRUM;
    rng_seed(&g, seed, 121);
    memcpy(w, ins == INS_KNIGHT ? KNIGHT : SYNTH, sizeof w);
    for (k = CS_HELD2; k < CS_N; k++)     /* held on 2 (and 4), a comp rhythm: only on the keys that play them (the
                                             percussive keys and the vibes), the others' draw as before */
        if (!harmony_comp_ok(ins, k)) w[k] = 0;
#if COMP_NPAT
    if (comp_pat_napproved() && harmony_comp_ok(ins, CS_PAT))   /* an approved comp rhythm: the keys that play it
                                                                   draw one at COMP_PAT_WEIGHT */
        w[CS_PAT] = COMP_PAT_WEIGHT;
#endif
    return rng_weighted(&g, w, CS_N);
}

int harmony_style_ins(int ins, uint32_t seed) { return style_draw(ins, seed); }

int harmony_comp_ok(int ins, int cs)
{
    if (is_guitar(ins)) return cs == CS_STRUM;          /* the guitars only strum */
    if (ins == INS_VIBES)                               /* the vibes on the keys ring: held, half notes, held on 2
                                                           (and 4), a comp rhythm (Sam, 2026-11-05: "enable vibes on
                                                           the new comp rhythm"); no stabs, strum or light eighths */
        return cs == CS_HELD || cs == CS_HALF || cs == CS_HELD2 || cs == CS_HELD24 || (COMP_NPAT && cs == CS_PAT);
    if (cs == CS_PAT) return COMP_NPAT && harmony_comp_ok(ins, CS_HELD2);   /* a comp rhythm: where held on 2 plays */
    if (cs != CS_HELD2 && cs != CS_LIGHT8 && cs != CS_HELD24) return 1;
    return ins == INS_RHODES || ins == INS_FELT_PIANO || ins == INS_KNIGHT;
}

int harmony_style(const song_roll_t *r)
{
    const lib_set_t *s = lib_set_by_id((uint8_t)r->v[RF_SET]);
    int ins = s ? s->chords : INS_NONE;
#if COMP_NPAT
    if (song_ov()->comp_pat > 0 && harmony_comp_ok(ins, CS_PAT))   /* the workbench's forced comp rhythm */
        return CS_PAT;
#endif
    if (song_ov()->comp >= 0)             /* the workbench's (or a host render's): where the keys can't play it, held
                                             (the guitars strum) */
        return harmony_comp_ok(ins, song_ov()->comp) ? song_ov()->comp : is_guitar(ins) ? CS_STRUM : CS_HELD;
    return style_draw(ins, r->seed);
}

/* how far voicing b is from a, as the voicer weighs it: the lower voices' moves, the top's 8 a semitone (and 6 for a
   leap), 6 for each common tone that moved */
static int vl_cost(const int *a, const int *b)
{
    int c = 0, v, w, d = b[3] > a[3] ? b[3] - a[3] : a[3] - b[3];
    for (v = 0; v < 3; v++)
        c += b[v] > a[v] ? b[v] - a[v] : a[v] - b[v];
    c += 8 * d + (d > 2 ? 6 : 0);
    for (v = 0; v < 4; v++) {
        int in = 0, at = 0;
        for (w = 0; w < 4; w++) {
            in |= (b[w] - a[v] + 120) % 12 == 0;
            at |= b[w] == a[v];
        }
        if (in && !at) c += 6;
    }
    return c;
}

/* The loop's voicings as a cycle: voice the loop pass after pass (each chord from the one before, the first from the
   last, so the wrap is voiced too) until it repeats, and keep the pass that voice-leads most smoothly all the way
   around. Rootless throughout (Sam: keys-only sections too). top, bottom: the first chord's reference spread (0: as
   rolled); hi: the voicings' top at most (C5, or the keys' window). Returns 1 when out differs from cmp (NULL: 0). */
static int loop_cycle(const song_roll_t *r, int top, int bottom, int hi, int out[4][4], int cmp[4][4])
{
    enum { PASSES = 8 };
    const lib_prog_t *p = prog_of(r);
    int pcs[4][5], np[4], v[PASSES][4][4], i, k, best = 1 << 30, pick = 0, nch = p->n < 4 ? p->n : 4;
    const int *prev = NULL;
    for (i = 0; i < nch; i++)
        np[i] = keys_chord_at(r, i * p->bars_per_chord, 1, 0, pcs[i]);
    for (k = 0; k < PASSES; k++)
        for (i = 0; i < nch; i++) {
            if (!prev && top)
                th_voice_spread(pcs[i], np[i], v[k][i], hi, 1, top, bottom);
            else
                th_voice_range(pcs[i], np[i], prev, v[k][i], hi, 1);
            prev = v[k][i];
        }
    for (k = 1; k < PASSES; k++) {                       /* (pass 0 began from nothing) */
        int c = 0;
        for (i = 0; i < nch; i++)
            c += vl_cost(v[k][(i + nch - 1) % nch], v[k][i]);
        if (c < best) { best = c; pick = k; }
    }
    memcpy(out, v[pick], sizeof(int) * 4 * (size_t)nch);
    return cmp && memcmp(cmp, v[pick], sizeof(int) * 4 * (size_t)nch) != 0;
}

/* The workbench's voicing re-roll (voicing_salt): the start shape near another reference spread (top D4-Bb4, bottom
   C3-Bb3; F4 and G3 as rolled), the first of up to 8 the salt draws whose cycle differs from the rolled one (none:
   as rolled). Every voicing rule holds: only the first chord's pull toward the spread moves. */
static int voice_hi(const song_roll_t *r);
static int strum_hi(const song_roll_t *r);

static void loop_voicings(const song_roll_t *r, harmony_voicing_t *vs)
{
    const lib_prog_t *p = prog_of(r);
    int rolled[4][4], v[4][4], t, nch = p->n < 4 ? p->n : 4, hi = voice_hi(r);
    uint32_t salt = song_ov()->voicing_salt;
    for (t = 0; t < nch && hi < 72; t++) {               /* the keys' window: every loop chord must fit under it */
        int pcs[5], n = keys_chord_at(r, t * p->bars_per_chord, 1, 0, pcs);
        if (!th_voice_range(pcs, n, NULL, v[0], hi, 1)) hi = 72;   /* one doesn't: the window is ignored */
    }
    loop_cycle(r, 0, 0, hi, rolled, NULL);
    memcpy(vs->loop, rolled, sizeof(int) * 4 * (size_t)nch);
    for (t = 0; salt && t < 8; t++) {
        uint32_t h = mix(salt, r->seed, 194, (uint32_t)t);
        if (loop_cycle(r, 62 + (int)(h % 9u), 48 + (int)(h / 9u % 11u), hi, v, rolled)) {
            memcpy(vs->loop, v, sizeof(int) * 4 * (size_t)nch);
            break;
        }
    }
    vs->nloop = nch;
}

/* Locking the loop's voicings (option 2) is a switch, on by default: off, every chord is voiced from the last one
   sounded, rootless as always (whether always-rootless voicing keeps the loop steady by itself is the question) */
static int lock_loop = 1;
void harmony_set_loop_lock(int on) { lock_loop = on != 0; }

/* what the loop's voicings depend on: when it changes (a request lands), they are worked out again */
static uint32_t harmony_sig(const song_roll_t *r)
{
    uint32_t h = 2166136261u;
    int f[5] = {RF_KEY, RF_MODE, RF_PROG, RF_COLOR, 0}, i;
    for (i = 0; i < 4; i++) h = (h ^ (uint32_t)(r->v[f[i]] + 1)) * 16777619u;
    h = (h ^ r->seed) * 16777619u;
    if (song_ov()->voicing_salt) h = (h ^ song_ov()->voicing_salt) * 16777619u;
    h = (h ^ (uint32_t)voice_hi(r)) * 16777619u;         /* the keys' window */
    h = (h ^ (uint32_t)strum_hi(r)) * 16777619u;
    return h | 1u;               /* never 0: 0 means not yet worked out */
}

/* The bar's voicing: the loop's for its chord, or (a chord recolored on this pass) voiced from the last one.
   Intro and outro (dark) voice exactly as the rest: lo-fi keeps its voicings through a song (Sam); they only play
   softer, and Plan 3's lo-fi chain darkens their sound. */
static void voice_chord(const song_roll_t *r, int bar, const int *pcs, int n, harmony_voicing_t *vs, int *sound);

static int keys_sustained(const song_roll_t *r);

int harmony_pad_place(const song_roll_t *r)
{
    rng_t g;
    if (!keys_sustained(r)) return PL_BELOW;
    if (song_ov()->pad_place >= 0) return song_ov()->pad_place;
    rng_seed(&g, r->seed, 160);
    return rng_int(&g, 3);
}

/* the workbench's keys_oct: the keys an octave up or down where the instrument's range holds the whole register
   the voicer (C3-E5) or the strum (E2-E5) can reach, else where they were; one shift for the song */
int harmony_keys_oct(const song_roll_t *r)
{
    const lib_set_t *set = lib_set_by_id((uint8_t)r->v[RF_SET]);
    int d = 12 * song_ov()->keys_oct, lo, hi, sh, strum;
    if (!d || !set || set->chords == INS_VIBES) return 0;   /* (the vibes: their register is the bars' own, F3 up,
                                                               place_keys) */
    strum = harmony_style(r) == CS_STRUM;
    sh = strum ? 0 : lib_set_keys_shift(set) + (harmony_pad_place(r) != PL_BELOW ? 12 : 0);
    song_ov_range(set->lead, &lo, &hi);
    if (d > 0 && !keys_sustained(r) && hi < 94) return 0;   /* the lead sits a 4th over percussive keys: raised, they
                                                               need a lead whose ceiling can rise with them (lead.c) */
    song_ov_range(set->chords, &lo, &hi);
    return (strum ? 40 : 48) + sh + d >= lo && 76 + sh + d <= hi ? d : 0;
}

/* The keys' window (harmony.h), and the voicer's top under it for this song: C5 (72) where it doesn't bind or can't
   hold (the top under C4, the voicer's lowest notes (C3, as placed) under the keys instrument's range or the
   window's floor); every keys part under the lead (Sam: sustained keys too, unless the pad sits with or above the
   lead), not a strum (strum_hi), and not where the workbench moved the keys an octave (keys_oct: its place wins, the
   lead's range moves up over them) */
static int win_lo, win_hi;
void harmony_keys_window(int lo, int hi) { win_lo = lo; win_hi = hi; }

static int voice_hi(const song_roll_t *r)
{
    const lib_set_t *set = lib_set_by_id((uint8_t)r->v[RF_SET]);
    int sh, h, lo, hi;
    if (!win_hi || !set || harmony_pad_place(r) != PL_BELOW || harmony_style(r) == CS_STRUM || harmony_keys_oct(r))
        return 72;
    sh = lib_set_keys_shift(set);
    h = win_hi - sh;
    song_ov_range(set->chords, &lo, &hi);
    return h >= 72 || h < 60 || 48 + sh < lo || 48 + sh < win_lo ? 72 : h;
}

/* a strum's top under the window (Sam: the guitar makes room for the lead too): E5 where it doesn't bind, or where
   it would leave the shapes under E4 (the open position's top) */
static int strum_hi(const song_roll_t *r)
{
    if (!win_hi || harmony_style(r) != CS_STRUM || harmony_keys_oct(r)) return 76;
    return win_hi >= 64 && win_hi < 76 ? win_hi : 76;
}

static void keys_oct(const song_roll_t *r, int *notes, int n)
{
    int i, d = harmony_keys_oct(r);
    for (i = 0; i < n && d; i++)
        if (notes[i]) notes[i] += d;
}

static void place_keys(const song_roll_t *r, int *notes)  /* a pad with or above the lead: an octave up */
{
    const lib_set_t *set = lib_set_by_id((uint8_t)r->v[RF_SET]);
    int i, sh = lib_set_keys_shift(set);
    if (harmony_pad_place(r) != PL_BELOW) sh += 12;
    for (i = 0; i < 4; i++)
        if (notes[i]) notes[i] += sh;
    keys_oct(r, notes, 4);
    if (set && set->chords == INS_VIBES) {   /* the vibes: four mallets in the bars' range, a note under F3 an octave
                                                up; one a semitone under another (the lift's) up again, a 7th over it */
        int j, k;
        for (i = 0; i < 4; i++)
            while (notes[i] && notes[i] < 53) notes[i] += 12;
        for (k = 0; k < 4; k++)
            for (i = 0; i < 4; i++)
                for (j = 0; j < 4; j++)
                    if (notes[i] && notes[j] - notes[i] == 1) notes[i] += 12;
    }
}

/* A strummed song's guitar shape for this bar's chord: the one it had before in the song, else a new one voice-led
   from the shape found last (strike or peek alike, so a peek ahead and the strike agree) */
static void strum_shape(const song_roll_t *r, int bar, int pass, harmony_voicing_t *vs, int shape[6])
{
    int pcs[5], n = keys_chord_at(r, bar, pass, 0, pcs), i, k, prev[6] = {0, 0, 0, 0, 0, 0};
    uint32_t sig = 2166136261u;
    for (i = 0; i < n; i++) sig = (sig ^ (uint32_t)(pcs[i] + 1)) * 16777619u;
    sig |= 1u;                                           /* never 0: an empty slot */
    for (k = 0; k < HV_SHAPES && k < vs->gn; k++)
        if (vs->gsig[k] == sig) {
            for (i = 0; i < 6; i++) shape[i] = vs->gtab[k][i];
            return;
        }
    if (vs->gn)
        for (i = 0; i < 6; i++) prev[i] = vs->gtab[(vs->gn - 1) % HV_SHAPES][i];
    strum_voicing(pcs, n, prev, shape, strum_hi(r));
    k = vs->gn % HV_SHAPES;
    for (i = 0; i < 6; i++) vs->gtab[k][i] = (uint8_t)shape[i];
    vs->gsig[k] = sig;
    vs->gn++;
}

void harmony_voicing_at(const song_roll_t *r, int bar, int pass, harmony_voicing_t *vs, int out4[4])
{
    harmony_voicing_raw_at(r, bar, pass, vs, out4, NULL);
}

void harmony_voicing_raw_at(const song_roll_t *r, int bar, int pass, harmony_voicing_t *vs, int out4[4], int raw4[4])
{
    int pcs[5], n, keep[4];
    if (harmony_style(r) == CS_STRUM) {                  /* the guitar shape's top 4 */
        int shape[6], i, k = 4;
        strum_shape(r, bar, pass, vs, shape);
        keys_oct(r, shape, 6);
        memset(out4, 0, 4 * sizeof(int));
        for (i = 5; i >= 0 && k > 0; i--)
            if (shape[i]) out4[--k] = shape[i];
        return;
    }
    n = keys_chord_at(r, bar, pass, 0, pcs);
    memcpy(keep, vs->last, sizeof keep);
    voice_chord(r, bar, pcs, n, vs, out4);
    memcpy(vs->last, keep, sizeof keep);                 /* a peek: the strike still voices from the same last */
    if (raw4) memcpy(raw4, out4, 4 * sizeof(int));
    place_keys(r, out4);
}

void harmony_keys_notes_at(const song_roll_t *r, int bar, int pass, harmony_voicing_t *vs, int out6[6])
{
    out6[4] = out6[5] = 0;
    if (harmony_style(r) == CS_STRUM) {                  /* the whole guitar shape, its low strings too */
        strum_shape(r, bar, pass, vs, out6);
        keys_oct(r, out6, 6);
        return;
    }
    harmony_voicing_at(r, bar, pass, vs, out6);
}

int harmony_keys_high(const song_roll_t *r)
{
    harmony_voicing_t vs;
    int bar, k, o[4], sum = 0, n = 0;
    memset(&vs, 0, sizeof vs);
    for (bar = 0; bar < 16; bar++) {
        harmony_voicing_at(r, bar, 0, &vs, o);
        for (k = 0; k < 4; k++)
            if (o[k] > 0) {
                sum += o[k];
                n++;
            }
    }
    return n && sum >= HV_HIGH * n;
}

static void voice_chord(const song_roll_t *r, int bar, const int *pcs, int n, harmony_voicing_t *vs, int *sound)
{
    const lib_prog_t *p = prog_of(r);
    int base[5], nb, idx = (bar / p->bars_per_chord) % p->n, have = vs->last[0] || vs->last[3];
    if (vs->sig != harmony_sig(r)) {
        loop_voicings(r, vs);
        vs->sig = harmony_sig(r);
    }
    nb = keys_chord_at(r, bar, 1, 0, base);
    if (lock_loop && idx < vs->nloop && nb == n && !memcmp(base, pcs, sizeof(int) * (size_t)n))
        memcpy(sound, vs->loop[idx], 4 * sizeof(int));
    else if (!th_voice_range(pcs, n, have ? vs->last : NULL, sound, voice_hi(r), 1))
        th_voice_range(pcs, n, have ? vs->last : NULL, sound, 72, 1);   /* no fit under the keys' window */
    memcpy(vs->last, sound, 4 * sizeof(int));
}

static int emit_chord(const int *notes, int n, uint32_t gap, int lo, int hi, uint32_t len, int dark, uint32_t salt,
                      int bar, int step, song_ev_t *out, int max)
{
    int i, cnt = 0;
    for (i = 0; i < n && cnt < max; i++) {
        int v = lo + (int)(mix(salt, (uint32_t)bar, (uint32_t)step, 40u + (uint32_t)i) % (uint32_t)(hi - lo + 1));
        v = clampi(v + (i == 0 ? 3 : 0), lo, hi);
        if (dark == 1) v -= 6;            /* softer in the dark intro and outro (was 10: with the comping quieter,
                                             the intro sank) */
        if (dark == 2) v += 6;            /* firmer with no kick or lead: the keys carry the block */
        out[cnt].voice = V_CHORD;
        out[cnt].note = (uint8_t)notes[i];
        out[cnt].vel = (uint8_t)clampi(v, 25, 110);
        out[cnt].pad = 0;
        out[cnt].delay = gap * (uint32_t)i;
        out[cnt].len = len > out[cnt].delay ? len - out[cnt].delay : 1u;   /* all notes end together */
        cnt++;
    }
    return cnt;
}

static int keys_sustained(const song_roll_t *r)
{
    const lib_set_t *set = lib_set_by_id((uint8_t)r->v[RF_SET]);
    return set && lib_ins_sustained(set->chords);
}

/* Sam: the stabs answer a sparse kick, never double it ("they can be on the same beat, but only if both aren't a
   sparse pattern"; stabs always are): over a kick of 3 hits a bar or fewer, the second stab goes on the and of 2, or,
   where the kick plays there, on the and of 3, beat 2 or beat 4, the first the kick leaves free; over a busier kick
   it stays on the and of 2 (COMP_STABS: patterns_gen.c, from patterns/comp-rhythms) */
unsigned harmony_stab_steps(const song_roll_t *r)
{
    const patterns_stabs_t *s = &COMP_STABS;
    const lib_pattern_t *p = lib_pattern_by_id((uint8_t)r->v[RF_PATTERN]);
    unsigned kick = p ? lib_lane_hits(p, D_KICK, 0) : 0u;
    int i, hits = 0;
    for (i = 0; i < 16; i++) hits += (kick >> i) & 1u;
    if (hits > s->busy_over) return s->busy_steps;
    for (i = 0; i < s->n; i++)
        if (!((kick >> s->second[i]) & 1u)) return 1u << s->first | 1u << s->second[i];
    return 1u << s->first;
}

/* the steps a held, half-note, held-on-2 (and 4), light-eighths or comp-rhythm comping strikes (COMP_*) */
static unsigned comp_mask(const patterns_comp_t *c)
{
    unsigned m = 0;
    int i;
    for (i = 0; c && i < c->n; i++) m |= 1u << c->step[i];
    return m;
}

int harmony_comp_pat(const song_roll_t *r)
{
#if COMP_NPAT
    int i, f = song_ov()->comp_pat - 1, n = comp_pat_napproved();
    uint32_t k;
    rng_t g;
    if (f >= 0 && f < COMP_NPAT) return f;
    rng_seed(&g, r->seed, 202);
    k = rng_next(&g) % (uint32_t)(n ? n : COMP_NPAT);
    for (i = 0; i < COMP_NPAT; i++)
        if ((!n || COMP_PAT_APPROVED(i)) && k-- == 0) return i;
#endif
    (void)r;
    return -1;
}

/* the strikes a held-on-2 (and 4), light-eighths or comp-rhythm song plays (NULL: another style) */
static const patterns_comp_t *strikes_of(const song_roll_t *r, int style)
{
#if COMP_NPAT
    if (style == CS_PAT) {
        int i = harmony_comp_pat(r);
        return i >= 0 ? &COMP_PAT[i] : NULL;
    }
#endif
    (void)r;
    return style == CS_HELD2 ? &COMP_HELD2 : style == CS_HELD24 ? &COMP_HELD24 : style == CS_LIGHT8 ? &COMP_LIGHT8
                                                                                                    : NULL;
}

/* Half notes hold instead over sustained keys (a pad never re-strikes), and over a kick that already plays on 1 and 3
   alone (Sam: the keys never copy the kick's rhythm) */
static int half_held(const song_roll_t *r)
{
    const lib_pattern_t *p = lib_pattern_by_id((uint8_t)r->v[RF_PATTERN]);
    return keys_sustained(r) || (p && (lib_lane_hits(p, D_KICK, 0) & 0xFFFFu) == comp_mask(&COMP_HALF));
}

#if COMP_NPAT
/* A comp rhythm holds instead, as half notes do, over a kick that plays its very rhythm (a charleston over a kick on
   1 and the and of 2) */
static int pat_held(const song_roll_t *r)
{
    const lib_pattern_t *p = lib_pattern_by_id((uint8_t)r->v[RF_PATTERN]);
    const patterns_comp_t *c = strikes_of(r, CS_PAT);
    return !c || (p && (lib_lane_hits(p, D_KICK, 0) & 0xFFFFu) == comp_mask(c));
}
#endif

/* the steps the keys strike when they are more than a note on 1 (stabs, half notes, held on 2 (and 4), light
   eighths, a comp rhythm), else 0: what the bass avoids */
unsigned harmony_comp_steps(const song_roll_t *r)
{
    int style = harmony_style(r);
    if (style == CS_STABS) return harmony_stab_steps(r);
#if COMP_NPAT
    if (style == CS_PAT && pat_held(r)) return 0u;
#endif
    if (style == CS_HELD2 || style == CS_LIGHT8 || style == CS_HELD24 || style == CS_PAT)
        return comp_mask(strikes_of(r, style));
    return harmony_half_strikes(r) ? comp_mask(&COMP_HALF) : 0u;
}

int harmony_half_strikes(const song_roll_t *r) { return harmony_style(r) == CS_HALF && !half_held(r); }

/* is the last voicing sounded made of this chord's pitch classes (so a later strike of it can keep it) */
static int last_is_chord(const harmony_voicing_t *vs, const int *pcs, int n)
{
    int i, j;
    if (!vs->last[0] && !vs->last[3]) return 0;
    for (i = 0; i < 4; i++) {
        int in = 0;
        for (j = 0; j < n; j++) in |= (vs->last[i] - pcs[j] + 120) % 12 == 0;
        if (!in) return 0;
    }
    return 1;
}

/* Held on 2's, held on 2 and 4's, light eighths' or a comp rhythm's strike on this step: its index in c (-1: none).
   Every bar plays the written strikes, a busy lead or not (Sam). A strike written as long as the gap to the next (or to
   the bar's end) is held to it; *held says so. */
static int comp_strike(const patterns_comp_t *c, int step, int *held)
{
    int k;
    for (k = 0; k < c->n && c->step[k] != step; k++)
        ;
    if (k == c->n) return -1;
    *held = step + c->len[k] >= (k + 1 < c->n ? c->step[k + 1] : 16);
    return k;
}

int harmony_step(const song_roll_t *r, const plan_block_t *b, int bar, int step, int pass, float step_samples,
                 harmony_voicing_t *vs, int busy, uint32_t salt, song_ev_t *out, int max)
{

    static const uint8_t ARP[6] = {0, 1, 2, 3, 2, 1};
    const lib_prog_t *p;
    int style, pcs[5], n, dark, lo, hi, k;
    uint32_t len, gap;
    int notes[4], one, rs, first = 0, later = 100;
    if (!(b->layers & L_KEYS) || max <= 0) return 0;
    p = prog_of(r);
    style = harmony_style(r);
    if (style == CS_HALF && half_held(r)) style = CS_HELD;
#if COMP_NPAT
    if (style == CS_PAT && pat_held(r)) style = CS_HELD;
#endif
    /* a drum entry with no kick and no bass (snare-first's keys, snare and hats) over struck keys held a chord of 2
       bars or more: they would ring out in its second bar, so the held chord restrikes each bar it spans */
    rs = style == CS_HELD && p->bars_per_chord >= 2 && !(b->flags & PB_DARK) && !(b->layers & (L_KICK | L_LEAD)) &&
         (b->layers & L_SNARE) && !(b->layers & L_BASS) && !keys_sustained(r);
    if (style == CS_STRUM) {                  /* the guitars' strum (strum.c): the lead's busy bars leave it be, as
                                                 held comping (the keys' duck makes the room) */
        int shape[6], i, k = 4;
        strum_shape(r, bar, pass, vs, shape);
        memset(vs->last, 0, sizeof vs->last);           /* the keys' last voicing: the shape's top 4 (as
                                                           harmony_voicing_at's), so a set change voice-leads on */
        for (i = 5; i >= 0 && k > 0; i--)
            if (shape[i]) vs->last[--k] = shape[i];
        keys_oct(r, shape, 6);
        return strum_step(r, b, bar, step, step_samples, shape, out, max);
    }
    switch (style) {
    case CS_HELD:
        if (step != COMP_HELD.step[0] || (bar % p->bars_per_chord != 0 && !rs)) return 0;
        len = (uint32_t)((float)((rs ? 1 : p->bars_per_chord) * 16) * step_samples);
        len = len > 441u ? len - 441u : 1u;
        lo = COMP_HELD.vel_lo; hi = COMP_HELD.vel_hi;   /* struck softly, a rounder attack (Sam); keys_gain keeps
                                                           the level */
        if (rs && bar % p->bars_per_chord != 0)
            hi = 54;                      /* the restrike a little softer */
        break;
    case CS_STABS:
        if (!((harmony_stab_steps(r) >> step) & 1u)) return 0;
        len = (uint32_t)(4.0f * step_samples * 0.6f);
        lo = COMP_STABS.vel_lo; hi = COMP_STABS.vel_hi;
        break;
    case CS_OFFBEATS:
        if (step != 2 && step != 6 && step != 10 && step != 14) return 0;
        len = (uint32_t)(2.0f * step_samples * 0.8f);
        lo = 60; hi = 85;
        break;
    case CS_HALF:                         /* beats 1 and 3, each held to the next strike */
        for (k = 0; k < COMP_HALF.n && COMP_HALF.step[k] != step; k++)
            ;
        if (k == COMP_HALF.n) return 0;
        len = (uint32_t)((float)COMP_HALF.len[k] * step_samples);
        len = len > 441u ? len - 441u : 1u;
        lo = COMP_HALF.vel_lo; hi = COMP_HALF.vel_hi;   /* as held: struck softly */
        break;
    case CS_HELD2:                        /* beat 2 (a chord change on a bar line too), held to the bar's end */
    case CS_HELD24:                       /* the same, held to a soft beat 4 struck every bar */
    case CS_LIGHT8:                       /* every 8th, half an 8th long, soft */
    case CS_PAT: {                        /* a comp rhythm's strikes (COMP_PAT), every bar */
        const patterns_comp_t *c = strikes_of(r, style);
        int held = 0;
        if (!c) return 0;
        k = comp_strike(c, step, &held);
        if (k < 0) return 0;
        len = (uint32_t)((float)c->len[k] * step_samples);
        if (held) len = len > 441u ? len - 441u : 1u;   /* as held: it ends just before the next strike */
        lo = c->vel_lo; hi = c->vel_hi;
        first = c->step[0];
        if (k > 0) later = c->later_vel_pct;
        break;
    }
    default:                             /* the arpeggio: one chord tone a 16th (Sam: in 8ths it reads as a melody) */
        len = (uint32_t)step_samples;
        lo = 50; hi = 70;
        break;
    }
    if (busy && style != CS_HELD && !keys_sustained(r) && !song_ov()->no_busy_thin) {   /* the lead is busy: the
                                                             comping makes room (Sam; the workbench may turn it off) */
        if (style == CS_HALF) {                 /* beat 1 of every bar only, held to the next bar's strike */
            if (step != 0) return 0;
            len = (uint32_t)(16.0f * step_samples);
            len = len > 441u ? len - 441u : 1u;
        }
        if (style == CS_STABS && step != 0) return 0;
        if (style == CS_OFFBEATS && step != 6 && step != 14) return 0;
        if (style == CS_ARP && (step & 1)) return 0;
    }
    dark = (b->flags & PB_DARK) != 0;
    dark = dark ? 1 : !(b->layers & (L_KICK | L_LEAD)) ? 2 : 0;   /* dark; carrying (no kick, no lead); or plain */
    n = keys_chord_at(r, bar, pass, dark, pcs);
    if ((style == CS_HALF || style == CS_STABS || style == CS_HELD2 || style == CS_HELD24 || style == CS_LIGHT8 ||
         style == CS_PAT || rs) &&
        !(step == first && bar % p->bars_per_chord == 0) && last_is_chord(vs, pcs, n))
        memcpy(notes, vs->last, sizeof notes);            /* the chord's other strikes keep its voicing, never
                                                             revoiced (one voicing per chord, Sam; the pad leaves
                                                             room for that one) */
    else
        voice_chord(r, bar, pcs, n, vs, notes);
    place_keys(r, notes);
    if (style == CS_ARP) {
        k = ARP[(bar * 16 + step) % 6];
        one = notes[k];
        return emit_chord(&one, 1, 0, lo, hi, len, dark, salt, bar, step, out, max);
    }
    /* per-chord roll spread, 10-60 ms between notes (the Knight: see below), the same for every strike of the chord
       in a bar */
    gap = (uint32_t)(10 + (int)(mix(salt, (uint32_t)bar, r->seed, 50) % 51u)) * 441u / 10u;
    if (lib_set_by_id((uint8_t)r->v[RF_SET]) && (lib_set_by_id((uint8_t)r->v[RF_SET])->chords == INS_KNIGHT ||
                                                 lib_set_by_id((uint8_t)r->v[RF_SET])->chords == INS_VIBES))
        gap = (mix(salt, (uint32_t)bar, r->seed, 50) % 9u) * 441u / 10u / 3u;   /* Sam: an acoustic piano strikes
                                                                                 together, 0-8 ms in all (the vibes'
                                                                                 four mallets too) */
    if (style == CS_LIGHT8 && gap * 9u > len)
        gap = len / 9u;                   /* a short strike: the roll spans a third of it at most (a wider one would
                                             leave the top note a click) */
    if (later < 100) {                    /* held on 2 and 4's beat 4 (a comp rhythm's later strikes): softer than
                                             the first (the same draw) */
        int c = emit_chord(notes, 4, gap, lo, hi, len, dark, salt, bar, first, out, max), i;
        for (i = 0; i < c; i++) out[i].vel = (uint8_t)clampi(out[i].vel * later / 100, 25, 110);
        return c;
    }
    if (style == CS_HALF && step != COMP_HALF.step[0]) {  /* the later strikes a little softer than the first (same
                                                             draw) */
        int c = emit_chord(notes, 4, gap, lo, hi, len, dark, salt, bar, 0, out, max), i;
        for (i = 0; i < c; i++) out[i].vel = (uint8_t)clampi(out[i].vel * COMP_HALF.later_vel_pct / 100, 25, 110);
        return c;
    }
    return emit_chord(notes, 4, gap, lo, hi, len, dark, salt, bar, step, out, max);
}
