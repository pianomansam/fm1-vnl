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

/* the chord of this bar after any reharmonization; color is the song color (before dark) */
static th_chord_spec_t spec_at(const song_roll_t *r, const lib_prog_t *p, int bar, int pass, int *color)
{
    int idx = (bar / p->bars_per_chord) % p->n;
    th_chord_spec_t c = p->c[idx];
    int mode = r->v[RF_MODE];
    rng_t g;
    int chance;
    *color = r->v[RF_COLOR];
    if (pass < 2) return c;
    rng_seed(&g, r->seed, 120);
    chance = 25 + rng_int(&g, 26);
    if ((int)(mix(r->seed, (uint32_t)pass, 1, 120) % 100u) >= chance) return c;
    if ((int)(mix(r->seed, (uint32_t)pass, 2, 120) % (uint32_t)p->n) != idx) return c;
    if (c.quality == Q_DOM7 || (c.degree == 4 && c.alter == 0 && c.quality == Q_DIATONIC &&
                                (mode == MODE_IONIAN || mode == MODE_LYDIAN))) {
        int a = c.alter + 6;
        c.alter = (int8_t)(a > 6 ? a - 12 : a);          /* tritone substitution */
        c.quality = Q_DOM7;
    } else if (c.degree == 3 && c.alter == 0 &&
               (c.quality == Q_MAJ7 || (c.quality == Q_DIATONIC &&
                                        (mode == MODE_IONIAN || mode == MODE_DORIAN || mode == MODE_MIXOLYDIAN)))) {
        c.quality = Q_MIN7;                              /* IV becomes iv */
    } else if (*color < COLOR_RICH) {
        (*color)++;
    }
    return c;
}

int harmony_chord_at(const song_roll_t *r, int bar, int pass, int dark, int *pcs)
{
    int color;
    th_chord_spec_t c = spec_at(r, prog_of(r), bar < 0 ? 0 : bar, pass, &color);
    if (dark && color > COLOR_7) color--;
    return th_chord_pcs(r->v[RF_KEY], r->v[RF_MODE], c, color, pcs);
}

int harmony_style(const song_roll_t *r)
{
    rng_t g;
    rng_seed(&g, r->seed, 121);
    return rng_int(&g, 4);
}

/* Voice the chord, reusing the held voicing while the chord is the same; dark drops tops above 64 an octave. */
static void voice_chord(const int *pcs, int n, int dark, int *state)
{
    int have = state[0] || state[1] || state[2] || state[3];
    int i, j, same = have;
    int out[4];
    if (have)
        for (i = 0; i < 4 && same; i++) {
            int in = 0;
            for (j = 0; j < n; j++) if (state[i] % 12 == pcs[j]) in = 1;
            for (j = 0; j < i; j++) if (state[j] % 12 == state[i] % 12) in = 0;
            if (!in) same = 0;
        }
    if (same) memcpy(out, state, sizeof out);
    else th_voice(pcs, n, have ? state : NULL, out);
    if (dark)
        for (i = 0; i < 4; i++) {
            int top = 0;
            for (j = 1; j < 4; j++) if (out[j] > out[top]) top = j;
            if (out[top] <= 64) break;
            out[top] -= 12;
        }
    for (i = 1; i < 4; i++)                              /* ascending order */
        for (j = i; j > 0 && out[j] < out[j - 1]; j--) { int t = out[j]; out[j] = out[j - 1]; out[j - 1] = t; }
    memcpy(state, out, sizeof out);
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
        out[cnt].len = len;
        cnt++;
    }
    return cnt;
}

int harmony_step(const song_roll_t *r, const plan_block_t *b, int bar, int step, int pass, float step_samples,
                 int *voicing_state, uint32_t salt, song_ev_t *out, int max)
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
    voice_chord(pcs, n, dark, voicing_state);
    memcpy(notes, voicing_state, sizeof notes);
    if (style == CS_ARP) {
        k = ARP[(bar * 8 + step / 2) % 6];
        one = notes[k];
        return emit_chord(&one, 1, 0, lo, hi, len, dark, salt, bar, step, out, max);
    }
    /* per-chord roll spread, 10-60 ms between notes, the same for every strike of the chord in a bar */
    gap = (uint32_t)(10 + (int)(mix(salt, (uint32_t)bar, r->seed, 50) % 51u)) * 441u / 10u;
    return emit_chord(notes, 4, gap, lo, hi, len, dark, salt, bar, step, out, max);
}
