/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The strum engine (see strum.h), from the instrument spike's part_strum and its patterns (branch
   spike/instruments): a stroke per pattern hit, its strings rolled one after another. */
#include <stdint.h>
#include <string.h>
#include "strum.h"
#include "rng.h"
#include "harmony.h"
#include "patterns_gen.h"
#include "override.h"
#include "library.h"

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static int absi(int v) { return v < 0 ? -v : v; }

/* ------------------------------------------------------------------------------------------------ the patterns */
/* 16 steps (the bossa 32: two bars). D an accented down stroke (every string), d a down stroke, U an up stroke (the
   top 4 strings), u a softer up (the top 3), X a chuck (muted, every string), T the thumb (the lowest string alone),
   P a pinch (the top 4, nearly together), B the thumb and a pinch, '.' a rest. Up strokes only on the "and"s (the odd
   8ths): the spike's chuck pattern had two 16th-note ups, moved to the "and"s. The patterns are
   STRUM_PAT (patterns_gen.c, from patterns/strum). */

/* the song's draw on stream 194, hashed: the streams' states differ by a constant XOR, and the comping style's draw
   (stream 121, mod 100) shares this one's low bits, so a bare draw mod 4 would tie the pattern to the style */
static uint32_t strum_draw(const song_roll_t *r)
{
    rng_t g;
    rng_seed(&g, r->seed, 194);
    return harmony_mix(rng_next(&g), r->seed, 194u, 1u);
}

/* the harp's figures (Sam approved them, 2026-11-13): the ballad, the bossa and the charleston, STRUM_PAT[7]
   ("D.....U.........": a stroke on 1, the top strings rolled up on the and of 2; song_test checks the index) */
#define SP_CHARLESTON 7
static const uint8_t HARP_PAT[3] = {SP_BALLAD, SP_BOSSA, SP_CHARLESTON};

/* the harp on the keys (harmony.c's is_harp) */
static int harp(const song_roll_t *r)
{
    const lib_set_t *s = lib_set_by_id((uint8_t)r->v[RF_SET]);
    return s && s->chords == INS_HARP;
}

int strum_pattern(const song_roll_t *r, int *slow)
{
    uint32_t h = strum_draw(r), n = 0, k, w;
    int i, p = 0, f = song_ov()->strum_pat, m = roll_motion(r);
    if (harp(r)) {        /* the harp: its three figures only (no up-and-down strumming, no chucks), drawn as the
                             guitars' are (the same hash, even weights leaning on the motion by busyness); a forced
                             other pattern plays the draw; its spread its own (strum_step) */
        for (i = 0; i < 3; i++) n += motion_tilt(1, STRUM_PAT_BUSY[HARP_PAT[i]], m);
        k = h % n;
        for (i = 0; i < 3; i++) {
            w = motion_tilt(1, STRUM_PAT_BUSY[HARP_PAT[i]], m);
            if (k < w) break;
            k -= w;
        }
        p = HARP_PAT[i < 3 ? i : 0];
        for (i = 0; i < 3; i++)
            if (f == HARP_PAT[i]) p = f;
        if (slow) *slow = 0;
        return p;
    }
    for (i = 0; i < STRUM_NPAT; i++)   /* the approved, evenly, leaning on the rolled motion by busyness */
        if (STRUM_PAT_STATUS[i] == 1) n += motion_tilt(1, STRUM_PAT_BUSY[i], m);
    k = n ? h % n : 0;
    for (i = 0; i < STRUM_NPAT; i++) {
        if (STRUM_PAT_STATUS[i] != 1) continue;
        w = motion_tilt(1, STRUM_PAT_BUSY[i], m);
        if (k < w) { p = i; break; }
        k -= w;
    }
    if (f >= 0 && f < STRUM_NPAT) p = f;
    if (slow) *slow = p == SP_BALLAD && ((h / (n ? n : 1u)) & 1u);
    return p;
}

/* the song's spread between strings, in ms: quick 7-12, slow 40-70; the harp's (slow 2) 12-25 */
static float strum_gap_ms(const song_roll_t *r, int slow)
{
    float u = (float)(strum_draw(r) >> 16) * (1.0f / 65536.0f);
    return slow == 2 ? 12.0f + 13.0f * u : slow ? 40.0f + 30.0f * u : 7.0f + 5.0f * u;
}

/* the ballad in a drum fill's bar (its kick on 1, 3 and the and of 4 would be the ballad's own rhythm, Sam: the keys
   never copy the kick's): the up stroke on the and of 2, out of the fill's way (STRUM_BALLAD_FILL) */

int strum_fill_drive(const song_roll_t *r)
{
    rng_t g;
    if (song_ov()->strum_fill > 0) return song_ov()->strum_fill == 2;   /* the workbench's Strum fill row */
    rng_seed(&g, r->seed, 199);
    return (int)(harmony_mix(rng_next(&g), r->seed, 199u, 1u) >> 16 & 1u);
}

