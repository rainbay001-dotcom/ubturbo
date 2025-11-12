/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: smap5.0 pid_ioctl module ut code
 */

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include <linux/list.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/device/class.h>

#include "iomem.h"
#include "migrate_task.h"
#include "pid_ioctl.h"

using namespace std;


class PidIoctlTest : public ::testing::Test {
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

extern "C" int dup_task(unsigned long long task_id, struct list_head *head);
TEST_F(PidIoctlTest, DupTask)
{
    int ret;
    LIST_HEAD(head);
    struct migrate_back_task taskArr[] = {
        { .task_id = 1, .status = MB_TASK_CREATED, },
        { .task_id = 2, .status = MB_TASK_WAITING, },
        { .task_id = 3, .status = MB_TASK_DONE, },
        { .task_id = 4, .status = MB_TASK_ERR, },
    };
    list_add_tail(&taskArr[0].task_node, &head);
    list_add_tail(&taskArr[1].task_node, &head);
    list_add_tail(&taskArr[2].task_node, &head);
    list_add_tail(&taskArr[3].task_node, &head);

    ret = dup_task(0, &head);
    EXPECT_EQ(NORMAL_ID, ret);

    ret = dup_task(1, &head);
    EXPECT_EQ(DUP_ID, ret);

    ret = dup_task(2, &head);
    EXPECT_EQ(DUP_ID, ret);

    ret = dup_task(3, &head);
    EXPECT_EQ(DUP_ID, ret);

    ret = dup_task(4, &head);
    EXPECT_EQ(RETRY_ID, ret);
    EXPECT_EQ(MB_TASK_WAITING, taskArr[3].status);
}

extern "C" int smap_ioctl_migrate_back(struct migrate_back_msg *msg);
extern "C" int init_migrate_back_subtask(struct migrate_back_task *task,
    struct migrate_back_inner_payload *p,
    struct migrate_back_subtask **result);
extern "C" int start_migrate_back_work(void);
TEST_F(PidIoctlTest, SmapIoctlMigrateBack)
{
    int ret;
    struct migrate_back_msg mb_msg = { 1, 0, { { 2, 0 } } };
    struct migrate_back_task task = {
        .subtask = LIST_HEAD_INIT(task.subtask)
    };
    struct migrate_back_subtask *subtask;

    ret = smap_ioctl_migrate_back(&mb_msg);
    EXPECT_EQ(0, ret);

    mb_msg.count = 1;
    MOCKER(init_migrate_back_task).stubs().will(returnValue((struct migrate_back_task*)NULL));
    ret = smap_ioctl_migrate_back(&mb_msg);
    EXPECT_EQ(-ENOMEM, ret);

    GlobalMockObject::verify();
    MOCKER(init_migrate_back_task).stubs().will(returnValue(&task));
    MOCKER(dup_task).stubs().will(returnValue((int)DUP_ID));
    MOCKER(kfree).stubs().will(ignoreReturnValue());
    ret = smap_ioctl_migrate_back(&mb_msg);
    EXPECT_EQ(-EINVAL, ret);

    GlobalMockObject::verify();
    MOCKER(init_migrate_back_task).stubs().will(returnValue(&task));
    MOCKER(dup_task).stubs().will(returnValue((int)RETRY_ID));
    MOCKER(kfree).stubs().will(ignoreReturnValue());
    ret = smap_ioctl_migrate_back(&mb_msg);
    EXPECT_EQ(0, ret);

    GlobalMockObject::verify();
    MOCKER(init_migrate_back_task).stubs().will(returnValue(&task));
    MOCKER(dup_task).stubs().will(returnValue((int)NORMAL_ID));
    ASSERT_TRUE(list_empty(&migrate_back_task_list));
    MOCKER(init_migrate_back_subtask).stubs().will(returnValue(-EPERM));
    MOCKER(kfree).stubs().will(ignoreReturnValue());
    ret = smap_ioctl_migrate_back(&mb_msg);
    EXPECT_TRUE(list_empty(&migrate_back_task_list));
    EXPECT_EQ(-EPERM, ret);

    GlobalMockObject::verify();
    MOCKER(init_migrate_back_task).stubs().will(returnValue(&task));
    MOCKER(dup_task).stubs().will(returnValue((int)NORMAL_ID));
    ASSERT_TRUE(list_empty(&migrate_back_task_list));
    subtask = (struct migrate_back_subtask *)kmalloc(sizeof(*subtask), GFP_KERNEL);
    ASSERT_NE(nullptr, subtask);
    MOCKER(init_migrate_back_subtask)
        .stubs()
        .with(any(), any(), outBoundP(&subtask, sizeof(subtask)))
        .will(returnValue(0));
    MOCKER(start_migrate_back_work).stubs().will(returnValue(-ENOENT));
    ret = smap_ioctl_migrate_back(&mb_msg);
    EXPECT_FALSE(list_empty(&migrate_back_task_list));
    EXPECT_FALSE(list_empty(&task.subtask));
    EXPECT_EQ(1, task.subtask_cnt);
    EXPECT_EQ(-ENOENT, ret);

    // post-test
    list_del(&subtask->task_list);
    ASSERT_TRUE(list_empty(&task.subtask));
    kfree(subtask);
    list_del(&task.task_node);
    ASSERT_TRUE(list_empty(&migrate_back_task_list));
}

extern "C" int smap_open(struct inode *inode, struct file *file);
TEST_F(PidIoctlTest, SmapOpen)
{
    int ret = smap_open(NULL, NULL);
    EXPECT_EQ(0, ret);
}

extern "C" int smap_release(struct inode *inode, struct file *file);
TEST_F(PidIoctlTest, SmapRelease)
{
    int ret = smap_release(NULL, NULL);
    EXPECT_EQ(0, ret);
}

extern "C" int __ioctl_migrate_back(void __user *argp);

extern "C" int smap_ioctl_migrate_back(struct migrate_back_msg *msg);
extern "C" int smap_is_remote_addr_valid(int nid, u64 pa_start, u64 pa_end);
TEST_F(PidIoctlTest, IoctlMigrateBackCopyFromUserError)
{
    int ret;

    MOCKER(copy_from_user).stubs().will(returnValue((unsigned long)1));
    ret = __ioctl_migrate_back(NULL);
    EXPECT_EQ(-EFAULT, ret);
}

TEST_F(PidIoctlTest, IoctlMigrateBackParamInvalid)
{
    int ret;
    struct migrate_back_msg mb_msg = { 1, 1, { SMAP_MAX_NUMNODES, 0, 1 } };

    MOCKER(copy_from_user)
        .stubs()
        .with(outBoundP((void*)&mb_msg, sizeof(mb_msg)))
        .will(returnValue((unsigned long)0));
    ret = __ioctl_migrate_back(NULL);
    EXPECT_EQ(-EINVAL, ret);

    mb_msg.payload[0].src_nid = 0;
    mb_msg.payload[0].dest_nid = SMAP_MAX_NUMNODES;
    ret = __ioctl_migrate_back(NULL);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(PidIoctlTest, IoctlMigrateBackFindRangeError)
{
    int ret;
    struct migrate_back_msg mb_msg = { 1, 1, { SMAP_MAX_NUMNODES - 1, 0, 1 } };

    MOCKER(copy_from_user)
        .stubs()
        .with(outBoundP((void*)&mb_msg, sizeof(mb_msg)))
        .will(returnValue((unsigned long)0));
    MOCKER(find_range_by_memid).stubs().will(returnValue(-EINVAL));
    ret = __ioctl_migrate_back(NULL);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(PidIoctlTest, IoctlMigrateBackAddrInvalid)
{
    int ret;
    struct migrate_back_msg mb_msg = { 1, 1, { SMAP_MAX_NUMNODES - 1, 0, 1 } };

    MOCKER(copy_from_user)
        .stubs()
        .with(outBoundP((void*)&mb_msg, sizeof(mb_msg)))
        .will(returnValue((unsigned long)0));
    MOCKER(find_range_by_memid).stubs().will(returnValue(0));
    MOCKER(smap_is_remote_addr_valid).stubs().will(returnValue(-EINVAL));
    ret = __ioctl_migrate_back(NULL);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(PidIoctlTest, IoctlMigrateBackIoctlError)
{
    int ret;
    struct migrate_back_msg mb_msg = { 1, 1, { SMAP_MAX_NUMNODES - 1, 0, 1 } };

    MOCKER(copy_from_user)
        .stubs()
        .with(outBoundP((void*)&mb_msg, sizeof(mb_msg)))
        .will(returnValue((unsigned long)0));
    MOCKER(find_range_by_memid).stubs().will(returnValue(0));
    MOCKER(smap_is_remote_addr_valid).stubs().will(returnValue(0));
    MOCKER(smap_ioctl_migrate_back).stubs().will(returnValue(-EINVAL));
    ret = __ioctl_migrate_back(NULL);
    EXPECT_EQ(-EINVAL, ret);
}

#undef INVALID_MAGIC
#define INVALID_MAGIC 0xA2
#undef SMAP_INVALID_IOCTL
#define SMAP_INVALID_IOCTL _IOW(INVALID_MAGIC, 0, int)
#define SMAP_ENABLE_NODE _IOW(SMAP_MAGIC, 1, int)
extern "C" long smap_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
TEST_F(PidIoctlTest, SmapIoctl)
{
    unsigned int cmd;
    unsigned long arg = 0;
    long ret;

    cmd = SMAP_INVALID_IOCTL;
    ret = smap_ioctl(NULL, cmd, arg);
    EXPECT_EQ(-EINVAL, ret);

    cmd = SMAP_MIGRATE_BACK;
    MOCKER(__ioctl_migrate_back).stubs().will(returnValue(-ENOENT));
    ret = smap_ioctl(NULL, cmd, arg);
    EXPECT_EQ(-ENOENT, ret);

    cmd = SMAP_ENABLE_NODE;
    ret = smap_ioctl(NULL, cmd, arg);
    EXPECT_EQ(-ENOTTY, ret);
}

extern "C" long PTR_ERR(const void *ptr);
extern "C" int smap_release(struct inode *inode, struct file *file);
extern "C" bool IS_ERR(const void *ptr);
#undef THIS_MODULES
#define THIS_MODULE "module"
TEST_F(PidIoctlTest, SmapDevInit)
{
    int ret;
    long tmp_long;

    MOCKER(alloc_chrdev_region).stubs().will(returnValue(-EPERM));
    ret = smap_dev_init();
    EXPECT_EQ(-EPERM, ret);

    GlobalMockObject::verify();
    MOCKER(alloc_chrdev_region).stubs().will(returnValue(0));
    MOCKER(cdev_init).stubs().will(ignoreReturnValue());
    MOCKER(cdev_add).stubs().will(returnValue(-ENOENT));
    MOCKER(unregister_chrdev_region).expects(once());
    ret = smap_dev_init();
    EXPECT_EQ(-ENOENT, ret);

    GlobalMockObject::verify();
    MOCKER(alloc_chrdev_region).stubs().will(returnValue(0));
    MOCKER(cdev_init).stubs().will(ignoreReturnValue());
    MOCKER(cdev_add).stubs().will(returnValue(0));
    MOCKER(IS_ERR).stubs().will(returnValue(false));
    MOCKER(unregister_chrdev_region).expects(never());
    MOCKER(cdev_del).expects(never());
    MOCKER(class_destroy).expects(never());
    ret = smap_dev_init();
    EXPECT_EQ(0, ret);
}

TEST_F(PidIoctlTest, SmapDevInitOne)
{
    int ret;
    long tmp_long;

    MOCKER(alloc_chrdev_region).stubs().will(returnValue(0));
    MOCKER(cdev_init).stubs().will(ignoreReturnValue());
    MOCKER(cdev_add).stubs().will(returnValue(0));
    MOCKER(IS_ERR).stubs().will(returnValue(true));
    MOCKER(PTR_ERR).stubs().will(returnValue(-EPERM));
    MOCKER(cdev_del).expects(once());
    ret = smap_dev_init();
    EXPECT_EQ(-EPERM, ret);
}

extern "C" void free_all_migrate_back_task(void);
extern "C" void device_destroy(struct class_stub *cls, dev_t devt);
extern "C" void smap_dev_exit(void);
TEST_F(PidIoctlTest, SmapDevExit)
{
    struct class_stub *cls = (struct class_stub *)malloc(sizeof(struct class_stub));
    dev_t dev = 0;
    struct cdev smap_cdev;

    MOCKER(free_all_migrate_back_task).stubs().will(ignoreReturnValue());
    MOCKER(device_destroy).stubs().will(ignoreReturnValue());
    MOCKER(class_destroy).stubs().will(ignoreReturnValue());
    MOCKER(cdev_del).stubs().will(ignoreReturnValue());
    MOCKER(unregister_chrdev_region).stubs().will(ignoreReturnValue());

    smap_dev_exit();
    free(cls);
}