/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: SMAP3.0 acpi helper module test code
 */

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include <linux/acpi.h>
#include "acpi_helper.h"

using namespace std;

class AcpiHelperTest : public ::testing::Test {
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
extern "C" acpi_sub_type acpi_get_subtable_type(char *id);

TEST_F(AcpiHelperTest, AcpiGetSubtableType)
{
    char *id1 = "ACPI";
    char *id2 = ACPI_SIG_HMAT;
    acpi_sub_type acpi_subtable_type1 = acpi_get_subtable_type(id2);
    EXPECT_TRUE(acpi_subtable_type1 == SUBTABLE_HMAT);
    acpi_sub_type acpi_subtable_type2 = acpi_get_subtable_type(id1);
    EXPECT_TRUE(acpi_subtable_type2 == SUBTABLE_COMMON);
}

extern "C" unsigned long acpi_get_subtable_header_length(struct acpi_subtable_entry *entry);

TEST_F(AcpiHelperTest, AcpiGetSubtableHeaderLength)
{
    struct acpi_subtable_entry entry1 = {
        .type = SUBTABLE_COMMON
    };
    struct acpi_subtable_entry entry2 = {
        .type = SUBTABLE_HMAT
    };
    struct acpi_subtable_entry *entryPoint1 = &entry1;
    struct acpi_subtable_entry *entryPoint2 = &entry2;
    unsigned long sizeof1 = acpi_get_subtable_header_length(entryPoint1);
    EXPECT_EQ(2, sizeof1);
    unsigned long sizeof2 = acpi_get_subtable_header_length(entryPoint2);
    EXPECT_EQ(8, sizeof2);
}

extern "C" unsigned long acpi_get_entry_type(struct acpi_subtable_entry *entry);

TEST_F(AcpiHelperTest, AcpiGetEntryType)
{
    union acpi_subtable_headers* headers1 =
        (union acpi_subtable_headers *)kmalloc(sizeof(union acpi_subtable_headers),  GFP_KERNEL);
    ASSERT_NE(nullptr, headers1);
    union acpi_subtable_headers* headers2 =
        (union acpi_subtable_headers *)kmalloc(sizeof(union acpi_subtable_headers),  GFP_KERNEL);
    ASSERT_NE(nullptr, headers2);
    headers1->common.type = 1;
    headers2->hmat.type = 2;
    struct acpi_subtable_entry entry1 = {
        .hdr = headers1,
        .type = SUBTABLE_COMMON
    };
    struct acpi_subtable_entry entry2 = {
        .hdr = headers2,
        .type = SUBTABLE_HMAT
    };
    struct acpi_subtable_entry *entryPoint1 = &entry1;
    struct acpi_subtable_entry *entryPoint2 = &entry2;
    unsigned long type1 = acpi_get_entry_type(entryPoint1);
    EXPECT_EQ(1, type1);
    unsigned long type2 = acpi_get_entry_type(entryPoint2);
    EXPECT_EQ(2, type2);
}

extern "C" unsigned long acpi_get_entry_length(struct acpi_subtable_entry *entry);

TEST_F(AcpiHelperTest, AcpiGetEntryLength)
{
    union acpi_subtable_headers* headers1 =
        (union acpi_subtable_headers *)kmalloc(sizeof(union acpi_subtable_headers),  GFP_KERNEL);
    ASSERT_NE(nullptr, headers1);
    union acpi_subtable_headers* headers2 =
        (union acpi_subtable_headers *)kmalloc(sizeof(union acpi_subtable_headers),  GFP_KERNEL);
    ASSERT_NE(nullptr, headers2);
    headers1->common.length = 1;
    headers2->hmat.length = 2;
    struct acpi_subtable_entry entry1 = {
        .hdr = headers1,
        .type = SUBTABLE_COMMON
    };
    struct acpi_subtable_entry entry2 = {
        .hdr = headers2,
        .type = SUBTABLE_HMAT
    };
    struct acpi_subtable_entry *entryPoint1 = &entry1;
    struct acpi_subtable_entry *entryPoint2 = &entry2;
    unsigned long length1 = acpi_get_entry_length(entryPoint1);
    EXPECT_EQ(1, length1);
    unsigned long length2 = acpi_get_entry_length(entryPoint2);
    EXPECT_EQ(2, length2);
}

TEST_F(AcpiHelperTest, AcpiParseEntriesArray)
{
    char *id1 = "ACPI";
    char *id2 = ACPI_SIG_HMAT;

    unsigned long tableSize = 1;

    struct acpi_table_header table_header1 = {
        .length = 1
    };
    struct acpi_table_header table_header2 = {
        .length = 10
    };

    struct acpi_subtable_proc proc[3] = {};
    proc[0].id = 1;
    proc[1].id = 2;
    proc[2].id = 2;
    proc[0].count = 0;
    proc[1].count = 0;
    proc[2].count = 0;
    int procNum = 3;

    unsigned int maxEntry1 = 0;
    unsigned int maxEntry2 = 100;

    proc[0].handler = [](union acpi_subtable_headers *header, const unsigned long end) {
        return int(1);
    };
    proc[1].handler = [](union acpi_subtable_headers *header, const unsigned long end) {
        return int(1);
    };
    proc[2].handler = [](union acpi_subtable_headers *header, const unsigned long end) {
        return int(1);
    };
    unsigned long cur = 0;
    MOCKER(acpi_get_subtable_type).stubs().with(id2).will(returnValue(SUBTABLE_HMAT));
    MOCKER(acpi_get_subtable_type).stubs().with(id1).will(returnValue(SUBTABLE_COMMON));

    MOCKER(acpi_get_subtable_header_length).stubs().with(any()).will(returnValue(static_cast<unsigned long>(2)));
    MOCKER(acpi_get_entry_type).stubs().with(any()).will(returnValue(static_cast<unsigned long>(2)));

    MOCKER(acpi_get_entry_length).defaults().with(any()).will(returnValue(static_cast<unsigned long>(0)));

    int ret1 = acpi_parse_entries_array(id1, tableSize, &table_header1, proc, procNum, maxEntry1);
    EXPECT_EQ(ret1, 0);
    int ret4 = acpi_parse_entries_array(id1, tableSize, &table_header2, proc, procNum, maxEntry1);
    EXPECT_EQ(ret4, -EINVAL);
    int ret2 = acpi_parse_entries_array(id1, tableSize, &table_header2, proc, procNum, maxEntry2);
    EXPECT_EQ(ret2, -EINVAL);

    proc[1].handler = nullptr;
    MOCKER(acpi_get_entry_length).stubs().with(any()).will(returnValue(static_cast<unsigned long>(1)));
    int ret3 = acpi_parse_entries_array(id1, tableSize, &table_header2, proc, procNum, maxEntry2);
    EXPECT_EQ(ret3, -EINVAL);
}
