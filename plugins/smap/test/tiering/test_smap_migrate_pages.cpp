/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: SMAP3.0 attr模块测试代码
 */
#include <asm/page_no.h>
#include <linux/nodemask.h>
#include <linux/slab.h>
#include <linux/pfn.h>
#include <linux/mm.h>
#include <linux/mmzone.h>
#include <linux/nodemask.h>
#include <linux/time64.h>
#include <linux/list.h>
#include <linux/gfp.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/completion.h>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include "iomem.h"
#include "smap_migrate_pages.h"
#include "smap_migrate_wrapper.h"
#include "tracking_manage.h"
#include "acpi_mem.h"
#include "common.h"
#include "migrate_task.h"
#include "numa.h"
#include "rmap.h"

using namespace std;
extern "C" int PageHuge(struct page *page);
extern "C" int PageHead(struct page *page);
extern "C" int isolate_hugetlb(struct page *page, struct list_head *list);
extern "C" int smap_add_page_for_migration(struct page *page, struct folio **folios,
    unsigned int *nr_folios, pid_t pid, bool migrate_all);
extern "C" bool IS_ERR(const void *ptr);
extern "C" int find_node_to_migrate_rr(int nid, int *out_nid);
extern "C" void refresh_nodes_nr_free(void);
extern "C" int isolate_lru_page(struct page *page);
extern "C" int migrate_multi_threaded(unsigned int nr_threads, struct folio **folios, unsigned int nr_folios,
    int to_node);
extern "C" int calc_paddr_acidx_numa(u64 pa, int *nid, u64 *index);
extern "C" struct multi_migrate_struct mig[MAX_NR_MIGRATE_THREADS];
extern "C" struct multi_migrate_struct;
extern "C" int is_filter_4k(struct page *page, int page_size);
extern "C" long PTR_ERR(const void *ptr);
extern "C" int PageHead(struct page *page);
extern "C" int isolate_hugetlb(struct page *page, struct list_head *list);
extern "C" bool __folio_test_movable(struct folio *folio);
extern "C" bool get_page_unless_zero(struct page *page);
extern "C" bool pfn_valid(unsigned long pfn);
extern "C" struct multi_migrate_struct {
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
#define THREAD_PREFIX "smap_migrate_"
#define num_online_cpus()	2U
extern "C" struct migrate_node {
    int next_nid;
    unsigned long nr[SMAP_MAX_NUMNODES];
} migrate_node;
struct num_node {
    int num;
    u64 page_phy_addr;
    struct hlist_node hlist_node;
};

class SmapMigratePagesTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        cout << "[Phase SetUp Begin]" << endl;
        cout << "[Phase SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[Phase TearDown Begin]" << endl;
        struct acpi_mem_segment *mem;
        struct acpi_mem_segment *ams_tmp;
        list_for_each_entry_safe(mem, ams_tmp, &acpi_mem.mem, segment) {
            list_del(&mem->segment);
            kfree(mem);
        }
        acpi_mem.len = 0;
        GlobalMockObject::verify();
        cout << "[Phase TearDown End]" << endl;
    }
};

TEST_F(SmapMigratePagesTest, TestSmapAddNoPage)
{
    struct page *page = nullptr;
    struct list_head pagelist;
    INIT_LIST_HEAD(&pagelist);
    pid_t pid;
    bool migrate_all = true;
    MOCKER(IS_ERR)
        .stubs()
        .will(returnValue(false));
    int ret = smap_add_page_for_migration(page, nullptr, nullptr, pid, migrate_all);
    EXPECT_EQ(-ENOENT, ret);
}

TEST_F(SmapMigratePagesTest, TestSmapAddPageOne)
{
    struct page *page;
    pid_t pid;
    bool migrate_all = true;
    MOCKER(PTR_ERR).stubs().will(returnValue(-1));
    MOCKER(IS_ERR).stubs().will(returnValue(true));
    int ret = smap_add_page_for_migration(page, nullptr, nullptr, pid, migrate_all);
    EXPECT_EQ(-1, ret);
}

TEST_F(SmapMigratePagesTest, TestSmapAddPageTwo)
{
    struct page *page;
    pid_t pid = 123;
    bool migrate_all = true;
    struct page_task_arg pta;
    pta.found = false;
    MOCKER(PTR_ERR).stubs().will(returnValue(0));
    MOCKER(IS_ERR).stubs().will(returnValue(false));
    MOCKER(PageHuge).stubs().will(returnValue(1));
    MOCKER(PageHead).stubs().will(returnValue(1));
    MOCKER(find_page_task).stubs().with(any(), any(), outBoundP(&pta, sizeof(struct page_task_arg)));
    int ret = smap_add_page_for_migration(page, nullptr, nullptr, pid, migrate_all);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(SmapMigratePagesTest, TestSmapAddPageThree)
{
    struct page *page;
    pid_t pid = 123;
    bool migrate_all = true;
    unsigned int nr_folios = 0;
    struct folio **folios = static_cast<struct folio**>(vzalloc(2 * sizeof(struct folio*)));
    struct page_task_arg pta;
    pta.found = true;
    MOCKER(PTR_ERR).stubs().will(returnValue(0));
    MOCKER(IS_ERR).stubs().will(returnValue(false));
    MOCKER(PageHuge).stubs().will(returnValue(1));
    MOCKER(PageHead).stubs().will(returnValue(1));
    MOCKER(find_page_task).stubs().with(any(), any(), outBoundP(&pta, sizeof(struct page_task_arg)));
    int ret = smap_add_page_for_migration(page, folios, &nr_folios, pid, migrate_all);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, nr_folios);
    vfree(folios);
}

