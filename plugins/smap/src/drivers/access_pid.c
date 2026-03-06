// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: smap access pid module
 */

#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/rwlock.h>
#include <linux/limits.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/pid.h>

#include "check.h"
#include "accessed_bit.h"
#include "access_ioctl.h"
#include "access_tracking.h"
#include "access_mmu.h"
#include "access_pid.h"

#undef pr_fmt
#define pr_fmt(fmt) "access_pid: " fmt

#define NODE_PATH "/dev/smap_node%d"
#define NODE_PATH_MAX 100

#define VM_MEMSLOT_PRIOR_THRE (3 * (1 << GB_TO_4K_SHIFT))
#define SEC_TO_MS 1000
#define SCAN_TIMES_NEEDED_BY_NEW_PID 2

struct access_pid_struct ap_data = {
	.list = LIST_HEAD_INIT(ap_data.list),
	.lock = __RWSEM_INITIALIZER(ap_data.lock),
	.state_lock = __SPIN_LOCK_UNLOCKED(ap_data.state_lock),
	.state_flag = AP_STATE_WALK,
};

void destroy_access_pid(struct access_pid *elem)
{
	int i;
	if (!elem) {
		return;
	}
	elem->numa_nodes = 0;
	for (i = 0; i < SMAP_MAX_NUMNODES; i++) {
		vfree(elem->paddr_bm[i]);
		elem->paddr_bm[i] = NULL;
		vfree(elem->white_list_bm[i]);
		elem->white_list_bm[i] = NULL;
		elem->bm_len[i] = 0;
		elem->scan_count[i] = 0;
		elem->page_num[i] = 0;
	}
	if (elem->info.mapping) {
		vfree(elem->info.mapping);
		elem->info.mapping = NULL;
	}
	kfree(elem);
	return;
}

static void destroy_access_ham_pid(struct ham_tracking_info *elem)
{
	if (!elem) {
		return;
	}
	vfree(elem->paddr[L1]);
	vfree(elem->freq[L1]);
	if (elem->l2_node != NUMA_NO_NODE) {
		vfree(elem->paddr[L2]);
		vfree(elem->freq[L2]);
	}
	elem->len[L1] = elem->len[L2] = 0;
	elem->paddr[L1] = elem->paddr[L2] = NULL;
	elem->freq[L1] = elem->freq[L2] = NULL;
	kfree(elem);
	return;
}

static void destroy_access_statistic_pid(struct statistics_tracking_info *elem)
{
	u64 i;
	if (!elem) {
		return;
	}
	/* vaddr[L1] stores the pointer of whole vaddr */
	vfree(elem->vaddr[L1]);
	for (i = 0; i < elem->window_num; ++i) {
		vfree(elem->sliding_windows[i]);
	}
	vfree(elem->sliding_windows);
	elem->sliding_windows = NULL;
	kfree(elem);
}

static void add_kvm_seg(struct vm_mapping_info *info,
			struct kvm_memory_slot *memslot, u64 gfn_start,
			u64 gfn_end)
{
	int i = info->nr_segs;
	int shift = __builtin_ctz(g_pagesize_huge);
	info->segs[i].base_gfn = memslot->base_gfn + gfn_start;
	info->segs[i].start =
		memslot->userspace_addr + (gfn_start << PAGE_SHIFT);
	info->segs[i].end = memslot->userspace_addr + (gfn_end << PAGE_SHIFT);
	info->segs[i].hugepages = (info->segs[i].end - info->segs[i].start) >>
				  shift;
	info->nr_segs++;
}

static int scan_kvm_gfn(struct kvm *kvm, struct vm_mapping_info *info)
{
	struct kvm_memslots *slots;
	struct kvm_memory_slot *memslot, *tmp_slot = NULL;
	int bkt;
	u64 gfn_thre = VM_MEMSLOT_PRIOR_THRE;

	if (!kvm || !kvm->arch.mmu.pgt) {
		pr_err("null kvm or kvm pagetable passed to kvm scan gfn\n");
		return -EINVAL;
	}

	slots = kvm_memslots(kvm);
	if (!slots) {
		pr_err("unable to get kvm memslots\n");
		return -EINVAL;
	}

	kvm_for_each_memslot(memslot, bkt, slots) {
		if (memslot->npages < (1 << HUGE_TO_4K_SHIFT))
			continue;
		if (memslot->base_gfn < (1 << GB_TO_4K_SHIFT))
			continue;
		if (memslot->base_gfn == (1 << GB_TO_4K_SHIFT) &&
		    memslot->npages > gfn_thre) {
			tmp_slot = memslot;
			add_kvm_seg(info, memslot, gfn_thre, memslot->npages);
		} else {
			add_kvm_seg(info, memslot, 0, memslot->npages);
		}
		info->vm_size += (memslot->npages >> HUGE_TO_4K_SHIFT);
	}
	if (tmp_slot) {
		u64 gfn = MIN(gfn_thre, tmp_slot->npages);
		add_kvm_seg(info, tmp_slot, 0, gfn);
	}
	return 0;
}

