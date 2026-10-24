/* SPDX-License-Identifier: MPL-2.0 */
/* The sustained lead's motif and 8-bar question-and-answer plan (see phrase.h). Streams 180 (the motif) and 181 (a
   pass's nudges and cadences). Everything is rebuilt from (seed, block, pass) on each call: nothing is stored. */
#include "phrase.h"
#include "rng.h"

static const uint8_t vocab[5] = { 2, 3, 4, 6, 8 };   /* 8ths: quarter, dotted quarter, half, dotted half, whole */

static int iabs(int x) { return x < 0 ? -x : x; }
static int mod7(int d) { return ((d % 7) + 7) % 7; }

void phrase_motif(uint32_t seed, motif_t2 *m)
{
    static const uint8_t nw[4] = { 2, 4, 3, 1 };   /* 3-6 notes over 2 bars */
    static const uint8_t sw[3] = { 4, 3, 2 };      /* the first note on the anchor, its 3rd or its 5th */
    rng_t g;
    int i, n, f, bars, tr, deg, force = 0, leapt = 0;
    uint8_t li[8], gap[8];
    rng_seed(&g, seed, 180);
    for (i = 0; i < 8; i++) { m->at[i] = m->len[i] = m->orn[i] = 0; m->step[i] = 0; }
    /* Rhythm: a 1-bar motif (3 notes) always starts off beat 1, so it can repeat without two beat-1 bars in a row; a
       2-bar motif starts on 1 half the time (about 65% off beat 1 overall). */
    bars = rng_int(&g, 10) < 3 ? 1 : 2;
    f = (bars == 2 && rng_int(&g, 2) == 0) ? 0 : 1 + rng_int(&g, 2);
    n = bars == 1 ? 3 : 3 + rng_weighted(&g, nw, 4);
    for (tr = 0; tr < 17; tr++) {
        int room, k, t, ok = 1;
        for (i = 0; i < n; i++) li[i] = gap[i] = 0;
        if (tr == 16) { f = 1; li[0] = 1; }   /* a last resort: a dotted quarter and quarters from the "and" of 1 */
        room = bars * 8 - f - 2 * n;
        for (k = 0; tr < 16 && k < 24 && room > 0; k++) {   /* lengthen notes (and now and then a breath) */
            i = rng_int(&g, n);
            if (i > 0 && rng_int(&g, 5) == 0) { gap[i]++; room--; continue; }
            if (li[i] < 4 && vocab[li[i] + 1] - vocab[li[i]] <= room) {
                room -= vocab[li[i] + 1] - vocab[li[i]];
                li[i]++;
            }
        }
        for (i = 0, t = f; i < n; i++) {
            t += gap[i];
            m->at[i] = (uint8_t)t;
            m->len[i] = vocab[li[i]];
            t += vocab[li[i]];
            if (f == 0 && m->at[i] == 8) ok = 0;   /* beat 1 rests in bar 2 when bar 1 starts on it */
            if (m->at[i] < 8 && m->at[i] > (f == 0 ? 5 : 6)) ok = 0;   /* room in bar 3 for the cadence's pickup */
        }
        if (ok) break;
    }
    m->n = (uint8_t)n;
    m->bars = (uint8_t)bars;
    /* Contour: mostly steps, at most one leap (a 4th-6th) and then a step back; pulled toward the anchor's octave. */
    m->step[0] = (int8_t)(2 * rng_weighted(&g, sw, 3));
    deg = m->step[0];
    for (i = 1; i < n; i++) {
        int s, up = deg > 4 ? 0 : deg < 0 ? 1 : rng_int(&g, 2);
        if (force) { s = force; force = 0; }
        else if (!leapt && i < n - 1 && rng_int(&g, 100) < 22) {
            s = 3 + rng_int(&g, 3);
            if (!(deg < 2 || (deg <= 4 && rng_int(&g, 2)))) s = -s;
            leapt = 1;
            force = s > 0 ? -1 : 1;
        } else {
            int r = rng_int(&g, 10);
            s = r == 0 ? 0 : r <= 6 ? 1 : 2;
            if (!up) s = -s;
        }
        m->step[i] = (int8_t)s;
        deg += s;
    }
    /* Ornaments: off beats 1 and 3, reached and left by step (passing and neighbor notes); low motion drops them. */
    for (i = 1; i < n; i++)
        m->orn[i] = (uint8_t)(m->at[i] % 4 != 0 && iabs(m->step[i]) <= 1 && i < n - 1 && iabs(m->step[i + 1]) <= 1);
}

