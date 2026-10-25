/* SPDX-License-Identifier: MPL-2.0 */
#include <stdint.h>
#include "lead.h"
#include "harmony.h"
#include "library.h"
#include "theory.h"
#include "rng.h"
#include "phrase.h"

typedef struct {                  /* what one bar's planning reads */
    const song_roll_t *r;
    const int *keys, *pcs;
    int np;
    unsigned bass;                /* pitch classes the bass strikes in the bar */
    int lo, hi;                   /* the lead's range this bar (a 4th over percussive keys' top; a pad's place) */
    int lo_x;                     /* the floor for a note that sounds on into the next chord */
    int shift;                    /* the keys' octave shift (a pad with or above the lead) */
    int can_push;                 /* the next bar's chord is known (no request lands on it) */
    int pad;                      /* the keys sustain (a pad, the organ) */
} ctx_t;

static int is_ct(int note, const int *pcs, int np)
{
    int j;
    for (j = 0; j < np; j++)
        if ((note - pcs[j] + 120) % 12 == 0) return 1;
    return 0;
}

/* a chord tone or a scale note, never a semitone from a chord tone or a note the bass strikes (R25: the maj7 over
   the root rubs), and not an avoid note */
static int legal(const song_roll_t *r, int note, const int *pcs, int np, unsigned bass)
{
    int j, pc = note % 12;
    if (th_is_avoid(r->v[RF_KEY], r->v[RF_MODE], pcs, np, note)) return 0;
    if (bass & (1u << ((pc + 1) % 12) | 1u << ((pc + 11) % 12))) return 0;
    for (j = 0; j < np; j++) {
        int d = (note - pcs[j] + 120) % 12;
        if (d == 1 || d == 11) return 0;
    }
    return is_ct(note, pcs, np) || th_in_scale(r->v[RF_KEY], r->v[RF_MODE], note);
}

/* the skeleton's range: E4-A5 as the spike sang it, raised to the floor, a pad placed above keeping it to C4-C5 */
static void skel_range(const ctx_t *c, int floor, int *lo, int *hi)
{
    *lo = c->hi <= 72 ? 60 : floor > 67 ? floor : 67;
    *hi = c->hi <= 72 ? 72 : *lo + 12 > 81 ? *lo + 12 : 81;
    if (*hi > c->hi) *hi = c->hi;
}

/* the chord tone nearest `from`, root least, within a 5th of the last skeleton note `prev` where one is */
static int skel_near(const ctx_t *c, const int *pcs, int np, int from, int prev, int floor)
{
    int best = -1, bc = 1 << 30, note, j, lo, hi, pass;
    for (pass = 0; pass < 3 && best < 0; pass++) {      /* the usual range, the lead's whole range, then a leap */
        skel_range(c, floor, &lo, &hi);
        if (pass == 1) { lo = floor > 60 ? floor : 60; hi = c->hi; }
        for (note = lo; note <= hi; note++)
            for (j = 0; j < np; j++)
                if ((note - pcs[j] + 120) % 12 == 0 && legal(c->r, note, pcs, np, c->bass) &&
                    (pass == 2 || !prev || (note > prev ? note - prev : prev - note) <= 7)) {
                int iv = (pcs[j] - pcs[0] + 12) % 12, cost = (note > from ? note - from : from - note) * 4;
                if (iv == 0) cost += 6;
                if (iv == 7) cost += 3;
                if (c->keys[3] && note == c->keys[3] + 12) cost += 4;
                if (cost < bc) { bc = cost; best = note; }
            }
    }
    return best;
}

static int skel_from(const ctx_t *c, const int *pcs, int np, int from, int floor)
{
    return skel_near(c, pcs, np, from, from, floor);
}

static int skel(const ctx_t *c, const int *pcs, int np, int from) { return skel_from(c, pcs, np, from, c->lo); }

static int place(int pc, int lo, int hi, int near)          /* pc's octave in [lo, hi] nearest `near`; 0 if none */
{
    int best = 0, note;
    for (note = lo; note <= hi; note++)
        if (note % 12 == pc &&
            (!best || (note > near ? note - near : near - note) < (best > near ? best - near : near - best)))
            best = note;
    return best;
}

static void put(lead_state_t *ls, int at, int note, int len)
{
    if (at < 0 || at > 15 || note <= 0) return;
    ls->on[at] = note;
    ls->len[at] = len;
}

static void land(lead_state_t *ls, int at, int note, int len)   /* the skeleton note, the arpeggio out of its way */
{
    int i;
    for (i = at < 0 ? 0 : at; i < at + len && i < 16; i++) ls->on[i] = 0;
    put(ls, at, note, len);
    if (at >= 0 && at < 16 && note > 0) ls->acc |= 1u << at;
}

