/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: SMAP3.0 内存地址模块测试代码
 */

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include <linux/dmi.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/errno.h>

#include "numa.h"
#include "iomem.h"

using namespace std;

#define INVALID_DMI_HANDLE 0xffff

constexpr uint64_t GIGABYTE = 1073741824;
constexpr int DEFAULT_NUMA_NODE = 0;

struct ram_info {
    size_t len;
    struct list_head sys_rams;
    struct list_head continuous_rams;
};
extern struct ram_info iomem;


class IomemTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        cout << "[Phase SetUp Begin]" << endl;
        cout << "[Phase SetUp End]" << endl;
    }
    void TearDown() override
    {
        struct ram_segment *seg;
        struct ram_segment *tmp;
        cout << "[Phase TearDown Begin]" << endl;
        if (!list_empty(&iomem.sys_rams)) {
            list_for_each_entry_safe(seg, tmp, &iomem.sys_rams, node) {
                list_del(&seg->node);
                kfree(seg);
            }
        }
        if (!list_empty(&iomem.continuous_rams)) {
            list_for_each_entry_safe(seg, tmp, &iomem.continuous_rams, node) {
                list_del(&seg->node);
                kfree(seg);
            }
        }
        GlobalMockObject::verify();
        cout << "[Phase TearDown End]" << endl;
    }
};

extern "C" int _get_system_ram(void);
TEST_F(IomemTest, _GetSystemRam)
{
    int ret;
    struct ram_segment *seg;
    struct ram_segment *tmp;
    struct resource res1 = { 0x0, 0x0FFF, "Reserved" };
    struct resource res2 = { 0x1000, 0x3FFF, "System RAM" }; // valid
    struct resource res3 = { 0x1000, 0x1FFF, "reserved" };
    struct resource res4 = { 0x2000, 0x3FFF, "reserved" };
    struct resource res5 = { 0x4000, 0xFFFF, "System RAM" }; // valid
    iomem_resource.start = 0x0000;
    iomem_resource.end = 0xFFFF;
    iomem_resource.child = &res1;
    res1.sibling = &res2;
    res2.child = &res3;
    res3.parent = &res2;
    res3.sibling = &res4;
    res4.parent = &res2;
    res2.sibling = &res5;

    ret = _get_system_ram();
    ASSERT_EQ(0, ret);
    EXPECT_EQ(0, iomem.len);
    EXPECT_TRUE(list_empty(&iomem.continuous_rams));

    ASSERT_FALSE(list_empty(&iomem.sys_rams));
    seg = container_of(iomem.sys_rams.next, struct ram_segment, node);
    EXPECT_EQ(DEFAULT_NUMA_NODE, seg->numa_node);
    EXPECT_EQ(0x1000, seg->start);
    EXPECT_EQ(0x3FFF, seg->end);
    seg = container_of(seg->node.next, struct ram_segment, node);
    EXPECT_EQ(DEFAULT_NUMA_NODE, seg->numa_node);
    EXPECT_EQ(0x4000, seg->start);
    EXPECT_EQ(0xFFFF, seg->end);

    list_for_each_entry_safe(seg, tmp, &iomem.sys_rams, node) {
        list_del(&seg->node);
        kfree(seg);
    }

    MOCKER(kmalloc).stubs().will(returnValue((void*)NULL));
    ret = _get_system_ram();
    ASSERT_EQ(-ENOMEM, ret);
}

