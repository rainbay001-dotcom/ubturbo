/*
* Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
* Description: SMAP3.0 core测试代码
*/
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include <linux/kobject.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uio.h>
#include <linux/bitops.h>
#include <linux/hrtimer.h>
#include <linux/device.h>
#include <linux/vmalloc.h>
#include "tracking_private.h"
#include "tracking_ioctl.h"
#include "bus.h"
#include "stub_core.h"

using namespace std;

#define SMAP_IOCTL_OUT _IOW('N', 15, struct iomem_seg)
struct tracking_core_ctrl {
    unsigned long node_bitmap;
    struct list_head node_cdev;
};
static struct device *ldev;
static struct tracking_node_dev *node_dev;
static struct tracking_dev *trk_dev;
extern "C" struct tracking_core_ctrl *trk_core_ctrl;
static struct tracking_operations stub_tracking_ops;
char *src;
char *new_data;
int g_length;
struct tracking_dev *trk_dev_temp;

class GlobalManager {
public:
    static GlobalManager &getInstance()
    {
        static GlobalManager instance;
        return instance;
    }
    void init_core()
    {
        node_dev = (struct tracking_node_dev*)kzalloc(sizeof(struct tracking_node_dev), GFP_KERNEL);
        ASSERT_NE(nullptr, node_dev);
        trk_dev = (struct tracking_dev*)kzalloc(sizeof(struct tracking_dev), GFP_KERNEL);
        ASSERT_NE(nullptr, trk_dev);
        ldev = (struct device*)kzalloc(sizeof(struct device), GFP_KERNEL);
        ASSERT_NE(nullptr, ldev);
        g_length = 10;
        src = (char*)kzalloc(g_length * sizeof(char), GFP_KERNEL);
        ASSERT_NE(nullptr, src);
        new_data = (char*)kzalloc(g_length * sizeof(char), GFP_KERNEL);
        ASSERT_NE(nullptr, new_data);
        ldev->numa_node = 0;
        trk_dev->target_node = 0;
        trk_dev->id = 0;
        trk_dev->dev = ldev;
        INIT_LIST_HEAD(&node_dev->dev_list);
        list_add_tail(&trk_dev->list, &node_dev->dev_list);
    }
    void exit_core()
    {
        list_del(&trk_dev->list);
        if (list_empty(&node_dev->dev_list)) {
            list_del(&node_dev->dev_list);
        }
        kfree(trk_dev);
        trk_dev = NULL;
        kfree(node_dev);
        node_dev = NULL;
        kfree(ldev);
        ldev = NULL;
        kfree(src);
        src = NULL;
        kfree(new_data);
        new_data = NULL;
    }
};

class DriversCoreTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        cout << "[Phase SetUp Begin]" << endl;
        GlobalManager::getInstance().init_core();
        cout << "[Phase SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[Phase TearDown Begin]" << endl;
        GlobalManager::getInstance().exit_core();
        GlobalMockObject::verify();
        cout << "[Phase TearDown End]" << endl;
    }
};

extern "C" int node_cdev_open(struct inode *inode, struct file *file);
extern "C" int node_cdev_release(struct inode *inode, struct file *file);
TEST_F(DriversCoreTest, NodeCdevOpen)
{
    int ret;
    struct inode *node = (struct inode*)kzalloc(sizeof(*node), GFP_KERNEL);
    ASSERT_NE(nullptr, node);
    struct file *filp = (struct file*)kzalloc(sizeof(*filp), GFP_KERNEL);
    ASSERT_NE(nullptr, filp);
    struct cdev *c_dev = (struct cdev*)kzalloc(sizeof(*c_dev), GFP_KERNEL);
    ASSERT_NE(nullptr, c_dev);
    node->i_cdev = c_dev;
    ret = node_cdev_open(node, filp);
    EXPECT_EQ(0, ret);
    ret = node_cdev_release(node, filp);
    EXPECT_EQ(0, ret);
}

