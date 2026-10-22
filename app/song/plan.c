/* SPDX-License-Identifier: MPL-2.0 */
/* The plan. Built on a grid of 4-bar slots: first the map's skeleton (intro, entry, middle event, ending), then
   Motion's optional moves (hats out, lead and candy toggles, 1-bar drops), then slots merge into blocks of 4 or 8
   bars along the 8-bar phrase grid of the middle.

   Layer-change budget: at most 2 of keys/kick/snare/hats/bass/lead/candy change at a boundary. The skeleton's
   drum entries and exits (entry, break, Drift's comings and goings) and the keys outro may need more; at those
   boundaries nothing optional changes. Elsewhere optional moves only use what the skeleton leaves of the 2. */
#include <string.h>
#include "rng.h"
#include "plan.h"

#define MAXS 24                                   /* slots: RF_BARS / 4 */
#define DRUMS (L_KICK | L_SNARE | L_HATS)
#define FULL (L_KEYS | DRUMS | L_BASS)

enum { S_INTRO = 1, S_ENTRY = 2, S_OUTRO = 4, S_BREAK = 8, S_KICKOUT = 16, S_DRUMOUT = 32, S_HATSOUT = 64 };

typedef struct {
    int n;                    /* slots */
    int ni, ms, oe;           /* first entry slot, first middle slot, first outro slot (n if none) */
    int nu;                   /* middle units: 8 bars from ms, the last may be 4 */
    uint8_t skel[MAXS];       /* skeleton layers (keys, drums, bass) */
    uint8_t kind[MAXS];       /* S_* */
    uint8_t lead[MAXS], candy[MAXS];
} grid_t;

static int bits(int x)
{
    int n = 0;
    for (; x; x >>= 1) n += x & 1;
    return n;
}

static int unit_start(const grid_t *g, int u) { return g->ms + 2 * u; }
static int unit_end(const grid_t *g, int u)
{
    int e = g->ms + 2 * u + 2;
    return e > g->oe ? g->oe : e;
}
static int unit_of(const grid_t *g, int s) { return (s - g->ms) / 2; }
static int chance(rng_t *r, int pct) { return rng_int(r, 100) < pct; }

/* Optional changes a boundary (slot s-1 -> s) can still take, given the skeleton and the lead already placed. */
static int room(const grid_t *g, int s)
{
    int a = g->skel[s - 1], b = g->skel[s];
    if (s >= g->oe) return 0;                                           /* the outro is mandated */
    if (!(a & DRUMS) != !(b & DRUMS)) return 0;                         /* drums entering or leaving */
    if (g->kind[s - 1] & g->kind[s] & (S_BREAK | S_KICKOUT)) return 0;  /* mandated blocks stay whole */
    return 2 - bits(a ^ b) - (g->lead[s] != g->lead[s - 1]);
}

