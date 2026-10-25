/* SPDX-License-Identifier: MPL-2.0 */
/* The strum engine (see strum.h), from the instrument spike's part_strum and its patterns (branch
   spike/instruments): a stroke per pattern hit, its strings rolled one after another. */
#include <stdint.h>
#include <string.h>
#include "strum.h"
#include "rng.h"

static uint32_t mix(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 1) * 0x85EBCA6Bu ^ (c + 1) * 0xC2B2AE35u ^ (d + 1) * 0x27D4EB2Fu;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
    return h;
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static int absi(int v) { return v < 0 ? -v : v; }

/* ------------------------------------------------------------------------------------------------ the patterns */
/* 16 steps (the bossa 32: two bars). D an accented down stroke (every string), d a down stroke, U an up stroke (the
   top 4 strings), u a softer up (the top 3), X a chuck (muted, every string), T the thumb (the lowest string alone),
   P a pinch (the top 4, nearly together), B the thumb and a pinch, '.' a rest. Up strokes only on the "and"s (the odd
   8ths): the spike's chuck pattern had two 16th-note ups, moved to the "and"s. */
static const char *const PAT[SP_N] = {
    "D...d.u...U.d.u.",                 /* D-DU-UDU */
    "D.u.X.U.d.u.X.U.",                 /* down-up 8ths, chucks on 2 and 4 */
    "B..P..P.T.P..P..T.P..P..T.P..P..", /* the thumb on 1 and 3, pinches between */
    "D.......d.....u.",                 /* the ballad: three strokes a bar */
};

/* the song's draw on stream 194, hashed: the streams' states differ by a constant XOR, and the comping style's draw
   (stream 121, mod 100) shares this one's low bits, so a bare draw mod 4 would tie the pattern to the style */
static uint32_t strum_draw(const song_roll_t *r)
{
    rng_t g;
    rng_seed(&g, r->seed, 194);
    return mix(rng_next(&g), r->seed, 194u, 1u);
}

int strum_pattern(const song_roll_t *r, int *slow)
{
    uint32_t h = strum_draw(r);
    int p = (int)(h % SP_N);
    if (slow) *slow = p == SP_BALLAD && ((h / SP_N) & 1u);
    return p;
}

/* the song's spread between strings, in ms: quick 7-12, slow 40-70 */
static float strum_gap_ms(const song_roll_t *r, int slow)
{
    float u = (float)(strum_draw(r) >> 16) * (1.0f / 65536.0f);
    return slow ? 40.0f + 30.0f * u : 7.0f + 5.0f * u;
}

/* the ballad in a drum fill's bar (its kick on 1, 3 and the and of 4 would be the ballad's own rhythm, Sam: the keys
   never copy the kick's): the up stroke on the and of 2, out of the fill's way */
static const char BALLAD_FILL[] = "D.....u.d.......";

static char hit_at(const char *p, int bar, int step)
{
    return p[(p[16] ? bar & 1 : 0) * 16 + step];          /* a bar, or two (the bossa) */
}

/* ------------------------------------------------------------------------------------------------- the voicer */
static const int TUNE[6] = {40, 45, 50, 55, 59, 64};   /* E2 A2 D3 G3 B3 E4 */
enum {
    V_LO = 48,     /* the lowest note: C3, the keys' floor (under it the mud high-pass thins a note away) */
    V_HI = 76,     /* E5: the top string's 12th fret */
    ROOT_HI = 59,  /* the root (the lowest note) at most B3 */
    SPAN = 4,      /* the fretting hand's reach: fretted notes within 4 frets (open strings free) */
    TOP_AIM = 67   /* the top note's home, G4 */
};

typedef struct {
    int pcs[5], np, root, third, seventh;   /* the chord; its 3rd's and 7th's pitch classes (-1 none) */
    const int *prev;                        /* the last shape (NULL none) */
    int ptop, plow;
    int cur[6], best[6], bcost, rs;
} vc_t;

static int top_of(const int *s)
{
    int i, t = 0;
    for (i = 0; i < 6; i++) if (s[i] > t) t = s[i];
    return t;
}

static int low_of(const int *s)
{
    int i;
    for (i = 0; i < 6; i++) if (s[i]) return s[i];
    return 0;
}

static int in_chord(const vc_t *c, int note)
{
    int j;
    for (j = 0; j < c->np; j++)
        if ((note - c->pcs[j] + 120) % 12 == 0) return 1;
    return 0;
}

/* the cost of a whole shape (lower is better): the 3rd and the 7th first, then the colors, more strings, the top
   note near the last one's (and near home), no 2nds low down */