extern "C" void node_tracking_enable(struct tracking_node_dev *node_dev);
TEST_F(DriversCoreTest, NodeTrackingEnable)
{
    node_tracking_enable(node_dev);
    list_for_each_entry (trk_dev_temp, &node_dev->dev_list, list) {
        EXPECT_EQ(0, trk_dev_temp->dev->numa_node);
    }
    trk_dev->ops = &stub_tracking_ops;
    node_tracking_enable(node_dev);
    list_for_each_entry (trk_dev_temp, &node_dev->dev_list, list) {
        EXPECT_EQ(0, trk_dev_temp->dev->numa_node);
    }
    stub_tracking_ops.tracking_enable = stub_tracking_enable;
    node_tracking_enable(node_dev);
    list_for_each_entry (trk_dev_temp, &node_dev->dev_list, list) {
        EXPECT_EQ(1, trk_dev_temp->dev->numa_node);
    }
}

extern "C" void node_tracking_disable(struct tracking_node_dev *node_dev);
TEST_F(DriversCoreTest, NodeTrackingDisable)
{
    node_tracking_disable(node_dev);
    list_for_each_entry (trk_dev_temp, &node_dev->dev_list, list) {
        EXPECT_EQ(0, trk_dev_temp->dev->numa_node);
    }
    trk_dev->ops = &stub_tracking_ops;
    node_tracking_disable(node_dev);
    list_for_each_entry (trk_dev_temp, &node_dev->dev_list, list) {
        EXPECT_EQ(0, trk_dev_temp->dev->numa_node);
    }
    stub_tracking_ops.tracking_disable = stub_tracking_disable;
    node_tracking_disable(node_dev);
    list_for_each_entry (trk_dev_temp, &node_dev->dev_list, list) {
        EXPECT_EQ(2, trk_dev_temp->dev->numa_node);
    }
}

extern "C" int node_tracking_set_page_size(struct tracking_node_dev *node_dev, u8 page_size);
TEST_F(DriversCoreTest, NodeTrackingSetPageSize)
{
    int ret;
    u8 page_size = 0;
    ret = node_tracking_set_page_size(node_dev, page_size);
    EXPECT_EQ(0, ret);
    list_for_each_entry (trk_dev_temp, &node_dev->dev_list, list) {
        EXPECT_EQ(0, trk_dev_temp->dev->numa_node);
    }
    trk_dev->ops = &stub_tracking_ops;
    ret = node_tracking_set_page_size(node_dev, page_size);
    EXPECT_EQ(0, ret);
    list_for_each_entry (trk_dev_temp, &node_dev->dev_list, list) {
        EXPECT_EQ(0, trk_dev_temp->dev->numa_node);
    }
    stub_tracking_ops.tracking_set_page_size = stub_tracking_set_page_size;
    ret = node_tracking_set_page_size(node_dev, page_size);
    EXPECT_EQ(-EINVAL, ret);
    list_for_each_entry (trk_dev_temp, &node_dev->dev_list, list) {
        EXPECT_EQ(6, trk_dev_temp->dev->numa_node);
    }

    MOCKER(stub_tracking_set_page_size).stubs().will(returnValue(0));
    ret = node_tracking_set_page_size(node_dev, page_size);
    EXPECT_EQ(0, ret);
    list_for_each_entry (trk_dev_temp, &node_dev->dev_list, list) {
        EXPECT_EQ(6, trk_dev_temp->dev->numa_node);
    }
}

extern "C" int node_tracking_set_trk_mode(struct tracking_node_dev *node_dev, u8 mode);
TEST_F(DriversCoreTest, NodeTrackingSetTrkMode)
{
    int ret = -1;
    int mode = 0;
    ret = node_tracking_set_trk_mode(node_dev, mode);
    EXPECT_EQ(0, ret);
    list_for_each_entry (trk_dev_temp, &node_dev->dev_list, list) {
        EXPECT_EQ(0, trk_dev_temp->dev->numa_node);
    }
    trk_dev->ops = &stub_tracking_ops;
    ret = node_tracking_set_trk_mode(node_dev, mode);
    EXPECT_EQ(0, ret);
    list_for_each_entry (trk_dev_temp, &node_dev->dev_list, list) {
        EXPECT_EQ(0, trk_dev_temp->dev->numa_node);
    }
    stub_tracking_ops.tracking_mode_set = stub_tracking_mode_set;
    ret = node_tracking_set_trk_mode(node_dev, mode);
    EXPECT_EQ(-EINVAL, ret);
    list_for_each_entry (trk_dev_temp, &node_dev->dev_list, list) {
        EXPECT_EQ(4, trk_dev_temp->dev->numa_node);
    }

    MOCKER(stub_tracking_mode_set).stubs().will(returnValue(0));
    ret = node_tracking_set_trk_mode(node_dev, mode);
    EXPECT_EQ(0, ret);
    list_for_each_entry (trk_dev_temp, &node_dev->dev_list, list) {
        EXPECT_EQ(4, trk_dev_temp->dev->numa_node);
    }
}

