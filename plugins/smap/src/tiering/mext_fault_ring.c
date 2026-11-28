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
#include <linux/mm_inline.h>
#include <linux/mmap_lock.h>
#include <linux/mmzone.h>
#include <linux/mutex.h>
#include <linux/pagewalk.h>
#include <linux/pid.h>
#include <linux/ptrace.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/sched/task.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/swap.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/vmstat.h>

#include "smap_migrate_wrapper.h"
#include "mext_fault_ring.h"

#undef pr_fmt
#define pr_fmt(fmt) "SMAP_mext: " fmt

#define MEXT_PID_HASH_BITS 13U
#define MEXT_PID_HASH_SIZE (1U << MEXT_PID_HASH_BITS) /* 8192 slots */
#define MEXT_PID_HASH_MASK (MEXT_PID_HASH_SIZE - 1U)
#define MEXT_PID_MAX_LOAD  ((MEXT_PID_HASH_SIZE * 3U) / 4U) /* 75% = 6144 */
#define MEXT_CAS_MAX_RETRY 8
#define MEXT_PRESSURE_NODE_SLOTS 64U
#define MEXT_PRESSURE_MIN_INTERVAL_NS 1000000ULL /* max 1 pressure wakeup / ms / node */
#define MEXT_PAGEOUT_BATCH 64U
#define MEXT_PAGEOUT_SCAN_LIMIT 32768U
#define MEXT_PAGEOUT_CURSOR_SLOTS 64U
#define MEXT_PAGEOUT_MIN_WRAP_NS 8000000000ULL /* min 8s KVM Stage-2 cold dwell */

struct mext_pageout_cursor {
	pid_t pid;
	unsigned long next_vaddr;
	u64 last_wrap_ns;
};

static DEFINE_SPINLOCK(g_pageout_cursor_lock);
static struct mext_pageout_cursor g_pageout_cursors[MEXT_PAGEOUT_CURSOR_SLOTS];

static struct mext_ring_buffer *g_mext_ring;
static size_t g_mext_ring_alloc_bytes;

static DEFINE_SPINLOCK(g_ctrl_lock);
static DEFINE_MUTEX(g_kprobe_mutex);
/*
 * Double-buffered lockless PID hash table: atomic kprobe readers access
 * g_tracked_pids[g_active_pid_tbl] wait-free in O(1), while writers compact
 * tombstones (-1) into the shadow table whenever load + tombstones reach 75%.
 */
static s32 g_tracked_pids[2][MEXT_PID_HASH_SIZE];
static int g_active_pid_tbl;
static u32 g_pid_live_cnt;
static u32 g_pid_tomb_cnt;
static bool g_track_all_pids;
static bool g_kprobe_registered;
static bool g_evict_kp_registered;
static bool g_evict_pvmw_registered;
static bool g_pressure_kp_registered;
static bool g_capture_enabled;

static atomic64_t g_faults_captured = ATOMIC64_INIT(0);
static atomic64_t g_evicts_captured = ATOMIC64_INIT(0);
static atomic64_t g_pressure_captured = ATOMIC64_INIT(0);
static atomic64_t g_ring_dropped = ATOMIC64_INIT(0);
static atomic64_t g_prefault_succeeded = ATOMIC64_INIT(0);
static atomic64_t g_swapout_epoch = ATOMIC64_INIT(0);
static atomic64_t g_last_pressure_ns[MEXT_PRESSURE_NODE_SLOTS];

static inline u32 mext_hash_pid(u32 pid)
{
	return (pid * 2654435761U) & MEXT_PID_HASH_MASK;
}

static inline bool mext_is_pid_tracked(pid_t pid)
{
	const s32 *tbl;
	u32 slot, i;
	s32 entry;

	if (READ_ONCE(g_track_all_pids))
		return true;
	if (pid <= 0)
		return false;

	tbl = g_tracked_pids[smp_load_acquire(&g_active_pid_tbl) & 1];
	slot = mext_hash_pid((u32)pid);
	for (i = 0; i < MEXT_PID_HASH_SIZE; i++) {
		entry = READ_ONCE(tbl[(slot + i) & MEXT_PID_HASH_MASK]);
		if (entry == (s32)pid)
			return true;
		if (entry == 0)
			return false;
	}
	return false;
}