/* the pattern's figure in a drum fill's bar: the ballad its own; every other pattern the song's step back or drive
   (the bossa its thumb-and-pinch versions) */
static const char *fill_figure(const song_roll_t *r, int pat)
{
    int k = pat == SP_BOSSA;
    if (pat == SP_BALLAD) return STRUM_BALLAD_FILL;
    return strum_fill_drive(r) ? STRUM_FILL_DRIVE[k] : STRUM_FILL_STEP[k];
}

static char hit_at(const char *p, int bar, int step)
{
    return p[(p[16] ? bar & 1 : 0) * 16 + step];          /* a bar, or two (the bossa) */
}

/* the bar's stroke at step: from beat 3 the drive figure dr where a fill was asked for (song_move_queue), else the bar's */
static char stroke_at(const char *ps, const char *dr, int bar, int step)
{
    return dr && step >= 8 ? dr[step] : hit_at(ps, bar, step);
}

/* ------------------------------------------------------------------------------------------------- the voicer */
static const int TUNE[6] = {40, 45, 50, 55, 59, 64};   /* E2 A2 D3 G3 B3 E4 */
enum {
    V_LO = 48,     /* the lowest note: C3, the keys' floor (under it the mud high-pass thins a note away) */
    V_HI = 76,     /* E5: the top string's 12th fret */
    ROOT_HI = 59,  /* the root (the lowest note) at most B3 */
    SPAN = 4,      /* the fretting hand's reach: fretted notes within 4 frets (open strings free) */
    TOP_AIM = 64,  /* the top note's home, E4: about the piano voicings' top (the lead sits a 4th over it) */
    TOP_CAP = 67   /* over G4 the top costs more: the lead's floor rises with it */
};

typedef struct {
    int pcs[5], np, root, third, seventh;   /* the chord; its 3rd's and 7th's pitch classes (-1 none) */
    const int *prev;                        /* the last shape (NULL none) */
    int ptop, plow;
    int cur[6], best[6], bcost, rs;
    int8_t cand[6][13], ncand[6];           /* each string's chord tones (frets), up the neck */
    int16_t topmin[V_HI + 1];               /* the least the top note's terms cost with the top at or over a note */
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
    cost += absi(top - TOP_AIM) + (top > TOP_CAP ? 3 * (top - TOP_CAP) : 0);   /* (the lead's room) */
    if (c->prev) {
        int d = absi(top - c->ptop);
        cost += 4 * d + (d > 5 ? 40 : 0) + absi(low - c->plow);
    } else
        cost += absi(top - TOP_AIM);
    return cost;
}

/* shape_cost's top-note terms alone, for the top note t */
static int top_cost(const vc_t *c, int t)
{
    int cost = absi(t - TOP_AIM) + (t > TOP_CAP ? 3 * (t - TOP_CAP) : 0);
    if (c->prev) {
        int d = absi(t - c->ptop);
        return cost + 4 * d + (d > 5 ? 40 : 0);
    }
    return cost + absi(t - TOP_AIM);
}

/* A lower bound on the cost of every shape that keeps strings 0..s-1 as they are (fmin..fmax: the frets so far):
   each term shape_cost can only add to as strings are added (a 2nd low down, a pitch class 3 times, a doubled 3rd,
   the muted strings so far and the inner ones among them, the low note, the stretch and the neck), and the top
   note's terms at their least for a top at or over the last note so far. The search leaves a branch whose bound
   reaches the best so far: none of its shapes could beat it, so the shape found is the full search's, exactly. */
static int vc_bound(const vc_t *c, int s, int last, int fmin, int fmax)
{
    int i, cost = 0, low = 0, n = 0, prev = -1, first = -1, muted = 0, cnt[12];
    memset(cnt, 0, sizeof cnt);
    for (i = 0; i < s; i++) {
        int x = c->cur[i];
        if (!x) { if (first >= 0) muted++; continue; }
        if (first < 0) first = i;
        else cost += 4 * muted;                         /* inner: muted strings between two notes */
        muted = 0;
        if (!low) low = x;
        n++;
        cnt[x % 12]++;
        if (prev >= 0 && x < 55 && x - c->cur[prev] < 3) cost += 15;
        prev = i;
    }
    for (i = 0; i < 12; i++)
        if (cnt[i] > 2) cost += 6 * (cnt[i] - 2);
    if (c->third >= 0 && cnt[c->third] > 1) cost += 4;
    cost += (s - n) * 5;                                /* a muted string so far stays muted */
    if (c->prev) cost += absi(low - c->plow);
    cost += c->topmin[last];
    return cost + (fmax - fmin >= SPAN ? 6 : 0) + (fmax > 9 ? 3 * (fmax - 9) : 0);
}

