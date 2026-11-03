/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The modeled vibraphone's note on (see modal.h). */
#include <string.h>
#include "modal.h"

#define SR 44100.0f
#define T60 6.9078f      /* ln 1000 */
#define MD_TOP 18000.0f  /* a mode above it is left out */
#define MD_NORM 1.5f     /* the output's scale: the first 150 ms about as loud as the sample's zones were (unit RMS) */

/* a mode: frequency hz, amplitude a, a -60 dB time t60 (s): a two-pole recursion whose next output is a r^n sin(w n),
   starting at 0 */
static void mode(md_voice_t *v, int k, float hz, float a, float t60)
{
    float w = FM_TWO_PI * hz / SR, r = fm_expf(-T60 / (t60 * SR));
    if (hz >= MD_TOP || a <= 0.0f) {
        v->c[k] = v->r2[k] = v->s1[k] = v->s2[k] = 0.0f;
        return;
    }
    v->c[k] = 2.0f * r * fm_cosf(w);
    v->r2[k] = r * r;
    v->s1[k] = -a * fm_sinf(w) / r;              /* y[-1] */
    v->s2[k] = -a * fm_sinf(2.0f * w) / (r * r); /* y[-2] */
}

void md_start(md_voice_t *v, const vx_patch_t *p, float note, float vel)
{
    float hz = 440.0f * fm_exp2f((note - 69.0f) * (1.0f / 12.0f)), oct = (note - 60.0f) * (1.0f / 12.0f);
    float fc = p->p[4] * (1.0f - p->p[5] * (1.0f - fm_clampf(vel, 0.0f, 1.0f))), x;
    float up = fm_exp2f(-oct * p->p[8]), tb;
    int k;
    memset(v, 0, sizeof *v);
    /* the fundamental: its -60 dB time the patch's decay, shorter up the keys by p[9] */
    mode(v, 0, hz, MD_NORM, p->dec * fm_exp2f(-oct * p->p[9]));
    for (k = 1; k < MD_N; k++) {
        float fk = hz * p->p[k - 1]; /* p[0], p[1]: the ratios */
        x = fk / fc;
        x *= x;
        x *= x; /* the mallet: a 4th-order roll-off, (f/fc)^8 under the root */
        mode(v, k, fk, MD_NORM * p->p[k + 1] * fm_rsqrtf(1.0f + x * x), p->p[k + 5] * up); /* p[2], p[3]; p[6], p[7] */
    }
    /* the tube's bloom: from the bar's share a0 to full, its time constant shorter up the keys (a resonator of the
       same Q builds up in the same number of cycles) */
    v->a0 = fm_clampf(p->p[10], 0.0f, 1.0f);
    v->tube = 1.0f - v->a0;
    tb = fm_maxf(p->p[11] * fm_exp2f(-oct), 0.002f);
    v->bl = 1.0f;
    v->blk = fm_expf(-(float)MD_CR / (tb * SR));
    v->g1 = v->a0;
    v->cc = MD_CR - 1u; /* the first sample sets the ramp */
}
