// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * Description: SMAP Tiering Memory Solution: get accessed bit模块
 */

#include <asm/pgtable.h>
#include <asm/kvm_mmu.h>
#include <asm/kvm_pgtable.h>
#include <asm/stage2_pgtable.h>

#include <linux/rmap.h>
#include <linux/vmalloc.h>
#include <linux/sort.h>
#include <linux/kvm_host.h>
#include <linux/hugetlb.h>
#include <linux/mmzone.h>
#include <linux/fdtable.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/pid_namespace.h>
#include <linux/pid.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/sched/mm.h>
#include <linux/sched/signal.h>
#include <linux/rcupdate.h>
#include <linux/bits.h>
#include <linux/stat.h>
#include <linux/pagewalk.h>
#include <linux/swapops.h>

#include "access_tracking_wrapper.h"
#include "access_pid.h"
#include "access_mmu.h"
#include "access_tracking.h"
#include "accessed_bit.h"

#define DECIMAL 10
#define DEFAULT_REF_COUNT 0
#define TRUE_REF 1
#define MAX_NR_KVM 100
#undef pr_fmt
#define pr_fmt(fmt) "access-bit: " fmt
#define MMAPLOCK_BATCH_SIZE (64UL * 1024 * 1024)

LIST_HEAD(ham_pid_list);
LIST_HEAD(statistic_pid_list);
spinlock_t ham_lock;
spinlock_t statistic_lock;
static u64 freq_info_num;

struct smap_vma_struct {
	unsigned long start_vaddr;
	unsigned long end_vaddr;
};

void free_mem(struct acpi_mem_segment *mem)
{
	if (!mem) {
		return;
	}
	kfree(mem);
}

/* Must be called with an elevated refcount on the filp */
static inline bool is_kvm_file(struct file *filp)
{
	if (strcmp(filp->f_inode->i_sb->s_type->name, "anon_inodefs") != 0)
		return false;
	return strcmp(filp->f_path.dentry->d_name.name, "kvm-vm") == 0;
}

/*
 * get_kvm_file_from_task - return struct file if task is a kvm process,
 * return NULL otherwise
 *
 * Context: fput must be called later if return value is non-NULL
 */
struct file *get_kvm_file_from_task(struct task_struct *task)
{
	struct file *filp = NULL;
	unsigned int i;

	if (!task || !task->files)
		return NULL;

	rcu_read_lock();
	task_lock(task);
	for (i = 0; i < files_fdtable(task->files)->max_fds; i++) {
		filp = files_lookup_fd_rcu(task->files, i);
		if (!filp) {
			continue;
		}
		if (!get_file_rcu(filp)) {
			continue;
		}

		if (!is_kvm_file(filp)) {
			fput(filp);
			continue;
		}
		task_unlock(task);
		rcu_read_unlock();
		return filp;
	}
	task_unlock(task);
	rcu_read_unlock();
	return NULL;
}

static int get_pid_from_tracking_file(const struct file *file)
{
	size_t len;
	int pid;
	char *number_str = NULL;
	const char *path = file->f_path.dentry->d_parent->d_name.name;
	if (!path) {
		return -EINVAL;
	}
	len = strlen(path);
	number_str = kmalloc(len, GFP_KERNEL);
	if (number_str == NULL) {
		pr_err("unable to allocate memory for tracking file path\n");
		return -ENOMEM;
	}
	memset(number_str, '\0', len);
	/* File name end with '_t', we don't need the last 2 characters */
	strncpy(number_str, path, len - 2);
	if (kstrtoint(number_str, DECIMAL, &pid) == 0) {
		kfree(number_str);
		return pid;
	}
	kfree(number_str);
	return -1;
}

static inline void clear_tracking_info(struct ham_tracking_info *info)
{
	memset(info->paddr[L1], 0, sizeof(u64) * info->len[L1]);
	memset(info->freq[L1], 0, sizeof(u16) * info->len[L1]);
	if (info->paddr[L2]) {
		memset(info->paddr[L2], 0, sizeof(u64) * info->len[L2]);
		memset(info->freq[L2], 0, sizeof(u16) * info->len[L2]);
	}
}

static struct freq_info *get_freq_info(struct ham_tracking_info *info, u64 *n)
{
	u64 i;
	u64 num = 0;
	struct freq_info *freq_info_array;
	u64 total_len;
	total_len = info->len[L1] + info->len[L2];
	freq_info_array = vzalloc(sizeof(struct freq_info) * total_len);
	if (!freq_info_array) {
		pr_err("unable to allocate memory for frequency info array\n");
		return NULL;
	}
	for (i = 0; i < info->len[L1]; i++) {
		if (info->freq[L1][i] != 0) {
			freq_info_array[num].hpa = info->paddr[L1][i];
			freq_info_array[num].freq = info->freq[L1][i];
			++num;
		}
	}
	if (info->l2_node != -1) {
		for (i = 0; i < info->len[L2]; i++) {
			if (info->freq[L2][i] != 0) {
				freq_info_array[num].hpa = info->paddr[L2][i];
				freq_info_array[num].freq = info->freq[L2][i];
				++num;
			}
		}
	}
	*n = num;
	return freq_info_array;
}