static int shape_cost(const vc_t *c, const int *s)
{
    int have = 0, i, j, n = 0, top = 0, low = 0, cost = 0, last = -1, inner = 0, cnt[12];
    memset(cnt, 0, sizeof cnt);
    for (i = 0; i < 6; i++) {
        if (!s[i]) { if (n) inner++; continue; }
        if (!low) low = s[i];
        top = s[i];
        n++;
        cnt[s[i] % 12]++;
        if (last >= 0 && s[i] < 55 && s[i] - s[last] < 3) cost += 15;   /* a 2nd low down: mud */
        last = i;
    }
    for (i = 5; i >= 0 && !s[i]; i--) inner--;           /* muted top strings are not inner */
    for (j = 0; j < c->np; j++)
        if (cnt[c->pcs[j]]) have |= 1 << j;
    for (j = 0; j < c->np; j++) {
        int pc = c->pcs[j], iv = (pc - c->root + 12) % 12;
        if (have & (1 << j)) continue;
        cost += pc == c->third ? 100 : pc == c->seventh ? 60 : iv == 7 ? 2 : 12;
    }
    for (i = 0; i < 12; i++)
        if (cnt[i] > 2) cost += 6 * (cnt[i] - 2);
    if (c->third >= 0 && cnt[c->third] > 1) cost += 4;   /* a doubled 3rd */
    cost += (6 - n) * 5 + inner * 4;
    cost += absi(top - TOP_AIM) + (top > 71 ? 3 * (top - 71) : 0);   /* over B4 it pokes up into the lead's range */
    if (c->prev) {
        int d = absi(top - c->ptop);
        cost += 4 * d + (d > 5 ? 40 : 0) + absi(low - c->plow);
    } else
        cost += absi(top - TOP_AIM);
    return cost;
}

/* string s onward: each muted, open or fretted within the hand (fmin..fmax so far), ascending, no rub */
static void vc_dfs(vc_t *c, int s, int last, int fmin, int fmax)
{
    int f, n, k, ok;
    if (s == 6) {
        int i, cnt = 0, cost;
        for (i = 0; i < 6; i++) cnt += c->cur[i] != 0;
        if (cnt < 4) return;
        cost = shape_cost(c, c->cur) + (fmax - fmin >= SPAN ? 6 : 0) + (fmax > 9 ? 3 * (fmax - 9) : 0);   /* a
                                                                             stretch; high up the neck */
        if (cost < c->bcost) { c->bcost = cost; memcpy(c->best, c->cur, sizeof c->best); }
        return;
    }
    c->cur[s] = 0;
    vc_dfs(c, s + 1, last, fmin, fmax);                  /* muted */
    for (f = 0; f <= 12; f++) {
        int lo = fmin, hi = fmax;
        n = TUNE[s] + f;
        if (n <= last || n > V_HI || !in_chord(c, n)) continue;
        if (f > 0) {                                     /* fretted: within the hand's reach */
            lo = f < lo ? f : lo;
            hi = f > hi ? f : hi;
            if (hi - lo > SPAN) continue;
        }
        for (k = 0, ok = 1; k < s && ok; k++)            /* no note a minor 2nd (or 9th ...) over another */
            ok = !c->cur[k] || (n - c->cur[k]) % 12 != 1;
        if (!ok) continue;
        c->cur[s] = n;
        vc_dfs(c, s + 1, n, lo, hi);
    }
    c->cur[s] = 0;
}

int strum_voicing(const int *pcs, int npcs, const int prev[6], int out[6])
{
    vc_t c;
    int i, rs, f, n = 0, have = prev && (prev[0] | prev[1] | prev[2] | prev[3] | prev[4] | prev[5]);
    memset(&c, 0, sizeof c);
    c.np = npcs < 0 ? 0 : npcs > 5 ? 5 : npcs;
    for (i = 0; i < c.np; i++) c.pcs[i] = (pcs[i] % 12 + 12) % 12;
    for (i = 0; i < 6; i++) out[i] = 0;
    if (!c.np) return 0;
    c.root = c.pcs[0];
    c.third = c.seventh = -1;
    for (i = 1; i < c.np; i++) {
        int iv = (c.pcs[i] - c.root + 12) % 12;
        if (iv == 3 || iv == 4) c.third = c.pcs[i];
        if (iv == 10 || iv == 11 || (iv == 9 && c.seventh < 0)) c.seventh = c.pcs[i];   /* (a 6th stands in) */
    }
    c.prev = have ? prev : NULL;
    c.ptop = have ? top_of(prev) : 0;
    c.plow = have ? low_of(prev) : 0;
    c.bcost = 1 << 30;
    for (rs = 0; rs < 3; rs++)                            /* the root on the low E, the A or the D string */
        for (f = 0; f <= 12; f++) {
            int root = TUNE[rs] + f;
            if (root < V_LO || root > ROOT_HI || (root - c.root + 120) % 12) continue;
            memset(c.cur, 0, sizeof c.cur);
            c.cur[rs] = root;
            vc_dfs(&c, rs + 1, root, f ? f : 99, f ? f : -99);
        }
    if (c.bcost < (1 << 30)) {
        for (i = 0; i < 6; i++) { out[i] = c.best[i]; n += out[i] != 0; }
        return n;
    }
    /* (no shape fits: the root and the chord tones stacked a 4th or so apart, never rubbing) */
    {
        int note = 48 + c.root, s = 1;
        out[s] = note;
        n = 1;
        while (++s < 6) {
            int x, ok = 0;
            for (x = note + 3; x <= V_HI && !ok; x++) {
                int k;
                ok = in_chord(&c, x);
                for (k = 1; k < s && ok; k++) ok = (x - out[k]) % 12 != 1;
                if (ok) { out[s] = note = x; n++; }
            }
            if (!ok) break;
        }
        return n;
    }
}

