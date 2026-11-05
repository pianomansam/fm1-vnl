/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Prints the engine's pattern tables (app/song/patterns_gen.c, generated from patterns/ by import.py) as JSON on
   stdout, for tools/patterns/export.py (built by export.sh). */
#include <stdio.h>
#include "patterns_gen.h"

static void str(const char *s)
{
    putchar('"');
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') putchar('\\');
        putchar(*s);
    }
    putchar('"');
}

static void ints(const char *key, const int *v, int n, const char *end)
{
    int i;
    printf("\"%s\": [", key);
    for (i = 0; i < n; i++) printf("%s%d", i ? ", " : "", v[i]);
    printf("]%s", end);
}

static void bytes(const char *key, const uint8_t *v, int n, const char *end)
{
    int x[64], i;
    for (i = 0; i < n && i < 64; i++) x[i] = v[i];
    ints(key, x, i, end);
}

static void masks(const uint16_t *m)
{
    int i;
    printf("[");
    for (i = 0; i < 6; i++) printf("%s%d", i ? ", " : "", m[i]);
    printf("]");
}

static void comp(const patterns_comp_t *c, int status, const char *end)
{
    printf("  {\"style\": %d, \"weight\": %d, \"busy\": %d, \"rules\": %d, \"keys\": %lu, \"vel\": [%d, %d], ",
           c->style, c->weight, c->busy, c->rules, (unsigned long)c->keys, c->vel_lo, c->vel_hi);
    bytes("steps", c->step, c->n, ", ");
    bytes("lens", c->len, c->n, ", ");
    printf("\"later_vel_pct\": %d, \"status\": %d}%s\n", c->later_vel_pct, status, end);
}

int main(void)
{
    int i, j, x[16];
    const patterns_stabs_t *s = &COMP_STABS;
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
        printf(", \"status\": %d}%s\n", LIB_PATTERNS_STATUS[i], i + 1 < LIB_NPATTERNS ? "," : "");
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
        printf(", \"status\": %d}%s\n", LIB_PROGS_STATUS[i], i + 1 < LIB_NPROGS ? "," : "");
    }
    printf("],\n\"comp\": [\n");           /* (the host's build: every row, candidates too) */
    for (i = 0; i < COMP_N; i++) comp(&COMP[i], COMP_STATUS[i], i + 1 < COMP_N ? "," : "");
    printf("],\n\"stabs\": {\"first\": %d, ", s->first);
    bytes("second", s->second, s->n, ", ");
    printf("\"len\": %d, \"busy_over\": %d, ", s->len, s->busy_over);
    for (i = 0, j = 0; i < 16; i++)
        if (s->busy_steps >> i & 1u) x[j++] = i;
    ints("busy_steps", x, j, "},\n");
    printf("\"strum\": [");
    for (i = 0; i < STRUM_NPAT; i++) {
        printf(i ? ", " : "");
        str(STRUM_PAT[i]);
    }
    printf("],\n");
    bytes("strum_status", STRUM_PAT_STATUS, STRUM_NPAT, ",\n");
    printf("\"strum_ballad_fill\": ");
    str(STRUM_BALLAD_FILL);
    printf(",\n\"strum_fill_step\": [");
    for (i = 0; i < STRUM_NFILL; i++) {
        printf(i ? ", " : "");
        str(STRUM_FILL_STEP[i]);
    }
    printf("],\n\"strum_fill_drive\": [");
    for (i = 0; i < STRUM_NFILL; i++) {
        printf(i ? ", " : "");
        str(STRUM_FILL_DRIVE[i]);
    }
    printf("]\n}\n");
    return 0;
}