TEST_F(SmapMigratePagesTest, TestSmapAddPageFour)
{
    struct page page;
    pid_t pid = 123;
    bool migrate_all = true;
    unsigned int nr_folios = 0;
    struct folio **folios = static_cast<struct folio**>(vzalloc(2 * sizeof(struct folio*)));
    MOCKER(PTR_ERR).stubs().will(returnValue(0));
    MOCKER(IS_ERR).stubs().will(returnValue(false));
    MOCKER(PageHuge).stubs().will(returnValue(0));
    int ret = smap_add_page_for_migration(&page, folios, &nr_folios, pid, migrate_all);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, nr_folios);
    vfree(folios);
}

TEST_F(SmapMigratePagesTest, TestMigrateMultiThreaded)
{
    unsigned int nr_threads = 2;
    int to_node = 1;
    unsigned int nr_folios = 10;
    struct folio **folios = static_cast<struct folio**>(vzalloc(nr_folios * sizeof(struct folio*)));
    int result = migrate_multi_threaded(nr_threads, folios, nr_folios, to_node);
    EXPECT_EQ(result, 0);
    vfree(folios);
}

TEST_F(SmapMigratePagesTest, TestMigrateMultiThreadedTwo)
{
    unsigned int nr_threads = 2;
    int to_node = 1;
    unsigned int nr_folios = 10;
    struct folio **folios = static_cast<struct folio**>(vzalloc(nr_folios * sizeof(struct folio*)));

    mig[0].init_flag = true;
    int result = migrate_multi_threaded(nr_threads, folios, nr_folios, to_node);
    EXPECT_EQ(result, 0);
    vfree(folios);
}

TEST_F(SmapMigratePagesTest, TestSmapMigrate)
{
    int to_node = 1;
    unsigned int failed_num = 0;
    unsigned int nr_folios = 10;
    struct folio **folios = static_cast<struct folio**>(vzalloc(nr_folios * sizeof(struct folio*)));
    MOCKER(isolate_and_migrate_folios).stubs().will(returnValue(2));
    failed_num = smap_migrate(folios, nr_folios, to_node, false);
    EXPECT_EQ(failed_num, 10);
    vfree(folios);
}

TEST_F(SmapMigratePagesTest, TestSmapMigrateTwo)
{
    int to_node = 1;
    unsigned int failed_num = 0;
    unsigned int nr_folios = 10;
    struct folio **folios = static_cast<struct folio**>(vzalloc(nr_folios * sizeof(struct folio*)));
    MOCKER(isolate_and_migrate_folios).stubs().will(returnValue(10));
    failed_num = smap_migrate(folios, nr_folios, to_node, true);
    EXPECT_EQ(failed_num, 10);
    vfree(folios);
}

TEST_F(SmapMigratePagesTest, TestRefreshNodesNrFree)
{
    nr_local_numa = 0;
    int i;
    for (i = 0; i < 10; i++) {
        migrate_node.nr[i] = 0;
    }
    refresh_nodes_nr_free();
    for (int i = 0; i < SMAP_MAX_NUMNODES; i++) {
        EXPECT_EQ(migrate_node.nr[i], 0);
    }

    nr_local_numa = 3;
    for (i = 0; i < 3; i++) {
        migrate_node.nr[i] = 0;
    }
    refresh_nodes_nr_free();

    EXPECT_EQ(migrate_node.nr[0], 0);
    EXPECT_EQ(migrate_node.nr[1], 0);
    EXPECT_EQ(migrate_node.nr[2], 0);
}

TEST_F(SmapMigratePagesTest, TestFindNdToMigRr)
{
    int out_nid1;
    int nid1 = 1;
    int i;
    nr_local_numa = 50;
    migrate_node.nr[nid1] = 35; // Sufficient pages
    EXPECT_EQ(find_node_to_migrate_rr(nid1, &out_nid1), 0);

    int out_nid2;
    int nid2 = 2;
    nr_local_numa = 2;
    EXPECT_EQ(find_node_to_migrate_rr(nid2, &out_nid2), -22);

    int out_nid3;
    int nid3 = -2;
    nr_local_numa = 5;
    EXPECT_EQ(find_node_to_migrate_rr(nid3, &out_nid3), 0);

    int out_nid4;
    int nid4 = 4;
    for (i = 0; i < 10; i++) {
        migrate_node.nr[i] = 0;
    }
    EXPECT_EQ(find_node_to_migrate_rr(nid4, &out_nid4), -12);
}

extern "C" int thread_fn(void *data);
TEST_F(SmapMigratePagesTest, TestThreadFn)
{
    struct multi_migrate_struct ms;
    ms.to_node = 0;
    ms.comp = {};
    ms.folios = (struct folio**)vzalloc(sizeof(struct folio *)); // vfree in thread_fn
    ASSERT_NE(nullptr, ms.folios);
    int ret = thread_fn(&ms);
    EXPECT_EQ(ret, 0);
}

