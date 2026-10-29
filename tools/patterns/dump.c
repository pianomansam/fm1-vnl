/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Prints the engine's pattern tables as JSON on stdout, for tools/patterns/export.py (built by export.sh).
   The tables private to strum.c, lead.c and melody.c are reached by compiling this file once per source with
   -DDUMP_PART=<n>, each part including that source (so their static names never meet); the stabs' second-stab
   candidates, a constant inside harmony_stab_steps, are probed through the function itself. The landing tables (AT,
   LEN, W), local to functions in lead.c, are out of reach here: export.py reads them from the source. */
#include <stdio.h>
#include <string.h>

#define PART_MAIN 0
#define PART_STRUM 1
#define PART_LEAD 2
#define PART_MELODY 3
#ifndef DUMP_PART
#define DUMP_PART PART_MAIN
#endif

#if DUMP_PART == PART_STRUM || DUMP_PART == PART_MAIN
static void str(const char *s)
{
    putchar('"');
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') putchar('\\');
        putchar(*s);
    }
    putchar('"');
}
#endif

#if DUMP_PART == PART_STRUM
#include "strum.c"
void dump_strum(void)
{
    int i;
    printf("\"strum\": [");
    for (i = 0; i < SP_N; i++) {
        printf(i ? ", " : "");
        str(PAT[i]);
    }
    printf("],\n\"strum_ballad_fill\": ");
    str(BALLAD_FILL);
    printf(",\n");
}

#elif DUMP_PART == PART_LEAD
#include "lead.c"
void dump_lead(void)
{
    int i, j;
    printf("\"arp_rhythms\": [");
    for (i = 0; i < 6; i++) {
        printf(i ? ", [" : "[");
        for (j = 0; j < 9 && RHY[i][j][0] >= 0; j++) printf("%s[%d, %d]", j ? ", " : "", RHY[i][j][0], RHY[i][j][1]);
        printf("]");
    }
    printf("],\n\"arp_patterns\": [");
    for (i = 0; i < 6; i++) {
        printf(i ? ", [" : "[");
        for (j = 0; j < 6 && PAT[i][j] >= 0; j++) printf("%s%d", j ? ", " : "", PAT[i][j]);
        printf("]");
    }
    printf("],\n");
}

#elif DUMP_PART == PART_MELODY
#include "melody.c"
void dump_melody(void)
{
    int i;
    printf("\"melody_cells\": [");
    for (i = 0; i < (int)sizeof CELLS; i++) printf("%s[%d, %d]", i ? ", " : "", CELLS[i], CELL_W[i]);
    printf("],\n");
}

#else
#include "library.h"
#include "harmony.h"
#include "roll.h"
void dump_strum(void);
void dump_lead(void);
void dump_melody(void);

static void masks(const uint16_t *m)
{
    int i;
    printf("[");
    for (i = 0; i < 6; i++) printf("%s%d", i ? ", " : "", m[i]);
    printf("]");
}

/* the stab steps over a kick of these hits, through the engine's own rule */
static unsigned stabs_over(unsigned kick)
{
    static lib_pattern_t p;
    song_roll_t r;
    memset(&p, 0, sizeof p);
    memset(&r, 0, sizeof r);
    p.hit[D_KICK] = (uint16_t)kick;
    lib_pattern_wb(&p);
    r.v[RF_PATTERN] = LIB_PATTERN_WB;
    return harmony_stab_steps(&r);
}

static void dump_stabs(void)
{
    unsigned kick = 0, cands = 0, s, first = 0, busy;
    int n = 0, over, i;
    printf("\"stabs\": {\"first\": 0, \"second\": [");
    /* the candidates in order: each one found is then blocked by the kick, which reveals the next */
    while ((s = stabs_over(kick) & ~1u) != 0 && n < 16) {
        for (i = 0; !((s >> i) & 1u); i++)
            ;
        if ((kick >> i) & 1u) break;                     /* the kick got busy: the busy rule answers, below */
        if (!n) first = 1u << i;
        printf("%s%d", n++ ? ", " : "", i);
        kick |= 1u << i;
    }
    cands = kick;
    /* the busy rule: over a kick of more than `over` hits the stabs keep fixed steps; a kick on the first candidate
       plus hits off every candidate tells it apart from the sparse rule (which would move the second stab) */
    s = first;
    kick = s;
    busy = stabs_over(kick);
    for (over = 1, i = 1; i < 16 && busy == stabs_over(s); i++)
        if (!((cands | 1u) >> i & 1u)) {
            kick |= 1u << i;
            over++;
            busy = stabs_over(kick);
        }
    over--;
    printf("], \"busy_over\": %d, \"busy_steps\": [", over);
    for (i = 0, n = 0; i < 16; i++)
        if ((busy >> i) & 1u) printf("%s%d", n++ ? ", " : "", i);
    printf("]},\n");
}

int main(void)
{
    int i, j;
    printf("{\n\"patterns\": [\n");
    for (i = 0; i < LIB_NPATTERNS; i++) {
        const lib_pattern_t *p = &LIB_PATTERNS[i];
        printf("  {\"id\": %d, \"family\": %d, \"hit\": ", p->id, p->family);
        masks(p->hit);
        printf(", \"ghost\": ");
        masks(p->ghost);
        printf(", \"half_time\": %d, \"poly3\": %d, \"src\": ", p->half_time, p->poly3);
        str(p->src);
        printf(", \"two_bar\": %d, \"hit_b\": ", p->two_bar);
        masks(p->hit_b);
        printf(", \"ghost_b\": ");
        masks(p->ghost_b);
        printf("}%s\n", i + 1 < LIB_NPATTERNS ? "," : "");
    }
    printf("],\n\"progs\": [\n");
    for (i = 0; i < LIB_NPROGS; i++) {
        const lib_prog_t *p = &LIB_PROGS[i];
        printf("  {\"id\": %d, \"kind\": %d, \"n\": %d, \"bars_per_chord\": %d, \"c\": [", p->id, p->kind, p->n,
               p->bars_per_chord);
        for (j = 0; j < 4; j++)
            printf("%s[%d, %d, %d]", j ? ", " : "", p->c[j].degree, p->c[j].alter, p->c[j].quality);
        printf("], \"moods\": %d, \"modes\": %d, \"src\": ", p->moods, p->modes);
        str(p->src);
        printf("}%s\n", i + 1 < LIB_NPROGS ? "," : "");
    }
    printf("],\n");
    dump_stabs();
    dump_strum();
    dump_lead();
    dump_melody();
    printf("\"end\": true\n}\n");
    return 0;
}
#endif
