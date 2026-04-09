// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: SMAP access mmu module
 */

#include <linux/hugetlb.h>
#include <linux/pagewalk.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/sched/mm.h>
#include <linux/mm_types.h>
#include <linux/page-isolation.h>

#include "check.h"
#include "access_tracking.h"
#include "access_tracking_wrapper.h"
#include "access_mmu.h"

#define PAGEMAP_WALK_SIZE (PMD_SIZE)
#define PAGEMAP_WALK_MASK (PMD_MASK)

#define PM_ENTRY_BYTES sizeof(pagemap_entry_t)
#define PM_PFRAME_BITS 55
#define PM_PFRAME_MASK GENMASK_ULL(PM_PFRAME_BITS - 1, 0)
#define PM_SOFT_DIRTY BIT_ULL(55)
#define PM_MMAP_EXCLUSIVE BIT_ULL(56)
#define PM_UFFD_WP BIT_ULL(57)
#define PM_FILE BIT_ULL(61)
#define PM_SWAP BIT_ULL(62)
#define PM_PRESENT BIT_ULL(63)

#define PM_END_OF_BUFFER 1

#define FILE_NAME_MAX 32
#define RAM_SLOT_ID 2
#ifdef CONFIG_MEM_SOFT_DIRTY
#define VM_SOFTDIRTY 0x08000000 /* Not soft dirty clean area */
#else
#define VM_SOFTDIRTY 0
#endif

#define PM_LEN 16384
#define PROC_READ_PM_LEN 128

#define MAPPING_U32_BITS (32)

#define MAPPING_PRIOR_BIT 8
#define MAPPING_PAIDX_BIT 20
#define MAPPING_NODE_BIT 4

#define MAPPING_PRIOR_SHIFT 0
#define MAPPING_PAIDX_SHIFT (MAPPING_PRIOR_SHIFT + MAPPING_PRIOR_BIT)
#define MAPPING_NODES_SHIFT (MAPPING_PAIDX_SHIFT + MAPPING_PAIDX_BIT)

#define MAPPING_NODES_MASK \
	(((1UL << MAPPING_NODE_BIT) - 1) << MAPPING_NODES_SHIFT)

static inline pagemap_entry_t make_pme(u64 frame, u64 flags)
{
	return (pagemap_entry_t){ .pme = (frame & PM_PFRAME_MASK) | flags };
}

static int calc_paddr_acidx(u64 paddr, int *nid, u64 *index)
{
	int page_size = is_access_hugepage() ? g_pagesize_huge : PAGE_SIZE;
	if (is_paddr_local(paddr))
		return calc_paddr_acidx_acpi(paddr, nid, index, page_size);
	return calc_paddr_acidx_iomem(paddr, nid, index, page_size);
}

static int set_non_anon_bm(struct access_pid *ap, u64 acidx, u64 paddr, int nid)
{
	unsigned long pfn;
	struct page *p_page = NULL;

	if (nid >= nr_local_numa) {
		return 0;
	}
	pfn = PHYS_PFN(paddr);
	if (!pfn_valid(pfn)) {
		return -EINVAL;
	}
	p_page = pfn_to_online_page(pfn);
	if (!p_page) {
		return -EINVAL;
	}
	if (!PageHuge(p_page)) {
		if (!PageAnon(p_page) || page_mapcount(p_page) > 1) {
			set_bit(acidx, ap->white_list_bm[nid]);
		}
	}
	return 0;
}

static int add_to_bm_page(u64 paddr, struct access_pid *ap)
{
	int nid, nid_pos, ret;
	u64 acidx;
	ret = calc_paddr_acidx(paddr, &nid, &acidx);
	if (ret)
		return ret;
	nid_pos = convert_nid_to_pos(nid);
	unsigned long numa_nodes = ap->numa_nodes;
	if (unlikely(!test_bit(nid_pos, &numa_nodes))) {
		return -EINVAL;
	}

	if (BIT_WORD(acidx) >= ap->bm_len[nid]) {
		return -ERANGE;
	}

	ret = set_non_anon_bm(ap, acidx, paddr, nid);
	if (ret) {
		return ret;
	}
	set_bit(acidx, ap->paddr_bm[nid]);
	ap->page_num[nid]++;
	return 0;
}

