/* SPDX-License-Identifier: MPL-2.0 */
#include "rng.h"

uint32_t rng_next(rng_t *r)
{
    uint32_t x = r->s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    r->s = x;
    return x;
}

void rng_seed(rng_t *r, uint32_t seed, uint32_t stream)
{
    int i;
    r->s = (seed * 0x9E3779B1u) ^ (stream * 0x85EBCA77u) ^ 0x2545F491u;
    if (r->s == 0)
        r->s = 0x6D2B79F5u;
    for (i = 0; i < 4; i++)
        rng_next(r);
}

float rng_float(rng_t *r)
{
    /* top 24 bits: exact in a float, so the result is always below 1.0 */
    return (float)(rng_next(r) >> 8) * (1.0f / 16777216.0f);
}

int rng_int(rng_t *r, int n)
{
    if (n <= 0)
        return 0;
    return (int)(rng_next(r) % (uint32_t)n);
}

int rng_weighted(rng_t *r, const uint8_t *w, int n)
{
    uint32_t total = 0, pick;
    int i;
    for (i = 0; i < n; i++)
        total += w[i];
    if (total == 0)
        return -1;
    pick = rng_next(r) % total;
    for (i = 0; i < n; i++) {
        if (pick < w[i])
            return i;
        pick -= w[i];
    }
    return -1;
}