static inline int pre_scan_kvm_gfn(struct kvm *kvm)
{
	int idx = srcu_read_lock(&kvm->srcu);
	read_lock(&kvm->mmu_lock);
	return idx;
}

static inline void post_scan_kvm_gfn(struct kvm *kvm, int idx)
{
	read_unlock(&kvm->mmu_lock);
	srcu_read_unlock(&kvm->srcu, idx);
}

static int init_vm_mapping(struct vm_mapping_info *info)
{
	if (info->vm_size) {
		info->mapping = vmalloc(info->vm_size * sizeof(u32));
		if (!info->mapping) {
			pr_err("unable to allocate memory for VM mapping\n");
			return -ENOMEM;
		}
		clear_vm_mapping(info->mapping, info->vm_size);
		return 0;
	}
	info->mapping = NULL;
	return 0;
}

static int init_vm_mapping_info(pid_t pid, struct vm_mapping_info *info)
{
	int srcu_idx, ret;
	struct file *filp;
	struct kvm *kvm;
	struct pid *pid_s;
	struct task_struct *task;
	pid_s = find_get_pid(pid);
	task = get_pid_task(pid_s, PIDTYPE_PID);
	if (!task) {
		put_pid(pid_s);
		pr_err("unable to get task struct of pid: %d\n", pid);
		return -EINVAL;
	}
	filp = get_kvm_file_from_task(task);
	if (!filp) {
		put_task_struct(task);
		put_pid(pid_s);
		pr_err("unable to get kvm file of pid: %d\n", pid);
		return -EINVAL;
	}
	kvm = filp->private_data;
	if (!kvm) {
		fput(filp);
		put_task_struct(task);
		put_pid(pid_s);
		pr_err("unable to get kvm of pid: %d\n", pid);
		return -EINVAL;
	}
	srcu_idx = pre_scan_kvm_gfn(kvm);

	ret = scan_kvm_gfn(kvm, info);
	if (ret) {
		pr_err("failed to scan kvm of pid %d\n", pid);
	}
	post_scan_kvm_gfn(kvm, srcu_idx);
	fput(filp);
	put_task_struct(task);
	put_pid(pid_s);
	if (!ret) {
		ret = init_vm_mapping(info);
	}
	return ret;
}

int init_access_pid(struct access_add_pid_payload *payload,
		    struct access_pid **elem)
{
	struct access_pid *ap = kzalloc(sizeof(*ap), GFP_KERNEL);
	if (!ap)
		return -ENOMEM;
	ap->pid = payload->pid;
	ap->numa_nodes = payload->numa_nodes;
	ap->scan_time = payload->scan_time;
	ap->ntimes = payload->ntimes;
	ap->type = payload->type;
	init_completion(&ap->work_done);
	for (int i = 0; i < SMAP_MAX_NUMNODES; i++) {
		ap->scan_count[i] = 0;
		ap->page_num[i] = 0;
		ap->bm_len[i] = 0;
		ap->paddr_bm[i] = NULL;
		ap->white_list_bm[i] = NULL;
	}
	if (is_access_hugepage()) {
		if (init_vm_mapping_info(ap->pid, &ap->info)) {
			kfree(ap);
			return -EINVAL;
		}
	}
	*elem = ap;
	return 0;
}

void print_access_pid_list(void)
{
	struct access_pid *ap;

	pr_debug("access pid list:\n");
	down_read(&ap_data.lock);
	list_for_each_entry(ap, &ap_data.list, node) {
		pr_debug("pid %d, numa_nodes %x, scan_time %d, type %d\n",
			 ap->pid, ap->numa_nodes, ap->scan_time, ap->type);
	}
	up_read(&ap_data.lock);
	pr_debug("---------------------\n");
}

void print_access_ham_pid_list(void)
{
	struct ham_tracking_info *ap;

	pr_debug("---access ham pid list---\n");
	spin_lock(&ham_lock);
	list_for_each_entry(ap, &ham_pid_list, node) {
		pr_debug("pid %d, l1_node %d, l2_node %d\n", ap->pid,
			 ap->l1_node, ap->l2_node);
	}
	spin_unlock(&ham_lock);
	pr_debug("---------------------\n");
}

void print_access_statistic_pid_list(void)
{
	struct statistics_tracking_info *ap;

	pr_debug("statistic access pid list:\n");
	down_read(&statistic_lock);
	list_for_each_entry(ap, &statistic_pid_list, node) {
		pr_debug("pid %d, l1_node %d, l2_node %d\n", ap->pid,
			 ap->l1_node, ap->l2_node);
	}
	up_read(&statistic_lock);
	pr_debug("---------------------\n");
}

