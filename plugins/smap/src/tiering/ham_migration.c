// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "ham_migration.h"
#include <asm/current.h>
#include <linux/cdev.h>
#include <linux/sort.h>
#include <linux/hugetlb.h>
#include <linux/workqueue.h>
#include <linux/cpuset.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/umh.h>
#include <linux/nodemask.h>
#include <linux/rwlock.h>
#include <linux/rwlock_types.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/migrate.h>
#include <linux/numa_remote.h>
#include <linux/pagewalk.h>
#include <linux/fs.h>
#include <linux/string.h>

#include "basic.h"
#include "ham_tasks_mgr.h"
#include "coherence_maintain.h"

#undef pr_fmt
#define pr_fmt(fmt) "HAM: " fmt
#define HAM_DEV_COUNT 1
#define HAM_4k_to_2M 9
#define DECIMAL 10

#define TIME_LENTH (msecs_to_jiffies(2000))
#define FINISH_LENGTH ((jiffies) + (msecs_to_jiffies(60000)))
#define MAX_RETRY_TIMES 1000
#define WAITE_TIME 100
#define MAX_HUGEPAGE_NUM (256 * 512)
#define MAX_DATA_SIZE 10
#define MAX_PATH_SIZE 100
#define NODE_2MB_HUGEPAGES_PATH \
	"/sys/devices/system/node/node%u/hugepages/hugepages-2048kB/nr_hugepages"
#define GET_FOLIO_RETRY_TIMES 4

typedef struct folio *get_migration_folio_t(struct ham_page_map *hpm);

static struct cdev g_ham_cdev;
static dev_t g_ham_dev;
static struct class *g_ham_class;
static struct device *g_ham_device;

static struct workqueue_struct *ham_wq;
static struct delayed_work ham_work;

static bool global_cache_mnt = false;

struct folio *ham_alloc_huge_page_node(int nid)
{
	gfp_t gfp_mask = GFP_HIGHUSER_MOVABLE;
	if (nid != NUMA_NO_NODE)
		gfp_mask |= __GFP_THISNODE;

	return get_hugetlb_folio_nodemask(PAGE_SIZE_2M, nid, NULL, gfp_mask,
					  NULL);
}

static int config_system_huge_page(unsigned long huge_page_number,
				   unsigned int node_id, struct hstate *h)
{
	ssize_t buf_size;
	loff_t pos = 0;
	struct file *file;
	char data[MAX_DATA_SIZE];
	char path[MAX_PATH_SIZE];
	unsigned int nr_page;
	int retry_times = 0;
	buf_size =
		snprintf(path, MAX_PATH_SIZE, NODE_2MB_HUGEPAGES_PATH, node_id);
	if (buf_size < 0 || buf_size >= MAX_PATH_SIZE)
		return -ENOEXEC;

	buf_size = snprintf(data, MAX_DATA_SIZE, "%lu", huge_page_number);
	if (buf_size < 0 || buf_size >= MAX_DATA_SIZE)
		return -ENOEXEC;

	file = filp_open(path, O_RDWR | O_TRUNC, 0);
	if (IS_ERR(file)) {
		pr_err("failed to open file, ret: %ld\n", PTR_ERR(file));
		return PTR_ERR(file);
	}

	while (retry_times < MAX_RETRY_TIMES) {
		buf_size = kernel_write(file, data,
					strnlen(data, MAX_DATA_SIZE), &pos);
		if (buf_size < 0) {
			pr_err("failed to write huge page number, data size: %ld\n",
			       strnlen(data, MAX_DATA_SIZE));
			break;
		}
		nr_page = h->nr_huge_pages_node[node_id];
		if (nr_page == huge_page_number) {
			pr_info("hugepage configuration succeeded: NUMA id = %u, nr_hugepages = %u, retry times = %d\n",
				node_id, nr_page, retry_times);
			filp_close(file, NULL);
			return 0;
		}
		retry_times++;
		msleep(WAITE_TIME);
	}

	filp_close(file, NULL);
	pr_err("nr_hugepages on node%d: %u, retry times: %d\n", node_id, nr_page,
	       retry_times);
	return -EFAULT;
}

static int compare_page_list(const void *a, const void *b)
{
	struct ham_page_map *struct_a = (struct ham_page_map *)a;
	struct ham_page_map *struct_b = (struct ham_page_map *)b;

	u64 hpa_a = PFN_PHYS(folio_pfn(struct_a->dst_folio));
	u64 hpa_b = PFN_PHYS(folio_pfn(struct_b->dst_folio));
	return (hpa_a > hpa_b) - (hpa_a < hpa_b);
}

static int compare_page_freq(const void *a, const void *b)
{
	struct ham_page_map *struct_a =
		list_entry(a, struct ham_page_map, list);
	struct ham_page_map *struct_b =
		list_entry(b, struct ham_page_map, list);

	return (struct_a->freq < struct_b->freq) -
	       (struct_a->freq > struct_b->freq);
}