static ssize_t proc_file_read(struct file *file, char __user *buf, size_t count,
			      loff_t *pos)
{
	int ret = 0;
	struct ham_tracking_info *info;
	u64 num = 0;
	struct freq_info *freq_info_array;

	int pid = get_pid_from_tracking_file(file);
	if (pid < 0)
		return -EINVAL;

	spin_lock(&ham_lock);
	list_for_each_entry(info, &ham_pid_list, node) {
		if (info->pid == pid)
			break;
	}
	spin_unlock(&ham_lock);

	if (info->pid != pid) {
		pr_err("unable to find tracking file of pid: %d in process file system\n",
		       pid);
		return -ENXIO;
	}

	freq_info_array = get_freq_info(info, &num);
	if (!freq_info_array) {
		pr_err("unable to get frequency info array\n");
		return -ENOMEM;
	}
	if (*pos == 0) {
		if (copy_to_user(buf, &num, sizeof(u64))) {
			pr_err("unable to copy size of frequency info array to user space\n");
			ret = -EFAULT;
		}
		freq_info_num = num;
		*pos += sizeof(u64);
		count = sizeof(u64);
		goto out;
	}
	num = min(num, freq_info_num);
	if (copy_to_user(buf, freq_info_array,
			 sizeof(struct freq_info) * num)) {
		pr_err("unable to copy frequency info array to user space\n");
		ret = -EFAULT;
		goto out;
	}
	*pos += (sizeof(struct freq_info) * num);
	count += (sizeof(struct freq_info) * num);
	clear_tracking_info(info);

out:
	if (freq_info_array) {
		vfree(freq_info_array);
	}
	return ret ? ret : count;
}

static const struct proc_ops proc_file_ops = {
	.proc_read = proc_file_read,
};

int smap_create_tracking_info_file(struct ham_tracking_info *info)
{
	int ret;
	char path[64];
	struct proc_dir_entry *proc_file = NULL;
	struct proc_dir_entry *proc_dir = NULL;

	ret = scnprintf(path, sizeof(path), "%d_t", info->pid);
	if (!ret) {
		pr_err("wirte pid to path of pid: %d failed\n", info->pid);
		return ret;
	}
	proc_dir = proc_mkdir(path, NULL);
	if (!proc_dir) {
		pr_err("unable to create directory of pid: %d in process file system\n",
		       info->pid);
		return -ENOENT;
	}
	proc_file =
		proc_create("tracking_info", S_IRUGO, proc_dir, &proc_file_ops);
	if (!proc_file) {
		pr_err("unable to create tracking info file of pid: %d in process file system\n",
		       info->pid);
		remove_proc_entry(path, NULL);
		return -ENOENT;
	}
	info->pde = proc_dir;
	return 0;
}

int get_ham_pages_freqs(pid_t pid, struct freq_info **freq_info_array,
			uint64_t *freq_info_num)
{
	struct ham_tracking_info *info;
	if (!access_pid_is_scanning(pid)) {
		pr_info("the current access pid: %d is not scanning\n",
		        info->pid);
		return -EINVAL;
	}
	spin_lock(&ham_lock);
	list_for_each_entry(info, &ham_pid_list, node) {
		if (info->pid == pid)
			break;
	}
	spin_unlock(&ham_lock);

	if (info->pid != pid) {
		pr_err("unable to find pid: %d in HAM managed pid list\n",
		       info->pid);
		return -ENXIO;
	}

	change_ap_type(pid);
	*freq_info_array = get_freq_info(info, freq_info_num);
	if (!*freq_info_array) {
		pr_err("unable to get frequency info of pid: %d\n", info->pid);
		return -ENOMEM;
	}
	clear_tracking_info(info);
	return 0;
}
EXPORT_SYMBOL(get_ham_pages_freqs);

int pmd_huge(pmd_t md)
{
	return pmd_val(md) && !(pmd_val(md) & PMD_TABLE_BIT);
}

int pud_huge(pud_t ud)
{
#ifndef __PAGETABLE_PMD_FOLDED
	return pud_val(ud) && !(pud_val(ud) & PUD_TABLE_BIT);
#else
	return 0;
#endif
}