/* Skeleton: intro, drum entry, middle event, outro. sk: the skeleton rng. */
static void skeleton(grid_t *g, const lib_map_t *m, rng_t *sk)
{
    int intro = m->entry == EN_FROM_TOP ? 0 : m->intro_bars / 4;
    int hj = m->entry == EN_TOGETHER ? 1 + rng_int(sk, 2) : (m->entry == EN_SNARE_FIRST ? 2 : 0);
    int outro = m->ending == END_KEYS_OUTRO ? 1 + rng_int(sk, 2) : 0;
    int s, u;

    if (m->sparse && hj > 1) hj = 1;                                    /* Drift: keep the drums' first stay short */
    if (m->middle != MID_NONE && m->middle != MID_HATS_ONLY && g->n - intro - hj - outro < 5) {
        if (hj > 1 && m->entry == EN_TOGETHER) hj = 1;                  /* room for the middle event */
        outro = outro ? 1 : 0;
    }
    g->ni = intro;
    g->ms = intro + hj;
    g->oe = g->n - outro;
    g->nu = (g->oe - g->ms + 1) / 2;
    for (s = 0; s < g->n; s++) {
        if (s < intro) { g->skel[s] = L_KEYS; g->kind[s] = S_INTRO; }
        else if (s < g->ms) {
            g->skel[s] = m->entry == EN_SNARE_FIRST ? L_KEYS | L_SNARE | L_HATS : L_KEYS | L_KICK | L_SNARE | L_BASS;
            g->kind[s] = S_ENTRY;
        } else if (s < g->oe) { g->skel[s] = FULL; g->kind[s] = 0; }
        else { g->skel[s] = L_KEYS; g->kind[s] = S_OUTRO; }
    }

    if (m->middle == MID_BREAK) {
        /* one 4-8 bar drum break starting 43-65% through, after at least one full slot, before at least one */
        int cs[MAXS * 2], cl[MAXS * 2], cw[MAXS * 2], nc = 0, tot = 0, x, i, L;
        int want = rng_int(sk, 3) ? 1 : 2;                              /* 4 bars twice as often as 8 */
        for (L = want; nc == 0 && L >= 1; L--)
            for (s = g->ms + 1; s + L <= g->oe - 1; s++)
                if (100 * 4 * s >= 43 * 4 * g->n && 100 * 4 * s <= 65 * 4 * g->n) {
                    cs[nc] = s; cl[nc] = L; cw[nc] = (s - g->ms) % 2 ? 1 : 3;   /* prefer the phrase grid */
                    tot += cw[nc++];
                }
        if (nc) {
            int keep_bass = chance(sk, 50);
            x = rng_int(sk, tot);
            for (i = 0; i < nc - 1 && x >= cw[i]; i++) x -= cw[i];
            for (s = cs[i]; s < cs[i] + cl[i]; s++) {
                g->skel[s] = keep_bass ? L_KEYS | L_BASS : L_KEYS;
                g->kind[s] = S_BREAK;
            }
        }
    } else if (m->middle == MID_KICK_OUT) {
        /* one 8-bar block with kick and bass out: not the first unit, ideally not the last, so the full groove
           returns before the ending */
        int c[MAXS], nc = 0, pass;
        for (pass = 0; pass < 2 && nc == 0; pass++)
            for (u = 1; u < g->nu - (pass == 0); u++)
                if (unit_end(g, u) - unit_start(g, u) == 2) c[nc++] = u;
        if (nc) {
            u = c[rng_int(sk, nc)];
            for (s = unit_start(g, u); s < unit_end(g, u); s++) {
                g->skel[s] = L_KEYS | L_SNARE | L_HATS;
                g->kind[s] = S_KICKOUT;
            }
        }
    } else if (m->middle == MID_HATS_ONLY && g->nu > 1) {
        /* one small hat drop-out: the whole 8-bar unit, or its first or second 4 bars */
        int a, e, k;
        u = 1 + rng_int(sk, g->nu - 1);
        a = unit_start(g, u);
        e = unit_end(g, u);
        k = rng_int(sk, 3);
        for (s = a; s < e; s++)
            if (e - a == 1 || k == 0 || (k == 1 && s == a) || (k == 2 && s == e - 1)) {
                g->skel[s] &= (uint8_t)~L_HATS;
                g->kind[s] = S_HATSOUT;
            }
    }

    if (m->sparse) {
        /* Drift: drums come and go; drums in at most 60% of the non-intro, non-outro slots. The first middle
           slot keeps them (the hats' entry); drop whole units, apart from each other when possible. */
        int M = g->oe - g->ni, D = 6 * M / 10, have = 0, order[MAXS], pass, i, j;
        for (s = g->ni; s < g->oe; s++) have += (g->skel[s] & DRUMS) != 0;
        for (i = 0; i < g->nu; i++) order[i] = i;
        for (i = g->nu - 1; i > 0; i--) { j = rng_int(sk, i + 1); u = order[i]; order[i] = order[j]; order[j] = u; }
        for (pass = 0; pass < 2 && have > D; pass++)
            for (i = 0; i < g->nu && have > D; i++) {
                int a, e;
                u = order[i];
                a = unit_start(g, u);
                e = unit_end(g, u);
                if (u == 0) a++;
                if (a >= e || (g->skel[a] & DRUMS) == 0) continue;
                if (pass == 0 && ((a > g->ms && !(g->skel[a - 1] & DRUMS)) || (e < g->oe && !(g->skel[e] & DRUMS))))
                    continue;
                for (s = a; s < e; s++) {
                    if (g->skel[s] & DRUMS) have--;
                    g->skel[s] &= (uint8_t)~DRUMS;
                    g->kind[s] = S_DRUMOUT;
                }
            }
    }
}

