/* SPDX-License-Identifier: MPL-2.0 */
/* The song engine on the host: seeded random streams (and, in later tasks, the rest of app/song). */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "rng.h"

static int fails;
#define CHECK(c, ...)                                                                                              \
    do {                                                                                                           \
        if (!(c)) {                                                                                                \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                            \
            printf(__VA_ARGS__);                                                                                   \
            printf("\n");                                                                                          \
            fails++;                                                                                               \
        }                                                                                                          \
    } while (0)

static void test_rng(void)
{
    rng_t a, b;
    int i, same = 1, hit[7] = {0}, all = 1, bad = 0, ones = 0;
    uint8_t w[3] = {0, 3, 1}, z[3] = {0, 0, 0};

    rng_seed(&a, 12345, 2);
    rng_seed(&b, 12345, 2);
    for (i = 0; i < 1000; i++)
        if (rng_next(&a) != rng_next(&b))
            same = 0;
    CHECK(same, "same (seed, stream) must repeat");

    rng_seed(&a, 777, 0);
    rng_seed(&b, 777, 1);
    CHECK(a.s != 0 && b.s != 0, "states must be nonzero");
    CHECK(rng_next(&a) != rng_next(&b), "streams 0 and 1 must differ");

    rng_seed(&a, 0, 0);
    CHECK(a.s != 0, "seed 0 stream 0 must give a nonzero state");

    rng_seed(&a, 99, 3);
    for (i = 0; i < 100000; i++) {
        float f = rng_float(&a);
        if (!(f >= 0.0f && f < 1.0f))
            bad++;
    }
    CHECK(bad == 0, "rng_float left [0,1) %d times", bad);

    for (i = 0; i < 1000; i++) {
        int v = rng_int(&a, 7);
        if (v < 0 || v > 6)
            bad++;
        else
            hit[v] = 1;
    }
    for (i = 0; i < 7; i++)
        all &= hit[i];
    CHECK(all && bad == 0, "rng_int(7) must hit all of 0..6 in range");
    CHECK(rng_int(&a, 0) == 0 && rng_int(&a, -3) == 0, "rng_int with n <= 0 is 0");

    for (i = 0; i < 10000; i++) {
        int v = rng_weighted(&a, w, 3);
        if (v == 0 || v < 0 || v > 2)
            bad++;
        if (v == 1)
            ones++;
    }
    CHECK(bad == 0, "rng_weighted returned a zero-weight or invalid index");
    CHECK(ones >= 7000 && ones <= 8000, "rng_weighted index 1 came %d of 10000 (want ~7500)", ones);
    CHECK(rng_weighted(&a, z, 3) == -1, "all-zero weights return -1");
}

int main(void)
{
    test_rng();
    if (fails) {
        printf("%d check(s) failed\n", fails);
        return 1;
    }
    printf("song: all checks passed\n");
    return 0;
}
