/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
* Description: SMAP 6.6
*/
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include "access_tracking_wrapper.h"

using namespace std;

class AccessTrackingWrapperTest : public ::testing::Test {
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

extern "C" pte_t *drivers_smap__pte_offset_map_lock(struct mm_struct *mm,
    pmd_t *pmd, unsigned long addr, spinlock_t **ptlp);
TEST_F(AccessTrackingWrapperTest, PteOffestMapLock)
{
    pte_t pte;

    pte_t *ret = drivers_smap__pte_offset_map_lock(nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(nullptr, ret);
}