static int init_numa_page_map(struct ham_migrate_task *mig_task,
			      struct migration_param *arg)
{
	unsigned int i;
	int ret;
	unsigned int page_num;
	struct ram_block_info *rbi;
	if (mig_task->numa_cnt == 0 || mig_task->numa_cnt > BATCH_NUM ||
	    arg->cnt == 0 || arg->cnt > BATCH_NUM) {
		pr_err("invalid params passed to init numa page map, numa_cnt = %u,  arg_cnt = %u\n",
		       mig_task->numa_cnt, arg->cnt);
		return -ENOMEM;
	}
	mig_task->ram_maps = kzalloc(
		mig_task->numa_cnt * sizeof(struct ram_block_map), GFP_KERNEL);
	if (!mig_task->ram_maps) {
		pr_err("unable to allocate memory for ram maps\n");
		return -ENOMEM;
	}

	for (i = 0; i < arg->cnt; i++) {
		rbi = &arg->ram_blocks[i];
		mig_task->ram_maps[i].rmt_numa_id = rbi->rmt_numa_id;
		mig_task->ram_maps[i].size = rbi->size;
		mig_task->ram_maps[i].hva_start = rbi->hva_start;
		page_num = rbi->size / PAGE_SIZE_2M;
		mig_task->ram_maps[i].page_num = page_num;
		mig_task->ram_maps[i].hpms = vzalloc(
			page_num * sizeof(struct ham_page_map));
		if (!mig_task->ram_maps[i].hpms) {
			pr_err("unable to allocate memory for HAM page map\n");
			ret = -ENOMEM;
			goto out_err;
		}
	}
	return 0;
out_err:
	for (i = 0; i < arg->cnt; i++) {
		if (mig_task->ram_maps[i].hpms) {
			vfree(mig_task->ram_maps[i].hpms);
			mig_task->ram_maps[i].hpms = NULL;
		}
	}
	kfree(mig_task->ram_maps);
	mig_task->ram_maps = NULL;
	return ret;
}

static struct ham_migrate_task *init_migrate_task(struct migration_param *arg,
						  struct hstate *h)
{
	struct ham_migrate_task *mig_task;

	mig_task = allocate_migrate_task(arg->pid);
	if (!mig_task) {
		return NULL;
	}
	mig_task->numa_cnt = arg->cnt;
	mig_task->hstate = h;
	return mig_task;
}

static void reclaim_task_fn(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	release_finished_tasks();
	queue_delayed_work(ham_wq, dwork, TIME_LENTH);
}

static int init_global_task_list(void)
{
	init_task_list();
	ham_wq = create_workqueue("ham_wq");
	if (!ham_wq) {
		pr_err("failed to create workqueue\n");
		return -ENOMEM;
	}
	INIT_DELAYED_WORK(&ham_work, reclaim_task_fn);
	queue_delayed_work(ham_wq, &ham_work, TIME_LENTH);
	return 0;
}

static void update_mems_allowed_with_remote_nodes(struct migration_param *param)
{
	unsigned int i;
	nodemask_t nodemask = cpuset_current_mems_allowed;
	for (i = 0; i < param->cnt; i++)
		node_set(param->ram_blocks[i].rmt_numa_id, nodemask);
	set_mems_allowed(nodemask);
}

static struct folio *alloc_folio_with_retry(int nid,
					    struct ham_migrate_task *mig_task,
					    struct migration_param *param)
{
	struct folio *new_folio = NULL;
	int retry_times;

	for (retry_times = 0; retry_times < GET_FOLIO_RETRY_TIMES;
	     retry_times++) {
		new_folio = ham_alloc_huge_page_node(nid);
		if (new_folio) {
			return new_folio;
		}

		pr_warn("unable to allocate folio, nid: %d nr_hugepages: %u, free_hugepages: %u\n",
			nid, mig_task->hstate->nr_huge_pages_node[nid],
			mig_task->hstate->free_huge_pages_node[nid]);
		/*
		 * When the folio allocation fails, it may be because the cpuset.mems permission was concurrently
		 * modified by libvirt to match the vm. Therefore, the permission is reset here,
		 * and the folio allocation is attempted again.
		 */
		update_mems_allowed_with_remote_nodes(param);
	}

	pr_err("unable to allocate folio after %d retries\n",
	       GET_FOLIO_RETRY_TIMES);
	return NULL;
}

static int create_numa_map(struct ham_migrate_task *mig_task,
			   struct migration_param *param)
{
	struct folio *new_folio;
	int ret, nid;
	unsigned int i, j;
	struct ram_block_map *map;
	ret = init_numa_page_map(mig_task, param);
	if (ret) {
		pr_err("HAM migrate task init numa page map failed, pid: %d\n",
		       mig_task->pid);
		return -ENOMEM;
	}

	if (check_task_status(mig_task, HAM_TASK_ALLOW_MIGR)) {
		return 0;
	}