TEST_F(SmapMigratePagesTest, TestThreadFnTwo)
{
    struct multi_migrate_struct ms;
    ms.to_node = 0;
    ms.comp = {};
    ms.folios = (struct folio**)vzalloc(sizeof(struct folio *)); // vfree in thread_fn
    ASSERT_NE(nullptr, ms.folios);
    MOCKER(isolate_and_migrate_folios).stubs().will(returnValue(2));
    int ret = thread_fn(&ms);
    EXPECT_EQ(ret, 2);
}

extern "C" void clear_mig_folios(unsigned int clear_idx);
TEST_F(SmapMigratePagesTest, TestClearMigFolios)
{
    MOCKER(vfree).stubs();
    clear_mig_folios(1);
    EXPECT_EQ(nullptr, mig[0].folios);
}

extern "C" int smap_add_page_for_migrate_back(u64 pa, struct folio ***migrate_folios, unsigned int *mig_pages_cnt,
    int dest_nid, bool migrate_all);
extern "C" long __must_check PTR_ERR(__force const void *ptr);
extern "C" bool (*fp_isolate_huge_page)(struct page *page, struct list_head *list);

TEST_F(SmapMigratePagesTest, TestAddPageForMigrateBackOne)
{
    u64 pa;
    int dest_nid = 1;
    bool migrate_all = true;

    MOCKER(pfn_valid).stubs().with(any()).will(returnValue(false));
    int ret = smap_add_page_for_migrate_back(pa, nullptr, nullptr, dest_nid, migrate_all);
    EXPECT_EQ(ret, -ENXIO);
}

TEST_F(SmapMigratePagesTest, TestAddPageForMigrateBackTwo)
{
    u64 pa;
    int dest_nid = 1;
    bool migrate_all = true;

    MOCKER(pfn_valid).stubs().with(any()).will(returnValue(true));
    int ret = smap_add_page_for_migrate_back(pa, nullptr, nullptr, dest_nid, migrate_all);
    EXPECT_EQ(ret, -EIO);
}

TEST_F(SmapMigratePagesTest, TestAddPageForMigrateBackThree)
{
    u64 pa;
    int dest_nid = 1;
    bool migrate_all = true;

    MOCKER(pfn_valid).stubs().with(any()).will(returnValue(true));
    MOCKER(pfn_to_online_page).stubs().with(any()).will(returnValue((page*)-1));
    MOCKER(PTR_ERR).stubs().with(any()).will(returnValue(-1));
    int ret = smap_add_page_for_migrate_back(pa, nullptr, nullptr, dest_nid, migrate_all);
    EXPECT_EQ(ret, -1);
}

TEST_F(SmapMigratePagesTest, TestAddPageForMigrateBackFour)
{
    u64 pa;
    int dest_nid = 1;
    bool migrate_all = true;
    page_task_arg pta = {.found = true, .node = 1, .nr_cpus_allowed = 1};
    nr_local_numa = 2;
    unsigned int mig_pages_cnt[SMAP_MAX_LOCAL_NUMNODES] = { 0 };
    struct folio **migrate_folios[SMAP_MAX_LOCAL_NUMNODES] = { nullptr };
    migrate_folios[1] = (struct folio**)vzalloc(2 * sizeof(struct folio*));
    MOCKER(pfn_valid).stubs().with(any()).will(returnValue(true));
    MOCKER(pfn_to_online_page).stubs().with(any()).will(returnValue((page*)-1));
    MOCKER(PTR_ERR).stubs().with(any()).will(returnValue(0));
    MOCKER(IS_ERR).stubs().with(any()).will(returnValue(false));
    MOCKER(PageHuge).stubs().with(any()).will(returnValue(1));
    MOCKER(PageHead).stubs().with(any()).will(returnValue(1));
    MOCKER(find_page_task).stubs().with(any(), any(), outBoundP(&pta, sizeof(struct page_task_arg)));
    int ret = smap_add_page_for_migrate_back(pa, migrate_folios, mig_pages_cnt, dest_nid, migrate_all);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(mig_pages_cnt[1], 1);
    vfree(migrate_folios[1]);
}

extern "C" bool check_addr_range_valid(struct migrate_back_subtask *task);
TEST_F(SmapMigratePagesTest, TestCheckAddrRangeVaild)
{
    struct migrate_back_subtask task;

    task.pa_start = 0x100;
    task.pa_end = 0;
    MOCKER(is_smap_pg_huge).stubs().will(returnValue(true));
    bool ret = check_addr_range_valid(&task);
    EXPECT_EQ(true, ret);
}

extern "C" bool check_addr_range_valid(struct migrate_back_subtask *task);
extern "C" void smap_handle_migrate_back_subtask(struct migrate_back_subtask *task);
TEST_F(SmapMigratePagesTest, TestSmapHandleMigrateBackSubtask)
{
    struct migrate_back_subtask task;
    MOCKER(check_addr_range_valid).stubs().with().will(returnValue(false));
    smap_handle_migrate_back_subtask(&task);
    EXPECT_EQ(true, task.isolated_flag);
    EXPECT_EQ(task.status, MB_SUBTASK_ERR);
}