/* A note on the phrase's 16th timeline (8 bars = 128). */
typedef struct { int t, len, deg, role, orn; } pn_t;

enum { N_STEP, N_LAST, N_INV, N_DELAY };

/* Whether a nudge changes the bar; up (bar 3, rising into the question) also keeps an inversion from lowering
   the bar's high point. */
static int nudge_ok(int kind, int up, int lim, const pn_t *b, int n)
{
    int i, h = n / 2, hi = -99, ihi = -99, moved = 0;
    if (n == 0) return 0;
    if (kind == N_INV) {
        if (n < 2) return 0;
        for (i = 0; i < n; i++) {
            int d = i < h ? b[i].deg : 2 * b[h - 1].deg - b[i].deg;
            if (b[i].deg > hi) hi = b[i].deg;
            if (d > ihi) ihi = d;
            if (d != b[i].deg) moved = 1;
        }
        return moved && (!up || ihi >= hi);
    }
    if (kind == N_DELAY) {
        for (i = 0; i < n; i++) if (b[i].t + 2 <= lim && b[i].len >= 4) return 1;
        return 0;
    }
    return 1;
}

/* One nudge to a bar's notes. up: bar 3 rises into the question (its highest note, or its last interval, goes up);
   bar 7 moves either way. A delay lands by 16th lim, leaving room for the cadence's pickup. Ornaments are re-tagged
   after: off beats 1 and 3, reached and left by step inside the bar. */
static void nudge(rng_t *g, int kind, int up, int lim, pn_t *b, int n)
{
    int i, h = n / 2, d = up || rng_int(g, 2) ? 1 : -1;
    switch (kind) {
    case N_STEP:
        if (up) {
            for (h = 0, i = 1; i < n; i++) if (b[i].deg > b[h].deg) h = i;
            b[h].deg++;
        } else {
            b[n >= 2 ? rng_int(g, n - 1) : 0].deg += d;
        }
        break;
    case N_LAST: b[n - 1].deg += 2 * d; break;
    case N_INV:
        for (i = h; i < n; i++) b[i].deg = 2 * b[h - 1].deg - b[i].deg;
        break;
    default: {
        int c[8], k = 0;
        for (i = 0; i < n; i++) if (b[i].t + 2 <= lim && b[i].len >= 4) c[k++] = i;
        i = c[rng_int(g, k)];
        b[i].t += 2;   /* delayed an 8th, ending where it did */
        b[i].len -= 2;
    }
    }
    for (i = 0; i < n; i++)
        b[i].orn = i > 0 && i < n - 1 && b[i].t % 8 != 0 && iabs(b[i].deg - b[i - 1].deg) <= 1 &&
                   iabs(b[i + 1].deg - b[i].deg) <= 1;
}

#define PMAX 48   /* notes in a plan; 8 bars use about 32 at most */

static int add(pn_t *p, int n, int t, int len, int deg, int role, int orn)
{
    if (n >= PMAX) return n;
    p[n].t = t; p[n].len = len; p[n].deg = deg; p[n].role = role; p[n].orn = orn;
    return n + 1;
}

/* The pickup into a target: a step from it. After a leap into the bar's last note it steps back the other way when
   that is a step from the target; else it comes from the near side (no new leap), or from past the target when the
   target is a leap away (a leap, then a step back); dir: the side when there's no leap to answer. */
