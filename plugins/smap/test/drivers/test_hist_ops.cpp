/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: SMAP3.0 hist_ops.c test code
 * Author: z30062841
 * Create: 2024-12-28
 */
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include <asm/types.h>
#include <asm/io.h>
#include <linux/byteorder/generic.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include "access_iomem.h"
#include "smap_hist_mid.h"
#include "ub_hist.h"
#include "hist_ops.h"

using namespace std;
class HistOpsTest : public ::testing::Test {
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

extern list_head drivers_remote_ram_list;
extern "C" struct smap_hist_dev g_smap_hist_dev;

extern "C" u64 align_addr(u64 addr, u32 low_bit_len);
TEST_F(HistOpsTest, align_addr)
{
    u64 ret;
    u64 addr = 0x1001;
    ret = align_addr(addr, 2);
    EXPECT_EQ(0x1000, ret);
}

extern "C" bool addr_is_aligned(u64 addr, u32 low_bit_len);
TEST_F(HistOpsTest, addr_is_aligned)
{
    u64 addr = 0x1000;
    EXPECT_EQ(true, addr_is_aligned(addr, 2));

    u64 addr1 = 0x1001;
    EXPECT_EQ(false, addr_is_aligned(addr1, 2));
}

extern "C" u64 align_addr_by_sts_size(u64 addr, u8 sts_size);
TEST_F(HistOpsTest, align_addr_by_sts_size)
{
    u64 ret;
    u64 size = 0x1000;
    MOCKER(align_addr).stubs().will(returnValue(size));
    ret = align_addr_by_sts_size(0x1, STS_SIZE_4K);
    EXPECT_EQ(size, ret);

    ret = align_addr_by_sts_size(0x1, STS_SIZE_2M);
    EXPECT_EQ(size, ret);
}

extern "C" bool addr_seg_is_valid(struct segs_info *info);
TEST_F(HistOpsTest, addr_seg_is_valid)
{
    bool ret;
    struct addr_seg seg = {
        .start = 0x100000,
        .size = 0x200000
    };

    struct segs_info seg_info = {
        .cnt = 1,
        .segs = &seg
    };

    MOCKER(addr_is_aligned).stubs().will(returnValue(true));
    MOCKER(addr_is_aligned).stubs().will(returnValue(true));
    ret = addr_seg_is_valid(&seg_info);
    EXPECT_EQ(true, ret);

    GlobalMockObject::verify();
    MOCKER(addr_is_aligned).stubs().will(returnValue(false));
    ret = addr_seg_is_valid(&seg_info);
    EXPECT_EQ(false, ret);
}

TEST_F(HistOpsTest, addr_seg_is_valid_two)
{
    bool ret;
    struct segs_info seg_info1 = {
        .cnt = 0,
    };

    struct segs_info seg_info2 = {
        .cnt = 1,
    };

    ret = addr_seg_is_valid(&seg_info1);
    EXPECT_EQ(false, ret);

    ret = addr_seg_is_valid(&seg_info2);
    EXPECT_EQ(false, ret);
}

extern "C" int addr_seg_cmp_start(const void *seg1, const void *seg2);
TEST_F(HistOpsTest, addr_seg_cmp_start)
{
    int ret;
    struct addr_seg s1 = {
        .start = 0x10,
    };
    struct addr_seg s2 = {
        .start = 0,
    };
    ret = addr_seg_cmp_start(&s1, &s2);
    EXPECT_EQ(1, ret);
}

extern "C" int addr_seg_cmp_max(const void *win1, const void *win2);
TEST_F(HistOpsTest, addr_seg_cmp_max)
{
    int ret;
    struct addr_seg s1 = {
        .max = 0,
    };
    struct addr_seg s2 = {
        .max = 1,
    };
    ret = addr_seg_cmp_max(&s1, &s2);
    EXPECT_EQ(1, ret);
}

extern "C" bool addr_seg_is_continuous_16g(struct addr_seg *seg1, struct addr_seg *seg2);
TEST_F(HistOpsTest, addr_seg_is_continuous_16g)
{
    bool ret;
    struct addr_seg s1 = {
        .start = 0,
        .size = 0x10,
    };
    struct addr_seg s2 = {
        .start = 0,
        .size = 0x100,
    };
    ret = addr_seg_is_continuous_16g(&s1, &s2);
    EXPECT_EQ(false, ret);
}

TEST_F(HistOpsTest, addr_seg_is_continuous_16g_two)
{
    bool ret;
    struct addr_seg s1 = {
        .start = 0,
        .size = 0x10,
    };
    struct addr_seg s2 = {
        .start = 0x400000001,
        .size = 0x100,
    };

    ret = addr_seg_is_continuous_16g(&s1, &s2);
    EXPECT_EQ(true, ret);
}

TEST_F(HistOpsTest, addr_seg_is_continuous_16g_three)
{
    bool ret;
    struct addr_seg s1 = {
        .start = 0x100,
        .size = 0x400000000,
    };
    struct addr_seg s2 = {
        .start = 0x400000000,
        .size = 0x100,
    };
    ret = addr_seg_is_continuous_16g(&s1, &s2);
    EXPECT_EQ(false, ret);
}

extern "C" u64 seg_end(struct addr_seg *seg);
TEST_F(HistOpsTest, seg_end)
{
    u64 ret;
    struct addr_seg seg = {
        .start = 0,
        .size = 0x1001,
    };
    ret = seg_end(&seg);
    EXPECT_EQ(0x1000, ret);
}

extern "C" int count_nr_windows(struct addr_seg *segs, int len, u8 sts_size);
TEST_F(HistOpsTest, count_nr_windows)
{
    int ret;
    int cnt = 3;
    struct addr_seg *seg = (struct addr_seg *)malloc(sizeof(struct addr_seg) * cnt);
    for (int i = 0; i < cnt; i++) {
        seg[i].start = i * 0x1000000;
        seg[i].size = 0x1000000;
    }
    ret = count_nr_windows(seg, cnt, STS_SIZE_4K);
    EXPECT_EQ(cnt, ret);
}

extern "C" void align_segments(struct addr_seg *segs, int cnt, u8 sts_size);
TEST_F(HistOpsTest, align_segments)
{
    struct addr_seg *seg = (struct addr_seg *)malloc(sizeof(struct addr_seg));
    seg[0].start = 0x1000;
    seg[0].size = 0x1000;
    MOCKER(count_nr_windows).stubs().will(returnValue(0));
    MOCKER(align_addr).stubs().will(returnValue((u64)0));
    align_segments(seg, 1, STS_SIZE_2M);
    EXPECT_EQ(0, seg[0].size);
    free(seg);
}

extern "C" int merge_segments(struct addr_seg *segs, int cnt);
TEST_F(HistOpsTest, merge_segments)
{
    int ret;
    int cnt = 2;
    struct addr_seg *seg = (struct addr_seg *)malloc(sizeof(struct addr_seg) * cnt);
    for (int i = 0; i < cnt; i++) {
        seg[i].start = i * 0x1000;
        seg[i].size = 0x1000;
    }
    MOCKER(addr_seg_is_continuous_16g).stubs().will(returnValue(true));
    ret = merge_segments(seg, cnt);
    EXPECT_EQ(1, ret);

    GlobalMockObject::verify();
    MOCKER(addr_seg_is_continuous_16g).stubs().will(returnValue(false));
    ret = merge_segments(seg, cnt);
    EXPECT_EQ(2, ret);
    free(seg);
}

extern "C" bool is_intersect(struct addr_seg *seg1, struct addr_seg *seg2, u64 *inter_start, u64 *inter_end);
TEST_F(HistOpsTest, is_intersect)
{
    bool ret;
    u64 inter_start;
    u64 inter_end;
    struct addr_seg seg1 = {
        .start = 0,
        .size = 0x1000
    };
    struct addr_seg seg2 = {
        .start = 0x10,
        .size = 0x1000
    };
    struct addr_seg seg3 = {
        .start = 0x10000,
        .size = 0x1000
    };
    ret = is_intersect(&seg1, &seg2, &inter_start, &inter_end);
    EXPECT_EQ(true, ret);

    ret = is_intersect(&seg1, &seg3, &inter_start, &inter_end);
    EXPECT_EQ(false, ret);
}

extern "C" void calc_32m_hot_wins(struct segs_info *info, u16 *buf, struct segs_info *win_info);
TEST_F(HistOpsTest, calc_32m_hot_wins)
{
    int i;
    struct addr_seg *segs = (struct addr_seg *)malloc(sizeof(struct addr_seg));
    segs[0].start = 0;
    segs[0].size = 0x1400000;
    struct segs_info info = {
        .cnt = 1,
        .segs = segs,
    };
    struct segs_info win_info = {
        .cnt = 2,
    };
    struct addr_seg *wins = (struct addr_seg *)malloc(sizeof(struct addr_seg) * win_info.cnt);
    win_info.segs = wins;
    u16 *buf = (u16 *)malloc(sizeof(u16) * 10);
    for (i = 0; i < 10; i++) {
        buf[i] = i;
    }
    calc_32m_hot_wins(&info, buf, &win_info);
    EXPECT_EQ(0, wins[0].start);
    free(segs);
    free(wins);
    free(buf);
}

extern "C" int generate_aligned_16gb_wins_info(struct segs_info *win_info,
    struct segs_info *info);
TEST_F(HistOpsTest, generate_aligned_16gb_wins_info)
{
    int ret;
    struct segs_info win_info;
    struct addr_seg *segs = (struct addr_seg *)malloc(sizeof(struct addr_seg));
    segs[0].start = 0;
    segs[0].size = 0x14000000;
    struct segs_info info = {
        .cnt = 1,
        .segs = segs,
    };
    ret = generate_aligned_16gb_wins_info(&win_info, &info);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, win_info.cnt);
    EXPECT_EQ(0, win_info.segs->start);
    EXPECT_EQ(16 * GB, win_info.segs->size);
    free(segs);
}