TEST_F(SmapMigratePagesTest, TestSmapHandleMigrateBackSubtaskOne)
{
    struct migrate_back_subtask task;
    struct page page;
    unsigned int nr_folios = 1;
    task.pa_start = 0x100000;
    task.pa_end = 0x200000;
    MOCKER(check_addr_range_valid).stubs().will(returnValue(true));
    MOCKER(refresh_nodes_nr_free).stubs().will(ignoreReturnValue());
    MOCKER(pfn_valid).stubs().will(returnValue(true));
    MOCKER(pfn_to_online_page).stubs().will(returnValue(&page));
    MOCKER(is_smap_pg_huge).stubs().will(returnValue(true));
    MOCKER(PageHuge).stubs().will(returnValue(1));
    MOCKER(PageHead).stubs().will(returnValue(1));
    MOCKER(smap_add_page_for_migration)
        .stubs()
        .with(any(), any(), outBoundP(&nr_folios, sizeof(nr_folios)), any(), any())
        .will(returnValue(0));
    MOCKER(smap_migrate).stubs().will(returnValue(0));
    smap_handle_migrate_back_subtask(&task);
    EXPECT_EQ(MB_SUBTASK_DONE, task.status);
}

TEST_F(SmapMigratePagesTest, TestSmapHandleMigrateBackSubtaskTwo)
{
    struct migrate_back_subtask task;
    struct page page;
    unsigned int nr_folios = 1;
    task.pa_start = 0x100000;
    task.pa_end = 0x200000;
    MOCKER(check_addr_range_valid).stubs().will(returnValue(true));
    MOCKER(refresh_nodes_nr_free).stubs().will(ignoreReturnValue());
    MOCKER(pfn_valid).stubs().will(returnValue(true));
    MOCKER(pfn_to_online_page).stubs().will(returnValue(&page));
    MOCKER(is_smap_pg_huge).stubs().will(returnValue(true));
    MOCKER(PageHuge).stubs().will(returnValue(1));
    MOCKER(PageHead).stubs().will(returnValue(1));
    MOCKER(smap_add_page_for_migration)
        .stubs()
        .with(any(), any(), outBoundP(&nr_folios, sizeof(nr_folios)), any(), any())
        .will(returnValue(1));
    MOCKER(smap_migrate).stubs().will(returnValue(1));
    smap_handle_migrate_back_subtask(&task);
    EXPECT_EQ(MB_SUBTASK_ERR, task.status);
}

TEST_F(SmapMigratePagesTest, TestSmapHandleMigrateBackSubtaskPageHeadZero)
{
    struct migrate_back_subtask task;
    struct page page;
    unsigned int nr_folios = 0;
    task.pa_start = 0x100000;
    task.pa_end = 0x200000;

    MOCKER(check_addr_range_valid).stubs().will(returnValue(true));
    MOCKER(refresh_nodes_nr_free).stubs().will(ignoreReturnValue());
    MOCKER(pfn_valid).stubs().will(returnValue(true));
    MOCKER(pfn_to_online_page).stubs().will(returnValue(&page));
    MOCKER(is_smap_pg_huge).stubs().will(returnValue(true));
    MOCKER(PageHuge).stubs().will(returnValue(1));
    MOCKER(PageHead).stubs().will(returnValue(0));
    smap_handle_migrate_back_subtask(&task);
    EXPECT_EQ(MB_SUBTASK_DONE, task.status);
}

extern "C" void smap_handle_migrate_back_subtask_4k(struct migrate_back_subtask *task);
extern "C" void process_pages_for_migration(struct migrate_back_subtask *task, struct folio ***migrate_folios,
                                            unsigned int *mig_pages_cnt,
                                            int *nr_pre_migrate_fail, unsigned long *nr_pre_migrate);
TEST_F(SmapMigratePagesTest, TestSmapHandleMigrateBackSubtask4K)
{
    struct page page;
    struct migrate_back_subtask task;
    task.pa_start = 0x100000;
    task.pa_end = 0x200000;
    unsigned int mig_pages_cnt[SMAP_MAX_LOCAL_NUMNODES] = { 0 };
    mig_pages_cnt[0] = 1;
    MOCKER(refresh_nodes_nr_free).stubs().will(ignoreReturnValue());
    MOCKER(process_pages_for_migration).stubs()
    .with(any(), any(), outBoundP(mig_pages_cnt, sizeof(mig_pages_cnt)), any(), any())
    .will(ignoreReturnValue());
    MOCKER(smap_migrate).stubs().will(returnValue(0));
    smap_handle_migrate_back_subtask_4k(&task);
    EXPECT_EQ(MB_SUBTASK_DONE, task.status);
}

