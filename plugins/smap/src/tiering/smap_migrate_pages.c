// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * Description: SMAP Tiering Memory Solution: SMAP MIGRATE PAGES
 */

#include <linux/migrate.h>
#include <linux/mm_inline.h>
#include <linux/rmap.h>
#include <linux/kthread.h>
#include <linux/ktime.h>
#include <linux/gfp.h>
#include <linux/cpumask.h>
#include <linux/page-isolation.h>
#include <linux/limits.h>

#include "numa.h"
#include "rmap.h"
#include "acpi_mem.h"
#include "dump_info.h"
#include "iomem.h"
#include "smap_migrate_wrapper.h"
#include "common.h"
#include "smap_migrate_pages.h"

#define PHYSICAL_ADDR 80
#define MAX_PROMOTE_NUM 51200
#define MAX_ISOLATE_RANGE_RETRY_TIME 5
#define MAX_MIGRATE_NUMA_RETRY_TIME 10
#define RAM_ADDR_START 0x2080000000
#define SOCKET2_START_ADDR 0x202000000000
#define ACTC_PAGE_SIZE (2 * 1024 * 1024)
#define ACTC_START_INDEX 1024

#define MAX_MIGRATE_NUM 1048576
#define MIGRATE_SUCCESS_STATE (-1)
#define MIGRATE_FAILED_STATE (-10)

#define NR_PAGE_LOW_WMARK 32
#define MAX_PID_TASK_NUM 4

#define INVALID_PADDR 0

#define MIGRATE_PAGES_DMA_OFFLOADING 10

#undef pr_fmt
#define pr_fmt(fmt) "SMAP_migrate: " fmt

struct num_list {
	struct hlist_head hlist_head;
};

struct num_node {
	int num;
	u64 page_phy_addr;
	struct hlist_node hlist_node;
};

#define THREAD_PREFIX "smap_migrate_"

extern u32 g_pagesize_huge;

struct multi_migrate_struct {
	ktime_t start_time;
	ktime_t end_time;
	unsigned int nr_folios;
	unsigned int failed_num;
	int to_node;
	struct completion comp;
	char thread_name[20];
	bool init_flag;
	struct folio **folios;
	struct task_struct *ts;
};
static struct multi_migrate_struct mig[MAX_NR_MIGRATE_THREADS];

static struct migrate_node {
	int next_nid;
	unsigned long nr[SMAP_MAX_NUMNODES];
} migrate_node;

static int smap_check_huge_page_for_migration(struct page *page, pid_t pid)
{
	struct page_task_arg pta = { 0 };
	if (!PageHead(page))
		return -1;
	if (pid) {
		pta.type = PAGE_PID_TYPE;
		pta.pid = pid;
		find_page_task(page, 0, &pta);
		if (!pta.found) {
			pr_debug("wrong pid %d\n", pid);
			return -EINVAL;
		}
	}
	return 0;
}

static int smap_add_page_for_migration(struct page *page, struct folio **folios,
				       unsigned int *nr_folios, pid_t pid,
				       bool migrate_all)
{
	int err;
	err = PTR_ERR(page);
	if (IS_ERR(page)) {
		pr_debug("invalid page passed to add page for migration\n");
		return err;
	}

	err = -ENOENT;
	if (!page) {
		pr_debug("null page passed to add page for migration\n");
		return err;
	}

	err = -EACCES;
	if (page_mapcount(page) > 1 && !migrate_all) {
		pr_debug("invalid page map count or null migrate all flag\n");
		return err;
	}
	if (!folio_try_get(page_folio(page))) {
		pr_debug("failed to add folio reference\n");
		return -EINVAL;
	}
	if (PageHuge(page)) {
		err = smap_check_huge_page_for_migration(page, pid);
		if (err) {
			folio_put(page_folio(page));
			return err;
		}
	}

	folios[*nr_folios] = page_folio(page);
	(*nr_folios)++;
	return 0;
}

struct migration_target_control {
	int nid; /* preferred node id */
	nodemask_t *nmask;
	gfp_t gfp_mask;
};