extern "C" int generate_aligned_32m_wins_info(struct segs_info *win_info,
    struct segs_info *info, u16 *buf);
TEST_F(HistOpsTest, generate_aligned_32m_wins_info)
{
    int ret;
    int buf_len = 10;
    struct segs_info win_info;
    struct segs_info info = {
        .cnt = 1,
    };
    struct addr_seg *segs = (struct addr_seg *)malloc(sizeof(struct addr_seg));
    segs[0].start = 0;
    segs[0].size = 0x14000000;
    info.segs = segs;
    u16 *buffer = (u16 *)malloc(sizeof(u16) * buf_len);
    MOCKER(count_nr_windows).stubs().will(returnValue(1));
    MOCKER(calc_32m_hot_wins).stubs().will(ignoreReturnValue());
    ret = generate_aligned_32m_wins_info(&win_info, &info, buffer);
    EXPECT_EQ(0, ret);
}

extern "C" int smap_hist_middle_read(u64 start, u64 size, struct addr_count_pair *actc_pair, int *pair_cnt);
TEST_F(HistOpsTest, smap_hist_middle_read)
{
    int ret;

    g_smap_hist_dev.abort_flag = 1;
    MOCKER(smap_hist_middle_reset_roi).stubs().will(returnValue(0));
    MOCKER(smap_hist_middle_add_roi).stubs().will(returnValue(0));
    MOCKER(smap_hist_middle_scan_enable).stubs().will(returnValue(0));
    MOCKER(smap_hist_middle_get_hot_pages).stubs().will(returnValue(0));
    MOCKER(smap_hist_middle_scan_disable).stubs().will(returnValue(0));
    ret = smap_hist_middle_read(0, 1, nullptr, nullptr);
    EXPECT_EQ(-EAGAIN, ret);

    g_smap_hist_dev.abort_flag = 0;
    ret = smap_hist_middle_read(0, 1, nullptr, nullptr);
    EXPECT_EQ(0, ret);
}