static int approach(int last, int ld, int target, int dir)
{
    int dl = last - target;
    if (ld && iabs(target - (last - ld)) == 1) return last - ld;
    if (dl == 0) return target - dir;
    if (dl <= 3 && dl >= -3) return target + (dl > 0 ? 1 : -1);
    return target - (dl > 0 ? 1 : -1);
}

/* The cadence at bar t0: the target sounds from beat 1 to beat 3, then rests. It is struck on beat 1 after a
   pickup (ap) at the end of the bar before, or (tie) tied in from that bar's end so beat 1 rests. */
static int cadence(rng_t *g, pn_t *p, int n, int t0, int target, int ap, int tie, int role)
{
    int lt = p[n - 1].t, c;
    if (tie) {
        c = lt <= t0 - 8 && rng_int(g, 2) ? t0 - 6 : t0 - 4;
        n = add(p, n, c, 2, ap, PR_CLOSE, 0);
        return add(p, n, c + 2, t0 + 8 - (c + 2), target, role, 0);
    }
    c = lt <= t0 - 6 && rng_int(g, 2) ? t0 - 4 : t0 - 2;
    n = add(p, n, c, t0 - c, ap, PR_CLOSE, 0);
    return add(p, n, t0, 8, target, role, 0);
}

/* Leaps (3+ steps, legato from the note before) not answered by a step back, in a bar's notes after a note of
   degree prev that sounds until pend (bar time); a leap into the last note is left to the cadence's pickup. */
static int unanswered(const pn_t *b, int n, int prev, int pend)
{
    int i, k = 0;
    for (i = 0; i + 1 < n; i++) {
        int d = b[i].deg - (i ? b[i - 1].deg : prev), nd = b[i + 1].deg - b[i].deg;
        int legato = i ? b[i - 1].t + b[i - 1].len >= b[i].t : pend >= b[i].t;
        if (legato && iabs(d) >= 3 && !(d > 0 ? nd == -1 : nd == 1)) k++;
    }
    return k;
}

/* Places a bar's notes (t within the bar or the motif) from t0, their degrees shifted. */
static int place(pn_t *p, int n, const pn_t *b, int nb, int t0, int shift, int role)
{
    int i;
    for (i = 0; i < nb; i++) n = add(p, n, t0 + b[i].t, b[i].len, b[i].deg + shift, role, b[i].orn);
    return n;
}

/* Monophonic: a note held past the next onset (bar 3's tie into the cadence) ends there. */
static void clip(pn_t *p, int n)
{
    int i;
    for (i = 0; i + 1 < n; i++)
        if (p[i].t + p[i].len > p[i + 1].t) p[i].len = p[i + 1].t - p[i].t;
}

