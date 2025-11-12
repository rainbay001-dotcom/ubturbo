/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: SMAP3.0 acpi mem module test code
 */

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include <linux/acpi.h>
#include "acpi_mem.h"
#include <acpi/actbl3.h>

using namespace std;

extern "C"
{
    static unsigned int DefaultHStateIDX = 0;
    static struct hstate (*DefaultHStates)[] = {NULL};

    void InitACPIMemTest()
    {
    }
}

class AcpiMemTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        cout << "[Phase SetUp Begin]" << endl;
        cout << "[Phase SetUp End]" << endl;
        InitACPIMemTest();
    }
    void TearDown() override
    {
        struct acpi_mem_segment *mem;
        struct acpi_mem_segment *ams_tmp;
        cout << "[Phase TearDown Begin]" << endl;
        list_for_each_entry_safe(mem, ams_tmp, &acpi_mem.mem, segment) {
            list_del(&mem->segment);
            kfree(mem);
        }
        acpi_mem.len = 0;
        GlobalMockObject::verify();
        cout << "[Phase TearDown End]" << endl;
    }
};

extern struct node_mem acpi_mem_cached[];
extern "C" void calc_node_distance(void);
#undef SMAP_MAX_NUMNODES
#define SMAP_MAX_NUMNODES 3
TEST_F(AcpiMemTest, CalcNodeDistance)
{
    int i;
    int j;
    bool flag;

    node_states[N_NORMAL_MEMORY].bits[0] = 0x107; // node 0 1 2 8 has memory
    MOCKER(__node_distance).stubs().with(eq(0), eq(0)).will(returnValue(10));
    MOCKER(__node_distance).stubs().with(eq(0), eq(1)).will(returnValue(21));
    MOCKER(__node_distance).stubs().with(eq(0), eq(2)).will(returnValue(14));

    MOCKER(__node_distance).stubs().with(eq(1), eq(0)).will(returnValue(21));
    MOCKER(__node_distance).stubs().with(eq(1), eq(1)).will(returnValue(10));
    MOCKER(__node_distance).stubs().with(eq(1), eq(2)).will(returnValue(24));

    MOCKER(__node_distance).stubs().with(eq(2), eq(0)).will(returnValue(14));
    MOCKER(__node_distance).stubs().with(eq(2), eq(1)).will(returnValue(24));
    MOCKER(__node_distance).stubs().with(eq(2), eq(2)).will(returnValue(10));
    MOCKER(__node_distance).stubs().with(any(), any()).will(returnValue(100));
    acpi_mem_cached[0].online = acpi_mem_cached[2].online = true;
    acpi_mem_cached[1].online = false;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            acpi_mem_cached[i].distance[j] = 0;
        }
    }
    ASSERT_EQ(0, acpi_mem_cached[0].distance[0]);
    ASSERT_EQ(0, acpi_mem_cached[1].distance[0]);
    ASSERT_EQ(0, acpi_mem_cached[2].distance[0]);

    calc_node_distance();
    EXPECT_EQ(10, acpi_mem_cached[0].distance[0]);
    EXPECT_EQ(0, acpi_mem_cached[1].distance[0]);
    EXPECT_EQ(14, acpi_mem_cached[2].distance[0]);

    // set node1 online and recalculate distance
    acpi_mem_cached[1].online = true;
    calc_node_distance();
    EXPECT_EQ(21, acpi_mem_cached[1].distance[0]);
    EXPECT_EQ(10, acpi_mem_cached[1].distance[1]);
    EXPECT_EQ(24, acpi_mem_cached[1].distance[2]);
}

