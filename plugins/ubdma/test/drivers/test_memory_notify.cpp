/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: memory_notify test
 */

#include <cerrno>
#include <cstdarg>
#include <cstddef>
#include <iostream>
#include <cstring>
#include "gtest/gtest.h"
#include "linux/compiler_attributes.h"
#include "linux/device.h"
#include "linux/memory.h"
#include "linux/notifier.h"
#include "linux/mm_types.h"
#include "linux/types.h"
#include "linux/dmaengine.h"
#include "linux/migrate.h"
#include "ubcore_types.h"
#include "stub_struct.h"
#include "mockcpp/ChainingMockHelper.h"
#include "mockcpp/mokc.h"
#include "urma.h"
#include "ubcore_opcode.h"

using namespace std;

class TestMemoryNotify : public ::testing::Test {
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

extern "C" {
void mem_nf_register_sge(struct work_struct *w __always_unused);
int ub_dma_unregister_segment(u64 pa_start, u64 pa_end);
int ub_dma_register_segment(u64 pa_start, u64 pa_end);
int pfn_valid(unsigned long pfn);
int memory_notifier_cb(struct memory_notify *mnb, unsigned long action);
void mem_nf_unregister_sge(struct work_struct *w __always_unused);
int pre_offline_notifier_cb(struct memory_notify *mnb, unsigned long action);
struct page *pfn_to_online_page(unsigned long pfn);
int ub_dma_memory_notifier(struct notifier_block *self, unsigned long action, void *arg);
}

TEST_F(TestMemoryNotify, mem_nf_register_sge_test)
{
    mem_nf_work_info *memwrk = (mem_nf_work_info*)malloc(sizeof(mem_nf_work_info));
    EXPECT_TRUE(memwrk != nullptr);
    MOCKER(ub_dma_register_segment).stubs().will(returnValue(1));
    mem_nf_register_sge(&memwrk->work);
    memory_notify mnb;
    mnb.start_pfn = 0x2314123;
    int ret = memory_notifier_cb(&mnb, 0);
    EXPECT_EQ(ret, 0);
    MOCKER(pfn_valid).stubs().will(returnValue(0));
    ret = memory_notifier_cb(&mnb, MEM_ONLINE);
    EXPECT_EQ(ret, -EINVAL);
    GlobalMockObject::verify();
    MOCKER(pfn_valid).stubs().will(returnValue(1)).then(returnValue(0));
    ret = memory_notifier_cb(&mnb, MEM_ONLINE);
    EXPECT_EQ(ret, 0);
    GlobalMockObject::verify();
    MOCKER(kzalloc).stubs().will(returnValue((void*)nullptr));
    MOCKER(pfn_valid).stubs().will(returnValue(1));
    ret = memory_notifier_cb(&mnb, MEM_ONLINE);
    EXPECT_EQ(ret, -ENOMEM);
    GlobalMockObject::verify();
    MOCKER(pfn_valid).stubs().will(returnValue(1));
    ret = memory_notifier_cb(&mnb, MEM_ONLINE);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestMemoryNotify, pre_offline_notifier_cb_test)
{
    mem_nf_work_info *memwrk = (mem_nf_work_info*)malloc(sizeof(mem_nf_work_info));
    EXPECT_TRUE(memwrk != nullptr);
    MOCKER(ub_dma_unregister_segment).stubs().will(returnValue(1));
    mem_nf_unregister_sge(&memwrk->work);
    memory_notify mnb;
    mnb.start_pfn = 0x2314123;
    MOCKER(pfn_to_online_page).stubs().will(returnValue(static_cast<page*>(nullptr)));
    int ret = pre_offline_notifier_cb(&mnb, 0);
    EXPECT_EQ(ret, -EINVAL);
    GlobalMockObject::verify();
    MOCKER(kzalloc).stubs().will(returnValue((void*)nullptr));
    ret = pre_offline_notifier_cb(&mnb, 0);
    EXPECT_EQ(ret, -ENOMEM);
    GlobalMockObject::verify();
    ret = pre_offline_notifier_cb(&mnb, 0);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestMemoryNotify, ub_dma_memory_notifier_test)
{
    notifier_block self;
    memory_notify mnb;
    MOCKER(memory_notifier_cb).stubs().will(returnValue(1));
    int ret = ub_dma_memory_notifier(&self, MEM_OFFLINE, (void*)&mnb);
    EXPECT_EQ(ret, NOTIFY_OK);
    MOCKER(pre_offline_notifier_cb).stubs().will(returnValue(1));
    ret = ub_dma_memory_notifier(&self, MEM_GOING_OFFLINE, (void*)&mnb);
    EXPECT_EQ(ret, NOTIFY_OK);
}