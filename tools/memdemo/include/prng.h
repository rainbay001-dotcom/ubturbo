/* SPDX-License-Identifier: MIT */
/*
 * Tiny seedable PRNG (xorshift128+) plus helpers, so every run with the same
 * --seed is byte-for-byte reproducible regardless of libc rand() differences.
 */
#ifndef MEMDEMO_PRNG_H
#define MEMDEMO_PRNG_H

#include <stdint.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct prng {
    uint64_t s[2];
};

static inline uint64_t splitmix64(uint64_t *x)
{
    uint64_t z = (*x += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static inline void prng_seed(struct prng *r, uint64_t seed)
{
    uint64_t sm = seed ? seed : 0x9E3779B97F4A7C15ULL;
    r->s[0] = splitmix64(&sm);
    r->s[1] = splitmix64(&sm);
}

static inline uint64_t prng_next(struct prng *r)
{
    uint64_t s1 = r->s[0];
    const uint64_t s0 = r->s[1];
    r->s[0] = s0;
    s1 ^= s1 << 23;
    r->s[1] = s1 ^ s0 ^ (s1 >> 18) ^ (s0 >> 5);
    return r->s[1] + s0;
}

/* Uniform double in [0, 1). */
static inline double prng_double(struct prng *r)
{
    return (prng_next(r) >> 11) * (1.0 / 9007199254740992.0);
}

/* Uniform integer in [0, n). */
static inline uint64_t prng_below(struct prng *r, uint64_t n)
{
    return n ? prng_next(r) % n : 0;
}

/* Standard normal sample via Box-Muller. */
static inline double prng_gaussian(struct prng *r)
{
    double u1 = prng_double(r);
    double u2 = prng_double(r);
    if (u1 < 1e-300)
        u1 = 1e-300;
    return sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
}

#endif /* MEMDEMO_PRNG_H */