extern "C" int do_hist_scan_sliding(struct segs_info *info, u32 scan_time, u16 *buf,
    u32 buf_len, u8 sts_size);
extern "C" int smap_hist_middle_set_scan_config(struct hist_scan_cfg *cfg_data);
extern "C" void copy_actc_to_buf(struct segs_info *info, struct addr_seg *seg, u16 *dst_buf,
    struct addr_count_pair *actc_pair, u32 buf_len, u8 sts_size);
TEST_F(HistOpsTest, do_hist_scan_sliding)
{
    int ret;
    u32 t = 100;
    u16 *buffer = (u16 *)malloc(sizeof(u16));
    struct segs_info info = {
        .cnt = 1,
    };
    struct addr_seg *seg = (struct addr_seg *)malloc(sizeof(struct addr_seg));
    seg[0].start = 0;
    seg[0].size = 0x100000;
    info.segs = seg;
    MOCKER(generate_aligned_16gb_wins_info).stubs()
        .with(outBoundP(&info, sizeof(info)), any()).will(returnValue(0));
    MOCKER(smap_hist_middle_set_scan_config).stubs().will(returnValue(0));
    MOCKER(smap_hist_middle_read).stubs().will(returnValue(0));
    MOCKER(copy_actc_to_buf).stubs();
    ret = do_hist_scan_sliding(&info, t, buffer, 1, STS_SIZE_2M);
    EXPECT_EQ(0, ret);

    GlobalMockObject::verify();
    MOCKER(generate_aligned_32m_wins_info).stubs().will(returnValue(-ENOMEM));
    ret = do_hist_scan_sliding(&info, t, buffer, 1, STS_SIZE_4K);
    EXPECT_EQ(-ENOMEM, ret);
    free(buffer);
}

