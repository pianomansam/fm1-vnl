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

void th_voice(const int *pcs, int n, const int *prev, int *out4)
{
    static const int REF[4] = {52, 55, 59, 62};
    int ref[4], best = 1 << 30, i, drop, v;
    if (prev) {
        memcpy(ref, prev, sizeof ref);
        sort4(ref);
    } else {
        memcpy(ref, REF, sizeof ref);
    }
    for (v = 0; v < 4; v++)
        out4[v] = 48 + 4 * v;                              /* fallback; always replaced below */
    for (drop = -1; drop < (n > 4 ? n : 0); drop++) {
        int t[4], m = 0, cand[4][3], nc[4], pen = 0, ix[4];
        if (n > 4) {                                       /* five tones: keep the 3rd and 7th, drop one other */
            int iv = drop >= 0 ? mod12(pcs[drop] - pcs[0]) : 0;
            if (drop < 0 || iv == 3 || iv == 4 || iv == 10 || iv == 11)
                continue;
            pen = drop == 0 ? 3 : 0;
        } else if (drop >= 0) {
            continue;
        }
        for (i = 0; i < n && m < 4; i++)
            if (i != drop)
                t[m++] = pcs[i];
        while (m < 4)
            t[m++] = pcs[0];                               /* a three-tone chord doubles the root */
        for (v = 0; v < 4; v++) {
            int note;
            nc[v] = 0;
            for (note = 48; note <= 72; note++)
                if (mod12(note) == mod12(t[v]) && nc[v] < 3)
                    cand[v][nc[v]++] = note;
        }
        for (ix[0] = 0; ix[0] < nc[0]; ix[0]++)
         for (ix[1] = 0; ix[1] < nc[1]; ix[1]++)
          for (ix[2] = 0; ix[2] < nc[2]; ix[2]++)
           for (ix[3] = 0; ix[3] < nc[3]; ix[3]++) {
               int o[4], c = pen, ok = 1;
               for (v = 0; v < 4; v++)
                   o[v] = cand[v][ix[v]];
               sort4(o);
               if (o[0] > 55 || o[0] == o[1] || o[1] == o[2] || o[2] == o[3])
                   ok = 0;
               if (!ok)
                   continue;
               for (v = 0; v < 4; v++)
                   c += o[v] > ref[v] ? o[v] - ref[v] : ref[v] - o[v];
               for (v = 0; v < 3; v++)
                   if (o[v + 1] - o[v] == 1)
                       c += 8;                              /* no tight clusters */
               if (c < best) {
                   best = c;
                   for (v = 0; v < 4; v++)
                       out4[v] = o[v];
               }
           }
    }
}

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
