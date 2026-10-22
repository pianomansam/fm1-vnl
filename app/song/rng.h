/* SPDX-License-Identifier: MPL-2.0 */
/* Seeded random streams: one small xorshift32 state per concern, so a song's seed reproduces it exactly. */
#ifndef SONG_RNG_H
#define SONG_RNG_H
#include <stdint.h>

typedef struct { uint32_t s; } rng_t;

void rng_seed(rng_t *r, uint32_t seed, uint32_t stream);   /* distinct, nonzero state per (seed, stream) */
uint32_t rng_next(rng_t *r);                               /* xorshift32 (13, 17, 5) */
float rng_float(rng_t *r);                                 /* [0, 1) */
int rng_int(rng_t *r, int n);                              /* [0, n); 0 when n <= 0 */
int rng_weighted(rng_t *r, const uint8_t *w, int n);       /* index by weight; -1 when all weights are 0 */

#endif