static int thread_fn(void *data)
{
	int ret;
	unsigned int nr_succeeded = 0;
	struct multi_migrate_struct *ms = data;
	ms->start_time = ktime_get();
	ms->end_time = 0;
	ktime_t mig_time;
	ret = isolate_and_migrate_folios(ms->folios, ms->nr_folios,
					 smap_alloc_new_node_page, NULL,
					 ms->to_node, MIGRATE_ASYNC,
					 &nr_succeeded);
	if (ret) {
		pr_err("failed to migrate pages, ret: %d\n", ret);
	}
	if (smap_pgsize == HUGE_PAGE) {
		nr_succeeded >>= (__builtin_ctz(g_pagesize_huge) - PAGE_SHIFT);
	}
	ms->end_time = ktime_get();
	mig_time = ktime_to_us(ktime_sub(ms->end_time, ms->start_time));
	if (ms->to_node >= nr_local_numa)
		pr_debug(
			"time spend: %lldus, to_node: %d, nr_folios: %u, nr_succeeded: %u\n",
			mig_time, ms->to_node, ms->nr_folios, nr_succeeded);
	ms->failed_num = ms->nr_folios - nr_succeeded;
	vfree(ms->folios);
	complete(&ms->comp);
	return ret;
}

static inline void clear_mig_folios(unsigned int clear_idx)
{
	unsigned int i;
	for (i = 0; i < clear_idx; i++) {
		vfree(mig[i].folios);
		mig[i].folios = NULL;
	}
}

static void cal_thread_time(ktime_t *start_time, ktime_t *end_time,
			    ktime_t thread_stime, ktime_t thread_etime)
{
	if (thread_etime > *end_time)
		*end_time = thread_etime;
	if (thread_stime < *start_time)
		*start_time = thread_stime;
}

static int init_mig(unsigned int nr_threads, unsigned int nr_folios,
		    int to_node)
{
	unsigned int i;
	unsigned int avg_cnt;

	avg_cnt = nr_folios / nr_threads + 1;
	for (i = 0; i < nr_threads; i++) {
		mig[i].nr_folios = 0;
		mig[i].failed_num = 0;
		mig[i].folios = vzalloc(avg_cnt * sizeof(struct folio *));
		if (!mig[i].folios) {
			clear_mig_folios(i);
			return -ENOMEM;
		}
		init_completion(&mig[i].comp);
		mig[i].init_flag = true;
		mig[i].to_node = to_node;
		if (sprintf(mig[i].thread_name, "%s%u", THREAD_PREFIX, i) < 0)
			pr_debug("sprintf failed: thread smap_migrate_%u ", i);
	}
	return 0;
}

static void put_folios(struct folio **folios, unsigned int nr_folios)
{
	unsigned int i;
	for (i = 0; i < nr_folios; i++) {
		folio_put(folios[i]);
	}
}

int migrate_multi_threaded(unsigned int nr_threads, struct folio **folios,
			   unsigned int nr_folios, int to_node)
{
	unsigned int i;
	int ret;
	ktime_t start_time = KTIME_MAX;
	ktime_t end_time = 0;

	if (nr_threads == 0 || nr_threads > MAX_NR_MIGRATE_THREADS) {
		put_folios(folios, nr_folios);
		return -EINVAL;
	}

	ret = init_mig(nr_threads, nr_folios, to_node);
	if (ret) {
		put_folios(folios, nr_folios);
		return ret;
	}

	for (i = 0; i < nr_folios; i++) {
		mig[i % nr_threads].folios[mig[i % nr_threads].nr_folios++] =
			folios[i];
	}

	for (i = 0; i < nr_threads; i++) {
		mig[i].ts = kthread_run(thread_fn, &mig[i], mig[i].thread_name);
		if (IS_ERR(mig[i].ts)) {
			complete(&mig[i].comp);
			put_folios(mig[i].folios, mig[i].nr_folios);
			pr_err("failed to create thread %u, ret: %ld\n", i,
			       PTR_ERR(mig[i].ts));
			vfree(mig[i].folios);
			continue;
		}
	}

	for (i = 0; i < nr_threads; i++) {
		wait_for_completion(&mig[i].comp);
		cal_thread_time(&start_time, &end_time, mig[i].start_time,
				mig[i].end_time);
	}

	if (to_node >= nr_local_numa)
		pr_debug("migration time spend: %lldus, nr_folios: %u\n",
			 ktime_to_us(ktime_sub(end_time, start_time)),
			 nr_folios);

	return 0;
}