/* string s onward: each muted, open or fretted within the hand (fmin..fmax so far), ascending, no rub (rub: the
   pitch classes a semitone over a note so far) */
static void vc_dfs(vc_t *c, int s, int last, int fmin, int fmax, int placed, unsigned rub)
{
    int k;
    if (s == 6) {
        int cost;
        if (placed < 4) return;
        cost = shape_cost(c, c->cur) + (fmax - fmin >= SPAN ? 6 : 0) + (fmax > 9 ? 3 * (fmax - 9) : 0);   /* a
                                                                             stretch; high up the neck */
        if (cost < c->bcost) { c->bcost = cost; memcpy(c->best, c->cur, sizeof c->best); }
        return;
    }
    if (placed + 6 - s < 4) return;                      /* 4 strings or more */
    if (vc_bound(c, s, last, fmin, fmax) >= c->bcost) return;
    c->cur[s] = 0;
    vc_dfs(c, s + 1, last, fmin, fmax, placed, rub);     /* muted */
    for (k = 0; k < c->ncand[s]; k++) {
        int f = c->cand[s][k], lo = fmin, hi = fmax, n = TUNE[s] + f;
        if (n <= last || (rub >> (n % 12) & 1u)) continue;   /* (no note a minor 2nd, or 9th ..., over another) */
        if (f > 0) {                                     /* fretted: within the hand's reach */
            lo = f < lo ? f : lo;
            hi = f > hi ? f : hi;
            if (hi - lo > SPAN) continue;
        }
        c->cur[s] = n;
        vc_dfs(c, s + 1, n, lo, hi, placed + 1, rub | 1u << (n + 1) % 12);
    }
    c->cur[s] = 0;
}