extern "C" int hist_scan_sliding(struct segs_info *info, u32 scan_time_total, u16 *buf, u32 buf_len, u32 pgsize);
TEST_F(HistOpsTest, hist_scan_sliding)
{
    int ret;
    u32 buf_len = 2;
    u16 *buf = (u16 *)malloc(sizeof(u16) * buf_len);
    struct addr_seg *segs = (struct addr_seg *)malloc(sizeof(struct addr_seg));
    segs[0].start = 0;
    segs[0].size = 0x14000000;
    struct segs_info info = {
        .cnt = 1,
        .segs = segs,
    };

    MOCKER(addr_seg_is_valid).stubs().will(returnValue(false));
    ret = hist_scan_sliding(&info, 200, nullptr, 0, SIZE_4K);
    EXPECT_EQ(-EINVAL, ret);
    GlobalMockObject::verify();
    MOCKER(addr_seg_is_valid).stubs().will(returnValue(true));
    MOCKER(do_hist_scan_sliding).stubs().will(returnValue(0));
    MOCKER(do_hist_scan_sliding).stubs().will(returnValue(0));
    ret = hist_scan_sliding(&info, 200, buf, buf_len, SIZE_4K);
    EXPECT_EQ(0, ret);
    free(segs);
    free(buf);
}

TEST_F(HistOpsTest, hist_scan_sliding_two)
{
    int ret;
    struct addr_seg *segs = (struct addr_seg *)malloc(sizeof(struct addr_seg));
    segs[0].start = 0;
    segs[0].size = 0x14000000;
    struct segs_info info = {
        .cnt = 1,
        .segs = segs,
    };

    MOCKER(addr_seg_is_valid).stubs().will(returnValue(true));
    ret = hist_scan_sliding(&info, 0, nullptr, 0, SIZE_4K);
    EXPECT_EQ(-EINVAL, ret);

    GlobalMockObject::verify();
    MOCKER(addr_seg_is_valid).stubs().will(returnValue(true));
    ret = hist_scan_sliding(&info, 100, nullptr, 0, SIZE_4K);
    EXPECT_EQ(-EINVAL, ret);
    free(segs);
}