static void mext_pid_filter_compact_locked(void)
{
	int cur = g_active_pid_tbl & 1;
	int nxt = cur ^ 1;
	s32 *src = g_tracked_pids[cur];
	s32 *dst = g_tracked_pids[nxt];
	u32 i, slot, j, idx, live = 0;

	memset(dst, 0, sizeof(g_tracked_pids[0]));
	for (i = 0; i < MEXT_PID_HASH_SIZE; i++) {
		s32 p = src[i];

		if (p <= 0)
			continue;
		slot = mext_hash_pid((u32)p);
		for (j = 0; j < MEXT_PID_HASH_SIZE; j++) {
			idx = (slot + j) & MEXT_PID_HASH_MASK;
			if (dst[idx] == 0) {
				dst[idx] = p;
				live++;
				break;
			}
		}
	}
	g_pid_live_cnt = live;
	g_pid_tomb_cnt = 0;
	smp_store_release(&g_active_pid_tbl, nxt);
}

static int mext_pid_filter_add(pid_t pid)
{
	s32 *tbl;
	u32 slot, i, idx;
	s32 first_tombstone = -1;

	if (pid == 0) {
		WRITE_ONCE(g_track_all_pids, true);
		return 0;
	}
	if (pid < 0)
		return -EINVAL;

	if (g_pid_live_cnt >= MEXT_PID_MAX_LOAD)
		return -ENOSPC;
	if (g_pid_live_cnt + g_pid_tomb_cnt >= MEXT_PID_MAX_LOAD)
		mext_pid_filter_compact_locked();

	tbl = g_tracked_pids[g_active_pid_tbl & 1];
	slot = mext_hash_pid((u32)pid);
	for (i = 0; i < MEXT_PID_HASH_SIZE; i++) {
		idx = (slot + i) & MEXT_PID_HASH_MASK;
		if (tbl[idx] == (s32)pid)
			return 0;
		if (tbl[idx] == -1 && first_tombstone < 0)
			first_tombstone = (s32)idx;
		if (tbl[idx] == 0) {
			if (first_tombstone >= 0) {
				idx = (u32)first_tombstone;
				if (g_pid_tomb_cnt > 0)
					g_pid_tomb_cnt--;
			}
			WRITE_ONCE(tbl[idx], (s32)pid);
			g_pid_live_cnt++;
			return 0;
		}
	}
	return -ENOSPC;
}

static void mext_pid_filter_remove(pid_t pid)
{
	s32 *tbl;
	u32 slot, i, idx;

	if (pid == 0) {
		WRITE_ONCE(g_track_all_pids, false);
		return;
	}
	if (pid < 0)
		return;

	tbl = g_tracked_pids[g_active_pid_tbl & 1];
	slot = mext_hash_pid((u32)pid);
	for (i = 0; i < MEXT_PID_HASH_SIZE; i++) {
		idx = (slot + i) & MEXT_PID_HASH_MASK;
		if (tbl[idx] == (s32)pid) {
			WRITE_ONCE(tbl[idx], -1);
			if (g_pid_live_cnt > 0)
				g_pid_live_cnt--;
			g_pid_tomb_cnt++;
			if (g_pid_live_cnt == 0 && g_pid_tomb_cnt > 0)
				mext_pid_filter_compact_locked();
			return;
		}
		if (tbl[idx] == 0)
			return;
	}
}

