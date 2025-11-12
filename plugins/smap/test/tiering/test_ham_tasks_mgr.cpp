// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

#include <linux/fs.h>
#include <linux/hugetlb.h>
#include <linux/sort.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include "ham_tasks_mgr.h"

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

using namespace std;

class HamTasksMgrTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        cout << "[Phase SetUp Begin]" << endl;
        init_task_list();
        cout << "[Phase SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[Phase TearDown Begin]" << endl;
        release_all_tasks();
        GlobalMockObject::verify();
        cout << "[Phase TearDown End]" << endl;
    }
};

TEST_F(HamTasksMgrTest, ReleaseMigrateTask)
{
    struct ham_migrate_task *mig_task = (struct ham_migrate_task *)kzalloc(sizeof(struct ham_migrate_task), GFP_KERNEL);
    ASSERT_NE(nullptr, mig_task);
    mig_task->status = HAM_TASK_MIGRATED;
    mig_task->is_finish = true;
    mig_task->pid = 1;

    struct ham_page_map *hpms = (struct ham_page_map *) calloc(3, sizeof(struct ham_page_map));
    hpms[0].is_migrate = true;
    hpms[1].is_migrate = false;
    hpms[1].dst_folio = reinterpret_cast<folio *>(1);
    hpms[2].dst_folio = reinterpret_cast<folio *>(1);
    hpms[2].is_migrate = false;

    struct ram_block_map *ram_maps = (struct ram_block_map *) calloc(2, sizeof(struct ram_block_map));
    ram_maps[0].hpms = nullptr;
    ram_maps[1].hpms = hpms;
    ram_maps[1].page_num = 3;

    mig_task->ram_maps = ram_maps;
    mig_task->numa_cnt = 2;

    MOCKER(page_to_nid).stubs().will(returnValue(0)).then(returnValue(1));

    release_migrate_task(mig_task);
    EXPECT_EQ(mig_task->status, HAM_TASK_IDLE);
    EXPECT_EQ(mig_task->is_finish, false);
    EXPECT_EQ(mig_task->pid, TASK_INIT_PID);

    /* mig_task->ram_maps = NULL */
    mig_task->status = HAM_TASK_MIGRATED;
    mig_task->ram_maps = nullptr;
    release_migrate_task(mig_task);
    EXPECT_EQ(mig_task->status, HAM_TASK_IDLE);
    kfree(mig_task);
}

TEST_F(HamTasksMgrTest, GetMigrateTask)
{
    pid_t pid = 1234;
    auto mig_task = allocate_migrate_task(pid);

    /* task occupied */
    EXPECT_EQ(get_migrate_task(pid), nullptr);

    /* task not occupied */
    mig_task->status = HAM_TASK_INITED;
    auto mig_task_get = get_migrate_task(pid);
    EXPECT_EQ(mig_task_get, mig_task);
    EXPECT_EQ(mig_task_get->status, HAM_TASK_OCCUPIED | HAM_TASK_INITED);
}

TEST_F(HamTasksMgrTest, PutMigrateTask)
{
    struct ham_migrate_task *mig_task = allocate_migrate_task(1234);
    mig_task->status |= HAM_TASK_MIGRATED;

    put_migrate_task(mig_task, HAM_TASK_INITED);
    EXPECT_EQ(mig_task->status, HAM_TASK_INITED | HAM_TASK_MIGRATED);
}

TEST_F(HamTasksMgrTest, AllocateMigrateTask)
{
    pid_t pid = 1234;
    struct ham_migrate_task *mig_task = allocate_migrate_task(pid);

    EXPECT_NE(mig_task, nullptr);
    EXPECT_EQ(mig_task->status, HAM_TASK_OCCUPIED);

    /* task exists */
    EXPECT_EQ(allocate_migrate_task(pid), nullptr);
}

TEST_F(HamTasksMgrTest, ReleaseFinishedTask)
{
    struct ham_migrate_task *mig_task_0 = allocate_migrate_task(1234);
    EXPECT_NE(mig_task_0, nullptr);
    struct ham_migrate_task *mig_task_1 = allocate_migrate_task(4321);
    EXPECT_NE(mig_task_1, nullptr);

    mig_task_1->is_finish = true;
    mig_task_1->finish_times = jiffies;

    MOCKER(find_vpid).stubs().will(returnValue((pid *)nullptr)).then(returnValue((pid *)1));

    release_finished_tasks();

    EXPECT_EQ(mig_task_0->pid, TASK_INIT_PID);
    EXPECT_EQ(mig_task_0->status, HAM_TASK_IDLE);
    EXPECT_EQ(mig_task_1->pid, TASK_INIT_PID);
    EXPECT_EQ(mig_task_1->status, HAM_TASK_IDLE);
}

TEST_F(HamTasksMgrTest, ReleaseAllTasks)
{
    init_task_list();
    for (int i = 0; i < HAM_MAX_TASK; ++i) {
        (void)allocate_migrate_task(i);
    }

    release_all_tasks();

    for (int i = 0; i < HAM_MAX_TASK; ++i) {
        EXPECT_EQ(get_migrate_task(i), nullptr);
    }
}

TEST_F(HamTasksMgrTest, CheckTaskStatus)
{
    int ret;
    struct ham_migrate_task *mig_task = allocate_migrate_task(1234);

    mig_task->status |= HAM_TASK_INITED;

    /* status expected */
    ret = check_task_status(mig_task, HAM_TASK_INITED);
    EXPECT_EQ(ret, 0);

    /* status unexpected */
    ret = check_task_status(mig_task, HAM_TASK_MIGRATED);
    EXPECT_EQ(ret, -EINVAL);
}

