// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * Description: SMAP MEXT kernel fault/eviction mmap ring & proactive prefault
 */

#include <linux/atomic.h>
#include <linux/errno.h>
#include <linux/kprobes.h>
#include <linux/ktime.h>
#include <linux/mm.h>
#include <linux/mmap_lock.h>
#include <linux/mutex.h>
#include <linux/pid.h>
#include <linux/ptrace.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/sched/task.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/vmalloc.h>

#include "mext_fault_ring.h"

#undef pr_fmt
#define pr_fmt(fmt) "SMAP_mext: " fmt

#define MEXT_PID_HASH_BITS 9U
#define MEXT_PID_HASH_SIZE (1U << MEXT_PID_HASH_BITS)
#define MEXT_PID_HASH_MASK (MEXT_PID_HASH_SIZE - 1U)
#define MEXT_CAS_MAX_RETRY 8

static struct mext_ring_buffer *g_mext_ring;
static size_t g_mext_ring_alloc_bytes;

static DEFINE_SPINLOCK(g_ctrl_lock);
static DEFINE_MUTEX(g_kprobe_mutex);
static s32 g_tracked_pids[MEXT_PID_HASH_SIZE];
static bool g_track_all_pids;
static bool g_kprobe_registered;
static bool g_capture_enabled;

static atomic64_t g_faults_captured = ATOMIC64_INIT(0);
static atomic64_t g_evicts_captured = ATOMIC64_INIT(0);
static atomic64_t g_ring_dropped = ATOMIC64_INIT(0);
static atomic64_t g_prefault_succeeded = ATOMIC64_INIT(0);
static atomic64_t g_swapout_epoch = ATOMIC64_INIT(0);

static inline u32 mext_hash_pid(u32 pid)
{
	return (pid * 2654435761U) & MEXT_PID_HASH_MASK;
}

static inline bool mext_is_pid_tracked(pid_t pid)
{
	u32 slot, i;
	s32 entry;

	if (READ_ONCE(g_track_all_pids))
		return true;
	if (pid <= 0)
		return false;

	slot = mext_hash_pid((u32)pid);
	for (i = 0; i < MEXT_PID_HASH_SIZE; i++) {
		entry = READ_ONCE(g_tracked_pids[(slot + i) & MEXT_PID_HASH_MASK]);
		if (entry == (s32)pid)
			return true;
		if (entry == 0)
			return false;
	}
	return false;
}

static int mext_pid_filter_add(pid_t pid)
{
	u32 slot, i, idx;
	s32 first_tombstone = -1;

	if (pid == 0) {
		WRITE_ONCE(g_track_all_pids, true);
		return 0;
	}
	if (pid < 0)
		return -EINVAL;

	slot = mext_hash_pid((u32)pid);
	for (i = 0; i < MEXT_PID_HASH_SIZE; i++) {
		idx = (slot + i) & MEXT_PID_HASH_MASK;
		if (g_tracked_pids[idx] == (s32)pid)
			return 0;
		if (g_tracked_pids[idx] == -1 && first_tombstone < 0)
			first_tombstone = (s32)idx;
		if (g_tracked_pids[idx] == 0) {
			if (first_tombstone >= 0)
				idx = (u32)first_tombstone;
			WRITE_ONCE(g_tracked_pids[idx], (s32)pid);
			return 0;
		}
	}
	if (first_tombstone >= 0) {
		WRITE_ONCE(g_tracked_pids[first_tombstone], (s32)pid);
		return 0;
	}
	return -ENOSPC;
}

static void mext_pid_filter_remove(pid_t pid)
{
	u32 slot, i, idx;

	if (pid == 0) {
		WRITE_ONCE(g_track_all_pids, false);
		return;
	}
	if (pid < 0)
		return;

	slot = mext_hash_pid((u32)pid);
	for (i = 0; i < MEXT_PID_HASH_SIZE; i++) {
		idx = (slot + i) & MEXT_PID_HASH_MASK;
		if (g_tracked_pids[idx] == (s32)pid) {
			WRITE_ONCE(g_tracked_pids[idx], -1);
			return;
		}
		if (g_tracked_pids[idx] == 0)
			return;
	}
}

static void mext_pid_filter_clear(void)
{
	int i;

	WRITE_ONCE(g_track_all_pids, false);
	for (i = 0; i < MEXT_PID_HASH_SIZE; i++)
		WRITE_ONCE(g_tracked_pids[i], 0);
}

/*
 * Lockless multi-producer push safe in kprobe / atomic contexts.
 */
