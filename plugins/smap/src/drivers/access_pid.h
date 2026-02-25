/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: smap access pid module
 */

#ifndef _SRC_ACCESS_PID_H
#define _SRC_ACCESS_PID_H

#include <linux/bitops.h>

#include "check.h"
#include "access_ioctl.h"
#include "bus.h"
#include "drv_common.h"

#define MAX_PATH_LENGTH 64
#define SEC_TO_MS 1000
#define NON_EXIST_PID (-1)
extern int nr_local_numa;

enum ap_state {
	AP_STATE_WALK = (1UL << 0),
	AP_STATE_READ = (1UL << 1),
	AP_STATE_FREQ = (1UL << 2),
	AP_STATE_MIG = (1UL << 3),
};

struct access_pid_struct {
	struct list_head list;
	struct rw_semaphore lock;
	spinlock_t state_lock;
	unsigned long state_flag;
};
extern struct access_pid_struct ap_data;

struct va_segment {
	u64 base_gfn;
	u64 start;
	u64 end;
	u64 hugepages;
};

struct vm_mapping_info {
	u8 nr_segs;
	u32 vm_size;
	struct va_segment segs[MAX_NODE_NUM];
	u32 *mapping;
};

typedef enum {
	NORMAL_MIGRATE,
	REMOTE_MIGRATE,
	MAX_MIGRATE_TYPE,
} migrate_type;

struct access_pid {
	pid_t pid;
	u32 numa_nodes;
	scan_type type;
	u32 scan_time;
	u32 ntimes;
	u32 cur_times;
	struct delayed_work scan_work;
	struct completion work_done;
	u32 scan_count[SMAP_MAX_NUMNODES];
	size_t page_num[SMAP_MAX_NUMNODES];
	size_t bm_len[SMAP_MAX_NUMNODES];
	unsigned long *paddr_bm[SMAP_MAX_NUMNODES];
	unsigned long *white_list_bm[SMAP_MAX_NUMNODES];
	struct list_head node;
	struct vm_mapping_info info;
};

typedef struct {
	u64 pme;
} pagemap_entry_t;

struct remote_migrate_info {
	pid_t pid;
	u64 page_cnt;
	int remote_nid;
	unsigned int mig_cnt;
	u64 folios_len;
	struct folio **folios;
};

struct pagemapread {
	int pos, len; /* units: PM_ENTRY_BYTES, not bytes */
	migrate_type mig_type;
	struct remote_migrate_info mig_info;
	struct access_pid *ap;
};

extern struct list_head ham_pid_list;
extern struct list_head statistic_pid_list;
extern spinlock_t ham_lock;
extern struct rw_semaphore statistic_lock;

void print_access_ham_pid_list(void);
void print_access_statistic_pid_list(void);
void access_remove_ham_pid(int len, struct access_remove_pid_payload *payload);
void access_remove_statistic_pid(int len,
				 struct access_remove_pid_payload *payload);
void destroy_access_pid(struct access_pid *elem);
int init_access_pid(struct access_add_pid_payload *payload,
		    struct access_pid **elem);
void print_access_pid_list(void);
int access_add_ham_pid(int len, struct access_add_pid_payload *payload);
int access_add_statistic_pid(int len, struct access_add_pid_payload *payload,
			     int page_size);
int access_add_pid(int len, struct access_add_pid_payload *payload);
void access_remove_pid(int len, struct access_remove_pid_payload *payload);
void access_remove_all_pid(void);
void change_ap_type(pid_t pid);
void clean_last_ap_data(struct access_pid *ap);
int access_walk_pagemap(struct access_pid *ap);
struct access_pid *find_access_pid(pid_t pid);
int read_pid_freq(pid_t pid, size_t *data_len, u16 **data);
int convert_pos_to_paddr_sorted(pid_t pid, int nid, u64 len, u64 *addr);

static inline bool access_pid_is_scanning(pid_t pid)
{
	struct access_pid *ap = find_access_pid(pid);
	return ap && ap->type != NO_SCAN;
}

static inline void clear_vm_mapping(u32 *mapping, u32 len)
{
	if (mapping)
		memset(mapping, 0xff, len * sizeof(u32));
}

static inline void set_ap_whole_state(struct access_pid_struct *aps,
				      unsigned long state)
{
	spin_lock(&aps->state_lock);
	aps->state_flag = state;
	spin_unlock(&aps->state_lock);
}

static inline bool check_and_clear_ap_state(struct access_pid_struct *aps,
					    enum ap_state state)
{
	spin_lock(&aps->state_lock);
	if (!(aps->state_flag & state)) {
		spin_unlock(&aps->state_lock);
		return false;
	}
	aps->state_flag = 0;
	spin_unlock(&aps->state_lock);
	return true;
}

#endif /* _SRC_ACCESS_PID_H */
