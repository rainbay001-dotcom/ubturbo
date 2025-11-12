/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#ifndef HAM_HAM_MIGRATION_H
#define HAM_HAM_MIGRATION_H

#include <linux/completion.h>
#include <linux/mmzone.h>
#ifdef __cplusplus
extern "C" {
#endif

#define BATCH_NUM 4
#define HAM_START_MIGRATION _IOW('N', 0, unsigned long)
#define HAM_MIGRATE_PAGES _IOW('N', 1, unsigned long)
#define HAM_ROLLBACK_PAGES _IOW('N', 2, unsigned long)
#define HAM_STOP_MIGRATION _IOW('N', 3, unsigned long)
#define HAM_MODIFY_PAGETABLE _IOW('N', 4, unsigned long)
#define HAM_CACHE_CLEAR _IOW('N', 5, unsigned long)
#define HAM_UB_DRAIN _IOW('N', 6, unsigned long)

#define PAGE_SIZE_2M (1UL << 21) // 2MB per page
#define PAGE_SIZE_4K (1UL << 12)
#define MAX_MIGRATE_RETRY 10

struct ram_block_info {
	int rmt_numa_id;
	unsigned long size;
	unsigned long hva_start;
};

struct migration_param {
	pid_t pid;
	unsigned int cnt;
	struct ram_block_info ram_blocks[BATCH_NUM];
};

typedef struct {
	pid_t pid;
	bool cacheable;
} maintain_info;

struct freq_info {
	u64 hpa;
	u16 freq;
};

extern int get_ham_pages_freqs(pid_t pid, struct freq_info **freq_info_array,
			       uint64_t *freq_info_num);
int ham_init(void);
void ham_exit(void);

#ifdef __cplusplus
}
#endif
#endif