static int smap_isolate_and_migrate_folios(struct folio **folios, unsigned int nr_folios,
    new_folio_t get_new_folio, free_folio_t put_new_folio,
    unsigned long private, enum migrate_mode mode, unsigned int *nr_succeeded)
{
	int ret = 0;
	unsigned int i;
	struct folio *folio;
	LIST_HEAD(source);

	for (i = 0; i < nr_folios; i++) {
		folio = folios[i];
		if (!folio_test_hugetlb(folio)) {
			VM_BUG_ON_FOLIO(!folio_ref_count(folio), folio);
		}
		fp_isolate_folio_to_list(folio, &source);
		if (!folio_test_hugetlb(folio)) {
			folio_put(folio);
		}
	}
	if (!list_empty(&source)) {
		ret = fp_migrate_pages(&source, get_new_folio, put_new_folio,
			private, mode, MR_HOTNESS, nr_succeeded);
			if (ret)
				fp_putback_movable_pages(&source);
			if (nr_succeeded && *nr_succeeded)
				count_vm_numa_events(NUMA_PAGE_MIGRATE, *nr_succeeded);
	}

	return ret;
}

unsigned int smap_migrate(struct folio **folios, unsigned int nr_folios,
			  int to_node, enum smap_migrate_type type)
{
	int err = 0;
	unsigned int nr_succeeded = 0;
	ktime_t start_time = 0;
	ktime_t mig_time = 0;
	if (nr_folios == 0 || !folios) {
		pr_debug("no folio to migrate\n");
		return nr_folios;
	}

	if (to_node < 0 || to_node >= SMAP_MAX_NUMNODES) {
		put_folios(folios, nr_folios);
		pr_debug("invalid destination node: %d passed to SMAP migrate\n",
			 to_node);
		return nr_folios;
	}
	start_time = ktime_get();
	if (MIGRATE_TYPE_BACK == type) {
		err = isolate_and_migrate_folios(
			folios, nr_folios, smap_alloc_new_node_page_mig_back,
			NULL, to_node, MIGRATE_ASYNC, &nr_succeeded);
		if (err) {
			pr_err("failed to migrate back, ret: %d\n", err);
		}
	} else if (MIGRATE_TYPE_HOTNESS == type) {
		err = isolate_and_migrate_folios(folios, nr_folios,
						 smap_alloc_new_node_page, NULL,
						 to_node, MIGRATE_ASYNC,
						 &nr_succeeded);
		if (err) {
			pr_err("failed to migrate, ret: %d\n", err);
		}
	} else if (MIGRATE_TYPE_REMOTE == type) {
		err = smap_isolate_and_migrate_folios(folios, nr_folios,
						 smap_alloc_new_node_page, NULL,
						 to_node, MIGRATE_PAGES_DMA_OFFLOADING,
						 &nr_succeeded);
		if (err > 0)
			pr_warn("isolat or migrate remote range err: %d\n", err);
		else if (err < 0)
			pr_err("failed to migrate folios, err: %d\n", err);
	}
	if (smap_pgsize == HUGE_PAGE) {
		nr_succeeded >>= (__builtin_ctz(g_pagesize_huge) - PAGE_SHIFT);
	}
	mig_time = calc_time_us(start_time);
	pr_debug(
		"migration time spend: %lldus, nr_folios: %u, nr_succeeded: %d\n",
		mig_time, nr_folios, nr_succeeded);
	if (err == 0 && nr_succeeded == 0 && smap_pgsize == NORMAL_PAGE) {
		if (folio_try_get(folios[0])) {
			shake_page(&folios[0]->page);
			folio_put(folios[0]);
		}
	}
	return nr_folios - nr_succeeded;
}

static inline void refresh_nodes_nr_free(void)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(migrate_node.nr); i++) {
		if (i < nr_local_numa) {
			migrate_node.nr[i] = get_node_nr_free_pages(i);
		} else {
			migrate_node.nr[i] = 0;
		}
	}
}

/*
 * find_node_to_migrate_rr - round-robin to find node to migrate
 *
 * @nid:	The node id we want to migrate, -1 means no such node.
 * @out_nid:	The node id we found
 *
 * Returns 0 if we found, or an error code.
 */
static int find_node_to_migrate_rr(int nid, int *out_nid)
{
	int i, temp_nid;

	if (nid >= 0) {
		if (nid >= nr_local_numa) {
			return -EINVAL;
		}
		if (migrate_node.nr[nid] > NR_PAGE_LOW_WMARK) {
			*out_nid = nid;
			return 0;
		}
	}
	i = temp_nid = migrate_node.next_nid;
	migrate_node.next_nid = (migrate_node.next_nid + 1) % nr_local_numa;
	do {
		if (migrate_node.nr[i] > NR_PAGE_LOW_WMARK) {
			*out_nid = i;
			return 0;
		}
		i = (i + 1) % nr_local_numa;
	} while (i != temp_nid);
	return -ENOMEM;
}

