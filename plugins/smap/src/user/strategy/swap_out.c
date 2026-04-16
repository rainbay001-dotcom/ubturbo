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

#define PA_TO_VA_MAP_SIZE 4096
#define PAGEMAP_ENTRY_BYTES 8

/*
 * PA-to-VA mapping entry, built by scanning /proc/pid/maps + /proc/pid/pagemap.
 */
struct pa_va_entry {
    uint64_t pa;
    uint64_t va;
};

struct pa_va_map {
    struct pa_va_entry *entries;
    uint64_t count;
    uint64_t capacity;
};

static int PaVaMapInit(struct pa_va_map *map, uint64_t capacity)
{
    map->entries = (struct pa_va_entry *)calloc(capacity, sizeof(struct pa_va_entry));
    if (!map->entries) {
        return -ENOMEM;
    }
    map->count = 0;
    map->capacity = capacity;
    return 0;
}

static void PaVaMapDestroy(struct pa_va_map *map)
{
    if (map->entries) {
        free(map->entries);
        map->entries = NULL;
    }
    map->count = 0;
    map->capacity = 0;
}

static void PaVaMapAdd(struct pa_va_map *map, uint64_t pa, uint64_t va)
{
    if (map->count >= map->capacity) {
        uint64_t new_cap = map->capacity * 2;
        struct pa_va_entry *new_entries = (struct pa_va_entry *)realloc(
            map->entries, new_cap * sizeof(struct pa_va_entry));
        if (!new_entries) {
            return;
        }
        map->entries = new_entries;
        map->capacity = new_cap;
    }
    map->entries[map->count].pa = pa;
    map->entries[map->count].va = va;
    map->count++;
}

static uint64_t PaVaMapLookup(struct pa_va_map *map, uint64_t pa)
{
    for (uint64_t i = 0; i < map->count; i++) {
        if (map->entries[i].pa == pa) {
            return map->entries[i].va;
        }
    }
    return 0; /* Not found */
}

/*
 * Build PA→VA mapping by walking /proc/pid/maps and /proc/pid/pagemap.
 * Only maps pages whose PA is in the candidate set (phys_addrs).
 */
static int BuildPaVaMap(pid_t pid, uint64_t *phys_addrs, uint64_t nr_pages, struct pa_va_map *map)
{
    char maps_path[BUFFER_SIZE];
    char pagemap_path[BUFFER_SIZE];
    int ret;

    ret = snprintf_s(maps_path, sizeof(maps_path), sizeof(maps_path), "/proc/%d/maps", pid);
    if (ret < 0) {
        return -EINVAL;
    }
    ret = snprintf_s(pagemap_path, sizeof(pagemap_path), sizeof(pagemap_path), "/proc/%d/pagemap", pid);
    if (ret < 0) {
        return -EINVAL;
    }

    FILE *maps_file = fopen(maps_path, "r");
    if (!maps_file) {
        SMAP_LOGGER_ERROR("Failed to open %s", maps_path);
        return -ENOENT;
    }

    int pagemap_fd = open(pagemap_path, O_RDONLY);
    if (pagemap_fd < 0) {
        fclose(maps_file);
        SMAP_LOGGER_ERROR("Failed to open %s", pagemap_path);
        return -ENOENT;
    }

    ret = PaVaMapInit(map, nr_pages);
    if (ret) {
        fclose(maps_file);
        close(pagemap_fd);
        return ret;
    }

    int page_size = IsHugeMode() ? (int)GetHugePageSize() : (int)GetNormalPageSize();
    char line[MAPS_MAX_LEN];

    while (fgets(line, sizeof(line), maps_file) && map->count < nr_pages) {
        unsigned long start, end;
        if (sscanf(line, "%lx-%lx", &start, &end) != MAPS_LIN_LEN) {
            continue;
        }
        if (IsHugeMode() && !IsHugePageRange(line)) {
            continue;
        }

        for (unsigned long vaddr = start; vaddr < end && map->count < nr_pages; vaddr += page_size) {
            off_t offset = (vaddr / PAGE_SIZE) * PAGEMAP_ENTRY_BYTES;
            if (lseek(pagemap_fd, offset, SEEK_SET) == (off_t)-1) {
                continue;
            }

            uint64_t entry;
            if (read(pagemap_fd, &entry, PAGEMAP_ENTRY_BYTES) != PAGEMAP_ENTRY_BYTES) {
                continue;
            }

            if (!(entry & PRESENT)) {
                continue;
            }

            uint64_t pfn = entry & PRN_SHIFT;
            uint64_t pa = pfn * PAGE_SIZE;

            /* Check if this PA is in our candidate set */
            for (uint64_t j = 0; j < nr_pages; j++) {
                if (phys_addrs[j] == pa) {
                    PaVaMapAdd(map, pa, vaddr);
                    break;
                }
            }
        }
    }

    fclose(maps_file);
    close(pagemap_fd);
    return 0;
}

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

int DoSwapOut(ProcessAttr *process, uint64_t *phys_addrs, uint64_t nr_pages, uint32_t batch_size)
{
    if (!process || !phys_addrs || nr_pages == 0) {
        return 0;
    }

    int pidfd = GetOrOpenPidfd(process);
    if (pidfd < 0) {
        process->swapAccounting.swap_out_failures++;
        return 0;
    }

    /* Build PA→VA mapping for the candidate pages */
    struct pa_va_map map;
    int ret = BuildPaVaMap(process->pid, phys_addrs, nr_pages, &map);
    if (ret != 0 || map.count == 0) {
        SMAP_LOGGER_DEBUG("pid %d: no VA mappings found for %lu swap candidates", process->pid, nr_pages);
        if (ret == 0) {
            PaVaMapDestroy(&map);
        }
        return 0;
    }

    int page_size = IsHugeMode() ? (int)GetHugePageSize() : (int)GetNormalPageSize();
    struct iovec *iov = (struct iovec *)calloc(batch_size, sizeof(struct iovec));
    if (!iov) {
        PaVaMapDestroy(&map);
        return 0;
    }

    uint64_t pages_done = 0;
    uint64_t total_mapped = map.count;

    while (pages_done < total_mapped) {
        uint32_t batch = 0;

        /* Build iovec batch from PA→VA mapping */
        for (uint64_t i = pages_done; i < total_mapped && batch < batch_size; i++) {
            iov[batch].iov_base = (void *)map.entries[i].va;
            iov[batch].iov_len = page_size;
            batch++;
        }

        if (batch == 0) {
            break;
        }

        /* Issue process_madvise syscall */
        ssize_t advised = syscall(SYS_process_madvise, pidfd, iov, (size_t)batch, MADV_PAGEOUT, 0);
        if (advised < 0) {
            if (errno == EAGAIN) {
                usleep(SWAP_YIELD_US);
                continue;
            }
            if (errno == ESRCH) {
                /* Process died */
                ClosePidfd(process);
                SMAP_LOGGER_INFO("pid %d: process exited during swap-out", process->pid);
                break;
            }
            SMAP_LOGGER_ERROR("process_madvise pid %d failed: %s", process->pid, strerror(errno));
            process->swapAccounting.swap_out_failures++;
            break;
        }

        pages_done += batch;

        /* Yield between batches */
        if (pages_done < total_mapped) {
            usleep(SWAP_YIELD_US);
        }
    }

    free(iov);
    PaVaMapDestroy(&map);

    SMAP_LOGGER_DEBUG("pid %d: swap-out completed %lu/%lu pages", process->pid, pages_done, total_mapped);
    return (int)pages_done;
}