static int calc_vaddr_acidx(u64 vaddr, struct vm_mapping_info *info, u64 *acidx)
{
	int i;
	*acidx = 0;
	int shift = __builtin_ctz(g_pagesize_huge);
	for (i = 0; i < info->nr_segs; i++) {
		bool flag = vaddr < info->segs[i].start ||
			    vaddr >= info->segs[i].end;
		if (flag) {
			*acidx += info->segs[i].hugepages;
			continue;
		}
		*acidx += (vaddr - info->segs[i].start) >> shift;
		return 0;
	}
	return -ERANGE;
}

static inline void set_mapping_numa(u32 *map, int nid)
{
	*map &= ~MAPPING_NODES_MASK;
	*map |= ((u32)nid << MAPPING_NODES_SHIFT);
}

static void set_pa_prior(struct access_pid *ap, u64 vaddr, u64 pa_idx, int nid)
{
	u8 prior;
	int prior_bits;
	u32 map = 0;
	u64 va_idx;
	int shift = __builtin_ctz(g_pagesize_huge);
	int ret = calc_vaddr_acidx(vaddr, &ap->info, &va_idx);
	if (ret != 0 || va_idx >= ap->info.vm_size) {
		pr_debug("set pa prior out of range\n");
		return;
	}
	if (va_idx & (1 << (shift - 1))) {
		pr_debug("va_idx is not aligned\n");
		return;
	}
	set_mapping_numa(&map, nid);
	map |= pa_idx << MAPPING_PRIOR_BIT;

	prior_bits = MAPPING_U32_BITS - __builtin_clz(ap->info.vm_size);
	if (prior_bits > MAPPING_PRIOR_BIT) {
		prior = va_idx >> (prior_bits - MAPPING_PRIOR_BIT);
	} else {
		prior = va_idx;
	}
	map |= prior;
	ap->info.mapping[va_idx] = map;
}

static int add_to_bm_hugepage(u64 vaddr, u64 paddr, struct access_pid *ap)
{
	int nid, nid_pos, ret;
	u64 acidx;

	ret = calc_paddr_acidx(paddr, &nid, &acidx);
	if (ret)
		return ret;
	nid_pos = convert_nid_to_pos(nid);
	unsigned long numa_nodes = ap->numa_nodes;
	if (unlikely(!test_bit(nid_pos, &numa_nodes))) {
		return -EINVAL;
	}

	if (BIT_WORD(acidx) >= ap->bm_len[nid]) {
		return -ERANGE;
	}
	set_bit(acidx, ap->paddr_bm[nid]);
	set_pa_prior(ap, vaddr, acidx, nid);
	ap->page_num[nid]++;
	return 0;
}

static inline void add_to_bm_huge(u64 vaddr, u64 paddr, struct access_pid *ap)
{
	u64 mask_paddr = paddr & TWO_MEGA_MASK;
	add_to_bm_hugepage(vaddr, mask_paddr, ap);
}

static void add_to_bm_normal(u64 paddr, struct access_pid *ap)
{
	add_to_bm_page(paddr, ap);
}

static bool is_paddr_belong_remote_node(u64 pa, int nid)
{
	struct ram_segment *seg;
	int numa = NUMA_NO_NODE;

	read_lock(&rem_ram_list_lock);
	list_for_each_entry(seg, &remote_ram_list, node) {
		if (pa < seg->start)
			break;
		if (pa <= seg->end) {
			numa = seg->numa_node;
			break;
		}
	}

	if (unlikely(list_entry_is_head(seg, &remote_ram_list, node))) {
		read_unlock(&rem_ram_list_lock);
		return false;
	}
	read_unlock(&rem_ram_list_lock);
	return numa == nid;
}