extern "C" void add_to_actc_data(u16 *dst, u16 *src, int len);
TEST_F(HistOpsTest, add_to_actc_data)
{
    u16 dst;
    u16 src;
    dst = 1;
    src = 1;
    add_to_actc_data(&dst, &src, 1);
    EXPECT_EQ(2, dst);

    src = U16_MAX;
    add_to_actc_data(&dst, &src, 1);
    EXPECT_EQ(U16_MAX, dst);
}

extern "C" void fetch_hist_actc_buf(u16 *dst_buf, struct addr_seg *seg);
TEST_F(HistOpsTest, fetch_hist_actc_buf)
{
    g_smap_hist_dev.pgsize = SIZE_2M;
    struct addr_seg *segs = (struct addr_seg *)malloc(sizeof(struct addr_seg));
    segs[0].start = 0;
    segs[0].size = 0xA00000;

    g_smap_hist_dev.info = {
        .cnt = 1,
        .segs = segs,
    };

    g_smap_hist_dev.status.status_all = 0;
    struct addr_seg seg = {
        .start = 0,
        .size = 0x400000,
    };
    u16 *dst_buf = (u16 *)malloc(sizeof(u16) * 2);
    MOCKER(is_intersect).stubs().will(returnValue(true));
    MOCKER(add_to_actc_data).stubs();
    fetch_hist_actc_buf(dst_buf, &seg);
    free(dst_buf);
    free(segs);
}

extern "C" void hist_update_pgsize(u32 pgsize);
TEST_F(HistOpsTest, hist_update_pgsize)
{
    g_smap_hist_dev.pgsize = SIZE_4K;
    hist_update_pgsize(SIZE_2M);
    EXPECT_EQ(SIZE_2M, g_smap_hist_dev.status.flag.new_pgsize);
}

extern "C" void hist_set_iomem(void);
TEST_F(HistOpsTest, hist_set_iomem)
{
    g_smap_hist_dev.status.flag.should_update_iomem = 0;
    hist_set_iomem();
    EXPECT_EQ(1, g_smap_hist_dev.status.flag.should_update_iomem);
}

extern "C" void flush_actc_data(struct smap_hist_dev *dev);
TEST_F(HistOpsTest, flush_actc_data)
{
    struct smap_hist_dev dev;
    dev.flush_actc = 0;
    flush_actc_data(&dev);
}

extern "C" unsigned int get_cpu_socket_count(void);
TEST_F(HistOpsTest, get_cpu_socket_count)
{
    unsigned int ret = get_cpu_socket_count();
    EXPECT_EQ(2, ret);
}

extern int nr_local_numa;
extern "C" int addr_segs_init(struct smap_hist_dev *dev, u32 pgsize);
TEST_F(HistOpsTest, addr_segs_init)
{
    int ret;
    struct smap_hist_dev dev;
    INIT_LIST_HEAD(&drivers_remote_ram_list);
    nr_local_numa = 1;
    struct ram_segment newdata1 = {
        .numa_node = 0,
        .start = 0,
        .end = 0x100,
    };

    struct ram_segment newdata2 = {
        .numa_node = 1,
        .start = 0x1000,
        .end = 0x401000,
    };
    list_add(&newdata1.node, &drivers_remote_ram_list);
    list_add(&newdata2.node, &drivers_remote_ram_list);
    ret = addr_segs_init(&dev, SIZE_2M);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(2, dev.pgcount);

    MOCKER(vzalloc).stubs().will(returnValue((void *)NULL));
    ret = addr_segs_init(&dev, SIZE_2M);
    EXPECT_EQ(-ENOMEM, ret);
}