#undef SMAP_MAX_NUMNODES
#define SMAP_MAX_NUMNODES 10
TEST_F(AcpiMemTest, FindClosestNode)
{
    int i;
    int j;
    int nid;
    int ret;

    ret = find_closest_node(-1, &nid);
    EXPECT_EQ(-EINVAL, ret);

    ret = find_closest_node(SMAP_MAX_NUMNODES, &nid);
    EXPECT_EQ(-EINVAL, ret);

    for (i = 0; i < SMAP_MAX_NUMNODES; i++) {
        acpi_mem_cached[i].online = false;
    }
    ret = find_closest_node(0, &nid);
    EXPECT_EQ(-EINVAL, ret);

    acpi_mem_cached[0].online = true;
    for (i = 0; i < SMAP_MAX_NUMNODES; i++) {
        acpi_mem_cached[0].distance[i] = 0;
    }
    ret = find_closest_node(0, &nid);
    EXPECT_EQ(-ENODATA, ret);

    acpi_mem_cached[0].distance[0] = 10;
    acpi_mem_cached[0].distance[1] = 21;
    acpi_mem_cached[0].distance[2] = 14;
    acpi_mem_cached[0].distance[3] = 24;
    MOCKER(node_state).stubs().with(0, N_CPU).will(returnValue(1));
    MOCKER(node_state).stubs().with(1, N_CPU).will(returnValue(1));
    MOCKER(node_state).stubs().with(2, N_CPU).will(returnValue(1));
    MOCKER(node_state).stubs().with(3, N_CPU).will(returnValue(1));
    MOCKER(node_state).stubs().with(any(), N_CPU).will(returnValue(1));
    ret = find_closest_node(0, &nid);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(2, nid);

    GlobalMockObject::verify();
    MOCKER(node_state).stubs().with(0, N_CPU).will(returnValue(1));
    MOCKER(node_state).stubs().with(1, N_CPU).will(returnValue(0));
    MOCKER(node_state).stubs().with(2, N_CPU).will(returnValue(1));
    MOCKER(node_state).stubs().with(3, N_CPU).will(returnValue(1));
    MOCKER(node_state).stubs().with(any(), N_CPU).will(returnValue(1));
    ret = find_closest_node(0, &nid);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, nid);
}

TEST_F(AcpiMemTest, InitAcpiMem)
{
    int ret;

    acpi_disabled = 1;
    ret = init_acpi_mem();
    EXPECT_EQ(-ENODEV, ret);
}

extern "C" acpi_status acpi_get_table(char *signature, u32 instance, struct acpi_table_header** out_table);
TEST_F(AcpiMemTest, InitAcpiMemOne)
{
    int ret;
    struct acpi_table_header *table_header = NULL;
    acpi_disabled = 0;
    MOCKER(acpi_get_table).stubs().with().will(returnValue(0));
    ret = init_acpi_mem();
    EXPECT_EQ(-ENODEV, ret);
}

extern "C" int acpi_parse_entries_array(char *id, unsigned long table_size,
    struct acpi_table_header *table_header,
    struct acpi_subtable_proc *proc, int proc_num,
    unsigned int max_entries);
extern "C" void acpi_put_table(struct acpi_table_header *table);
TEST_F(AcpiMemTest, InitAcpiMemTwo)
{
    int ret = 0;
    int i = 0;
    struct acpi_table_header *table_header;
    struct acpi_mem_segment *ams1;
    table_header = (struct acpi_table_header *)kmalloc(sizeof(*table_header), GFP_KERNEL);
    ASSERT_NE(nullptr, table_header);
    table_header->length = 1;
    ams1 = (struct acpi_mem_segment *)kmalloc(sizeof(*ams1), GFP_KERNEL);
    ASSERT_NE(nullptr, ams1);
    // 1024个2M页，524288个4K页
    ams1->start = 0x0;
    ams1->end = 0x7fffffff;
    ams1->node = 0;
    list_add_tail(&ams1->segment, &acpi_mem.mem);
    acpi_mem.len = 1;
    for (i = 0; i < SMAP_MAX_NUMNODES; i++) {
        acpi_mem_cached[i].online = false;
    }
    for (i = 0; i < SMAP_MAX_NUMNODES; i++) {
        acpi_mem_cached[0].distance[i] = 0;
    }
    acpi_disabled = 0;

    MOCKER(acpi_get_table)
        .stubs()
        .with(any(), any(), outBoundP(&table_header, sizeof(struct acpi_table_header)))
        .will(returnValue(0));
    MOCKER(acpi_parse_entries_array).stubs().with().will(returnValue(0));
    MOCKER(acpi_put_table).stubs().will(ignoreReturnValue());
    MOCKER(calc_node_distance).stubs().will(ignoreReturnValue());
    ret = init_acpi_mem();
    EXPECT_EQ(0, ret);

    nr_local_numa = 0;
    list_del(&ams1->segment);
    acpi_mem.len = 0;
    kfree(table_header);
    kfree(ams1);
}

