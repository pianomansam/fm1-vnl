/* SPDX-License-Identifier: MPL-2.0 */
/* The voicing search follows the idea of AMB-1's en_voice_lead (MPL-2.0): choose the assignment of chord tones to
 * voices that moves least from the previous chord; here by enumerating octave placements instead of permutations. */
#include <stdint.h>
#include <string.h>
#include "theory.h"

static const uint8_t SCALE[MODE_N][7] = {
    {0, 2, 4, 5, 7, 9, 11}, {0, 2, 3, 5, 7, 9, 10}, {0, 1, 3, 5, 7, 8, 10},
    {0, 2, 4, 6, 7, 9, 11}, {0, 2, 4, 5, 7, 9, 10}, {0, 2, 3, 5, 7, 8, 10}};

static int mod12(int x)
{
    x %= 12;
    return x < 0 ? x + 12 : x;
}

uint16_t th_mode_pool(int mode)
{
    uint16_t m = 0;
    int i;
    if (mode < 0 || mode >= MODE_N)
        mode = 0;
    for (i = 0; i < 7; i++)
        m |= (uint16_t)(1u << SCALE[mode][i]);
    return m;
}

int th_in_scale(int key, int mode, int note)
{
    return (th_mode_pool(mode) >> mod12(note - key)) & 1;
}

/* interval in semitones from the degree's root up k scale steps (k = 2 third, 4 fifth, 6 seventh, 8 ninth ...) */
static int stack(int mode, int d, int k)
{
    return mod12(SCALE[mode][(d + k) % 7] - SCALE[mode][d]);
}

int th_chord_pcs(int key, int mode, th_chord_spec_t c, int color, int *pcs)
{
    int d = c.degree < 0 ? 0 : c.degree > 6 ? 6 : c.degree;
    int root, i3, i5, i7, i9, ext = -1, n = 0, base, t[6], i, k;
    if (mode < 0 || mode >= MODE_N)
        mode = 0;
    root = mod12(key + SCALE[mode][d] + c.alter);
    i3 = stack(mode, d, 2); i5 = stack(mode, d, 4); i7 = stack(mode, d, 6); i9 = stack(mode, d, 8);
    if (c.quality != Q_DIATONIC) {
        i5 = 7; i9 = 2;
        i3 = c.quality == Q_MIN7 ? 3 : 4;
        i7 = c.quality == Q_MAJ7 ? 11 : 10;
        if (c.quality == Q_DOM7)
            ext = 9;
        else if (c.quality == Q_MIN7)
            ext = 5;
        else if (mode == MODE_LYDIAN)
            ext = 6;
    } else if (i3 == 4 && i7 == 10) {
        ext = stack(mode, d, 12);                          /* dominant: 13th */
    } else if (i3 == 3 && i5 == 7 && i7 == 10) {
        ext = stack(mode, d, 10);                          /* minor 7: 11th */
    } else if (i3 == 4 && i7 == 11 && stack(mode, d, 10) == 6) {
        ext = 6;                                           /* major 7 in Lydian: #11 */
    }
    t[n++] = 0;
    t[n++] = i3;
    if (color != COLOR_RICH || i5 != 7)
        t[n++] = i5;                                       /* rich voicings drop a perfect 5th, never a b5 */
    t[n++] = i7;
    base = n;
    if (color >= COLOR_9)
        t[n++] = i9;
    if (color == COLOR_RICH && ext >= 0)
        t[n++] = ext;
    /* a color tone a semitone above a chord tone is an avoid note: leave it out */
    for (i = base; i < n; i++) {
        for (k = 0; k < i; k++)
            if (t[i] == mod12(t[k] + 1))
                break;
        if (k < i) {
            memmove(&t[i], &t[i + 1], (size_t)(n - i - 1) * sizeof t[0]);
            n--;
            i--;
        }
    }
    for (i = 0; i < n; i++)
        pcs[i] = mod12(root + t[i]);
    return n;
}

static int role(const int *pcs, int n, int a, int b)       /* first tone whose interval from the root is a or b */
{
    int i;
    for (i = 1; i < n; i++) {
        int iv = mod12(pcs[i] - pcs[0]);
        if (iv == a || iv == b)
            return pcs[i];
    }
    return -1;
}