extern "C" long handle_tracking_cmd(struct tracking_node_dev *node_dev, unsigned long arg);
TEST_F(DriversCoreTest, HandTrackingCmd)
{
    struct tracking_node_dev *t_node_dev = NULL;
    long ret = 0;

    MOCKER(node_tracking_disable).stubs().will(ignoreReturnValue());
    ret = handle_tracking_cmd(t_node_dev, TRACKING_DISABLED);
    EXPECT_EQ(0, ret);

    MOCKER(node_tracking_enable).stubs().will(ignoreReturnValue());
    ret = handle_tracking_cmd(t_node_dev, TRACKING_ENABLED);
    EXPECT_EQ(0, ret);
}

extern "C" long handle_mode_set_cmd(struct tracking_node_dev *node_dev, unsigned long arg);
TEST_F(DriversCoreTest, HandModeSetCmdCaseOne)
{
    struct tracking_node_dev node_dev;
    unsigned long arg = 0;
    long ret = 0;

    arg = MODE_MAX;
    ret = handle_mode_set_cmd(&node_dev, arg);
    EXPECT_EQ(-EINVAL, ret);
}

extern "C" long handle_mode_set_cmd(struct tracking_node_dev *node_dev, unsigned long arg);
TEST_F(DriversCoreTest, HandModeSetCmdCaseTwo)
{
    struct tracking_node_dev node_dev;
    unsigned long arg = 0;
    long ret = 0;
    arg = ACCESS_MODE_SUM;
    MOCKER(node_tracking_set_trk_mode).stubs().will(returnValue(-EINVAL));
    ret = handle_mode_set_cmd(&node_dev, arg);
    EXPECT_EQ(-EINVAL, ret);
}

extern "C" long handle_mode_set_cmd(struct tracking_node_dev *node_dev, unsigned long arg);
TEST_F(DriversCoreTest, HandModeSetCmdCaseThree)
{
    struct tracking_node_dev node_dev;
    unsigned long arg = 0;
    long ret = 0;

    arg = ACCESS_MODE_SUM;
    MOCKER(node_tracking_set_trk_mode).stubs().will(returnValue((int)0));
    ret = handle_mode_set_cmd(&node_dev, arg);
    EXPECT_EQ(0, ret);
}

extern "C" long handle_page_size_set_cmd(struct tracking_node_dev *node_dev, unsigned long arg);
TEST_F(DriversCoreTest, HandPageSizeSetCmd)
{
    struct tracking_node_dev node_dev;
    unsigned long arg = 0;
    long ret = 0;

    arg = 0;
    MOCKER(node_tracking_set_page_size).stubs().will(returnValue(-EINVAL));
    ret = handle_page_size_set_cmd(&node_dev, arg);
    EXPECT_EQ(-EINVAL, ret);
    GlobalMockObject::verify();

    MOCKER(node_tracking_set_page_size).stubs().will(returnValue(0));
    ret = handle_page_size_set_cmd(&node_dev, arg);
    EXPECT_EQ(0, ret);
}

extern "C" long handle_reinit_node_cmd(struct tracking_node_dev *node_dev);
extern "C" int node_tracking_reinit_node(struct tracking_node_dev *node_dev);
TEST_F(DriversCoreTest, HandReinitNodeCmd)
{
    struct tracking_node_dev node_dev;
    long ret = 0;

    MOCKER(node_tracking_reinit_node).stubs().will(returnValue(-EINVAL));
    ret = handle_reinit_node_cmd(&node_dev);
    EXPECT_EQ(-EINVAL, ret);
    GlobalMockObject::verify();

    MOCKER(node_tracking_reinit_node).stubs().will(returnValue(0));
    ret = handle_reinit_node_cmd(&node_dev);
    EXPECT_EQ(0, ret);
}

TEST_F(DriversCoreTest, HandNodeTrackingReinitNode)
{
    int ret;
    trk_dev->ops = &stub_tracking_ops;
    stub_tracking_ops.tracking_reinit_node = StubTrackingReinitNode;
    ret = node_tracking_reinit_node(node_dev);
    EXPECT_EQ(0, ret);
}