static int smap_add_page_for_migrate_back(u64 pa,
					  struct folio ***migrate_folios,
					  unsigned int *mig_pages_cnt,
					  int dest_nid, bool migrate_all)
{
	int ret;
	int nid = NUMA_NO_NODE;
	unsigned long pfn;
	struct page *page;
	struct page_task_arg pta = { 0 };

	pfn = PHYS_PFN(pa);
	if (!pfn_valid(pfn)) {
		return -ENXIO;
	}
	page = pfn_to_online_page(pfn);
	if (!page) {
		return -EIO;
	}

	if (IS_ERR(page)) {
		return PTR_ERR(page);
	}

	if (page_mapcount(page) > 1 && !migrate_all) {
		return -EACCES;
	}
	ret = 0;
	pta.type = PAGE_NODE_TYPE;
	if (PageHuge(page)) {
		if (PageHead(page)) {
			find_page_task(page, 0, &pta);
			if (pta.found &&
			    pta.nr_cpus_allowed < num_online_cpus()) {
				nid = pta.node;
			} else {
				ret = find_node_to_migrate_rr(dest_nid, &nid);
			}
		}
	} else {
		struct page *head = compound_head(page);
		if (__folio_test_movable(page_folio(head))) {
			return -EINVAL;
		}

		find_page_task(head, 0, &pta);
		if (pta.found && pta.nr_cpus_allowed < num_online_cpus()) {
			nid = pta.node;
		} else {
			ret = find_node_to_migrate_rr(dest_nid, &nid);
		}
	}
	if (ret)
		return ret;

	if (nid < 0 || nid >= nr_local_numa) {
		pr_err("invalid local NUMA node: %d passed to add pages for migrate back\n",
		       nid);
		return -EINVAL;
	}
	if (!folio_try_get(page_folio(page))) {
		pr_debug("unable to add folio reference\n");
		return -EINVAL;
	}
	migrate_folios[nid][mig_pages_cnt[nid]] = page_folio(page);
	mig_pages_cnt[nid]++;

	return 0;
}

static bool check_addr_range_valid(struct migrate_back_subtask *task)
{
	__u64 pa;
	unsigned long tmp_pfn;
	struct page *tmp_page;

	for (pa = task->pa_start; pa < task->pa_end; pa += g_pagesize_huge) {
		tmp_pfn = PHYS_PFN(pa);
		if (!pfn_valid(tmp_pfn)) {
			return false;
		}
		tmp_page = pfn_to_online_page(tmp_pfn);
		if (!tmp_page) {
			return false;
		}
		if (get_pageblock_migratetype(tmp_page) == MIGRATE_ISOLATE)
			return false;
	}
	return true;
}

void smap_handle_migrate_back_subtask(struct migrate_back_subtask *task)
{
	int ret;
	unsigned int nr_pre_migrate_fail, nr_migrate_fail;
	__u64 pa;
	unsigned long pfn;
	struct page *page;
	unsigned int page_size = is_smap_pg_huge() ? g_pagesize_huge :
						     PAGE_SIZE;
#ifdef DEBUG
	ktime_t start_time, end_time;
	s64 delta_time_ms;
#endif

	if (!check_addr_range_valid(task)) {
		pr_err("MIGRATE_ISOLATE pages in range\n");
		task->status = MB_SUBTASK_ERR;
		task->isolated_flag = true;
		return;
	}
	task->isolated_flag = false;

	refresh_nodes_nr_free();

	nr_pre_migrate_fail = nr_migrate_fail = 0;
	unsigned int nr_folios = 0;
	unsigned long max_nr_folios =
		(task->pa_end - task->pa_start + 1) / page_size;
	struct folio **migrate_folios =
		vzalloc(max_nr_folios * sizeof(struct folio *));
	if (!migrate_folios) {
		task->status = MB_SUBTASK_ERR;
		return;
	}

	for (pa = task->pa_start; pa < task->pa_end; pa += page_size) {
		pfn = PHYS_PFN(pa);
		if (!pfn_valid(pfn)) {
			continue;
		}
		page = pfn_to_online_page(pfn);
		if (!page)
			continue;
		if (!is_smap_pg_huge()) {
			if (!(!__folio_test_movable(page_folio(page)) &&
			      page_ref_count(page) != 0)) {
				continue;
			}
			if (PageTransHuge(page) || PageHuge(page)) {
				continue;
			}
		}
		if (is_migrate_isolate_page(page)) {
			continue;
		}
		if (is_smap_pg_huge() && PageHuge(page) && !PageHead(page)) {
			continue;
		}
		ret = smap_add_page_for_migration(
			page, migrate_folios, &nr_folios, 0, MPOL_MF_MOVE_ALL);
		if (ret) {
			nr_pre_migrate_fail++;
		}
	}
	if (nr_folios == 0) {
		task->status = MB_SUBTASK_DONE;
		vfree(migrate_folios);
		return;
	}

#ifdef DEBUG
	start_time = ktime_get();
#endif
	nr_migrate_fail =
		smap_migrate(migrate_folios, nr_folios, task->src_nid, MIGRATE_TYPE_BACK);

#ifdef DEBUG
	end_time = ktime_get();
	delta_time_ms = ktime_to_ms(ktime_sub(end_time, start_time));
	pr_debug("migrate back total %lu pages, use %lldms\n", nr_folios,
		 delta_time_ms);
#endif
	vfree(migrate_folios);
	if (nr_migrate_fail) {
		task->status = MB_SUBTASK_ERR;
		pr_err("migrate to node%d failed %d pages\n", task->src_nid,
		       nr_migrate_fail);
	}
	task->status = (nr_migrate_fail ||
			(!is_smap_pg_huge() && nr_pre_migrate_fail)) ?
			       MB_SUBTASK_ERR :
			       MB_SUBTASK_DONE;
}