pte_t *huge_pte_offset(struct mm_struct *mm, unsigned long addr,
		       unsigned long sz)
{
	pgd_t *gdp;
	p4d_t *p4d;
	pud_t *pudp, ud;
	pmd_t *pmdp, md;

	gdp = pgd_offset(mm, addr);
	if (!pgd_present(READ_ONCE(*gdp)))
		return NULL;

	p4d = p4d_offset(gdp, addr);
	if (!p4d_present(READ_ONCE(*p4d)))
		return NULL;

	pudp = pud_offset(p4d, addr);
	ud = READ_ONCE(*pudp);
	if (sz != PUD_SIZE && pud_none(ud))
		return NULL;
	if (pud_huge(ud) || !pud_present(ud))
		return (pte_t *)pudp;

	if (sz == CONT_PMD_SIZE)
		addr &= CONT_PMD_MASK;

	pmdp = pmd_offset(pudp, addr);
	md = READ_ONCE(*pmdp);
	if (!(sz == PMD_SIZE || sz == CONT_PMD_SIZE) && pmd_none(md))
		return NULL;
	if (pmd_huge(md) || !pmd_present(md))
		return (pte_t *)pmdp;

	if (sz == CONT_PTE_SIZE)
		return pte_offset_kernel(pmdp, (addr & CONT_PTE_MASK));

	return NULL;
}

static void actc_data_add(phys_addr_t paddr, u32 page_size)
{
	struct access_tracking_dev *adev;
	int ret = 0, node_id;
	u64 pa_index;
	if (calc_paddr_acidx_acpi(paddr, &node_id, &pa_index, page_size)) {
		ret = calc_paddr_acidx_iomem(paddr, &node_id, &pa_index,
					     page_size);
	}
	if (unlikely(ret)) {
		return;
	}

	adev = get_access_tracking_dev(node_id);
	if (unlikely(!adev)) {
		return;
	}
	if (unlikely(pa_index >= adev->page_count)) {
		return;
	}
	adev->access_bit_actc_data[pa_index]++;
}

static int hva_to_hpa_hugetlb(struct kvm *kvm, u64 host_va)
{
	struct hstate *h;
	unsigned long hmask;
	unsigned long sz;
	pte_t *ptep;
	pte_t pte;
	phys_addr_t paddr;

	if (!kvm)
		return -EINVAL;

	h = hstate_vma(find_vma(kvm->mm, host_va));
	hmask = huge_page_mask(h);
	sz = huge_page_size(h);
	if (sz != PAGE_SIZE_2M)
		return -EINVAL;

	ptep = huge_pte_offset(kvm->mm, host_va & hmask, sz);
	if (!ptep) {
		return -EFAULT;
	}

	pte = smap_huge_ptep_get(ptep);
	if (!pte_present(pte)) {
		pr_err("unable to get PTE\n");
		return -EINVAL;
	}

	paddr = PFN_PHYS(pte_pfn(pte));
	actc_data_add(paddr, PAGE_SIZE_2M);

	return 0;
}

static void ham_actc_data_add(int pid, phys_addr_t paddr, u32 page_size)
{
	struct access_tracking_dev *adev;
	struct ham_tracking_info *tmp;
	int ret = 0, node_id;
	u64 pa_index;
	if (calc_paddr_acidx_acpi(paddr, &node_id, &pa_index, page_size)) {
		ret = calc_paddr_acidx_iomem(paddr, &node_id, &pa_index,
					     page_size);
	}
	if (unlikely(ret)) {
		return;
	}

	adev = get_access_tracking_dev(node_id);
	if (unlikely(!adev || pa_index >= adev->page_count)) {
		return;
	}
	spin_lock(&ham_lock);
	list_for_each_entry(tmp, &ham_pid_list, node) {
		if (tmp->pid == pid && tmp->l1_node == node_id) {
			tmp->paddr[L1][pa_index] = paddr;
			tmp->freq[L1][pa_index]++;
		}
		if (tmp->pid == pid && tmp->l2_node == node_id) {
			tmp->paddr[L2][pa_index] = paddr;
			tmp->freq[L2][pa_index]++;
		}
	}
	spin_unlock(&ham_lock);
}

static int hva_to_hpa_ham(struct kvm *kvm, u64 host_va, pid_t pid)
{
	struct hstate *h;
	unsigned long hmask;
	unsigned long sz;
	pte_t *ptep;
	pte_t pte;
	phys_addr_t paddr;

	if (!kvm)
		return -EINVAL;

	h = hstate_vma(find_vma(kvm->mm, host_va));
	hmask = huge_page_mask(h);
	sz = huge_page_size(h);
	if (sz != PAGE_SIZE_2M) {
		return -EINVAL;
	}

	ptep = huge_pte_offset(kvm->mm, host_va & hmask, sz);
	if (!ptep) {
		return -EFAULT;
	}

	pte = smap_huge_ptep_get(ptep);
	if (!pte_present(pte)) {
		pr_err("unable to get PTE\n");
		return -EINVAL;
	}

	paddr = PFN_PHYS(pte_pfn(pte));
	ham_actc_data_add(pid, paddr, PAGE_SIZE_2M);
	return 0;
}

