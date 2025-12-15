// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "coherence_maintain.h"
#include <linux/hugetlb.h>
#include <linux/iopoll.h>
#include <linux/pagewalk.h>
#include <linux/sched.h>
#include <linux/semaphore.h>

#include "basic.h"

#undef pr_fmt
#define pr_fmt(fmt) "HAM: " fmt
#define MAX_FLUSH_SIZE (1UL << 29)
#define MAX_RESCHED_ROUND 1
#define MAX_CM_RETRY 20

struct pfn_range {
	unsigned long start_pfn;
	unsigned long end_pfn;
};

struct modify_info {
	int pmd_cnt;
	int pte_cnt;
	int pmd_leaf_cnt;
	unsigned int hugetlb_cnt;
	struct pfn_range *hugetlb_ranges;
	bool cacheable;
	int ret;
};

/* Flush the entire process address space */
void ham_flush_tlb(struct mm_struct *mm)
{
	unsigned long asid;

	dsb(ishst);
	asid = __TLBI_VADDR(0, ASID(mm));
	__tlbi(aside1is, asid);
	__tlbi_user(aside1is, asid);
	dsb(ish);
}

static int modify_hugetlb_prot(pte_t *pte, unsigned long hmask __always_unused,
			       unsigned long addr,
			       unsigned long next __always_unused,
			       struct mm_walk *walk)
{
	struct modify_info *info = (struct modify_info *)walk->private;
	bool cacheable = info->cacheable;
	struct vm_area_struct *vma = walk->vma;
	spinlock_t *ptl;
	pgprot_t prot;
	pte_t entry;

	ptl = huge_pte_lock(hstate_vma(vma), walk->mm, pte);
	entry = ptep_get(pte);
	if (unlikely(!pte_present(entry))) {
		spin_unlock(ptl);
		return 0;
	}

	info->hugetlb_cnt++;

	prot = cacheable ? pgprot_tagged(pte_pgprot(entry)) :
			   pgprot_writecombine(pte_pgprot(entry));
	entry = pte_modify(entry, prot);
	__set_pte(pte, entry);

	spin_unlock(ptl);
	return 0;
}

int task_pgtable_within_mm_set_cacheable(struct mm_struct *mm,
					 unsigned long start,
					 unsigned long size, bool cacheable)
{
	int ret;
	struct modify_info info = { 0 };
	struct mm_walk_ops walk_ops = {
		.hugetlb_entry = modify_hugetlb_prot,
	};

	info.cacheable = cacheable;
	unsigned long end = start + size;

	mmap_write_lock(mm);
	ret = walk_page_range(mm, start, end, &walk_ops, &info);
	if (ret) {
		pr_err("failed to walk a memory map's page table\n");
	}
	mmap_write_unlock(mm);
	ham_flush_tlb(mm);

	pr_info("\tpmd: %d\n", info.pmd_cnt);
	pr_info("\tpmd leaf: %d\n", info.pmd_leaf_cnt);
	pr_info("\tpte: %d\n", info.pte_cnt);
	pr_info("\thugetlb: %u\n", info.hugetlb_cnt);
	return ret;
}

/**
 * task_pgtable_within_pid_set_cacheable - maintain task pgtable
 * @pid:	pid_t representing the target process
 * @start:	start address of the virtual address range
 * @size:	size of the virtual address range
 * @cacheable:	true for cacheable, false for non-cacheable
 *
 * Recursively walk the page table tree of the process represented by @pid
 * within the virtual address range [@start, @start + @size). And set the
 * corresponding task page table attribute to @cacheable.
 */
int task_pgtable_within_pid_set_cacheable(pid_t pid, unsigned long start,
					  unsigned long size, bool cacheable)
{
	struct task_struct *task;
	struct mm_struct *mm;
	int ret = -EINVAL;

	pr_info("start modify task pgtable, pid: %d, size: %#lx\n", pid, size);
	/* Get process information */
	task = find_get_task_by_vpid(pid);
	if (!task) {
		pr_err("failed to get task_struct of pid: %d\n", pid);
		return ret;
	}
	mm = get_task_mm(task);
	if (!mm) {
		pr_err("failed to get mm_struct of pid: %d\n", pid);
		goto exit_with_taskput;
	}
	/* Maintain task pgtable */
	ret = task_pgtable_within_mm_set_cacheable(mm, start, size, cacheable);
	if (ret) {
		pr_err("failed to modify task pgtable, pid: %d, size: %#lx\n",
		       pid, size);
	}
	mmput(mm);

exit_with_taskput:
	put_task_struct(task);
	return ret;
}