static void process_pages_for_migration(struct migrate_back_subtask *task,
					struct folio ***migrate_folios,
					unsigned int *mig_pages_cnt,
					unsigned long *nr_pre_migrate_fail,
					unsigned long *nr_pre_migrate)
{
	int ret;
	__u64 pa;
	unsigned long pfn;
	struct page *page;
	*nr_pre_migrate_fail = *nr_pre_migrate = 0;

	for (pa = task->pa_start; pa < task->pa_end; pa += PAGE_SIZE) {
		pfn = PHYS_PFN(pa);
		if (!pfn_valid(pfn)) {
			continue;
		}
		page = pfn_to_online_page(pfn);
		if (!page) {
			continue;
		}
		if (__folio_test_movable(page_folio(page)) ||
		    page_ref_count(page) == 0 || PageTransHuge(page) ||
		    PageHuge(page)) {
			continue;
		}
		if (is_migrate_isolate_page(page)) {
			continue;
		}
		ret = smap_add_page_for_migrate_back(pa, migrate_folios,
						     mig_pages_cnt,
						     task->dest_nid,
						     MPOL_MF_MOVE_ALL);
		if (ret) {
			(*nr_pre_migrate_fail)++;
		} else {
			(*nr_pre_migrate)++;
		}
	}
}

void smap_handle_migrate_back_subtask_4k(struct migrate_back_subtask *task)
{
	int i, j;
	unsigned int nr_migrate_fail, nr_fail;
	unsigned int mig_pages_cnt[SMAP_MAX_LOCAL_NUMNODES] = { 0 };
	struct folio **migrate_folios[SMAP_MAX_LOCAL_NUMNODES] = { NULL };
	unsigned long nr_pre_migrate_fail;
	unsigned long max_nr_folios =
		(task->pa_end - task->pa_start) / PAGE_SIZE;
	unsigned long nr_pre_migrate = 0;
#ifdef DEBUG
	ktime_t start_time, end_time;
	s64 delta_time_ms;
#endif
	for (i = 0; i < SMAP_MAX_LOCAL_NUMNODES; i++) {
		migrate_folios[i] =
			vzalloc(max_nr_folios * sizeof(struct folio *));
		if (!migrate_folios[i]) {
			for (j = 0; j < i; j++) {
				vfree(migrate_folios[j]);
			}
			task->status = MB_SUBTASK_ERR;
			pr_err("unable to allocate memory for migrate folio list\n");
			return;
		}
	}
	refresh_nodes_nr_free();
	nr_pre_migrate_fail = nr_migrate_fail = 0;
	process_pages_for_migration(task, migrate_folios, mig_pages_cnt,
				    &nr_pre_migrate_fail, &nr_pre_migrate);
	for (i = 0; i < SMAP_MAX_LOCAL_NUMNODES; i++) {
		if (mig_pages_cnt[i] == 0) {
			vfree(migrate_folios[i]);
			continue;
		}

#ifdef DEBUG
		start_time = ktime_get();
#endif
		nr_fail = smap_migrate(migrate_folios[i], mig_pages_cnt[i], i,
				       MIGRATE_TYPE_BACK);
#ifdef DEBUG
		end_time = ktime_get();
		delta_time_ms = ktime_to_ms(ktime_sub(end_time, start_time));
		pr_debug("migrate back total %lu pages, use %lldms\n",
			 nr_pre_migrate, delta_time_ms);
#endif
		if (nr_fail) {
			task->status = MB_SUBTASK_ERR;
			pr_err("migrate to node: %d failed %d pages\n", i,
			       nr_fail);
			nr_migrate_fail += nr_fail;
		}
		vfree(migrate_folios[i]);
	}
	task->status = (nr_migrate_fail || nr_pre_migrate_fail) ?
			       MB_SUBTASK_ERR :
			       MB_SUBTASK_DONE;
}

