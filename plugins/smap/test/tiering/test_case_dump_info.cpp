/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: SMAP 测试代码
 */

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include <linux/fs.h>
#include <linux/kmod.h>
#include <linux/umh.h>
#include <linux/slab.h>
#include "dump_info.h"

extern struct list_head pid_list;

class DumpInfoTest : public ::testing::Test {
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

extern "C" int check_and_create_dir(char *dir);
extern "C" bool IS_ERR(const void *ptr);
extern "C" long __must_check PTR_ERR(__force const void *ptr);
extern "C" int call_usermodehelper(char const*, char**, char**, int);
TEST_F(DumpInfoTest, check_and_create_dir_dt)
{
    char dir[] = "qwe";
    int ret = check_and_create_dir(dir);
    EXPECT_EQ(ret, 0);
    MOCKER(IS_ERR).stubs().will(returnValue(false));
    MOCKER(call_usermodehelper).stubs().will(returnValue(0));
    ret = check_and_create_dir(dir);
    EXPECT_EQ(ret, 0);
    GlobalMockObject::reset();
    MOCKER(IS_ERR).stubs().will(returnValue(true));
    MOCKER(call_usermodehelper).stubs().will(returnValue(1)).then(returnValue(0));
    MOCKER(PTR_ERR).stubs().will(returnValue(-2));
    ret = check_and_create_dir(dir);
    EXPECT_EQ(ret, 1);
    ret = check_and_create_dir(dir);
    EXPECT_EQ(ret, 0);
}
extern "C" int check_filesize(char *filepath, loff_t max_sz, int *exceed);
TEST_F(DumpInfoTest, check_filesize_dt)
{
    char filepath[] = "qwe";
    loff_t max_sz = 0;
    int exceed = 0;
    int ret = check_filesize(filepath, max_sz, &exceed);
    EXPECT_EQ(ret, 0);
    MOCKER(IS_ERR).stubs().will(returnValue(false)).then(returnValue(true));
    ret = check_filesize(filepath, max_sz, &exceed);
    EXPECT_EQ(ret, 0);
    MOCKER(PTR_ERR).stubs().will(returnValue(-2));
    ret = check_filesize(filepath, max_sz, &exceed);
    EXPECT_EQ(ret, 0);
}
extern "C" int rename_file(char *old_name, char *new_name);
TEST_F(DumpInfoTest, rename_file_dt)
{
    char old_name[] = "qwe";
    char new_name[] = "asd";
    int ret = rename_file(old_name, new_name);
    EXPECT_EQ(ret, 0);
    MOCKER(call_usermodehelper).stubs().will(returnValue(1));
    ret = rename_file(old_name, new_name);
    EXPECT_EQ(ret, 1);
}
extern "C" int backup_file_if_needed(char *dir, char *name, loff_t max_sz);
TEST_F(DumpInfoTest, backup_file_if_needed_dt)
{
    char dir[] = "qwe";
    char name[] = "asd";
    loff_t max_sz = 0;
    int ret = backup_file_if_needed(dir, name, max_sz);
    EXPECT_EQ(ret, 0);
    MOCKER(check_filesize).stubs().will(returnValue(1)).then(returnValue(0));
    ret = backup_file_if_needed(dir, name, max_sz);
    EXPECT_EQ(ret, 1);
    GlobalMockObject::reset();
    MOCKER(IS_ERR).stubs().will(returnValue(false));
    MOCKER(rename_file).stubs().will(returnValue(1));
    max_sz = -20;
    ret = backup_file_if_needed(dir, name, max_sz);
    EXPECT_EQ(ret, 1);
}

extern "C" void filter_4k_migrate_info(void);
TEST_F(DumpInfoTest, Filter4kMigrateInfo)
{
    filter_4k_migrate_info();
    GlobalMockObject::verify();
}