extern "C" int stub_tracking_ram_change(struct device *ldev, void __user *argp);
extern "C" int node_ram_change_cmd(struct tracking_node_dev *node_dev, void __user *argp);
TEST_F(DriversCoreTest, NodeRamChangeCmd)
{
    void __user *argp = NULL;
    int ret;
    trk_dev->ops = &stub_tracking_ops;
    stub_tracking_ops.tracking_ram_change = stub_tracking_ram_change;
    MOCKER(stub_tracking_ram_change).stubs().will(returnValue(-EINVAL));
    ret = node_ram_change_cmd(node_dev, argp);
    EXPECT_EQ(-EINVAL, ret);
    GlobalMockObject::verify();

    MOCKER(stub_tracking_ram_change).stubs().will(returnValue(0));
    ret = node_ram_change_cmd(node_dev, argp);
    EXPECT_EQ(0, ret);
}

extern "C" long handle_ram_change_cmd(struct tracking_node_dev *node_dev, void __user *argp);
TEST_F(DriversCoreTest, HandleRamChangeCmd)
{
    struct tracking_node_dev node_dev;
    void __user *argp = NULL;
    long ret = 0;

    MOCKER(node_ram_change_cmd).stubs().will(returnValue(-EINVAL));
    ret = handle_ram_change_cmd(&node_dev, argp);
    EXPECT_EQ(-EINVAL, ret);
    GlobalMockObject::verify();

    MOCKER(node_ram_change_cmd).stubs().will(returnValue(0));
    ret = handle_ram_change_cmd(&node_dev, argp);
    EXPECT_EQ(0, ret);
}

extern "C" int stub_tracking_acpi_len_get(struct device *ldev, void __user *argp);
extern "C" int node_acpi_len_cmd(struct tracking_node_dev *node_dev, void __user *argp);
TEST_F(DriversCoreTest, NodeAcpiLenCmd)
{
    void __user *argp = NULL;
    int ret;
    trk_dev->ops = &stub_tracking_ops;
    stub_tracking_ops.tracking_acpi_len_get = stub_tracking_acpi_len_get;
    MOCKER(stub_tracking_acpi_len_get).stubs().will(returnValue(-EINVAL));
    ret = node_acpi_len_cmd(node_dev, argp);
    EXPECT_EQ(-EINVAL, ret);
    GlobalMockObject::verify();

    MOCKER(stub_tracking_acpi_len_get).stubs().will(returnValue(0));
    ret = node_acpi_len_cmd(node_dev, argp);
    EXPECT_EQ(0, ret);
}

extern "C" long handle_acpi_len_cmd(struct tracking_node_dev *node_dev, void __user *argp);
TEST_F(DriversCoreTest, HandleAcpiLenCmd)
{
    struct tracking_node_dev node_dev;
    void __user *argp = NULL;
    long ret = 0;

    MOCKER(node_acpi_len_cmd).stubs().will(returnValue(-EINVAL));
    ret = handle_acpi_len_cmd(&node_dev, argp);
    EXPECT_EQ(-EINVAL, ret);
    GlobalMockObject::verify();

    MOCKER(node_acpi_len_cmd).stubs().will(returnValue(0));
    ret = handle_acpi_len_cmd(&node_dev, argp);
    EXPECT_EQ(0, ret);
}

extern "C" int stub_tracking_iomem_len_get(struct device *ldev, void __user *argp);
extern "C" int node_iomem_len_cmd(struct tracking_node_dev *node_dev, void __user *argp);
TEST_F(DriversCoreTest, NodeIomemLenCmd)
{
    void __user *argp = NULL;
    int ret;
    trk_dev->ops = &stub_tracking_ops;
    stub_tracking_ops.tracking_iomem_len_get = stub_tracking_iomem_len_get;
    MOCKER(stub_tracking_iomem_len_get).stubs().will(returnValue(-EINVAL));
    ret = node_iomem_len_cmd(node_dev, argp);
    EXPECT_EQ(-EINVAL, ret);
    GlobalMockObject::verify();

    MOCKER(stub_tracking_iomem_len_get).stubs().will(returnValue(0));
    ret = node_iomem_len_cmd(node_dev, argp);
    EXPECT_EQ(0, ret);
}