static unsigned int smu_migrate(struct folio **folios, unsigned int nr_folios,
				int to_node, struct mig_pra *mul_mig)
{
	int ret;
	unsigned int i;
	unsigned int failed_num = 0;
	unsigned int nr_threads;
	if (mul_mig && mul_mig->is_mul_thread && mul_mig->nr_thread > 0) {
		nr_threads = mul_mig->nr_thread;
		ret = migrate_multi_threaded(nr_threads, folios, nr_folios,
					     to_node);
		if (ret) {
			pr_err("failed to migrate with multi threads, ret:%d\n",
			       ret);
			return nr_folios;
		}
		for (i = 0; i < nr_threads; i++) {
			if (mig[i].nr_folios == 0) {
				continue;
			}
			failed_num += mig[i].failed_num;
		}
	} else {
		failed_num = smap_migrate(folios, nr_folios, to_node, MIGRATE_TYPE_HOTNESS);
	}
	return failed_num;
}

int is_filter_4k(struct page *page, int page_size)
{
	if (page_size == PAGE_SIZE) {
		if (PageTransHuge(page)) {
			return PAGE_TYPE_TRANSHUGE;
		}
		if (PageHuge(page)) {
			return PAGE_TYPE_HUGE;
		}
		if (__folio_test_movable(page_folio(page))) {
			return PAGE_TYPE_NOR_LRU;
		}
		if (page_ref_count(page) == 0) {
			return PAGE_TYPE_ZERO_REF;
		}
	}
	return -1;
}

size_t nr_abnormal[NR_ABNORMAL] = { 0 };

static inline bool is_filter_anon(struct page *page)
{
	if (PageHuge(page)) {
		return false;
	}
	return !PageAnon(page) || page_mapcount(page) > 1;
}