/* --------------------------------------------------------------------------------------------- the strokes */
int strum_step(const song_roll_t *r, const plan_block_t *b, int bar, int step, float step_samples, const int shape[6],
               song_ev_t *out, int max)
{
    const char *ps;
    int slow, pat, fill, lo = 0, dir = 1, vel, mute = 0, thumb = 0, pinch = 0, i, k, n = 0, dark, next, low = -1;
    char c;
    float gap, fac = 1.0f, span;
    uint32_t t0 = 0, sw = 0;
    if (!(b->layers & L_KEYS) || max <= 0) return 0;
    pat = strum_pattern(r, &slow);
    fill = bar == b->start + b->bars - 1 && (b->flags & PB_FILL_LAST) && !(b->flags & PB_DROP_LAST);
    ps = fill && pat == SP_BALLAD ? BALLAD_FILL : PAT[pat];
    c = hit_at(ps, bar, step);
    if (c == '.') return 0;
    for (next = step + 1; next < 16 && hit_at(ps, bar, next) == '.'; next++) {}   /* the next stroke (a bar's step 0
                                                                                    always strikes) */
    span = (float)(next - step) * step_samples;
    switch (c) {
    case 'D': vel = 76; break;
    case 'd': vel = 64; break;
    case 'U': vel = 58; lo = 2; dir = -1; fac = 0.8f; break;
    case 'u': vel = 50; lo = 3; dir = -1; fac = 0.8f; break;
    case 'X': vel = 62; mute = 1; fac = 0.6f; break;
    case 'T': vel = 66; thumb = 1; break;
    case 'P': vel = 54; lo = 2; pinch = 1; fac = 0.6f; break;
    default: vel = 54; lo = 2; thumb = pinch = 1; fac = 0.6f; break;   /* B */
    }
    gap = strum_gap_ms(r, slow) * fac;
    gap = slow ? (gap < 32.0f ? 32.0f : gap > 78.0f ? 78.0f : gap) : (gap < 6.0f ? 6.0f : gap > 14.0f ? 14.0f : gap);
    gap *= 44.1f;                                        /* in samples */
    dark = (b->flags & PB_DARK) ? -6 : !(b->layers & (L_KICK | L_LEAD)) ? 6 : 0;
    if (step & 1) {                                      /* an odd 16th swings as the bass does */
        float s = ((float)r->v[RF_SWING] * 0.001f - 0.5f) * 2.0f * step_samples;
        sw = s > 0.0f ? (uint32_t)(s + 0.5f) : 0u;
    }
    t0 = sw + mix(r->seed, (uint32_t)bar, (uint32_t)step, 60u) % 133u;   /* the hand a little late: 0-3 ms */
    for (i = 0; i < 6; i++)
        if (shape[i]) { low = i; break; }
    if (low < 0) return 0;
    for (k = 0; k < 6 && n < max; k++) {
        int s = dir > 0 ? k : 5 - k, v;
        uint32_t at, end;
        if (!shape[s] || (thumb ? s != low && (!pinch || s < lo) : s < lo)) continue;   /* the thumb: the lowest */
        at = t0 + (uint32_t)((float)n * gap) + mix(r->seed, (uint32_t)bar, (uint32_t)step, 70u + (uint32_t)s) % 45u;
        end = (uint32_t)span > 2u ? (uint32_t)span - 2u : 0u;   /* by the next stroke */
        if (at + 1u >= end) break;
        v = vel + (int)(mix(r->seed, (uint32_t)bar, (uint32_t)step, 80u + (uint32_t)s) % 9u) - 4;
        if (dir < 0) v -= v * n / 25;                    /* an up stroke fades as it goes (4% a string) */
        out[n].voice = V_CHORD;
        out[n].note = (uint8_t)shape[s];
        out[n].vel = (uint8_t)clampi(v + dark, 25, 110);
        out[n].pad = (uint8_t)mute;
        out[n].delay = at;
        out[n].len = end - at;
        if (mute && out[n].len > 2646u) out[n].len = 2646u;   /* a chuck: 60 ms */
        n++;
    }
    return n;
}
