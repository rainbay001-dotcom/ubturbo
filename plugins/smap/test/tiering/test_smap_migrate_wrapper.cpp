/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: SMAP3.0 模块测试代码
 */
#include <linux/slab.h>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include "smap_migrate_wrapper.h"

using namespace std;

extern "C" spinlock_t *fp_hugetlb_lock;

extern "C" struct hstate *(*fp_size_to_hstate)(unsigned long size);
extern "C" bool (*fp_isolate_huge_page)(struct page *page, struct list_head *list);
extern "C" unsigned long (*fp_sum_zone_node_page_state)(int node,
	enum zone_stat_item item);
extern "C" struct anon_vma *(*fp_page_lock_anon_vma_read)(struct page *page);

#if LINUX_VERSION_CODE == KERNEL_VERSION(6, 6, 0)

#else /* KERNEL_VERSION(5, 10, 0) */
extern "C" struct hstate (*fp_hstates)[];
extern "C" unsigned int *fp_default_hstate_idx;
extern "C" void (*fp_putback_movable_pages)(struct list_head *l);
extern "C" void (*fp_rmap_walk)(struct folio *folio, struct rmap_walk_control *rwc);
extern "C" int (*fp_migrate_pages)(struct list_head *from, new_page_t get_new_page,
	free_page_t put_new_page, unsigned long privat, enum migrate_mode mode, int reason);
#endif /* LINUX_VERSION_CODE */

class SmapMigrateWrapperTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        cout << "[Phase SetUp Begin]" << endl;
        cout << "[Phase SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[Phase TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[Phase TearDown End]" << endl;
    }
};

extern "C" int pfn_to_bitidx(const struct page *page, unsigned long pfn);
TEST_F(SmapMigrateWrapperTest, PfnToBitidx)
{
    int ret = pfn_to_bitidx(nullptr, 0);
    EXPECT_EQ(0, ret);
}

extern "C" struct folio *dequeue_huge_page_nodemask(struct hstate *h, gfp_t gfp_mask,
	int nid, nodemask_t *nmask);

struct folio *smap_alloc_huge_page_node(struct folio *folio, int nid, bool is_mig_back);


TEST_F(SmapMigrateWrapperTest, SmapAllocHugePageNode)
{
    struct folio *new_folio = NULL;
    struct folio *old_folio = (struct folio *)kmalloc(sizeof(struct folio*), GFP_KERNEL);
    MOCKER(get_hugetlb_folio_nodemask)
        .stubs()
        .with(any())
        .will(returnValue((struct folio*)NULL));
    new_folio = smap_alloc_huge_page_node(old_folio, 0, false);
    EXPECT_EQ(NULL, new_folio);
}