static int init_ham_pid_memory(struct ham_tracking_info *info,
			       enum node_level level)
{
	if (info->len[level] == 0) {
		pr_err("ham tracking info len %d is 0\n", level);
		return -EINVAL;
	}
	info->paddr[level] = vzalloc(info->len[level] * sizeof(u64));
	if (!info->paddr[level]) {
		pr_err("unable to allocate memory for HAM access pid physical address array\n");
		return -ENOMEM;
	}
	info->freq[level] = vzalloc(info->len[level] * sizeof(actc_t));
	if (!info->freq[level]) {
		vfree(info->paddr[level]);
		pr_err("unable to allocate memory for HAM access pid frequency array\n");
		return -ENOMEM;
	}
	return 0;
}

static inline void clean_ham_tracking_info(struct ham_tracking_info *tmp)
{
	if (tmp->paddr[L1]) {
		vfree(tmp->paddr[L1]);
	}
	if (tmp->freq[L1]) {
		vfree(tmp->freq[L1]);
	}
}

static int init_ham_pid_len(struct ham_tracking_info *tmp)
{
	struct access_tracking_dev *adev;

	list_for_each_entry(adev, &access_dev, list) {
		if (adev->node == tmp->l1_node) {
			down_read(&adev->buffer_lock);
			tmp->len[L1] = adev->page_count;
			up_read(&adev->buffer_lock);
			if (init_ham_pid_memory(tmp, L1)) {
				return -ENOMEM;
			}
		} else if (adev->node == tmp->l2_node) {
			down_read(&adev->buffer_lock);
			tmp->len[L2] = adev->page_count;
			up_read(&adev->buffer_lock);
			if (init_ham_pid_memory(tmp, L2)) {
				clean_ham_tracking_info(tmp);
				return -ENOMEM;
			}
		}
	}
	return 0;
}

static inline int find_first_local_numa(unsigned long *nodes)
{
	unsigned long pos;
	unsigned long size;

	if (!nodes || nr_local_numa < 0) {
		return NUMA_NO_NODE;
	}
	size = BITS_PER_BYTE * sizeof(unsigned long);
	pos = find_first_bit(nodes, size);
	return pos >= 0 && pos < (u32)nr_local_numa ? pos : NUMA_NO_NODE;
}

static int find_first_remote_numa(unsigned long *nodes)
{
	unsigned long pos;
	int nid;
	unsigned long size;

	if (!nodes || nr_local_numa < 0) {
		return NUMA_NO_NODE;
	}
	size = BITS_PER_BYTE * sizeof(unsigned long);
	pos = find_next_bit(nodes, size, nr_local_numa);
	nid = convert_pos_to_nid(pos);
	if ((nid >= nr_local_numa) &&
	    (nid < nr_local_numa + SMAP_MAX_REMOTE_NUMNODES)) {
		return nid;
	} else {
		return NUMA_NO_NODE;
	}
}

static int init_access_ham_pid(struct access_add_pid_payload *payload)
{
	struct ham_tracking_info *tmp;
	int l1_node, l2_node;
	unsigned long numa_nodes;

	if (!payload) {
		return -EINVAL;
	}
	numa_nodes = (unsigned long)payload->numa_nodes;
	l1_node = find_first_local_numa(&numa_nodes);
	l2_node = find_first_remote_numa(&numa_nodes);
	if (l1_node == NUMA_NO_NODE) {
		pr_err("invalid local node: %d passed to init access HAM pid\n",
		       l1_node);
		return -EINVAL;
	}

	spin_lock(&ham_lock);
	list_for_each_entry(tmp, &ham_pid_list, node) {
		if (tmp->pid == payload->pid) {
			tmp->l1_node = l1_node;
			tmp->l2_node = l2_node;
			spin_unlock(&ham_lock);
			return 0;
		}
	}
	spin_unlock(&ham_lock);
	tmp = kzalloc(sizeof(struct ham_tracking_info), GFP_KERNEL);
	if (!tmp) {
		pr_err("unable to allocate memory for HAM tracking info\n");
		return -ENOMEM;
	}
	tmp->pid = payload->pid;
	tmp->l1_node = l1_node;
	tmp->l2_node = l2_node;
	tmp->len[L1] = tmp->len[L2] = 0;
	tmp->paddr[L1] = tmp->paddr[L2] = NULL;
	tmp->freq[L1] = tmp->freq[L2] = NULL;

	if (init_ham_pid_len(tmp)) {
		pr_err("unable to init HAM pid length\n");
		kfree(tmp);
		return -ENOMEM;
	}
	if (smap_create_tracking_info_file(tmp)) {
		pr_err("unable to create tracking info file of pid: %d in process file system\n",
		       tmp->pid);
		kfree(tmp);
		return -ENXIO;
	}

	spin_lock(&ham_lock);
	list_add_tail(&tmp->node, &ham_pid_list);
	spin_unlock(&ham_lock);
	return 0;
}

