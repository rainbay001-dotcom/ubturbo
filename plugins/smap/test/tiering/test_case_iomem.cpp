/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: SMAP 测试代码
 */

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include <linux/list.h>
#include <linux/dmi.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/fs.h>

#include "iomem.h"
#include "smap_msg.h"

extern list_head remote_ram_list;
class IomemTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        std::cout << "[Phase SetUp Begin]" << std::endl;
        INIT_LIST_HEAD(&remote_ram_list);
        std::cout << "[Phase SetUp End]" << std::endl;
    }
    void TearDown() override
    {
        std::cout << "[Phase TearDown Begin]" << std::endl;
        GlobalMockObject::reset();
        std::cout << "[Phase TearDown End]" << std::endl;
    }
};

struct obmm_dev_info {
    struct list_head list;
    struct mutex lock;
};

struct read_obmm_callback {
    struct dir_context ctx;
    int ret;
};

extern "C" struct obmm_dev_info obmm_dev;
extern "C" void free_remote_ram(struct list_head *head);
TEST_F(IomemTest, free_remote_ram_dt)
{
    ram_segment *ramdata1 = new ram_segment;
    ram_segment *ramdata2 = new ram_segment;
    list_head test_list;
    INIT_LIST_HEAD(&ramdata1->node);
    list_add(&ramdata2->node, &ramdata1->node);
    free_remote_ram(&ramdata1->node);
    if (ramdata1 != nullptr) {
        delete(ramdata1);
    }
}
extern "C" void copy_remote_ram(struct list_head *dst, struct list_head *src);
TEST_F(IomemTest, copy_remote_ram_dt)
{
    ram_segment ramdata1 = {0};
    ram_segment ramdata2 = {0};
    list_head test_list;
    INIT_LIST_HEAD(&test_list);
    list_add(&ramdata1.node, &test_list);
    list_add(&ramdata2.node, &test_list);
    copy_remote_ram(&ramdata1.node, &ramdata2.node);
}
extern "C" void __print_remote_ram(struct list_head *head);
TEST_F(IomemTest, __print_remote_ram_dt)
{
    ram_segment ramdata1 = {0};
    ram_segment ramdata2 = {0};
    list_head test_list;
    INIT_LIST_HEAD(&test_list);
    list_add(&ramdata1.node, &test_list);
    list_add(&ramdata2.node, &test_list);
    __print_remote_ram(&test_list);
}

extern "C" bool pfn_valid(unsigned long pfn);
extern "C" int page_to_nid(const struct page *page);
extern "C" struct page *pfn_to_online_page(unsigned long pfn);
extern "C" int insert_remote_ram(u64 pa_start, u64 pa_end, struct list_head *head);
TEST_F(IomemTest, insert_remote_rams_dt)
{
    int ret = 0;
    u64 pa_start = 0;
    u64 pa_end = 3;
    LIST_HEAD(test_list);
    ret = insert_remote_ram(pa_start, pa_end, &test_list);
    EXPECT_EQ(ret, 0);
    ret = insert_remote_ram(pa_start, pa_end, &test_list);
    EXPECT_EQ(ret, 0);
}
TEST_F(IomemTest, insert_remote_rams_dt_One)
{
    int ret = 0;
    u64 pa_start = 0;
    u64 pa_end = 3;
    struct page page;
    LIST_HEAD(test_list);

    MOCKER(pfn_valid).stubs().will(returnValue(true));
    MOCKER(pfn_to_online_page).stubs().will(returnValue(&page));
    MOCKER(page_to_nid).stubs().will(returnValue(NUMA_NO_NODE));
    ret = insert_remote_ram(pa_start, pa_end, &test_list);
    EXPECT_EQ(ret, -EINVAL);
}
TEST_F(IomemTest, insert_remote_rams_dt_Two)
{
    int ret = 0;
    u64 pa_start = 0;
    u64 pa_end = 3;
    struct page page;
    LIST_HEAD(test_list);

    MOCKER(pfn_valid).stubs().will(returnValue(true));
    MOCKER(pfn_to_online_page).stubs().will(returnValue(&page));
    MOCKER(page_to_nid).stubs().will(returnValue(1));
    MOCKER(kmalloc).stubs().will(returnValue((void*)nullptr));
    ret = insert_remote_ram(pa_start, pa_end, &test_list);
    EXPECT_EQ(ret, -ENOMEM);
}