extern "C" long handle_iomem_len_cmd(struct tracking_node_dev *node_dev, void __user *argp);
TEST_F(DriversCoreTest, HandleIomemLenCmd)
{
    struct tracking_node_dev node_dev;
    void __user *argp = NULL;
    long ret = 0;

    MOCKER(node_iomem_len_cmd).stubs().will(returnValue(-EINVAL));
    ret = handle_iomem_len_cmd(&node_dev, argp);
    EXPECT_EQ(-EINVAL, ret);
    GlobalMockObject::verify();

    MOCKER(node_iomem_len_cmd).stubs().will(returnValue(0));
    ret = handle_iomem_len_cmd(&node_dev, argp);
    EXPECT_EQ(0, ret);
}

extern "C" int stub_tracking_acpi_mem_get(struct device *ldev, void __user *argp);
extern "C" int node_acpi_mem_cmd(struct tracking_node_dev *node_dev, void __user *argp);
TEST_F(DriversCoreTest, NodeAcpiMemCmd)
{
    void __user *argp = NULL;
    int ret;
    trk_dev->ops = &stub_tracking_ops;
    stub_tracking_ops.tracking_acpi_mem_get = stub_tracking_acpi_mem_get;
    MOCKER(stub_tracking_acpi_mem_get).stubs().will(returnValue(-EINVAL));
    ret = node_acpi_mem_cmd(node_dev, argp);
    EXPECT_EQ(-EINVAL, ret);
    GlobalMockObject::verify();

    MOCKER(stub_tracking_acpi_mem_get).stubs().will(returnValue(0));
    ret = node_acpi_mem_cmd(node_dev, argp);
    EXPECT_EQ(0, ret);
}

extern "C" long handle_acpi_mem_cmd(struct tracking_node_dev *node_dev, void __user *argp);
TEST_F(DriversCoreTest, HandleAcpiMemCmd)
{
    struct tracking_node_dev node_dev;
    void __user *argp = NULL;
    long ret = 0;

    MOCKER(node_acpi_mem_cmd).stubs().will(returnValue(-EINVAL));
    ret = handle_acpi_mem_cmd(&node_dev, argp);
    EXPECT_EQ(-EINVAL, ret);
    GlobalMockObject::verify();

    MOCKER(node_acpi_mem_cmd).stubs().will(returnValue(0));
    ret = handle_acpi_mem_cmd(&node_dev, argp);
    EXPECT_EQ(0, ret);
}

extern "C" int stub_tracking_iomem_get(struct device *ldev, void __user *argp);
extern "C" int node_iomem_cmd(struct tracking_node_dev *node_dev, void __user *argp);
TEST_F(DriversCoreTest, NodeIomemCmd)
{
    void __user *argp = NULL;
    int ret;
    trk_dev->ops = &stub_tracking_ops;
    stub_tracking_ops.tracking_iomem_get = stub_tracking_iomem_get;
    MOCKER(stub_tracking_iomem_get).stubs().will(returnValue(-EINVAL));
    ret = node_iomem_cmd(node_dev, argp);
    EXPECT_EQ(-EINVAL, ret);
    GlobalMockObject::verify();

    MOCKER(stub_tracking_iomem_get).stubs().will(returnValue(0));
    ret = node_iomem_cmd(node_dev, argp);
    EXPECT_EQ(0, ret);
}

extern "C" long handle_iomem_cmd(struct tracking_node_dev *node_dev, void __user *argp);
TEST_F(DriversCoreTest, HandleIomemCmd)
{
    struct tracking_node_dev node_dev;
    void __user *argp = NULL;
    long ret = 0;

    MOCKER(node_iomem_cmd).stubs().will(returnValue(-EINVAL));
    ret = handle_iomem_cmd(&node_dev, argp);
    EXPECT_EQ(-EINVAL, ret);
    GlobalMockObject::verify();

    MOCKER(node_iomem_cmd).stubs().will(returnValue(0));
    ret = handle_iomem_cmd(&node_dev, argp);
    EXPECT_EQ(0, ret);
}

extern "C" long node_cdev_user_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
TEST_F(DriversCoreTest, NodeCdevUserIoctlTra)
{
    struct file file;
    unsigned int cmd = 0;
    unsigned long arg = 0;
    int ret;

    cmd = SMAP_IOCTL_TRACKING_CMD;
    MOCKER(handle_tracking_cmd).stubs().will(returnValue(0));
    ret = node_cdev_user_ioctl(&file, cmd, arg);
    EXPECT_EQ(0, ret);
}