static struct vm_area_struct *get_vma_if_huge_page(struct kvm *kvm,
						   unsigned long host_va)
{
	struct vm_area_struct *vma = NULL;
	if (!kvm->mm) {
		return NULL;
	}
	vma = find_vma(kvm->mm, host_va);
	if (!vma) {
		return NULL;
	}
	if (!is_vm_hugetlb_page(vma)) {
		return NULL;
	}
	return vma;
}

static int hva_to_hpa(struct kvm *kvm, u64 host_va, scan_type type, pid_t pid)
{
	int ret = 0;
	struct vm_area_struct *vma;

	if (!kvm || !kvm->mm)
		return -EINVAL;
	vma = find_vma(kvm->mm, host_va);
	if (!vma) {
		return -EFAULT;
	}

	if (is_vm_hugetlb_page(vma)) {
		if (type == HAM_SCAN) {
			hva_to_hpa_ham(kvm, host_va, pid);
		} else {
			ret = hva_to_hpa_hugetlb(kvm, host_va);
		}
	}
	return ret;
}

static bool find_tracking_index(struct statistics_tracking_info *stat_info,
				unsigned long vaddr, unsigned int *index)
{
	u64 i;

	for (i = 0; i < stat_info->page_num[L1]; i++) {
		if (stat_info->vaddr[L1][i] == vaddr) {
			*index = i;
			return true;
		}
	}

	for (i = 0; i < stat_info->page_num[L2]; i++) {
		if (stat_info->vaddr[L2][i] == vaddr) {
			*index = i + stat_info->page_num[L1];
			return true;
		}
	}

	return false;
}

static int calc_tracking_index(unsigned long vaddr, pid_t pid,
			       unsigned int *index)
{
	bool found = false;
	struct statistics_tracking_info *tmp;
	spin_lock(&statistic_lock);
	list_for_each_entry(tmp, &statistic_pid_list, node) {
		if (tmp->pid == pid) {
			found = find_tracking_index(tmp, vaddr, index);
			if (found) {
				break;
			}
		}
	}
	spin_unlock(&statistic_lock);
	if (!found)
		return -ERANGE;
	return 0;
}

static void clear_statistic_tracking_info(pid_t pid)
{
	u32 wins_index;
	struct statistics_tracking_info *tmp;
	spin_lock(&statistic_lock);
	list_for_each_entry(tmp, &statistic_pid_list, node) {
		if (tmp->pid == pid) {
			wins_index = tmp->scan_num;
			if (tmp->sliding_windows[wins_index]) {
				memset(tmp->sliding_windows[wins_index],
				       DEFAULT_REF_COUNT,
				       (tmp->page_num[L1] + tmp->page_num[L2]) *
					       sizeof(u8));
			}
		}
	}
	spin_unlock(&statistic_lock);
}

static void update_tracking_info(pid_t pid, unsigned int index)
{
	u32 wins_index;
	struct statistics_tracking_info *tmp;
	spin_lock(&statistic_lock);
	list_for_each_entry(tmp, &statistic_pid_list, node) {
		if (tmp->pid == pid) {
			wins_index = tmp->scan_num;
			tmp->sliding_windows[wins_index][index] = TRUE_REF;
			break;
		}
	}
	spin_unlock(&statistic_lock);
}

static int fill_statistics_tracking(unsigned long hva, pid_t pid)
{
	int ret;
	unsigned int index;
	ret = calc_tracking_index(hva, pid, &index);
	if (ret) {
		pr_err("out of range while finding index of HVA\n");
		return ret;
	}

	update_tracking_info(pid, index);
	return ret;
}

static bool memslot_is_mem(struct kvm_memory_slot *memslot)
{
	if (memslot->base_gfn < (1 << GB_TO_4K_SHIFT)) {
		return false;
	}
	if (memslot->npages <= (1 << HUGE_TO_4K_SHIFT)) {
		return false;
	}
	return true;
}

static int scan_kvm_memslots(struct kvm *kvm, pid_t pid, int page_size,
			     scan_type type)
{
	struct kvm_memslots *slots;
	struct kvm_memory_slot *memslot;
	gpa_t gpa;
	int bkt;

	if (!kvm || !kvm->arch.mmu.pgt) {
		pr_err("invalid kvm passed to scan kvm memslots\n");
		return -EINVAL;
	}

	slots = kvm_memslots(kvm);
	if (!slots)
		return -EINVAL;
	clear_statistic_tracking_info(pid);
	kvm_for_each_memslot(memslot, bkt, slots) {
		unsigned long hva;
		int ret;
		if (!memslot_is_mem(memslot))
			continue;
		for (gpa = memslot->base_gfn << PAGE_SHIFT;
		     gpa < (memslot->base_gfn + memslot->npages) << PAGE_SHIFT;
		     gpa += (gpa_t)page_size) {
			if (!smap_kvm_pgtable_stage2_mkold(kvm->arch.mmu.pgt,
							   gpa))
				continue;
			hva = gfn_to_hva_memslot(memslot, gpa_to_gfn(gpa));
			if (!get_vma_if_huge_page(kvm, hva))
				continue;
			ret = hva_to_hpa(kvm, hva, type, pid);
			if (ret)
				continue;
			if (type == STATISTIC_SCAN) {
				ret = fill_statistics_tracking(hva, pid);
			}
		}
	}
	kvm_flush_remote_tlbs(kvm);

	return 0;
}