/* a push: the next chord's note an 8th early, only when it is also legal over this chord */
static int push_ok(const ctx_t *c, int note) { return note > 0 && legal(c->r, note, c->pcs, c->np, c->bass); }

int lead_plays(const plan_block_t *b)
{
    return b && b->section != SEC_INTRO && b->section != SEC_OUTRO && (b->layers & L_KEYS) && (b->layers & L_LEAD);
}

/* ---- the sustained lead: the phrase (sustained-lead design) ----
   phrase.c plans an 8-bar question and answer in scale degrees from an anchor; this sings it over the chords. */

/* Where bar sits in its phrase (the design's call 1): a run of contiguous lead blocks (so a request that splits a
   block, or a 4-bar lead block after another, resumes the phrase rather than restarting it); 8-bar phrases from the
   run's start, a 4-bar remainder half a phrase (the answer when no lead block follows in the song). b is the plan's
   own block: its neighbors are read from the plan (blocks are contiguous from bar 0, and the plan's unused blocks
   are zero). */
static void phrase_at(const plan_block_t *b, int bar, int *run, int *pass, int *bip, int *pbars, int *last)
{
    const plan_block_t *f = b, *e = b, *x;
    int end;
    while (f->start > 0 && lead_plays(f - 1)) f--;
    while ((e + 1)->bars && (e + 1)->start == e->start + e->bars && lead_plays(e + 1)) e++;
    end = e->start + e->bars;
    *last = 1;
    for (x = e + 1; x->bars && x->start == (x - 1)->start + (x - 1)->bars; x++)
        if (lead_plays(x)) *last = 0;
    *run = f->start;
    *pass = (bar - f->start) / 8;
    *bip = (bar - f->start) % 8;
    *pbars = end - (f->start + *pass * 8) >= 8 ? 8 : 4;
}

typedef struct { int t, len, deg, role, orn; } tl_t;   /* a planned note on the phrase's timeline (16ths) */
#define TLMAX 48

/* The phrase's notes, every bar of it. Motion under 40 drops the ornaments (passing and neighbor notes) and the note
   before each holds through its time (Sam: fewer notes, held longer); never past it, so never into a breath. */
static int timeline(uint32_t seed, int run, int pass, int pbars, int last, int motion, tl_t *tl)
{
    phrase_note_t o[8];
    int nb = pbars < 8 ? 4 : 8, bb, i, k, n = 0;
    for (bb = 0; bb < nb; bb++) {
        k = phrase_bar(seed, run, pass, bb, pbars, last, o, 8);
        for (i = 0; i < k && n < TLMAX; i++) {
            if (motion < 40 && o[i].orn && n) {
                int end = bb * 16 + o[i].at + o[i].len;
                if (end > tl[n - 1].t + tl[n - 1].len) tl[n - 1].len = end - tl[n - 1].t;
                continue;
            }
            tl[n].t = bb * 16 + o[i].at;
            tl[n].len = o[i].len;
            tl[n].deg = o[i].deg;
            tl[n].role = o[i].role;
            tl[n].orn = o[i].orn;
            n++;
        }
    }
    return n;
}

/* the roll's scale, 7 pitch classes over the key, and scale index <-> MIDI note (index 0: the key's lowest octave) */
static void scale_of(const song_roll_t *r, int *sc)
{
    unsigned pool = th_mode_pool(r->v[RF_MODE]);
    int i, k = 0;
    for (i = 0; i < 12 && k < 7; i++)
        if ((pool >> i) & 1u) sc[k++] = i;
    for (; k < 7; k++) sc[k] = sc[k - 1] + 1;
}

static int si_note(int key, const int *sc, int si)
{
    int o = si >= 0 ? si / 7 : -((6 - si) / 7);
    return key + 12 * o + sc[si - 7 * o];
}

static int note_si(int key, const int *sc, int note)   /* the scale note at or under note */
{
    int d = note - key, o = d >= 0 ? d / 12 : -((11 - d) / 12), j = 6;
    d -= 12 * o;
    while (j > 0 && sc[j] > d) j--;
    return 7 * o + j;
}

/* The phrase's anchor (degree 0): a chord tone of its first chord, in the scale, placed so the phrase's span sits
   around the lead's register center (C5, kept 6 semitones inside the range). */
