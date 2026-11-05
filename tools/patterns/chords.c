/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Test only (test_patterns.py): every progression's chords as the engine makes them (theory.c's th_chord_pcs, in its
   major or minor key), in every key at every color, as JSON on stdout: {"id": [chord][key][color][pitch classes]}. */
#include <stdio.h>
#include "patterns_gen.h"

int main(void)
{
    int i, k, key, color, j;
    printf("{");
    for (i = 0; i < LIB_NPROGS; i++) {
        const lib_prog_t *p = &LIB_PROGS[i];
        printf("%s\"%d\": [", i ? ",\n" : "", p->id);
        for (k = 0; k < p->n; k++) {
            printf("%s[", k ? ", " : "");
            for (key = 0; key < 12; key++) {
                printf("%s[", key ? ", " : "");
                for (color = COLOR_7; color <= COLOR_RICH; color++) {
                    int pcs[5], n = th_chord_pcs(key, (p->modes >> MODE_MINOR) & 1, p->c[k], color, pcs);
                    printf("%s[", color ? ", " : "");
                    for (j = 0; j < n; j++) printf("%s%d", j ? ", " : "", pcs[j]);
                    printf("]");
                }
                printf("]");
            }
            printf("]");
        }
        printf("]");
    }
    printf("}\n");
    return 0;
}