static int hugetlb_entry_collect_pfn(pte_t *pte,
				     unsigned long hmask __always_unused,
				     unsigned long addr,
				     unsigned long next __always_unused,
				     struct mm_walk *walk)
{
	struct modify_info *info = (struct modify_info *)walk->private;
	pte_t entry;
	unsigned long pgsize;
	unsigned long start_pfn, end_pfn;

	entry = kernel_huge_ptep_get(pte);
	if (unlikely(!pte_present(entry))) {
		return 0;
	}

	start_pfn = pte_pfn(entry);
	pgsize = huge_page_size(hstate_vma(walk->vma));
	end_pfn = start_pfn + (pgsize >> PAGE_SHIFT);

	info->hugetlb_ranges[info->hugetlb_cnt].start_pfn = start_pfn;
	info->hugetlb_ranges[info->hugetlb_cnt].end_pfn = end_pfn;
	info->hugetlb_cnt++;
	return 0;
}

/**
 * kernel_pgtable_within_mm_set_valid - maintain kernel pgtable
 * @mm:		mm_struct representing the target process
 * @start:	start address of the virtual address range
 * @size:	size of the virtual address range
 * @valid:	true for valid, false for invalid
 *
 * Recursively walk the page table tree of the process represented by @mm
 * within the virtual address range [@start, @start + @size). And set the
 * corresponding kernel page table attribute to @valid.
 */
int kernel_pgtable_within_mm_set_valid(struct mm_struct *mm,
				       unsigned long start, unsigned long size,
				       bool valid)
{
	struct mm_walk_ops walk_ops = {
		.hugetlb_entry = hugetlb_entry_collect_pfn,
	};

	int ret;
	unsigned long start_pfn, end_pfn;
	unsigned long end = start + size;
	size_t i, max_hugetlb_cnt = DIV_ROUND_UP(size, PMD_SIZE);
	struct modify_info info = { 0 };

	info.hugetlb_ranges = kmalloc_array(
		max_hugetlb_cnt, sizeof(struct pfn_range), GFP_KERNEL);
	if (!info.hugetlb_ranges) {
		pr_err("unable to allocate memory for hugetlb array\n");
		return -ENOMEM;
	}

	/* Collect pfn range of each hugetlb_entry */
	mmap_read_lock(mm);
	ret = walk_page_range(mm, start, end, &walk_ops, &info);
	mmap_read_unlock(mm);
	if (ret) {
		goto out;
	}

	/* Modify kernel pgtable attr */
	for (i = 0; i < info.hugetlb_cnt && i < max_hugetlb_cnt; i++) {
		start_pfn = info.hugetlb_ranges[i].start_pfn;
		end_pfn = info.hugetlb_ranges[i].end_pfn;
		/*
		 * Currently, kernel_pgtable_XX_set_valid is only executed at the
		 * destination end which modify pgtable attr from invalid to valid.
		 * Even if it fails here, it will be modified later when obmm unexport.
		 */
		ret = set_linear_mapping_invalid(start_pfn, end_pfn, !valid);
		if (ret) {
			pr_warn("set_linear_mapping_invalid failed[%zu/%d], ret: %d\n",
				i, info.hugetlb_cnt, ret);
		}
		cond_resched();
	}
out:
	kfree(info.hugetlb_ranges);
	return ret;
}

/**
 * kernel_pgtable_within_pid_set_valid - maintain kernel pgtable
 * @pid:	pid_t representing the target process
 * @start:	start address of the virtual address range
 * @size:	size of the virtual address range
 * @valid:	true for valid, false for invalid
 *
 * Recursively walk the page table tree of the process represented by @pid
 * within the virtual address range [@start, @start + @size). And set the
 * corresponding kernel page table attribute to @valid.
 */
int kernel_pgtable_within_pid_set_valid(pid_t pid, unsigned long start,
					unsigned long size, bool valid)
{
	struct task_struct *task;
	struct mm_struct *mm;
	int ret = -EINVAL;

	pr_info("start modify kernel pgtable, pid: %d\n", pid);
	/* Get process information */
	task = find_get_task_by_vpid(pid);
	if (!task) {
		pr_err("failed to get task_struct of pid: %d\n", pid);
		return ret;
	}
	mm = get_task_mm(task);
	if (!mm) {
		pr_err("Failed to get mm_struct of pid: %d\n", pid);
		goto exit_with_taskput;
	}
	/* Maintain kernel pgtable */
	ret = kernel_pgtable_within_mm_set_valid(mm, start, size, valid);
	if (ret) {
		pr_err("Failed to modify kernel pgtable, pid: %d\n", pid);
	}
	mmput(mm);

exit_with_taskput:
	put_task_struct(task);
	return ret;
}