static bool mext_ring_push(u32 type, u32 pid, u64 vpn, u64 aux)
{
	struct mext_ring_header *hdr;
	struct mext_ring_event *ev;
	u64 w, tail, prev;
	int tries;

	if (!g_mext_ring)
		return false;

	hdr = &g_mext_ring->hdr;
	for (tries = 0; tries < MEXT_CAS_MAX_RETRY; tries++) {
		w = READ_ONCE(hdr->write_idx);
		tail = smp_load_acquire(&hdr->tail);
		if (w - tail >= MEXT_RING_CAPACITY) {
			atomic64_inc(&g_ring_dropped);
			WRITE_ONCE(hdr->dropped,
				   (u64)atomic64_read(&g_ring_dropped));
			return false;
		}
		prev = cmpxchg(&hdr->write_idx, w, w + 1);
		if (prev == w) {
			ev = &g_mext_ring->events[w & MEXT_RING_MASK];
			if (unlikely(READ_ONCE(ev->type) != MEXT_EVT_NONE)) {
				atomic64_inc(&g_ring_dropped);
				WRITE_ONCE(hdr->dropped,
					   (u64)atomic64_read(&g_ring_dropped));
				return false;
			}
			WRITE_ONCE(ev->pid, pid);
			WRITE_ONCE(ev->vpn, vpn);
			WRITE_ONCE(ev->aux, aux);
			WRITE_ONCE(ev->timestamp, ktime_get_ns());
			smp_store_release(&ev->type, type);
			return true;
		}
	}

	atomic64_inc(&g_ring_dropped);
	WRITE_ONCE(hdr->dropped, (u64)atomic64_read(&g_ring_dropped));
	return false;
}

static int kp_do_swap_page_pre(struct kprobe *p, struct pt_regs *regs)
{
	struct vm_fault *vmf;
	struct vm_area_struct *vma;
	pid_t tgid;
	u64 vpn, vma_slot;

	if (!READ_ONCE(g_capture_enabled))
		return 0;

	tgid = current->tgid;
	if (!mext_is_pid_tracked(tgid))
		return 0;

#if defined(CONFIG_ARM64)
	vmf = (struct vm_fault *)regs->regs[0];
#elif defined(CONFIG_X86_64)
	vmf = (struct vm_fault *)regs->di;
#else
	vmf = (struct vm_fault *)regs_get_kernel_argument(regs, 0);
#endif
	if (unlikely(!vmf))
		return 0;

	vma = READ_ONCE(vmf->vma);
	if (unlikely(!vma))
		return 0;

	vpn = (u64)(READ_ONCE(vmf->address) >> PAGE_SHIFT);
	vma_slot = (u64)((READ_ONCE(vma->vm_start) >> 21) & 0x1fULL);

	if (mext_ring_push(MEXT_EVT_SWAP_FAULT, (u32)tgid, vpn, vma_slot))
		atomic64_inc(&g_faults_captured);

	return 0;
}

static struct kprobe g_swap_fault_kp = {
	.symbol_name = "do_swap_page",
	.pre_handler = kp_do_swap_page_pre,
};

u64 mext_next_swapout_epoch(void)
{
	u64 epoch = (u64)atomic64_inc_return(&g_swapout_epoch);

	if (g_mext_ring)
		WRITE_ONCE(g_mext_ring->hdr.swapout_epoch, epoch);
	return epoch;
}

void mext_record_eviction(pid_t pid, u64 vpn, u64 epoch)
{
	if (pid <= 0 || vpn == 0)
		return;
	if (mext_ring_push(MEXT_EVT_EVICTED, (u32)pid, vpn, epoch))
		atomic64_inc(&g_evicts_captured);
}

int mext_fault_ring_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long size;

	if (!g_mext_ring)
		return -ENODEV;
	if (vma->vm_pgoff != 0)
		return -EINVAL;

	size = vma->vm_end - vma->vm_start;
	if (size == 0 || size > g_mext_ring_alloc_bytes)
		return -EINVAL;

	return remap_vmalloc_range(vma, g_mext_ring, 0);
}

int mext_ioctl_prefault(void __user *argp)
{
	struct mext_prefault_msg msg;
	u64 *vpns;
	struct task_struct *task;
	struct mm_struct *mm;
	unsigned int gup_flags = 0;
	u32 i, succeeded = 0;

	if (copy_from_user(&msg, argp, sizeof(msg)))
		return -EFAULT;

	if (msg.pid <= 0 || msg.nr_pages == 0 ||
	    msg.nr_pages > MEXT_MAX_PREFAULT_PAGES || !msg.vpns_ptr)
		return -EINVAL;

	vpns = memdup_user((const void __user *)(uintptr_t)msg.vpns_ptr,
			   msg.nr_pages * sizeof(u64));
	if (IS_ERR(vpns))
		return PTR_ERR(vpns);

	rcu_read_lock();
	task = pid_task(find_vpid(msg.pid), PIDTYPE_PID);
	if (task)
		get_task_struct(task);
	rcu_read_unlock();
	if (!task) {
		kfree(vpns);
		return -ESRCH;
	}

	mm = get_task_mm(task);
	put_task_struct(task);
	if (!mm) {
		kfree(vpns);
		return -EINVAL;
	}

	if (msg.flags & MEXT_PREFAULT_FLAG_WRITE)
		gup_flags |= FOLL_WRITE;

	for (i = 0; i < msg.nr_pages; i++) {
		unsigned long vaddr = (unsigned long)(vpns[i] << PAGE_SHIFT);
		struct page *page = NULL;
		int locked = 1;
		long pinned;

		if (vaddr >= TASK_SIZE)
			continue;

		mmap_read_lock(mm);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 5, 0)
		pinned = get_user_pages_remote(mm, vaddr, 1, gup_flags,
					       &page, &locked);
#else
		pinned = get_user_pages_remote(mm, vaddr, 1, gup_flags,
					       &page, NULL, &locked);
#endif
		if (pinned == 1 && page) {
			put_page(page);
			succeeded++;
		}
		if (locked)
			mmap_read_unlock(mm);

		if ((i & 31U) == 31U)
			cond_resched();
	}

	mmput(mm);
	kfree(vpns);

	atomic64_add(succeeded, &g_prefault_succeeded);
	msg.nr_succeeded = succeeded;
	if (copy_to_user(argp, &msg, sizeof(msg)))
		return -EFAULT;

	return 0;
}