static void mext_pid_filter_clear(void)
{
	WRITE_ONCE(g_track_all_pids, false);
	memset(g_tracked_pids, 0, sizeof(g_tracked_pids));
	g_pid_live_cnt = 0;
	g_pid_tomb_cnt = 0;
	smp_store_release(&g_active_pid_tbl, 0);
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
	if (unlikely(READ_ONCE(vma->vm_flags) &
		     (VM_PFNMAP | VM_IO | VM_SHARED | VM_SPECIAL)))
		return 0;

	vpn = (u64)(READ_ONCE(vmf->address) >> PAGE_SHIFT);
	/*
	 * Partition giant hypervisor guest-RAM VMAs (e.g. 2-4 GiB Firecracker
	 * GPA arenas) into 64 MiB region slots so concurrent guest subsystems
	 * and vCPUs do not collide in a single vma_start bucket.
	 */
	vma_slot = (u64)(((READ_ONCE(vmf->address) >> 26) ^
			  (READ_ONCE(vma->vm_start) >> 21)) & 0x1fULL);

	if (mext_ring_push(MEXT_EVT_SWAP_FAULT, (u32)tgid, vpn, vma_slot))
		atomic64_inc(&g_faults_captured);

	return 0;
}

static struct kprobe g_swap_fault_kp = {
	.symbol_name = "do_swap_page",
	.pre_handler = kp_do_swap_page_pre,
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
/*
 * Capture kernel/cgroup LRU swap-out unmappings (`try_to_unmap_one`) so
 * standalone MEXT populates SwapShadowMap (`MEXT_EVT_EVICTED`) even without
 * SMAP_MIG_SWAP_OUT, unlocking temporal co-eviction prediction across
 * fragmented Firecracker Guest Physical Addresses (GPA) and MAP_PRIVATE CoW VMAs.
 */
static int kp_try_to_unmap_one_pre(struct kprobe *p, struct pt_regs *regs)
{
	struct folio *folio;
	struct vm_area_struct *vma;
	struct mm_struct *mm;
	struct task_struct *owner;
	unsigned long addr;
	pid_t tgid;
	u64 epoch;

	if (!READ_ONCE(g_capture_enabled))
		return 0;

#if defined(CONFIG_ARM64)
	folio = (struct folio *)regs->regs[0];
	vma = (struct vm_area_struct *)regs->regs[1];
	addr = (unsigned long)regs->regs[2];
#elif defined(CONFIG_X86_64)
	folio = (struct folio *)regs->di;
	vma = (struct vm_area_struct *)regs->si;
	addr = (unsigned long)regs->dx;
#else
	folio = (struct folio *)regs_get_kernel_argument(regs, 0);
	vma = (struct vm_area_struct *)regs_get_kernel_argument(regs, 1);
	addr = (unsigned long)regs_get_kernel_argument(regs, 2);
#endif
	if (unlikely(!folio || !vma || !addr || addr >= TASK_SIZE))
		return 0;
	if (!folio_test_anon(folio) || !folio_test_swapcache(folio))
		return 0;

	mm = READ_ONCE(vma->vm_mm);
	if (unlikely(!mm))
		return 0;

	rcu_read_lock();
	owner = rcu_dereference(mm->owner);
	tgid = owner ? READ_ONCE(owner->tgid) : 0;
	rcu_read_unlock();

	if (tgid <= 0 || !mext_is_pid_tracked(tgid))
		return 0;

	/* ~2.09 ms temporal co-eviction epoch bucket */
	epoch = ktime_get_ns() >> 21;
	if (mext_ring_push(MEXT_EVT_EVICTED, (u32)tgid,
			   (u64)(addr >> PAGE_SHIFT), epoch))
		atomic64_inc(&g_evicts_captured);
	return 0;
}

static struct kprobe g_evict_kp = {
	.symbol_name = "try_to_unmap_one",
	.pre_handler = kp_try_to_unmap_one_pre,
};

/*
 * Cross-TU fallback eviction hook on `page_vma_mapped_walk` (`mm/page_vma_mapped.c`,
 * non-static global symbol that GCC `-O2` never inlines across `mm/rmap.o`).
 * Fires only on the initial entry (`!pvmw->pte && !pvmw->pmd`) for an isolated,
 * locked anonymous swapcache folio undergoing `shrink_folio_list -> try_to_unmap`.
 */
static int kp_page_vma_mapped_walk_pre(struct kprobe *p, struct pt_regs *regs)
{
	struct page_vma_mapped_walk *pvmw;
	struct vm_area_struct *vma;
	struct mm_struct *mm;
	struct task_struct *owner;
	struct page *page;
	struct folio *folio;
	unsigned long pfn, addr;
	pid_t tgid;
	u64 epoch;

	if (!READ_ONCE(g_capture_enabled))
		return 0;

#if defined(CONFIG_ARM64)
	pvmw = (struct page_vma_mapped_walk *)regs->regs[0];
#elif defined(CONFIG_X86_64)
	pvmw = (struct page_vma_mapped_walk *)regs->di;
#else
	pvmw = (struct page_vma_mapped_walk *)regs_get_kernel_argument(regs, 0);
#endif
	if (unlikely(!pvmw))
		return 0;
	if (READ_ONCE(pvmw->pte) || READ_ONCE(pvmw->pmd))
		return 0;

	vma = READ_ONCE(pvmw->vma);
	addr = READ_ONCE(pvmw->address);
	pfn = READ_ONCE(pvmw->pfn);
	if (unlikely(!vma || !addr || addr >= TASK_SIZE || !pfn_valid(pfn)))
		return 0;

	page = pfn_to_online_page(pfn);
	if (unlikely(!page || PageTail(page)))
		return 0;

	folio = page_folio(page);
	if (!folio_test_anon(folio) || !folio_test_swapcache(folio) ||
	    !folio_test_locked(folio) || folio_test_lru(folio))
		return 0;

	mm = READ_ONCE(vma->vm_mm);
	if (unlikely(!mm))
		return 0;

	rcu_read_lock();
	owner = rcu_dereference(mm->owner);
	tgid = owner ? READ_ONCE(owner->tgid) : 0;
	rcu_read_unlock();

	if (tgid <= 0 || !mext_is_pid_tracked(tgid))
		return 0;

	epoch = ktime_get_ns() >> 21;
	if (mext_ring_push(MEXT_EVT_EVICTED, (u32)tgid,
			   (u64)(addr >> PAGE_SHIFT), epoch))
		atomic64_inc(&g_evicts_captured);
	return 0;
}

static struct kprobe g_evict_pvmw_kp = {
	.symbol_name = "page_vma_mapped_walk",
	.pre_handler = kp_page_vma_mapped_walk_pre,
};
#endif

/*
 * Early zone pressure hook on wakeup_kswapd(zone, gfp_flags, order, ...).
 * Rate-limited to at most 1 event per ms per NUMA node so user-space SMAP
 * Proactive Reclaim wakes up in microseconds ahead of __alloc_pages_slowpath.
 */
static int kp_wakeup_kswapd_pre(struct kprobe *p, struct pt_regs *regs)
{
	struct zone *zone;
	int order;
	u32 nid, slot;
	u64 now_ns, prev_ns, free_pages, wmark_low, aux;

	if (!READ_ONCE(g_capture_enabled))
		return 0;

#if defined(CONFIG_ARM64)
	zone = (struct zone *)regs->regs[0];
	order = (int)regs->regs[2];
#elif defined(CONFIG_X86_64)
	zone = (struct zone *)regs->di;
	order = (int)regs->dx;
#else
	zone = (struct zone *)regs_get_kernel_argument(regs, 0);
	order = (int)regs_get_kernel_argument(regs, 2);
#endif
	if (unlikely(!zone))
		return 0;

	nid = (u32)zone_to_nid(zone);
	slot = nid & (MEXT_PRESSURE_NODE_SLOTS - 1U);
	now_ns = ktime_get_ns();
	prev_ns = (u64)atomic64_read(&g_last_pressure_ns[slot]);
	if (now_ns - prev_ns < MEXT_PRESSURE_MIN_INTERVAL_NS)
		return 0;
	if ((u64)atomic64_cmpxchg(&g_last_pressure_ns[slot],
				 (s64)prev_ns, (s64)now_ns) != prev_ns)
		return 0;

	free_pages = (u64)zone_page_state(zone, NR_FREE_PAGES);
	wmark_low = (u64)low_wmark_pages(zone);
	aux = (wmark_low << 16) | ((u64)(u32)order & 0xffffULL);

	if (mext_ring_push(MEXT_EVT_MEM_PRESSURE, nid, free_pages, aux)) {
		u64 cnt = (u64)atomic64_inc_return(&g_pressure_captured);

		if (g_mext_ring)
			WRITE_ONCE(g_mext_ring->hdr.pressure_events, cnt);
	}
	return 0;
}

static struct kprobe g_pressure_kp = {
	.symbol_name = "wakeup_kswapd",
	.pre_handler = kp_wakeup_kswapd_pre,
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

struct mext_pageout_walk_ctx {
	struct folio *folios[MEXT_PAGEOUT_BATCH];
	u64 vpns[MEXT_PAGEOUT_BATCH];
	u32 batch_count;
	u32 max_batch;
	u32 scanned_present;
	unsigned long next_vaddr;
	bool strict_cold;
};

static int mext_pageout_test_walk(unsigned long addr, unsigned long next,
				  struct mm_walk *walk)
{
	struct vm_area_struct *vma = walk->vma;