	update_mems_allowed_with_remote_nodes(param);
	map = mig_task->ram_maps;
	for (i = 0; i < mig_task->numa_cnt; i++) {
		for (j = 0; j < map[i].page_num; j++) {
			nid = map[i].rmt_numa_id;
			new_folio =
				alloc_folio_with_retry(nid, mig_task, param);
			if (new_folio == NULL) {
				return -ENOMEM;
			}
			map[i].hpms[j].src_numa_id = NUMA_NO_NODE;
			map[i].hpms[j].src_folio = NULL;
			map[i].hpms[j].dst_folio = new_folio;
			map[i].hpms[j].is_migrate = false;
			map[i].hpms[j].present = false;
			map[i].hpms[j].freq = 0;
		}
		sort(map[i].hpms, map[i].page_num, sizeof(struct ham_page_map),
		     compare_page_list, NULL);
		map[i].rmt_numa_start =
			PFN_PHYS(folio_pfn(map[i].hpms[0].dst_folio));
		map[i].cacheable = true;
	}
	return 0;
}

static int fill_folios_hugetlb(pte_t *pte, unsigned long hmask,
			       unsigned long addr, unsigned long end,
			       struct mm_walk *walk)
{
	struct ram_block_map *ram_map = walk->private;
	struct folio *folio;
	pte_t entry;
	unsigned long index;

	index = (addr - ram_map->hva_start) >> PMD_SHIFT;
	if (index >= ram_map->page_num) {
		pr_err("index out of bounds [%lu/%u]\n", index,
		       ram_map->page_num);
		return -EINVAL;
	}

	entry = kernel_huge_ptep_get(pte);
	if (!pte_present(entry)) {
		return 0;
	}

	folio = pfn_folio(pte_pfn(entry));
	/* Skip folio which has been migrated */
	if (folio_nid(folio) == ram_map->rmt_numa_id) {
		return 0;
	}

	/* Save src_folio */
	ram_map->hpms[index].src_folio = folio;
	ram_map->hpms[index].src_numa_id = folio_nid(folio);
	ram_map->hpms[index].present = true;
	return 0;
}

/*
 * Walk through page tables and collect pages to be migrated.
 */
static int fill_task_pages(struct ham_migrate_task *mig_task)
{
	struct mm_walk_ops walk_ops = {
		.hugetlb_entry = fill_folios_hugetlb,
	};

	struct ram_block_map *ram_map;
	struct mm_struct *mm;
	unsigned int i;
	int ret = 0;

	mm = find_get_mm_by_vpid(mig_task->pid);
	if (!mm) {
		return -EINVAL;
	}

	mmap_read_lock(mm);
	for (i = 0; i < mig_task->numa_cnt; i++) {
		ram_map = &mig_task->ram_maps[i];
		ret = walk_page_range(mm, ram_map->hva_start,
				      ram_map->hva_start + ram_map->size,
				      &walk_ops, ram_map);
		if (ret) {
			break;
		}
	}
	mmap_read_unlock(mm);
	mmput(mm);
	return ret;
}

static void sort_hpm_list(struct list_head *head, unsigned int nr_hpm)
{
	struct list_head **array;
	unsigned int i = 0;
	struct ham_page_map *hpm;

	if (nr_hpm == 0) {
		return;
	}
	array = kmalloc_array(nr_hpm, sizeof(struct list_head *), GFP_KERNEL);
	if (!array) {
		pr_warn("unable to allocate memory for HAM page map array\n");
		return;
	}
	list_for_each_entry(hpm, head, list) {
		array[i++] = &hpm->list;
	}
	sort(array, nr_hpm, sizeof(struct list_head *), compare_page_freq,
	     NULL);

	INIT_LIST_HEAD(head);
	for (i = 0; i < nr_hpm; i++) {
		list_add_tail(array[i], head);
	}
	kfree(array);
	pr_info("sort page list by frequency successfully\n");
}

/*
 * queue_qualified_pages - Filter qualified ham_page_map from the migrate
 *          task and add them to the list.
 *
 * @mig_task:       The ham migrate task.
 * @migrate_flag:   The flag indicating whether the page to be filtered has
 *          been migrated.
 * @ret_hpm_list:   The filtered list of qualified ham_page_map.
 *
 * Returns the number of qualified ham_page_map which is also the length
 * of the ret_hpm_list.
 */
static unsigned int queue_qualified_pages(struct ham_migrate_task *mig_task,
					  bool migrate_flag,
					  struct list_head *ret_hpm_list)
{
	struct ram_block_map *ram_map;
	struct ham_page_map *hpm;
	unsigned int i, j, nr_hpm = 0;

	for (i = 0; i < mig_task->numa_cnt; i++) {
		ram_map = &mig_task->ram_maps[i];
		for (j = 0; j < ram_map->page_num; j++) {
			hpm = &ram_map->hpms[j];
			if (!hpm->present || hpm->is_migrate != migrate_flag) {
				continue;
			}

			/*
			 * Prioritizing the migration of low-address memory appears
			 * to be more effective than prioritizing the migration of
			 * high-address memory. Therefore, list_add_tail() is used
			 * instead of list_add() here.
			 */
			list_add_tail(&hpm->list, ret_hpm_list);
			nr_hpm++;
		}
	}
	return nr_hpm;
}

static unsigned int construct_page_list(struct list_head *hpm_list,
	get_migration_folio_t get_migration_folio, struct folio **folios)
{
	struct ham_page_map *hpm;
	struct folio *folio;
	unsigned int i = 0;

	if (!get_migration_folio) {
		return 0;
	}

	list_for_each_entry(hpm, hpm_list, list) {
		folio = get_migration_folio(hpm);
		if (!folio || !folio_try_get(folio))
			continue;

		folios[i++] = folio;
	}
	return i;
}