TEST_F(AcpiMemTest, InitAcpiMemThree)
{
    int ret = 0;
    int i = 0;
    struct acpi_table_header *table_header;
    struct acpi_mem_segment *ams1;
    table_header = (struct acpi_table_header *)kmalloc(sizeof(*table_header), GFP_KERNEL);
    table_header->length = 1;
    ams1 = (struct acpi_mem_segment *)kmalloc(sizeof(*ams1), GFP_KERNEL);
    ASSERT_NE(nullptr, ams1);
    ams1->start = 0x0;
    ams1->end = 0x7fffffff;
    ams1->node = 15;
    list_add_tail(&ams1->segment, &acpi_mem.mem);
    acpi_mem.len = 1;
    for (i = 0; i < SMAP_MAX_NUMNODES; i++) {
        acpi_mem_cached[i].online = false;
    }
    for (i = 0; i < SMAP_MAX_NUMNODES; i++) {
        acpi_mem_cached[0].distance[i] = 0;
    }
    acpi_disabled = 0;

    MOCKER(acpi_get_table)
        .stubs()
        .with(any(), any(), outBoundP(&table_header, sizeof(struct acpi_table_header)))
        .will(returnValue(0));
    MOCKER(acpi_parse_entries_array).stubs().with().will(returnValue(0));
    MOCKER(acpi_put_table).stubs().will(ignoreReturnValue());
    MOCKER(calc_node_distance).stubs().will(ignoreReturnValue());
    ret = init_acpi_mem();
    EXPECT_EQ(0, ret);

    nr_local_numa = 0;
    list_del(&ams1->segment);
    acpi_mem.len = 0;
    kfree(table_header);
    kfree(ams1);
}

TEST_F(AcpiMemTest, InitAcpiMemFour)
{
    int ret = 0;
    int i = 0;
    struct acpi_table_header *table_header;
    struct acpi_mem_segment *ams1;
    table_header = (struct acpi_table_header *)kmalloc(sizeof(*table_header), GFP_KERNEL);
    table_header->length = 1;
    ams1 = (struct acpi_mem_segment *)kmalloc(sizeof(*ams1), GFP_KERNEL);
    ASSERT_NE(nullptr, ams1);
    // 1024个2M页，524288个4K页
    ams1->start = 0x0;
    ams1->end = 0x7fffffff;
    ams1->node = -1;
    list_add_tail(&ams1->segment, &acpi_mem.mem);
    acpi_mem.len = 1;
    for (i = 0; i < SMAP_MAX_NUMNODES; i++) {
        acpi_mem_cached[i].online = false;
    }
    for (i = 0; i < SMAP_MAX_NUMNODES; i++) {
        acpi_mem_cached[0].distance[i] = 0;
    }
    acpi_disabled = 0;

    MOCKER(acpi_get_table)
        .stubs()
        .with(any(), any(), outBoundP(&table_header, sizeof(struct acpi_table_header)))
        .will(returnValue(0));
    MOCKER(acpi_parse_entries_array).stubs().with().will(returnValue(0));
    MOCKER(acpi_put_table).stubs().will(ignoreReturnValue());
    MOCKER(calc_node_distance).stubs().will(ignoreReturnValue());
    ret = init_acpi_mem();
    EXPECT_EQ(-ERANGE, ret);

    nr_local_numa = 0;
    list_del(&ams1->segment);
    acpi_mem.len = 0;
    kfree(table_header);
    kfree(ams1);
}

TEST_F(AcpiMemTest, InitAcpiMemFive)
{
    int ret = 0;
    int i = 0;
    struct acpi_table_header *table_header;
    struct acpi_mem_segment *ams1;
    table_header = (struct acpi_table_header *)kmalloc(sizeof(*table_header), GFP_KERNEL);
    table_header->length = 1;
    ams1 = (struct acpi_mem_segment *)kmalloc(sizeof(*ams1), GFP_KERNEL);
    ASSERT_NE(nullptr, ams1);
    // 1024个2M页，524288个4K页
    ams1->start = 0x0;
    ams1->end = 0x7fffffff;
    ams1->node = -1;
    list_add_tail(&ams1->segment, &acpi_mem.mem);
    acpi_mem.len = 1;
    acpi_disabled = 0;

    MOCKER(acpi_get_table)
        .stubs()
        .with(any(), any(), outBoundP(&table_header, sizeof(struct acpi_table_header)))
        .will(returnValue(0));
    MOCKER(acpi_parse_entries_array).stubs().with().will(returnValue(0));
    MOCKER(acpi_put_table).stubs().will(ignoreReturnValue());
    MOCKER(calc_node_distance).stubs().will(ignoreReturnValue());
    ret = init_acpi_mem();
    EXPECT_EQ(-ERANGE, ret);

    nr_local_numa = 0;
    list_del(&ams1->segment);
    acpi_mem.len = 0;
    kfree(table_header);
    kfree(ams1);
}