	if (!vma || is_vm_hugetlb_page(vma))
		return 1;
	/*
	 * Allow private writable VMAs (both pure anonymous `00:00 0` and CoW
	 * `MAP_PRIVATE` VMAs such as Firecracker `/dev/ublkb2` snapshots, where
	 * `folio_test_anon` selects only dirty CoW anonymous folios), while
	 * skipping shared rings (`kvm-vcpu`, `io_uring`, `memfd`), locked, and
	 * special device mappings.
	 */
	if (!(vma->vm_flags & VM_WRITE) ||
	    (vma->vm_flags &
	     (VM_LOCKED | VM_SPECIAL | VM_PFNMAP | VM_IO | VM_SHARED)))
		return 1;
	return 0;
}

static int mext_pageout_pte_entry(pte_t *ptep, unsigned long addr,
				  unsigned long next, struct mm_walk *walk)
{
	struct mext_pageout_walk_ctx *ctx = walk->private;
	pte_t pte = ptep_get(ptep);
	unsigned long pfn;
	struct page *page;
	struct folio *folio;

	ctx->next_vaddr = addr + PAGE_SIZE;
	if (!pte_present(pte))
		return 0;

	ctx->scanned_present++;
	if (ctx->strict_cold && pte_young(pte)) {
		if (ctx->scanned_present >= MEXT_PAGEOUT_SCAN_LIMIT)
			return 1;
		return 0;
	}

	pfn = pte_pfn(pte);
	if (!pfn_valid(pfn))
		return 0;

	page = pfn_to_online_page(pfn);
	if (!page || PageTail(page) || PageTransHuge(page) || PageHuge(page))
		return 0;

	folio = page_folio(page);
	if (!folio_test_lru(folio) || !folio_test_anon(folio) ||
	    folio_test_unevictable(folio) || folio_likely_mapped_shared(folio))
		return 0;

	if (ctx->strict_cold &&
	    (folio_test_referenced(folio) || folio_test_young(folio))) {
		if (ctx->scanned_present >= MEXT_PAGEOUT_SCAN_LIMIT)
			return 1;
		return 0;
	}

	if (!folio_try_get(folio))
		return 0;

	if (unlikely(page_folio(page) != folio || !folio_test_lru(folio))) {
		folio_put(folio);
		return 0;
	}

	ctx->folios[ctx->batch_count] = folio;
	ctx->vpns[ctx->batch_count] = (u64)(addr >> PAGE_SHIFT);
	ctx->batch_count++;

	if (ctx->batch_count >= ctx->max_batch ||
	    ctx->scanned_present >= MEXT_PAGEOUT_SCAN_LIMIT)
		return 1;
	return 0;
}

static const struct mm_walk_ops mext_pageout_walk_ops = {
	.test_walk = mext_pageout_test_walk,
	.pte_entry = mext_pageout_pte_entry,
};

static u32 mext_pageout_sweep_mm(struct mm_struct *mm, pid_t pid,
				 u32 target_pages, bool strict_cold,
				 bool allow_wrap, u64 epoch,
				 unsigned long *cursor_inout,
				 bool *did_wrap_out)
{
	unsigned long initial_addr = *cursor_inout;
	unsigned long cur_addr = initial_addr;
	u32 reclaimed = 0;
	u32 total_scanned = 0;
	bool wrapped = (initial_addr == 0);

	if (did_wrap_out)
		*did_wrap_out = false;

	while (reclaimed < target_pages &&
	       total_scanned < MEXT_PAGEOUT_SCAN_LIMIT) {
		struct mext_pageout_walk_ctx wctx = { 0 };
		u64 isolated_vpns[MEXT_PAGEOUT_BATCH];
		u32 isolated_cnt = 0;
		u32 k;
		int walk_ret;
		LIST_HEAD(folio_list);

		wctx.max_batch = min_t(u32, MEXT_PAGEOUT_BATCH,
				       target_pages - reclaimed);
		wctx.next_vaddr = cur_addr;
		wctx.strict_cold = strict_cold;

		if (!mmap_read_trylock(mm))
			break;
		walk_ret = walk_page_range(mm, cur_addr, TASK_SIZE,
					   &mext_pageout_walk_ops, &wctx);
		mmap_read_unlock(mm);

		total_scanned += wctx.scanned_present;
		if (wctx.batch_count == 0) {
			if (!wrapped && allow_wrap) {
				cur_addr = 0;
				wrapped = true;
				if (did_wrap_out)
					*did_wrap_out = true;
				continue;
			}
			cur_addr = TASK_SIZE;
			break;
		}

		cur_addr = (wctx.next_vaddr < TASK_SIZE) ? wctx.next_vaddr :
							   TASK_SIZE;

		for (k = 0; k < wctx.batch_count; k++) {
			struct folio *folio = wctx.folios[k];

			if (!folio_test_lru(folio) ||
			    folio_test_unevictable(folio)) {
				folio_put(folio);
				continue;
			}
			if (!fp_folio_isolate_lru(folio)) {
				folio_put(folio);
				continue;
			}
			if (folio_test_unevictable(folio)) {
				fp_folio_putback_lru(folio);
				folio_put(folio);
				continue;
			}
			list_add(&folio->lru, &folio_list);
			isolated_vpns[isolated_cnt++] = wctx.vpns[k];
			folio_put(folio);
		}

		if (!list_empty(&folio_list)) {
			/*
			 * Pass `ignore_references = false` so `shrink_folio_list`
			 * runs `folio_check_references()` -> `mmu_notifier` on
			 * KVM Stage-2 page tables (`kvm->arch.mmu.pgt`), aging
			 * Stage-2 accessed bits and keeping active Firecracker
			 * guest working-set pages in DRAM!
			 */
			unsigned long rec =
				fp_reclaim_pages(&folio_list, false);

			reclaimed += (u32)rec;
			if (rec > 0 && !READ_ONCE(g_evict_kp_registered) &&
			    rec == isolated_cnt) {
				for (k = 0; k < isolated_cnt; k++)
					mext_record_eviction(
						pid, isolated_vpns[k], epoch);
			}
		}

		cond_resched();
		if (walk_ret == 0) {
			if (!wrapped && allow_wrap) {
				cur_addr = 0;
				wrapped = true;
				if (did_wrap_out)
					*did_wrap_out = true;
			} else {
				cur_addr = TASK_SIZE;
				break;
			}
		}
	}

	*cursor_inout = cur_addr;
	return reclaimed;
}

static int mext_ioctl_pageout_cold(struct mext_prefault_msg *msg,
				   void __user *argp)
{
	struct task_struct *task;
	struct mm_struct *mm;
	unsigned long cursor = 0;
	u64 last_wrap_ns = 0;
	u64 now_ns;
	u32 slot, target, reclaimed;
	u64 epoch;
	bool force_age, allow_wrap, did_wrap = false;

	if (msg->pid <= 0 || msg->nr_pages == 0)
		return -EINVAL;
	if (!fp_reclaim_pages || !fp_folio_isolate_lru ||
	    !fp_folio_putback_lru)
		return -ENOSYS;

	target = min_t(u32, msg->nr_pages, MEXT_MAX_PAGEOUT_PAGES);
	force_age = !!(msg->flags & MEXT_PREFAULT_FLAG_FORCE_AGE);
	now_ns = ktime_get_ns();

	slot = mext_hash_pid((u32)msg->pid) & (MEXT_PAGEOUT_CURSOR_SLOTS - 1U);
	spin_lock(&g_pageout_cursor_lock);
	if (g_pageout_cursors[slot].pid == msg->pid) {
		cursor = g_pageout_cursors[slot].next_vaddr;
		last_wrap_ns = g_pageout_cursors[slot].last_wrap_ns;
	}
	spin_unlock(&g_pageout_cursor_lock);

	/*
	 * Enforce an 8-second minimum KVM Stage-2 aging generation between full
	 * address-space sweeps (unless `MEXT_PREFAULT_FLAG_FORCE_AGE` host
	 * emergency is set). Pass 1 clears KVM Stage-2 `AF` via `mmu_notifier`;
	 * parking at `TASK_SIZE` for 8s lets active inter-command VM pages
	 * re-assert Stage-2 `AF=1` so Pass 2 evicts genuinely dead pages only.
	 */
	if (cursor >= TASK_SIZE) {
		if (!force_age && last_wrap_ns > 0 &&
		    now_ns > last_wrap_ns &&
		    (now_ns - last_wrap_ns) < MEXT_PAGEOUT_MIN_WRAP_NS) {
			msg->nr_succeeded = 0;
			if (copy_to_user(argp, msg, sizeof(*msg)))
				return -EFAULT;
			return 0;
		}
		cursor = 0;
		last_wrap_ns = now_ns;
	} else if (last_wrap_ns == 0) {
		last_wrap_ns = now_ns;
	}

	rcu_read_lock();
	task = pid_task(find_vpid(msg->pid), PIDTYPE_PID);
	if (task)
		get_task_struct(task);
	rcu_read_unlock();
	if (!task)
		return -ESRCH;

	mm = get_task_mm(task);
	put_task_struct(task);
	if (!mm)
		return -EINVAL;

	allow_wrap = force_age ||
		     (now_ns >= last_wrap_ns &&
		      (now_ns - last_wrap_ns) >= MEXT_PAGEOUT_MIN_WRAP_NS);
	epoch = mext_next_swapout_epoch();
	reclaimed = mext_pageout_sweep_mm(mm, msg->pid, target, true,
					  allow_wrap, epoch, &cursor,
					  &did_wrap);
	if (did_wrap)
		last_wrap_ns = now_ns;

	spin_lock(&g_pageout_cursor_lock);
	g_pageout_cursors[slot].pid = msg->pid;
	g_pageout_cursors[slot].next_vaddr = cursor;
	g_pageout_cursors[slot].last_wrap_ns = last_wrap_ns;
	spin_unlock(&g_pageout_cursor_lock);

	mmput(mm);

	msg->nr_succeeded = reclaimed;
	if (copy_to_user(argp, msg, sizeof(*msg)))
		return -EFAULT;
	return 0;
}

static int mext_check_swap_pte_entry(pte_t *ptep, unsigned long addr,
				     unsigned long next, struct mm_walk *walk)
{
	bool *is_swap = walk->private;
	pte_t pte = ptep_get(ptep);

	if (!pte_present(pte) && is_swap_pte(pte))
		*is_swap = true;
	return 1;
}

static const struct mm_walk_ops mext_check_swap_walk_ops = {
	.pte_entry = mext_check_swap_pte_entry,
};

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

