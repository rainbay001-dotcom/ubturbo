/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: SMAP5.0 远端内存监控模块测试代码
 */

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/memory.h>

#include "check.h"
#include "access_acpi_mem.h"
#include "access_iomem.h"
#include "access_tracking.h"
#include "memory_notifier.h"

using namespace std;
extern "C" unsigned int drivers_smap_scene;

class MemNotifierTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        std::cout << "[Phase SetUp Begin]" << std::endl;
        std::cout << "[Phase SetUp End]" << std::endl;
    }
    void TearDown() override
    {
        std::cout << "[Phase TearDown Begin]" << std::endl;
        GlobalMockObject::reset();
        std::cout << "[Phase TearDown End]" << std::endl;
    }
};

extern "C" struct page *pfn_to_online_page(unsigned long pfn);
extern "C" bool pfn_valid(unsigned long pfn);
extern "C" int refresh_remote_ram(void);
extern "C" int memory_notifier_cb(struct memory_notify *mnb, unsigned long action);
TEST_F(MemNotifierTest, memory_notifier_cb)
{
    int ret;
    struct page page;
    struct memory_notify mnb = {
        .start_pfn = 0,
        .nr_pages = 10,
    };
    drivers_smap_scene = UB_QEMU_SCENE;
    ret = memory_notifier_cb(&mnb, MEM_ONLINE);
    EXPECT_EQ(0, ret);

    drivers_smap_scene = NORMAL_SCENE;
    MOCKER(pfn_valid).stubs().will(returnValue(false));
    ret = memory_notifier_cb(&mnb, MEM_ONLINE);
    EXPECT_EQ(-EINVAL, ret);

    GlobalMockObject::reset();
    MOCKER(pfn_to_online_page).stubs().will(returnValue(&page));
    MOCKER(page_to_nid).stubs().will(returnValue(0));
    MOCKER(refresh_remote_ram).stubs().will(returnValue(0));
    MOCKER(tracking_core_reinit_actc_buffer).stubs().will(returnValue(0));
    ret = memory_notifier_cb(&mnb, MEM_ONLINE);
    EXPECT_EQ(0, ret);

    GlobalMockObject::reset();
    MOCKER(pfn_to_online_page).stubs().will(returnValue(static_cast<struct page *>(nullptr)));
    ret = memory_notifier_cb(&mnb, MEM_ONLINE);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(MemNotifierTest, memory_notifier_cb_two)
{
    int ret;
    struct page page;
    struct memory_notify mnb = {
        .start_pfn = 0,
        .nr_pages = 10,
    };
    drivers_smap_scene = NORMAL_SCENE;
    MOCKER(get_numa_by_pfn).stubs().will(returnValue(NUMA_NO_NODE));
    MOCKER(pfn_to_online_page).stubs().will(returnValue(&page));
    MOCKER(page_to_nid).stubs().will(returnValue(0));
    MOCKER(refresh_remote_ram).stubs()
        .will(returnValue(-EINVAL))
        .then(returnValue(0));
    MOCKER(tracking_core_reinit_actc_buffer).stubs()
        .will(returnValue(-EINVAL))
        .then(returnValue(0));
    ret = memory_notifier_cb(&mnb, MEM_OFFLINE);
    EXPECT_EQ(-EINVAL, ret);
    ret = memory_notifier_cb(&mnb, MEM_OFFLINE);
    EXPECT_EQ(-EINVAL, ret);
    ret = memory_notifier_cb(&mnb, MEM_OFFLINE);
    EXPECT_EQ(0, ret);
}

extern "C" int pre_offline_notifier_cb(struct memory_notify *mnb, unsigned long action);
TEST_F(MemNotifierTest, pre_offline_notifier_cb)
{
    int ret;
    struct memory_notify mnb;
    ret = pre_offline_notifier_cb(&mnb, MEM_ONLINE);
    EXPECT_EQ(0, ret);
}

extern "C" int smap_memory_notifier(struct notifier_block *self,
    unsigned long action, void *arg);
TEST_F(MemNotifierTest, smap_memory_notifier)
{
    int ret;
    struct notifier_block nb;
    MOCKER(memory_notifier_cb).stubs().will(returnValue(0));
    ret = smap_memory_notifier(&nb, MEM_ONLINE, NULL);
    EXPECT_EQ(NOTIFY_OK, ret);
}

TEST_F(MemNotifierTest, smap_memory_notifier_two)
{
    int ret;
    struct notifier_block nb;
    MOCKER(pre_offline_notifier_cb).stubs().will(returnValue(0));
    ret = smap_memory_notifier(&nb, MEM_GOING_OFFLINE, NULL);
    EXPECT_EQ(NOTIFY_OK, ret);

    ret = smap_memory_notifier(&nb, MEM_CANCEL_ONLINE, NULL);
    EXPECT_EQ(NOTIFY_OK, ret);
}

extern "C" int memory_notifier_init(void);
TEST_F(MemNotifierTest, memory_notifier_init)
{
    int ret;
    struct notifier_block nb;
    MOCKER(register_memory_notifier).stubs().will(returnValue(0));
    ret = memory_notifier_init();
    EXPECT_EQ(0, ret);
}

TEST_F(MemNotifierTest, memory_notifier_exit)
{
    memory_notifier_exit();
}