/*
 * handle_ham_migration - perform ham migration based on the given hpm_list.
 *
 * @hpm_list:       The list of ham_page_map to be migrated.
 * @nr_hpm_max:     Maximum length of hpm_list.
 * @get_migration_folio:  The function used to get the folio to be migrated
 *          from ham_page_map.
 * @get_new_folio:  The function used to allocate free folios to be used
 *          as the target of the folio migration.
 * @put_new_folio:  The function used to free target folios if migration
 *          fails, or NULL if no special handling is necessary.
 *
 * Return 0 on success, or a negative error code on failure.
 */
static int handle_ham_migration(struct list_head *hpm_list,
				unsigned int nr_hpm_max,
				get_migration_folio_t get_migration_folio,
				new_folio_t get_new_folio,
				free_folio_t put_new_folio)
{
	struct folio **folios;
	unsigned int nr_succeeded, nr_folios, nr_left;
	int ret;

	if (list_empty(hpm_list)) {
		pr_info("no pages need to be migrated\n");
		return 0;
	}

	folios = kmalloc_array(nr_hpm_max, sizeof(struct folio *), GFP_KERNEL);
	if (!folios) {
		pr_err("unable to allocate for folios array\n");
		return -ENOMEM;
	}

	/* Filter out the qualified folios and place them into an array. */
	nr_folios = construct_page_list(hpm_list, get_migration_folio, folios);
	if (nr_folios == 0) {
		pr_warn(" No qualified folios were found in the hpm_list.\n");
		kfree(folios);
		return 0;
	}

	/* Use the MIGRATE_SYNC migrate_mode, and retrying seems unnecessary. */
	ret = isolate_and_migrate_folios(folios, nr_folios, get_new_folio,
					 put_new_folio, (uintptr_t)hpm_list,
					 MIGRATE_SYNC, &nr_succeeded);
	nr_left = (nr_folios << (PMD_SHIFT - PAGE_SHIFT)) - nr_succeeded;
	if (nr_left || ret) {
		pr_info("isolate and migrate folios failed, ret: %d, nr_folios: %u, nr_successed: %u, nr_left: %u\n",
			ret, nr_folios, nr_succeeded, nr_left);
		kfree(folios);
		return -EPERM;
	}

	kfree(folios);
	return 0;
}

inline struct folio *get_folio_migrate_out(struct ham_page_map *hpm)
{
	return hpm->src_folio;
}

inline struct folio *get_folio_migrate_back(struct ham_page_map *hpm)
{
	return hpm->dst_folio;
}

struct folio *get_new_folio(struct folio *folio, unsigned long private)
{
	struct ham_page_map *hpm;
	struct list_head *hpm_list = (struct list_head *)private;

	list_for_each_entry(hpm, hpm_list, list) {
		if (hpm->is_migrate || !hpm->present)
			continue;

		if (hpm->src_folio == folio) {
			hpm->is_migrate = true;
			return hpm->dst_folio;
		}
	}
	return NULL;
}

void put_new_folio(struct folio *folio, unsigned long private)
{
	struct ham_page_map *hpm;
	struct list_head *hpm_list = (struct list_head *)private;

	list_for_each_entry(hpm, hpm_list, list) {
		if (!hpm->is_migrate || !hpm->present)
			continue;

		if (hpm->dst_folio == folio) {
			hpm->is_migrate = false;
			return;
		}
	}
	pr_warn("no matched folio to put\n");
}

struct folio *get_new_folio_rollBack(struct folio *folio, unsigned long private)
{
	struct ham_page_map *hpm;
	struct list_head *hpm_list = (struct list_head *)private;

	list_for_each_entry(hpm, hpm_list, list) {
		if (!hpm->is_migrate || !hpm->present)
			continue;

		if (hpm->dst_folio == folio) {
			return ham_alloc_huge_page_node(hpm->src_numa_id);
		}
	}
	return NULL;
}

static int ham_cache_clear(pid_t pid, struct ham_migrate_task *mig_task)
{
	unsigned int i;
	int ret = 0;

	if (global_cache_mnt) {
		return flush_cache_global(HISI_CACHE_MAINT_CLEANINVALID);
	}

	for (i = 0; i < mig_task->numa_cnt; i++) {
		ret = flush_cache_by_pa(mig_task->ram_maps[i].rmt_numa_start,
					mig_task->ram_maps[i].size,
					HISI_CACHE_MAINT_CLEANINVALID);
		if (ret) {
			pr_err("failed to flush cache, pid: %d\n", pid);
			return ret;
		}
	}
	return ret;
}