TEST_F(DriversCoreTest, NodeCdevUserIoctlMod)
{
    struct file file;
    unsigned int cmd = 0;
    unsigned long arg = 0;
    int ret;

    cmd = SMAP_IOCTL_MODE_SET_CMD;
    MOCKER(handle_mode_set_cmd).stubs().will(returnValue(0));
    ret = node_cdev_user_ioctl(&file, cmd, arg);
    EXPECT_EQ(0, ret);
}

TEST_F(DriversCoreTest, NodeCdevUserIoctlPgSize)
{
    struct file file;
    unsigned int cmd = 0;
    unsigned long arg = 0;
    int ret;

    cmd = SMAP_IOCTL_PAGE_SIZE_SET_CMD;
    MOCKER(handle_page_size_set_cmd).stubs().will(returnValue(0));
    ret = node_cdev_user_ioctl(&file, cmd, arg);
    EXPECT_EQ(0, ret);
}

TEST_F(DriversCoreTest, NodeCdevUserIoctlReInit)
{
    struct file file;
    unsigned int cmd = 0;
    unsigned long arg = 0;
    int ret;

    cmd = SMAP_IOCTL_REINIT_NODE_CMD;
    MOCKER(handle_reinit_node_cmd).stubs().will(returnValue(0));
    ret = node_cdev_user_ioctl(&file, cmd, arg);
    EXPECT_EQ(0, ret);
}

TEST_F(DriversCoreTest, NodeCdevUserIoctlRamChange)
{
    struct file file;
    unsigned int cmd = 0;
    unsigned long arg = 0;
    int ret;

    cmd = SMAP_IOCTL_RAM_CHANGE;
    MOCKER(handle_ram_change_cmd).stubs().will(returnValue(0));
    ret = node_cdev_user_ioctl(&file, cmd, arg);
    EXPECT_EQ(0, ret);
}

TEST_F(DriversCoreTest, NodeCdevUserIoctlAcpiLen)
{
    struct file file;
    unsigned int cmd = 0;
    unsigned long arg = 0;
    int ret;

    cmd = SMAP_IOCTL_ACPI_LEN;
    MOCKER(handle_acpi_len_cmd).stubs().will(returnValue(0));
    ret = node_cdev_user_ioctl(&file, cmd, arg);
    EXPECT_EQ(0, ret);
}

TEST_F(DriversCoreTest, NodeCdevUserIoctlIomemLen)
{
    struct file file;
    unsigned int cmd = 0;
    unsigned long arg = 0;
    int ret;

    cmd = SMAP_IOCTL_IOMEM_LEN;
    MOCKER(handle_iomem_len_cmd).stubs().will(returnValue(0));
    ret = node_cdev_user_ioctl(&file, cmd, arg);
    EXPECT_EQ(0, ret);
}

TEST_F(DriversCoreTest, NodeCdevUserIoctlAcpiAddr)
{
    struct file file;
    unsigned int cmd = 0;
    unsigned long arg = 0;
    int ret;

    cmd = SMAP_IOCTL_ACPI_ADDR;
    MOCKER(handle_acpi_mem_cmd).stubs().will(returnValue(0));
    ret = node_cdev_user_ioctl(&file, cmd, arg);
    EXPECT_EQ(0, ret);
}

TEST_F(DriversCoreTest, NodeCdevUserIoctlIomemAddr)
{
    struct file file;
    unsigned int cmd = 0;
    unsigned long arg = 0;
    int ret;

    cmd = SMAP_IOCTL_IOMEM_ADDR;
    MOCKER(handle_iomem_cmd).stubs().will(returnValue(0));
    ret = node_cdev_user_ioctl(&file, cmd, arg);
    EXPECT_EQ(0, ret);
}

TEST_F(DriversCoreTest, NodeCdevUserIoctlOut)
{
    struct file file;
    unsigned int cmd = 0;
    unsigned long arg = 0;
    int ret;

    cmd = SMAP_IOCTL_OUT;
    ret = node_cdev_user_ioctl(&file, cmd, arg);
    EXPECT_EQ(-EINVAL, ret);
}