extern "C" int _concat_ram(void);
TEST_F(IomemTest, _ConcatRam)
{
    struct ram_segment *seg;
    struct ram_segment *seg1;
    struct ram_segment *seg2;
    struct ram_segment *seg3;
    int ret;

    ASSERT_TRUE(list_empty(&iomem.sys_rams));
    ASSERT_TRUE(list_empty(&iomem.continuous_rams));

    // construct iomem.sys_rams
    seg1 = (ram_segment*)kmalloc(sizeof(*seg1), GFP_KERNEL);
    ASSERT_NE(nullptr, seg1);
    seg2 = (ram_segment*)kmalloc(sizeof(*seg2), GFP_KERNEL);
    ASSERT_NE(nullptr, seg2);
    seg3 = (ram_segment*)kmalloc(sizeof(*seg3), GFP_KERNEL);
    ASSERT_NE(nullptr, seg3);
    seg1->numa_node = DEFAULT_NUMA_NODE;
    seg1->start = 0x1000;
    seg1->end = 0x1FFF;
    seg2->numa_node = DEFAULT_NUMA_NODE;
    seg2->start = 0x3000;
    seg2->end = 0xCFFF;
    seg3->numa_node = DEFAULT_NUMA_NODE;
    seg3->start = 0xD000;
    seg3->end = 0x10000;
    list_add_tail(&seg1->node, &iomem.sys_rams);
    list_add_tail(&seg2->node, &iomem.sys_rams);
    list_add_tail(&seg3->node, &iomem.sys_rams);

    ret = _concat_ram();
    ASSERT_EQ(0, ret);

    EXPECT_EQ(2, iomem.len);
    EXPECT_FALSE(list_empty(&iomem.sys_rams));
    ASSERT_FALSE(list_empty(&iomem.continuous_rams));

    seg = container_of(iomem.continuous_rams.next, struct ram_segment, node);
    EXPECT_EQ(DEFAULT_NUMA_NODE, seg->numa_node);
    EXPECT_EQ(0x1000, seg->start);
    EXPECT_EQ(0x1FFF, seg->end);
    seg = container_of(seg->node.next, struct ram_segment, node);
    EXPECT_EQ(DEFAULT_NUMA_NODE, seg->numa_node);
    EXPECT_EQ(0x3000, seg->start);
    EXPECT_EQ(0x10000, seg->end);
}

extern struct numa_list numa_info;
extern "C" int _assign_numa_to_ram(void);
TEST_F(IomemTest, _AssignNumaToRam)
{
    int ret;
    struct ram_segment *seg1;
    struct ram_segment *seg2;
    struct numa_node numa_nodes[4] = {
        { 0, { 0x1000, 0x1FFF } },
        { 1, { 0x2000, 0x2FFF } },
        { 2, { 0x3000, 0x4FFF } },
        { 3, { 0x5000, 0x10000 } }
    };
    numa_info.nr = 4;
    numa_info.nodes = numa_nodes;

    ASSERT_TRUE(list_empty(&iomem.sys_rams));
    ASSERT_TRUE(list_empty(&iomem.continuous_rams));

    // construct iomem.continuous_rams
    seg1 = (ram_segment*)kmalloc(sizeof(*seg1), GFP_KERNEL);
    ASSERT_NE(nullptr, seg1);
    seg2 = (ram_segment*)kmalloc(sizeof(*seg2), GFP_KERNEL);
    ASSERT_NE(nullptr, seg2);
    seg1->numa_node = DEFAULT_NUMA_NODE;
    seg1->start = 0x1000;
    seg1->end = 0x1FFF;
    seg2->numa_node = DEFAULT_NUMA_NODE;
    seg2->start = 0x3000;
    seg2->end = 0x10000;
    list_add_tail(&seg1->node, &iomem.continuous_rams);
    list_add_tail(&seg2->node, &iomem.continuous_rams);
    iomem.len = 2;

    ret = _assign_numa_to_ram();
    ASSERT_EQ(0, ret);
    EXPECT_EQ(3, iomem.len);
    EXPECT_TRUE(list_empty(&iomem.sys_rams));
    ASSERT_FALSE(list_empty(&iomem.continuous_rams));
    numa_info.nodes = NULL;
}

