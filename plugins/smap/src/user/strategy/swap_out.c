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
 * Option A: ioctl-based swap-out via kernel NVMe swap module.
 *
 * This implementation sends physical addresses directly to the kernel
 * via ioctl(SMAP_SWAP_OUT).  The kernel module (smap_swap.c) handles:
 *   - PFN validation and page lookup
 *   - NVMe slot allocation (bitmap)
 *   - Synchronous bio write to NVMe
 *   - Slot<->page mapping maintenance
 *
 * Compared to Option B (process_madvise):
 *   - No PA->VA conversion needed (kernel works with physical addresses)
 *   - No pidfd needed
 *   - Direct bio path to NVMe (bypasses kernel swap subsystem overhead)
 *   - Trade-off: requires custom kernel module for I/O
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "smap_user_log.h"
#include "securec.h"
#include "manage/manage.h"
#include "manage/smap_ioctl.h"
#include "swap_out.h"

#define MAX_IOCTL_BATCH 4096

int DoSwapOut(ProcessAttr *process, uint64_t *phys_addrs, uint64_t nr_pages, uint32_t batch_size)
{
    struct ProcessManager *manager;
    int mig_fd;
    uint64_t pages_done = 0;
    int total_swapped = 0;

    if (!process || !phys_addrs || nr_pages == 0) {
        return 0;
    }

    manager = GetProcessManager();
    if (!manager) {
        SMAP_LOGGER_ERROR("DoSwapOut: no process manager");
        return 0;
    }

    mig_fd = manager->fds.migrate;
    if (mig_fd < 0) {
        SMAP_LOGGER_ERROR("DoSwapOut: migrate device not open");
        process->swapAccounting.swap_out_failures++;
        return 0;
    }

    while (pages_done < nr_pages) {
        uint64_t batch_cnt = nr_pages - pages_done;
        if (batch_cnt > batch_size) {
            batch_cnt = batch_size;
        }
        if (batch_cnt > MAX_IOCTL_BATCH) {
            batch_cnt = MAX_IOCTL_BATCH;
        }

        /* Allocate results buffer for this batch */
        uint64_t *results = (uint64_t *)calloc(batch_cnt, sizeof(uint64_t));
        if (!results) {
            SMAP_LOGGER_ERROR("DoSwapOut: alloc results failed");
            break;
        }

        /*
         * Build the ioctl message.  Physical addresses go directly
         * to the kernel -- no PA->VA conversion needed.
         */
        struct SwapIoctlMsg msg;
        msg.cnt = (int)batch_cnt;
        msg.pid = process->pid;
        msg.addrs = &phys_addrs[pages_done];
        msg.results = results;

        int ret = ioctl(mig_fd, SMAP_SWAP_OUT, &msg);
        if (ret < 0) {
            if (errno == ENOSPC) {
                SMAP_LOGGER_INFO("pid %d: NVMe swap space full at %lu/%lu",
                                 process->pid, pages_done, nr_pages);
                free(results);
                break;
            }
            if (errno == ENODEV) {
                SMAP_LOGGER_ERROR("pid %d: NVMe swap device not configured",
                                  process->pid);
                process->swapAccounting.swap_out_failures++;
                free(results);
                break;
            }
            SMAP_LOGGER_ERROR("pid %d: SMAP_SWAP_OUT ioctl failed: %s",
                              process->pid, strerror(errno));
            process->swapAccounting.swap_out_failures++;
            free(results);
            break;
        }

        /* Count successfully swapped pages (result != -1) */
        for (uint64_t i = 0; i < batch_cnt; i++) {
            if (results[i] != (uint64_t)-1) {
                total_swapped++;
            }
        }

        free(results);
        pages_done += batch_cnt;

        /* Yield between batches to avoid starving other work */
        if (pages_done < nr_pages) {
            usleep(SWAP_YIELD_US);
        }
    }

    SMAP_LOGGER_DEBUG("pid %d: ioctl swap-out completed %d/%lu pages",
                      process->pid, total_swapped, nr_pages);
    return total_swapped;
}