static inline int pre_scan_kvm_memslots(struct kvm *kvm)
{
	int idx = srcu_read_lock(&kvm->srcu);
	mmap_read_lock(kvm->mm);
	read_lock(&kvm->mmu_lock);
	return idx;
}

static inline void post_scan_kvm_memslots(struct kvm *kvm, int idx)
{
	read_unlock(&kvm->mmu_lock);
	mmap_read_unlock(kvm->mm);
	srcu_read_unlock(&kvm->srcu, idx);
}

static int hva_cmp(const void *a, const void *b)
{
	const struct hva_info *x = a, *y = b;
	if (x->nid < y->nid) {
		return -1;
	} else if (x->nid > y->nid) {
		return 1;
	} else {
		if (x->va < y->va) {
			return -1;
		} else if (x->va > y->va) {
			return 1;
		} else {
			return 0;
		}
	}
}

static int get_total_huge_page_nr(struct kvm *kvm, u64 *total_huge_page_nr)
{
	u64 huge_page_nr = 0;
	struct kvm_memslots *slots;
	struct kvm_memory_slot *memslot;
	gpa_t gpa;
	int bkt;
	slots = kvm_memslots(kvm);
	if (!slots) {
		return -EINVAL;
	}
	kvm_for_each_memslot(memslot, bkt, slots) {
		unsigned long hva;
		for (gpa = memslot->base_gfn << PAGE_SHIFT;
		     gpa < (memslot->base_gfn + memslot->npages) << PAGE_SHIFT;
		     gpa += PAGE_SIZE_2M) {
			hva = gfn_to_hva_memslot(memslot, gpa_to_gfn(gpa));
			if (get_vma_if_huge_page(kvm, hva))
				++huge_page_nr;
		}
	}
	*total_huge_page_nr = huge_page_nr;
	return 0;
}

static int get_vma_numa_node(struct kvm *kvm, struct vm_area_struct *vma,
			     unsigned long addr)
{
	struct hstate *h;
	unsigned long hmask;
	unsigned long sz;
	pte_t *ptep;
	pte_t pte;
	int node_calc, page_size;
	u64 index;
	phys_addr_t paddr;

	node_calc = NUMA_NO_NODE;
	if (!kvm) {
		return NUMA_NO_NODE;
	}

	h = hstate_vma(vma);
	hmask = huge_page_mask(h);
	sz = huge_page_size(h);
	if (sz != PAGE_SIZE_2M) {
		return NUMA_NO_NODE;
	}

	ptep = huge_pte_offset(kvm->mm, addr & hmask, sz);
	if (!ptep) {
		return NUMA_NO_NODE;
	}

	pte = smap_huge_ptep_get(ptep);
	if (!pte_present(pte)) {
		pr_err("PTE is not presented\n");
		return NUMA_NO_NODE;
	}

	paddr = PFN_PHYS(pte_pfn(pte));
	page_size = PAGE_SIZE_2M;
	if (calc_paddr_acidx_iomem(paddr, &node_calc, &index, page_size) == 0) {
		return node_calc;
	} else if (calc_paddr_acidx_acpi(paddr, &node_calc, &index,
					 page_size) == 0) {
		return node_calc;
	}

	return NUMA_NO_NODE;
}