static int phrase_anchor(const ctx_t *c, const tl_t *tl, int n, int key, const int *sc)
{
    int dmin = 0, dmax = 0, i, center = 72, best = 72, bc = 1 << 30, note, mid;
    for (i = 0; i < n; i++) {
        if (tl[i].deg < dmin) dmin = tl[i].deg;
        if (tl[i].deg > dmax) dmax = tl[i].deg;
    }
    if (center < c->lo + 6) center = c->lo + 6;
    if (center > c->hi - 6) center = c->hi - 6;
    if (c->lo + 6 > c->hi - 6) center = (c->lo + c->hi) / 2;
    mid = (dmin + dmax) * 6 / 7;                         /* semitones from the anchor to the span's middle */
    for (note = 48; note <= 96; note++)
        if (is_ct(note, c->pcs, c->np) && th_in_scale(key, c->r->v[RF_MODE], note) &&
            legal(c->r, note, c->pcs, c->np, c->bass)) {
            int cost = note + mid > center ? note + mid - center : center - note - mid;
            if (cost < bc) { bc = cost; best = note; }
        }
    return note_si(key, sc, best);
}

enum { K_ANY, K_CT, K_Q, K_OPEN, K_ARR, K_THIRD };      /* what a note must be: legal; a chord tone; the 3rd or
                                                           5th; a chord tone not the root; the root (or the 3rd over
                                                           home); the 3rd */

typedef struct {                                         /* the next bar, for a note tied over the bar line */
    const int *pcs;
    int np;
    unsigned bass;
    int home;                                            /* its chord is home (the progression's first) */
} nxt_t;

typedef struct {
    int target, prev, want, strong;  /* the planned note, the last one sung (0: no line yet), where the planned
                                        interval from it lands, struck on beat 1 or 3 */
    int next;                        /* the next note follows at once: 1 any legal note, 2 a chord tone */
    int fast;                        /* an 8th or less after the last onset: it moves by step */
    int pass;                        /* the step into a cadence (a passing note) */
    int conn;                        /* it follows the last note at once (within an 8th): one gesture */
    int leap;                        /* the last interval leapt (+1 up, -1 down) */
    int ctrun;                       /* quick chord tones by skips ending at the last note (1: it alone) */
    int nfast;                       /* the next note is struck an 8th or less after this one */
    const nxt_t *nx;                 /* ... over the next bar's chord (0: this bar's) */
} aim_t;

/* a step back (1-2 semitones the other way) from note that the next note may take */
static int step_back(const ctx_t *c, const aim_t *a, int note, int dir)
{
    const int *pcs = a->nx ? a->nx->pcs : c->pcs;
    int np = a->nx ? a->nx->np : c->np, d;
    unsigned bass = a->nx ? a->nx->bass : c->bass;
    for (d = 1; d <= 2; d++) {
        int m = note - dir * d;
        if (m >= c->lo && m <= c->hi && legal(c->r, m, pcs, np, bass) && (a->next < 2 || is_ct(m, pcs, np)))
            return 1;
    }
    return 0;
}

/* The legal note of `kind` over kp (the chord it lands on) nearest both the planned note and the planned interval
   from the last note, keeping the planned direction where it can; legal over the next chord too when it rings into
   it (x). 0 if none. A 5th the voicing leaves out counts when it isn't struck on a strong beat. */
static int pick(const ctx_t *c, const aim_t *a, int dir, int kind, const int *kp, int kn, int home, const nxt_t *x)
{
    int note, best = 0, bc = 1 << 30, lo = x ? (c->lo_x > c->lo ? c->lo_x : c->lo) : c->lo;
    for (note = lo; note <= c->hi; note++) {
        int iv = (note - kp[0] + 120) % 12, ct = is_ct(note, kp, kn), cost, d;
        if (!legal(c->r, note, c->pcs, c->np, c->bass)) continue;
        if (x && !legal(c->r, note, x->pcs, x->np, x->bass)) continue;
        if (a->strong && !is_ct(note, c->pcs, c->np)) continue;
        if (kind != K_ANY && kind != K_Q && !ct) continue;
        if (kind == K_Q && !((ct && (iv == 3 || iv == 4 || (iv >= 6 && iv <= 8))) || iv == 7)) continue;
        if (kind == K_OPEN && iv == 0) continue;
        if (kind == K_ARR && !(iv == 0 || (home && (iv == 3 || iv == 4)))) continue;
        if (kind == K_THIRD && !(iv == 3 || iv == 4)) continue;
        d = note - a->target;
        cost = (d < 0 ? -d : d) * 2;
        if (kind == K_ARR && iv) cost += 1;              /* home: the root before the 3rd */
        if (a->prev) {
            int m = note - a->prev, w = note - a->want;
            cost += (w < 0 ? -w : w) * 2;                /* the planned interval */
            if ((m > 0) - (m < 0) != dir) cost += dir ? 16 : 8;   /* the contour: the planned direction (or a
                                                                    repeat), kept where a chord tone allows */
            if (a->conn && (m > 7 || m < -7)) cost += 40 * ((m > 0 ? m : -m) - 7);   /* no leap past a 5th */
            if (a->fast && (m > 2 || m < -2)) cost += 4 * ((m > 0 ? m : -m) - 2);
            if ((a->fast || a->pass) && kind == K_ANY && ct) cost += 3;   /* a quick or passing note: off the chord */
            if (a->next && (m >= 5 || m <= -5) && !step_back(c, a, note, m > 0 ? 1 : -1))
                cost += 30;                              /* a leap the next note can't answer by a step */
            if (a->leap && (m > 0) - (m < 0) == -a->leap && m <= 2 && m >= -2)
                cost -= 12;                              /* a leap answered by a step back */
            if (is_ct(note, c->pcs, c->np)) {            /* never an arpeggio: three quick chord tones by skips */
                int run = a->fast && (m >= 3 || m <= -3) ? a->ctrun + 1 : 1;
                if (run >= 3) cost += 200;
                else if (run == 2 && a->next == 2 && a->nfast) cost += 30;   /* the next is a chord tone too */
            }
        }
        if (cost < bc) { bc = cost; best = note; }
    }
    return best;
}

