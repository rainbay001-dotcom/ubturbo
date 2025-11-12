/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: SMAP3.0 migrate_back_debugfs.c测试代码
 */

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include <linux/dcache.h>
#include <linux/slab.h>
#include "migrate_back_debugfs.h"
#include "migrate_task.h"

using namespace std;

class MigrateBackDebugfsTest : public ::testing::Test {
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

TEST_F(MigrateBackDebugfsTest, SmapMigrateRead)
{
    struct file *fileTmp;
    struct migrate_back_task *taskTmp;
    fileTmp = (struct file *)kmalloc(sizeof(*fileTmp), GFP_KERNEL);
    ASSERT_NE(nullptr, fileTmp);
    taskTmp = (struct migrate_back_task *)kmalloc(sizeof(*taskTmp), GFP_KERNEL);
    ASSERT_NE(nullptr, taskTmp);
    taskTmp->status = MB_TASK_CREATED;
    taskTmp->progress = 1;
    fileTmp->private_data = static_cast<void *>(taskTmp);

    ssize_t ret;
    char *user_buf = "user";
    size_t count = 0;
    loff_t *ppos;
    ret = smap_migrate_back_debugfs_read(fileTmp, user_buf, count, ppos);
    EXPECT_EQ(0, ret);

    kfree(fileTmp);
    kfree(taskTmp);
}

extern "C" struct dentry *debugfs_lookup(const char *name, struct dentry *parent);
TEST_F(MigrateBackDebugfsTest, CreateMigrateDebugfs)
{
    struct migrate_back_task *taskTmp = (struct migrate_back_task *)kmalloc(sizeof(*taskTmp), GFP_KERNEL);
    ASSERT_NE(nullptr, taskTmp);
    taskTmp->task_id = 1;
    create_migrate_back_debugfs(taskTmp);

    struct dentry *dentryTmp = (struct dentry *)kmalloc(sizeof(*dentryTmp), GFP_KERNEL);
    ASSERT_NE(nullptr, dentryTmp);
    MOCKER(debugfs_lookup).stubs().with(any(), any()).will(returnValue(dentryTmp));
    create_migrate_back_debugfs(taskTmp);

    kfree(dentryTmp);
    kfree(taskTmp);
}

TEST_F(MigrateBackDebugfsTest, RemoveMigrateDebugfs)
{
    struct migrate_back_task *taskTmp = (struct migrate_back_task *)kmalloc(sizeof(*taskTmp), GFP_KERNEL);
    ASSERT_NE(nullptr, taskTmp);
    taskTmp->task_id = 1;
    remove_migrate_back_debugfs(taskTmp);

    struct dentry *dentryTmp = (struct dentry *)kmalloc(sizeof(*dentryTmp), GFP_KERNEL);
    ASSERT_NE(nullptr, dentryTmp);
    MOCKER(debugfs_lookup).stubs().with(any(), any()).will(returnValue(dentryTmp));
    remove_migrate_back_debugfs(taskTmp);

    kfree(dentryTmp);
    kfree(taskTmp);
}