int access_add_ham_pid(int len, struct access_add_pid_payload *payload)
{
	int ret = 0;
	for (int i = 0; i < len; i++) {
		if (payload[i].type == HAM_SCAN) {
			pr_debug("init access ham pid %d\n", payload[i].pid);
			ret = init_access_ham_pid(&payload[i]);
		}
		if (ret) {
			pr_err("unable to init HAM access pid: %d\n",
			       payload[i].pid);
			return ret;
		}
	}
	return ret;
}

static int init_statistic_window(u8 ***sliding_windows, u32 duration,
				 u32 scan_time, u64 total_page_num,
				 u64 *windows_num)
{
	u64 win_nr;
	u8 **wins;
	u64 i, j;
	win_nr = (duration * SEC_TO_MS + scan_time - 1) / scan_time;
	*windows_num = win_nr;
	wins = vzalloc(win_nr * sizeof(u8 *));
	if (!wins) {
		pr_err("unable to allocate memory for windows list of statistic access\n");
		return -ENOMEM;
	}
	for (i = 0; i < win_nr; ++i) {
		wins[i] = vzalloc(total_page_num * sizeof(u8));
		if (!wins[i]) {
			pr_err("unable to allocate memory for window of statistic access\n");
			for (j = 0; j < i; ++j) {
				vfree(wins[j]);
			}
			vfree(wins);
			return -ENOMEM;
		}
	}
	*sliding_windows = wins;
	return 0;
}

static int init_access_statistic_pid_2m(struct access_add_pid_payload *payload)
{
	struct statistics_tracking_info *tmp, *ap;
	int l1_node, l2_node, ret;
	unsigned long numa_nodes;

	if (!payload)
		return -EINVAL;

	numa_nodes = (unsigned long)payload->numa_nodes;
	l1_node = find_first_local_numa(&numa_nodes);
	l2_node = find_first_remote_numa(&numa_nodes);
	if (l1_node == NUMA_NO_NODE) {
		pr_err("invalid local node passed to init statistic access of pid: %d\n",
		       payload->pid);
		return -EINVAL;
	}

	down_write(&statistic_lock);
	list_for_each_entry_safe(ap, tmp, &statistic_pid_list, node) {
		if (ap->pid == payload->pid) {
			pr_info("statistic access pid: %d already exists, update info\n",
				payload->pid);
			list_del(&ap->node);
			destroy_access_statistic_pid(ap);
			break;
		}
	}
	up_write(&statistic_lock);

	tmp = kzalloc(sizeof(struct statistics_tracking_info), GFP_KERNEL);
	if (!tmp) {
		pr_err("unable to allocate memory for statistic tracking info\n");
		return -ENOMEM;
	}
	tmp->pid = payload->pid;
	tmp->l1_node = l1_node;
	tmp->l2_node = l2_node;
	tmp->scan_num = 0;
	ret = scan_hva_info(payload->pid, &tmp->page_num[L1],
			    &tmp->page_num[L2], &tmp->vaddr[L1],
			    &tmp->vaddr[L2]);
	if (ret) {
		pr_err("failed to scan hva info for statistic tracking\n");
		kfree(tmp);
		return ret;
	}

	ret = init_statistic_window(&tmp->sliding_windows, payload->duration,
				    payload->scan_time,
				    tmp->page_num[L1] + tmp->page_num[L2],
				    &tmp->window_num);
	if (ret) {
		pr_err("failed to init statistic tracking windows\n");
		kfree(tmp);
		return ret;
	}

	down_write(&statistic_lock);
	list_add_tail(&tmp->node, &statistic_pid_list);
	up_write(&statistic_lock);

	pr_info("init statistic access tracking, pid: %d, local page number: %llu, remote page number: %llu\n",
		tmp->pid, tmp->page_num[L1], tmp->page_num[L2]);
	return 0;
}

static int init_access_statistic_pid_4k(struct access_add_pid_payload *payload)
{
	struct statistics_tracking_info *tmp, *ap;
	int l1_node, l2_node, ret;
	unsigned long numa_nodes;

	if (!payload)
		return -EINVAL;

	numa_nodes = (unsigned long)payload->numa_nodes;
	l1_node = find_first_local_numa(&numa_nodes);
	l2_node = find_first_remote_numa(&numa_nodes);
	if (l1_node == NUMA_NO_NODE) {
		pr_err("invalid local node passed to init statistic access of pid: %d\n",
		       payload->pid);
		return -EINVAL;
	}

	down_write(&statistic_lock);
	list_for_each_entry_safe(ap, tmp, &statistic_pid_list, node) {
		if (ap->pid == payload->pid) {
			pr_info("statistic access pid: %d already exists, update info\n",
				payload->pid);
			list_del(&ap->node);
			destroy_access_statistic_pid(ap);
			break;
		}
	}
	up_write(&statistic_lock);

	tmp = kzalloc(sizeof(struct statistics_tracking_info), GFP_KERNEL);
	if (!tmp) {
		pr_err("unable to allocate memory for statistic tracking info\n");
		return -ENOMEM;
	}
	tmp->pid = payload->pid;
	tmp->l1_node = l1_node;
	tmp->l2_node = l2_node;
	tmp->scan_num = 0;
	ret = scan_hva_info_4k(payload->pid, &tmp->page_num[L1],
			       &tmp->page_num[L2], &tmp->vaddr[L1],
			       &tmp->vaddr[L2]);
	if (ret) {
		pr_err("failed to scan hva info for statistic tracking\n");
		kfree(tmp);
		return ret;
	}

	ret = init_statistic_window(&tmp->sliding_windows, payload->duration,
				    payload->scan_time,
				    tmp->page_num[L1] + tmp->page_num[L2],
				    &tmp->window_num);
	if (ret) {
		pr_err("failed to init statistic tracking windows\n");
		kfree(tmp);
		return ret;
	}

	down_write(&statistic_lock);
	list_add_tail(&tmp->node, &statistic_pid_list);
	up_write(&statistic_lock);

	pr_info("init statistic access tracking, pid: %d, local page number: %llu, remote page number: %llu\n",
		tmp->pid, tmp->page_num[L1], tmp->page_num[L2]);
	return 0;
}