static int src_suspend_pgtable_maintain(struct ham_migrate_task *mig_task)
{
	int ret;
	unsigned int i;
	ktime_t t1, t2, t3;
	s64 elapsed_kernel, elapsed_task;
	struct ram_block_map *ram_map;

	for (i = 0; i < mig_task->numa_cnt; i++) {
		ram_map = &mig_task->ram_maps[i];

		t1 = ktime_get();
		ret = kernel_pgtable_within_pa_set_cacheable(
			ram_map->rmt_numa_start, ram_map->size, false);
		if (ret) {
			pr_err("kernel_pgtable_within_pa_set_cacheable failed, pid: %d, size: %zx, ret: %d",
			       mig_task->pid, ram_map->size, ret);
			return ret;
		}

		t2 = ktime_get();
		ret = task_pgtable_within_pid_set_cacheable(mig_task->pid,
							    ram_map->hva_start,
							    ram_map->size,
							    false);
		if (ret) {
			pr_err("task_pgtable_within_pid_set_cacheable failed, pid: %d, size: %zx, ret: %d\n",
			       mig_task->pid, ram_map->size, ret);
			return ret;
		}

		t3 = ktime_get();
		elapsed_kernel = ktime_to_us(ktime_sub(t2, t1));
		elapsed_task = ktime_to_us(ktime_sub(t3, t2));
		pr_info("[KERNEL_PGTABLE_MNT] elapsed time: %lld us\n"
			"[TASK_PGTABLE_MNT] elapsed time: %lld us\n", elapsed_kernel, elapsed_task);

		ram_map->cacheable = false;
	}
	return 0;
}

static int src_pgtable_maintain_rollback(struct ham_migrate_task *mig_task)
{
	unsigned int i;
	int ret = 0;
	struct ram_block_map *ram_map;

	for (i = 0; i < mig_task->numa_cnt; i++) {
		ram_map = &mig_task->ram_maps[i];
		if (ram_map->cacheable) {
			continue;
		}
		pr_info("page table maintain rollback, size: %zx\n",
			ram_map->size);
		ret = task_pgtable_within_pid_set_cacheable(
			mig_task->pid, ram_map->hva_start, ram_map->size, true);
		if (ret) {
			pr_warn("failed to set the page table attribute of relevant process\n");
		}
		ram_map->cacheable = true;
	}
	return ret;
}

static int dst_resume_pgtable_maintain(struct ham_migrate_task *mig_task)
{
	int ret;
	unsigned int i;
	struct ram_block_map *ram_map;

	for (i = 0; i < mig_task->numa_cnt; i++) {
		ram_map = &mig_task->ram_maps[i];

		/* Maintain task pgtable. From NC to CC. */
		ret = task_pgtable_within_pid_set_cacheable(
			mig_task->pid, ram_map->hva_start, ram_map->size, true);
		if (ret) {
			pr_err("task_pgtable_within_pid_set_cacheable failed, pid: %d, size: %zx, ret: %d\n",
			       mig_task->pid, ram_map->size, ret);
			return ret;
		}

		/* Matintain kernel pgtable. From invalid to valid. */
		ret = kernel_pgtable_within_pid_set_valid(
			mig_task->pid, ram_map->hva_start, ram_map->size, true);
		if (ret) {
			pr_err("kernel_pgtable_within_pid_set_valid failed, pid: %d, size: %zx, ret: %d",
			       mig_task->pid, ram_map->size, ret);
			return ret;
		}
	}
	return 0;
}

static void fill_freqs_to_rbm(struct ram_block_map *rbm,
			      struct freq_info *freq_info_array,
			      int freq_info_num)
{
	unsigned int i;
	int j;
	u64 hpa;
	for (i = 0; i < rbm->page_num; i++) {
		hpa = PFN_PHYS(folio_pfn(rbm->hpms[i].src_folio));
		for (j = 0; j < freq_info_num; j++) {
			if (freq_info_array[j].hpa == hpa) {
				rbm->hpms[i].freq = freq_info_array[j].freq;
			}
		}
	}
}

static int get_folios_freqs(struct ham_migrate_task *mig_task)
{
	unsigned int i;
	int ret;
	struct ram_block_map *map;
	struct freq_info *freq_info_array;
	u64 freq_info_num;
	ret = get_ham_pages_freqs(mig_task->pid, &freq_info_array,
				  &freq_info_num);
	if (ret) {
		pr_err("failed to get HAM page's frequency from SMAP, pid: %d\n",
		       mig_task->pid);
		return -EINVAL;
	}

	if (!freq_info_array) {
		pr_err("failed to get folio's frequency, pid: %d\n",
		       mig_task->pid);
		return -EINVAL;
	}
	map = mig_task->ram_maps;
	for (i = 0; i < mig_task->numa_cnt; i++) {
		fill_freqs_to_rbm(&map[i], freq_info_array, freq_info_num);
	}
	vfree(freq_info_array);
	return 0;
}

static inline int ham_modify_pgtable(struct ham_migrate_task *mig_task,
				     bool cacheable)
{
	return cacheable ? dst_resume_pgtable_maintain(mig_task) :
			   src_suspend_pgtable_maintain(mig_task);
}

static inline bool pid_legally(pid_t pid)
{
	return current->tgid == pid;
}

