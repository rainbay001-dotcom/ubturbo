/* SPDX-License-Identifier: MIT */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <math.h>

#include "memdemo.h"
#include "prng.h"

/* Pick the next page index for the configured model. */
static uint64_t next_index(const struct memdemo_config *cfg, struct prng *rng,
                           uint64_t npages, uint64_t hot_pages, uint64_t seq)
{
    switch (cfg->pattern) {
    case PAT_UNIFORM:
        /* Round-robin sweep: every page gets the same number of hits. */
        return seq % npages;

    case PAT_RANDOM:
        return prng_below(rng, npages);

    case PAT_GAUSSIAN: {
        /* Centre the bell inside the hot region [0, hot_pages). */
        double mean = hot_pages / 2.0;
        double sigma = hot_pages / 6.0;
        if (sigma < 1.0)
            sigma = 1.0;
        double x = mean + prng_gaussian(rng) * sigma;
        if (x < 0.0)
            x = 0.0;
        if (x >= (double)npages)
            x = (double)(npages - 1);
        return (uint64_t)x;
    }

    case PAT_ZIPF: {
        /*
         * Rejection sampler for Zipf(s=1) over [1, npages] (Devroye).
         * Low indices are hottest, matching real hot-page skew.
         */
        const double s = 1.07;
        double n = (double)npages;
        double b = pow(2.0, s - 1.0);
        for (;;) {
            double u = prng_double(rng);
            double v = prng_double(rng);
            double x = floor(pow(u, -1.0 / (s - 1.0)));
            if (x < 1.0 || x > n)
                continue;
            double t = pow(1.0 + 1.0 / x, s - 1.0);
            if (v * x * (t - 1.0) / (b - 1.0) <= t / b)
                return (uint64_t)x - 1;
        }
    }
    }
    return 0;
}

int run_access_pattern(const struct memdemo_config *cfg,
                       const struct mem_region *region, uint64_t *counts)
{
    struct prng rng;
    prng_seed(&rng, cfg->seed);

    uint64_t npages = region->npages;
    uint64_t hot_pages = (uint64_t)(cfg->hot_ratio * (double)npages);
    if (hot_pages < 1)
        hot_pages = 1;

    volatile unsigned char *mem = region->base;
    size_t pb = region->page_bytes;

    /*
     * Fault every page in once so the region is fully resident before we
     * start counting; this pre-touch is deliberately not recorded.
     */
    for (uint64_t i = 0; i < npages; i++)
        mem[i * pb] = 0;

    int by_time = cfg->duration_sec > 0;
    uint64_t iters = cfg->iterations ? cfg->iterations : npages * 100;

    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);

    uint64_t done = 0;
    for (uint64_t seq = 0;; seq++) {
        uint64_t idx = next_index(cfg, &rng, npages, hot_pages, seq);

        /* Touch the page: read-modify-write to set the access bit. */
        volatile unsigned char *p = &mem[idx * pb];
        *p = (unsigned char)(*p + 1);
        counts[idx]++;
        done++;

        if (by_time) {
            if ((seq & 0xFFFF) == 0) {
                struct timespec now;
                clock_gettime(CLOCK_MONOTONIC, &now);
                double el = (now.tv_sec - start.tv_sec) +
                        (now.tv_nsec - start.tv_nsec) / 1e9;
                if (el >= (double)cfg->duration_sec)
                    break;
            }
        } else if (done >= iters) {
            break;
        }
    }

    fprintf(stderr, "access: %llu accesses over %llu pages (%s)\n",
        (unsigned long long)done, (unsigned long long)npages,
        by_time ? "duration" : "iterations");
    return 0;
}