	if (msg.flags & MEXT_PREFAULT_FLAG_PAGEOUT_COLD)
		return mext_ioctl_pageout_cold(&msg, argp);

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
	if (msg.flags & MEXT_PREFAULT_FLAG_NOWAIT)
		gup_flags |= FOLL_NOWAIT;

	for (i = 0; i < msg.nr_pages; i++) {
		unsigned long vaddr = (unsigned long)(vpns[i] << PAGE_SHIFT);
		struct page *page = NULL;
		int locked = 1;
		long pinned;

		if (vaddr >= TASK_SIZE)
			continue;

		/*
		 * Use non-blocking mmap_read_trylock so speculative prefaulting
		 * never contends with mmap()/munmap() writers during VM/sandbox
		 * creation or dynamic heap growth.
		 */
		if (!mmap_read_trylock(mm))
			break;
		/*
		 * Verify target VA currently holds a non-present swap PTE before
		 * calling get_user_pages_remote: prevents allocating zero pages
		 * into untouched `pte_none` holes inside sparse 4 GiB Firecracker
		 * GPA VMAs while cleanly swapping in `/dev/ublkb2` CoW swap PTEs!
		 */
		{
			bool has_swap_pte = false;

			(void)walk_page_range(mm, vaddr, vaddr + PAGE_SIZE,
					      &mext_check_swap_walk_ops,
					      &has_swap_pte);
			if (!has_swap_pte) {
				mmap_read_unlock(mm);
				continue;
			}
		}
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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	if (!g_evict_kp_registered && !g_evict_pvmw_registered) {
		ret = register_kprobe(&g_evict_kp);
		if (ret == 0) {
			g_evict_kp_registered = true;
			pr_info("MEXT cgroup eviction hook active via try_to_unmap_one\n");
		} else {
			ret = register_kprobe(&g_evict_pvmw_kp);
			if (ret == 0) {
				g_evict_pvmw_registered = true;
				g_evict_kp_registered = true;
				pr_info("MEXT cgroup eviction hook active via page_vma_mapped_walk fallback\n");
			} else {
				pr_warn("MEXT eviction kprobes unavailable (ttu=%d, pvmw=%d)\n",
					-ENOENT, ret);
			}
		}
	}
#endif
	if (!g_pressure_kp_registered) {
		ret = register_kprobe(&g_pressure_kp);
		if (ret == 0) {
			g_pressure_kp_registered = true;
		} else {
			pr_warn("register_kprobe(wakeup_kswapd) unavailable (%d); using user-space telemetry\n",
				ret);
		}
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
		if (msg.pid > 0) {
			u32 cslot = mext_hash_pid((u32)msg.pid) &
				    (MEXT_PAGEOUT_CURSOR_SLOTS - 1U);
			spin_lock(&g_pageout_cursor_lock);
			if (g_pageout_cursors[cslot].pid == msg.pid) {
				g_pageout_cursors[cslot].pid = 0;
				g_pageout_cursors[cslot].next_vaddr = 0;
				g_pageout_cursors[cslot].last_wrap_ns = 0;
			}
			spin_unlock(&g_pageout_cursor_lock);
		}
		break;
	case MEXT_CTRL_CLEAR_PIDS:
		spin_lock(&g_ctrl_lock);
		mext_pid_filter_clear();
		spin_unlock(&g_ctrl_lock);
		spin_lock(&g_pageout_cursor_lock);
		memset(g_pageout_cursors, 0, sizeof(g_pageout_cursors));
		spin_unlock(&g_pageout_cursor_lock);
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
	int i;

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
	g_mext_ring->hdr.pressure_events = 0;

	for (i = 0; i < MEXT_PRESSURE_NODE_SLOTS; i++)
		atomic64_set(&g_last_pressure_ns[i], 0);

	mext_pid_filter_clear();
	pr_info("MEXT fault/eviction/pressure ring initialized (%u slots, %zu bytes)\n",
		MEXT_RING_CAPACITY, g_mext_ring_alloc_bytes);
	return 0;
}

void mext_fault_ring_exit(void)
{
	WRITE_ONCE(g_capture_enabled, false);
	if (g_pressure_kp_registered) {
		unregister_kprobe(&g_pressure_kp);
		g_pressure_kp_registered = false;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	if (g_evict_pvmw_registered) {
		unregister_kprobe(&g_evict_pvmw_kp);
		g_evict_pvmw_registered = false;
		g_evict_kp_registered = false;
	} else if (g_evict_kp_registered) {
		unregister_kprobe(&g_evict_kp);
		g_evict_kp_registered = false;
	}
#endif
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
