/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_PART_STAT_H
#define _LINUX_PART_STAT_H

#include <linux/blk_types.h>
#include <asm-generic/local.h>

struct disk_stats {
    /* depends disk_stats */
    u64 nsecs[NR_STAT_GROUPS];
    unsigned long merges[NR_STAT_GROUPS];
    unsigned long ios[NR_STAT_GROUPS];
    unsigned long sectors[NR_STAT_GROUPS];
    unsigned long io_ticks;
    local_t in_flight[2];
};

#endif