/* SPDX-License-Identifier: MPL-2.0 */
/* The firmware's <string.h> (fm1.toml device_include; the host and the emulator use their own libc's). The JieLi
   toolchain is freestanding: the BSP's chip/libc.c defines memset, memcpy and memcmp, with unsigned lengths as
   declared here (size_t is as wide). The song engine's memmove is app.c's. */
#ifndef VNL_STRING_H
#define VNL_STRING_H
#include <stddef.h>                      /* size_t, NULL (the compiler's own header) */

void *memset(void *d, int c, unsigned n);
void *memcpy(void *d, const void *s, unsigned n);
int memcmp(const void *a, const void *b, unsigned n);

/* memmove is the app's own (app.c), under another name: nothing to collide with in the BSP's libc.c or a
   Bluetooth build's SDK libraries */
void *vnl_memmove(void *d, const void *s, unsigned n);
#define memmove vnl_memmove

#endif
