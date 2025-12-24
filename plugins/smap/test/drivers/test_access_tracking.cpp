/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
* Description: SMAP3.0 accessed_bit测试代码
*/

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include <asm/pgtable.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/spinlock.h>
#include <linux/hugetlb.h>
#include <linux/list.h>
#include <linux/mmap_lock.h>
#include <linux/errno.h>
#include <linux/pid.h>
#include <linux/mm_types.h>
#include <linux/kvm_host.h>
#include <linux/workqueue.h>

#include "access_acpi_mem.h"
#include "access_ioctl.h"
#include "access_tracking.h"

using namespace std;

extern "C" int init_acpi_mem(void);
extern "C" struct list_head access_dev;
extern "C" int hist_actc_data_init(void);

class AccessTrackingTest : public ::testing::Test {
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

extern "C" bool is_access_hugepage(void);
TEST_F(AccessTrackingTest, is_access_hugepage)
{
    (void)is_access_hugepage();
}

extern "C" void drivers_init_actc_data(struct access_tracking_dev *adev);
extern "C" void access_tracking_enable(struct device *ldev);
TEST_F(AccessTrackingTest, access_tracking_enable)
{
        struct access_tracking_dev dev;
        MOCKER(drivers_init_actc_data).stubs().will(ignoreReturnValue());
        (void)access_tracking_enable(&dev.ldev);
}

extern "C" void access_tracking_disable(struct device *ldev);
TEST_F(AccessTrackingTest, access_tracking_disable)
{
        struct access_tracking_dev dev;
        (void)access_tracking_disable(&dev.ldev);
}

TEST_F(AccessTrackingTest, init_actc_data)
{
    struct access_tracking_dev adev;
    adev.mode = ACCESS_MODE_AND;
    adev.page_count = 1;
    u16 data = 0;
    adev.access_bit_actc_data = &data;
    drivers_init_actc_data(&adev);
    EXPECT_EQ(0xffff, adev.access_bit_actc_data[0]);
}

extern "C" int get_page_size(struct access_tracking_dev *adev);
TEST_F(AccessTrackingTest, get_page_size)
{
    struct access_tracking_dev adev;
    adev.page_size_mode = PAGE_MODE_2M;
    int ret = get_page_size(&adev);
    EXPECT_EQ(PAGE_SIZE_2M, ret);
}

extern "C" u64 calc_access_len_v2(struct access_tracking_dev *adev);
TEST_F(AccessTrackingTest, calc_access_len_v2)
{
    struct access_tracking_dev adev;
    adev.page_size_mode = PAGE_MODE_2M;
    nr_local_numa = 4;
    adev.node = 4;
    MOCKER(get_node_page_cnt_iomem).stubs().will(returnValue((u64)1));
    u64 ret = calc_access_len_v2(&adev);
    EXPECT_EQ(1, ret);

    adev.node = 3;
    MOCKER(get_node_actc_len).stubs().will(returnValue((u64)2));
    ret = calc_access_len_v2(&adev);
    EXPECT_EQ(2, ret);
}

extern "C" int access_tracking_mode_set(struct device *ldev, u8 mode);
TEST_F(AccessTrackingTest, access_tracking_mode_set)
{
    struct access_tracking_dev adev;
    int ret = access_tracking_mode_set(&adev.ldev, 100);
    EXPECT_EQ(-EPERM, ret);

    adev.access_bit_actc_data = nullptr;
    ret = access_tracking_mode_set(&adev.ldev, ACCESS_MODE_AND);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(ACCESS_MODE_AND, adev.mode);
}

extern "C" void access_print_acpi_mem(void);
TEST_F(AccessTrackingTest, access_print_acpi_mem)
{
    access_print_acpi_mem();
}

extern "C" int actc_buffer_reinit(struct access_tracking_dev *adev);
TEST_F(AccessTrackingTest, actc_buffer_reinit)
{
    struct access_tracking_dev adev;
    MOCKER(calc_access_len_v2).stubs().will(returnValue((u64)1));
    adev.page_count = 1;
    adev.access_bit_actc_data = nullptr;
    int ret = actc_buffer_reinit(&adev);
    EXPECT_EQ(0, ret);

    GlobalMockObject::verify();
    MOCKER(calc_access_len_v2).stubs().will(returnValue((u64)0));
    adev.page_count = 2;
    MOCKER(vfree).stubs();
    ret = actc_buffer_reinit(&adev);
    EXPECT_EQ(0, ret);
}

TEST_F(AccessTrackingTest, actc_buffer_reinit_two)
{
    struct access_tracking_dev adev;
    adev.page_count = 2;
    MOCKER(calc_access_len_v2).stubs().will(returnValue((u64)1));
    MOCKER(vfree).stubs();
    MOCKER(vzalloc).stubs().will(returnValue((void *)nullptr));
    int ret = actc_buffer_reinit(&adev);
    EXPECT_EQ(-ENOMEM, ret);

    GlobalMockObject::verify();
    MOCKER(calc_access_len_v2).stubs().will(returnValue((u64)1));
    MOCKER(vfree).stubs();
    ret = actc_buffer_reinit(&adev);
    EXPECT_EQ(0, ret);
    vfree(adev.access_bit_actc_data);
}

extern "C" int access_tracking_reinit_actc_buffer(struct device *ldev);
TEST_F(AccessTrackingTest, access_tracking_reinit_actc_buffer)
{
    struct access_tracking_dev adev;
    MOCKER(actc_buffer_reinit).stubs().will(returnValue(0));
    int ret = access_tracking_reinit_actc_buffer(&adev.ldev);
    EXPECT_EQ(0, ret);
}

extern "C" int access_tracking_set_page_size(struct device *ldev, u8 page_size_index);
TEST_F(AccessTrackingTest, access_tracking_set_page_size)
{
    struct access_tracking_dev adev;
    int ret = access_tracking_set_page_size(&adev.ldev, 5);
    EXPECT_EQ(-EINVAL, ret);

    MOCKER(actc_buffer_reinit).stubs().will(returnValue(-1));
    ret = access_tracking_set_page_size(&adev.ldev, PAGE_MODE_2M);
    EXPECT_EQ(-1, ret);
}

TEST_F(AccessTrackingTest, access_tracking_set_page_size_two)
{
    struct access_tracking_dev adev;
    MOCKER(actc_buffer_reinit).stubs().will(returnValue(0));
    int ret = access_tracking_set_page_size(&adev.ldev, PAGE_MODE_2M);
    EXPECT_EQ(0, ret);
}

extern "C" int access_tracking_ram_change(struct device *ldev, void __user *argp);
TEST_F(AccessTrackingTest, access_tracking_ram_change)
{
    MOCKER(drivers_ram_changed).stubs().will(returnValue(true));
    MOCKER(copy_to_user).stubs().will(returnValue(1UL));
    int ret = access_tracking_ram_change(nullptr, nullptr);
    EXPECT_EQ(-EFAULT, ret);

    GlobalMockObject::verify();
    MOCKER(drivers_ram_changed).stubs().will(returnValue(true));
    MOCKER(copy_to_user).stubs().will(returnValue(0UL));
    ret = access_tracking_ram_change(nullptr, nullptr);
    EXPECT_EQ(0, ret);
}

extern "C" int calc_access_len(struct access_tracking_dev *adev);
TEST_F(AccessTrackingTest, calc_access_len)
{
    struct access_tracking_dev adev;
    adev.page_size_mode = PAGE_MODE_2M;
    MOCKER(get_node_actc_len).stubs().will(returnValue((u64)0));
    int ret = calc_access_len(&adev);
    EXPECT_EQ(0, ret);
}

extern "C" int actc_buffer_init(struct access_tracking_dev *adev);
TEST_F(AccessTrackingTest, actc_buffer_init)
{
    u16 buf;
    struct access_tracking_dev adev;
    MOCKER(calc_access_len_v2).stubs().will(returnValue((u64)0));
    int ret = actc_buffer_init(&adev);
    EXPECT_EQ(0, ret);

    GlobalMockObject::verify();
    MOCKER(calc_access_len_v2).stubs().will(returnValue((u64)1));
    MOCKER(vzalloc).stubs().will(returnValue((void *)nullptr));
    ret = actc_buffer_init(&adev);
    EXPECT_EQ(-ENOMEM, ret);

    GlobalMockObject::verify();
    MOCKER(calc_access_len_v2).stubs().will(returnValue((u64)1));
    MOCKER(vzalloc).stubs().will(returnValue((void *)nullptr));
    MOCKER(vfree).stubs();
    ret = actc_buffer_init(&adev);
    EXPECT_EQ(-ENOMEM, ret);
}

extern "C" void actc_buffer_deinit(struct access_tracking_dev *adev);
TEST_F(AccessTrackingTest, actc_buffer_deinit)
{
    struct access_tracking_dev adev;
    adev.page_count = 1;
    MOCKER(vfree).stubs();
    actc_buffer_deinit(&adev);
    EXPECT_EQ(0, adev.page_count);
}

extern "C" int access_tracking_add(void);
TEST_F(AccessTrackingTest, access_tracking_add)
{
    MOCKER(kzalloc).stubs().will(returnValue((void *)nullptr));
    int ret = access_tracking_add();
    EXPECT_EQ(-ENODEV, ret);

    GlobalMockObject::verify();
    MOCKER(actc_buffer_init).stubs().will(returnValue(1));
    ret = access_tracking_add();
    EXPECT_EQ(-ENODEV, ret);

    GlobalMockObject::verify();
    MOCKER(actc_buffer_init).stubs().will(returnValue(0));
    MOCKER(device_add).stubs().will(returnValue(1));
    ret = access_tracking_add();
    EXPECT_EQ(-ENODEV, ret);
}

TEST_F(AccessTrackingTest, access_tracking_add_two)
{
    int ret = 0;
    struct access_tracking_dev *adev;
    struct access_tracking_dev *n;
    MOCKER(actc_buffer_init).stubs().will(returnValue(0));
    MOCKER(tracking_dev_add).stubs().will(returnValue((struct tracking_dev *)nullptr));
    MOCKER(kfree).stubs();
    ret = access_tracking_add();
    EXPECT_EQ(-ENODEV, ret);
}

extern "C" void adev_buffer_down_read(void);
extern "C" void adev_buffer_up_read(void);
extern "C" void drivers_work_func(struct work_struct *work);
TEST_F(AccessTrackingTest, drivers_work_func)
{
    struct access_pid ap;
    struct access_tracking_dev adev;

    EXPECT_EQ(true, list_empty(&access_dev));
    list_add(&adev.list, &access_dev);
    adev.page_size_mode = PAGE_MODE_2M;
    ap.cur_times = 0;
    ap.ntimes = 10;
    ap.pid = 1;
    MOCKER(scan_accessed_bit_forward_vm).stubs().will(returnValue(0));
    drivers_work_func(&ap.scan_work.work);
    EXPECT_EQ(1, ap.cur_times);

    ap.ntimes = 1;
    ap.type = STATISTIC_SCAN;
    MOCKER(access_walk_pagemap).stubs();
    drivers_work_func(&ap.scan_work.work);
    EXPECT_EQ(2, ap.cur_times);
    list_del(&adev.list);
}

extern "C" void drivers_work_func(struct work_struct *work);
extern "C" int __init access_tracking_init(void);
TEST_F(AccessTrackingTest, access_tracking_init)
{
    int ret;

    MOCKER(init_acpi_mem).stubs().will(returnValue(2));
    ret = access_tracking_init();
    EXPECT_EQ(2, ret);

    GlobalMockObject::verify();
    MOCKER(init_acpi_mem).stubs().will(returnValue(0));
    MOCKER(drivers_refresh_remote_ram).stubs().will(returnValue(3));
    MOCKER(reset_acpi_mem).stubs();
    ret = access_tracking_init();
    EXPECT_EQ(3, ret);
}

extern "C" void release_remote_ram(void);
extern "C" int create_scan_workqueue(void);
TEST_F(AccessTrackingTest, access_tracking_init_two)
{
    MOCKER(init_acpi_mem).stubs().will(returnValue(0));
    MOCKER(drivers_refresh_remote_ram).stubs().will(returnValue(0));
    MOCKER(release_remote_ram).stubs();
    MOCKER(reset_acpi_mem).stubs();
    drivers_smap_scene = NORMAL_SCENE;
    int ret = access_tracking_init();
    EXPECT_EQ(-EINVAL, ret);

    struct ram_segment *seg = (struct ram_segment *)malloc(sizeof(struct ram_segment));
    INIT_LIST_HEAD(&seg->node);
    seg->numa_node = 1;
    seg->start = 1;
    seg->end = 10;
    list_add(&seg->node, &drivers_remote_ram_list);
    MOCKER(access_ioctl_init).stubs().will(returnValue(0));
    MOCKER(access_tracking_add).stubs().will(returnValue(0));
    MOCKER(create_scan_workqueue).stubs().will(returnValue(0));
    MOCKER(hist_actc_data_init).stubs().will(returnValue(0));
    MOCKER(access_print_acpi_mem).stubs();
    ret = access_tracking_init();
    EXPECT_EQ(0, ret);
    list_del(&seg->node);
    free(seg);
}

extern "C" void release_remote_ram(void);
extern "C" void memory_notifier_exit(void);
extern "C" void hist_actc_data_deinit(void);
extern "C" void __exit access_tracking_exit(void);
extern "C" void destroy_scan_workqueue(void);
TEST_F(AccessTrackingTest, access_tracking_exit)
{
    struct access_tracking_dev adev;
    list_add(&adev.list, &access_dev);
    MOCKER(destroy_scan_workqueue).stubs();
    MOCKER(tracking_dev_remove).stubs();
    MOCKER(vfree).stubs();
    MOCKER(kfree).stubs();
    MOCKER(release_remote_ram).stubs();
    MOCKER(reset_acpi_mem).stubs();
    MOCKER(memory_notifier_exit).stubs();
    MOCKER(hist_actc_data_deinit).stubs();
    access_tracking_exit();
    list_del(&adev.list);
}