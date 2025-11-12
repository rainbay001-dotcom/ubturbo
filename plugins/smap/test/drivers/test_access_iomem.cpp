/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: SMAP5.0 内存地址模块测试代码
 */

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include <linux/list.h>
#include <linux/dmi.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/memory.h>
#include <linux/pfn.h>

#include "access_tracking.h"
#include "access_iomem.h"

using namespace std;

class AccessIomemTest : public ::testing::Test {
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

extern "C" struct list_head drivers_remote_ram_list;
extern "C" int get_remote_ram_len(void);
TEST_F(AccessIomemTest, get_remote_ram_len)
{
    int len;
    LIST_HEAD(drivers_remote_ram_list);

    len = get_remote_ram_len();
    EXPECT_EQ(0, len);
}

extern "C" void drivers_free_remote_ram(struct list_head *head);
TEST_F(AccessIomemTest, free_remote_ram)
{
    LIST_HEAD(head);
    struct ram_segment seg1;
    struct ram_segment seg2;

    MOCKER(kfree).expects(exactly(2)).will(ignoreReturnValue());
    drivers_free_remote_ram(&head);
    EXPECT_TRUE(list_empty(&head));
}

extern "C" void move_remote_ram(struct list_head *dst, struct list_head *src);
TEST_F(AccessIomemTest, move_remote_ram)
{
    struct ram_segment seg;
    LIST_HEAD(src);
    LIST_HEAD(dst);
    list_add(&seg.node, &src);
    move_remote_ram(&dst, &src);
    EXPECT_TRUE(list_empty(&src));
    EXPECT_FALSE(list_empty(&dst));
    EXPECT_EQ(&seg.node, dst.next);
}

extern "C" void drivers__print_remote_ram(struct list_head *head);
TEST_F(AccessIomemTest, __print_remote_ram)
{
    LIST_HEAD(src);
    drivers__print_remote_ram(&src);
}

extern "C" bool pfn_valid(unsigned long pfn);
extern "C" struct page *pfn_to_online_page(unsigned long pfn);
extern "C" int page_to_nid(const struct page *page);
extern "C" int drivers_insert_remote_ram(u64 pa_start, u64 pa_end, struct list_head *head);
TEST_F(AccessIomemTest, insert_remote_ram)
{
    int ret;
    int len = 0;
    struct ram_segment *seg;
    struct ram_segment *tmp;
    struct page page;
    LIST_HEAD(head);
    u64 start;
    u64 end;
    unsigned long pfn[2];
    int remoteNid[2] = { 4, 5 };

    start = 0;
    end = start + 2 * MIN_MEMORY_BLOCK_SIZE - 1;
    pfn[0] = (unsigned long)PHYS_PFN(start);
    pfn[1] = (unsigned long)PHYS_PFN(start + MIN_MEMORY_BLOCK_SIZE);
    MOCKER(pfn_valid).stubs().will(returnValue(true));
    MOCKER(pfn_to_online_page).stubs().will(returnValue(&page));
    MOCKER(page_to_nid).stubs().will(returnValue(remoteNid[0])).then(returnValue(remoteNid[1]));
    ret = drivers_insert_remote_ram(start, end, &head);
    EXPECT_EQ(0, ret);
    list_for_each_entry_safe(seg, tmp, &head, node) {
        if (len++ == 0) {
            EXPECT_EQ(remoteNid[0], seg->numa_node);
            EXPECT_EQ(start, seg->start);
            EXPECT_EQ(start + MIN_MEMORY_BLOCK_SIZE - 1, seg->end);
        } else {
            EXPECT_EQ(remoteNid[1], seg->numa_node);
            EXPECT_EQ(start + MIN_MEMORY_BLOCK_SIZE, seg->start);
            EXPECT_EQ(end, seg->end);
        }
        list_del(&seg->node);
        kfree(seg);
    }
    EXPECT_TRUE(list_empty(&head));
    EXPECT_EQ(2, len);
}

TEST_F(AccessIomemTest, insert_remote_ram_two)
{
    int ret;
    LIST_HEAD(head);
    u64 start;
    u64 end;
    unsigned long pfn[2];
    int hcomNid = 63;

    start = 0;
    end = start + 2 * MIN_MEMORY_BLOCK_SIZE - 1;
    pfn[0] = (unsigned long)PHYS_PFN(start);
    pfn[1] = (unsigned long)PHYS_PFN(start + MIN_MEMORY_BLOCK_SIZE);
    MOCKER(pfn_to_nid).stubs().will(returnValue(hcomNid));
    MOCKER(kmalloc).expects(never()).will(returnValue(nullptr));
    ret = drivers_insert_remote_ram(start, end, &head);
    EXPECT_EQ(0, ret);
}

extern "C" int drivers_fixed_remote_ram(struct list_head *head);
TEST_F(AccessIomemTest, fixed_remote_ram)
{
    struct ram_segment *seg;
    struct ram_segment *tmp;
    LIST_HEAD(head);
    int ret = drivers_fixed_remote_ram(&head);
    EXPECT_EQ(0, ret);
    list_for_each_entry_safe(seg, tmp, &head, node) {
        list_del(&seg->node);
        kfree(seg);
    }
}

extern "C" int drivers_update_resource(struct resource *r, void *arg);
TEST_F(AccessIomemTest, update_resource)
{
    struct resource r;
    LIST_HEAD(head);

    r.flags = 0x02000000;
    MOCKER(drivers_insert_remote_ram).stubs().will(returnValue(-1));
    MOCKER(drivers_free_remote_ram).stubs();
    int ret = drivers_update_resource(&r, &head);
    EXPECT_EQ(-1, ret);
}

TEST_F(AccessIomemTest, update_resource_two)
{
    struct resource r;
    LIST_HEAD(head);

    int ret = drivers_update_resource(nullptr, nullptr);
    EXPECT_EQ(-EINVAL, ret);

    r.flags = 0;
    ret = drivers_update_resource(&r, &head);
    EXPECT_EQ(0, ret);
}

extern "C" int walk_iomem_res_desc(unsigned long desc, unsigned long flags, u64 start,
    u64 end, void *arg, int (*func)(struct resource *, void *));
extern "C" int drivers_walk_system_ram_remote_range(struct list_head *head);
TEST_F(AccessIomemTest, walk_system_ram_remote_range)
{
    int ret = drivers_walk_system_ram_remote_range(nullptr);
    EXPECT_EQ(-EINVAL, ret);

    LIST_HEAD(head);
    MOCKER(walk_iomem_res_desc).stubs().will(returnValue(0));
    ret = drivers_walk_system_ram_remote_range(&head);
    EXPECT_EQ(0, ret);
}

extern "C" void drivers_release_remote_ram(void);
TEST_F(AccessIomemTest, release_remote_ram)
{
    MOCKER(drivers_free_remote_ram).stubs();
    drivers_release_remote_ram();
}

extern "C" bool drivers_ram_changed(void);
extern bool drivers_remote_ram_changed;
extern unsigned int drivers_smap_scene;
TEST_F(AccessIomemTest, drivers_ram_changed)
{
    bool ret;

    drivers_remote_ram_changed = true;
    drivers_smap_scene = UB_QEMU_SCENE;
    ret = drivers_ram_changed();
    EXPECT_FALSE(ret);

    drivers_smap_scene = NORMAL_SCENE;
    ret = drivers_ram_changed();
    EXPECT_TRUE(ret);
}

extern "C" int drivers_refresh_remote_ram(void);
TEST_F(AccessIomemTest, refresh_remote_ram_normal_scene)
{
    int ret;
    drivers_remote_ram_changed = false;
    drivers_smap_scene = NORMAL_SCENE;
    MOCKER(drivers_walk_system_ram_remote_range).stubs().will(returnValue(-EINVAL));
    ret = drivers_refresh_remote_ram();
    EXPECT_EQ(-EINVAL, ret);
    EXPECT_FALSE(drivers_remote_ram_changed);

    GlobalMockObject::reset();
    MOCKER(drivers_walk_system_ram_remote_range).stubs().will(returnValue(0));
    MOCKER(drivers_free_remote_ram).stubs().will(ignoreReturnValue());
    MOCKER(move_remote_ram).stubs().will(ignoreReturnValue());
    MOCKER(drivers__print_remote_ram).stubs().will(ignoreReturnValue());
    ret = drivers_refresh_remote_ram();
    EXPECT_EQ(0, ret);
    EXPECT_TRUE(drivers_remote_ram_changed);
}

TEST_F(AccessIomemTest, refresh_remote_ram_ub_qemu_scene)
{
    int ret;
    drivers_remote_ram_changed = false;
    drivers_smap_scene = UB_QEMU_SCENE;
    MOCKER(drivers_fixed_remote_ram).stubs().will(returnValue(-EINVAL));
    ret = drivers_refresh_remote_ram();
    EXPECT_EQ(-EINVAL, ret);
    EXPECT_FALSE(drivers_remote_ram_changed);

    GlobalMockObject::reset();
    MOCKER(drivers_fixed_remote_ram).stubs().will(returnValue(0));
    MOCKER(drivers_free_remote_ram).stubs().will(ignoreReturnValue());
    MOCKER(move_remote_ram).stubs().will(ignoreReturnValue());
    MOCKER(drivers__print_remote_ram).stubs().will(ignoreReturnValue());
    ret = drivers_refresh_remote_ram();
    EXPECT_EQ(0, ret);
    EXPECT_TRUE(drivers_remote_ram_changed);
}

TEST_F(AccessIomemTest, get_numa_by_pfn)
{
    struct ram_segment seg;
    seg.numa_node = 4;
    seg.start = 4000;
    seg.end = 5000;
    list_add(&seg.node, &drivers_remote_ram_list);

    int ret = get_numa_by_pfn(1);
    EXPECT_EQ(4, ret);
    list_del(&seg.node);
}

extern "C" u64 get_node_page_cnt_iomem(int nid, int page_size);
extern "C" int drivers_nr_local_numa;
TEST_F(AccessIomemTest, get_node_page_cnt_iomem)
{
    u64 ret;
    u64 len = 0;
    struct ram_segment seg1 = { .numa_node = 0 };
    struct ram_segment seg2 = { .numa_node = 1, .start = 0x0, .end = 0xfffff, };
    struct ram_segment seg3 = { .numa_node = 1, .start = 0x200000, .end = 0x3fffff, };

    drivers_nr_local_numa = 1;
    ret = get_node_page_cnt_iomem(0, PAGE_SIZE_2M);
    EXPECT_EQ(0, ret);

    GlobalMockObject::reset();
    cout << "second" << endl;
    ASSERT_TRUE(list_empty(&drivers_remote_ram_list));
    list_add_tail(&seg1.node, &drivers_remote_ram_list);
    list_add_tail(&seg2.node, &drivers_remote_ram_list);
    list_add_tail(&seg3.node, &drivers_remote_ram_list);
    len += calc_2m_count(seg2.end - seg2.start + 1);
    len += calc_2m_count(seg3.end - seg3.start + 1);
    ret = get_node_page_cnt_iomem(drivers_nr_local_numa, PAGE_SIZE_2M);
    EXPECT_EQ(len, ret);
    list_del(&seg1.node);
    list_del(&seg2.node);
    list_del(&seg3.node);
}

extern "C" int drivers_calc_paddr_acidx_iomem(u64 pa, int *nid, u64 *index, int page_size);
TEST_F(AccessIomemTest, CalcPaddrAcidxIomemTest)
{
    u64 index = 3;
    u64 pa = 2;
    int nid = 2;
    int pagesize = 4096;
    int ret = drivers_calc_paddr_acidx_iomem(pa, &nid, &index, pagesize);
    EXPECT_EQ(-ERANGE, ret);

    struct ram_segment seg = { .numa_node = 1, .start = 0x0, .end = 0xfffff, };
    list_add(&seg.node, &drivers_remote_ram_list);
    pa = 0x1000;
    ret = drivers_calc_paddr_acidx_iomem(pa, &nid, &index, pagesize);
    EXPECT_EQ(0, ret);
    list_del(&seg.node);
}

extern "C" int smap_is_remote_addr_valid(int nid, u64 pa_start, u64 pa_end);
TEST_F(AccessIomemTest, smap_is_remote_addr_valid)
{
    int ret = smap_is_remote_addr_valid(0, 0x1000, 0x100);
    EXPECT_EQ(-EINVAL, ret);

    ret = smap_is_remote_addr_valid(0, 0x0, 0x100);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(AccessIomemTest, smap_is_remote_addr_valid_two)
{
    struct ram_segment seg = { .numa_node = 1, .start = 0x0, .end = 0xfffff, };
    list_add(&seg.node, &drivers_remote_ram_list);

    int ret = smap_is_remote_addr_valid(0, 0x10, 0x100);
    EXPECT_EQ(-EINVAL, ret);

    ret = smap_is_remote_addr_valid(1, 0x10, 0xfffffff);
    EXPECT_EQ(-EINVAL, ret);

    ret = smap_is_remote_addr_valid(1, 0x10, 0x100);
    EXPECT_EQ(0, ret);

    ret = smap_is_remote_addr_valid(1, 0x0, 0xfffff);
    EXPECT_EQ(0, ret);

    ret = smap_is_remote_addr_valid(1, 0x0, 0xfff00);
    EXPECT_EQ(0, ret);
    list_del(&seg.node);
}