static int check_rmt_numa_info(struct ram_block_info *rbi)
{
	struct pglist_data *pgdata;

	if (rbi->rmt_numa_id == NUMA_NO_NODE) {
		return 0;
	}

	if (rbi->rmt_numa_id >= MAX_NUMNODES ||
	    !numa_is_remote_node(rbi->rmt_numa_id)) {
		pr_err("node: %u is not a remote NUMA node\n",
		       rbi->rmt_numa_id);
		return -EINVAL;
	}

	pgdata = NODE_DATA(rbi->rmt_numa_id);
	if (pgdata->node_present_pages != pgdata->node_spanned_pages ||
	    pgdata->node_present_pages << PAGE_SHIFT != rbi->size) {
		pr_err("invalid NUMA node: %d\n", rbi->rmt_numa_id);
		return -EINVAL;
	}
	return 0;
}

static int check_migration_param(struct migration_param param,
				 struct hstate **h)
{
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	struct ram_block_info *rbi;
	unsigned int i;
	int ret = -EINVAL;
	unsigned long sum = 0;

	if (!pid_legally(param.pid)) {
		pr_err("invalid pid: %d\n", param.pid);
		return ret;
	}
	if (param.cnt > BATCH_NUM || param.cnt == 0) {
		pr_err("invalid count: %d\n", param.cnt);
		return ret;
	}

	mm = find_get_mm_by_vpid(param.pid);
	if (!mm) {
		pr_err("failed to get mm_struct of pid: %d\n", param.pid);
		return ret;
	}
	for (i = 0; i < param.cnt; i++) {
		rbi = &param.ram_blocks[i];
		vma = find_vma(mm, rbi->hva_start);
		if (!vma || rbi->hva_start != vma->vm_start || rbi->size == 0 ||
		    rbi->size % PMD_SIZE != 0 ||
		    rbi->size != vma->vm_end - vma->vm_start) {
			pr_err("invalid ramblock info of pid: %d\n", param.pid);
			goto exit_with_mmput;
		}

		*h = hstate_vma(vma);
		if (!(*h) || huge_page_size(*h) != PMD_SIZE) {
			pr_err("unable to find hugepage of relevant VMA of pid: %d\n",
			       param.pid);
			goto exit_with_mmput;
		}

		if ((rbi->size >> PMD_SHIFT) > MAX_HUGEPAGE_NUM - sum) {
			pr_err("the number of hugepages exceeds the maximum limit: %u\n",
			       MAX_HUGEPAGE_NUM);
			return -EINVAL;
		}
		sum += (rbi->size >> PMD_SHIFT);

		if (check_rmt_numa_info(rbi)) {
			goto exit_with_mmput;
		}
	}
	ret = 0;

exit_with_mmput:
	mmput(mm);
	return ret;
}

static long ioctl_start_migration(unsigned long arg)
{
	struct ham_migrate_task *mig_task;
	struct migration_param param;
	unsigned long huge_page_num;
	unsigned int i;
	int ret;
	struct hstate *h;
	struct ram_block_info *rbi;

	if (copy_from_user(&param, (void __user *)arg, sizeof(param))) {
		pr_err("failed to copy migrate params from user space\n");
		return -EFAULT;
	}

	if (check_migration_param(param, &h)) {
		pr_err("invalid params passed to start migration\n");
		return -EINVAL;
	}

	mig_task = init_migrate_task(&param, h);
	if (!mig_task) {
		pr_err("failed to init migrate task of pid: %d\n", param.pid);
		return -ENOMEM;
	}

	for (i = 0; i < param.cnt; i++) {
		if (param.ram_blocks[i].rmt_numa_id == NUMA_NO_NODE) {
			goto create_map;
		}
	}

	mig_task->status |= HAM_TASK_ALLOW_MIGR;
	for (i = 0; i < param.cnt; i++) {
		rbi = &param.ram_blocks[i];
		huge_page_num = rbi->size / PAGE_SIZE_2M;
		ret = config_system_huge_page(huge_page_num, rbi->rmt_numa_id,
					      mig_task->hstate);
		if (ret < 0) {
			pr_err("failed to configure hugepage number, pid = %d, block_cnt = %d, numa_id = %d, nr_hugepages = %lu, "
			       "ret = %d\n",
			       mig_task->pid, i, rbi->rmt_numa_id,
			       huge_page_num, ret);
			goto out_err;
		}
	}

create_map:
	ret = create_numa_map(mig_task, &param);
	if (ret) {
		pr_err("failed to create NUMA map, pid: %d\n", mig_task->pid);
		goto out_err;
	}

	put_migrate_task(mig_task, HAM_TASK_INITED);
	return 0;
out_err:
	release_migrate_task(mig_task);
	pr_err("ioctl start migration err: %d\n", ret);
	return ret;
}