void th_guide_tones(const int *pcs, int n, int *third, int *seventh)
{
    *third = role(pcs, n, 3, 4);
    *seventh = role(pcs, n, 10, 11);
}

int th_is_avoid(int key, int mode, const int *pcs, int n, int note)
{
    int i, pc = mod12(note);
    if (!th_in_scale(key, mode, pc))
        return 0;
    for (i = 0; i < n; i++)
        if (pcs[i] == pc)
            return 0;
    for (i = 0; i < n; i++)
        if (mod12(pcs[i] + 1) == pc)
            return 1;
    return 0;
}

static void sort4(int *a)
{
    int i, j;
    for (i = 1; i < 4; i++) {
        int v = a[i];
        for (j = i - 1; j >= 0 && a[j] > v; j--)
            a[j + 1] = a[j];
        a[j + 1] = v;
    }
}

static int role_iv(const int *pcs, int n, int iv)
{
    int k;
    for (k = 1; k < n; k++)
        if (mod12(pcs[k] - pcs[0]) == iv) return 1;
    return 0;
}

static int octaves(int pc, int hi)                       /* how many C3..hi notes have this pitch class */
{
    int note, k = 0;
    for (note = 48; note <= hi; note++)
        k += mod12(note - pc) == 0;
    return k;
}

/* A tone's role by its interval over the root: 0 root, 1 3rd, 2 5th, 3 7th, 4 extension. With a perfect 5th
   present, a b5 is a #11 and a #5 is a b13 (extensions); without one they are the 5th. */
static int role_of(const int *pcs, int n, int i)
{
    int iv = mod12(pcs[i] - pcs[0]), p5 = 0, k;
    for (k = 1; k < n; k++)
        p5 |= mod12(pcs[k] - pcs[0]) == 7;
    if (i == 0 || iv == 0) return 0;
    if (iv == 3 || iv == 4) return 1;
    if (iv == 10 || iv == 11) return 3;
    if (iv == 7 || ((iv == 6 || iv == 8) && !p5)) return 2;
    return 4;
}

/* Voice leading, as a pianist comps (Sam): common tones stay where they are, the other voices move to the nearest
   chord tone, and the top voice comes first: it stays when the new chord has it, else it steps. Rootless voicings
   are welcome (the bass has the root): a five-tone chord drops its root first, then its 5th, never its 3rd or 7th,
   a dominant keeps at least one extension. Register E3-A4 (52-69) preferred, C3-C5 (48-72) allowed so no two
   notes are ever forced a semitone apart; the bottom at most C4, the top pulled back up when it drifts low, no
   semitone clusters and no tight seconds low down. */