int do_migrate(struct migrate_msg *msg, struct mig_list *mig_list)
{
	int i;
	int pre_migrate_err;
	u64 j, p_addr;
	unsigned int mig_num = 0;
	unsigned int pre_migrate_num;
	unsigned int pre_migrate_failed;
	unsigned int failed_num = 0;
	unsigned int non_anon_num = 0;
	unsigned int tmp_pre_migrate_nr;
	unsigned long pfn;
	int pre_migrate_flag = -1;
	struct page *p_page = NULL;
	int *arr;
	memset(nr_abnormal, 0, sizeof(nr_abnormal));
	if (msg->cnt == 0) {
		return 0;
	}
	arr = kzalloc(msg->cnt * sizeof(*arr), GFP_KERNEL);
	if (!arr)
		return -ENOMEM;

	while (1) {
		int index;
		int max_from = NUMA_NO_NODE;
		/* Do promotion before demotion */
		for (index = 0; index < msg->cnt; index++) {
			if (arr[index])
				continue;
			if (mig_list[index].from > max_from) {
				max_from = mig_list[index].from;
				i = index;
			}
		}
		if (max_from == NUMA_NO_NODE) {
			break;
		}
		pr_info("[%d] pid %d from %d to %d nr %llu\n", i,
			mig_list[i].pid, mig_list[i].from, mig_list[i].to,
			mig_list[i].nr);
		arr[i] = 1;

		if (is_node_invalid(mig_list[i].from) ||
		    is_node_invalid(mig_list[i].to)) {
			continue;
		}
		if (mig_list[i].nr <= 0 || mig_list[i].nr > MAX_MIG_LIST_NR) {
			continue;
		}
		pre_migrate_failed = 0;
		pre_migrate_num = 0;
		tmp_pre_migrate_nr = 0;
		struct folio **migrate_folios =
			vzalloc(mig_list[i].nr * sizeof(struct folio *));
		if (!migrate_folios) {
			kfree(arr);
			return -ENOMEM;
		}
		unsigned int nr_folios = 0;
		for (j = 0; j < mig_list[i].nr; j++) {
			mig_num++;
			p_addr = mig_list[i].addr[j];
			if (p_addr == INVALID_PADDR) {
				continue;
			}
			pfn = PHYS_PFN(p_addr);
			if (!pfn_valid(pfn)) {
				pr_debug("invalid PA\n");
				continue;
			}
			p_page = pfn_to_online_page(pfn);
			if (!p_page)
				continue;
			if (is_filter_anon(p_page)) {
				non_anon_num++;
				continue;
			}
			int err = is_filter_4k(p_page, msg->mul_mig.page_size);
			if (err >= 0) {
				nr_abnormal[err]++;
				continue;
			}
			pre_migrate_flag = smap_add_page_for_migration(
				p_page, migrate_folios, &nr_folios,
				mig_list[i].pid, MPOL_MF_MOVE_ALL);
			if (pre_migrate_flag == 0) {
				pre_migrate_num++;
				tmp_pre_migrate_nr++;
			} else {
				pre_migrate_failed++;
				pre_migrate_err = pre_migrate_flag;
			}
		}

		if (pre_migrate_failed) {
			pr_warn("pre_migrate fail %u pages, ret: %d\n",
				pre_migrate_failed, pre_migrate_err);
		}

		mig_list[i].failed_pre_migrated_nr =
			mig_list[i].nr - tmp_pre_migrate_nr;
		if (nr_folios == 0) {
			pr_debug("no page to migrate\n");
			vfree(migrate_folios);
			continue;
		}
		mig_list[i].failed_mig_nr =
			smu_migrate(migrate_folios, nr_folios, mig_list[i].to,
				    &msg->mul_mig);
		failed_num += mig_list[i].failed_mig_nr;
		mig_list[i].success_to_user = true;
		if (mig_list[i].failed_mig_nr) {
			pr_debug("[%d]: migrate failed, pre_migrate_num: %d, failed_num: %llu\n",
			         i, pre_migrate_num, mig_list[i].failed_mig_nr);
		}
		pr_debug(
			"[%d]: mig_num %d, pre_migrate_num %d, failed_num %llu\n",
			i, mig_num, pre_migrate_num, mig_list[i].failed_mig_nr);
		vfree(migrate_folios);
	}
	kfree(arr);
	pr_debug("non anon page number: %u\n", non_anon_num);
	return failed_num;
}

static int smap_pre_migrate_range(struct folio **folios,
				  unsigned int *nr_folios,
				  unsigned long start_pfn,
				  unsigned long end_pfn)
{
	unsigned long pfn;
	struct page *page, *head;
	int nr_hugepage = 0;
	int nr_normalpage = 0;
	for (pfn = start_pfn; pfn < end_pfn; pfn++) {
		struct folio *folio;

		if (!pfn_valid(pfn)) {
			continue;
		}
		page = pfn_to_online_page(pfn);
		if (!page) {
			continue;
		}
		folio = page_folio(page);
		head = &folio->page;
		if (is_migrate_isolate_page(page)) {
			continue;
		}
		if (is_smap_pg_huge()) {
			if (PageHuge(page) && !PageHead(page)) {
				continue;
			}
			if (!folio_ref_count(folio)) {
				pr_debug("folio ret count is 0\n");
				continue;
			}
			nr_hugepage++;
		} else {
			if (__folio_test_movable(page_folio(page)) ||
			    page_ref_count(page) == 0 || PageTransHuge(page) ||
			    PageHuge(page)) {
				continue;
			}
			if (!folio_try_get(folio)) {
				pr_debug("unable to add folio reference\n");
				continue;
			}
			nr_normalpage++;
		}
		folios[*nr_folios] = folio;
		(*nr_folios)++;
	}
	pr_info("pre migrate: %d huge page, %d base page\n", nr_hugepage, nr_normalpage);
	return nr_hugepage + nr_normalpage;
}