extern "C" void addr_segs_deinit(struct smap_hist_dev *dev);
TEST_F(HistOpsTest, addr_segs_deinit)
{
    struct segs_info info = {
        .cnt = 1,
    };
    info.segs =(struct addr_seg *)malloc(sizeof(struct addr_seg) * info.cnt);
    struct smap_hist_dev dev = {
        .info = info,
    };
    addr_segs_deinit(&dev);
    EXPECT_EQ(NULL, dev.info.segs);
}

extern "C" void hist_buffer_deinit(struct smap_hist_dev *dev);
extern "C" int hist_buffer_init(struct smap_hist_dev *dev);
TEST_F(HistOpsTest, hist_buffer_init)
{
    int ret;
    struct smap_hist_dev dev = {
        .pgcount = 1,
    };
    ret = hist_buffer_init(&dev);
    EXPECT_EQ(0, ret);

    hist_buffer_deinit(&dev);
    EXPECT_EQ(NULL, dev.buf);
    u16 *buf = (u16 *)malloc(sizeof(u16) * dev.pgcount);
    MOCKER(vzalloc).stubs().will(returnValue(static_cast<void *>(buf))).then(returnValue(static_cast<void *>(nullptr)));
    ret = hist_buffer_init(&dev);
    EXPECT_EQ(0, ret);
}

TEST_F(HistOpsTest, hist_buffer_initTwo)
{
    int ret;
    struct smap_hist_dev dev = {
        .pgcount = 1,
    };
    MOCKER(vzalloc).stubs().will(returnValue(static_cast<void *>(nullptr)));
    ret = hist_buffer_init(&dev);
    EXPECT_EQ(-ENOMEM, ret);
}

TEST_F(HistOpsTest, hist_buffer_deinit)
{
    struct smap_hist_dev dev = {
        .pgcount = 1,
    };
    dev.buf = (u16 *)malloc(sizeof(u16) * dev.pgcount);
    hist_buffer_deinit(&dev);
    EXPECT_EQ(NULL, dev.buf);
}

extern "C" int hist_pginfo_reinit(struct smap_hist_dev *dev, u32 pgsize_new);
TEST_F(HistOpsTest, hist_pginfo_reinit)
{
    int ret;
    struct smap_hist_dev dev = {
        .pgsize = SIZE_2M,
    };
    MOCKER(addr_segs_deinit).stubs().will(ignoreReturnValue());
    MOCKER(addr_segs_init).stubs().will(returnValue(1));
    ret = hist_pginfo_reinit(&dev, 0);
    EXPECT_EQ(-ENOMEM, ret);

    GlobalMockObject::verify();
    MOCKER(addr_segs_deinit).stubs().will(ignoreReturnValue());
    MOCKER(addr_segs_init).stubs().will(returnValue(0));
    MOCKER(hist_buffer_deinit).stubs().will(ignoreReturnValue());
    MOCKER(hist_buffer_init).stubs().will(returnValue(1));
    MOCKER(addr_segs_deinit).stubs().will(ignoreReturnValue());
    ret = hist_pginfo_reinit(&dev, 0);
    EXPECT_EQ(-ENOMEM, ret);
}

extern "C" int scan_thread_run(void *data);
TEST_F(HistOpsTest, scan_thread_run)
{
    int ret;
    MOCKER(kthread_should_stop).stubs().will(returnValue(false)).then(returnValue(true));
    MOCKER(drivers_ram_changed).stubs().will(returnValue(false));

    g_smap_hist_dev.status.status_all = 1;
    g_smap_hist_dev.thread_enable = 1;
    MOCKER(hist_pginfo_reinit).stubs().will(returnValue(0));
    MOCKER(hist_scan_sliding).stubs().will(returnValue(0));
    MOCKER(add_to_actc_data).stubs().will(ignoreReturnValue());
    ret = scan_thread_run(nullptr);
    EXPECT_EQ(0, ret);
}

