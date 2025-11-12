/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <gtest/gtest.h>
#include "mockcpp/mokc.h"
 
#include "linux_mock.h"
#include "ucache.h"
 
using namespace std;
 
class CoreTest : public ::testing::Test {
protected:
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};
 
extern "C" {
int ucache_open(struct inode *inode, struct file *file);
int ucache_query_migrate_success(uintptr_t arg);
struct migrate_success *get_migrate_success(int nid);
}
 
TEST_F(CoreTest, QueryMigrateSuccess)
{
    struct inode ino = {0};
    struct file file = {0};
    EXPECT_EQ(ucache_open(&ino, &file), 0);
 
    unsigned long user_arg;
    struct migrate_success tmp = {1, 1, 1};

    MOCKER(copy_to_user).stubs().will(returnValue(false));
    int err = ucache_query_migrate_success(user_arg);
    EXPECT_EQ(err, -EINVAL);
    
    GlobalMockObject::verify();
    MOCKER(copy_from_user).stubs().will(returnValue(true));
    MOCKER(get_migrate_success).defaults().will(returnValue(static_cast<void *>(nullptr)));
    err = ucache_query_migrate_success(user_arg);
    EXPECT_EQ(err, 1);
}
 
extern "C" int dev_ucache_migrate_folios(uintptr_t arg);
TEST_F(CoreTest, UcacheMigrateFolios)
{
    unsigned long user_arg;
    MOCKER(ucache_scan_folios).defaults().will(returnValue(0));
    MOCKER(ucache_migrate_folios).defaults().will(returnValue(0));
    int err = dev_ucache_migrate_folios(user_arg);
    EXPECT_EQ(err, 0);
 
    MOCKER(ucache_migrate_folios).stubs().will(returnValue(-EINVAL));
    err = dev_ucache_migrate_folios(user_arg);
    EXPECT_EQ(err, -EINVAL);
 
    GlobalMockObject::verify();
    MOCKER(ucache_scan_folios).defaults().will(returnValue(-1));
    err = dev_ucache_migrate_folios(user_arg);
    EXPECT_EQ(err, -1);
 
    unsigned int nr_folios = 0;
    MOCKER(ucache_scan_folios)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), mockcpp::any(), outBoundP(&nr_folios, sizeof(nr_folios)))
        .will(returnValue(0));
    err = dev_ucache_migrate_folios(user_arg);
    EXPECT_EQ(err, 0);
 
    MOCKER(copy_from_user).stubs().will(returnValue(true));
    err = dev_ucache_migrate_folios(user_arg);
    EXPECT_EQ(err, 1);
}
 
extern "C" long ucache_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
TEST_F(CoreTest, UcacheIoctl)
{
    struct file filp = {0};
    unsigned long user_arg;
    int err = ucache_ioctl(&filp, 100, user_arg);
    EXPECT_EQ(err, -EINVAL);
 
    MOCKER(ucache_query_migrate_success).stubs().will(returnValue(0));
    err = ucache_ioctl(&filp, UCACHE_QUERY_MIGRATE_SUCCESS, user_arg);
    EXPECT_EQ(err, 0);
 
    MOCKER(dev_ucache_migrate_folios).stubs().will(returnValue(0));
    err = ucache_ioctl(&filp, UCACHE_SCAN_MIGRATE_FOLIOS, user_arg);
    EXPECT_EQ(err, 0);
}
 
extern "C" {
int __init ucache_core_init(void);
void __exit ucache_core_exit(void);
}
 
TEST_F(CoreTest, UcacheCoreInit)
{
    int err = ucache_core_init();
    EXPECT_EQ(err, -ENOMEM);
 
    struct class1 cls = {0};
    MOCKER(class_create).stubs().will(returnValue(&cls));
    MOCKER(IS_ERR).stubs().will(returnValue(false));
    err = ucache_core_init();
    EXPECT_EQ(err, -ENOMEM);
 
    MOCKER(cdev_add).stubs().will(returnValue(-1));
    err = ucache_core_init();
    EXPECT_EQ(err, -1);
 
    MOCKER(alloc_chrdev_region).stubs().will(returnValue(-1));
    err = ucache_core_init();
    EXPECT_EQ(err, -1);
 
    ucache_core_exit();
}