TEST_F(SmapMigratePagesTest, TestSmapHandleMigrateBackSubtask4KMigrateFail)
{
    struct page page;
    struct migrate_back_subtask task;
    task.pa_start = 0x100000;
    task.pa_end = 0x200000;
    unsigned int mig_pages_cnt[SMAP_MAX_LOCAL_NUMNODES] = { 1 };
    MOCKER(refresh_nodes_nr_free).stubs().will(ignoreReturnValue());
    MOCKER(process_pages_for_migration).stubs()
        .with(any(), any(), outBoundP(mig_pages_cnt, sizeof(mig_pages_cnt)), any(), any())
        .will(ignoreReturnValue());
    MOCKER(smap_migrate).stubs().will(returnValue(5));
    smap_handle_migrate_back_subtask_4k(&task);
    EXPECT_EQ(MB_SUBTASK_ERR, task.status);
}

TEST_F(SmapMigratePagesTest, TestSmapHandleMigrateBackSubtask4KEmptyMigration)
{
    struct page page;
    struct migrate_back_subtask task;
    task.pa_start = 0x100000;
    task.pa_end = 0x200000;
    unsigned int mig_pages_cnt[SMAP_MAX_LOCAL_NUMNODES] = {};
    MOCKER(refresh_nodes_nr_free).stubs().will(ignoreReturnValue());
    MOCKER(process_pages_for_migration).stubs()
        .with(any(), any(), outBoundP(mig_pages_cnt, sizeof(mig_pages_cnt)), any(), any())
        .will(ignoreReturnValue());
    MOCKER(smap_migrate).stubs().will(returnValue(0));
    smap_handle_migrate_back_subtask_4k(&task);
    EXPECT_EQ(MB_SUBTASK_DONE, task.status);
}

TEST_F(SmapMigratePagesTest, TestSmapHandleMigrateBackSubtask4KPreMigrateFail)
{
    struct page page;
    struct migrate_back_subtask task;
    task.pa_start = 0x100000;
    task.pa_end = 0x200000;
    unsigned int mig_pages_cnt[SMAP_MAX_LOCAL_NUMNODES] = { 1 };
    int nr_pre_migrate_fail = 1;
    MOCKER(refresh_nodes_nr_free).stubs().will(ignoreReturnValue());
    MOCKER(process_pages_for_migration)
        .stubs()
        .with(any(), any(), outBoundP(mig_pages_cnt, sizeof(mig_pages_cnt)),
              outBoundP(&nr_pre_migrate_fail, sizeof(nr_pre_migrate_fail)), any())
        .will(ignoreReturnValue());
    MOCKER(smap_migrate).stubs().will(returnValue(0));
    smap_handle_migrate_back_subtask_4k(&task);
    EXPECT_EQ(MB_SUBTASK_ERR, task.status);
}

int MockIsolatePageRangeTwo(unsigned long start_pfn, unsigned long end_pfn,
    int migratetype, int flags, gfp_t gfp_flags)
{
    return 0;
}

struct migration_target_control {
    int nid; /* preferred node id */
    nodemask_t *nmask;
    gfp_t gfp_mask;
};

extern "C" struct folio *alloc_migration_target(struct folio *, unsigned long);
TEST_F(SmapMigratePagesTest, TestAllocDemotePage)
{
    int nid = 1;
    struct folio *page = (struct folio *)kmalloc(sizeof(struct folio), GFP_KERNEL);
    struct folio *new_page = nullptr;
    MOCKER(alloc_migration_target).stubs().with(any(), any()).will(returnValue(static_cast<struct folio*>(nullptr)));

    new_page =  alloc_demote_page(page, static_cast<unsigned long>(nid));
    EXPECT_EQ(new_page, nullptr);
    kfree(page);
}

TEST_F(SmapMigratePagesTest, TestSmapAllocNewNodePage)
{
    int nid = 1;
    struct folio *page = (struct folio *)kmalloc(sizeof(struct folio), GFP_KERNEL);
    struct folio *new_page = nullptr;
    MOCKER(alloc_demote_page).stubs().with(any(), any()).will(returnValue(static_cast<struct folio*>(nullptr)));
    MOCKER(folio_test_hugetlb).stubs().with(any()).will(returnValue(false));
    new_page =  smap_alloc_new_node_page(page, static_cast<unsigned long>(nid));
    EXPECT_EQ(new_page, nullptr);
    kfree(page);
}

TEST_F(SmapMigratePagesTest, TestSmapAllocNewNodePageTwo)
{
    int nid = 1;
    struct folio *page = (struct folio *)kmalloc(sizeof(struct folio), GFP_KERNEL);
    struct folio *new_page = nullptr;
    MOCKER(alloc_demote_page).stubs().will(returnValue(static_cast<struct folio*>(nullptr)));
    new_page = smap_alloc_new_node_page(page, static_cast<unsigned long>(nid));
    EXPECT_EQ(new_page, nullptr);
    kfree(page);
}

extern "C" int smu_migrate(struct folio **folios, unsigned int nr_folios, int to_node,
                           struct mig_pra *mul_mig);
TEST_F(SmapMigratePagesTest, SmuMigrate)
{
    int to_node = 0;
    struct mig_pra mul_mig;
    mul_mig.nr_thread = 1;
    mul_mig.is_mul_thread = true;
    unsigned int nr_folios = 10;
    struct folio **folios = static_cast<struct folio**>(vzalloc(nr_folios * sizeof(struct folio*)));
    MOCKER(smap_migrate).stubs().will(returnValue(2));
    MOCKER(migrate_multi_threaded).stubs().will(returnValue(0));
    unsigned int ret = smu_migrate(folios, nr_folios, to_node, &mul_mig);
    EXPECT_EQ(0, ret);
    vfree(folios);
}