TEST_F(HistOpsTest, scan_thread_run_two)
{
    int ret;
    MOCKER(kthread_should_stop).stubs().will(returnValue(false)).then(returnValue(true));
    MOCKER(drivers_ram_changed).stubs().will(returnValue(false));

    g_smap_hist_dev.status.status_all = 1;
    MOCKER(hist_pginfo_reinit).stubs().will(returnValue(1));
    ret = scan_thread_run(nullptr);
    EXPECT_EQ(0, ret);
}

TEST_F(HistOpsTest, scan_thread_run_three)
{
    int ret;
    MOCKER(kthread_should_stop).stubs().will(returnValue(false)).then(returnValue(true));
    MOCKER(drivers_ram_changed).stubs().will(returnValue(false));

    g_smap_hist_dev.status.status_all = 1;
    g_smap_hist_dev.thread_enable = 1;
    MOCKER(hist_pginfo_reinit).stubs().will(returnValue(0));
    MOCKER(hist_scan_sliding).stubs().will(returnValue(-EINVAL));
    ret = scan_thread_run(nullptr);
    EXPECT_EQ(0, ret);
}

extern "C" int scan_thread_init(struct smap_hist_dev *dev);
TEST_F(HistOpsTest, scan_thread_init)
{
    int ret;
    struct smap_hist_dev dev;
    ret = scan_thread_init(&dev);
    EXPECT_EQ(-ECHILD, ret);
    EXPECT_EQ(nullptr, dev.kthread);
}

extern "C" void scan_thread_deinit(struct smap_hist_dev *dev);
TEST_F(HistOpsTest, scan_thread_deinit)
{
    struct smap_hist_dev dev;
    dev.kthread = (struct task_struct *)malloc(sizeof(struct task_struct));
    scan_thread_deinit(&dev);
    EXPECT_EQ(nullptr, dev.kthread);
}

TEST_F(HistOpsTest, get_hist_dev)
{
    struct smap_hist_dev *dev = &g_smap_hist_dev;
    EXPECT_EQ(dev, get_hist_dev());
}

extern "C" void smap_hist_mid_exit(void);
extern "C" void ub_hist_exit(void);
TEST_F(HistOpsTest, hist_deinit)
{
    g_smap_hist_dev.pgcount = 1;
    struct smap_hist_dev *dev = &g_smap_hist_dev;
    hist_buffer_init(dev);
    MOCKER(scan_thread_deinit).stubs().will(ignoreReturnValue());
    MOCKER(addr_segs_deinit).stubs().will(ignoreReturnValue());
    MOCKER(smap_hist_mid_exit).stubs().will(ignoreReturnValue());
    MOCKER(ub_hist_exit).stubs().will(ignoreReturnValue());
    hist_deinit();
    EXPECT_EQ(nullptr, g_smap_hist_dev.buf);
}

extern "C" int smap_hist_chip_init(int nr_socket, bool is_ub_qemu);
TEST_F(HistOpsTest, smap_hist_chip_init)
{
    int ret;

    MOCKER(ub_hist_init).stubs().will(returnValue(0));
    MOCKER(smap_hist_mid_init).stubs().will(returnValue(-1));
    MOCKER(ub_hist_exit).stubs();
    ret = smap_hist_chip_init(2, true);
    EXPECT_EQ(-1, ret);

    GlobalMockObject::verify();
    MOCKER(ub_hist_init).stubs().will(returnValue(0));
    MOCKER(smap_hist_mid_init).stubs().will(returnValue(0));
    ret = smap_hist_chip_init(1, false);
    EXPECT_EQ(0, ret);
}

TEST_F(HistOpsTest, hist_init)
{
    int ret;
    MOCKER(get_cpu_socket_count).stubs().will(returnValue(1));
    MOCKER(smap_hist_chip_init).stubs().will(returnValue(0));
    MOCKER(addr_segs_init).stubs().will(returnValue(0));
    MOCKER(hist_buffer_init).stubs().will(returnValue(0));
    MOCKER(scan_thread_init).stubs().will(returnValue(0));
    ret = hist_init(SIZE_2M, true);
    EXPECT_EQ(0, ret);
}