static bool is_page_ready_for_migrate(struct page *page)
{
	if (is_migrate_isolate_page(page)) {
		return false;
	}
	if (is_access_hugepage()) {
		if (PageHuge(page) && !PageHead(page))
			return false;
	} else {
		if (__folio_test_movable(page_folio(page)) ||
		    page_ref_count(page) == 0 || PageTransHuge(page) ||
		    PageHuge(page)) {
			return false;
		}
	}
	if (page_mapcount(page) > 1) {
		pr_debug("page mapcount invalid.\n");
		return false;
	}
	if (!PageHuge(page)) {
		if (!folio_try_get(page_folio(page))) {
			return false;
		}
	}
	return true;
}

static int add_to_bm(unsigned long vaddr, pagemap_entry_t *pme,
		     struct pagemapread *pm)
{
	u64 paddr;
	u64 pfn;
	struct page *page;
	pfn = pme->pme & PM_PFRAME_MASK;
	if (!pfn || !(pme->pme & PM_PRESENT))
		goto inc_pm_pos;
	paddr = PFN_PHYS(pfn);

	if (pm->mig_type == REMOTE_MIGRATE) {
		page = pfn_to_online_page(pfn);
		if (!page) {
			goto inc_pm_pos;
		}
		pm->mig_info.page_cnt++;
		if (!is_paddr_belong_remote_node(paddr,
						 pm->mig_info.remote_nid)) {
			goto inc_pm_pos;
		}

		if (!is_page_ready_for_migrate(page)) {
			goto inc_pm_pos;
		}

		if (pm->mig_info.mig_cnt < pm->mig_info.folios_len) {
			pm->mig_info.folios[pm->mig_info.mig_cnt++] =
				page_folio(page);
		}
	} else {
		if (is_access_hugepage())
			add_to_bm_huge(vaddr, paddr, pm->ap);
		else
			add_to_bm_normal(paddr, pm->ap);
	}

inc_pm_pos:
	if (++pm->pos >= pm->len)
		return PM_END_OF_BUFFER;
	return 0;
}

static pagemap_entry_t pte_to_pagemap_entry(struct pagemapread *pm,
					    struct vm_area_struct *vma,
					    unsigned long addr, pte_t pte)
{
	u64 frame = 0, flags = 0;
	swp_entry_t entry;
	pgoff_t offset;

	if (pte_present(pte)) {
		frame = pte_pfn(pte);
		flags |= PM_PRESENT;
	} else if (is_swap_pte(pte)) {
		entry = pte_to_swp_entry(pte);
		offset = swp_offset(entry);
		frame = swp_type(entry) | (offset << MAX_SWAPFILES_SHIFT);
	}

	return make_pme(frame, flags);
}

void pmd_clear_bad(pmd_t *pmd)
{
	pmd_ERROR(*pmd);
	pmd_clear(pmd);
}

spinlock_t *__pmd_trans_huge_lock(pmd_t *pmd, struct vm_area_struct *vma)
{
	spinlock_t *ptl;
	ptl = pmd_lock(vma->vm_mm, pmd);
	if (likely(is_swap_pmd(*pmd) || pmd_trans_huge(*pmd) ||
		   pmd_devmap(*pmd)))
		return ptl;
	spin_unlock(ptl);
	return NULL;
}

static int pagemap_pte_range(pte_t *pte, unsigned long addr, unsigned long next,
			     struct mm_walk *walk)
{
	struct vm_area_struct *vma = walk->vma;
	struct pagemapread *pm = walk->private;
	pagemap_entry_t pme;

	pme = pte_to_pagemap_entry(pm, vma, addr, ptep_get(pte));
	return add_to_bm(addr, &pme, pm);
}

static int pagemap_hugetlb_range(pte_t *ptep, unsigned long hmask,
				 unsigned long addr, unsigned long end,
				 struct mm_walk *walk)
{
	struct pagemapread *pm = walk->private;
	struct vm_area_struct *vma = walk->vma;
	pagemap_entry_t pme;
	u64 flags = 0, frame = 0;
	int err = 0;
	pte_t pte;

	if (vma->vm_flags & VM_SOFTDIRTY)
		flags |= PM_SOFT_DIRTY;

	pte = smap_huge_ptep_get(ptep);
	if (pte_present(pte)) {
		flags |= PM_PRESENT;
		frame = pte_pfn(pte) + ((addr & ~hmask) >> PAGE_SHIFT);
		pme = make_pme(frame, flags);
		err = add_to_bm(addr, &pme, pm);
	}
	return err;
}