static long ioctl_migration_pages(unsigned long arg)
{
	struct ham_migrate_task *mig_task;
	LIST_HEAD(hpm_list);
	unsigned int nr_hpm;
	int ret;
	pid_t pid;

	if (copy_from_user(&pid, (void __user *)arg, sizeof(pid_t))) {
		pr_err("failed to copy pages to migrate from user space\n");
		return -EFAULT;
	}

	mig_task = get_migrate_task(pid);
	if (!mig_task) {
		pr_err("failed to search migration task, pid: %d\n", pid);
		return -EINVAL;
	}

	ret = check_task_status(mig_task, HAM_TASK_INITED | HAM_TASK_MIGRATED |
						  HAM_TASK_ALLOW_MIGR);
	if (ret) {
		pr_err("pre task status is not satisfied, migration is not allowed\n");
		return ret;
	}

	ret = fill_task_pages(mig_task);
	if (ret) {
		pr_err("failed to fill migration pages, pid: %d\n", pid);
		goto out;
	}

	nr_hpm = queue_qualified_pages(mig_task, false, &hpm_list);
	pr_info("%u pages need to be migrated\n", nr_hpm);
	if (nr_hpm == 0) {
		goto out;
	}

	ret = get_folios_freqs(mig_task);
	if (!ret) {
		sort_hpm_list(&hpm_list, nr_hpm);
	}

	ret = handle_ham_migration(&hpm_list, nr_hpm, get_folio_migrate_out,
				   get_new_folio, put_new_folio);
	if (ret) {
		pr_err("migration failed, pid: %d\n", pid);
		goto out;
	}

	pr_info("migration successed\n");
out:
	put_migrate_task(mig_task, HAM_TASK_MIGRATED);
	return ret;
}

static long ioctl_rollback_pages(unsigned long arg)
{
	struct ham_migrate_task *mig_task;
	LIST_HEAD(hpm_list);
	unsigned int nr_hpm;
	int ret;
	pid_t pid;

	if (copy_from_user(&pid, (void __user *)arg, sizeof(pid_t))) {
		pr_err("failed to copy pages to migrate from user space\n");
		return -EFAULT;
	}
	pr_info("start to rollback pages, pid: %d\n", pid);

	mig_task = get_migrate_task(pid);
	if (!mig_task) {
		pr_err("failed to search migration task, pid: %d\n", pid);
		return -EINVAL;
	}

	ret = check_task_status(mig_task, HAM_TASK_MIGRATED);
	if (ret) {
		pr_warn("nothing need to be rollback\n");
		goto out;
	}

	mig_task->is_finish = false;

	if (!find_vpid(pid)) {
		pr_err("unable to find pid: %d", pid);
		ret = -EFAULT;
		goto out;
	}

	/* Page table maintenance failure should not block rollback */
	ret = src_pgtable_maintain_rollback(mig_task);
	if (ret) {
		pr_warn("src page table maintain rollback abnormally\n");
	}

	nr_hpm = queue_qualified_pages(mig_task, true, &hpm_list);
	pr_info("%u pages need to be rollback\n", nr_hpm);
	if (nr_hpm == 0) {
		ret = 0;
		goto out;
	}

	ret = handle_ham_migration(&hpm_list, nr_hpm, get_folio_migrate_back,
				   get_new_folio_rollBack, NULL);
	if (ret) {
		pr_err("failed to rollback pages, pid: %d\n", mig_task->pid);
	}

out:
	release_migrate_task(mig_task);
	return ret;
}

static long ioctl_stop_migration(unsigned long arg)
{
	struct ham_migrate_task *mig_task;
	pid_t pid;
	int ret;

	if (copy_from_user(&pid, (void __user *)arg, sizeof(pid_t))) {
		pr_err("failed to copy pid to stop migrate task from user space\n");
		return -EFAULT;
	}

	mig_task = get_migrate_task(pid);
	if (!mig_task) {
		pr_err("failed to search migration task, pid: %d\n", pid);
		return -EINVAL;
	}

	ret = check_task_status(mig_task, HAM_TASK_INITED | HAM_TASK_MIGRATED);
	if (ret) {
		pr_err("pre task status is not satisfied, stop migration is not allowed\n");
		return ret;
	}

	mig_task->finish_times = FINISH_LENGTH;
	mig_task->is_finish = true;

	pr_info("stop migration successfully\n");
	put_migrate_task(mig_task, HAM_TASK_STOPPED);
	return 0;
}

static long ioctl_modify_pagetable(unsigned long arg)
{
	maintain_info mt_info;
	int ret;
	struct ham_migrate_task *mig_task;

	if (copy_from_user(&mt_info, (void __user *)arg,
			   sizeof(maintain_info))) {
		pr_err("failed to copy maintain info from user space\n");
		return -EFAULT;
	}

	mig_task = get_migrate_task(mt_info.pid);
	if (!mig_task) {
		pr_err("failed to search migration task, pid: %d\n",
		       mt_info.pid);
		return -EINVAL;
	}

	ret = check_task_status(mig_task, HAM_TASK_INITED);
	if (ret) {
		pr_err("pre task status is not satisfied, page table modification is not allowed\n");
		return ret;
	}

	pr_info("start to modify page, pid: %d, cacheable: %d\n", mt_info.pid,
		mt_info.cacheable);
	ret = ham_modify_pgtable(mig_task, mt_info.cacheable);
	if (ret) {
		pr_err("failed to modify page table, pid: %d\n", mt_info.pid);
	} else {
		pr_info("modify page table successfully, pid: %d\n",
			mt_info.pid);
	}

	put_migrate_task(mig_task, mig_task->status);
	return ret;
}

