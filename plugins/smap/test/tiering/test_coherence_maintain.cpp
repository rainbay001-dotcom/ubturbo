// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

#include <linux/hugetlb.h>
#include <linux/pagewalk.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include "coherence_maintain.h"

extern "C" pte_t g_tmp_pte;

using namespace std;

struct pfn_range {
    unsigned long start_pfn;
    unsigned long end_pfn;
};

struct modify_info {
    int pmd_cnt;
    int pte_cnt;
    int pmd_leaf_cnt;
    int hugetlb_cnt;
    struct pfn_range *hugetlb_ranges;
    bool cacheable;
    int ret;
};

class CoherenceMaintainTest : public ::testing::Test {
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

extern "C" void ham_flush_tlb(struct mm_struct *mm);
TEST_F(CoherenceMaintainTest, HamFlushTlb)
{
    ham_flush_tlb(nullptr);
}

extern "C" int modify_hugetlb_prot(pte_t *pte, unsigned long hmask __always_unused, unsigned long addr,
    unsigned long next __always_unused, struct mm_walk *walk);
TEST_F(CoherenceMaintainTest, ModifyHugetlbProt)
{
    int ret;
    pte_t pte;
    unsigned long hmask = 0;
    unsigned long addr = 0;
    unsigned long next = 0;
    struct mm_walk walk = {};
    struct modify_info info = {};
    info.cacheable = true;
    info.hugetlb_cnt = 0;
    walk.private_data = &info;
    walk.vma = (struct vm_area_struct *)0x1;
    walk.mm = (struct mm_struct *)0x1;
    g_tmp_pte.pte = 1;

    ret = modify_hugetlb_prot(&pte, hmask, addr, next, &walk);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(1, info.hugetlb_cnt);
}

TEST_F(CoherenceMaintainTest, TaskPgtalbWithinMm)
{
    struct mm_struct mm = {};
    unsigned long start = 0;
    unsigned long size = 100;
    bool cacheable = true;

    int ret = task_pgtable_within_mm_set_cacheable(&mm, start, size, cacheable);
    EXPECT_EQ(ret, 0);

    MOCKER(walk_page_range).stubs().will(returnValue(-1));
    ret = task_pgtable_within_mm_set_cacheable(&mm, start, size, cacheable);
    EXPECT_EQ(ret, -1);
}

TEST_F(CoherenceMaintainTest, TaskPgtalbWithinPid)
{
    pid_t pid = 1;
    unsigned long start = 0;
    unsigned long size = 1024;
    bool cacheable = true;
    struct task_struct task = {};
    struct mm_struct mm = {};

    MOCKER(find_get_task_by_vpid).expects(once()).with(eq(pid)).will(returnValue(&task));
    MOCKER(get_task_mm).expects(once()).with(eq(&task)).will(returnValue(&mm));
    MOCKER(task_pgtable_within_mm_set_cacheable).expects(once()).will(returnValue(-1));

    EXPECT_EQ(-1, task_pgtable_within_pid_set_cacheable(pid, start, size, cacheable));
}