TEST_F(SmapMigratePagesTest, SmuMigrateOne)
{
    int to_node = 0;
    struct mig_pra mul_mig;
    mul_mig.nr_thread = 0;
    mul_mig.is_mul_thread = false;
    unsigned int nr_folios = 10;
    struct folio **folios = static_cast<struct folio**>(vzalloc(nr_folios * sizeof(struct folio*)));
    MOCKER(smap_migrate).stubs().will(returnValue(2));
    unsigned int ret = smu_migrate(folios, nr_folios, to_node, &mul_mig);
    EXPECT_EQ(2, ret);
    vfree(folios);
}

TEST_F(SmapMigratePagesTest, SmuMigrateTwo)
{
    int to_node = 0;
    struct mig_pra mul_mig;
    mul_mig.nr_thread = 1;
    mul_mig.is_mul_thread = true;
    unsigned int nr_folios = 10;
    struct folio **folios = static_cast<struct folio**>(vzalloc(nr_folios * sizeof(struct folio*)));
    MOCKER(smap_migrate).stubs().will(returnValue(0));
    unsigned int ret = smu_migrate(folios, nr_folios, to_node, &mul_mig);
    EXPECT_EQ(0, ret);
    vfree(folios);
}

extern "C" int is_filter_4k(struct page *page, int page_size);
extern "C" struct page *pfn_to_online_page(unsigned long pfn);
extern "C" int do_migrate(struct migrate_msg *msg, struct mig_list *mig_list);
TEST_F(SmapMigratePagesTest, DoMigrateTestOnce)
{
    int ret = 0;
    int arr;
    struct migrate_msg *msg = (struct migrate_msg *)kmalloc(sizeof(struct migrate_msg), GFP_KERNEL);
    struct mig_list *mig_list = (struct mig_list *)kmalloc(sizeof(struct mig_list), GFP_KERNEL);

    msg->cnt = 0;
    MOCKER(kzalloc).stubs().will(returnValue((void*)&arr));
    ret = do_migrate(msg, nullptr);
    EXPECT_EQ(0, ret);
    kfree(msg);
    kfree(mig_list);
}

extern "C" bool is_filter_anon(struct page *page);
TEST_F(SmapMigratePagesTest, DoMigrateMultiListFailed)
{
    struct page p_page;
    struct migrate_msg msg;
    struct mig_list mig_list[2];
    u64 addr = 1;
    mig_list[0].addr = &addr;
    mig_list[1].addr = &addr;
    unsigned int nr_folios = 1;
    msg.cnt = 2;
    mig_list[0].from = 0;
    mig_list[0].pid = 1;
    mig_list[0].to = 4;
    mig_list[0].nr = 3;
    mig_list[1].from = 0;
    mig_list[1].pid = 2;
    mig_list[1].to = 4;
    mig_list[1].nr = 1;
    MOCKER(pfn_valid).stubs().will(returnValue(true));
    MOCKER(pfn_to_online_page).stubs().will(returnValue(&p_page));
    MOCKER(is_filter_anon).stubs().will(returnValue(false));
    MOCKER(is_filter_4k).stubs().will(returnValue(-1));
    MOCKER(smap_add_page_for_migration)
        .stubs()
        .with(any(), any(), outBoundP(&nr_folios, sizeof(nr_folios)), any(), any())
        .will(returnValue(0));
    MOCKER(smu_migrate).stubs().will(returnValue(1)).then(returnValue(0));
    int ret = do_migrate(&msg, mig_list);
    EXPECT_EQ(1, ret);
    EXPECT_EQ(1, mig_list[0].failed_mig_nr);
    EXPECT_EQ(0, mig_list[0].failed_pre_migrated_nr);
    EXPECT_EQ(0, mig_list[1].failed_mig_nr);
    EXPECT_EQ(0, mig_list[1].failed_pre_migrated_nr);
}

TEST_F(SmapMigratePagesTest, DoMigrateKzallocFailed)
{
    int ret = 0;
    struct migrate_msg *msg = (struct migrate_msg *)kmalloc(sizeof(struct migrate_msg), GFP_KERNEL);
    struct mig_list *mig_list = (struct mig_list *)kmalloc(sizeof(struct mig_list), GFP_KERNEL);
    msg->cnt = 1;

    MOCKER(kzalloc).stubs().will(returnValue((void*)nullptr));
    ret = do_migrate(msg, mig_list);
    EXPECT_EQ(-ENOMEM, ret);
    kfree(msg);
    kfree(mig_list);
}