TEST_F(AcpiMemTest, InitAcpiMemSix)
{
    int ret = 0;
    int i = 0;
    struct acpi_table_header *table_header;
    struct acpi_mem_segment *ams1;
    table_header = (struct acpi_table_header *)kmalloc(sizeof(*table_header), GFP_KERNEL);
    table_header->length = 1;
    ams1 = (struct acpi_mem_segment *)kmalloc(sizeof(*ams1), GFP_KERNEL);
    ASSERT_NE(nullptr, ams1);
    // 1024个2M页，524288个4K页
    ams1->start = 0x0;
    ams1->end = 0x7fffffff;
    ams1->node = 15;
    list_add_tail(&ams1->segment, &acpi_mem.mem);
    acpi_mem.len = 1;
    for (i = 0; i < SMAP_MAX_NUMNODES; i++) {
        acpi_mem_cached[i].online = true;
    }
    for (i = 0; i < SMAP_MAX_NUMNODES; i++) {
        acpi_mem_cached[0].distance[i] = 10;
    }
    acpi_disabled = 0;

    MOCKER(acpi_get_table)
        .stubs()
        .with(any(), any(), outBoundP(&table_header, sizeof(struct acpi_table_header)))
        .will(returnValue(0));
    MOCKER(acpi_parse_entries_array).stubs().with().will(returnValue(0));
    MOCKER(acpi_put_table).stubs().will(ignoreReturnValue());
    MOCKER(calc_node_distance).stubs().will(ignoreReturnValue());
    ret = init_acpi_mem();
    EXPECT_EQ(0, ret);

    nr_local_numa = 0;
    list_del(&ams1->segment);
    acpi_mem.len = 0;
    kfree(table_header);
    kfree(ams1);
}

TEST_F(AcpiMemTest, InitAcpiMemSeven)
{
    int ret = 0;
    int i = 0;
    struct acpi_table_header *table_header;
    struct acpi_mem_segment *ams1;
    table_header = (struct acpi_table_header *)kmalloc(sizeof(*table_header), GFP_KERNEL);
    table_header->length = 1;
    ams1 = (struct acpi_mem_segment *)kmalloc(sizeof(*ams1), GFP_KERNEL);
    ASSERT_NE(nullptr, ams1);
    // 1024个2M页，524288个4K页
    ams1->start = 0x1;
    ams1->end = 0x7fffffff;
    ams1->node = 150;
    list_add_tail(&ams1->segment, &acpi_mem.mem);
    acpi_mem.len = 1;
    for (i = 0; i < SMAP_MAX_NUMNODES; i++) {
        acpi_mem_cached[i].online = false;
    }
    for (i = 0; i < SMAP_MAX_NUMNODES; i++) {
        acpi_mem_cached[0].distance[i] = 100;
    }
    acpi_disabled = 0;

    MOCKER(acpi_get_table)
        .stubs()
        .with(any(), any(), outBoundP(&table_header, sizeof(struct acpi_table_header)))
        .will(returnValue(0));
    MOCKER(acpi_parse_entries_array).stubs().with().will(returnValue(0));
    MOCKER(acpi_put_table).stubs().will(ignoreReturnValue());
    MOCKER(calc_node_distance).stubs().will(ignoreReturnValue());
    ret = init_acpi_mem();
    EXPECT_EQ(-ERANGE, ret);

    nr_local_numa = 0;
    list_del(&ams1->segment);
    acpi_mem.len = 0;
    kfree(table_header);
    kfree(ams1);
}