static int fill_vaddrs_info(struct kvm *kvm, struct hva_info *hva_vec, u64 len,
			    u64 *l1_page_num, u64 *l2_page_num)
{
	int bkt, cur_node;
	u64 l1_page_nr = 0, l2_page_nr = 0;
	gpa_t gpa;
	struct kvm_memory_slot *memslot;
	struct kvm_memslots *slots = kvm_memslots(kvm);
	u64 idx = 0;
	if (!slots) {
		return -EINVAL;
	}
	kvm_for_each_memslot(memslot, bkt, slots) {
		unsigned long hva;
		for (gpa = memslot->base_gfn << PAGE_SHIFT;
		     gpa < (memslot->base_gfn + memslot->npages) << PAGE_SHIFT;
		     gpa += PAGE_SIZE_2M) {
			struct vm_area_struct *vma;
			if (idx >= len) {
				pr_err("exceeds upper bound: %llu when looking up GPA in memslots\n",
				       len);
				return -EFAULT;
			}
			hva = gfn_to_hva_memslot(memslot, gpa_to_gfn(gpa));
			vma = get_vma_if_huge_page(kvm, hva);
			if (vma == NULL)
				continue;
			cur_node = get_vma_numa_node(kvm, vma, hva);
			hva_vec[idx] = (struct hva_info){ hva, cur_node };
			if (cur_node >= nr_local_numa &&
			    cur_node < nr_local_numa + SMAP_MAX_REMOTE_NUMNODES)
				++l2_page_nr;
			else if (cur_node < nr_local_numa && cur_node >= 0)
				++l1_page_nr;
			else {
				pr_err("invalid NUMA index: %d\n", cur_node);
				return -EFAULT;
			}
			++idx;
		}
	}
	if (l1_page_nr + l2_page_nr != len) {
		pr_err("failed to fill page info due to number of pages to fill not equal to that of total pages\n");
		return -EFAULT;
	}
	*l1_page_num = l1_page_nr;
	*l2_page_num = l2_page_nr;
	return 0;
}

static int get_hva_info_by_scan_kvm_memslots(struct kvm *kvm, u64 *l1_page_num,
					     u64 *l2_page_num,
					     u64 total_huge_page_num,
					     u64 *vaddrs,
					     struct hva_info *hva_vec)
{
	u64 l1_page_nr, l2_page_nr, i;
	if (!kvm || !kvm->arch.mmu.pgt) {
		pr_err("invalid kvm passed to scan kvm memslots\n");
		return -EINVAL;
	}

	l1_page_nr = 0;
	l2_page_nr = 0;
	if (fill_vaddrs_info(kvm, hva_vec, total_huge_page_num, &l1_page_nr,
			     &l2_page_nr))
		return -EINVAL;
	pr_debug("L1 huge page num=%llu, L2 huge page num=%llu\n", l1_page_nr,
		 l2_page_nr);
	sort(hva_vec, total_huge_page_num, sizeof(struct hva_info), hva_cmp,
	     NULL);
	for (i = 0; i < total_huge_page_num; ++i) {
		vaddrs[i] = hva_vec[i].va;
	}
	*l1_page_num = l1_page_nr;
	*l2_page_num = l2_page_nr;
	return 0;
}

static int alloc_vaddr_info(struct kvm *kvm, struct hva_info **hva_vec,
			    u64 **vaddrs, u64 total_huge_page_nr)
{
	struct hva_info *hva_vec_tmp = NULL;
	u64 *vaddrs_tmp = NULL;

	if (!kvm || !kvm->arch.mmu.pgt) {
		pr_err("invalid kvm passed to allocate memory to store page info\n");
		return -EINVAL;
	}

	hva_vec_tmp = vzalloc(total_huge_page_nr * sizeof(struct hva_info));
	if (!hva_vec_tmp) {
		pr_err("unable to allocate memory for page info vector\n");
		return -ENOMEM;
	}
	vaddrs_tmp = vzalloc(total_huge_page_nr * sizeof(u64));
	if (!vaddrs_tmp) {
		pr_err("unable to allocate memory for virtual address vector\n");
		vfree(hva_vec_tmp);
		return -ENOMEM;
	}
	*vaddrs = vaddrs_tmp;
	*hva_vec = hva_vec_tmp;
	return 0;
}

/* Caller must free those pointers after successfully called this function */
static int get_kvm_by_pid(pid_t pid_nr, struct pid **pid,
			  struct task_struct **task, struct file **filp,
			  struct kvm **kvm)
{
	struct pid *pid_tmp;
	struct task_struct *task_tmp;
	struct file *filp_tmp;
	struct kvm *kvm_tmp;

	pid_tmp = find_get_pid(pid_nr);
	task_tmp = get_pid_task(pid_tmp, PIDTYPE_PID);
	if (!task_tmp) {
		pr_err("unable to get task struct of pid: %d\n", pid_nr);
		put_pid(pid_tmp);
		return -ESRCH;
	}
	filp_tmp = get_kvm_file_from_task(task_tmp);
	if (!filp_tmp) {
		put_task_struct(task_tmp);
		put_pid(pid_tmp);
		pr_err("unable to get kvm file of pid: %d\n", pid_nr);
		return -EBADF;
	}
	kvm_tmp = filp_tmp->private_data;
	if (!kvm_tmp) {
		put_task_struct(task_tmp);
		put_pid(pid_tmp);
		pr_err("unable to get kvm of pid: %d\n", pid_nr);
		return -EINVAL;
	}
	*pid = pid_tmp;
	*task = task_tmp;
	*filp = filp_tmp;
	*kvm = kvm_tmp;
	return 0;
}

