/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

/*
 * Option B Swap-Out: process_madvise(MADV_PAGEOUT)
 *
 * Key insight: actcData[nid][i].addr is a bitmap INDEX (0,1,2,...), NOT a
 * physical address. The kernel's access_pid driver maintains a bitmap where
 * each set bit corresponds to a page belonging to the process on that NUMA
 * node. The index tells us WHICH page in the bitmap, but not its VA.
 *
 * To use process_madvise (which requires virtual addresses), we scan
 * /proc/pid/numa_maps to find VMA ranges on L2 NUMA nodes, then advise
 * MADV_PAGEOUT on entire cold VMA ranges. We use actcData frequency
 * statistics (freqZero count, cold_windows) to decide WHETHER to swap,
 * but we let the kernel decide WHICH specific pages within the VMA to
 * reclaim — the kernel already has the LRU information.
 *
 * This is coarser than per-page control but requires no kernel changes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/uio.h>
#include <sys/syscall.h>

#include "smap_user_log.h"
#include "securec.h"
#include "manage/manage.h"
#include "swap_out.h"

#ifndef SYS_pidfd_open
#define SYS_pidfd_open 434
#endif

#ifndef SYS_process_madvise
#define SYS_process_madvise 440
#endif

#ifndef MADV_PAGEOUT
#define MADV_PAGEOUT 21
#endif

#define MAX_VMA_RANGES 256
#define NUMA_MAPS_LINE_LEN 1024

/*
 * VMA range on a specific NUMA node, parsed from /proc/pid/numa_maps.
 */
struct vma_range {
    uint64_t start;
    uint64_t end;
    int nid;
};

static int GetOrOpenPidfd(ProcessAttr *process)
{
    if (process->swap_pidfd >= 0) {
        return process->swap_pidfd;
    }

    int fd = (int)syscall(SYS_pidfd_open, process->pid, 0);
    if (fd < 0) {
        SMAP_LOGGER_ERROR("pidfd_open(%d) failed: %s", process->pid, strerror(errno));
        return -errno;
    }
    process->swap_pidfd = fd;
    return fd;
}

void ClosePidfd(ProcessAttr *process)
{
    if (!process) {
        return;
    }
    if (process->swap_pidfd >= 0) {
        close(process->swap_pidfd);
        process->swap_pidfd = -1;
    }
}

/*
 * Collect VMA ranges residing on L2 NUMA nodes by parsing /proc/pid/numa_maps.
 *
 * numa_maps format: "<hex_addr> <policy> <details...> N<nid>=<pages> ..."
 * We look for entries where most pages are on L2 nodes (nid >= LOCAL_NUMA_NUM).
 */
static int CollectL2VmaRanges(pid_t pid, struct vma_range *ranges, int max_ranges, int *out_count)
{
    char path[BUFFER_SIZE];
    int ret = snprintf_s(path, sizeof(path), sizeof(path), "/proc/%d/numa_maps", pid);
    if (ret < 0) {
        return -EINVAL;
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        SMAP_LOGGER_ERROR("Failed to open %s", path);
        return -ENOENT;
    }

    char line[NUMA_MAPS_LINE_LEN];
    int count = 0;
    int nrLocal = GetNrLocalNuma();

    while (fgets(line, sizeof(line), f) && count < max_ranges) {
        uint64_t vma_start = 0;
        /* Parse VMA start address (first hex field) */
        if (sscanf(line, "%lx", &vma_start) != 1) {
            continue;
        }

        /* Find which NUMA node has the most pages in this VMA */
        int best_nid = -1;
        unsigned long best_pages = 0;
        unsigned long total_pages = 0;

        /* Scan for N<nid>=<pages> patterns */
        char *pos = line;
        while ((pos = strstr(pos, " N")) != NULL) {
            pos += 2; /* skip " N" */
            int nid = 0;
            unsigned long pages = 0;
            if (sscanf(pos, "%d=%lu", &nid, &pages) == 2) {
                total_pages += pages;
                if (pages > best_pages) {
                    best_pages = pages;
                    best_nid = nid;
                }
            }
        }

        /* Only include VMAs primarily on L2 nodes */
        if (best_nid >= nrLocal && total_pages > 0) {
            ranges[count].start = vma_start;
            ranges[count].nid = best_nid;
            /* We don't know end from numa_maps; use maps to find it */
            ranges[count].end = 0;
            count++;
        }
    }
    fclose(f);

    /* Fill in VMA end addresses from /proc/pid/maps */
    if (count > 0) {
        ret = snprintf_s(path, sizeof(path), sizeof(path), "/proc/%d/maps", pid);
        if (ret < 0) {
            *out_count = 0;
            return -EINVAL;
        }
        f = fopen(path, "r");
        if (f) {
            while (fgets(line, sizeof(line), f)) {
                unsigned long start, end;
                if (sscanf(line, "%lx-%lx", &start, &end) == 2) {
                    for (int i = 0; i < count; i++) {
                        if (ranges[i].start == start) {
                            ranges[i].end = end;
                            break;
                        }
                    }
                }
            }
            fclose(f);
        }
        /* Remove entries where we couldn't find the end */
        int valid = 0;
        for (int i = 0; i < count; i++) {
            if (ranges[i].end > ranges[i].start) {
                if (valid != i) {
                    ranges[valid] = ranges[i];
                }
                valid++;
            }
        }
        count = valid;
    }

    *out_count = count;
    return 0;
}