int flush_cache_by_pa(phys_addr_t addr, size_t size,
		      enum hisi_soc_cache_maint_type maint_type)
{
	static DEFINE_SEMAPHORE(ham_sem, 1);
	phys_addr_t curr_addr = addr;
	size_t remain_size = size;
	int ret = 0, round_to_resched = MAX_RESCHED_ROUND;
	int retry_times = MAX_CM_RETRY;

	down(&ham_sem);
	while (remain_size != 0) {
		size_t flush_size;
		flush_size = remain_size <= MAX_FLUSH_SIZE ? remain_size :
							     MAX_FLUSH_SIZE;
		/* Retry if there is contention over hardware */
		while (retry_times) {
			pr_debug(
				"call external: hisi_soc_cache_maintain(%#zx, %u)\n",
				flush_size, maint_type);
			ret = hisi_soc_cache_maintain(curr_addr, flush_size,
						      maint_type);
			pr_debug(
				"external called: hisi_soc_cache_maintain(), retval=%d\n",
				ret);

			if (ret != -EBUSY) {
				break;
			}
			pr_info_once(
				"racing access of cache flushing hardware "
				"identified, the performance of UB memory may "
				"signficantly degrade.\n");
			retry_times--;
			schedule();
		}
		if (ret) {
			break;
		}

		curr_addr += flush_size;
		remain_size -= flush_size;
		if (--round_to_resched == 0) {
			schedule();
			round_to_resched = MAX_RESCHED_ROUND;
		}
	}
	up(&ham_sem);

	if (remain_size != 0)
		pr_warn("%s: %#zx not flushed due to unexpected error; retval=%d.\n",
			__func__, remain_size, ret);

	return ret;
}

/*
 * The flush_tlb_XX(), when expanded, includes a function that is not
 * symbolically exported, so stub a dummy implementation here.
*/
void __mmu_notifier_arch_invalidate_secondary_tlbs(struct mm_struct *mm,
							  unsigned long start,
							  unsigned long end)
{
}

static int modify_single_hugetlb_prot(pte_t *pte,
				     unsigned long hmask __always_unused,
				     unsigned long addr,
				     unsigned long next __always_unused,
				     struct mm_walk *walk)
{
	struct vm_area_struct *vma = walk->vma;
	spinlock_t *ptl;
	pte_t entry;
	unsigned long pgsize;
	unsigned long start_pfn;
	pgprot_t prot;
	int ret;

	ptl = huge_pte_lock(hstate_vma(vma), walk->mm, pte);
	entry = kernel_huge_ptep_get(pte);
	if (unlikely(!pte_present(entry))) {
		spin_unlock(ptl);
		return 0;
	}

	start_pfn = pte_pfn(entry);
	pgsize = huge_page_size(hstate_vma(vma));

	pte_val(entry) &= ~PTE_VALID;
	__set_pte(pte, entry);
	flush_tlb_range(vma, addr, addr + pgsize);

	ret = set_linear_mapping_nc(start_pfn, pgsize, false);
	if (ret) {
		pr_warn("set kernel pgtable NC to CC failed\n");
	}

	prot = pgprot_tagged(pte_pgprot(entry));
	entry = pte_modify(entry, prot);
	pte_val(entry) |= PTE_VALID;
	__set_pte(pte, entry);
	spin_unlock(ptl);
	return 0;
}

/**
 * set_pid_pgtable_cacheable - maintain task pgtable and kernel pgtable
 * @pid:	pid_t representing the target process
 * @start:	start address of the virtual address range
 * @size:	size of the virtual address range
 *
 * Recursively walk the page table tree of the process represented by @pid
 * within the virtual address range [@start, @start + @size).
 * 1. set the corresponding task page table attribute to invalid.
 * 2. set the corresponding kernel page table attribute to cacheable.
 * 3. set the corresponding task page table attribute to cacheable.
 * 4. set the corresponding task page table attribute to valid.
 */
int set_pid_pgtable_cacheable(pid_t pid, unsigned long start,
					unsigned long size)
{
	struct mm_struct *mm;
	int ret = -EINVAL;
	struct modify_info info = { 0 };
	struct mm_walk_ops walk_ops = {
		.hugetlb_entry = modify_single_hugetlb_prot,
	};
	unsigned long end = start + size;

	pr_info("Start modify task and kernel pgtable to cacheable and valid, "
			"pid: %d, size: %#lx\n", pid, size);
	/* Get process information */
	mm = find_get_mm_by_vpid(pid);
	if (!mm) {
		pr_err("Failed to get mm_struct of pid: %d\n", pid);
		return ret;
	}
	mmap_write_lock(mm);
	ret = walk_page_range(mm, start, end, &walk_ops, &info);
	mmap_write_unlock(mm);
	if (ret) {
		pr_err("failed to walk a memory map's page table, pid: %d, ret: %d\n",
		       pid, ret);
	}
	mmput(mm);
	return ret;
}