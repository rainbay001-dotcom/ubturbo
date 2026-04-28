/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * Description: SMAP Tiering Memory Solution: get accessed bit模块
 */

#ifndef _SRC_ACCESSED_BIT_H
#define _SRC_ACCESSED_BIT_H

#include "access_pid.h"
#include "access_iomem.h"
#include "access_acpi_mem.h"
#include "check.h"
#include "kvm_pgtable.h"

#define SCAN_PERIOD_UNIT_MS 50

extern struct list_head remote_ram_list;
extern int nr_local_numa;
extern struct access_pid_struct ap_data;
extern bool remote_ram_changed;

struct statistics_tracking_info {
	pid_t pid;
	int l1_node;
	int l2_node;
	u64 page_num[NR_LEVEL]; // 近远端页面数
	u64 *vaddr[NR_LEVEL]; // 虚地址数组, 顺序排序
	u32 scan_num; // 已扫描次数，次数 % 窗口个数 = 待更新窗口
	u8 **sliding_windows; // 统计时长除以单次周期 = 窗口个数， 单个窗口大小 = Pagenum(l1) + Pagenum(l2)
	struct list_head node;
	u64 window_num; // 窗口个数
};

struct ham_tracking_info {
	pid_t pid;
	int l1_node;
	int l2_node;
	struct proc_dir_entry *pde;
	u64 len[NR_LEVEL];
	u64 *paddr[NR_LEVEL];
	u16 *freq[NR_LEVEL];
	struct list_head node;
};

struct hva_info {
	u64 va;
	int nid;
};

struct pte_walk {
	pid_t pid;
	scan_type type;
	int nid;
	u16 *buf;
	int buf_len;
	bool flag;
	struct hva_info *hva_info;
	u64 index;
	int page_size;
	u64 nr_page[NR_LEVEL];
	u64 statistic_cnt;
	u64 *statistic_vaddr;
	struct access_pid *ap;
};

struct freq_info {
	u64 hpa;
	u16 freq;
};

int smap_create_tracking_info_file(struct ham_tracking_info *info);
int get_ham_pages_freqs(pid_t pid, struct freq_info **freq_info_array,
			uint64_t *freq_info_num);
struct file *get_kvm_file_from_task(struct task_struct *task);
int scan_accessed_bit_forward_vm(pid_t pid, int page_size, scan_type type,
				 struct access_pid *ap);
int scan_accessed_bit_forward_mm(pid_t pid, int page_size, scan_type type,
				 struct access_pid *ap);
int scan_hva_info(pid_t pid_nr, u64 *l1_page_num, u64 *l2_page_num,
		  u64 **l1_vaddr, u64 **l2_vaddr);
int scan_hva_info_4k(pid_t pid, u64 *l1_page_num, u64 *l2_page_num,
		     u64 **l1_vaddr, u64 **l2_vaddr);
#endif /* _SRC_ACCESSED_BIT_H */