TEST_F(AcpiMemTest, InitAcpiMemEight)
{
    int ret;
    struct acpi_mem_segment *mem;
    struct acpi_mem_segment *tmp;
    struct acpi_mem_segment mem1;
    struct acpi_table_header *table_header = (struct acpi_table_header *)malloc(sizeof(struct acpi_table_header));

    acpi_disabled = 0;
    MOCKER(acpi_get_table)
        .stubs()
        .with(any(), any(), outBoundP(&table_header, sizeof(table_header)))
        .will(ignoreReturnValue());
    MOCKER(acpi_parse_entries_array).stubs().will(returnValue(0));
    if (!list_empty(&acpi_mem.mem)) {
        list_for_each_entry_safe(mem, tmp, &acpi_mem.mem, segment)
        {
            list_del(&mem->segment);
            kfree(mem);
        }
    }
    mem1.node = 0;
    mem1.pxm = 0;
    mem1.start = 0;
    mem1.end = 0;
    list_add(&mem1.segment, &acpi_mem.mem);
    MOCKER(calc_node_distance).stubs();
    ret = init_acpi_mem();
    EXPECT_EQ(0, ret);
    list_del(&mem1.segment);
    nr_local_numa = 0;
}


TEST_F(AcpiMemTest, GetNodeActcLen)
{
    int ret;
    int len;
    u64 *node_len;
    struct acpi_mem_segment *ams1;
    struct acpi_mem_segment *ams2;
    struct acpi_mem_segment *ams3;

    len = 0;
    ret = get_node_actc_len(len, node_len);
    EXPECT_EQ(-EINVAL, ret);

    len = 2;
    node_len = (u64*)kzalloc(2 * sizeof(*node_len), GFP_KERNEL);
    ASSERT_NE(nullptr, node_len);
    ret = get_node_actc_len(len, node_len);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0, node_len[0]);
    EXPECT_EQ(0, node_len[1]);

    ams1 = (struct acpi_mem_segment *)kmalloc(sizeof(*ams1), GFP_KERNEL);
    ams2 = (struct acpi_mem_segment *)kmalloc(sizeof(*ams2), GFP_KERNEL);
    ams3 = (struct acpi_mem_segment *)kmalloc(sizeof(*ams3), GFP_KERNEL);
    ASSERT_NE(nullptr, ams1);
    ASSERT_NE(nullptr, ams2);
    ASSERT_NE(nullptr, ams3);
    // 约等于2G
    ams1->start = 0x1000;
    ams1->end = 0x7FFFFFFF;
    ams1->node = 0;
    // 4G
    ams2->start = 0x100000000;
    ams2->end = 0x1FFFFFFFF;
    ams2->node = 0;
    // 2G
    ams3->start = 0x200000000;
    ams3->end = 0x27FFFFFFF;
    ams3->node = 1;
    list_add_tail(&ams1->segment, &acpi_mem.mem);
    list_add_tail(&ams2->segment, &acpi_mem.mem);
    list_add_tail(&ams3->segment, &acpi_mem.mem);
    acpi_mem.len = 3;

    ret = get_node_actc_len(len, node_len);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(1024 + 2048, node_len[0]);
    ASSERT_EQ(1024, node_len[1]);
}

