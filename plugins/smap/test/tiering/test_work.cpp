/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: SMAP3.0 work module test code
 */
 
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
 
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/slab.h>
#include "migrate_task.h"
#include "migrate_back_debugfs.h"
#include "work.h"
 
using namespace std;
 
class WorkTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        cout << "[Phase SetUp Begin]" << endl;
        cout << "[Phase SetUp End]" << endl;
    }
    void TearDown() override
    {
        struct migrate_back_task *t;
        struct migrate_back_task *tmp;
        cout << "[Phase TearDown Begin]" << endl;
        list_for_each_entry_safe(t, tmp, &migrate_back_task_list, task_node) {
            list_del(&t->task_node);
            kfree(t);
        }
        GlobalMockObject::verify();
        cout << "[Phase TearDown End]" << endl;
    }
};
 
extern "C" int start_migrate_back_work(void);
TEST_F(WorkTest, StartMigrateBackWork)
{
    int ret;
    int i;
    struct migrate_back_task *t1 = NULL;
    struct migrate_back_task *t2 = NULL;
    struct migrate_back_task *t = NULL;
 
    t1 = (struct migrate_back_task*)kmalloc(sizeof(*t1), GFP_KERNEL);
    t2 = (struct migrate_back_task*)kmalloc(sizeof(*t2), GFP_KERNEL);
    t = (struct migrate_back_task*)kmalloc(sizeof(*t), GFP_KERNEL);
    ASSERT_NE(nullptr, t1);
    ASSERT_NE(nullptr, t2);
    ASSERT_NE(nullptr, t);
 
    MOCKER(spin_lock).stubs().will(ignoreReturnValue());
    ASSERT_TRUE(list_empty(&migrate_back_task_list));
    // the list displays as : head -> t2 -> t1
    t1->status = MB_TASK_CREATED;
    list_add(&t1->task_node, &migrate_back_task_list);
    t2->status = MB_TASK_DONE;
    list_add(&t2->task_node, &migrate_back_task_list);
    MOCKER(spin_unlock).stubs().will(ignoreReturnValue());
    MOCKER(create_migrate_back_debugfs).stubs().will(ignoreReturnValue());
    ret = start_migrate_back_work();
    ASSERT_EQ(0, ret);
    i = 0;
    list_for_each_entry(t, &migrate_back_task_list, task_node) {
        if (i == 0) {
            EXPECT_EQ(MB_TASK_DONE, t->status);
        } else if (i == 1) {
            EXPECT_EQ(MB_TASK_WAITING, t->status);
        }
        i++;
    }
    EXPECT_EQ(2, i);
}