// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

#include <linux/mm.h>
#include <linux/sched/mm.h>
#include "basic.h"

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

using namespace std;

extern "C" {
    int kernel_num_contig_ptes(unsigned long size, size_t *pgsize);
}

class BasicTest : public ::testing::Test {
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

TEST_F(BasicTest, KernelNumContigPtes)
{
    unsigned long size;
    size_t pgsize;
    size = PUD_SIZE;
    int ret = kernel_num_contig_ptes(size, &pgsize);
    EXPECT_EQ(1, ret);

    size = PMD_SIZE;
    ret = kernel_num_contig_ptes(size, &pgsize);
    EXPECT_EQ(1, ret);

    size = CONT_PMD_SIZE;
    ret = kernel_num_contig_ptes(size, &pgsize);
    EXPECT_EQ(CONT_PMDS, ret);
    EXPECT_EQ(PMD_SIZE, pgsize);

    size = CONT_PTE_SIZE;
    ret = kernel_num_contig_ptes(size, &pgsize);
    EXPECT_EQ(CONT_PMDS, ret);
    EXPECT_EQ(PAGE_SIZE, pgsize);
}

TEST_F(BasicTest, FindGetMmByVpid)
{
    struct mm_struct *mm;
    struct task_struct task = {};
    pid_t pid = 0;
    MOCKER(find_get_task_by_vpid).stubs().will(returnValue((struct task_struct *)nullptr));
    mm = find_get_mm_by_vpid(pid);
    EXPECT_EQ(nullptr, mm);
    GlobalMockObject::verify();

    MOCKER(find_get_task_by_vpid).stubs().will(returnValue(&task));
    MOCKER(get_task_mm).stubs().will(returnValue((struct mm_struct *)nullptr));
    MOCKER(put_task_struct).stubs().will(ignoreReturnValue());
    mm = find_get_mm_by_vpid(pid);
    EXPECT_EQ(nullptr, mm);
}