TEST_F(IomemTest, insert_remote_rams_dt_Three)
{
    int ret = 0;
    u64 pa_start = 0;
    u64 pa_end = 3;
    struct page page;
    LIST_HEAD(test_list);
 
    MOCKER(pfn_valid).stubs().will(returnValue(true));
    MOCKER(pfn_to_online_page).stubs().will(returnValue(&page));
    MOCKER(page_to_nid).stubs().will(returnValue(1));
    ret = insert_remote_ram(pa_start, pa_end, &test_list);
    EXPECT_EQ(0, ret);

    struct ram_segment *seg;
    struct ram_segment *tmp;
    list_for_each_entry_safe(seg, tmp, &test_list, node)
    {
        list_del(&seg->node);
        kfree(seg);
    }
}

TEST_F(IomemTest, insert_remote_rams_dt_Four)
{
    int ret = 0;
    u64 pa_start = 2;
    u64 pa_end = 3;
    struct page page;
    LIST_HEAD(test_list);
    struct ram_segment test_node = {
        .numa_node = 1,
        .start = 0,
        .end =1,
    };
    list_add_tail(&test_node.node, &test_list);
    MOCKER(pfn_valid).stubs().will(returnValue(true));
    MOCKER(pfn_to_online_page).stubs().will(returnValue(&page));
    MOCKER(page_to_nid).stubs().will(returnValue(1));
    ret = insert_remote_ram(pa_start, pa_end, &test_list);
    EXPECT_EQ(0, ret);
 
    list_del(&test_node.node);
}

TEST_F(IomemTest, insert_remote_rams_dt_Five)
{
    int ret = 0;
    u64 pa_start = 1;
    u64 pa_end = 3;
    struct page page;
    LIST_HEAD(test_list);
    struct ram_segment test_node = {
        .numa_node = 1,
        .start = 0,
        .end =0,
    };
    list_add_tail(&test_node.node, &test_list);
    MOCKER(pfn_valid).stubs().will(returnValue(true));
    MOCKER(pfn_to_online_page).stubs().will(returnValue(&page));
    MOCKER(page_to_nid).stubs().will(returnValue(20));
    ret = insert_remote_ram(pa_start, pa_end, &test_list);
    EXPECT_EQ(0, ret);
 
    list_del(&test_node.node);
}

TEST_F(IomemTest, insert_remote_rams_dt_Six)
{
    int ret = 0;
    u64 pa_start = 1;
    u64 pa_end = 3;
    struct page page;
    LIST_HEAD(test_list);
    struct ram_segment test_node = {
        .numa_node = 1,
        .start = 4,
        .end =4,
    };
    list_add_tail(&test_node.node, &test_list);
    MOCKER(pfn_valid).stubs().will(returnValue(true));
    MOCKER(pfn_to_online_page).stubs().will(returnValue(&page));
    MOCKER(page_to_nid).stubs().will(returnValue(1));
    ret = insert_remote_ram(pa_start, pa_end, &test_list);
    EXPECT_EQ(0, ret);
 
    list_del(&test_node.node);
}

TEST_F(IomemTest, insert_remote_rams_dt_Seven)
{
    int ret = 0;
    u64 pa_start = 1;
    u64 pa_end = 3;
    struct page page;
    LIST_HEAD(test_list);
    struct ram_segment test_node = {
        .numa_node = 1,
        .start = 2,
        .end =4,
    };
    list_add_tail(&test_node.node, &test_list);
    MOCKER(pfn_valid).stubs().will(returnValue(true));
    MOCKER(pfn_to_online_page).stubs().will(returnValue(&page));
    MOCKER(page_to_nid).stubs().will(returnValue(1));
    ret = insert_remote_ram(pa_start, pa_end, &test_list);
    EXPECT_EQ(0, ret);
 
    list_del(&test_node.node);
}

TEST_F(IomemTest, insert_remote_rams_dt_Eight)
{
    int ret = 0;
    u64 pa_start = 1;
    u64 pa_end = 3;
    struct page page;
    LIST_HEAD(test_list);
    struct ram_segment test_node = {
        .numa_node = 1,
        .start = 4,
        .end =2,
    };
    list_add_tail(&test_node.node, &test_list);
    MOCKER(pfn_valid).stubs().will(returnValue(true));
    MOCKER(pfn_to_online_page).stubs().will(returnValue(&page));
    MOCKER(page_to_nid).stubs().will(returnValue(1));
    ret = insert_remote_ram(pa_start, pa_end, &test_list);
    EXPECT_EQ(0, ret);
 
    list_del(&test_node.node);
}