void th_voice_range(const int *pcs, int n, const int *prev, int *out4, int hi)
{
    int ref[4], best = 1 << 30, i, drop, v, next = 0, role[5], dom;
    if (prev) {
        memcpy(ref, prev, sizeof ref);
        sort4(ref);
    }
    for (i = 0; i < n && i < 5; i++) {
        role[i] = role_of(pcs, n, i);
        next += role[i] == 4;
    }
    dom = n > 2 && role_iv(pcs, n, 4) && role_iv(pcs, n, 10);
    for (v = 0; v < 4; v++)
        out4[v] = 52 + 4 * v;                              /* fallback; always replaced below */
    for (drop = -1; drop < (n >= 4 ? n : 0); drop++) {
        int t[4], m = 0, cand[4][3], nc[4], pen = 0, ix[4];
        if (n > 4) {                                       /* five tones: drop one */
            if (drop < 0 || role[drop] == 1 || role[drop] == 3) continue;
            if (role[drop] == 4 && next < 2 && dom) continue;   /* a dominant keeps an extension */
            pen = role[drop] == 0 ? 0 : role[drop] == 2 ? 1 : 3;
        } else if (n == 4 && drop >= 0) {                  /* four tones: may go rootless, the 5th doubled */
            if (role[drop] != 0) continue;
            pen = 2;
        } else if (drop >= 0) {
            continue;
        }
        for (i = 0; i < n && m < 4; i++)
            if (i != drop)
                t[m++] = pcs[i];
        if (m < 4 && n == 4)                               /* rootless: double the 5th (else the 3rd), where */
            for (i = 0; i < n && m < 4; i++)               /* it has two octaves in the register */
                if (role[i] == 2 && octaves(pcs[i], hi) > 1) t[m++] = pcs[i];
        if (m < 4 && n == 4)
            for (i = 0; i < n && m < 4; i++)
                if (role[i] == 1 && octaves(pcs[i], hi) > 1) t[m++] = pcs[i];
        if (m < 4) continue;
        while (m < 4)
            t[m++] = pcs[0];                               /* a three-tone chord doubles the root */
        for (v = 0; v < 4; v++) {
            int note;
            nc[v] = 0;
            for (note = 48; note <= hi; note++)
                if (mod12(note) == mod12(t[v]) && nc[v] < 3)
                    cand[v][nc[v]++] = note;
        }
        for (ix[0] = 0; ix[0] < nc[0]; ix[0]++)
         for (ix[1] = 0; ix[1] < nc[1]; ix[1]++)
          for (ix[2] = 0; ix[2] < nc[2]; ix[2]++)
           for (ix[3] = 0; ix[3] < nc[3]; ix[3]++) {
               int o[4], c = pen, w, d;
               for (v = 0; v < 4; v++)
                   o[v] = cand[v][ix[v]];
               sort4(o);
               if (o[0] > 60 || o[0] == o[1] || o[1] == o[2] || o[2] == o[3])
                   continue;
               if (prev) {
                   for (v = 0; v < 3; v++)
                       c += o[v] > ref[v] ? o[v] - ref[v] : ref[v] - o[v];
                   d = o[3] > ref[3] ? o[3] - ref[3] : ref[3] - o[3];
                   c += 8 * d + (d > 2 ? 6 : 0);            /* the top voice: stay, else step */
                   for (v = 0; v < 4; v++) {               /* a common tone that moved */
                       int in = 0, at = 0;
                       for (w = 0; w < 4; w++) {
                           in |= mod12(t[w] - ref[v]) == 0;
                           at |= o[w] == ref[v];
                       }
                       if (in && !at) c += 6;
                   }
               } else {
                   c += (o[3] > 65 ? o[3] - 65 : 65 - o[3]) + (o[0] > 55 ? o[0] - 55 : 55 - o[0]);
               }
               if (o[3] < 60) c += 60 - o[3];               /* keep the top line up, not muddy */
               for (v = 0; v < 4; v++) {                   /* E3-A4 preferred; C3-C5 when a rub needs room */
                   if (o[v] < 52) c += 4 * (52 - o[v]);
                   if (o[v] > 69) c += 4 * (o[v] - 69);
               }
               for (v = 0; v < 3; v++) {
                   if (o[v + 1] - o[v] == 1) c += 100;     /* no semitone clusters (unless nothing else fits) */
                   if (o[v + 1] - o[v] == 2 && o[v] < 55) c += 3;   /* no tight seconds low down */
               }
               if (c < best) {
                   best = c;
                   for (v = 0; v < 4; v++)
                       out4[v] = o[v];
               }
           }
    }
}

void th_voice(const int *pcs, int n, const int *prev, int *out4) { th_voice_range(pcs, n, prev, out4, 72); }

static const char *const NOTE[12] = {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};

const char *th_chord_name(const int *pcs, int n, char *buf)
{
    uint32_t iv = 0;
    const char *q, *r = NOTE[mod12(pcs[0])];
    int i, k = 0;
#define HAS(x) ((iv >> (x)) & 1u)
    for (i = 1; i < n; i++)
        iv |= 1u << mod12(pcs[i] - pcs[0]);
    if (HAS(3)) {
        if (HAS(6))
            q = HAS(10) ? "m7b5" : HAS(9) ? "dim7" : "dim";
        else if (HAS(11))
            q = "mM7";
        else if (HAS(10))
            q = HAS(5) ? "m11" : HAS(9) ? "m13" : HAS(2) ? "m9" : "m7";
        else
            q = HAS(9) ? "m6" : "m";
    } else if (HAS(4)) {
        if (HAS(10))
            q = HAS(9) ? "13" : HAS(2) ? "9" : HAS(1) ? "7b9" : "7";
        else if (HAS(11))
            q = HAS(6) ? "maj#11" : HAS(2) ? "maj9" : "maj7";
        else
            q = HAS(2) ? "add9" : "";
    } else {
        q = "5";
    }
#undef HAS
    while (*r)
        buf[k++] = *r++;
    while (*q)
        buf[k++] = *q++;
    buf[k] = 0;
    return buf;
}
