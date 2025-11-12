/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _SMAP_L2_H_
#define _SMAP_L2_H_
#include <linux/list.h>
#include <linux/mm_types.h>

#ifdef __cplusplus
extern "C" {
#endif
struct smap_huge_page {
    unsigned int ac_count;
    struct list_head smap_lru;
    struct page *page;
};

#define RAM_ADDR_START 0x2080000000
#define ACTC_PAGE_SIZE (2 * 1024 * 1024)
#define SOCKET2_START_ADDR 0x202000000000
#define ACTC_START_INDEX 1024
#define ACTC_TABLE_LEN (32 * 1024 / 2)
#define MAX_CHANNEL 8
#define THRESHOLD_RATIO 7

#ifdef __cplusplus
}
#endif

#endif