TEST_F(SmapMigratePagesTest, DoMigrateSingleListSuccess)
{
    struct page page;
    int ret = 0;
    int arr[1] = {0};
    struct migrate_msg *msg = (struct migrate_msg *)kmalloc(sizeof(struct migrate_msg), GFP_KERNEL);
    struct mig_list *mig_list = (struct mig_list *)kmalloc(sizeof(struct mig_list), GFP_KERNEL);
    mig_list[0].from = NUMA_NO_NODE + 1;
    mig_list[0].nr = 1;
    msg->cnt = 1;
    msg->mig_list = mig_list;
    unsigned int nr_folios = 1;
    MOCKER(kzalloc).stubs().will(returnValue((void*)arr));
    MOCKER(pfn_valid).stubs().will(returnValue(true));
    MOCKER(pfn_to_online_page).stubs().will(returnValue(&page));
    MOCKER(is_filter_4k).stubs().will(returnValue(-1));
    MOCKER(smap_add_page_for_migration)
        .stubs()
        .with(any(), any(), outBoundP(&nr_folios, sizeof(nr_folios)), any(), any())
        .will(returnValue(0));
    MOCKER(smu_migrate).stubs().will(returnValue(0));
    MOCKER(kfree).stubs().will(ignoreReturnValue());
    ret = do_migrate(msg, mig_list);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0, mig_list[0].failed_mig_nr);
    EXPECT_EQ(0, mig_list[0].failed_pre_migrated_nr);
    kfree(msg);
    kfree(mig_list);
}

TEST_F(SmapMigratePagesTest, DoMigrateNoValidNid)
{
    int ret = 0;
    int arr[1] = {1};
    struct migrate_msg *msg = (struct migrate_msg *)kmalloc(sizeof(struct migrate_msg), GFP_KERNEL);
    struct mig_list *mig_list = (struct mig_list *)kmalloc(sizeof(struct mig_list), GFP_KERNEL);
    mig_list[0].from = NUMA_NO_NODE;
    mig_list[0].nr = 1;
    msg->cnt = 1;
    msg->mig_list = mig_list;

    MOCKER(kzalloc).stubs().will(returnValue((void*)arr));
    MOCKER(kfree).stubs().will(ignoreReturnValue());
    ret = do_migrate(msg, mig_list);
    EXPECT_EQ(0, ret);
    kfree(msg);
    kfree(mig_list);
}

TEST_F(SmapMigratePagesTest, DoMigrateSingleListFailed)
{
    struct page page;
    int ret = 0;
    int arr[1] = {0};
    unsigned int nr_folios = 1;
    struct migrate_msg *msg = (struct migrate_msg *)kmalloc(sizeof(struct migrate_msg), GFP_KERNEL);
    struct mig_list *mig_list = (struct mig_list *)kmalloc(sizeof(struct mig_list), GFP_KERNEL);
    mig_list[0].from = NUMA_NO_NODE + 1;
    mig_list[0].nr = 1;
    msg->cnt = 1;
    msg->mig_list = mig_list;

    MOCKER(kzalloc).stubs().will(returnValue((void*)arr));
    MOCKER(pfn_valid).stubs().will(returnValue(true));
    MOCKER(pfn_to_online_page).stubs().will(returnValue(&page));
    MOCKER(is_filter_4k).stubs().will(returnValue(-1));
    MOCKER(smap_add_page_for_migration)
        .stubs()
        .with(any(), any(), outBoundP(&nr_folios, sizeof(nr_folios)), any(), any())
        .will(returnValue(1));
    MOCKER(smu_migrate).stubs().will(returnValue(1));
    MOCKER(kfree).stubs().will(ignoreReturnValue());
    ret = do_migrate(msg, mig_list);
    EXPECT_EQ(1, ret);
    EXPECT_EQ(1, mig_list[0].failed_mig_nr);
    EXPECT_EQ(1, mig_list[0].failed_pre_migrated_nr);
    kfree(msg);
    kfree(mig_list);
}

TEST_F(SmapMigratePagesTest, DoMigrate4KPageIsPageTransHuge)
{
    struct page page;
    int ret = 0;
    int arr[1] = {0};
    unsigned int nr_folios = 1;
    struct migrate_msg *msg = (struct migrate_msg *)kmalloc(sizeof(struct migrate_msg), GFP_KERNEL);
    struct mig_list *mig_list = (struct mig_list *)kmalloc(sizeof(struct mig_list), GFP_KERNEL);
    mig_list[0].from = NUMA_NO_NODE + 1;
    mig_list[0].nr = 1;
    msg->cnt = 1;
    msg->mig_list = mig_list;

    MOCKER(kzalloc).stubs().will(returnValue((void*)arr));
    MOCKER(pfn_to_online_page).stubs().will(returnValue(&page));
    MOCKER(is_filter_4k).stubs().will(returnValue(0)); // page is PageTransHuge
    MOCKER(kfree).stubs().will(ignoreReturnValue());
    ret = do_migrate(msg, mig_list);
    EXPECT_EQ(0, ret);
    kfree(msg);
    kfree(mig_list);
}


TEST_F(SmapMigratePagesTest, TestIsFilter4k)
{
    struct page page;
    int pageSize = 0x1000;
    int ret = is_filter_4k(&page, pageSize);
    EXPECT_EQ(2, ret);

    pageSize = 0x3;
    ret = is_filter_4k(&page, pageSize);
    EXPECT_EQ(-1, ret);
}

extern "C" unsigned long page_to_pfn(struct page *page);
extern "C" unsigned long compound_nr(struct page *page);
extern "C" int smap_pre_migrate_range(struct folio **folios, unsigned int *nr_folios, unsigned long start_pfn,
    unsigned long end_pfn);