int strum_voicing(const int *pcs, int npcs, const int prev[6], int out[6], int hi)
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
    for (i = 0; i < 6; i++)
        for (f = 0; f <= 12; f++)
            if (TUNE[i] + f <= hi && TUNE[i] + f <= V_HI && in_chord(&c, TUNE[i] + f)) c.cand[i][c.ncand[i]++] = (int8_t)f;
    for (i = V_HI; i >= 0; i--) {
        int t = top_cost(&c, i);
        c.topmin[i] = (int16_t)(i == V_HI || t < c.topmin[i + 1] ? t : c.topmin[i + 1]);
    }
    for (rs = 0; rs < 3; rs++)                            /* the root on the low E, the A or the D string */
        for (f = 0; f <= 12; f++) {
            int root = TUNE[rs] + f;
            if (root < V_LO || root > ROOT_HI || (root - c.root + 120) % 12) continue;
            memset(c.cur, 0, sizeof c.cur);
            c.cur[rs] = root;
            vc_dfs(&c, rs + 1, root, f ? f : 99, f ? f : -99, 1, 1u << (root + 1) % 12);
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
            for (x = note + 3; x <= hi && x <= V_HI && !ok; x++) {
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

/* The keys never copy the bass's rhythm (test_keys_bass_rhythms_differ) nor the kick's (test_keys_kick_differ: the
   pattern "D.....U" over a kick on 1 and the and of 2): where a bar's strokes (ps) fall exactly on the steps the bass
   strikes, or the drum pattern's kick, a soft up stroke on the first "and" both leave free. Returns that step, or -1
   (the bar differs already, or no "and" is free). Cached per bar. */
static int bass_twin(const song_roll_t *r, const plan_block_t *b, int bar, const char *ps)
{
    static uint32_t cseed;
    static int cbar = -1, cstep = -1;
    static const char *cps;
    const lib_prog_t *pr = lib_prog_by_id((uint8_t)r->v[RF_PROG]);
    int loop = pr && pr->n ? pr->n * (pr->bars_per_chord ? pr->bars_per_chord : 1) : 4, pcs[5], np, i, bn[16];
    const lib_pattern_t *dp = lib_pattern_by_id((uint8_t)r->v[RF_PATTERN]);
    unsigned km = 0, bm = 0, dm = dp ? lib_song_hits(r, dp, D_KICK, bar) & 0xFFFFu : 0u;
    if (cbar == bar && cseed == r->seed && cps == ps) return cstep;
    cbar = bar;
    cseed = r->seed;
    cps = ps;
    cstep = -1;
    np = harmony_chord_at(r, bar, bar / loop + 1, 0, pcs);
    groove_bass_bar(r, b, bar, pcs, np, bn);
    for (i = 0; i < 16; i++) {
        if (hit_at(ps, bar, i) != '.') km |= 1u << i;
        if (bn[i]) bm |= 1u << i;
    }
    if ((km != bm && km != dm) || km == 1u) return cstep;
    for (i = 2; i < 16 && cstep < 0; i += 4)
        if (!((km >> i) & 1u) && !((dm >> i) & 1u)) cstep = i;
    return cstep;
}

/* --------------------------------------------------------------------------------------------- the strokes */
int strum_step(const song_roll_t *r, const plan_block_t *b, int bar, int step, float step_samples, const int shape[6],
               song_ev_t *out, int max)
{
    const char *ps, *dr;
    int slow, pat, fill, lo = 0, dir = 1, vel, mute = 0, thumb = 0, pinch = 0, i, k, n = 0, dark, next, low = -1, tw;
    char c;
    float gap, fac = 1.0f, span;
    uint32_t t0 = 0, sw = 0;
    if (!(b->layers & L_KEYS) || max <= 0) return 0;
    pat = strum_pattern(r, &slow);
    fill = bar == b->start + b->bars - 1 && (b->flags & PB_FILL_LAST) && !(b->flags & PB_DROP_LAST);
    ps = fill ? fill_figure(r, pat) : STRUM_PAT[pat];
    /* a fill asked for (PB_FILL_COMP, the block's last bar): its second half the drive figure's, pushing into the
       change (the bossa its thumb-and-pinch one) */
    dr = (b->flags & PB_FILL_COMP) && bar == b->start + b->bars - 1 ? STRUM_FILL_DRIVE[pat == SP_BOSSA] : 0;
    c = stroke_at(ps, dr, bar, step);
    tw = bass_twin(r, b, bar, ps);                      /* (-1 none) */
    if (dr && tw >= 8) tw = -1;
    if (c == '.' && step == tw) c = 'u';
    if (c == '.') return 0;
    for (next = step + 1; next < 16 && next != tw && stroke_at(ps, dr, bar, next) == '.'; next++) {}   /* the next
                                                                         stroke (a bar's step 0 always strikes) */
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
    if (harp(r)) {                                       /* the harp: every stroke rolled up, low to high, 12-25 ms a
                                                            string (Sam, round 2: the slow spread "too slow") */
        dir = 1;
        gap = strum_gap_ms(r, 2) * fac;
        gap = gap < 12.0f ? 12.0f : gap > 25.0f ? 25.0f : gap;
    } else {
        gap = strum_gap_ms(r, slow) * fac;
        gap = slow ? (gap < 32.0f ? 32.0f : gap > 78.0f ? 78.0f : gap)
                   : (gap < 6.0f ? 6.0f : gap > 14.0f ? 14.0f : gap);
    }
    gap *= 44.1f;                                        /* in samples */
    dark = (b->flags & PB_DARK) ? -6 : !(b->layers & (L_KICK | L_LEAD)) ? 6 : 0;
    if (step & 1) {                                      /* an odd 16th swings as the bass does */
        float s = ((float)r->v[RF_SWING] * 0.001f - 0.5f) * 2.0f * step_samples;
        sw = s > 0.0f ? (uint32_t)(s + 0.5f) : 0u;
    }
    t0 = sw + harmony_mix(r->seed, (uint32_t)bar, (uint32_t)step, 60u) % 133u;   /* the hand a little late: 0-3 ms */
    for (i = 0; i < 6; i++)
        if (shape[i]) { low = i; break; }
    if (low < 0) return 0;
    if (step == tw) {                                    /* the twin's up stroke must sound: on a shape with no string
                                                            from its lo up (a thinned shape), its top string */
        for (i = 5; i > 0 && !shape[i]; i--) {}
        if (i < lo) lo = i;
    }
    for (k = 0; k < 6 && n < max; k++) {
        int s = dir > 0 ? k : 5 - k, v;
        uint32_t at, end;
        if (!shape[s] || (thumb ? s != low && (!pinch || s < lo) : s < lo)) continue;   /* the thumb: the lowest */
        at = t0 + (uint32_t)((float)n * gap) +
             harmony_mix(r->seed, (uint32_t)bar, (uint32_t)step, 70u + (uint32_t)s) % 45u;   /* about 1 ms */
        end = (uint32_t)span > 2u ? (uint32_t)span - 2u : 0u;   /* by the next stroke */
        if (at + 1u >= end) break;
        v = vel + (int)(harmony_mix(r->seed, (uint32_t)bar, (uint32_t)step, 80u + (uint32_t)s) % 9u) - 4;
        if (dir < 0) v -= v * n / 25;                    /* an up stroke fades as it goes (4% a string) */
        out[n].voice = V_CHORD;
        out[n].note = (uint8_t)shape[s];
        out[n].vel = (uint8_t)clampi(v + dark, 25, 110);
        out[n].pad = (uint8_t)(mute | (s + 1) << 1);    /* bit 0 a chuck; bits 1-3 the string, 1-6 */
        out[n].delay = at;
        out[n].len = end - at;
        if (mute && out[n].len > 2646u) out[n].len = 2646u;   /* a chuck: 60 ms */
        n++;
    }
    return n;
}
