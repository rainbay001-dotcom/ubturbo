/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: SMAP : tests for smap_hist_mid
 * Create: 2025-7-2
 */
#include <asm/types.h>
#include <linux/acpi.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/platform_device.h>

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include "smap_hist_mid.h"
#include "ub_hist.h"

using namespace std;

constexpr int DEFAULT_NUMA_NODE = 4;
constexpr uint64_t REG_BASE_CPP_ADDR_0 = 0x33030a0000;
constexpr uint64_t REG_BASE_CPP_ADDR_1 = 0x330a0a0000;
constexpr uint64_t REG_BASE_CPP_ADDR_2 = 0x73030a0000;
constexpr uint64_t REG_BASE_CPP_ADDR_3 = 0x730a0a0000;

struct ub_hist_ba_device {
    struct device *dev;
    void __iomem *base_addr;
    struct list_head list;
    struct ub_hist_ba_info info;
};

struct platform_device *pdev[DEFAULT_NUMA_NODE];

static uint64_t reg_base_cpp_addrs[] = {
    REG_BASE_CPP_ADDR_0,
    REG_BASE_CPP_ADDR_1,
    REG_BASE_CPP_ADDR_2,
    REG_BASE_CPP_ADDR_3
};

extern "C" int ub_hist_ba_init(struct ub_hist_ba_device *ba_dev);
extern "C" int ub_hist_probe(struct platform_device *pdev);
extern "C" int ub_hist_remove(struct platform_device *pdev);
class DriversUbHistMidTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        cout << "[Phase SetUp Begin]==============================" << endl;
        MOCKER(ub_hist_ba_init).stubs().will(returnValue(0));
        ub_hist_init(PLATFORM_EVB_TWO_SOCKETS);
        for (int i = 0; i < DEFAULT_NUMA_NODE; i++) {
            pdev[i] = (platform_device *)malloc(sizeof(platform_device));
            struct device dev = {
                .numa_node = i,
            };
            pdev[i]->dev = dev;
            ub_hist_probe(pdev[i]);
        }
        smap_hist_mid_init();
        cout << "[Phase SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[Phase TearDown Begin]" << endl;
        smap_hist_mid_exit();
        for (int i = 0; i < DEFAULT_NUMA_NODE; i++) {
            ub_hist_remove(pdev[i]);
            if (pdev[i]) {
                free(pdev[i]);
                pdev[i] = nullptr;
            }
        }
        ub_hist_exit();
        cout << "[Phase TearDown End]" << endl;
    }
};

TEST_F(DriversUbHistMidTest, comprehensive_blocking)
{
    struct hist_scan_cfg cfg_data;
    uint32_t result_count;
    uint32_t max_entry;
    size_t total_len;
    struct addr_count_pair *result_pair;

    smap_hist_middle_add_roi(0x40000000000ULL, 16 * GB);

    smap_hist_middle_get_scan_config(&cfg_data);
    cfg_data.scan_mode = SCAN_WITH_2M;
    cfg_data.run_mode = RUN_SYNC_BLOCKING;
    smap_hist_middle_set_scan_config(&cfg_data);

    total_len = smap_hist_middle_get_roi_total_len();
    max_entry = total_len >> SHIFT_2M;
    result_pair = (struct addr_count_pair *)malloc(sizeof(struct addr_count_pair) * max_entry);

    smap_hist_middle_scan_enable();
    smap_hist_middle_get_hot_pages(result_pair, &result_count);
    free(result_pair);
    smap_hist_middle_scan_disable();
}

TEST_F(DriversUbHistMidTest, comprehensive_threading)
{
    struct ub_hist_ba_info ba_info[4];
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(0, ub_hist_query_ba_info(reg_base_cpp_addrs[i], &ba_info[i]));
    }
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(0, smap_hist_middle_add_roi(ba_info[i].nc_range.start, 100 * MB));
        EXPECT_EQ(0, smap_hist_middle_add_roi(ba_info[i].cc_range.start, 100 * MB));
    }
    EXPECT_EQ(800 * MB, smap_hist_middle_get_roi_total_len());
    EXPECT_EQ(8, smap_hist_middle_get_roi_count());

    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(0, smap_hist_middle_del_roi(ba_info[i].nc_range.start + 20 * MB, 20 * MB));
        EXPECT_EQ(0, smap_hist_middle_del_roi(ba_info[i].cc_range.start + 20 * MB, 20 * MB));
    }

    EXPECT_EQ((800 - 160) * MB, smap_hist_middle_get_roi_total_len());
    EXPECT_EQ(16, smap_hist_middle_get_roi_count());
    struct hist_roi_data *roi_buf = (struct hist_roi_data *)malloc(sizeof(struct hist_roi_data) * 16);
    EXPECT_EQ(0, smap_hist_get_roi_info(roi_buf, 16));
    free(roi_buf);

    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(0, smap_hist_middle_del_roi(ba_info[i].nc_range.start + 10 * MB, 30 * MB));
        EXPECT_EQ(0, smap_hist_middle_del_roi(ba_info[i].cc_range.start + 10 * MB, 30 * MB));
    }
    EXPECT_EQ((800 - 160 - 80) * MB, smap_hist_middle_get_roi_total_len());
    EXPECT_EQ(16, smap_hist_middle_get_roi_count());

    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(0, smap_hist_middle_del_roi(ba_info[i].nc_range.start + 20 * MB, 30 * MB));
        EXPECT_EQ(0, smap_hist_middle_del_roi(ba_info[i].cc_range.start + 20 * MB, 30 * MB));
    }
    EXPECT_EQ((800 - 160 - 80 - 80) * MB, smap_hist_middle_get_roi_total_len());
    EXPECT_EQ(16, smap_hist_middle_get_roi_count());

    for (int i = 0; i < 4; i++) {
        GTEST_ASSERT_TRUE(smap_hist_middle_query_addr_in_roi(ba_info[i].cc_range.start + 50 * MB));
        GTEST_ASSERT_FALSE(smap_hist_middle_query_addr_in_roi(ba_info[i].cc_range.start + 30 * MB));

        EXPECT_EQ(0, smap_hist_middle_add_roi(ba_info[i].nc_range.start + 5 * MB, 70 * MB));
        EXPECT_EQ(0, smap_hist_middle_add_roi(ba_info[i].cc_range.start + 10 * MB, 80 * MB));

        GTEST_ASSERT_TRUE(smap_hist_middle_query_addr_in_roi(ba_info[i].cc_range.start + 50 * MB));
        GTEST_ASSERT_TRUE(smap_hist_middle_query_addr_in_roi(ba_info[i].cc_range.start + 30 * MB));
    }
    EXPECT_EQ(800 * MB, smap_hist_middle_get_roi_total_len());
    EXPECT_EQ(8, smap_hist_middle_get_roi_count());

    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(0, smap_hist_middle_del_roi(ba_info[i].nc_range.start, 100 * MB));
    }
    EXPECT_EQ(400 * MB, smap_hist_middle_get_roi_total_len());
    EXPECT_EQ(4, smap_hist_middle_get_roi_count());

    smap_hist_middle_reset_roi();
    EXPECT_EQ(0, smap_hist_middle_get_roi_total_len());
    EXPECT_EQ(0, smap_hist_middle_get_roi_count());
}
