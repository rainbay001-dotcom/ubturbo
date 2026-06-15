/* SPDX-License-Identifier: MIT */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "memdemo.h"

#ifndef MAP_HUGE_SHIFT
#define MAP_HUGE_SHIFT 26
#endif
#ifndef MAP_HUGE_2MB
#define MAP_HUGE_2MB (21 << MAP_HUGE_SHIFT)
#endif

int mem_alloc(const struct memdemo_config *cfg, struct mem_region *region)
{
    size_t pb = memdemo_page_bytes(cfg->page_kind);
    size_t len = cfg->size_bytes;
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    int hugetlb = 0;

    if (cfg->page_kind == PAGE_2M) {
        flags |= MAP_HUGETLB | MAP_HUGE_2MB;
        hugetlb = 1;
    }

    void *base = mmap(NULL, len, PROT_READ | PROT_WRITE, flags, -1, 0);
    if (base == MAP_FAILED && hugetlb) {
        perror("mmap(MAP_HUGETLB)");
        fprintf(stderr,
            "hint: reserve 2M pages first, e.g.\n"
            "  echo %zu > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages\n",
            (len + pb - 1) / pb);
        return -1;
    }
    if (base == MAP_FAILED) {
        perror("mmap");
        return -1;
    }

    /*
     * For the 4K path, keep THP from silently collapsing the region into
     * huge pages - otherwise the ground truth no longer reflects 4K pages.
     */
    if (cfg->page_kind == PAGE_4K)
        (void)madvise(base, len, MADV_NOHUGEPAGE);

    region->base = base;
    region->length = len;
    region->page_bytes = pb;
    region->npages = len / pb;
    region->hugetlb = hugetlb;
    return 0;
}

void mem_free(struct mem_region *region)
{
    if (region && region->base && region->base != MAP_FAILED) {
        munmap(region->base, region->length);
        region->base = NULL;
    }
}