int DoSwapOut(ProcessAttr *process, uint64_t *cold_indices, uint64_t nr_cold, uint32_t batch_size)
{
    if (!process || nr_cold == 0) {
        return 0;
    }

    int pidfd = GetOrOpenPidfd(process);
    if (pidfd < 0) {
        process->swapAccounting.swap_out_failures++;
        return 0;
    }

    /*
     * Collect VMA ranges on L2 NUMA nodes. We advise MADV_PAGEOUT on these
     * ranges, letting the kernel reclaim the coldest pages within them.
     * The cold_indices/nr_cold from the cold tracker tells us HOW MANY
     * pages are cold (driving the decision to swap), but the kernel's
     * MADV_PAGEOUT handler picks which pages to actually reclaim based
     * on PTE access bits.
     */
    struct vma_range *ranges = (struct vma_range *)calloc(MAX_VMA_RANGES, sizeof(struct vma_range));
    if (!ranges) {
        return 0;
    }

    int range_count = 0;
    int ret = CollectL2VmaRanges(process->pid, ranges, MAX_VMA_RANGES, &range_count);
    if (ret != 0 || range_count == 0) {
        SMAP_LOGGER_DEBUG("pid %d: no L2 VMA ranges found", process->pid);
        free(ranges);
        return 0;
    }

    /* Build iovec array from VMA ranges */
    struct iovec *iov = (struct iovec *)calloc(range_count, sizeof(struct iovec));
    if (!iov) {
        free(ranges);
        return 0;
    }

    int iov_count = 0;
    for (int i = 0; i < range_count && iov_count < (int)batch_size; i++) {
        iov[iov_count].iov_base = (void *)ranges[i].start;
        iov[iov_count].iov_len = ranges[i].end - ranges[i].start;
        iov_count++;
    }

    if (iov_count == 0) {
        free(iov);
        free(ranges);
        return 0;
    }

    /* Issue process_madvise syscall */
    ssize_t advised = syscall(SYS_process_madvise, pidfd, iov, (size_t)iov_count, MADV_PAGEOUT, 0);
    int pages_done = 0;

    if (advised < 0) {
        if (errno == ESRCH) {
            ClosePidfd(process);
            SMAP_LOGGER_INFO("pid %d: process exited during swap-out", process->pid);
        } else if (errno == EACCES || errno == EPERM) {
            SMAP_LOGGER_ERROR("process_madvise pid %d: need CAP_SYS_NICE: %s",
                              process->pid, strerror(errno));
        } else {
            SMAP_LOGGER_ERROR("process_madvise pid %d failed: %s",
                              process->pid, strerror(errno));
        }
        process->swapAccounting.swap_out_failures++;
    } else {
        int page_size = IsHugeMode() ? (int)GetHugePageSize() : (int)GetNormalPageSize();
        pages_done = (int)(advised / page_size);
        SMAP_LOGGER_INFO("pid %d: MADV_PAGEOUT advised %zd bytes (%d pages) across %d L2 VMAs",
                         process->pid, advised, pages_done, iov_count);
    }

    free(iov);
    free(ranges);
    return pages_done;
}