/* Motion's moves on the layers: hats out, lead toggles, ear candy. mo: the Motion rng; motion 0-100. */
static void moves(grid_t *g, rng_t *mo, int motion, int hats_ok)
{
    int u, s, i, j, t;
    int nl, lead_u[MAXS], pick[MAXS], f, need;

    /* hats out for 4-8 bars: full-groove units after the first, apart from other hat drop-outs and away from the
       kick-out block (its snare + hats carry) */
    for (u = 1; u < g->nu; u++) {
        int a = unit_start(g, u), e = unit_end(g, u), ok = 1, roll = chance(mo, 30 * motion / 100), half = rng_int(mo, 3);
        for (s = a; s < e; s++) if (g->skel[s] != FULL || g->kind[s]) ok = 0;
        if (g->kind[a - 1] & (S_KICKOUT | S_HATSOUT)) ok = 0;
        if (e < g->oe && (g->kind[e] & (S_KICKOUT | S_HATSOUT))) ok = 0;
        if (!ok || !roll || !hats_ok) continue;
        for (s = a; s < e; s++)
            if (e - a == 1 || half == 0 || (half == 1 && s == a) || (half == 2 && s == e - 1)) {
                g->skel[s] &= (uint8_t)~L_HATS;
                g->kind[s] = S_HATSOUT;
            }
    }

    /* lead in about half the middle units, never the first: one run at Motion 0, scattered more with Motion;
       it changes at unit starts, or 4 bars in when the unit's start is a mandated move */
    memset(lead_u, 0, sizeof lead_u);
    nl = g->nu / 2;
    if (nl > g->nu - 1) nl = g->nu - 1;
    for (i = 0; i < g->nu - 1; i++) pick[i] = i + 1;
    t = rng_int(mo, 100);
    if (nl > 0 && t < motion) {
        for (i = 0; i < nl; i++) {
            j = i + rng_int(mo, g->nu - 1 - i);
            u = pick[i]; pick[i] = pick[j]; pick[j] = u;
            lead_u[pick[i]] = 1;
        }
    } else if (nl > 0) {
        int a = 1 + rng_int(mo, g->nu - nl);
        for (u = a; u < a + nl; u++) lead_u[u] = 1;
    }
    memset(g->lead, 0, sizeof g->lead);
    for (s = g->ms; s < g->oe; s++) {
        int want = lead_u[unit_of(g, s)];
        g->lead[s] = s > 0 ? g->lead[s - 1] : 0;
        if (s > 0 && want != g->lead[s] && room(g, s) > 0) g->lead[s] = (uint8_t)want;   /* blocked: 4 bars later */
    }

    /* make sure the lead is heard: if dice and mandated moves left it under a quarter of the middle (or 8 bars),
       place one run instead, starting and ending only where a boundary has room (it may carry through a break) */
    need = (g->oe - g->ni + 3) / 4;
    if (need < 2) need = 2;
    for (s = g->ms, t = 0; s < g->oe; s++) t += g->lead[s];
    if (t < need) {
        int c[MAXS], nc = 0, aligned;
        memset(g->lead, 0, sizeof g->lead);
        for (aligned = 1; aligned >= 0 && nc == 0; aligned--)
            for (s = g->ms > 0 ? g->ms : 1; s + need <= g->oe; s++)
                if (room(g, s) > 0 && (!aligned || (s - g->ms) % 2 == 0)) c[nc++] = s;
        if (nc) {
            int a = c[rng_int(mo, nc)], e = a + need;
            while (e < g->oe && room(g, e) <= 0) e++;
            for (s = a; s < e; s++) g->lead[s] = 1;
        }
    }

    /* ear candy, more of it with Motion: a whole unit, or failing that one of its halves (a map's drum moves leave
       no room at many unit edges), never the unit of the lead's first statement, never back to back, only where
       both of its edges have room left */
    memset(g->candy, 0, sizeof g->candy);
    f = -1;
    for (s = 0; s < g->n && f < 0; s++) if (g->lead[s]) f = unit_of(g, s);
    for (u = 0; u < g->nu; u++) {
        int a = unit_start(g, u), e = unit_end(g, u), k;
        static const int8_t TRY[3][2] = {{0, 0}, {0, -1}, {1, 0}};   /* offsets of [start, end) from [a, e) */
        if (!motion || !chance(mo, 25 + 55 * motion / 100) || u == f || a == 0) continue;
        for (k = 0; k < (e - a == 2 ? 3 : 1); k++) {
            int x = a + TRY[k][0], y = e + TRY[k][1];
            if (g->candy[x - 1] || room(g, x) < 1 || (y < g->oe && room(g, y) < 1)) continue;
            for (s = x; s < y; s++) g->candy[s] = 1;
            break;
        }
    }
}

