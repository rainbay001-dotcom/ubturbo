/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: smap migrate task module test code
 */

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include <linux/slab.h>

#include "migrate_task.h"

using namespace std;

class MigrateTaskTest : public ::testing::Test {
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

extern "C" struct migrate_back_task *init_migrate_back_task(unsigned long long task_id);
TEST_F(MigrateTaskTest, InitMigrateBackTask)
{
    struct migrate_back_task *ret = nullptr;
    ret = (struct migrate_back_task*)kmalloc(sizeof(struct migrate_back_task), GFP_KERNEL);
    ASSERT_NE(nullptr, ret);

    ret = init_migrate_back_task(10);
    EXPECT_EQ(10, ret->task_id);
    kfree(ret);
}

extern "C" void free_all_migrate_back_task(void);
TEST_F(MigrateTaskTest, FreeAllMigrateBackTask)
{
    free_all_migrate_back_task();
}

extern "C" int init_migrate_back_subtask(struct migrate_back_task *task,
    struct migrate_back_inner_payload *p, struct migrate_back_subtask **result);
TEST_F(MigrateTaskTest, InitMigrateBackSubtask)
{
    int ret;
    struct migrate_back_task task;
    struct migrate_back_inner_payload p;
    struct migrate_back_subtask **result = nullptr;
    result = static_cast<struct migrate_back_subtask**>(kmalloc(sizeof(*result), GFP_KERNEL));
    ASSERT_NE(nullptr, result);

    p.src_nid = 1;
    ret = init_migrate_back_subtask(&task, &p, result);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, result[0]->src_nid);

    kfree(result);
}

TEST_F(MigrateTaskTest, InitMigrateBackSubtaskTwo)
{
    int ret;
    struct migrate_back_task task;
    struct migrate_back_inner_payload p;
    struct migrate_back_subtask **result = nullptr;
    result = static_cast<struct migrate_back_subtask**>(kmalloc(sizeof(*result), GFP_KERNEL));
    ASSERT_NE(nullptr, result);

    p.src_nid = 1;
    MOCKER(kzalloc).stubs().will(returnValue((void*)NULL));
    ret = init_migrate_back_subtask(&task, &p, result);
    EXPECT_EQ(-ENOMEM, ret);

    kfree(result);
}

extern "C" void clear_migrate_back_task(void);
TEST_F(MigrateTaskTest, ClearMigrateBackTask)
{
    clear_migrate_back_task();
}