TEST_F(IomemTest, insert_remote_rams_dt_Nine)
{
    int ret = 0;
    u64 pa_start = 1;
    u64 pa_end = 3;
    struct page page;
    LIST_HEAD(test_list);
    struct ram_segment test_node = {
        .numa_node = 1,
        .start = 1,
        .end =3,
    };
    list_add_tail(&test_node.node, &test_list);
    MOCKER(pfn_valid).stubs().will(returnValue(true));
    MOCKER(pfn_to_online_page).stubs().will(returnValue(&page));
    MOCKER(page_to_nid).stubs().will(returnValue(1));
    ret = insert_remote_ram(pa_start, pa_end, &test_list);
    EXPECT_EQ(0, ret);
 
    list_del(&test_node.node);
}

extern "C" int fixed_remote_ram(struct list_head *head);
TEST_F(IomemTest, fixed_remote_ram_dt)
{
    struct ram_segment *seg = (struct ram_segment*)kmalloc(sizeof(*seg), GFP_KERNEL);
    LIST_HEAD(test_list);
    int ret = fixed_remote_ram(&test_list);
    EXPECT_EQ(ret, 0);
}
TEST_F(IomemTest, fixed_remote_ram_dt_One)
{
    LIST_HEAD(test_list);
    MOCKER(kmalloc).stubs().will(returnValue((void*)nullptr));
    int ret = fixed_remote_ram(&test_list);
    EXPECT_EQ(ret, -ENOMEM);
}
extern "C" int walk_system_ram_remote_range(struct list_head *head);
extern "C" int walk_iomem_res_desc(unsigned long desc, unsigned long flags, u64 start,
    u64 end, void *arg, int (*func)(struct resource *, void *));
TEST_F(IomemTest, walk_system_ram_remote_range_dt)
{
    int ret = walk_system_ram_remote_range(nullptr);
    EXPECT_EQ(-EINVAL, ret);

    LIST_HEAD(head);
    MOCKER(walk_iomem_res_desc).stubs().will(returnValue(0));
    ret = walk_system_ram_remote_range(&head);
    EXPECT_EQ(0, ret);
}

extern "C" void free_obmm_dev(void);
extern "C" void update_obmm_dev(u64 memid, u64 pa, u64 size);
TEST_F(IomemTest, update_obmm_dev_dt)
{
    ASSERT_TRUE(list_empty(&obmm_dev.list));
    update_obmm_dev(0, 0, 0);

    struct memid_range *tmp = (struct memid_range *)kmalloc(sizeof(*tmp), GFP_KERNEL);
    ASSERT_NE(nullptr, tmp);

    tmp->memid = 1;
    tmp->start = 0x0;
    tmp->end = 0x10;
    tmp->seq = 1;
    list_add(&tmp->node, &obmm_dev.list);

    update_obmm_dev(1, 0x10, 0x100);
    tmp = container_of(obmm_dev.list.next, struct memid_range, node);
    EXPECT_EQ(0x10, tmp->start);

    update_obmm_dev(2, 0x200, 0x300);
    tmp = container_of(obmm_dev.list.next, struct memid_range, node);
    EXPECT_EQ(0x200, tmp->start);

    free_obmm_dev();
}

extern "C" int kern_path(const char *name, unsigned int flags, struct path *path);
extern "C" bool is_import_shmdev(const char *name);
TEST_F(IomemTest, is_import_shmdev_dt)
{
    bool ret = is_import_shmdev("obmm_shmde");
    EXPECT_EQ(false, ret);

    ret = is_import_shmdev("obmm_shmdev1");
    EXPECT_EQ(true, ret);

    MOCKER(kern_path).stubs().will(returnValue(-1));
    ret = is_import_shmdev("obmm_shmdev1");
    EXPECT_EQ(false, ret);
}