/* the note for a planned one: its kind by role and beat. A question or arrival tied over the bar line takes its own
   kind only (else the tie is refused and it lands on 1); struck, it falls back (a question to another chord tone
   than the root, an arrival to the 3rd), then to any chord tone; a weak-beat note to any legal note. */
static int sing(const ctx_t *c, const aim_t *a, int dir, int role, int home, const nxt_t *x)
{
    const int *kp = c->pcs;
    int kn = c->np, note = 0;
    if (x && (role == PR_QUESTION || role == PR_ARRIVAL)) {   /* tied in: its chord's, and its home */
        kp = x->pcs;
        kn = x->np;
        home = x->home;
    }
    if (role == PR_QUESTION) {
        note = pick(c, a, dir, K_Q, kp, kn, home, x);
        if (!note && !x) note = pick(c, a, dir, K_OPEN, kp, kn, home, x);
    }
    if (role == PR_ARRIVAL) {
        note = pick(c, a, dir, K_ARR, kp, kn, home, x);
        if (!note && !legal(c->r, kp[0], kp, kn, x ? x->bass : c->bass))
            note = pick(c, a, dir, K_THIRD, kp, kn, home, x);   /* the root rubs (a maj7): the 3rd */
    }
    if (x && (role == PR_QUESTION || role == PR_ARRIVAL)) return note;
    if (!note && (a->strong || role == PR_QUESTION || role == PR_ARRIVAL))
        note = pick(c, a, dir, K_CT, kp, kn, home, x);
    if (!note && !a->strong) note = pick(c, a, dir, K_ANY, kp, kn, home, x);
    return note;
}

/* The phrase's bar: each planned degree re-anchored to this bar's chord. A note tied in from the last bar rings on
   (or, when the tie was refused, strikes again on 1); a note that rings into the next bar is a push, legal over both
   chords, or cut at the bar line. Velocity: an arc with the line's height, softening into the arrival. */