static int init_access_statistic_pid(struct access_add_pid_payload *payload,
				     int page_size)
{
	if (page_size == g_pagesize_huge)
		return init_access_statistic_pid_2m(payload);
	else if (page_size == PAGE_SIZE)
		return init_access_statistic_pid_4k(payload);
	else
		return -EINVAL;
}

int access_add_statistic_pid(int len, struct access_add_pid_payload *payload,
			     int page_size)
{
	int ret = 0;
	int i;
	for (i = 0; i < len; i++) {
		if (payload[i].type != STATISTIC_SCAN)
			continue;
		if (payload[i].duration > MAX_SCAN_DURATION_SEC) {
			pr_err("invalid scan duration: %u of pid: %d passed to add statistic access tracking\n",
			       payload[i].duration, payload[i].pid);
			return -EINVAL;
		}
		pr_debug("init access statistic pid %d\n", payload[i].pid);
		ret = init_access_statistic_pid(&payload[i], page_size);
		if (ret) {
			pr_err("failed to add statistic access pid: %d\n",
			       payload[i].pid);
			return ret;
		}
	}
	return ret;
}

static void move_to_ap_data_list(struct list_head *tmp_head)
{
	struct access_pid *ap, *tmp, *tmp2;
	LIST_HEAD(dup_head);

	down_write(&ap_data.lock);
	/* check if pids to be added already exists in ap_data.list */
	list_for_each_entry_safe(ap, tmp, tmp_head, node) {
		list_for_each_entry(tmp2, &ap_data.list, node) {
			if (ap->pid == tmp2->pid) {
				pr_info("set pid: %d NUMA mask from %#x to %#x\n",
					ap->pid, tmp2->numa_nodes,
					ap->numa_nodes);
				tmp2->numa_nodes = ap->numa_nodes;
				tmp2->scan_time = ap->scan_time;
				tmp2->ntimes = ap->ntimes;
				tmp2->type = ap->type;
				list_move_tail(&ap->node, &dup_head);
				break;
			}
		}
	}
	/* move all new pids to ap_data.list */
	list_for_each_entry_safe(ap, tmp, tmp_head, node) {
		/*
		 * A new pid may be attached during scan period or migrate period,
		 * if in migrate period, no need to call submit_one_work;
		 * if in scan period, set ap->cur_times refer to ap_head->cur_times and ap_head->ntimes,
		 * if cur_times + SCAN_TIMES_NEEDED_BY_NEW_PID < ap->ntimes, no need to call submit_one_work
		 */
		if (list_empty(&ap_data.list)) {
			ap->cur_times = ap->ntimes;
		} else {
			struct access_pid *ap_head = list_first_entry(
				&ap_data.list, struct access_pid, node);
			if (ap_head->ntimes != 0) {
				ap->cur_times = ap_head->cur_times *
						ap->ntimes / ap_head->ntimes;
			} else {
				pr_warn("scan times of the first entry in access pid list had been setted to 0\n");
				ap->cur_times = ap->ntimes;
			}
		}
		if ((ap->cur_times + SCAN_TIMES_NEEDED_BY_NEW_PID) <=
		    ap->ntimes) {
			submit_one_work(ap);
		} else {
			complete(&ap->work_done);
		}
		list_move_tail(&ap->node, &ap_data.list);
	}
	up_write(&ap_data.lock);
	list_for_each_entry_safe(ap, tmp, &dup_head, node) {
		list_del(&ap->node);
		destroy_access_pid(ap);
	}
}