TEST_F(IomemTest, _AssignNumaToRamEmptyNuma)
{
    int ret;

    // numa_info.nr is 0
    numa_info.nr = 0;
    ret = _assign_numa_to_ram();
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(IomemTest, _AssignNumaToRamKmallocFailed)
{
    int ret;
    struct ram_segment *seg1;
    struct ram_segment *seg2;
    struct numa_node numa_nodes[4] = {
        { 0, { 0x1000, 0x1FFF } },
        { 1, { 0x2000, 0x2FFF } },
        { 2, { 0x3000, 0x4FFF } },
        { 3, { 0x5000, 0x10000 } }
    };
    numa_info.nr = 4;
    numa_info.nodes = numa_nodes;

    ASSERT_TRUE(list_empty(&iomem.sys_rams));
    ASSERT_TRUE(list_empty(&iomem.continuous_rams));

    // construct iomem.continuous_rams
    seg1 = (ram_segment*)kmalloc(sizeof(*seg1), GFP_KERNEL);
    ASSERT_NE(nullptr, seg1);
    seg2 = (ram_segment*)kmalloc(sizeof(*seg2), GFP_KERNEL);
    ASSERT_NE(nullptr, seg2);
    seg1->numa_node = DEFAULT_NUMA_NODE;
    seg1->start = 0x1000;
    seg1->end = 0x1FFF;
    seg2->numa_node = DEFAULT_NUMA_NODE;
    seg2->start = 0x3000;
    seg2->end = 0x10000;
    list_add_tail(&seg1->node, &iomem.continuous_rams);
    list_add_tail(&seg2->node, &iomem.continuous_rams);
    iomem.len = 2;

    // kmalloc failed
    MOCKER(kmalloc).stubs().will(returnValue((void*)NULL));
    ret = _assign_numa_to_ram();
    ASSERT_EQ(-ENOMEM, ret);
    EXPECT_EQ(2, iomem.len);
    numa_info.nodes = NULL;
}

extern "C" void reset_iomem(void);
TEST_F(IomemTest, _ResetIomem)
{
    struct ram_segment *seg1;
    struct ram_segment *seg2;
    struct ram_segment *seg3;
    struct ram_segment *seg4;
    struct ram_segment *seg5;

    ASSERT_TRUE(list_empty(&iomem.sys_rams));
    ASSERT_TRUE(list_empty(&iomem.continuous_rams));

    // construct iomem.sys_rams
    seg1 = (ram_segment*)kmalloc(sizeof(*seg1), GFP_KERNEL);
    ASSERT_NE(nullptr, seg1);
    seg2 = (ram_segment*)kmalloc(sizeof(*seg2), GFP_KERNEL);
    ASSERT_NE(nullptr, seg2);
    seg3 = (ram_segment*)kmalloc(sizeof(*seg3), GFP_KERNEL);
    ASSERT_NE(nullptr, seg3);
    seg1->numa_node = DEFAULT_NUMA_NODE;
    seg1->start = 0x1000;
    seg1->end = 0x1FFF;
    seg2->numa_node = DEFAULT_NUMA_NODE;
    seg2->start = 0x3000;
    seg2->end = 0xCFFF;
    seg3->numa_node = DEFAULT_NUMA_NODE;
    seg3->start = 0xD000;
    seg3->end = 0x10000;
    list_add_tail(&seg1->node, &iomem.sys_rams);
    list_add_tail(&seg2->node, &iomem.sys_rams);
    list_add_tail(&seg3->node, &iomem.sys_rams);

    // construct iomem.continuous_rams
    seg4 = (ram_segment*)kmalloc(sizeof(*seg4), GFP_KERNEL);
    ASSERT_NE(nullptr, seg4);
    seg5 = (ram_segment*)kmalloc(sizeof(*seg5), GFP_KERNEL);
    ASSERT_NE(nullptr, seg5);
    seg4->numa_node = DEFAULT_NUMA_NODE;
    seg4->start = 0x1000;
    seg4->end = 0x1FFF;
    seg5->numa_node = DEFAULT_NUMA_NODE;
    seg5->start = 0x3000;
    seg5->end = 0x10000;
    list_add_tail(&seg4->node, &iomem.continuous_rams);
    list_add_tail(&seg5->node, &iomem.continuous_rams);
    iomem.len = 2;

    reset_iomem();
    ASSERT_TRUE(list_empty(&iomem.sys_rams));
    ASSERT_TRUE(list_empty(&iomem.continuous_rams));
    ASSERT_EQ(0, iomem.len);
}

extern "C" int _init_iomem(void);
TEST_F(IomemTest, _InitIomem)
{
    int ret;

    iomem.len = 1;
    ret = _init_iomem();
    EXPECT_EQ(0, ret);

    MOCKER(_get_system_ram).stubs().will(returnValue(-1));
    MOCKER(reset_iomem).stubs().will(returnValue(NULL));
    iomem.len = 0;
    ret = _init_iomem();
    EXPECT_EQ(-1, ret);

    GlobalMockObject::verify();
    MOCKER(_get_system_ram).stubs().will(returnValue(0));
    MOCKER(_concat_ram).stubs().will(returnValue(0));
    ret = _init_iomem();
    EXPECT_EQ(0, ret);
}

extern "C" void _init_addr_ah(void);
TEST_F(IomemTest, _InitAddrAh)
{
    struct ram_segment *seg1;
    struct ram_segment *seg2;
    struct ram_segment *seg3;
    struct ram_segment *seg4;
    struct ram_segment *seg5;

    ASSERT_TRUE(list_empty(&iomem.continuous_rams));

    // construct iomem.continuous_rams
    seg1 = (ram_segment*)kmalloc(sizeof(*seg1), GFP_KERNEL);
    ASSERT_NE(nullptr, seg1);
    seg2 = (ram_segment*)kmalloc(sizeof(*seg2), GFP_KERNEL);
    ASSERT_NE(nullptr, seg2);
    seg3 = (ram_segment*)kmalloc(sizeof(*seg3), GFP_KERNEL);
    ASSERT_NE(nullptr, seg3);
    seg4 = (ram_segment*)kmalloc(sizeof(*seg4), GFP_KERNEL);
    ASSERT_NE(nullptr, seg4);
    seg5 = (ram_segment*)kmalloc(sizeof(*seg5), GFP_KERNEL);
    ASSERT_NE(nullptr, seg5);
    seg1->numa_node = 0;
    seg1->start = 0x1000;
    seg1->end = 0x73FFFFFF;
    seg1->numa_node = 0;
    seg2->start = 0x74000000;
    seg2->end = 0x7FFFFFFF;
    seg3->numa_node = 0;
    seg3->start = 0x100000000;
    seg3->end = 0x100001000;
    seg4->numa_node = 0;
    seg4->start = 0x100002000;
    seg4->end = 0x100002FFF;
    seg5->numa_node = 1;
    seg5->start = 0x200000000;
    seg5->end = 0x200002FFF;
    list_add_tail(&seg1->node, &iomem.continuous_rams);
    list_add_tail(&seg2->node, &iomem.continuous_rams);
    list_add_tail(&seg3->node, &iomem.continuous_rams);
    list_add_tail(&seg4->node, &iomem.continuous_rams);
    list_add_tail(&seg5->node, &iomem.continuous_rams);
    iomem.len = 5;

    _init_addr_ah();
    EXPECT_EQ(0x100000000, addr_ah[0]);
    EXPECT_EQ(0x200000000, addr_ah[1]);
}

extern "C" int _init_system_ram(void);
TEST_F(IomemTest, _InitSystemRam)
{
    int ret;

    MOCKER(reset_numa).stubs().will(returnValue(NULL));
    MOCKER(reset_iomem).stubs().will(returnValue(NULL));
    MOCKER(_init_iomem).stubs().will(returnValue(-1));
    ret = _init_system_ram();
    EXPECT_EQ(-1, ret);
    GlobalMockObject::verify();

    MOCKER(reset_numa).stubs().will(returnValue(NULL));
    MOCKER(reset_iomem).stubs().will(returnValue(NULL));
    MOCKER(_init_iomem).stubs().will(returnValue(0));
    ret = _init_system_ram();
    EXPECT_EQ(-2, ret);
    GlobalMockObject::verify();

    MOCKER(reset_numa).stubs().will(returnValue(NULL));
    MOCKER(reset_iomem).stubs().will(returnValue(NULL));
    MOCKER(_init_iomem).stubs().will(returnValue(0));
    MOCKER(_assign_numa_to_ram).stubs().will(returnValue(-3));
    ret = _init_system_ram();
    EXPECT_EQ(-3, ret);
    GlobalMockObject::verify();

    MOCKER(reset_numa).stubs().will(returnValue(NULL));
    MOCKER(reset_iomem).stubs().will(returnValue(NULL));
    MOCKER(_init_iomem).stubs().will(returnValue(0));
    MOCKER(_assign_numa_to_ram).stubs().will(returnValue(0));
    MOCKER(_init_addr_ah).stubs().will(returnValue(NULL));
    ret = _init_system_ram();
    EXPECT_EQ(0, ret);
}

extern "C" void _reset_system_ram(void);
TEST_F(IomemTest, GetSystemRamLen)
{
    int ret;
    size_t len;

    MOCKER(_init_system_ram).stubs().will(returnValue(-1));
    ret = get_system_ram_len(&len);
    EXPECT_EQ(-1, ret);
    GlobalMockObject::verify();

    MOCKER(_init_system_ram).stubs().will(returnValue(0));
    MOCKER(_reset_system_ram).stubs().will(returnValue(NULL));
    iomem.len = 5;
    ret = get_system_ram_len(&len);
    ASSERT_EQ(0, ret);
    EXPECT_EQ(5, len);
}

TEST_F(IomemTest, GetSystemRam)
{
    int ret;
    struct continuous_ram tmp = { 0, NULL };
    struct ram_segment *seg1;
    struct ram_segment *seg2;
    struct ram_segment *seg3;

    ret = get_system_ram(&tmp);
    EXPECT_EQ(0, ret);

    MOCKER(_init_system_ram).stubs().will(returnValue(-1));
    tmp.len = 4;
    ret = get_system_ram(&tmp);
    EXPECT_EQ(-1, ret);
    GlobalMockObject::verify();

    ASSERT_TRUE(list_empty(&iomem.continuous_rams));
    // construct iomem.continuous_rams
    seg1 = (ram_segment*)kmalloc(sizeof(*seg1), GFP_KERNEL);
    ASSERT_NE(nullptr, seg1);
    seg2 = (ram_segment*)kmalloc(sizeof(*seg2), GFP_KERNEL);
    ASSERT_NE(nullptr, seg2);
    seg3 = (ram_segment*)kmalloc(sizeof(*seg3), GFP_KERNEL);
    ASSERT_NE(nullptr, seg3);
    seg1->numa_node = 0;
    seg1->start = 0x1000;
    seg1->end = 0x7FFFFFFF;
    seg2->numa_node = 0;
    seg2->start = 0x100000000;
    seg2->end = 0x100001000;
    seg3->numa_node = 1;
    seg3->start = 0x200000000;
    seg3->end = 0x200002FFF;
    list_add_tail(&seg1->node, &iomem.continuous_rams);
    list_add_tail(&seg2->node, &iomem.continuous_rams);
    list_add_tail(&seg3->node, &iomem.continuous_rams);
    iomem.len = 3;

    MOCKER(_reset_system_ram).stubs().will(returnValue(NULL));
    MOCKER(_init_system_ram).stubs().will(returnValue(0));
    MOCKER(_reset_system_ram).stubs().will(returnValue(NULL));
    tmp.len = 2;
    tmp.ram = (struct ram_node*)kzalloc(3 * sizeof(*tmp.ram), GFP_KERNEL);
    ASSERT_NE(nullptr, tmp.ram);
    ret = get_system_ram(&tmp);
    ASSERT_EQ(0, ret);
    EXPECT_EQ(0, tmp.ram[0].node);
    EXPECT_EQ(0x1000, tmp.ram[0].start);
    EXPECT_EQ(0x7FFFFFFF, tmp.ram[0].end);
    EXPECT_EQ(0, tmp.ram[1].node);
    EXPECT_EQ(0x100000000, tmp.ram[1].start);
    EXPECT_EQ(0x100001000, tmp.ram[1].end);
    EXPECT_EQ(0, tmp.ram[2].node);
    EXPECT_EQ(0, tmp.ram[2].start);
    EXPECT_EQ(0, tmp.ram[2].end);

    tmp.len = 3;
    ret = get_system_ram(&tmp);
    ASSERT_EQ(0, ret);
    EXPECT_EQ(0, tmp.ram[0].node);
    EXPECT_EQ(0x1000, tmp.ram[0].start);
    EXPECT_EQ(0x7FFFFFFF, tmp.ram[0].end);
    EXPECT_EQ(0, tmp.ram[1].node);
    EXPECT_EQ(0x100000000, tmp.ram[1].start);
    EXPECT_EQ(0x100001000, tmp.ram[1].end);
    EXPECT_EQ(1, tmp.ram[2].node);
    EXPECT_EQ(0x200000000, tmp.ram[2].start);
    EXPECT_EQ(0x200002FFF, tmp.ram[2].end);
}

TEST_F(IomemTest, GetRamSize)
{
    u64 ram_size = 0;
    MOCKER(dmi_memdev_handle).stubs().with(eq(0)).will(returnValue(0));
    MOCKER(dmi_memdev_handle).stubs().with(eq(1)).will(returnValue(1));
    MOCKER(dmi_memdev_handle).stubs().with(eq(2)).will(returnValue(0xffff)); // invalid handle

    MOCKER(dmi_memdev_size).stubs().with(eq(u16(0))).will(returnValue((u64)8 * GIGABYTE));
    MOCKER(dmi_memdev_size).stubs().with(eq(u16(1))).will(returnValue((u64)4 * GIGABYTE));

    MOCKER(dmi_memdev_type).stubs().with(eq(u16(0))).will(returnValue(1)); // invalid type
    MOCKER(dmi_memdev_type).stubs().with(eq(u16(1))).will(returnValue(3));

    ram_size = get_ram_size();
    EXPECT_EQ((u64)4 * GIGABYTE, ram_size);
}

TEST_F(IomemTest, GetNodeActcLen)
{
    int ret;
    int len;
    u64 *node_len;
    struct ram_segment *seg1;
    struct ram_segment *seg2;
    struct ram_segment *seg3;
    len = 0;
    node_len = (int*)kzalloc(2 * sizeof(*node_len), GFP_KERNEL);
    ASSERT_NE(nullptr, node_len);
    ret = get_node_actc_len(len, node_len);
    EXPECT_EQ(-EINVAL, ret);

    len = 2;
    MOCKER(_init_system_ram).stubs().will(returnValue(-1));
    ret = get_node_actc_len(len, node_len);
    EXPECT_EQ(-1, ret);
    GlobalMockObject::verify();

    MOCKER(get_ram_size).stubs().will(returnValue((u64)8 * GIGABYTE));
    MOCKER(_init_system_ram).stubs().will(returnValue(0));
    MOCKER(_reset_system_ram).stubs().will(returnValue(NULL));
    ASSERT_TRUE(list_empty(&iomem.continuous_rams));
    // construct iomem.sys_rams
    seg1 = (ram_segment*)kmalloc(sizeof(*seg1), GFP_KERNEL);
    ASSERT_NE(nullptr, seg1);
    seg2 = (ram_segment*)kmalloc(sizeof(*seg2), GFP_KERNEL);
    ASSERT_NE(nullptr, seg2);
    seg3 = (ram_segment*)kmalloc(sizeof(*seg3), GFP_KERNEL);
    ASSERT_NE(nullptr, seg3);
    seg1->numa_node = 0;
    seg1->start = 0x1000;
    seg1->end = 0x7FFFFFFF; // 约等于2G
    seg2->numa_node = 0;
    seg2->start = 0x100000000;
    seg2->end = 0x1FFFFFFFF; // 4G
    seg3->numa_node = 1;
    seg3->start = 0x200000000;
    seg3->end = 0x27FFFFFFF; // 2G
    list_add_tail(&seg1->node, &iomem.continuous_rams);
    list_add_tail(&seg2->node, &iomem.continuous_rams);
    list_add_tail(&seg3->node, &iomem.continuous_rams);
    iomem.len = 3;

    ret = get_node_actc_len(len, node_len);
    ASSERT_EQ(0, ret);
    ASSERT_EQ((1024 + 2048), node_len[0]);
    ASSERT_EQ((2048), node_len[1]);
}