extern "C" bool is_smap_pg_huge(void);
TEST_F(AcpiMemTest, CalcPaddrAcidxNoCache)
{
    u64 addr;
    int nid;
    u64 index;
    int ret;

    struct acpi_mem_segment *ams1;
    struct acpi_mem_segment *ams2;
    struct acpi_mem_segment *ams3;
    ams1 = (struct acpi_mem_segment *)kmalloc(sizeof(*ams1), GFP_KERNEL);
    ams2 = (struct acpi_mem_segment *)kmalloc(sizeof(*ams2), GFP_KERNEL);
    ams3 = (struct acpi_mem_segment *)kmalloc(sizeof(*ams3), GFP_KERNEL);
    ASSERT_NE(nullptr, ams1);
    ASSERT_NE(nullptr, ams2);
    ASSERT_NE(nullptr, ams3);
    // 1024个2M页，524288个4K页
    ams1->start = 0x0;
    ams1->end = 0x7fffffff;
    ams1->node = 0;
    // 4个2M页，2048个4K页
    ams2->start = 0x2080000000;
    ams2->end = 0x20807fffff;
    ams2->node = 0;
    // 16384个2M页，8388608个4K页
    ams3->start = 0x202000000000;
    ams3->end = 0x2027ffffffff;
    ams3->node = 1;
    list_add_tail(&ams1->segment, &acpi_mem.mem);
    list_add_tail(&ams2->segment, &acpi_mem.mem);
    list_add_tail(&ams3->segment, &acpi_mem.mem);
    acpi_mem.len = 3;

    // disable cache
    for (nid = 0; nid < SMAP_MAX_NUMNODES; nid++) {
        acpi_mem_cached[nid].start = acpi_mem_cached[nid].end = 0;
    }

    // migrate page is 2M
    MOCKER(is_smap_pg_huge).stubs().will(returnValue(true));

    addr = 0x0;
    ret = calc_paddr_acidx(addr, &nid, &index);
    ASSERT_EQ(0, ret);
    EXPECT_EQ(0, nid);
    EXPECT_EQ(0, index);

    addr = 0x70123000;
    ret = calc_paddr_acidx(addr, &nid, &index);
    ASSERT_EQ(0, ret);
    EXPECT_EQ(0, nid);
    EXPECT_EQ(896, index);

    addr = 0x7fffffff;
    ret = calc_paddr_acidx(addr, &nid, &index);
    ASSERT_EQ(0, ret);
    EXPECT_EQ(0, nid);
    EXPECT_EQ(1023, index);

    addr = 0x80000000;
    ret = calc_paddr_acidx(addr, &nid, &index);
    EXPECT_EQ(-ERANGE, ret);

    // 空洞后首个个地址段的第一个地址
    addr = 0x2080000000;
    ret = calc_paddr_acidx(addr, &nid, &index);
    ASSERT_EQ(0, ret);
    EXPECT_EQ(0, nid);
    EXPECT_EQ(1024, index);

    // 空洞后的第1段地址的，第3个2M页
    addr = 0x2080400000;
    ret = calc_paddr_acidx(addr, &nid, &index);
    ASSERT_EQ(0, ret);
    EXPECT_EQ(0, nid);
    EXPECT_EQ(1026, index);

    // 空洞后的第2段地址的，第4096个2M页
    addr = 0x202200000000;
    ret = calc_paddr_acidx(addr, &nid, &index);
    ASSERT_EQ(0, ret);
    EXPECT_EQ(1, nid);
    EXPECT_EQ(4096, index);

    // 内核空间地址返回-ERANGE
    addr = 0x00c8b8c0c4b8c8c0;
    ret = calc_paddr_acidx(addr, &nid, &index);
    ASSERT_EQ(-ERANGE, ret);

    // migrate page is 4K
    GlobalMockObject::verify();
    MOCKER(is_smap_pg_huge).stubs().will(returnValue(false));

    addr = 0x70123000;
    ret = calc_paddr_acidx(addr, &nid, &index);
    ASSERT_EQ(0, ret);
    EXPECT_EQ(0, nid);
    EXPECT_EQ(459043, index);

    addr = 0x2080400000;
    ret = calc_paddr_acidx(addr, &nid, &index);
    ASSERT_EQ(0, ret);
    EXPECT_EQ(0, nid);
    EXPECT_EQ(525312, index);

    addr = 0x202000201234;
    ret = calc_paddr_acidx(addr, &nid, &index);
    ASSERT_EQ(0, ret);
    EXPECT_EQ(1, nid);
    EXPECT_EQ(513, index);
}

TEST_F(AcpiMemTest, CalcPaddrAcidxCache)
{
    u64 addr;
    int nid;
    u64 index;
    int ret;

    acpi_mem_cached[5].start = 0x20123000;
    acpi_mem_cached[5].end = 0x20654000;

    // migrate page is 2M
    MOCKER(is_smap_pg_huge).stubs().will(returnValue(true));

    addr = 0x20323000;
    ret = calc_paddr_acidx(addr, &nid, &index);
    EXPECT_EQ(-ERANGE, ret);
}