static void plan_phrase(lead_state_t *ls, const ctx_t *c, const plan_block_t *b, int bar, int home, const nxt_t *nx,
                        const int *lpcs, int lnp)
{
    tl_t tl[TLMAX];
    int run, pass, bip, pbars, last, n, i, sc[7], key = c->r->v[RF_KEY], P, prev = ls->last, pdeg = 0, has = 0;
    int dmin = 99, dmax = -99, tied = ls->tied, lt = -1, leap = ls->leap;
    int ctrun = ls->ctrun, pnp = lnp;
    const int *pct = lpcs;
    phrase_at(b, bar, &run, &pass, &bip, &pbars, &last);
    n = timeline(c->r->seed, run, pass, pbars, last, c->r->v[RF_MOTION], tl);
    scale_of(c->r, sc);
    if (bip == 0 || !ls->akey) {                         /* a new phrase (or none placed): its anchor, no line yet */
        ls->anchor = phrase_anchor(c, tl, n, key, sc);
        prev = 0;
        leap = 0;
    } else if (ls->akey - 1 != key) {                    /* a key request mid-phrase: the anchor moves with the key */
        int d = ((key - (ls->akey - 1)) % 12 + 18) % 12 - 6;   /* -6..5, as the motif moves */
        ls->anchor += 7 * (ls->akey - 1 + d - key) / 12;       /* an octave (7 degrees) where the move crosses C */
    }
    ls->akey = key + 1;
    ls->tied = 0;
    for (i = 0; i < n; i++) {
        if (tl[i].deg < dmin) dmin = tl[i].deg;
        if (tl[i].deg > dmax) dmax = tl[i].deg;
    }
    P = bip * 16;
    for (i = 0; i < n; i++) {
        int t = tl[i].t - P, end = t + tl[i].len, dir, note, len, v;
        aim_t a;
        const nxt_t *x;
        if (t >= 16) break;
        if (end <= 0 || (t < 0 && (tied || end < 4))) {  /* before the bar, or tied in and still ringing */
            pdeg = tl[i].deg;
            has = 1;
            continue;
        }
        if (t < 0) t = 0;                                /* the tie was refused: the held note again, on 1 */
        if (i && tl[i].t - (tl[i - 1].t + tl[i - 1].len) >= 8) { has = 0; prev = 0; leap = 0; }   /* after a breath */
        dir = has ? (tl[i].deg > pdeg) - (tl[i].deg < pdeg) : 0;
        x = end > 16 && c->can_push && nx->np > 0 ? nx : 0;
        a.target = si_note(key, sc, ls->anchor + tl[i].deg);
        a.prev = prev;
        a.want = prev + a.target - si_note(key, sc, ls->anchor + pdeg);
        if (leap && prev && i && tl[i].t - (tl[i - 1].t + tl[i - 1].len) <= 2) {
            dir = -leap;                                 /* a leap sung is answered by a step back */
            a.target = a.want = prev - leap * 2;
        }
        a.strong = t == 0 || t == 8;
        a.next = 0;
        a.nx = 0;
        a.fast = i && tl[i].t - tl[i - 1].t <= 2;
        a.pass = tl[i].role == PR_CLOSE;
        a.conn = i && tl[i].t - (tl[i - 1].t + tl[i - 1].len) <= 2;
        a.leap = a.conn ? leap : 0;
        a.ctrun = prev && is_ct(prev, pct, pnp) ? ctrun : 0;
        a.nfast = i + 1 < n && tl[i + 1].t - tl[i].t <= 2;
        if (i + 1 < n && tl[i + 1].t - P < 32 && tl[i + 1].t - (tl[i].t + tl[i].len) <= 2 &&
            (tl[i + 1].t - P < 16 || nx->np > 0)) {
            a.next = (tl[i + 1].t - P) % 8 == 0 || tl[i + 1].role == PR_QUESTION || tl[i + 1].role == PR_ARRIVAL
                     ? 2 : 1;
            a.nx = tl[i + 1].t - P >= 16 ? nx : 0;
        }
        if (a.next && (tl[i + 1].role == PR_QUESTION || tl[i + 1].role == PR_ARRIVAL) && tl[i].role == PR_CLOSE) {
            ctx_t c2 = *c;                               /* the step into a cadence: aim a step from where it lands */
            aim_t a2 = a;
            int t2 = tl[i + 1].t - P, tx, side = tl[i].deg > tl[i + 1].deg ? 1 : -1, h2 = home;
            const nxt_t *x2 = t2 < 16 && t2 + tl[i + 1].len > 16 && c->can_push && nx->np > 0 ? nx : 0;
            if (t2 >= 16) { c2.pcs = nx->pcs; c2.np = nx->np; c2.bass = nx->bass; h2 = nx->home; }
            a2.target = si_note(key, sc, ls->anchor + tl[i + 1].deg);
            a2.prev = 0;
            a2.strong = t2 % 8 == 0;
            a2.next = a2.fast = a2.pass = 0;
            tx = sing(&c2, &a2, 0, tl[i + 1].role, h2, x2);
            if (!tx && x2) tx = sing(&c2, &a2, 0, tl[i + 1].role, h2, 0);
            if (tx) a.target = a.want = tx + side * 2;
        }
        note = sing(c, &a, dir, tl[i].role, home, x);
        if (!note && x) {                                /* no push: it lands on 1 instead */
            x = 0;
            if ((tl[i].role == PR_QUESTION || tl[i].role == PR_ARRIVAL) && end - 16 >= 4) {
                if (lt >= 0) ls->len[lt] = 16 - lt;      /* the step into it holds to the bar line */
                break;
            }
            note = sing(c, &a, dir, tl[i].role, home, x);
        }
        pdeg = tl[i].deg;
        has = 1;
        if (!note) continue;                             /* nothing legal: the note drops (rare) */
        len = end - t;
        if (end > 16 && !x) len = 16 - t;                /* no push: cut at the bar line */
        put(ls, t, note, len);
        lt = t;
        leap = prev && (note - prev >= 5 || prev - note >= 5) ? (note > prev ? 1 : -1) : 0;
        ctrun = !is_ct(note, c->pcs, c->np) ? 0
              : a.fast && prev && is_ct(prev, pct, pnp) && (note - prev >= 3 || prev - note >= 3) ? ctrun + 1 : 1;
        pct = c->pcs;
        pnp = c->np;
        v = 52 + (dmax > dmin ? 16 * (tl[i].deg - dmin) / (dmax - dmin) : 8);   /* the arc, with the height */
        v += len <= 4 ? 28 : len <= 6 ? 14 : 0;   /* a short note sounds less of its swell */
        if (!c->pad && note < 76) v += 76 - note + (note < 72 ? 72 - note : 0);   /* low: among percussive keys */
        if (tl[i].role == PR_ARRIVAL) v -= 8;            /* softening into the arrival */
        else if (tl[i].role == PR_CLOSE && tl[i].t >= 96) v -= 4;
        ls->vel[t] = (uint8_t)(v > 110 ? 110 : v);
        if (x) {
            ls->ring |= 1u << t;                         /* rings across the bar line into the next chord */
            ls->tied = 1;
        }
        prev = note;
    }
    ls->last = prev;
    ls->leap = leap;
    ls->ctrun = ctrun;
}

