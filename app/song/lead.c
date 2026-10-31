/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdint.h>
#include "lead.h"
#include "leads/leads.h"

int lead_plays(const plan_block_t *b)
{
    return b && b->section != SEC_INTRO && b->section != SEC_OUTRO && (b->layers & L_KEYS) && (b->layers & L_LEAD);
}

int lead_plan_bar(lead_state_t *ls, const song_roll_t *r, const plan_block_t *b, int bar, const int keys[4],
                  const int nkeys[4], const int *pcs, int np, int can_push)
{
    int i, n = 0;
    for (i = 0; i < 16; i++) ls->on[i] = 0;
    ls->acc = 0;
    ls->ring = 0;
    for (i = 0; i < 16; i++) ls->vel[i] = 0;
    for (i = 0; i < 16; i++) ls->dl[i] = 0;
    ls->active = ls->tied;
    ls->onsets = 0;
    if (!lead_plays(b) || np <= 0) { ls->tied = 0; return 0; }
    leads_plan_bar(ls, r, b, bar, keys, nkeys, pcs, np, can_push);   /* the song's style */
    for (i = 0; i < 16; i++) n += ls->on[i] > 0;
    ls->active |= n > 0;
    ls->onsets = n;
    return n;
}

int lead_step(const lead_state_t *ls, int step, float step_samples, song_ev_t *out, int max)
{
    float len;
    if (step < 0 || step > 15 || ls->on[step] <= 0 || max <= 0) return 0;
    len = (float)ls->len[step] * step_samples - (float)ls->dl[step] * step_samples / 3.0f - 200.0f;
    out->voice = V_LEAD;
    out->note = (uint8_t)ls->on[step];
    out->vel = ls->vel[step];                          /* a style's, 25-110 (leads_put) */
    out->pad = (uint8_t)((ls->ring >> step) & 1);      /* 1: a push, ringing past the bar (and the 4-bar line) */
    out->delay = (uint32_t)((float)ls->dl[step] * step_samples / 3.0f);   /* a style's triplet: late by thirds */
    out->len = len > 1.0f ? (uint32_t)len : 1u;
    return 1;
}