int access_add_pid(int len, struct access_add_pid_payload *payload)
{
	int i, ret;
	struct access_pid *ap, *tmp;
	LIST_HEAD(tmp_head);

	for (i = 0; i < len; i++) {
		if (payload[i].pid == NON_EXIST_PID) {
			continue;
		}
		/* check if payload has duplicate pid */
		list_for_each_entry(tmp, &tmp_head, node) {
			if (unlikely(payload[i].pid == tmp->pid)) {
				ret = -EINVAL;
				goto err;
			}
		}
		/* allocate memory for pid */
		ret = init_access_pid(&payload[i], &ap);
		if (ret)
			goto err;
		list_add_tail(&ap->node, &tmp_head);
	}
	move_to_ap_data_list(&tmp_head);
	return 0;

err:
	list_for_each_entry_safe(ap, tmp, &tmp_head, node) {
		list_del(&ap->node);
		destroy_access_pid(ap);
	}

	return ret;
}

void access_remove_ham_pid(int len, struct access_remove_pid_payload *payload)
{
	int i, ret;
	struct ham_tracking_info *ap, *tmp;
	char path[MAX_PATH_LENGTH];

	spin_lock(&ham_lock);
	for (i = 0; i < len; i++) {
		list_for_each_entry_safe(ap, tmp, &ham_pid_list, node) {
			if (ap->pid != payload[i].pid) {
				continue;
			}
			ret = scnprintf(path, sizeof(path), "%d_t", ap->pid);
			if (!ret) {
				pr_err("write pid to path of pid: %d failed\n",
				       ap->pid);
				break;
			}
			remove_proc_entry("tracking_info", ap->pde);
			remove_proc_entry(path, NULL);
			list_del(&ap->node);
			destroy_access_ham_pid(ap);
		}
	}
	spin_unlock(&ham_lock);
}

void access_remove_statistic_pid(int len,
				 struct access_remove_pid_payload *payload)
{
	int i;
	struct statistics_tracking_info *ap, *tmp;

	down_write(&statistic_lock);
	for (i = 0; i < len; i++) {
		list_for_each_entry_safe(ap, tmp, &statistic_pid_list, node) {
			if (ap->pid == payload[i].pid) {
				list_del(&ap->node);
				destroy_access_statistic_pid(ap);
				break;
			}
		}
	}
	up_write(&statistic_lock);
}

void access_remove_pid(int len, struct access_remove_pid_payload *payload)
{
	int i;
	struct access_pid *ap, *tmp;

	down_write(&ap_data.lock);
	for (i = 0; i < len; i++) {
		list_for_each_entry_safe(ap, tmp, &ap_data.list, node) {
			if (ap->pid == payload[i].pid) {
				list_del(&ap->node);
				cancel_ap_scan_work(ap);
				destroy_access_pid(ap);
				break;
			}
		}
	}
	up_write(&ap_data.lock);
}

void access_remove_all_pid(void)
{
	struct access_pid *ap, *tmp;
	struct ham_tracking_info *ap_ham, *tmp_ham;
	struct statistics_tracking_info *ap_statistic, *tmp_statistic;
	char path[MAX_PATH_LENGTH];

	pr_info("remove all pid\n");
	down_write(&ap_data.lock);
	list_for_each_entry_safe(ap, tmp, &ap_data.list, node) {
		list_del(&ap->node);
		cancel_ap_scan_work(ap);
		destroy_access_pid(ap);
	}
	up_write(&ap_data.lock);

	int ret;
	spin_lock(&ham_lock);
	list_for_each_entry_safe(ap_ham, tmp_ham, &ham_pid_list, node) {
		ret = scnprintf(path, sizeof(path), "%d_t", ap_ham->pid);
		if (!ret)
			continue;
		remove_proc_entry("tracking_info", ap_ham->pde);
		remove_proc_entry(path, NULL);
		list_del(&ap_ham->node);
		destroy_access_ham_pid(ap_ham);
	}
	spin_unlock(&ham_lock);

	down_write(&statistic_lock);
	list_for_each_entry_safe(ap_statistic, tmp_statistic,
				 &statistic_pid_list, node) {
		list_del(&ap_statistic->node);
		destroy_access_statistic_pid(ap_statistic);
	}
	up_write(&statistic_lock);
}

static inline void free_ap_bm(struct access_pid *ap)
{
	int i;
	if (!ap) {
		return;
	}
	for (i = 0; i < SMAP_MAX_NUMNODES; i++) {
		vfree(ap->paddr_bm[i]);
		ap->paddr_bm[i] = NULL;
		ap->bm_len[i] = 0;
	}
}

static void free_ap_white_list_bm(struct access_pid *ap)
{
	int i;
	if (!ap) {
		return;
	}
	for (i = 0; i < SMAP_MAX_NUMNODES; i++) {
		vfree(ap->paddr_bm[i]);
		ap->paddr_bm[i] = NULL;
		vfree(ap->white_list_bm[i]);
		ap->white_list_bm[i] = NULL;
		ap->bm_len[i] = 0;
	}
}