/* the hybrid's arpeggio rhythms; the song's is the one that lands least on the snare (Sam), not forced */
static const int8_t RHY[6][9][2] = {
    {{0, 2}, {2, 2}, {4, 2}, {6, 2}, {8, 2}, {10, 2}, {12, 2}, {14, 2}, {-1, 0}},   /* 8ths */
    {{0, 3}, {3, 1}, {4, 3}, {7, 1}, {8, 3}, {11, 1}, {12, 4}, {-1, 0}},             /* dotted */
    {{0, 2}, {2, 2}, {6, 2}, {8, 2}, {10, 2}, {14, 2}, {-1, 0}},                     /* gaps */
    {{0, 4}, {4, 2}, {6, 2}, {8, 4}, {12, 1}, {13, 1}, {14, 2}, {-1, 0}},            /* long-short */
    {{0, 3}, {3, 3}, {6, 2}, {8, 3}, {11, 3}, {14, 2}, {-1, 0}},                     /* 3+3+2 */
    {{2, 2}, {4, 2}, {6, 3}, {10, 2}, {12, 2}, {14, 2}, {-1, 0}}};                   /* off the one */
static const int8_t PAT[6][6] = {{0, 1, 2, 3, -1}, {3, 2, 1, 0, -1}, {0, 1, 2, 3, 2, 1}, {0, 2, 1, 3, -1},
                                 {0, 3, 1, 3, -1}, {3, 0, 2, 1, -1}};

static void choose(lead_state_t *ls, const song_roll_t *r)   /* one pattern, rhythm, direction and swap per song */
{
    const lib_pattern_t *dp = lib_pattern_by_id((uint8_t)r->v[RF_PATTERN]);
    unsigned snare = dp ? (unsigned)dp->hit[D_SNARE] : 0x1010u;
    int i, j, best = 1 << 30, tie;
    rng_t g;
    rng_seed(&g, r->seed, 142);
    ls->pat = rng_int(&g, 6);
    ls->rev = rng_int(&g, 3) == 0;
    ls->swap = rng_int(&g, 2) ? rng_int(&g, 5) : -1;    /* two neighboring notes swapped, or none */
    tie = rng_int(&g, 1000);
    for (i = 0; i < 6; i++) {                            /* share of notes on a snare hit, in per mille */
        int on = 0, all = 0, cost;
        for (j = 0; RHY[i][j][0] >= 0; j++) { all++; on += (snare >> RHY[i][j][0]) & 1; }
        cost = on * 1000 / all * 4 + (tie + i * 377) % 1000;
        if (cost < best) { best = cost; ls->rhy = i; }
    }
    ls->chosen = 1;
}


/* the arpeggio keeps 2 + motion/33 of its notes a bar, at most 4 (Sam: not a wall of arpeggio): by rank, the strongest
   beats first (1 and 3, then 2 and 4, then the "ands", then 16ths; earlier first), so the same notes drop every bar
   (Sam: dropping different notes is bad). The motion lever (a request) reaches it at the next bar. Skeleton landings
   are placed after, so they always sound. */
static int arp_rank(int at) { return at % 8 == 0 ? 0 : at % 4 == 0 ? 1 : at % 2 == 0 ? 2 : 3; }

