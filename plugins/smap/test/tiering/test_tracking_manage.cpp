/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: SMAP5.0 测试代码
 */

#include <linux/nodemask.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/version.h>

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include "tracking_manage.h"

using namespace std;

extern "C" char *qemu_name;
extern "C" int node_modes[SMAP_MAX_NUMNODES];

class TrackingManageTest : public ::testing::Test {
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

extern "C" int refresh_remote_ram(void);
extern "C" void destroy_workqueue(struct workqueue_struct *wq);
extern "C" bool cancel_delayed_work_sync(struct delayed_work *dwork);

extern "C" void release_remote_ram(void);
extern "C" void reset_acpi_mem(void);
extern "C" void exit_migrate(void);
extern "C" void smap_dev_exit(void);
extern "C" void smap_debugfs_mod_exit(void);
extern "C" void resource(void);
TEST_F(TrackingManageTest, Resource)
{
    MOCKER(cancel_delayed_work_sync).stubs().will(returnValue(true));
    MOCKER(release_remote_ram).stubs().will(ignoreReturnValue());
    MOCKER(reset_acpi_mem).stubs().will(ignoreReturnValue());
    MOCKER(destroy_workqueue).stubs().will(ignoreReturnValue());
    MOCKER(exit_migrate).stubs().will(ignoreReturnValue());
    MOCKER(smap_dev_exit).stubs().will(ignoreReturnValue());
    MOCKER(smap_debugfs_mod_exit).stubs().will(ignoreReturnValue());
    resource();
}

extern "C" int is_qemu_name_valid(void);
TEST_F(TrackingManageTest, IsQemuNameValid)
{
    int ret;

    qemu_name = "\0";
    ret = is_qemu_name_valid();
    EXPECT_EQ(-EINVAL, ret);

    qemu_name = "qemu-kvm";
    ret = is_qemu_name_valid();
    EXPECT_EQ(0, ret);
}

extern "C" int is_smap_args_valid(void);
TEST_F(TrackingManageTest, IsArgsVaild)
{
    int ret;

    smap_pgsize = 10;
    ret = is_smap_args_valid();
    EXPECT_EQ(-EINVAL, ret);

    smap_pgsize = 0;
    smap_mode = 9;
    ret = is_smap_args_valid();
    EXPECT_EQ(-EINVAL, ret);

    smap_pgsize = 0;
    smap_mode = 0;
    node_modes[0] = 100;
    ret = is_smap_args_valid();
    EXPECT_EQ(-EINVAL, ret);

    for (int i = 0; i < 10; i++) {
        node_modes[i] = i;
    }

    MOCKER(is_qemu_name_valid).stubs().will(returnValue(-EINVAL));
    ret = is_smap_args_valid();
    EXPECT_EQ(-EINVAL, ret);

    GlobalMockObject::verify();
    MOCKER(is_qemu_name_valid).stubs().will(returnValue(0));
    ret = is_smap_args_valid();
    EXPECT_EQ(0, ret);
}

extern "C" struct list_head migrate_back_task_list;
extern "C" void migrate_back_work_func(struct work_struct *work);
TEST_F(TrackingManageTest, MigrateBackWorkFunc)
{
    struct work_struct work;
    struct migrate_back_task task;
    struct migrate_back_subtask subtask;

    task.subtask_cnt = 10;
    task.status = MB_TASK_WAITING;
    subtask.status = MB_SUBTASK_WAITING;
    subtask.isolated_flag = 0;
    subtask.pa_start = 0;
    subtask.pa_end = 0x100;
    INIT_LIST_HEAD(&migrate_back_task_list);
    INIT_LIST_HEAD(&task.subtask);
    list_add(&task.task_node, &migrate_back_task_list);
    MOCKER(is_smap_pg_huge).stubs().will(returnValue(true));
    MOCKER(smap_handle_migrate_back_subtask).stubs();
    migrate_back_work_func(&work);

    list_del(&task.task_node);
}

extern "C" bool queue_delayed_work(struct workqueue_struct *wq,
    struct delayed_work *dwork,
    unsigned long delay);
extern "C" int init_acpi_mem(void);
extern "C" struct workqueue_struct *migrate_back_wq;
extern "C" int __init tracking_init(void);
TEST_F(TrackingManageTest, TestTrackingInit)
{
    int ret;

    MOCKER(is_smap_args_valid).stubs().will(returnValue(-1));
    ret = tracking_init();
    EXPECT_EQ(-1, ret);

    GlobalMockObject::verify();
    MOCKER(is_smap_args_valid).stubs().will(returnValue(0));
    MOCKER(init_acpi_mem).stubs().will(returnValue(-1));
    ret = tracking_init();
    EXPECT_EQ(-1, ret);
}

extern "C" void resource(void);
extern "C" void __exit tracking_exit(void);
TEST_F(TrackingManageTest, TestTrackingExit)
{
    MOCKER(resource).stubs().will(ignoreReturnValue());
    tracking_exit();
}