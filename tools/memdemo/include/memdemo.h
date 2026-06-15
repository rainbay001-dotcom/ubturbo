/* SPDX-License-Identifier: MIT */
/*
 * memdemo - ground-truth generator for evaluating SMAP scan accuracy.
 *
 * Allocates a region of 4K or 2M pages, drives a chosen access pattern over
 * it, and emits a binary hot/cold bitmap (the "ground truth") that can be
 * diffed against what SMAP reports.
 */
#ifndef MEMDEMO_H
#define MEMDEMO_H

#include <stdint.h>
#include <stddef.h>

/* Page granularity (requirement 1). */
enum page_kind {
    PAGE_4K,
    PAGE_2M,
};

/* Run shape (requirement 2/3). */
enum run_mode {
    MODE_PROCESS, /* ordinary anonymous process */
    MODE_VM,      /* KVM memslot registered so SMAP treats us as a guest */
};

/* Access model (requirement 4 + zipf). */
enum access_pattern {
    PAT_UNIFORM,  /* sweep every page equally (round-robin) */
    PAT_RANDOM,   /* uniform random over all pages */
    PAT_GAUSSIAN, /* normal distribution centred on the hot region */
    PAT_ZIPF,     /* zipf: a few pages take most of the accesses */
};

struct memdemo_config {
    enum page_kind page_kind;
    enum run_mode mode;
    enum access_pattern pattern;

    uint64_t size_bytes;   /* total region size, rounded up to a page */
    double hot_ratio;      /* fraction of pages forming the hot region */
    uint64_t iterations;   /* number of accesses (0 => use duration) */
    uint64_t duration_sec; /* run for this many seconds (0 => iterations) */
    uint64_t seed;         /* PRNG seed for reproducibility */
    int64_t hot_threshold; /* hot if count >= this; <0 => use mean */

    uint64_t gpa_base;     /* guest physical base for the VM memslot */
    const char *out_path;  /* bitmap output file */
};

/* Resolved page size in bytes for the configured page_kind. */
size_t memdemo_page_bytes(enum page_kind kind);

/* arg_parse.c */
int parse_args(int argc, char **argv, struct memdemo_config *cfg);
void print_usage(const char *prog);

/* mem_alloc.c */
struct mem_region {
    void *base;
    size_t length;     /* bytes actually mapped */
    size_t page_bytes; /* per-page size */
    uint64_t npages;   /* number of pages in the region */
    int hugetlb;       /* non-zero if mapped with MAP_HUGETLB */
};
int mem_alloc(const struct memdemo_config *cfg, struct mem_region *region);
void mem_free(struct mem_region *region);

/* vm_sim.c - lightweight KVM memslot registration (variant A, no vCPU). */
struct vm_context {
    int kvm_fd;
    int vm_fd;
};
int vm_register(const struct memdemo_config *cfg,
        const struct mem_region *region, struct vm_context *vm);
void vm_release(struct vm_context *vm);

/* access_pattern.c - drive the model and fill per-page access counts. */
int run_access_pattern(const struct memdemo_config *cfg,
                       const struct mem_region *region, uint64_t *counts);

/* groundtruth.c - classify hot pages and write the binary bitmap. */
int write_ground_truth(const struct memdemo_config *cfg,
                       const struct mem_region *region,
                       const uint64_t *counts);

#endif /* MEMDEMO_H */