static void plan_arp(lead_state_t *ls, const ctx_t *c, int bar)
{
    int i, k, plen, n = 0, keep = 2 + c->r->v[RF_MOTION] / 33;
    unsigned kept = 0;
    if (!ls->chosen) choose(ls, c->r);
    for (plen = 0; plen < 6 && PAT[ls->pat][plen] >= 0; plen++)
        ;
    if (keep > 4) keep = 4;
    for (k = 0; k < 4 && n < keep; k++)                  /* the kept slots, by rank */
        for (i = 0; RHY[ls->rhy][i][0] >= 0 && n < keep; i++)
            if (arp_rank(RHY[ls->rhy][i][0]) == k) { kept |= 1u << i; n++; }
    for (i = 0; RHY[ls->rhy][i][0] >= 0; i++) {
        int at = RHY[ls->rhy][i][0], pos = (bar * 8 + i) % plen, idx, v;
        if (ls->swap >= 0 && ls->swap % (plen - 1) == pos) pos++;
        else if (ls->swap >= 0 && ls->swap % (plen - 1) + 1 == pos) pos--;
        idx = PAT[ls->pat][pos];
        if ((bar & 1) && ls->rev) idx = 3 - idx;         /* the direction turned every other bar */
        if (bar % 4 == 3 && (i == 2 || i == 5)) continue;   /* the phrase's last bar drops two notes */
        if (!(kept >> i & 1)) continue;                  /* motion thins it, by rank */
        v = c->keys[idx];
        for (k = 1; k < 4 && v && !legal(c->r, v, c->pcs, c->np, c->bass); k++)
            v = c->keys[(idx + k) & 3];                  /* a tone that would rub gives way to the next */
        if (v && legal(c->r, v, c->pcs, c->np, c->bass)) {
            int len = RHY[ls->rhy][i][1];
            int note = place(v % 12, at + len > 16 ? c->lo_x : c->lo, c->hi, v + 12 - c->shift);
            put(ls, at, note, len);
        }
    }
}

/* how a chord change lands on the skeleton note: on 1 for an 8th, on 1 for a quarter, an 8th late, a 16th late,
   pushed an 8th early, or a beat's rest then on 2 (Sam: not always a quarter on 1, but mostly on 1: 55% on 1,
   25% late, 20% pushed) */
int lead_landing(uint32_t seed, int bar)
{
    static const uint8_t W[6] = {25, 30, 10, 5, 20, 10};
    uint32_t h = seed * 0x9E3779B1u ^ (uint32_t)(bar + 1) * 0x85EBCA6Bu;
    int x, i;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 13;
    x = (int)(h % 100u);
    for (i = 0; i < 5 && x >= W[i]; i++) x -= W[i];
    return i;
}

static void plan_hybrid(lead_state_t *ls, const ctx_t *c, int bar, int bpc, const int *npcs, int nn)
{
    int pushed = ls->tied;
    ls->tied = 0;
    plan_arp(ls, c, bar);
    if (bar % bpc == 0) {
        int how = lead_landing(c->r->seed, bar);
        if (how == 4 && !pushed) how = 0;                /* the push was refused: land on 1 instead */
        if (how == 4) {
            land(ls, 0, 0, 2);                           /* it already sounds from the last bar: let it ring */
        } else {
            static const int8_t AT[6] = {0, 0, 2, 3, 2, 4}, LEN[6] = {2, 4, 4, 3, 4, 4};
            int note = skel(c, c->pcs, c->np, ls->last ? ls->last : 74);
            ls->last = note;
            if (AT[how]) land(ls, 0, 0, AT[how]);        /* beat 1 rests until the landing */
            land(ls, AT[how], note, LEN[how]);
        }
    }
    if ((bar + 1) % bpc == 0 && nn > 0 && c->can_push && lead_landing(c->r->seed, bar + 1) == 4) {
        int nxt = skel_from(c, npcs, nn, ls->last, c->lo_x);
        if (push_ok(c, nxt)) {
            ls->last = nxt;
            int i, n = 0, w = -1;
            land(ls, 14, nxt, 4);                        /* the next chord's note an 8th early, across the bar */
            ls->ring |= 1u << 14;
            ls->tied = 1;
            for (i = 0; i < 16; i++) n += ls->on[i] > 0;
            for (i = 1; n > 5 && i < 14; i++)          /* at most 5 a bar: the weakest arpeggio note gives way */
                if (ls->on[i] && !(ls->acc >> i & 1) && (w < 0 || arp_rank(i) >= arp_rank(w))) w = i;
            if (n > 5 && w >= 0) { ls->on[w] = 0; ls->vel[w] = 0; }
        }
    }
}


