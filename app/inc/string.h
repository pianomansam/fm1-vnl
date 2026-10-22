/* SPDX-License-Identifier: MPL-2.0 */
/* The firmware's <string.h> (fm1.toml device_include; the host and the emulator use their own libc's). The JieLi
   toolchain is freestanding: the BSP's chip/libc.c defines memset, memcpy and memcmp, with unsigned lengths as
   declared here (size_t is as wide); memmove, which the song engine also uses, is defined here. */
#ifndef VNL_STRING_H
#define VNL_STRING_H
#include <stddef.h>                      /* size_t, NULL (the compiler's own header) */

void *memset(void *d, int c, unsigned n);
void *memcpy(void *d, const void *s, unsigned n);
int memcmp(const void *a, const void *b, unsigned n);

static inline void *memmove(void *d, const void *s, unsigned n)
{
    unsigned char *p = d;
    const unsigned char *q = s;
    if (p < q)
        while (n--)
            *p++ = *q++;
    else
        while (n--)
            p[n] = q[n];
    return d;
}

#endif
