/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: SMAP3.0 stub_smap_migrate_wrapper.c test code
 * Author: hebo h00519890
 * Create: 2024-8-20
 */

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include "smap_migrate_wrapper.h"

using namespace std;

class MigrateWrapperTest : public ::testing::Test {
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

struct folio *smap_alloc_huge_page_node(struct folio *folio, int nid, bool is_mig_back);
TEST_F(MigrateWrapperTest, Hugepage)
{
    struct folio *new_folio = NULL;
    struct folio *old_folio = (struct folio *)kmalloc(sizeof(struct folio*), GFP_KERNEL);

    new_folio = smap_alloc_huge_page_node(old_folio, 0, false);
    MOCKER(get_hugetlb_folio_nodemask).stubs().will(returnValue((struct folio *)nullptr));
    EXPECT_EQ(NULL, new_folio);
    kfree(old_folio);
}