int lead_plan_bar(lead_state_t *ls, const song_roll_t *r, const plan_block_t *b, int bar, const int keys[4],
                  const int nkeys[4], const int *pcs, int np, int can_push)
{
    const lib_prog_t *pr = lib_prog_by_id((uint8_t)r->v[RF_PROG]);
    const lib_set_t *set = lib_set_by_id((uint8_t)r->v[RF_SET]);
    int bpc = pr && pr->bars_per_chord ? pr->bars_per_chord : 1, loop = pr && pr->n ? pr->n * bpc : 4;
    int i, n = 0, npcs[5], nn;
    ctx_t c;
    for (i = 0; i < 16; i++) ls->on[i] = 0;
    ls->acc = 0;
    ls->ring = 0;
    for (i = 0; i < 16; i++) ls->vel[i] = 0;
    ls->active = ls->tied;
    ls->onsets = 0;
    if (!lead_plays(b) || np <= 0) { ls->tied = 0; return 0; }
    c.r = r; c.keys = keys; c.pcs = pcs; c.np = np; c.bass = 0; c.can_push = can_push;
    if (set && lib_ins_sustained(set->chords)) {         /* a pad: the octave doesn't matter (Sam), its place does */
        int pl = harmony_pad_place(r);
        c.pad = 1;
        c.lo = c.lo_x = 60;
        c.hi = pl == PL_ABOVE ? 72 : 83;
        c.shift = pl == PL_BELOW ? 0 : 12;
    } else {                                             /* percussive keys: a 4th or more over their top */
        int top = 0, ntop = 0;
        for (i = 0; i < 4; i++) {
            if (keys[i] > top) top = keys[i];
            if (nkeys[i] > ntop) ntop = nkeys[i];
        }
        c.pad = 0;
        c.lo = top + 5;
        c.lo_x = (ntop > top ? ntop : top) + 5;
        c.hi = 88;
        c.shift = 0;
    }
    for (i = 0; i < 16; i++) {
        int bn = groove_bass_note(r, b, bar, i, pcs, np);
        if (bn) c.bass |= 1u << (bn % 12);
    }
    nn = harmony_chord_at(r, bar + 1, (bar + 1) / loop + 1, 0, npcs);
    if (set && lib_ins_sustained(set->lead)) {
        const plan_block_t *nb = bar + 1 < b->start + b->bars ? b
                               : (b + 1)->bars && (b + 1)->start == b->start + b->bars ? b + 1 : 0;
        int hp[5], nhp[5], lpcs[5], lnp;
        nxt_t nx;
        nx.pcs = npcs;
        nx.np = nb ? nn : 0;
        nx.bass = 0;
        for (i = 0; nb && i < 16; i++) {
            int bn = groove_bass_note(r, nb, bar + 1, i, npcs, nn);
            if (bn) nx.bass |= 1u << (bn % 12);
        }
        harmony_chord_at(r, bar - bar % loop, bar / loop + 1, 0, hp);   /* home: the progression's first chord */
        harmony_chord_at(r, bar + 1 - (bar + 1) % loop, (bar + 1) / loop + 1, 0, nhp);
        nx.home = nhp[0] % 12 == npcs[0] % 12;
        lnp = bar > 0 ? harmony_chord_at(r, bar - 1, (bar - 1) / loop + 1, 0, lpcs) : 0;   /* the last bar's chord */
        plan_phrase(ls, &c, b, bar, hp[0] % 12 == pcs[0] % 12, &nx, lpcs, lnp);
    } else {
        ls->akey = 0;                                    /* a later sustained lead places its own anchor */
        plan_hybrid(ls, &c, bar, bpc, npcs, nn);
    }
    for (i = 0; i < 16; i++) n += ls->on[i] > 0;
    ls->active |= n > 0;
    ls->onsets = n;
    return n;
}

int lead_step(const lead_state_t *ls, int step, float step_samples, song_ev_t *out, int max)
{
    float len;
    if (step < 0 || step > 15 || ls->on[step] <= 0 || max <= 0) return 0;
    len = (float)ls->len[step] * step_samples - 200.0f;
    out->voice = V_LEAD;
    out->note = (uint8_t)ls->on[step];
    /* the hybrid: landings 110, the arpeggio 106 (sparser since the arpeggio's cap, so each note carries more) */
    out->vel = ls->vel[step] ? ls->vel[step] : (uint8_t)((ls->acc >> step) & 1 ? 110 : 106);
    out->pad = (uint8_t)((ls->ring >> step) & 1);      /* 1: a push, ringing past the bar (and the 4-bar line) */
    out->delay = 0;
    out->len = len > 1.0f ? (uint32_t)len : 1u;
    return 1;
}
