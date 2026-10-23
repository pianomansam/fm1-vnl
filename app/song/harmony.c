/* SPDX-License-Identifier: MPL-2.0 */
#include <stdint.h>
#include <string.h>
#include "harmony.h"
#include "rng.h"
#include "theory.h"
#include "library.h"

static uint32_t mix(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 1) * 0x85EBCA6Bu ^ (c + 1) * 0xC2B2AE35u ^ (d + 1) * 0x27D4EB2Fu;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
    return h;
}

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
 * extension (each would clash), the chord drops its 7th: a plain major triad. */
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

int harmony_style(const song_roll_t *r)
{
    rng_t g;
    rng_seed(&g, r->seed, 121);
    return rng_int(&g, 4);
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
   around. Rootless throughout (Sam: keys-only sections too). */
static void loop_voicings(const song_roll_t *r, harmony_voicing_t *vs)
{
    enum { PASSES = 8 };
    const lib_prog_t *p = prog_of(r);
    int pcs[4][5], np[4], v[PASSES][4][4], i, k, best = 1 << 30, pick = 0, nch = p->n < 4 ? p->n : 4;
    const int *prev = NULL;
    for (i = 0; i < nch; i++)
        np[i] = harmony_chord_at(r, i * p->bars_per_chord, 1, 0, pcs[i]);
    for (k = 0; k < PASSES; k++)
        for (i = 0; i < nch; i++) {
            th_voice_range(pcs[i], np[i], prev, v[k][i], 72, 1);
            prev = v[k][i];
        }
    for (k = 1; k < PASSES; k++) {                       /* (pass 0 began from nothing) */
        int c = 0;
        for (i = 0; i < nch; i++)
            c += vl_cost(v[k][(i + nch - 1) % nch], v[k][i]);
        if (c < best) { best = c; pick = k; }
    }
    for (i = 0; i < nch; i++)
        memcpy(vs->loop[i], v[pick][i], sizeof vs->loop[i]);
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
    return (h ^ r->seed) * 16777619u | 1u;               /* never 0: 0 means not yet worked out */
}

/* The bar's voicing: the loop's for its chord, or (a chord recolored on this pass) voiced from the last one.
   Intro and outro (dark) voice exactly as the rest: lo-fi keeps its voicings through a song (Sam); they only play
   softer, and Plan 3's lo-fi chain darkens their sound. */
static void voice_chord(const song_roll_t *r, int bar, const int *pcs, int n, harmony_voicing_t *vs, int *sound)
{
    const lib_prog_t *p = prog_of(r);
    int base[5], nb, idx = (bar / p->bars_per_chord) % p->n, have = vs->last[0] || vs->last[3];
    if (vs->sig != harmony_sig(r)) {
        loop_voicings(r, vs);
        vs->sig = harmony_sig(r);
    }
    nb = harmony_chord_at(r, bar, 1, 0, base);
    if (lock_loop && idx < vs->nloop && nb == n && !memcmp(base, pcs, sizeof(int) * (size_t)n))
        memcpy(sound, vs->loop[idx], 4 * sizeof(int));
    else
        th_voice_range(pcs, n, have ? vs->last : NULL, sound, 72, 1);
    memcpy(vs->last, sound, 4 * sizeof(int));
}

static int emit_chord(const int *notes, int n, uint32_t gap, int lo, int hi, uint32_t len, int dark, uint32_t salt,
                      int bar, int step, song_ev_t *out, int max)
{
    int i, cnt = 0;
    for (i = 0; i < n && cnt < max; i++) {
        int v = lo + (int)(mix(salt, (uint32_t)bar, (uint32_t)step, 40u + (uint32_t)i) % (uint32_t)(hi - lo + 1));
        v = clampi(v + (i == 0 ? 3 : 0), lo, hi);
        if (dark) v -= 10;
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

int harmony_step(const song_roll_t *r, const plan_block_t *b, int bar, int step, int pass, float step_samples,
                 harmony_voicing_t *vs, uint32_t salt, song_ev_t *out, int max)
{
    static const uint8_t ARP[6] = {0, 1, 2, 3, 2, 1};
    const lib_prog_t *p;
    int style, pcs[5], n, dark, lo, hi, k;
    uint32_t len, gap;
    int notes[4], one;
    if (!(b->layers & L_KEYS) || max <= 0) return 0;
    p = prog_of(r);
    style = harmony_style(r);
    switch (style) {
    case CS_HELD:
        if (step != 0 || bar % p->bars_per_chord != 0) return 0;
        len = (uint32_t)((float)(p->bars_per_chord * 16) * step_samples);
        len = len > 441u ? len - 441u : 1u;
        lo = 55; hi = 75;
        break;
    case CS_STABS:
        if (step != 0 && step != 6) return 0;
        len = (uint32_t)(4.0f * step_samples * 0.6f);
        lo = 60; hi = 85;
        break;
    case CS_OFFBEATS:
        if (step != 2 && step != 6 && step != 10 && step != 14) return 0;
        len = (uint32_t)(2.0f * step_samples * 0.8f);
        lo = 60; hi = 85;
        break;
    default:
        if (step & 1) return 0;
        len = (uint32_t)(2.0f * step_samples);
        lo = 50; hi = 70;
        break;
    }
    dark = (b->flags & PB_DARK) != 0;
    n = harmony_chord_at(r, bar, pass, dark, pcs);
    voice_chord(r, bar, pcs, n, vs, notes);
    if (style == CS_ARP) {
        k = ARP[(bar * 8 + step / 2) % 6];
        one = notes[k];
        return emit_chord(&one, 1, 0, lo, hi, len, dark, salt, bar, step, out, max);
    }
    /* per-chord roll spread, 10-60 ms between notes, the same for every strike of the chord in a bar */
    gap = (uint32_t)(10 + (int)(mix(salt, (uint32_t)bar, r->seed, 50) % 51u)) * 441u / 10u;
    return emit_chord(notes, 4, gap, lo, hi, len, dark, salt, bar, step, out, max);
}