extern "C" struct file *filp_open(const char *filename, int flags, umode_t mode);
extern "C" int filp_close(struct file *filp, fl_owner_t id);
extern "C" bool IS_ERR(const void *ptr);
extern "C" int extract_hex_content(const char *file_path, u64 *content);
TEST_F(IomemTest, extract_hex_content_dt)
{
    struct file filp;
    char buf[6] = "0x100";
    u64 content;

    MOCKER(filp_open).stubs().will(returnValue((struct file *)&filp));
    MOCKER(IS_ERR).stubs().will(returnValue(false));
    MOCKER(kernel_read).stubs().with(any(), outBoundP((void *)buf, sizeof(buf)), any(), any()).will(returnValue(5));
    MOCKER(filp_close).stubs().will(ignoreReturnValue());
    int ret = extract_hex_content("size", &content);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0x100, content);
}

extern "C" bool fill_obmmdev(struct dir_context *ctx, const char *name, int namelen,
    loff_t offset, u64 ino, unsigned int d_type);
TEST_F(IomemTest, fill_obmmdev_dt)
{
    struct read_obmm_callback call;
    call.ret = 1;

    bool ret = fill_obmmdev(&call.ctx, "obmm", 4, 0, 0, 0);
    EXPECT_EQ(true, ret);
    EXPECT_EQ(0, call.ret);
}

extern "C" void release_remote_ram(void);
TEST_F(IomemTest, release_remote_ram_dt)
{
    release_remote_ram();
}

extern "C" int refresh_remote_ram(void);
TEST_F(IomemTest, refresh_remote_ram_dt)
{
    smap_scene = NORMAL_SCENE;
    MOCKER(walk_system_ram_remote_range).stubs().will(returnValue(1)).then(returnValue(0));
    int ret = refresh_remote_ram();
    EXPECT_EQ(ret, 1);
    ret = refresh_remote_ram();
    EXPECT_EQ(ret, 0);
}
TEST_F(IomemTest, refresh_remote_ram_dt_One)
{
    smap_scene = UB_QEMU_SCENE;
    MOCKER(fixed_remote_ram).stubs().will(returnValue(0));
    int ret = refresh_remote_ram();
    EXPECT_EQ(ret, 0);
    smap_scene = NORMAL_SCENE;
}
extern "C" bool is_smap_pg_huge(void);
extern "C" int calc_acidx_paddr_iomem(u64 index, int nid, u64 *paddr);
TEST_F(IomemTest, calc_acidx_paddr_iomem_dt)
{
    u64 paddr[10] = {0};
    u64 addr;
    u64 index = 1;
    int nid = 1;
    ram_segment newdata1 = {0};
    ram_segment newdata2 = {0};

    list_add_tail(&newdata1.node, &remote_ram_list);
    list_add_tail(&newdata2.node, &remote_ram_list);
    int ret = calc_acidx_paddr_iomem(index, nid, paddr);
    EXPECT_EQ(ret, -34);
    newdata1.end = 1 << 22;
    nid = 0;
    ret = calc_acidx_paddr_iomem(index, nid, paddr);
    EXPECT_EQ(ret, 0);
    MOCKER(is_smap_pg_huge).stubs().will(returnValue(false));
    ret = calc_acidx_paddr_iomem(index, nid, paddr);
    EXPECT_EQ(ret, 0);

    GlobalMockObject::verify();

    nid = 0;
    index = 0x00000400;
    newdata1.start = 0x0;
    newdata1.end =   0x7fffffff;
    newdata1.numa_node = 0;

    newdata2.start = 0x2080000000;
    newdata2.end = 0x20807fffff;
    newdata2.numa_node = 0;
    
    MOCKER(is_smap_pg_huge).stubs().will(returnValue(true));
    ret = calc_acidx_paddr_iomem(index, nid, &addr);
    ASSERT_EQ(0, ret);
    EXPECT_EQ(0x2080000000, addr);

    list_del(&newdata1.node);
    list_del(&newdata2.node);
}

extern "C" int find_range_by_memid(u64 memid, u64 *start, u64 *end);
TEST_F(IomemTest, find_range_by_memid_dt)
{
    u64 start;
    u64 end;
    struct memid_range mr;
    mr.memid = 1;
    mr.start = 0x0;
    mr.end = 0xFF;

    free_obmm_dev();
    ASSERT_TRUE(list_empty(&obmm_dev.list));
    list_add(&mr.node, &obmm_dev.list);
    int ret = find_range_by_memid(1, &start, &end);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0xFF, end);

    ret = find_range_by_memid(2, &start, &end);
    EXPECT_EQ(-ENOENT, ret);
    list_del(&mr.node);
}