static long ioctl_cache_clear(unsigned long arg)
{
	pid_t pid;
	int ret;
	ktime_t t1, t2;
	s64 elapsed_cache;
	struct ham_migrate_task *mig_task;

	pr_info("start to clear cache\n");
	if (copy_from_user(&pid, (void __user *)arg, sizeof(pid_t))) {
		pr_err("failed to copy pid to clear cache from user space\n");
		return -EFAULT;
	}

	mig_task = get_migrate_task(pid);
	if (!mig_task) {
		pr_err("failed to search migration task, pid: %d\n", pid);
		return -EINVAL;
	}

	ret = check_task_status(mig_task, HAM_TASK_MIGRATED);
	if (ret) {
		pr_err("pre task status is not satisfied, cache clear is not allowed\n");
		return ret;
	}

	t1 = ktime_get();
	ret = ham_cache_clear(pid, mig_task);
	if (ret) {
		pr_err("failed to clear cache, pid: %d\n", pid);
		return ret;
	}
	t2 = ktime_get();
	elapsed_cache = ktime_to_us(ktime_sub(t2, t1));
	pr_info("[CACHE_MNT] elapsed time: %lld us\n", elapsed_cache);

	put_migrate_task(mig_task, mig_task->status);
	return 0;
}

static int ham_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int ham_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t ham_read(struct file *file, char __user *buf, size_t size,
			loff_t *ppos)
{
	return 0;
}

static ssize_t ham_write(struct file *file, const char __user *buf,
			 size_t count, loff_t *ppos)
{
	return 0;
}

static long ham_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case HAM_START_MIGRATION:
		return ioctl_start_migration(arg);
	case HAM_MIGRATE_PAGES:
		return ioctl_migration_pages(arg);
	case HAM_ROLLBACK_PAGES:
		return ioctl_rollback_pages(arg);
	case HAM_STOP_MIGRATION:
		return ioctl_stop_migration(arg);
	case HAM_MODIFY_PAGETABLE:
		return ioctl_modify_pagetable(arg);
	case HAM_CACHE_CLEAR:
		return ioctl_cache_clear(arg);
	default:
		return -EINVAL;
	}
}

static const struct file_operations g_ham_fops = {
	.owner = THIS_MODULE,
	.open = ham_open,
	.release = ham_release,
	.read = ham_read,
	.write = ham_write,
	.unlocked_ioctl = ham_ioctl,
};

static ssize_t global_cache_mnt_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", global_cache_mnt);
}

static ssize_t global_cache_mnt_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int val;
	if (kstrtoint(buf, DECIMAL, &val) == 0)
		global_cache_mnt = val ? true : false;
	return count;
}

static DEVICE_ATTR(global_cache_mnt, 0664, global_cache_mnt_show,
			global_cache_mnt_store);

int ham_init(void)
{
	dev_t dev_id;
	int ret;

	ret = alloc_chrdev_region(&dev_id, 0, HAM_DEV_COUNT, "ham_migrate");
	if (ret < 0) {
		pr_err("unable to allocate character device region\n");
		return ret;
	}
	g_ham_dev = MKDEV(MAJOR(dev_id), 0);
	cdev_init(&g_ham_cdev, &g_ham_fops);
	ret = cdev_add(&g_ham_cdev, g_ham_dev, HAM_DEV_COUNT);
	if (ret < 0) {
		pr_err("failed to init HAM character device\n");
		goto cdev_add_err;
	}

	g_ham_class = class_create("ham_migrate");
	if (IS_ERR(g_ham_class)) {
		ret = PTR_ERR(g_ham_class);
		pr_err("failed to init HAM class\n");
		goto class_create_err;
	}

	g_ham_device = device_create(g_ham_class, NULL, g_ham_dev, NULL,
				   "ham_migrate");
	if (IS_ERR(g_ham_device)) {
		ret = -EBUSY;
		pr_err("failed to init HAM device\n");
		goto dev_create_err;
	}

	ret = device_create_file(g_ham_device, &dev_attr_global_cache_mnt);
	if (ret)
		goto dev_attr_err;

	ret = init_global_task_list();
	if (ret)
		goto init_task_err;

	pr_info("Module loaded\n");
	return 0;

init_task_err:
	device_remove_file(g_ham_device, &dev_attr_global_cache_mnt);
dev_attr_err:
	device_destroy(g_ham_class, g_ham_dev);
dev_create_err:
	class_destroy(g_ham_class);
class_create_err:
	cdev_del(&g_ham_cdev);
cdev_add_err:
	unregister_chrdev_region(g_ham_dev, HAM_DEV_COUNT);
	return ret;
}

void ham_exit(void)
{
	release_all_tasks();
	cancel_delayed_work_sync(&ham_work);
	flush_workqueue(ham_wq);
	destroy_workqueue(ham_wq);
	device_remove_file(g_ham_device, &dev_attr_global_cache_mnt);
	device_destroy(g_ham_class, g_ham_dev);
	class_destroy(g_ham_class);
	cdev_del(&g_ham_cdev);
	unregister_chrdev_region(g_ham_dev, HAM_DEV_COUNT);
	pr_info("Module unloaded\n");
}