static int mext_enable_kprobe_locked(void)
{
	int ret;

	if (!g_kprobe_registered) {
		ret = register_kprobe(&g_swap_fault_kp);
		if (ret < 0) {
			pr_warn("register_kprobe(do_swap_page) failed: %d\n",
				ret);
			return ret;
		}
		g_kprobe_registered = true;
	}
	WRITE_ONCE(g_capture_enabled, true);
	return 0;
}

int mext_ioctl_ctrl(void __user *argp)
{
	struct mext_ctrl_msg msg;
	int ret = 0;

	if (copy_from_user(&msg, argp, sizeof(msg)))
		return -EFAULT;

	switch (msg.op) {
	case MEXT_CTRL_ENABLE:
		mutex_lock(&g_kprobe_mutex);
		ret = mext_enable_kprobe_locked();
		mutex_unlock(&g_kprobe_mutex);
		if (ret)
			return ret;
		break;
	case MEXT_CTRL_DISABLE:
		mutex_lock(&g_kprobe_mutex);
		WRITE_ONCE(g_capture_enabled, false);
		mutex_unlock(&g_kprobe_mutex);
		break;
	case MEXT_CTRL_ADD_PID:
		spin_lock(&g_ctrl_lock);
		ret = mext_pid_filter_add(msg.pid);
		spin_unlock(&g_ctrl_lock);
		break;
	case MEXT_CTRL_REMOVE_PID:
		spin_lock(&g_ctrl_lock);
		mext_pid_filter_remove(msg.pid);
		spin_unlock(&g_ctrl_lock);
		break;
	case MEXT_CTRL_CLEAR_PIDS:
		spin_lock(&g_ctrl_lock);
		mext_pid_filter_clear();
		spin_unlock(&g_ctrl_lock);
		break;
	case MEXT_CTRL_GET_STATS:
		break;
	default:
		return -EINVAL;
	}

	msg.faults_captured = (u64)atomic64_read(&g_faults_captured);
	msg.evicts_captured = (u64)atomic64_read(&g_evicts_captured);
	msg.ring_dropped = (u64)atomic64_read(&g_ring_dropped);
	msg.prefault_succeeded = (u64)atomic64_read(&g_prefault_succeeded);

	if (copy_to_user(argp, &msg, sizeof(msg)))
		return -EFAULT;
	return ret;
}

int mext_fault_ring_init(void)
{
	g_mext_ring_alloc_bytes = PAGE_ALIGN(sizeof(struct mext_ring_buffer));
	g_mext_ring = vmalloc_user(g_mext_ring_alloc_bytes);
	if (!g_mext_ring) {
		pr_err("failed to allocate %zu bytes for MEXT ring\n",
		       g_mext_ring_alloc_bytes);
		return -ENOMEM;
	}

	g_mext_ring->hdr.magic = MEXT_RING_MAGIC;
	g_mext_ring->hdr.version = MEXT_RING_VERSION;
	g_mext_ring->hdr.capacity = MEXT_RING_CAPACITY;
	g_mext_ring->hdr.page_shift = PAGE_SHIFT;
	g_mext_ring->hdr.write_idx = 0;
	g_mext_ring->hdr.tail = 0;
	g_mext_ring->hdr.dropped = 0;
	g_mext_ring->hdr.swapout_epoch = 0;

	mext_pid_filter_clear();
	pr_info("MEXT fault/eviction ring initialized (%u slots, %zu bytes)\n",
		MEXT_RING_CAPACITY, g_mext_ring_alloc_bytes);
	return 0;
}

void mext_fault_ring_exit(void)
{
	WRITE_ONCE(g_capture_enabled, false);
	if (g_kprobe_registered) {
		unregister_kprobe(&g_swap_fault_kp);
		g_kprobe_registered = false;
	}
	if (g_mext_ring) {
		vfree(g_mext_ring);
		g_mext_ring = NULL;
	}
	pr_info("MEXT fault/eviction ring destroyed\n");
}
