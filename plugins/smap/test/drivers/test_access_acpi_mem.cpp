/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: SMAP3.0 access_acpi_mem模块测试代码
 */

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include <linux/types.h>
#include "access_acpi_helper.h"
#include "access_acpi_mem.h"

using namespace std;

class DriversAcpiMemTest : public ::testing::Test {
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

extern "C" struct mem_info drivers_acpi_mem;
extern "C" bool drivers_is_paddr_local(u64 pa);
TEST_F(DriversAcpiMemTest, IsPaddrLocalNormal)
{
    u64 pa = 10;
    struct acpi_mem_segment *mem_temp = (struct acpi_mem_segment *)kzalloc(sizeof(struct acpi_mem_segment), GFP_KERNEL);
    ASSERT_NE(nullptr, mem_temp);
    mem_temp->start = 0;
    mem_temp->end = 100;
    INIT_LIST_HEAD(&drivers_acpi_mem.mem);
    list_add_tail(&mem_temp->segment, &drivers_acpi_mem.mem);
    bool ret = drivers_is_paddr_local(pa);
    EXPECT_EQ(true, ret);
    list_del(&mem_temp->segment);
    kfree(mem_temp);
}

TEST_F(DriversAcpiMemTest, IsPaddrLocalAbnormal)
{
    u64 pa = 1025;
    bool ret = drivers_is_paddr_local(pa);
    EXPECT_EQ(false, ret);
}

extern "C" int pxm_to_node(int pxm);
extern "C" int drivers_acpi_table_build_mem(struct acpi_subtable_header *header);
TEST_F(DriversAcpiMemTest, AcpiTableBuildMemOne)
{
    struct acpi_srat_mem_affinity tmp;
    tmp.flags = 0;
    int ret = drivers_acpi_table_build_mem(&tmp.header);
    EXPECT_EQ(0, ret);

    tmp.flags = 1;
    MOCKER(kmalloc).stubs().will(returnValue((void *)nullptr));
    ret = drivers_acpi_table_build_mem(&tmp.header);
    EXPECT_EQ(-ENOMEM, ret);
}

TEST_F(DriversAcpiMemTest, AcpiTableBuildMemTwo)
{
    struct acpi_srat_mem_affinity tmp;
    struct acpi_mem_segment *am;
    struct acpi_mem_segment *tmp1;
    tmp.flags = 1;
    tmp.base_address = 0;
    tmp.length = 10;
    MOCKER(pxm_to_node).stubs().will(returnValue(1));
    int ret = drivers_acpi_table_build_mem(&tmp.header);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, drivers_acpi_mem.len);
    list_for_each_entry_safe(am, tmp1, &drivers_acpi_mem.mem, segment)
    {
        list_del(&am->segment);
    }
}

TEST_F(DriversAcpiMemTest, AcpiTableBuildMemThree)
{
    struct acpi_srat_mem_affinity tmp;
    struct acpi_mem_segment *am;
    struct acpi_mem_segment *tmp1;
    struct acpi_mem_segment node1;
    struct acpi_mem_segment node2;
    node1.start = 1;
    node2.start = 15;
    tmp.flags = 1;
    tmp.base_address = 10;
    tmp.length = 10;
    drivers_acpi_mem.len = 0;
    MOCKER(pxm_to_node).stubs().will(returnValue(1));
    list_add_tail(&node1.segment, &drivers_acpi_mem.mem);
    int ret = drivers_acpi_table_build_mem(&tmp.header);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, drivers_acpi_mem.len);

    list_add(&node2.segment, &drivers_acpi_mem.mem);
    ret = drivers_acpi_table_build_mem(&tmp.header);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(2, drivers_acpi_mem.len);
    list_for_each_entry_safe(am, tmp1, &drivers_acpi_mem.mem, segment)
    {
        list_del(&am->segment);
    }
}

extern "C" int drivers_acpi_parse_memory_affinity(union acpi_subtable_headers *header, const unsigned long end);
TEST_F(DriversAcpiMemTest, AcpiParseMemoryAffinity)
{
    union acpi_subtable_headers *header = NULL;
    struct acpi_srat_mem_affinity *p = NULL;
    p = (struct acpi_srat_mem_affinity *)kzalloc(sizeof(struct acpi_srat_mem_affinity), GFP_KERNEL);
    ASSERT_NE(nullptr, p);
    MOCKER(drivers_acpi_table_build_mem).stubs().will(returnValue(1));
    int ret = drivers_acpi_parse_memory_affinity(header, 0);
    EXPECT_EQ(1, ret);
    kfree(p);
}