int scan_hva_info(pid_t pid_nr, u64 *l1_page_num, u64 *l2_page_num,
		  u64 **l1_vaddr, u64 **l2_vaddr)
{
	struct pid *pid;
	struct task_struct *task;
	struct file *filp;
	struct kvm *kvm;
	int ret, srcu_idx;
	u64 l1_page_num_tmp, l2_page_num_tmp, total_huge_page_nr;
	u64 *vaddrs;
	struct hva_info *hva_vec;
	ret = get_kvm_by_pid(pid_nr, &pid, &task, &filp, &kvm);
	if (ret) {
		pr_err("unable to get kvm of pid: %d\n", pid_nr);
		return ret;
	}

	srcu_idx = pre_scan_kvm_memslots(kvm);
	ret = get_total_huge_page_nr(kvm, &total_huge_page_nr);
	post_scan_kvm_memslots(kvm, srcu_idx);
	if (ret) {
		pr_err("unable to get total hugepage number of pid: %d\n",
		       pid_nr);
		goto out_free_task_file;
	}

	ret = alloc_vaddr_info(kvm, &hva_vec, &vaddrs, total_huge_page_nr);
	if (ret) {
		pr_err("unable to allocate memory to store virtual address info of pid: %d\n",
		       pid_nr);
		goto out_free_task_file;
	}

	srcu_idx = pre_scan_kvm_memslots(kvm);
	ret = get_hva_info_by_scan_kvm_memslots(kvm, &l1_page_num_tmp,
						&l2_page_num_tmp,
						total_huge_page_nr, vaddrs,
						hva_vec);
	post_scan_kvm_memslots(kvm, srcu_idx);

	if (ret) {
		pr_err("unable to get frequency info of all pages of pid: %d by scan kvm\n",
		       pid_nr);
		vfree(hva_vec);
		vfree(vaddrs);
		goto out_free_task_file;
	}
	*l1_page_num = l1_page_num_tmp;
	*l2_page_num = l2_page_num_tmp;
	*l1_vaddr = vaddrs;
	*l2_vaddr = vaddrs + l1_page_num_tmp;
	vfree(hva_vec);
out_free_task_file:
	fput(filp);
	put_task_struct(task);
	put_pid(pid);
	return ret;
}

static void release_resources(struct file *filp, struct task_struct *task,
			      struct pid *pid)
{
	if (filp)
		fput(filp);
	if (task)
		put_task_struct(task);
	if (pid)
		put_pid(pid);
}

static void update_statistic_scan_num(pid_t pid)
{
	struct statistics_tracking_info *tmp;
	spin_lock(&statistic_lock);
	list_for_each_entry(tmp, &statistic_pid_list, node) {
		if (tmp->pid == pid) {
			tmp->scan_num++;
			tmp->scan_num %= tmp->window_num;
			break;
		}
	}
	spin_unlock(&statistic_lock);
}

static int scan_forward_2M(pid_t pid, int page_size, scan_type type)
{
	int srcu_idx;
	struct file *filp;
	struct kvm *kvm;
	struct pid *pid_s;
	struct task_struct *task;
	if (type == NO_SCAN) {
		return 0;
	}
	pid_s = find_get_pid(pid);
	task = get_pid_task(pid_s, PIDTYPE_PID);
	if (!task) {
		put_pid(pid_s);
		return -EINVAL;
	}

	filp = get_kvm_file_from_task(task);
	if (!filp) {
		release_resources(filp, task, pid_s);
		return -EINVAL;
	}
	kvm = filp->private_data;
	if (!kvm) {
		release_resources(filp, task, pid_s);
		return -EINVAL;
	}
	srcu_idx = pre_scan_kvm_memslots(kvm);
	if (scan_kvm_memslots(kvm, pid, page_size, type))
		pr_err("failed to scan kvm mem slots for pid: %d\n", pid);

	post_scan_kvm_memslots(kvm, srcu_idx);
	release_resources(filp, task, pid_s);
	update_statistic_scan_num(pid);
	return 0;
}

static inline void smap_flush_tlb_mm(struct mm_struct *mm)
{
	unsigned long asid;
	dsb(ishst);
	asid = __TLBI_VADDR(0, ASID(mm));
	__tlbi(aside1is, asid);
	__tlbi_user(aside1is, asid);
	dsb(ish);
}

static int check_pte_young(pte_t *pte, unsigned long addr, unsigned long next,
			   struct mm_walk *walk)
{
	struct pte_walk *pte_walk = walk->private;
	if (is_swap_pte(*pte)) {
		return 0;
	}
	if (pte_young(*pte)) {
		phys_addr_t paddr = (phys_addr_t)__pte_to_phys(*pte);
		if (paddr == 0) {
			return 0;
		}
		actc_data_add(paddr, PAGE_SIZE_4K);
		pte_walk->flag = true;
		__ptep_test_and_clear_young(NULL, 0, pte);
	}
	return 0;
}