extern "C" bool is_paddr_local(u64 pa);
extern "C" struct mem_info acpi_mem;
TEST_F(AcpiMemTest, IsPaddrLocalNormal)
{
    u64 pa = 10;
    struct acpi_mem_segment *mem_temp = (struct acpi_mem_segment*)kzalloc(sizeof(struct acpi_mem_segment), GFP_KERNEL);
    ASSERT_NE(nullptr, mem_temp);
    mem_temp->start = 0;
    mem_temp->end = 100;
    INIT_LIST_HEAD(&acpi_mem.mem);
    list_add_tail(&mem_temp->segment, &acpi_mem.mem);
    bool ret = is_paddr_local(pa);
    EXPECT_EQ(true, ret);
    list_del(&mem_temp->segment);
    kfree(mem_temp);
}

TEST_F(AcpiMemTest, IsPaddrLocalAbnormal)
{
    u64 pa = 1025;
    struct acpi_mem_segment *mem_temp = (struct acpi_mem_segment*)kzalloc(sizeof(struct acpi_mem_segment), GFP_KERNEL);
    ASSERT_NE(nullptr, mem_temp);
    mem_temp->start = 0;
    mem_temp->end = 100;
    INIT_LIST_HEAD(&acpi_mem.mem);
    list_add_tail(&mem_temp->segment, &acpi_mem.mem);
    bool ret = is_paddr_local(pa);
    EXPECT_EQ(false, ret);
    list_del(&mem_temp->segment);
    kfree(mem_temp);
}

extern "C" int acpi_table_build_mem(struct acpi_subtable_header *header);
TEST_F(AcpiMemTest, AcpiTableBuildMem)
{
    struct acpi_subtable_header header;
    struct acpi_srat_mem_affinity srat_mem;
    srat_mem.flags = 0;
    header.length = sizeof(srat_mem);
    srat_mem.header = header;
    int ret = acpi_table_build_mem((struct acpi_subtable_header *)&srat_mem);
    EXPECT_EQ(0, ret);
}

TEST_F(AcpiMemTest, AcpiTableBuildMemOne)
{
    struct acpi_subtable_header header;
    struct acpi_srat_mem_affinity srat_mem;
    srat_mem.flags = ACPI_SRAT_MEM_ENABLED | ACPI_SRAT_MEM_HOT_PLUGGABLE;
    header.length = sizeof(srat_mem);
    srat_mem.header = header;
    int ret = acpi_table_build_mem((struct acpi_subtable_header *)&srat_mem);
    EXPECT_EQ(0, ret);
}

TEST_F(AcpiMemTest, AcpiTableBuildMemTwo)
{
    struct acpi_subtable_header header;
    struct acpi_srat_mem_affinity srat_mem;
    srat_mem.flags = ACPI_SRAT_MEM_ENABLED;
    header.length = sizeof(srat_mem);
    srat_mem.header = header;
    MOCKER(kmalloc).stubs().will(returnValue((void *)nullptr));
    int ret = acpi_table_build_mem((struct acpi_subtable_header *)&srat_mem);
    EXPECT_EQ(-ENOMEM, ret);
}

extern "C" void print_acpi_mem(void);
TEST_F(AcpiMemTest, PrintAcpiMem)
{
    struct acpi_mem_segment *mem_temp = (struct acpi_mem_segment*)kzalloc(sizeof(struct acpi_mem_segment), GFP_KERNEL);
    ASSERT_NE(nullptr, mem_temp);
    mem_temp->start = 0;
    mem_temp->end = 100;
    INIT_LIST_HEAD(&acpi_mem.mem);
    list_add_tail(&mem_temp->segment, &acpi_mem.mem);
    print_acpi_mem();
    EXPECT_EQ(0, mem_temp->start);
    EXPECT_EQ(100, mem_temp->end);
    list_del(&mem_temp->segment);
    kfree(mem_temp);
}

extern "C" void reset_acpi_mem(void);
TEST_F(AcpiMemTest, ResetAcpiMem)
{
    struct acpi_mem_segment *mem_temp = (struct acpi_mem_segment*)kzalloc(sizeof(struct acpi_mem_segment), GFP_KERNEL);
    ASSERT_NE(nullptr, mem_temp);
    mem_temp->start = 0;
    mem_temp->end = 100;
    INIT_LIST_HEAD(&acpi_mem.mem);
    list_add_tail(&mem_temp->segment, &acpi_mem.mem);
    reset_acpi_mem();
    EXPECT_EQ(0, acpi_mem.len);
}