static int build(uint32_t seed, int block, int pass, pn_t *p)
{
    motif_t2 m;
    pn_t mo[8], b1[8], v[8];
    rng_t g;
    int i, n = 0, nm, n1 = 0, deg = 0, k3 = -1, b1ok, lim, last, q = 0, a;
    phrase_motif(seed, &m);
    for (i = 0, nm = m.n; i < nm; i++) {
        deg += m.step[i];
        mo[i].t = m.at[i] * 2; mo[i].len = m.len[i] * 2; mo[i].deg = deg; mo[i].role = PR_MOTIF; mo[i].orn = m.orn[i];
        if (mo[i].t < 16) b1[n1++] = mo[i];
    }
    b1ok = mo[0].t != 0;   /* bar 1 rests on beat 1, so a cadence may strike on beat 1 */
    lim = b1ok ? 12 : 10;  /* bars 3 and 7's last onset: room for a pickup (and a tie in when beat 1 rests) */
    rng_seed(&g, seed ^ ((uint32_t)block * 0x27D4EB2Fu) ^ ((uint32_t)pass * 0x165667B1u), 181);
    for (i = 0; i < 2; i++) {   /* the question (bars 1-4), then the answer (bars 5-8) */
        int t0 = i * 64, j, tie, k, tries, pend, ld, d;
        n = place(p, n, mo, nm, t0, 0, PR_MOTIF);
        if (m.bars == 1) n = place(p, n, mo, nm, t0 + 16, 1, PR_MOTIF);   /* a 1-bar motif again, a step higher */
        /* a breath before bar 3 restarts the motif with a leap (the same in both halves: bars 5-6 = bars 1-2) */
        pend = p[n - 1].t + p[n - 1].len - (t0 + 32);
        if (pend >= b1[0].t && iabs(b1[0].deg - p[n - 1].deg) >= 3) {
            p[n - 1].len -= pend - (b1[0].t - 2);
            pend = b1[0].t - 2;
        }
        /* bar 3 (7): one nudge, a different kind in the answer; up to 8 draws for one that leaves no leap
           unanswered */
        for (tries = 0; tries < 8; tries++) {
            do k = rng_int(&g, 4); while ((i && k == k3) || !nudge_ok(k, !i, lim, b1, n1));
            for (j = 0; j < n1; j++) v[j] = b1[j];
            nudge(&g, k, !i, lim, v, n1);
            if (!unanswered(v, n1, p[n - 1].deg, pend)) break;
        }
        if (!i) k3 = k;
        n = place(p, n, v, n1, t0 + 32, 0, PR_VARIED);
        last = p[n - 1].deg;
        d = n1 > 1 ? last - p[n - 2].deg : 0;
        ld = n1 > 1 && iabs(d) >= 3 && p[n - 2].t + p[n - 2].len >= p[n - 1].t ? (d > 0 ? 1 : -1) : 0;
        tie = !b1ok || (p[n - 1].t <= t0 + 42 && rng_int(&g, 3) == 0);   /* beat 1 must rest, or now and then */
        if (!i) {   /* the question: the nearest 3rd or 5th a step or two up (else just below), by step */
            static const int8_t near[6] = { 2, 1, 3, 0, -1, 4 };
            q = -99;
            if (ld) {   /* after a leap, one the pickup can step back toward */
                for (j = 1; j >= -1 && q == -99; j -= 2)
                    if (mod7(last - ld + j) == 2 || mod7(last - ld + j) == 4) q = last - ld + j;
            }
            for (j = 0; j < 6 && q == -99; j++)
                if (mod7(last + near[j]) == 2 || mod7(last + near[j]) == 4) q = last + near[j];
            n = cadence(&g, p, n, t0 + 48, q, approach(last, ld, q, 1), tie, PR_QUESTION);
        } else {    /* the answer settles by step down to the anchor degree nearest a step below, under the question */
            a = last - 1;
            a = a - mod7(a) + (mod7(a) >= 4 ? 7 : 0);
            if (a >= q && last - (a - 7) <= 6) a -= 7;   /* under the question, unless that means a leap past a 6th */
            n = cadence(&g, p, n, t0 + 48, a, approach(last, ld, a, -1), tie, PR_ARRIVAL);
        }
    }
    clip(p, n);
    return n;
}

int phrase_bar(uint32_t seed, int block, int pass, int bar_in_phrase, int phrase_bars, int last_block,
               phrase_note_t *out, int max)
{
    pn_t p[PMAX];
    int n, i, k = 0, t0;
    if (bar_in_phrase < 0 || bar_in_phrase >= (phrase_bars < 8 ? 4 : 8)) return 0;
    if (phrase_bars < 8 && last_block) bar_in_phrase += 4;
    n = build(seed, block, pass, p);
    t0 = bar_in_phrase * 16;
    for (i = 0; i < n && k < max; i++) {
        if (p[i].t < t0 || p[i].t >= t0 + 16) continue;
        out[k].on = 1;
        out[k].at = (uint8_t)(p[i].t - t0);
        out[k].len = (uint8_t)p[i].len;
        out[k].deg = (int8_t)p[i].deg;
        out[k].role = (uint8_t)p[i].role;
        out[k].orn = (uint8_t)p[i].orn;
        k++;
    }
    return k;
}
