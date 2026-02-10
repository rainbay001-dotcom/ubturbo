/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: segment test
 */

#include <cerrno>
#include <cstdarg>
#include <cstddef>
#include <iostream>
#include <cstring>
#include "gtest/gtest.h"
#include "linux/compiler_attributes.h"
#include "linux/device.h"
#include "linux/mm_types.h"
#include "linux/types.h"
#include "linux/dmaengine.h"
#include "linux/migrate.h"
#include "ubcore_types.h"
#include "mockcpp/ChainingMockHelper.h"
#include "mockcpp/mokc.h"
#include "stub_struct.h"
#include "urma.h"
#include "ubcore_opcode.h"

#define SMAP_MIG_MAGIC 0xbe
#define SMAP_MIG_MIGRATE _IOW(SMAP_MIG_MAGIC, 0, struct migrate_msg)
#define SMAP_MIG_FOLIO_MIGRATE _IOW(SMAP_MIG_MAGIC, 1, struct migrate_msg)

using namespace std;

class TestSegment : public ::testing::Test {
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
    int get_urma_trans_segment(struct urma_trans_segment_info *src_info, struct urma_trans_segment_info *dst_info);
    void ub_dma_free(struct ub_dma_dev *udev);
    int check_mem_info(u64 pa_start, u64 pa_end);
    int pfn_valid(unsigned long pfn);
    int pfn_to_nid(unsigned long pfn);
    int ub_dma_register_segment(u64 pa_start, u64 pa_end);
    void unregister_urma_segment(struct urma_sge_info *sge_info);
    void ub_dma_unregister_segment(void);
    int urma_meta_sge_init(void);
}

TEST_F(TestSegment, check_mem_info_test)
{
    u64 pa_start = 0;
    u64 pa_end = 0;
    struct urma_trans_segment_info src_info;
    struct urma_trans_segment_info dst_info;

    pa_start = 2; // 2
    pa_end = 0;
    int ret1 = check_mem_info(pa_start, pa_end);
    EXPECT_EQ(ret1, -EINVAL);
    pa_start = 0;
    pa_end = 3; // 3
    MOCKER(pfn_valid).stubs().will(returnValue(0));
    ret1 = check_mem_info(pa_start, pa_end);
    EXPECT_EQ(ret1, -EINVAL);
    GlobalMockObject::verify();
    MOCKER(pfn_to_nid).stubs().will(returnValue(1)).then(returnValue(0));
    ret1 = check_mem_info(pa_start, pa_end);
    EXPECT_EQ(ret1, -EINVAL);
    GlobalMockObject::verify();
    ret1 = check_mem_info(pa_start, pa_end);
    EXPECT_EQ(ret1, 0);
}

TEST_F(TestSegment, ub_dma_register_segment_test)
{
    u64 pa_start = 0;
    u64 pa_end = 3; // 3
    MOCKER(kzalloc).stubs().will(returnValue((void*)nullptr));
    MOCKER(check_mem_info).stubs().will(returnValue(0));
    int ret1 = ub_dma_register_segment(pa_start, pa_end);
    EXPECT_EQ(ret1, -ENOMEM);

    GlobalMockObject::verify();
    MOCKER(check_mem_info).stubs().will(returnValue(0));
    MOCKER(urma_register_segment).stubs().will(returnValue(1));
    ret1 = ub_dma_register_segment(pa_start, pa_end);
    EXPECT_EQ(ret1, 1);

    GlobalMockObject::verify();
    MOCKER(check_mem_info).stubs().will(returnValue(0));
    MOCKER(urma_register_segment).stubs().will(returnValue(0));
    urma_meta_sge_init();
    ret1 = ub_dma_register_segment(pa_start, pa_end);
    EXPECT_EQ(ret1, 0);

    GlobalMockObject::verify();
    MOCKER(unregister_urma_segment).stubs();
    ub_dma_unregister_segment();
}

TEST_F(TestSegment, get_urma_trans_segment_test)
{
    struct urma_trans_segment_info *src_info;
    struct urma_trans_segment_info *dst_info;
    int ret = get_urma_trans_segment(src_info, dst_info);
    EXPECT_EQ(0, ret);
}