static int init_ap_bm(int node_len, u64 *node_page_count, struct access_pid *ap)
{
	size_t nr_bytes = sizeof(unsigned long);
	int i;

	for (i = 0; i < SMAP_MAX_NUMNODES; i++) {
		ap->page_num[i] = 0;
		ap->bm_len[i] = BITS_TO_LONGS(node_page_count[i]);
		if (ap->bm_len[i] == 0) {
			continue;
		}
		ap->paddr_bm[i] = vzalloc(ap->bm_len[i] * nr_bytes);
		if (!ap->paddr_bm[i]) {
			pr_err("unable to allocate memory for bitmap on node %d of pid: %d\n",
			       i, ap->pid);
			free_ap_bm(ap);
			return -ENOMEM;
		}

		ap->white_list_bm[i] = vzalloc(ap->bm_len[i] * nr_bytes);
		if (!ap->white_list_bm[i]) {
			pr_err("unable to allocate memory for white list bitmap on node %d of pid: %d\n",
			       i, ap->pid);
			free_ap_white_list_bm(ap);
			return -ENOMEM;
		}
	}

	return 0;
}

void change_ap_type(pid_t pid)
{
	struct access_pid *tmp;
	down_write(&ap_data.lock);
	list_for_each_entry(tmp, &ap_data.list, node) {
		if (tmp->pid == pid) {
			tmp->type = NO_SCAN;
			pr_info("change scan type of pid: %d successfully\n",
				pid);
			break;
		}
	}
	up_write(&ap_data.lock);
}

void clean_last_ap_data(struct access_pid *ap)
{
	int i;
	for (i = 0; i < SMAP_MAX_NUMNODES; i++) {
		vfree(ap->paddr_bm[i]);
		ap->paddr_bm[i] = NULL;
		vfree(ap->white_list_bm[i]);
		ap->white_list_bm[i] = NULL;
		ap->bm_len[i] = 0;
		ap->page_num[i] = 0;
	}
	if (ap->info.mapping) {
		vfree(ap->info.mapping);
		ap->info.mapping = NULL;
	}
}

int access_walk_pagemap(struct access_pid *ap)
{
	int ret;
	struct access_tracking_dev *adev;
	u64 nodes_page_count[SMAP_MAX_NUMNODES] = { 0 };
	struct pagemapread pm = { 0 };
	if (!ap) {
		return -EINVAL;
	}
	if (ap->type != NORMAL_SCAN) {
		return 0;
	}
	list_for_each_entry(adev, &access_dev, list) {
		down_read(&adev->buffer_lock);
		nodes_page_count[adev->node] = adev->page_count;
		up_read(&adev->buffer_lock);
	}
	clean_last_ap_data(ap);
	ret = init_ap_bm(SMAP_MAX_NUMNODES, nodes_page_count, ap);
	if (ret) {
		pr_err("unable to init access bitmap for pid: %d\n", ap->pid);
		return ret;
	}
	ret = init_vm_mapping(&ap->info);
	if (ret)
		return ret;
	pm.ap = ap;
	pm.mig_info.pid = ap->pid;
	pm.mig_type = NORMAL_MIGRATE;
	walk_pid_pagemap(&pm);

	return 0;
}

struct access_pid *find_access_pid(pid_t pid)
{
	struct access_pid *ap;
	down_read(&ap_data.lock);
	list_for_each_entry(ap, &ap_data.list, node) {
		if (ap->pid == pid) {
			up_read(&ap_data.lock);
			return ap;
		}
	}
	up_read(&ap_data.lock);
	return NULL;
}

int read_pid_freq(pid_t pid, size_t *data_len, actc_t **data)
{
	int i;
	u32 index;
	size_t acidx, bm_len;
	struct access_pid *ap;
	struct access_tracking_dev *adev;

	if (!data_len || !data) {
		pr_err("null data pointer passed to pid frequency read\n");
		return -EINVAL;
	}

	ap = find_access_pid(pid);
	if (!ap) {
		pr_err("unable to find pid: %d in access pid list\n", pid);
		return -EINVAL;
	}

	pr_debug("start writing pid %d freq\n", pid);
	for (i = 0; i < SMAP_MAX_NUMNODES; i++) {
		if (ap->page_num[i] == 0 || !ap->paddr_bm[i] || !data[i]) {
			continue;
		}
		list_for_each_entry(adev, &access_dev, list) {
			if (adev->node == i) {
				break;
			}
		}
		if (list_entry_is_head(adev, &access_dev, list)) {
			continue;
		}
		down_read(&adev->buffer_lock);
		bm_len = BITS_PER_LONG * ap->bm_len[i];
		for (index = acidx = 0; acidx < bm_len && index < data_len[i];
		     acidx++) {
			if (!test_bit(acidx, ap->paddr_bm[i])) {
				continue;
			}
			if (unlikely(acidx >= adev->page_count)) {
				pr_warn("exceeds total page amount: %llu when lookup access index: %zu on access device: %d\n",
					adev->page_count, acidx, i);
				break;
			}
			data[i][index++] = adev->access_bit_actc_data[acidx];
			pr_debug("Node%d acidx %zu index %u\n", i, acidx,
				 index - 1);
		}
		up_read(&adev->buffer_lock);
	}
	return 0;
}