extern "C" int acpi_parse_entries_array(char *id, unsigned long table_size, struct acpi_table_header *table_header,
    struct acpi_subtable_proc *proc, int proc_num, unsigned int max_entries);
extern "C" int drivers_init_acpi_mem(void);
TEST_F(DriversAcpiMemTest, InitAcpiMem)
{
    acpi_disabled = 1;
    int ret = drivers_init_acpi_mem();
    EXPECT_EQ(-ENODEV, ret);

    acpi_disabled = 0;
    MOCKER(acpi_parse_entries_array).stubs();
    ret = drivers_init_acpi_mem();
    EXPECT_EQ(-ENODEV, ret);
}

extern "C" void drivers_reset_acpi_mem(void);
TEST_F(DriversAcpiMemTest, ResetAcpiMem)
{
    struct acpi_mem_segment *mem_temp = (struct acpi_mem_segment *)kzalloc(sizeof(struct acpi_mem_segment), GFP_KERNEL);
    ASSERT_NE(nullptr, mem_temp);
    INIT_LIST_HEAD(&drivers_acpi_mem.mem);
    list_add_tail(&mem_temp->segment, &drivers_acpi_mem.mem);
    drivers_reset_acpi_mem();
    EXPECT_EQ(0, drivers_acpi_mem.len);
}

extern "C" u64 drivers_get_node_actc_len(int node_id, int page_size);
TEST_F(DriversAcpiMemTest, GetNodeActcLen)
{
    struct acpi_mem_segment mem;
    struct acpi_mem_segment *am;
    struct acpi_mem_segment *tmp;
    mem.node = 1;
    mem.start = 0;
    mem.end = 4194303;
    list_for_each_entry_safe(am, tmp, &drivers_acpi_mem.mem, segment) {
        list_del(&am->segment);
        kfree(am);
    }
    list_add(&mem.segment, &drivers_acpi_mem.mem);
    int ret = drivers_get_node_actc_len(1, PAGE_SIZE_2M);
    EXPECT_EQ(2, ret);

    ret = drivers_get_node_actc_len(1, PAGE_SIZE_4K);
    EXPECT_EQ(1024, ret);
    list_del(&mem.segment);
}

extern "C" int calc_paddr_acidx_acpi(u64 paddr, int *nid, u64 *index, int page_size);
TEST_F(DriversAcpiMemTest, CalcPaddrAcidxIomemAcpi)
{
    u64 index = 3;
    u64 paddr = 11;
    int nid = 2;
    int pagesize = 4096;

    struct acpi_mem_segment mem;
    mem.node = 1;
    mem.start = 0;
    mem.end = 10;
    list_add(&mem.segment, &drivers_acpi_mem.mem);
    int ret = calc_paddr_acidx_acpi(paddr, &nid, &index, pagesize);
    EXPECT_EQ(-ERANGE, ret);

    paddr = 5;
    ret = calc_paddr_acidx_acpi(paddr, &nid, &index, pagesize);
    EXPECT_EQ(0, ret);
    list_del(&mem.segment);
}

TEST_F(DriversAcpiMemTest, CalcPaddrAcidxIomemAcpiTwo)
{
    int nid = 0;
    u64 index = 0;
    struct acpi_mem_segment mem;
    mem.node = 1;
    mem.start = 5;
    mem.end = 10;
    list_add(&mem.segment, &drivers_acpi_mem.mem);
    int ret = calc_paddr_acidx_acpi(1, &nid, &index, 4096);
    EXPECT_EQ(-ERANGE, ret);
    list_del(&mem.segment);
}

TEST_F(DriversAcpiMemTest, CalcAcidxPaddrAcpi)
{
    u64 paddr;
    struct acpi_mem_segment mem;

    EXPECT_EQ(true, list_empty(&drivers_acpi_mem.mem));
    mem.node = 1;
    mem.end = 0xFFFF;
    mem.start = 0;
    list_add(&mem.segment, &drivers_acpi_mem.mem);
    int ret = calc_acidx_paddr_acpi(1, 1, &paddr, PAGE_SIZE);
    EXPECT_EQ(0, ret);
    list_del(&mem.segment);
}