extern "C" int node_trk_data_read(struct tracking_node_dev *node_dev, void *buffer, u32 length);
TEST_F(DriversCoreTest, NodeTrkDataRead)
{
    u32 *buffer = NULL;
    int ret;
    u32 length;
    struct file *filp = (struct file *)kzalloc(sizeof(*filp), GFP_KERNEL);
    ASSERT_NE(nullptr, filp);

    trk_dev->ops = &stub_tracking_ops;
    node_dev->file = filp;
    stub_tracking_ops.tracking_read = stub_tracking_read;
    MOCKER(stub_tracking_read).stubs().will(returnValue(-EINVAL));
    ret = node_trk_data_read(node_dev, buffer, length);
    EXPECT_EQ(-EINVAL, ret);
    GlobalMockObject::verify();

    MOCKER(stub_tracking_read).stubs().will(returnValue(0));
    ret = node_trk_data_read(node_dev, buffer, length);
    EXPECT_EQ(-EIO, ret);
}

extern "C" ssize_t node_cdev_read_iter(struct kiocb *iocb, struct iov_iter *iov);
TEST_F(DriversCoreTest, NodeCdevReadIter)
{
    struct file *filp = (struct file *)kzalloc(sizeof(*filp), GFP_KERNEL);
    ASSERT_NE(nullptr, filp);
    struct kiocb iocb = {0};
    iocb.ki_filp = filp;
    struct iov_iter iov = {
        .nr_segs = 1,
    };
    char __user *buf;
    struct kvec kvec1 = {
        .iov_base = (void*)buf,
        .iov_len = 10,
    };
    iov.kvec = &kvec1;

    size_t ret;
    MOCKER(node_trk_data_read).stubs().will(returnValue(0));
    ret = node_cdev_read_iter(&iocb, &iov);
    EXPECT_EQ(0, ret);
    kfree(filp);
}

extern "C" int tracking_device_probe(struct tracking_dev *dev);
extern "C" void tracking_device_remove(struct tracking_dev *dev);
extern "C" int __init tracking_core_init(void);
extern "C" bool IS_ERR(const void *ptr);
extern "C" struct tracking_driver tracking_device_driver;
extern "C" int match_always_count;
TEST_F(DriversCoreTest, TrackingCoreInit)
{
    match_always_count = 0;
    int ret;
    MOCKER(IS_ERR).stubs().will(returnValue(false));
    MOCKER(tracking_bus_init).stubs().will(returnValue(0));
    tracking_device_driver.match_always = 0;
    ret = tracking_core_init();
    EXPECT_EQ(0, ret);
    tracking_device_driver.match_always = 1;
}

TEST_F(DriversCoreTest, TrackingCoreInitError)
{
    // alloc failed case
    match_always_count = 0;
    int ret;
    MOCKER(IS_ERR).stubs().will(returnValue(false));
    MOCKER(tracking_bus_init).stubs().will(returnValue(0));
    tracking_device_driver.match_always = 0;
    MOCKER(kzalloc).stubs().will(returnValue((void*)NULL));
    ret = tracking_core_init();
    EXPECT_EQ(-ENOMEM, ret);
    tracking_device_driver.match_always = 1;

    // re-regist driver case
    GlobalMockObject::verify();
    MOCKER(IS_ERR).stubs().will(returnValue(false));
    MOCKER(tracking_bus_init).stubs().will(returnValue(0));
    ret = tracking_core_init();
    EXPECT_EQ(0, ret);
    ret = tracking_core_init();
    EXPECT_EQ(-EINVAL, ret);
    match_always_count = 0;
}

extern "C" void __exit tracking_core_exit(void);
TEST_F(DriversCoreTest, TrackingCoreExit)
{
    tracking_core_exit();
}

extern "C" void tracking_device_remove(struct tracking_dev *dev);
TEST_F(DriversCoreTest, tracking_device_probe)
{
    int ret;
    trk_core_ctrl = (struct tracking_core_ctrl *)kzalloc(sizeof(*trk_core_ctrl), GFP_KERNEL);
    INIT_LIST_HEAD(&trk_core_ctrl->node_cdev);
    struct tracking_dev dev = {
        .target_node = 1,
    };
    ret = tracking_device_probe(&dev);
    EXPECT_EQ(0, ret);
    tracking_device_remove(&dev);
    kfree(trk_core_ctrl);
    trk_core_ctrl = nullptr;
}
