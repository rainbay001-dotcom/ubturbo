/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: SMAP common header
 */

#ifndef _SRC_TIERING_COMMON_H
#define _SRC_TIERING_COMMON_H

#include <asm/page.h>
#include <linux/list.h>

#define MAX(a, b) ((a) >= (b) ? (a) : (b))
#define MIN(a, b) ((a) <= (b) ? (a) : (b))

#define HUNDRED 100
#define HALF_HUNDRED (HUNDRED / 2)
#define MILLION 1000000
#define UNIT_OF_TIME 1000
#define CYCLE_MAX_RECORD 1000
#define TWO_MEGA_SHIFT 21
#define TWO_MEGA_SIZE (1UL << TWO_MEGA_SHIFT)
#define TWO_MEGA_MASK (~(TWO_MEGA_SIZE - 1))
#define BAREMETAL_DEFAULT_RATIO 50
#define MAX_4K_PROCESSES_CNT 300
#define MAX_2M_PROCESSES_CNT 100
#define MAX_PER_PID_MIG_LIST_COUNT 8
#define MAX_2M_MIGMSG_CNT (MAX_2M_PROCESSES_CNT * MAX_PER_PID_MIG_LIST_COUNT)
#define MAX_4K_MIGMSG_CNT (MAX_4K_PROCESSES_CNT * MAX_PER_PID_MIG_LIST_COUNT)

#define MB_SHIFT 20
#define _4K_TO_2M 9

#define SMAP_MAX_LOCAL_NUMNODES 4
#define SMAP_MAX_NUMNODES 22
#define HUGE_PAGE_SIZE 2097152

/* last physical address before hole */
#define ADDR_BH 0x7FFFFFFF

enum node_level {
	L1,
	L2,
	NR_LEVEL,
};

struct mig_list {
	bool success_to_user;
	u64 nr;
	u64 failed_mig_nr;
	u64 failed_pre_migrated_nr;
	pid_t pid;
	int from;
	int to;
	u64 *addr;
};

struct mig_pra {
	int page_size;
	int nr_thread;
	bool is_mul_thread;
};

struct migrate_msg {
	int cnt;
	struct mig_pra mul_mig;
	struct mig_list *mig_list;
};

#define MAX_NR_MIGNUMA 50

struct pa_range {
	u64 pa_start;
	u64 pa_end;
};

struct migrate_numa_inner_msg {
	int src_nid;
	int dest_nid;
	int count;
	struct pa_range range[MAX_NR_MIGNUMA];
};

struct migrate_numa_msg {
	int src_nid;
	int dest_nid;
	int count;
	u64 memids[MAX_NR_MIGNUMA];
};

struct mig_payload {
    pid_t pid;
    int src_nid;
    int dest_nid;
    int ratio;
	int keep_ratio;
    u64 mem_size;
    bool is_ratio_mode;
    u64 success_cnt;
};

struct migrate_pid_remote_numa_msg {
    int pid_cnt;
    struct mig_payload *payloads;
    int *mig_res_array; // 迁移结果
};

typedef enum {
	NORMAL_MIGRATE,
	REMOTE_MIGRATE,
	MAX_MIGRATE_TYPE,
} migrate_type;

static inline u64 calc_2m_count(u64 range)
{
	return (range & ~TWO_MEGA_MASK) == 0 ?
		       (u64)(range >> TWO_MEGA_SHIFT) :
		       (u64)((range >> TWO_MEGA_SHIFT) + 1);
}

static inline u64 calc_4k_count(u64 range)
{
	return (range & ~PAGE_MASK) == 0 ? (u64)(range >> PAGE_SHIFT) :
					   (u64)((range >> PAGE_SHIFT) + 1);
}

static inline bool is_node_invalid(int node)
{
	return node < 0 || node >= SMAP_MAX_NUMNODES;
}

#endif /* _SRC_TIERING_COMMON_H */
