/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "rolled.h"
#include "harmony.h"
#include "groove.h"
#include "strum.h"
#include "song.h"
#include "leads/leads.h"

void song_rolled(const song_roll_t *r, song_rolled_t *out)
{
    int slow, blk;
    out->comp = harmony_style(r);
    out->comp_pat = harmony_comp_pat(r);
    out->lead_style = leads_style(r);
    out->pad_on = song_pad_on(r);
    out->pad_place = harmony_pad_place(r);
    out->strum_pat = strum_pattern(r, &slow);
    out->strum_fill = strum_fill_drive(r);
    out->lead_loop = LEAD_LOOP_DEFAULT;
    out->bass_style = groove_bass_style(r);
    out->ride = groove_ride(r, &blk);
    out->keys_high = harmony_keys_high(r);
    leads_band_for(r, out->lead_style, &out->lead_lo, &out->lead_hi);
    out->keys_with = harmony_keys_draw(r);
}