struct absolute_pos {
	unsigned long last_pos;
	unsigned long pos;
	long last_index;
	long index;
};

static int calc_absolute_pos(unsigned long *paddr_bm, size_t bm_len,
			     struct absolute_pos *abs_pos)
{
	unsigned long tmp_pos = abs_pos->last_pos + 1;
	long last_index = abs_pos->last_index;
	size_t nr_bm_bits = BITS_PER_BYTE * sizeof(unsigned long) * bm_len;

	if (!paddr_bm || bm_len == 0) {
		return -EINVAL;
	}

	if (unlikely(last_index == abs_pos->index)) {
		abs_pos->pos = abs_pos->last_pos;
		return 0;
	}
	if (unlikely(last_index > abs_pos->index)) {
		return -EINVAL;
	}

	while (tmp_pos < nr_bm_bits) {
		tmp_pos = find_next_bit(paddr_bm, nr_bm_bits, tmp_pos);
		if (tmp_pos == nr_bm_bits) {
			return -ERANGE;
		}
		if (++last_index == abs_pos->index) {
			abs_pos->pos = tmp_pos;
			return 0;
		}
		tmp_pos++;
	}

	return -ERANGE;
}

#define INVALID_PADDR 0
#define INITIAL_POS ULONG_MAX
#define INITIAL_POS_INDEX (-1)

static int check_parameters_and_state(u64 len, u64 *addr)
{
	if (len == 0 || !addr) {
		pr_err("invalid length or address passed to check\n");
		return -EINVAL;
	}
	if (!check_and_clear_ap_state(&ap_data, AP_STATE_MIG)) {
		pr_err("ap_data is a state that not allowed to migrate\n");
		return -EAGAIN;
	}
	return 0;
}

static void convert_single_address(struct access_pid *ap, int nid,
				   int page_size, u64 *addr,
				   struct absolute_pos *abs_pos)
{
	int ret;

	/* firstly, calculate absolute position by relative position */
	abs_pos->index = *addr;
	ret = calc_absolute_pos(ap->paddr_bm[nid], ap->bm_len[nid], abs_pos);
	if (ret < 0) {
		pr_err("calculate pid %d %lluth page pos failed\n", ap->pid,
		       *addr);
		*addr = INVALID_PADDR;
		return;
	}

	abs_pos->last_index = abs_pos->index;
	abs_pos->last_pos = abs_pos->pos;
	pr_debug("convert node%d index %ld acidx %lu\n", nid, abs_pos->index,
		 abs_pos->pos);
	/* Secondly, calculate physical address by absolute position */
	if (nid < nr_local_numa) {
		ret = calc_acidx_paddr_acpi(nid, abs_pos->pos, addr, page_size);
	} else {
		ret = calc_acidx_paddr_iomem(nid, abs_pos->pos, addr,
					     page_size);
	}

	if (ret) {
		pr_err("calculate acidx %lx paddr failed\n", abs_pos->index);
		*addr = INVALID_PADDR;
	}
}

/* Caller must ensures that addr is in ascending order */
int convert_pos_to_paddr_sorted(pid_t pid, int nid, u64 len, u64 *addr)
{
	u64 i;
	int ret;
	bool found = false;
	struct access_pid *ap;
	struct absolute_pos abs_pos = { .last_pos = INITIAL_POS,
					.pos = INVALID_PADDR,
					.last_index = INITIAL_POS_INDEX,
					.index = INITIAL_POS_INDEX };
	int page_size = is_access_hugepage() ? g_pagesize_huge : PAGE_SIZE;
	ret = check_parameters_and_state(len, addr);
	if (ret) {
		return ret;
	}

	down_read(&ap_data.lock);
	list_for_each_entry(ap, &ap_data.list, node) {
		if (ap->pid == pid) {
			found = true;
			break;
		}
	}

	if (!found) {
		pr_err("invalid pid: %d passed to convert position to PA\n",
		       pid);
		set_ap_whole_state(&ap_data, AP_STATE_WALK | AP_STATE_READ |
						     AP_STATE_FREQ |
						     AP_STATE_MIG);
		up_read(&ap_data.lock);
		return -EINVAL;
	}

	pr_debug("%llu addresses of pid: %d on node%d has been converted\n",
		 len, pid, nid);
	for (i = 0; i < len; i++) {
		convert_single_address(ap, nid, page_size, &addr[i], &abs_pos);
	}

	up_read(&ap_data.lock);
	set_ap_whole_state(&ap_data, AP_STATE_WALK | AP_STATE_READ |
					     AP_STATE_FREQ | AP_STATE_MIG);
	return 0;
}
EXPORT_SYMBOL(convert_pos_to_paddr_sorted);