static const struct mm_walk_ops pte_range_ops = {
	.pte_entry = check_pte_young,
};

static int small_vma_walk(struct mm_struct *mm, unsigned long start_vaddr,
			  unsigned long end_vaddr, struct pte_walk *pte_walk)
{
	int ret;

	ret = mmap_read_lock_killable(mm);
	if (ret) {
		pr_err("unable to get mmap read lock, ret: %d\n", ret);
		return ret;
	}

	ret = walk_page_range(mm, start_vaddr, end_vaddr, &pte_range_ops,
			      pte_walk);
	if (ret) {
		pr_err("failed to walk page range, ret: %d\n", ret);
		mmap_read_unlock(mm);
		return ret;
	}
	mmap_read_unlock(mm);
	return 0;
}

static int huge_vma_walk(struct mm_struct *mm, struct smap_vma_struct *vma,
			 struct pte_walk *pte_walk)
{
	int ret;
	unsigned long start_vaddr;
	unsigned long end_vaddr;
	unsigned long end;

	start_vaddr = vma->start_vaddr;
	end_vaddr = vma->end_vaddr;
	while (start_vaddr < end_vaddr) {
		end = (start_vaddr + MMAPLOCK_BATCH_SIZE);
		end = min(end, end_vaddr);
		ret = small_vma_walk(mm, start_vaddr, end, pte_walk);
		if (ret) {
			return ret;
		}
		start_vaddr = end;
	}
	return 0;
}

static int take_vma_snapshot(struct mm_struct *mm,
			     struct smap_vma_struct **vma_arr, int *vma_count)
{
	int count = 0;
	int ret;
	int i = 0;
	struct vm_area_struct *vma;
	struct smap_vma_struct *arr;

	ret = mmap_read_lock_killable(mm);
	if (ret) {
		pr_err("unable to get mmap read lock, ret: %d\n", ret);
		return ret;
	}
	for (vma = find_vma(mm, 0); vma; vma = find_vma(mm, vma->vm_end)) {
		count++;
	}
	if (!count) {
		pr_err("unable to get VMA\n");
		mmap_read_unlock(mm);
		return -EINVAL;
	}
	arr = kzalloc(sizeof(struct smap_vma_struct) * count, GFP_KERNEL);
	if (!arr) {
		pr_err("unable to allocate memory for SMAP VMA descriptor\n");
		mmap_read_unlock(mm);
		return -ENOMEM;
	}
	for (vma = find_vma(mm, 0); vma && i < count;
	     vma = find_vma(mm, vma->vm_end), i++) {
		arr[i].start_vaddr = vma->vm_start;
		arr[i].end_vaddr = vma->vm_end;
	}
	*vma_count = count;
	*vma_arr = arr;
	mmap_read_unlock(mm);
	return 0;
}

static int scan_forward_4k_mm(int pid, int page_size)
{
	int ret;
	int i = 0;
	int vma_count = 0;
	struct mm_struct *mm;
	struct smap_vma_struct *vma_array = NULL;
	struct pte_walk pte_walk = { .flag = false };

	mm = get_mm_by_pid(pid);
	if (IS_ERR(mm) || !mm || !mmget_not_zero(mm)) {
		pr_err("bad mm of pid: %d\n", pid);
		return -EINVAL;
	}
	ret = take_vma_snapshot(mm, &vma_array, &vma_count);
	if (ret) {
		pr_err("failed to take VMA snapshot, ret: %d\n", ret);
		mmput(mm);
		return -EINVAL;
	}
	for (i = 0; i < vma_count; i++) {
		if (vma_array[i].end_vaddr - vma_array[i].start_vaddr <
		    MMAPLOCK_BATCH_SIZE) {
			ret = small_vma_walk(mm, vma_array[i].start_vaddr,
					     vma_array[i].end_vaddr, &pte_walk);
			if (ret) {
				pr_err("failed to walk VMA, pid: %d, ret: %d\n",
				       pid, ret);
				break;
			}
		} else {
			ret = huge_vma_walk(mm, &vma_array[i], &pte_walk);
			if (ret) {
				pr_err("failed to walk VMA, pid: %d, ret: %d\n",
				       pid, ret);
				break;
			}
		}
	}
	if (pte_walk.flag) {
		smap_flush_tlb_mm(mm);
	}
	mmput(mm);
	kfree(vma_array);
	return 0;
}

int scan_accessed_bit_forward_vm(pid_t pid, int page_size, scan_type type)
{
	if (page_size == PAGE_SIZE_2M) {
		return scan_forward_2M(pid, page_size, type);
	} else {
		return -EINVAL;
	}
}

int scan_accessed_bit_forward_mm(pid_t pid, int page_size)
{
	if (page_size == PAGE_SIZE_4K) {
		return scan_forward_4k_mm(pid, page_size);
	}
	return -EINVAL;
}