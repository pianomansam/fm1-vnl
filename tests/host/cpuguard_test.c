/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The CPU guard's stepping (app/drums/cpuguard.c cg_selftest): over the ceiling a level at a time, each after the
   last one's saving shows; back down under the release level; a lone late block steps nothing; off, never. The
   levels' own effects are stubbed here (their "off" is bit for bit today's: the workbench's and the renders' tests). */
#include <stdio.h>
int cg_selftest(void);
void fx_guard(int plate_off, int bed_off) { (void)plate_off, (void)bed_off; }
void wb_drums_guard(int tails) { (void)tails; }
int main(void)
{
    int bad = cg_selftest();
    if (bad)
        printf("cpuguard: check %d FAILED\n", bad);
    else
        printf("cpuguard: ok\n");
    return bad != 0;
}