static const struct mm_walk_ops pagemap_mm_ops = {
	.pte_entry = pagemap_pte_range,
};

static const struct mm_walk_ops pagemap_vm_ops = {
	.hugetlb_entry = pagemap_hugetlb_range,
};

static void pos_to_addr(struct mm_struct *mm, unsigned long pos,
			unsigned long *vaddr)
{
	unsigned long count, nr_pages;
	u64 last_vaddr;
	struct vm_area_struct *vma;
	struct vma_iterator vmi;
	MA_STATE(mas, &mm->mm_mt, 0, ULONG_MAX);
	vmi.mas = mas;
	count = 0;
	last_vaddr = 0;

	mmap_assert_locked(mm);

	for (vma = vma_find(&vmi, ULONG_MAX); vma; vma = vma_next(&vmi)) {
		if (is_access_hugepage() && !is_vm_hugetlb_page(vma)) {
			continue;
		}
		last_vaddr = vma->vm_end;
		if (count == pos) {
			*vaddr = vma->vm_start;
			return;
		}
		nr_pages = (vma->vm_end - vma->vm_start) >> PAGE_SHIFT;
		if (count + nr_pages > pos) {
			*vaddr = vma->vm_start + (pos - count) * PAGE_SIZE;
			return;
		}
		count += nr_pages;
	}
	*vaddr = last_vaddr;
}

struct mm_struct *get_mm_by_pid(pid_t pid)
{
	struct task_struct *task = get_pid_task(find_vpid(pid), PIDTYPE_PID);
	struct mm_struct *mm = ERR_PTR(-ESRCH);
	int err;

	if (task) {
		err = down_read_killable(&task->signal->exec_update_lock);
		if (err) {
			put_task_struct(task);
			return ERR_PTR(err);
		}

		mm = get_task_mm(task);
		up_read(&task->signal->exec_update_lock);
		put_task_struct(task);

		if (!IS_ERR_OR_NULL(mm)) {
			/* ensure this mm_struct can't be freed */
			mmgrab(mm);
			/* but do not pin its memory */
			mmput(mm);
		}
	}

	return mm;
}

void walk_pid_pagemap(struct pagemapread *pm)
{
	struct mm_struct *mm;
	unsigned long svpfn = 0;
	unsigned long evpfn;
	unsigned long start_vaddr, end_vaddr;
	ktime_t start_time, pagemap_time;
	start_time = ktime_get();
	if (!pm) {
		return;
	}
	/* 获取mm_struct */
	mm = get_mm_by_pid(pm->mig_info.pid);
	if (IS_ERR(mm))
		return;
	if (!mm || !mmget_not_zero(mm))
		return;
	pm->len = PM_LEN;

	while (1) {
		pm->pos = 0;
		/* walk page */
		if (mmap_read_lock_killable(mm)) {
			mmput(mm);
			return;
		}
		/* watch out for wraparound */
		evpfn = svpfn + (unsigned long)pm->len;
		if (svpfn > ULONG_MAX - (unsigned long)pm->len)
			evpfn = ULONG_MAX;

		pos_to_addr(mm, svpfn, &start_vaddr);
		pos_to_addr(mm, evpfn, &end_vaddr);
		/* break if reach the last vma */
		if (start_vaddr == end_vaddr) {
			mmap_read_unlock(mm);
			break;
		}

		if (is_access_hugepage()) {
			walk_page_range(mm, start_vaddr, end_vaddr,
					&pagemap_vm_ops, pm);
		} else {
			walk_page_range(mm, start_vaddr, end_vaddr,
					&pagemap_mm_ops, pm);
		}
		mmap_read_unlock(mm);
		svpfn = evpfn;
	}
	pagemap_time = calc_time_us(start_time);
	pr_debug("walk pagemap of pid: %d took %lldus\n", pm->mig_info.pid,
		 pagemap_time);
	mmput(mm);
}
EXPORT_SYMBOL(walk_pid_pagemap);