TEST_F(SmapMigratePagesTest, TestSmapIsolateRange)
{
    unsigned int nr_folios = 0;
    struct page tmp_page;
    unsigned long start = 0x0;
    unsigned long end = 0x1;
    struct folio **migrate_folios = static_cast<struct folio**>(vzalloc((end - start + 1) * sizeof(struct folio*)));
    MOCKER(pfn_valid).stubs().will(returnValue(true));
    MOCKER(pfn_to_online_page).stubs().will(returnValue(&tmp_page));
    MOCKER(is_smap_pg_huge).stubs().will(returnValue(true));
    MOCKER(PageHuge).stubs().will(returnValue(0));
    MOCKER(PageHead).stubs().will(returnValue(1));
    int ret = smap_pre_migrate_range(migrate_folios, &nr_folios, start, end);
    EXPECT_EQ(end - start, ret);
    vfree(migrate_folios);
}

TEST_F(SmapMigratePagesTest, TestSmapIsolateRangeOne)
{
    unsigned int nr_folios = 0;
    struct page tmp_page;
    unsigned long start = 0x0;
    unsigned long end = 0x1;
    struct folio **migrate_folios = static_cast<struct folio**>(vzalloc((end - start + 1) * sizeof(struct folio*)));
    MOCKER(pfn_valid).stubs().will(returnValue(true));
    MOCKER(pfn_to_online_page).stubs().will(returnValue(&tmp_page));
    MOCKER(PageHuge).stubs().will(returnValue(0));
    MOCKER(get_page_unless_zero).stubs().will(returnValue(true));
    int ret = smap_pre_migrate_range(migrate_folios, &nr_folios, start, end);
    EXPECT_EQ(end - start, ret);
    vfree(migrate_folios);
}

extern "C" int smap_migrate_range(int nid, u64 start_pa, u64 end_pa);
TEST_F(SmapMigratePagesTest, TestSmapMigrateRange)
{
    struct page tmp_page;
    unsigned long start = 0x0;
    unsigned long end = 0x400000;
    MOCKER(smap_pre_migrate_range).stubs().will(returnValue(0));
    MOCKER(smap_migrate).stubs().will(returnValue(1));
    unsigned int ret = smap_migrate_range(5, start, end);
    EXPECT_EQ(1, ret);

    GlobalMockObject::verify();
    MOCKER(smap_pre_migrate_range).stubs().will(returnValue(0));
    MOCKER(smap_migrate).stubs().will(returnValue(0));
    ret = smap_migrate_range(5, start, end);
    EXPECT_EQ(0, ret);

    // invalid pa case
    ret = smap_migrate_range(5, end, start);
    EXPECT_EQ(-EINVAL, ret);
    ret = smap_migrate_range(5, end, end);
    EXPECT_EQ(-EINVAL, ret);
}

extern "C" unsigned int smap_migrate_numa(struct migrate_numa_inner_msg *msg);
TEST_F(SmapMigratePagesTest, TestSmapMigrateNuma)
{
    struct migrate_numa_inner_msg msg = {
        .src_nid = 4,
        .dest_nid = 5,
        .count = 1,
        .range = { { 0x0, 0x400000 } }
    };

    MOCKER(smap_migrate_range).stubs().will(returnValue(1));
    unsigned int ret = smap_migrate_numa(&msg);
    EXPECT_EQ(1, ret);

    // retry case, last retry success
    GlobalMockObject::verify();
    int migRetryTime = 10;
    MOCKER(smap_migrate_range).expects(exactly(migRetryTime + 1))
        .will(repeat(-EINVAL, migRetryTime))
        .then(returnValue(0));
    ret = smap_migrate_numa(&msg);
    EXPECT_EQ(0, ret);
}

extern "C" bool check_subtask_range(struct migrate_back_task *task, unsigned long pfn);
TEST_F(SmapMigratePagesTest, TestCheckSubtaskRange)
{
    struct migrate_back_task task;
    struct migrate_back_subtask subtask;

    INIT_LIST_HEAD(&task.subtask);
    list_add(&subtask.task_list, &task.subtask);
    subtask.pa_start = 0x100;
    subtask.pa_end = 0x1000;

    bool ret = check_subtask_range(&task, 1);
    EXPECT_EQ(true, ret);
}

extern "C" struct list_head migrate_back_task_list;
extern "C" bool is_folio_in_migrate_back_range(struct folio *folio);
TEST_F(SmapMigratePagesTest, TestIsFolioInMigrateBackRange)
{
    struct folio folio;
    struct migrate_back_task task;

    task.status = MB_TASK_DONE;
    EXPECT_EQ(true, list_empty(&migrate_back_task_list));
    list_add(&task.task_node, &migrate_back_task_list);
    bool ret = is_folio_in_migrate_back_range(&folio);
    EXPECT_EQ(false, ret);

    task.status = MB_TASK_WAITING;
    MOCKER(check_subtask_range).stubs().will(returnValue(true));
    ret = is_folio_in_migrate_back_range(&folio);
    EXPECT_EQ(true, ret);
    list_del(&task.task_node);
}