static unsigned int smap_migrate_range(int nid, u64 start_pa, u64 end_pa)
{
	int nr_pre_migrate;
	unsigned nr_migrate_fail;
	unsigned long start_pfn = PHYS_PFN(start_pa);
	unsigned long end_pfn = PHYS_PFN(end_pa);
	unsigned int nr_folios = 0;
	struct folio **migrate_folios;

	if (!pfn_valid(start_pfn) || !pfn_valid(end_pfn)) {
		pr_err("invalid pfn passed to migrate range\n");
		return -EINVAL;
	}
	if (start_pfn >= end_pfn ||
	    (end_pfn - start_pfn + 1) > MAX_MIG_LIST_NR) {
		pr_err("invalid pfn passed to migrate range\n");
		return -EINVAL;
	}
	migrate_folios =
		vzalloc((end_pfn - start_pfn + 1) * sizeof(struct folio *));
	if (!migrate_folios) {
		pr_err("unable to allocate memory for migrate folio list\n");
		return -ENOMEM;
	}
	nr_pre_migrate = smap_pre_migrate_range(migrate_folios, &nr_folios,
						start_pfn, end_pfn);
	nr_migrate_fail = smap_migrate(migrate_folios, nr_folios, nid, MIGRATE_TYPE_REMOTE);
	if (nr_migrate_fail) {
		pr_err("migrate pre_migrate cnt: %d, mig failed %d pages in pfn range %#lx-%#lx\n",
		       nr_pre_migrate, nr_migrate_fail, start_pfn, end_pfn);
	}
	vfree(migrate_folios);
	return nr_migrate_fail;
}

unsigned int smap_migrate_numa(struct migrate_numa_inner_msg *msg)
{
	unsigned int ret = 0;
	int i;
	int nid = msg->dest_nid;
	for (i = 0; i < msg->count; i++) {
		int retry = MAX_MIGRATE_NUMA_RETRY_TIME;
		u64 start_pa = msg->range[i].pa_start;
		u64 end_pa = msg->range[i].pa_end;
		do {
			ret = smap_migrate_range(nid, start_pa, end_pa);
			if (ret == 0)
				break;
			pr_info("migrate range to %d failed %d pages\n", nid,
				ret);
		} while (retry--);
		if (retry == 0)
			return ret;
	}
	return ret;
}

struct folio *alloc_demote_page(struct folio *folio, unsigned long node)
{
	unsigned int order = 0;
	struct migration_target_control mtc = {
		/*
		 * Allocate from 'node', or fail quickly and quietly.
		 * When this happens, 'folio' will likely just be discarded
		 * instead of migrated.
		 */
		.gfp_mask = GFP_HIGHUSER_MOVABLE | __GFP_THISNODE,
		.nid = node,
	};
	return __folio_alloc(mtc.gfp_mask, order, mtc.nid, mtc.nmask);
}

static bool check_subtask_range(struct migrate_back_task *task,
				unsigned long pfn)
{
	struct migrate_back_subtask *subtask;
	list_for_each_entry(subtask, &task->subtask, task_list) {
		unsigned long start_pfn = PHYS_PFN(subtask->pa_start);
		unsigned long end_pfn = PHYS_PFN(subtask->pa_end);
		if (pfn >= start_pfn && pfn <= end_pfn)
			return true;
	}
	return false;
}

bool is_folio_in_migrate_back_range(struct folio *folio)
{
	unsigned long pfn = folio_pfn(folio);
	struct migrate_back_task *task;
	spin_lock(&migrate_back_task_lock);
	list_for_each_entry(task, &migrate_back_task_list, task_node) {
		if (task->status != MB_TASK_WAITING) {
			continue;
		}

		if (check_subtask_range(task, pfn)) {
			spin_unlock(&migrate_back_task_lock);
			return true;
		}
	}
	spin_unlock(&migrate_back_task_lock);
	return false;
}

static inline bool mig_back_filter(struct folio *folio)
{
	return is_folio_in_migrate_back_range(folio);
}

struct folio *smap_alloc_huge_page_node(struct folio *folio, int nid,
					bool is_mig_back)
{
	nodemask_t nodemask;
	unsigned long size = folio_size(folio);
	gfp_t gfp_mask = GFP_HIGHUSER_MOVABLE;
	if (nid != NUMA_NO_NODE)
		gfp_mask |= __GFP_THISNODE;

	nodes_clear(nodemask);
	node_set(nid, nodemask);

	if (is_mig_back) {
		return get_hugetlb_folio_nodemask(size, nid, &nodemask,
						  gfp_mask, mig_back_filter);
	}
	return get_hugetlb_folio_nodemask(size, nid, &nodemask, gfp_mask, NULL);
}

struct folio *smap_alloc_new_node_page(struct folio *folio, unsigned long node)
{
	if (folio_test_hugetlb(folio)) {
		return smap_alloc_huge_page_node(folio, node, false);
	}
	return alloc_demote_page(folio, node);
}

struct folio *smap_alloc_new_node_page_mig_back(struct folio *folio,
						unsigned long node)
{
	if (folio_test_hugetlb(folio)) {
		return smap_alloc_huge_page_node(folio, node, true);
	}
	return alloc_demote_page(folio, node);
}
