/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The sample bank's check and mount (samples.h). */
#include "samples.h"

/* the payload mounted, or 0 (no bank: smp_bank and smp_hit give NULL). The host starts with its own copy
   (samples_blob.c: every build but the firmware's, the one with FELUCCA_ID); the FM-1 with none until app_init
   mounts its flash's. */
#ifndef FELUCCA_ID
static const uint8_t *base = smp_blob + SMP_HEAD;
#else
static const uint8_t *base;
#endif

static uint32_t rd32(const uint8_t *p)
{
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

uint32_t smp_crc32(uint32_t crc, const uint8_t *p, uint32_t n)
{
    static const uint32_t T[16] = {0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u,
                                   0x4DB26158u, 0x5005713Cu, 0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
                                   0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu};
    crc = ~crc;
    while (n--) {
        crc ^= *p++;
        crc = crc >> 4 ^ T[crc & 15u];
        crc = crc >> 4 ^ T[crc & 15u];
    }
    return ~crc;
}

int smp_head_ok(const uint8_t *h)
{
    return rd32(h) == SMP_MAGIC && rd32(h + 4) == SMP_FORMAT && rd32(h + 8) == smp_bytes() && rd32(h + 12) == smp_crc();
}

int smp_mount(const uint8_t *bank, uint32_t avail)
{
    base = 0;
    if (!bank || avail < SMP_HEAD + smp_bytes() || !smp_head_ok(bank) ||
        smp_crc32(0, bank + SMP_HEAD, smp_bytes()) != smp_crc())
        return -1;
    base = bank + SMP_HEAD;
    return 0;
}

int smp_ready(void) { return base != 0; }

const uint8_t *smp_data(const smp_zone_t *z) { return base + z->off; }