void plan_build(const song_roll_t *r, song_plan_t *p)
{
    static grid_t g;   /* static: no stack burden on the device */
    const lib_map_t *m = lib_map_by_id((uint8_t)r->v[RF_MAP]);
    int bars = r->v[RF_BARS], motion = r->v[RF_MOTION], s, i, run = 0, steady, drops_ok;
    rng_t sk, mo;

    if (!m) m = &LIB_MAPS[0];
    if (bars < 36) bars = 36;
    if (bars > 76) bars = 76;
    bars &= ~3;
    if (motion < 0) motion = 0;
    if (motion > 100) motion = 100;
    memset(&g, 0, sizeof g);
    memset(p, 0, sizeof *p);
    g.n = bars / 4;
    rng_seed(&sk, r->seed ^ 0x504C414Eu, 100);   /* skeleton dice: independent of Motion */
    rng_seed(&mo, r->seed ^ 0x4D4F5645u, 100);   /* Motion dice */
    skeleton(&g, m, &sk);
    /* R11: the skeleton binds over Motion. Slow bloom's drums never drop once the kick is in (no hats-out, no
       drops); Straight in has only small hat drop-outs (no 1-bar drops). */
    steady = m->middle == MID_NONE && !m->sparse;
    moves(&g, &mo, motion, !steady);
    drops_ok = !steady && m->middle != MID_HATS_ONLY;

    /* merge slots into blocks: a new block on a layer or section change, at each 8-bar unit of the middle, at the
       entry, middle and outro starts, and after 8 bars */
    for (s = 0; s < g.n; s++) {
        uint8_t L = (uint8_t)(g.skel[s] | (g.lead[s] ? L_LEAD : 0) | (g.candy[s] ? L_CANDY : 0));
        uint8_t sec = (g.kind[s] & S_INTRO) ? SEC_INTRO : (g.kind[s] & S_OUTRO) ? SEC_OUTRO
                    : ((g.kind[s] & (S_BREAK | S_KICKOUT)) || !(L & (L_KICK | L_SNARE))) ? SEC_BREAK
                    : (L & L_LEAD) ? SEC_LEAD : SEC_GROOVE;   /* R12: break = drums out or thinned */
        plan_block_t *b = p->n ? &p->b[p->n - 1] : 0;
        int cut = !b || b->layers != L || b->section != sec || run == 2 || s == g.ni || s == g.ms || s == g.oe ||
                  (s > g.ms && s < g.oe && (s - g.ms) % 2 == 0 && !(g.kind[s - 1] & g.kind[s] & S_BREAK));
        if (cut) {
            b = &p->b[p->n++];
            b->start = (uint8_t)(4 * s);
            b->bars = 0;
            b->layers = L;
            b->section = sec;
            b->flags = 0;
            run = 0;
        }
        b->bars += 4;
        run++;
    }
    p->bars = (uint8_t)bars;

    for (i = 0; i < p->n; i++) {
        plan_block_t *b = &p->b[i], *nx = i + 1 < p->n ? &p->b[i + 1] : 0;
        int drop = chance(&mo, 40 * motion / 100);
        if (b->section == SEC_INTRO || b->section == SEC_OUTRO) b->flags |= PB_DARK;
        if (!nx) continue;
        if (!(b->layers & DRUMS) && (nx->layers & DRUMS)) b->flags |= PB_SWELL;
        if (!(b->layers & (L_KICK | L_SNARE))) continue;
        if (drop && drops_ok && b->bars == 8 && ((b->layers ^ nx->layers) & (DRUMS | L_BASS)) == 0) b->flags |= PB_DROP_LAST;
        else if ((b->layers ^ nx->layers) & (L_KICK | L_SNARE | L_BASS)) b->flags |= PB_FILL_LAST;
    }
    if (m->ending == END_FADE) p->b[p->n - 1].flags |= PB_FADE;
}

const plan_block_t *plan_at(const song_plan_t *p, int bar)
{
    int i;
    if (bar < 0 || bar >= p->bars) return 0;
    for (i = 0; i < p->n; i++)
        if (bar < p->b[i].start + p->b[i].bars) return &p->b[i];
    return 0;
}

const char *plan_section_name(int section)
{
    static const char *const names[] = {"intro", "groove", "lead", "break", "outro"};
    return section >= 0 && section <= SEC_OUTRO ? names[section] : "?";
}
