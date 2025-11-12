/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: SMAP3.0 hist_tracking.c test code
 * Author: z30062841
 * Create: 2024-12-28
 */

#include <asm/types.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/ktime.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/container_of.h>

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include "check.h"
#include "bus.h"
#include "hist_ops.h"
#include "access_iomem.h"
#include "access_acpi_mem.h"
#include "access_tracking.h"
#include "access_iomem.h"
#include "hist_tracking.h"

using namespace std;

#define to_hist_tracking_dev(n) container_of(n, struct hist_tracking_dev, ldev)
extern "C" struct list_head g_hist_tracking_dev;
extern "C" struct list_head drivers_remote_ram_list;

class HistTrackingTest : public ::testing::Test {
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

extern "C" struct smap_hist_dev g_smap_hist_dev;
extern "C" void hist_tracking_enable(struct device *ldev);
TEST_F(HistTrackingTest, hist_tracking_enable)
{
    struct device dev;
    struct smap_hist_dev sdev;
    MOCKER(get_hist_dev).stubs().will(returnValue(&sdev));
    hist_tracking_enable(&dev);
    EXPECT_EQ(true, g_smap_hist_dev.thread_enable);
}

extern "C" void hist_tracking_disable(struct device *ldev);
TEST_F(HistTrackingTest, hist_tracking_disable)
{
    struct device dev;
    struct smap_hist_dev sdev;
    MOCKER(get_hist_dev).stubs().will(returnValue(&sdev));
    hist_tracking_disable(&dev);
    EXPECT_EQ(false, g_smap_hist_dev.thread_enable);
}

extern "C" void drivers_actc_buffer_deinit(struct hist_tracking_dev *hdev);
TEST_F(HistTrackingTest, actc_buffer_deinit)
{
    struct hist_tracking_dev dev = {
        .page_count = 1
    };
    dev.hist_actc_data = (u16 *)malloc(dev.page_count * sizeof(u16));
    drivers_actc_buffer_deinit(&dev);
    EXPECT_EQ(0, dev.page_count);
}

extern "C" void drivers_reset_actc_data(struct hist_tracking_dev *hdev);
TEST_F(HistTrackingTest, reset_actc_data)
{
    struct hist_tracking_dev dev = {
        .page_count = 1
    };
    dev.hist_actc_data = (u16 *)malloc(dev.page_count * sizeof(u16));
    drivers_reset_actc_data(&dev);
    EXPECT_EQ(0, dev.hist_actc_data[0]);
}

extern "C" int drivers_get_page_size(struct hist_tracking_dev *hdev);
TEST_F(HistTrackingTest, get_page_size)
{
    int ret = 0;
    struct hist_tracking_dev dev = {
        .page_size_mode = 0
    };
    ret = drivers_get_page_size(&dev);
    EXPECT_EQ(PAGE_SIZE_4K, ret);
}

extern int nr_local_numa;
extern "C" u64 drivers_calc_access_len(struct hist_tracking_dev *hdev);
TEST_F(HistTrackingTest, calc_access_len)
{
    u64 ret = 0;
    nr_local_numa = 1;
    struct hist_tracking_dev dev = {
        .node = 1
    };
    MOCKER(drivers_get_page_size).stubs().will(returnValue(0));
    MOCKER(get_node_page_cnt_iomem).stubs().will(returnValue((u64)1));
    ret = drivers_calc_access_len(&dev);
    EXPECT_EQ(1, ret);

    GlobalMockObject::verify();
    nr_local_numa = 3;
    MOCKER(drivers_get_page_size).stubs().will(returnValue(0));
    MOCKER(get_node_actc_len).stubs().will(returnValue((u64)2));
    ret = drivers_calc_access_len(&dev);
    EXPECT_EQ(2, ret);
}

static void hist_update_pgsize_call(u32 pgsize)
{
    return;
}

extern "C" void hist_dev_pgsize_update(u8 page_size_mode);
TEST_F(HistTrackingTest, hist_dev_pgsize_update)
{
    MOCKER(hist_update_pgsize).stubs();
    hist_dev_pgsize_update(PAGE_MODE_2M);
}

extern "C" int drivers_actc_buffer_reinit(struct hist_tracking_dev *hdev);
TEST_F(HistTrackingTest, actc_buffer_reinit)
{
    int ret;
    struct hist_tracking_dev dev = {
        .node = 0,
        .page_count = 1
    };

    MOCKER(drivers_calc_access_len).stubs().will(returnValue((u64)10));
    MOCKER(drivers_actc_buffer_deinit).stubs().will(ignoreReturnValue());
    MOCKER(hist_dev_pgsize_update).stubs().will(ignoreReturnValue());
    ret = drivers_actc_buffer_reinit(&dev);
    EXPECT_EQ(10, dev.page_count);
}

TEST_F(HistTrackingTest, actc_buffer_reinit_two)
{
    int ret;
    struct hist_tracking_dev dev = {
        .node = 0,
        .page_count = 0
    };

    MOCKER(drivers_calc_access_len).stubs().will(returnValue((u64)0));
    MOCKER(drivers_reset_actc_data).stubs().will(ignoreReturnValue());
    ret = drivers_actc_buffer_reinit(&dev);
    EXPECT_EQ(0, ret);
}

TEST_F(HistTrackingTest, actc_buffer_reinit_three)
{
    int ret;
    struct hist_tracking_dev dev = {
        .node = 0,
        .page_count = 1
    };

    MOCKER(drivers_calc_access_len).stubs().will(returnValue((u64)1));
    MOCKER(drivers_reset_actc_data).stubs().will(ignoreReturnValue());
    ret = drivers_actc_buffer_reinit(&dev);
    EXPECT_EQ(0, ret);
}

extern "C" int hist_tracking_reinit_actc_buffer(struct device *ldev);
TEST_F(HistTrackingTest, hist_tracking_reinit_actc_buffer)
{
    struct hist_tracking_dev hdev;
    MOCKER(drivers_actc_buffer_reinit).stubs().will(returnValue(1));
    int ret = hist_tracking_reinit_actc_buffer(&hdev.ldev);
    EXPECT_EQ(1, ret);

    GlobalMockObject::verify();
    MOCKER(drivers_actc_buffer_reinit).stubs().will(returnValue(0));
    ret = hist_tracking_reinit_actc_buffer(&hdev.ldev);
    EXPECT_EQ(0, ret);
}

extern "C" int hist_tracking_set_page_size(struct device *ldev, u8 pgsize);
TEST_F(HistTrackingTest, hist_tracking_set_page_size)
{
    int ret;
    struct device dev;
    ret = hist_tracking_set_page_size(&dev, 1);
    EXPECT_EQ(-EINVAL, ret);

    MOCKER(drivers_actc_buffer_reinit).stubs().will(returnValue(0));
    ret = hist_tracking_set_page_size(&dev, 0);
    EXPECT_EQ(0, ret);
}

extern "C" int hist_tracking_read(struct device *ldev, void *buffer, u32 length);
TEST_F(HistTrackingTest, hist_tracking_read)
{
    int ret;
    struct device dev;
    u32 len = 10;
    void *buffer = malloc(len);

    MOCKER(drivers_reset_actc_data).stubs().will(ignoreReturnValue());
    ret = hist_tracking_read(&dev, buffer, len);
    EXPECT_EQ(len, ret);
    free(buffer);
}

TEST_F(HistTrackingTest, hist_tracking_read_two)
{
    int ret;
    struct device dev;
    u32 len = 10;
    void *buffer = malloc(len);
    ret = hist_tracking_read(&dev, nullptr, len);
    EXPECT_EQ(-EINVAL, ret);

    ret = hist_tracking_read(&dev, buffer, 0);
    EXPECT_EQ(-EINVAL, ret);
    free(buffer);
}

extern "C" int drivers_actc_buffer_init(struct hist_tracking_dev *hdev);
TEST_F(HistTrackingTest, actc_buffer_init)
{
    int ret;
    struct hist_tracking_dev dev;
    MOCKER(drivers_calc_access_len).stubs().will(returnValue((u64)0));
    ret = drivers_actc_buffer_init(&dev);
    EXPECT_EQ(0, ret);

    GlobalMockObject::verify();
    MOCKER(drivers_calc_access_len).stubs().will(returnValue((u64)1));
    ret = drivers_actc_buffer_init(&dev);
    EXPECT_EQ(1, dev.page_count);
}

static void fetch_hist_actc_buf_call(u16 *dst_buf, struct addr_seg *seg)
{
    dst_buf[0] = 1;
    return;
}

extern "C" void scan_hist(struct hist_tracking_dev *hdev);
TEST_F(HistTrackingTest, scan_hist)
{
    struct hist_tracking_dev hdev = {
        .node = 0,
    };
    INIT_LIST_HEAD(&drivers_remote_ram_list);
    struct ram_segment newdata1 = {
        .numa_node = 0,
        .start = 0,
        .end = 10,
    };
    list_add(&newdata1.node, &drivers_remote_ram_list);
    scan_hist(&hdev);
}

extern "C" void update_hist_actc_batch(void);
TEST_F(HistTrackingTest, update_hist_actc_batch)
{
    struct hist_tracking_dev hdev;
    INIT_LIST_HEAD(&g_hist_tracking_dev);
    list_add(&hdev.list, &g_hist_tracking_dev);
    MOCKER(scan_hist).stubs();
    update_hist_actc_batch();
    list_del(&hdev.list);
}

extern "C" void hist_tracking_deinit(void);
TEST_F(HistTrackingTest, hist_tracking_deinit)
{
    struct hist_tracking_dev *hdev = (struct hist_tracking_dev *)kmalloc(
        sizeof(struct hist_tracking_dev), GFP_KERNEL);
    hdev->page_count = 1;
    hdev->tracking_dev = (struct tracking_dev *)malloc(sizeof(struct tracking_dev));
    list_add_tail(&hdev->list, &g_hist_tracking_dev);
    MOCKER(tracking_dev_remove).stubs().will(ignoreReturnValue());
    MOCKER(drivers_actc_buffer_deinit).stubs().will(ignoreReturnValue());
    hist_tracking_deinit();
}

extern "C" int hist_tracking_init(struct smap_hist_dev *dev);
TEST_F(HistTrackingTest, hist_tracking_init)
{
    int ret;
    struct tracking_dev *trk_dev =  (struct tracking_dev *)malloc(sizeof(struct tracking_dev));
    struct smap_hist_dev dev;

    MOCKER(drivers_actc_buffer_init).stubs().will(returnValue(0));
    MOCKER(tracking_dev_add).stubs().will(returnValue(trk_dev));
    ret = hist_tracking_init(&dev);
    EXPECT_EQ(0, ret);
}

TEST_F(HistTrackingTest, hist_tracking_init_two)
{
    int ret;
    struct smap_hist_dev dev;

    MOCKER(drivers_actc_buffer_init).stubs().will(returnValue(1));
    ret = hist_tracking_init(&dev);
    EXPECT_EQ(-ENODEV, ret);

    GlobalMockObject::verify();
    MOCKER(drivers_actc_buffer_init).stubs().will(returnValue(0));
    MOCKER(tracking_dev_add).stubs().will(returnValue((struct tracking_dev *)nullptr));
    ret = hist_tracking_init(&dev);
    EXPECT_EQ(-ENODEV, ret);
}

extern "C" int __init hist_module_init(void);
TEST_F(HistTrackingTest, hist_module_init)
{
    int ret;
    struct smap_hist_dev *dev = (struct smap_hist_dev *)malloc(sizeof(struct smap_hist_dev));
    MOCKER(init_acpi_mem).stubs().will(returnValue(0));
    MOCKER(drivers_refresh_remote_ram).stubs().will(returnValue(0));
    MOCKER(hist_init).stubs().will(returnValue(0));
    MOCKER(get_hist_dev).stubs().will(returnValue(dev));
    MOCKER(hist_tracking_init).stubs().will(returnValue(0));
    ret = hist_module_init();
    EXPECT_EQ(0, ret);
    free(dev);
}

TEST_F(HistTrackingTest, hist_module_init_two)
{
    int ret;
    MOCKER(init_acpi_mem).stubs().will(returnValue(1));
    ret = hist_module_init();
    EXPECT_EQ(1, ret);
}

TEST_F(HistTrackingTest, hist_module_init_three)
{
    int ret;
    struct smap_hist_dev *dev = (struct smap_hist_dev *)malloc(sizeof(struct smap_hist_dev));
    MOCKER(init_acpi_mem).stubs().will(returnValue(0));
    MOCKER(drivers_refresh_remote_ram).stubs().will(returnValue(0));
    MOCKER(hist_init).stubs().will(returnValue(1));
    MOCKER(reset_acpi_mem).stubs().will(ignoreReturnValue());
    ret = hist_module_init();
    EXPECT_EQ(1, ret);
    GlobalMockObject::verify();

    MOCKER(init_acpi_mem).stubs().will(returnValue(0));
    MOCKER(drivers_refresh_remote_ram).stubs().will(returnValue(0));
    MOCKER(hist_init).stubs().will(returnValue(0));
    MOCKER(get_hist_dev).stubs().will(returnValue(dev));
    MOCKER(hist_tracking_init).stubs().will(returnValue(1));
    MOCKER(hist_deinit).stubs().will(ignoreReturnValue());
    MOCKER(reset_acpi_mem).stubs().will(ignoreReturnValue());
    ret = hist_module_init();
    EXPECT_EQ(1, ret);
    free(dev);
}

extern "C" void __exit hist_module_exit(void);
TEST_F(HistTrackingTest, hist_module_exit)
{
    struct smap_hist_dev *dev = (struct smap_hist_dev *)malloc(sizeof(struct smap_hist_dev));
    MOCKER(hist_tracking_deinit).stubs().will(ignoreReturnValue());
    MOCKER(hist_deinit).stubs().will(ignoreReturnValue());
    MOCKER(reset_acpi_mem).stubs().will(ignoreReturnValue());
    hist_module_exit();
}