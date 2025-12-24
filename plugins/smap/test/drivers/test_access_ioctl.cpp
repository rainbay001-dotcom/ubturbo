/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: smap access ioctl module
 */


#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/list.h>

#include "access_acpi_mem.h"
#include "access_iomem.h"
#include "check.h"
#include "access_tracking.h"
#include "access_pid.h"
#include "access_ioctl.h"

using namespace std;

class AccessIoctlTestKernel : public ::testing::Test {
public:
    static struct access_add_pid_msg m_msg;

protected:
    void SetUp() override
    {
        cout << "[Phase SetUp Begin]" << endl;
        cout << "[Phase SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[Phase TearDown Begin]" << endl;
        INIT_LIST_HEAD(&ap_data.list);
        GlobalMockObject::verify();
        cout << "[Phase TearDown End]" << endl;
    }

    long checkIoctlAddPid(struct access_add_pid_msg msg);
    static unsigned long mockCopyFromUserSetMsg(void *to, const void *from, unsigned long n);
    static unsigned long mockCopyFromUserSetPayload(void *to, const void *from, unsigned long n);
    static bool checkCopyFromUserIsMsg(const void *from);
    static bool checkCopyFromUserIsPayload(const void *from);
};

struct access_add_pid_msg AccessIoctlTestKernel::m_msg = {.count=0, .payload=NULL};

extern "C" long ioctl_add_pid(void __user *argp);
extern "C" int add_payload(int len, struct access_add_pid_payload *payload, int page_size);
extern "C" int check_msg_validity(struct access_add_pid_msg *msg);

unsigned long AccessIoctlTestKernel::mockCopyFromUserSetMsg(void *to, const void *from, unsigned long n)
{
    cout << "mockCopyFromUserSetMsg" << endl;
    struct access_add_pid_msg *tmpMsg = (struct access_add_pid_msg*) to;
    *tmpMsg = m_msg;
    return 0UL;
};

unsigned long AccessIoctlTestKernel::mockCopyFromUserSetPayload(void *to, const void *from, unsigned long n)
{
    cout << "mockCopyFromUserSetPayload" << endl;
    struct access_add_pid_payload *tmpPayload = (struct access_add_pid_payload*) to;
    if (n / sizeof(struct access_add_pid_payload) != m_msg.count) {
        return n;
    }
    for (int i = 0; i < m_msg.count; ++i) {
        tmpPayload[i] = m_msg.payload[i];
    }
    return 0UL;
};

bool AccessIoctlTestKernel::checkCopyFromUserIsMsg(const void *from)
{
    return from == &m_msg;
}

bool AccessIoctlTestKernel::checkCopyFromUserIsPayload(const void *from)
{
    return from == m_msg.payload;
}

long AccessIoctlTestKernel::checkIoctlAddPid(struct access_add_pid_msg msg)
{
    long ret = 0;
    m_msg = msg;
    MOCKER(copy_from_user)
        .stubs()
        .with(any(), checkWith(checkCopyFromUserIsMsg))
        .will(invoke(AccessIoctlTestKernel::mockCopyFromUserSetMsg));
    MOCKER(copy_from_user)
        .stubs()
        .with(any(), checkWith(checkCopyFromUserIsPayload))
        .will(invoke(AccessIoctlTestKernel::mockCopyFromUserSetPayload));
    ret = ioctl_add_pid(&m_msg);
    return ret;
}

TEST_F(AccessIoctlTestKernel, IoctlAddPidWithOnePid)
{
    int i;
    long ret;
    struct access_add_pid_msg msg;

    msg.count = 1;
    msg.payload = (struct access_add_pid_payload *)vzalloc(sizeof(struct access_add_pid_payload) * msg.count);
    ASSERT_NE(nullptr, msg.payload);
    for (i = 0; i < msg.count; i++) {
        msg.payload[i].pid = 14587;
        msg.payload[i].numa_nodes = 0x10;
        msg.payload[i].scan_time = 50;
        msg.payload[i].ntimes = 40;
        msg.payload[i].duration = 1;
        msg.payload[i].type = HAM_SCAN;
    }

    // success case
    MOCKER(add_payload).stubs().will(returnValue(0));
    ret = checkIoctlAddPid(msg);
    EXPECT_EQ(0, ret);

    // add_payload failed case
    GlobalMockObject::verify();
    MOCKER(add_payload).stubs().will(returnValue(-EINVAL));
    ret = checkIoctlAddPid(msg);
    EXPECT_EQ(-EINVAL, ret);

    // following tests will not run add_payload, since don't need add_payload mock
    // invalid scan_type case
    msg.payload[0].type = (scan_type) - 1;
    ret = checkIoctlAddPid(msg);
    EXPECT_EQ(-EINVAL, ret);

    msg.payload[0].type = MAX_SCAN_TYPE;
    ret = checkIoctlAddPid(msg);
    EXPECT_EQ(-EINVAL, ret);

    if (msg.payload) {
        vfree(msg.payload);
    }
}

TEST_F(AccessIoctlTestKernel, IoctlAddPidWithMultiPid)
{
    int i;
    long ret;
    struct access_add_pid_msg msg;

    msg.count = 15;
    msg.payload = (struct access_add_pid_payload *)vzalloc(sizeof(struct access_add_pid_payload) * msg.count);
    ASSERT_NE(nullptr, msg.payload);
    for (i = 0; i < msg.count; i++) {
        msg.payload[i].pid = 14587 + i;
        msg.payload[i].numa_nodes = 0x10;
        msg.payload[i].duration = 1;
        if (i%2) {
            msg.payload[i].scan_time = 50;
            msg.payload[i].ntimes = 40;
            msg.payload[i].type = HAM_SCAN;
        } else {
            msg.payload[i].scan_time = 10;
            msg.payload[i].ntimes = 200;
            msg.payload[i].type = STATISTIC_SCAN;
        }
    }
    // set add_payload mock
    MOCKER(add_payload).stubs()
        .will(returnValue(0))
        .then(returnValue(-EINVAL));

    // success case
    ret = checkIoctlAddPid(msg);
    EXPECT_EQ(0, ret);

    // failed case
    ret = checkIoctlAddPid(msg);
    EXPECT_EQ(-EINVAL, ret);

    // following tests will not run add_payload, since don't need add_payload mock
    // invalid scan_type case
    msg.payload[14].type = (scan_type)-1;
    ret = checkIoctlAddPid(msg);
    EXPECT_EQ(-EINVAL, ret);

    if (msg.payload)
        vfree(msg.payload);
}

TEST_F(AccessIoctlTestKernel, AddPayloadTest)
{
    int len = 1;
    struct access_add_pid_payload payload;
    MOCKER(access_add_ham_pid).stubs()
        .will(returnValue(-EINVAL))
        .then(returnValue(0));
    MOCKER(access_add_statistic_pid).stubs()
        .will(returnValue(-EINVAL))
        .then(returnValue(0));
    MOCKER(access_add_pid).stubs()
        .will(returnValue(-EINVAL))
        .then(returnValue(0));

    // test all case
    EXPECT_EQ(-EINVAL, add_payload(len, &payload, PAGE_SIZE_2M));
    EXPECT_EQ(-EINVAL, add_payload(len, &payload, PAGE_SIZE_2M));
    EXPECT_EQ(-EINVAL, add_payload(len, &payload, PAGE_SIZE_2M));
    EXPECT_EQ(0, add_payload(len, &payload, PAGE_SIZE_2M));
}

TEST_F(AccessIoctlTestKernel, CheckMsgValidityTest)
{
    struct access_add_pid_msg msg;
    struct access_add_pid_payload payload;
    msg.count = 1;
    msg.payload = &payload;
    // success case
    EXPECT_EQ(0, check_msg_validity(&msg));
    // failed case
    msg.count = 0;
    EXPECT_EQ(-EINVAL, check_msg_validity(&msg));

    msg.count = 41;
    EXPECT_EQ(-EINVAL, check_msg_validity(&msg));

    msg.payload = nullptr;
    EXPECT_EQ(-EINVAL, check_msg_validity(&msg));

    EXPECT_EQ(-EINVAL, check_msg_validity(nullptr));
}

static struct access_add_pid_msg *g_test_msg = nullptr;
static struct access_add_pid_payload *g_test_payload = nullptr;
static size_t g_test_payload_size = 0;

static unsigned long mock_copy_from_user(void *to, const void *from, unsigned long n)
{
    if (n == sizeof(struct access_add_pid_msg) && g_test_msg) {
        *(struct access_add_pid_msg *)to = *g_test_msg;
    } else if (n == g_test_payload_size && g_test_payload) {
        for (size_t i = 0; i < n / sizeof(struct access_add_pid_payload); i++) {
            ((struct access_add_pid_payload *)to)[i] = g_test_payload[i];
        }
    }
    return 0UL;
}

TEST_F(AccessIoctlTestKernel, IoctlAddPid)
{
    struct access_add_pid_msg msg;
    struct access_add_pid_payload *payload_data = nullptr;
    int i;
    int ret = 0;

    msg.count = 1;
    payload_data = (struct access_add_pid_payload *)vzalloc(sizeof(struct access_add_pid_payload) * msg.count);
    ASSERT_NE(nullptr, payload_data);
    msg.payload = payload_data;
    ASSERT_NE(nullptr, msg.payload);
    for (i = 0; i < msg.count; i++) {
        msg.payload[i].pid = i;
        msg.payload[i].numa_nodes = 0x10;
        payload_data[i].ntimes = 40;
    }

    g_test_msg = &msg;
    g_test_payload = payload_data;
    g_test_payload_size = sizeof(struct access_add_pid_payload) * msg.count;

    MOCKER(copy_from_user)
        .stubs()
        .will(invoke(mock_copy_from_user));

    MOCKER(access_add_ham_pid).stubs().will(returnValue(0));
    MOCKER(access_add_pid).stubs().will(returnValue(0));
    ret = ioctl_add_pid(NULL);
    EXPECT_EQ(0, ret);

    vfree(payload_data);

    g_test_msg = nullptr;
    g_test_payload = nullptr;
    g_test_payload_size = 0;
}

TEST_F(AccessIoctlTestKernel, IoctlAddPidTwo)
{
    struct access_add_pid_msg msg;
    int i;
    int ret = 0;

    msg.count = 1;
    msg.payload = (struct access_add_pid_payload *)vzalloc(sizeof(struct access_add_pid_payload) * msg.count);
    ASSERT_NE(nullptr, msg.payload);
    for (i = 0; i < msg.count; i++) {
        msg.payload[i].pid = i;
        msg.payload[i].numa_nodes = 0x10;
    }

    MOCKER(copy_from_user)
        .stubs()
        .with(outBoundP((void*)&msg, sizeof(msg)))
        .will(returnValue(0UL))
        .then(returnValue(1UL));

    ret = ioctl_add_pid(NULL); // or ioctl_add_pid(msg)
    EXPECT_EQ(-EFAULT, ret);
}

TEST_F(AccessIoctlTestKernel, IoctlAddPidThree)
{
    struct access_add_pid_msg msg;
    struct access_add_pid_payload *payload_data = nullptr;
    int i;
    int ret = 0;

    // 测试 1
    msg.count = 0;
    msg.payload = nullptr;
    g_test_msg = &msg;
    g_test_payload = nullptr;
    g_test_payload_size = 0;

    MOCKER(copy_from_user)
        .stubs()
        .will(invoke(mock_copy_from_user));

    ret = ioctl_add_pid(NULL);
    EXPECT_EQ(-EINVAL, ret);

    GlobalMockObject::verify();
    GlobalMockObject::reset();

    msg.count = 1;
    payload_data = (struct access_add_pid_payload *)vzalloc(sizeof(struct access_add_pid_payload) * msg.count);
    ASSERT_NE(nullptr, payload_data);
    msg.payload = payload_data;

    for (i = 0; i < msg.count; i++) {
        payload_data[i].pid = i;
        payload_data[i].numa_nodes = 0x10;
        payload_data[i].ntimes = 40;
        payload_data[i].type = HAM_SCAN;
        payload_data[i].duration = 0;
        payload_data[i].scan_time = 0;
    }

    g_test_msg = &msg;
    g_test_payload = payload_data;
    g_test_payload_size = sizeof(struct access_add_pid_payload) * msg.count;

    MOCKER(copy_from_user)
        .stubs()
        .will(invoke(mock_copy_from_user));

    MOCKER(access_add_ham_pid).stubs().will(returnValue(1));

    ret = ioctl_add_pid(NULL);
    EXPECT_EQ(1, ret);

    vfree(payload_data);

    g_test_msg = nullptr;
    g_test_payload = nullptr;
    g_test_payload_size = 0;
}

extern "C" long ioctl_remove_pid(void __user *argp);
TEST_F(AccessIoctlTestKernel, IoctlRemovePid)
{
    int ret = 0;
    struct access_remove_pid_msg msg;

    MOCKER(copy_from_user).stubs().will(returnValue(1UL));
    ret = ioctl_remove_pid(NULL);
    EXPECT_EQ(-EFAULT, ret);

    GlobalMockObject::verify();
    msg.count = 0;
    MOCKER(copy_from_user)
        .stubs()
        .with(outBoundP((void*)&msg, sizeof(msg)))
        .will(returnValue(0UL));
    ret = ioctl_remove_pid(NULL);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(AccessIoctlTestKernel, IoctlRemovePidTwo)
{
    struct access_remove_pid_msg msg;
    msg.count = 1;
    msg.payload[0].pid = 1;
    MOCKER(copy_from_user)
        .stubs()
        .with(outBoundP((void*)&msg, sizeof(msg)))
        .will(returnValue(0UL));
    MOCKER(access_remove_pid).stubs();
    MOCKER(access_remove_ham_pid).stubs();
    int ret = ioctl_remove_pid(NULL);
    EXPECT_EQ(0, ret);
}

extern "C" long ioctl_remove_all_pid(void __user *argp);
TEST_F(AccessIoctlTestKernel, IoctlRemoveAllPid)
{
    int ret = 0;

    MOCKER(access_remove_all_pid).stubs();
    ret = ioctl_remove_all_pid(NULL);
    EXPECT_EQ(0, ret);
}

extern "C" long ioctl_walk_pagemap(void __user *argp);
extern "C" size_t calc_bitmap_len(void);
TEST_F(AccessIoctlTestKernel, IoctlWalkPagemap)
{
    int ret = 0;
    ap_data.state_flag = 1;
    MOCKER(calc_bitmap_len).stubs().will(returnValue(0UL));
    ret = ioctl_walk_pagemap(NULL);
    EXPECT_EQ(-EFAULT, ret);
    EXPECT_EQ(AP_STATE_WALK, ap_data.state_flag);

    GlobalMockObject::verify();
    MOCKER(calc_bitmap_len).stubs().will(returnValue(360UL));
    MOCKER(copy_to_user).stubs().will(returnValue(0UL));
    MOCKER(hist_actc_data_reinit).stubs().will(returnValue(0));
    ret = ioctl_walk_pagemap(NULL);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(AP_STATE_WALK | AP_STATE_READ, ap_data.state_flag);
}

extern "C" void update_tracking_data(u16 *tracking_data, struct statistics_tracking_info *stat_info,
    struct tracking_info_payload *payload_info);
extern "C" long ioctl_get_tracking(void __user *argp);
TEST_F(AccessIoctlTestKernel, IoctlGetTrackingError)
{
    int ret = 0;
    // copy error case
    struct tracking_info_payload msg = {
        .pid = 100,
        .length = 10,
        .data = NULL,
    };
    MOCKER(copy_from_user).stubs().will(returnValue(1UL));
    ret = ioctl_get_tracking(NULL);
    EXPECT_EQ(-EFAULT, ret);

    // invalid length case
    GlobalMockObject::verify();
    msg.length = 65536;
    MOCKER(copy_from_user)
        .stubs()
        .with(outBoundP((void*)&msg, sizeof(msg)))
        .will(returnValue(0UL));
    ret = ioctl_get_tracking(NULL);
    EXPECT_EQ(-EINVAL, ret);

    // null data case
    GlobalMockObject::verify();
    msg.length = 1;
    MOCKER(copy_from_user)
        .stubs()
        .with(outBoundP((void*)&msg, sizeof(msg)))
        .will(returnValue(0UL));
    ret = ioctl_get_tracking(NULL);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(AccessIoctlTestKernel, IoctlGetTracking)
{
    int ret = 0;
    struct tracking_info_payload msg;
    u16 *tracking_data = NULL;
    struct statistics_tracking_info *tmp = NULL;

    msg.pid = 1;
    msg.length = 5;
    msg.data = (u16 *)malloc(sizeof(u16) * msg.length);
    ASSERT_NE(nullptr, msg.data);
    tracking_data = (u16 *)vzalloc(sizeof(u16) * msg.length);
    ASSERT_NE(nullptr, tracking_data);

    // 模拟统计PID列表中的数据
    tmp = (struct statistics_tracking_info *)kzalloc(sizeof(struct statistics_tracking_info), GFP_KERNEL);
    tmp->pid = 1;
    tmp->page_num[L1] = 3;
    tmp->page_num[L2] = 2;
    tmp->window_num = 2;
    tmp->sliding_windows = (u8 **)malloc(sizeof(u8 *) * tmp->window_num);
    for (int i = 0; i < tmp->window_num; i++) {
        tmp->sliding_windows[i] = (u8 *)malloc(sizeof(u8) * msg.length);
        for (int j = 0; j < msg.length; j++) {
            tmp->sliding_windows[i][j] = j + 1; // [1, 2, 3 ... msg.length+1]
        }
    }
    list_add(&tmp->node, &statistic_pid_list);
    MOCKER(copy_from_user)
        .stubs()
        .with(outBoundP((void*)&msg, sizeof(msg)))
        .will(returnValue(0UL));
    MOCKER(copy_to_user).stubs().will(returnValue(0UL));

    ret = ioctl_get_tracking(NULL);
    EXPECT_EQ(0, ret);

    // 验证msg.length是否正确
    EXPECT_EQ(5, msg.length);

    // 验证update_tracking_data
    update_tracking_data(tracking_data, tmp, &msg);
    for (int i = 0; i < msg.length; ++i) {
        u16 expect = tmp->window_num * (i + 1); // sliding_windows数值叠加
        EXPECT_EQ(expect, tracking_data[i]);
    }

    // 清理模拟数据
    list_del(&tmp->node);
    for (int i = 0; i < tmp->window_num; i++) {
        free(tmp->sliding_windows[i]);
    }
    free(tmp->sliding_windows);
    free(tmp);
    free(msg.data);
    free(tracking_data);
}

extern "C" size_t read_bitmap(char __user *buf, size_t cnt, loff_t *loff);
TEST_F(AccessIoctlTestKernel, ReadBitmap)
{
    int ret = 0;
    ret = read_bitmap(NULL, 0, NULL);
    EXPECT_EQ(0, ret);
}

extern "C" bool drivers_ram_changed(void);
TEST_F(AccessIoctlTestKernel, ReadBitmapTwo)
{
    int ret = 0;
    struct access_pid *ap1 = (struct access_pid *)malloc(sizeof(struct access_pid));
    INIT_LIST_HEAD(&ap_data.list);
    list_add_tail(&ap1->node, &ap_data.list);
    ap1->pid = 1;
    ap1->numa_nodes = 0x10;
    ap1->page_num[0] = 5;
    ap1->page_num[1] = 1;
    ap1->bm_len[0] = 10;
    ap1->bm_len[1] = 1;
    ap1->type = NORMAL_SCAN;
    for (int i = 0; i < NR_LEVEL; i++) {
        int len = ap1->bm_len[i];
        ap1->paddr_bm[i] = (unsigned long *)malloc(sizeof(unsigned long) * len);
        ASSERT_NE(nullptr, ap1->paddr_bm[i]);
        for (int j = 0; j < ap1->bm_len[i]; j++) {
            ap1->paddr_bm[i][j] = j;
        }
    }

    MOCKER(clean_last_ap_data).stubs().will(ignoreReturnValue());
    MOCKER(drivers_ram_changed).stubs().will(returnValue(false));
    MOCKER(copy_to_user).stubs().will(returnValue(0UL));
    ret = read_bitmap(NULL, 536, NULL);
    EXPECT_EQ(536, ret);

    GlobalMockObject::verify();
    MOCKER(clean_last_ap_data).stubs().will(ignoreReturnValue());
    MOCKER(drivers_ram_changed).stubs().will(returnValue(false));
    MOCKER(copy_to_user).stubs().will(returnValue(1UL));
    ret = read_bitmap(NULL, 0, NULL);
    EXPECT_EQ(0, ret);
    free(ap1->paddr_bm[0]);
    free(ap1->paddr_bm[1]);
    free(ap1);
}

extern "C" int smap_access_open(struct inode *inode, struct file *file);
TEST_F(AccessIoctlTestKernel, SmapAccessOpen)
{
    int ret = 0;

    ret = smap_access_open(NULL, NULL);
    EXPECT_EQ(0, ret);
}

extern "C" int smap_access_release(struct inode *inode, struct file *file);
TEST_F(AccessIoctlTestKernel, SmapAccessRelease)
{
    int ret = 0;

    ret = smap_access_release(NULL, NULL);
    EXPECT_EQ(0, ret);
}

extern "C" long smap_access_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
TEST_F(AccessIoctlTestKernel, SmapAccessIoctl)
{
    long ret = smap_access_ioctl(NULL, 0, NULL);
    EXPECT_EQ(-22, ret);

    MOCKER(ioctl_add_pid).stubs().will(returnValue(1));
    ret = smap_access_ioctl(NULL, SMAP_ACCESS_ADD_PID, NULL);
    EXPECT_EQ(1, ret);
}

TEST_F(AccessIoctlTestKernel, SmapAccessIoctlTwo)
{
    long ret;

    MOCKER(ioctl_remove_pid).stubs().will(returnValue(2));
    ret = smap_access_ioctl(NULL, SMAP_ACCESS_REMOVE_PID, NULL);
    EXPECT_EQ(2, ret);

    MOCKER(ioctl_remove_all_pid).stubs().will(returnValue(3));
    ret = smap_access_ioctl(NULL, SMAP_ACCESS_REMOVE_ALL_PID, NULL);
    EXPECT_EQ(3, ret);
}

TEST_F(AccessIoctlTestKernel, SmapAccessIoctlThree)
{
    long ret;

    MOCKER(ioctl_walk_pagemap).stubs().will(returnValue(4));
    ret = smap_access_ioctl(NULL, 0x4008BB04, NULL);
    EXPECT_EQ(4, ret);

    ret = smap_access_ioctl(NULL, 0x4324BB05, NULL);
    EXPECT_EQ(-ENOTTY, ret);
}

extern "C" ssize_t smap_access_read(struct file *file, char __user *buf, size_t cnt, loff_t *loff);
TEST_F(AccessIoctlTestKernel, SmapAccessRead)
{
    ssize_t ret;
    MOCKER(read_bitmap).stubs().will(returnValue((unsigned long)0));
    ret = smap_access_read(NULL, NULL, 0, NULL);
    EXPECT_EQ(0, ret);
}

extern "C" void access_dev_exit(void);
TEST_F(AccessIoctlTestKernel, AccessDevExit)
{
    access_dev_exit();
}

extern "C" int access_dev_init(void);
TEST_F(AccessIoctlTestKernel, AccessDevInit)
{
    int ret;
    MOCKER(alloc_chrdev_region).stubs().will(returnValue(-1));
    ret = access_dev_init();
    EXPECT_EQ(-1, ret);

    GlobalMockObject::verify();
    MOCKER(alloc_chrdev_region).stubs().will(returnValue(0));
    MOCKER(cdev_init).stubs().will(returnValue(0));
    MOCKER(cdev_add).stubs().will(returnValue(-1));
    ret = access_dev_init();
    EXPECT_EQ(-1, ret);
}

extern "C" bool IS_ERR(const void *ptr);
extern "C" long PTR_ERR(const void *ptr);
TEST_F(AccessIoctlTestKernel, AccessDevInitTwo)
{
    int ret;

    MOCKER(alloc_chrdev_region).stubs().will(returnValue(0));
    MOCKER(cdev_init).stubs().will(returnValue(0));
    MOCKER(cdev_add).stubs().will(returnValue(0));
    MOCKER(IS_ERR).stubs().will(returnValue(false)).then(returnValue(true));
    MOCKER(PTR_ERR).stubs().will(returnValue(-2));
    ret = access_dev_init();
    EXPECT_EQ(-2, ret);
}

extern "C" void access_remove_all_pid(void);
TEST_F(AccessIoctlTestKernel, AccessIoctlExit)
{
    MOCKER(access_remove_all_pid).stubs().will(ignoreReturnValue());
    MOCKER(access_dev_exit).stubs().will(ignoreReturnValue());
    access_ioctl_exit();
}

extern "C" int access_ioctl_init(void);
TEST_F(AccessIoctlTestKernel, AccessIoctlInit)
{
    int ret = access_ioctl_init();
    EXPECT_EQ(ret, 0);
}


extern "C" long ioctl_read_pid_freq(void __user *argp);
extern "C" void update_hist_tracking(void);
extern "C" int read_pid_freq(pid_t pid, size_t *data_len, u16 **data);
extern "C" int transfer_frequency_data(struct access_pid_freq_msg *msg, u16 **data);

TEST_F(AccessIoctlTestKernel, IoctlReadPidFreqSuccess)
{
    struct access_pid_freq_msg msg = {};
    msg.pid = 1234;
    for (int i = 0; i < SMAP_MAX_NUMNODES; i++) {
        msg.len[i] = 10;
    }
    void *user_argp = &msg;
    ap_data.state_flag = 4;
    struct access_pid *ap1 = (struct access_pid *)malloc(sizeof(struct access_pid));
    struct access_pid *ap2 = (struct access_pid *)malloc(sizeof(struct access_pid));
    ap1->pid = 1234;
    ap2->pid = 124;
    INIT_LIST_HEAD(&ap_data.list);
    list_add_tail(&ap1->node, &ap_data.list);
    list_add_tail(&ap2->node, &ap_data.list);

    MOCKER(copy_from_user).stubs().with(outBoundP((void *)&msg, sizeof(msg))).will(returnValue(0UL));
    MOCKER(update_hist_tracking).stubs();
    MOCKER(read_pid_freq).stubs().will(returnValue(0));
    MOCKER(transfer_frequency_data).stubs().will(returnValue(0));

    long ret = ioctl_read_pid_freq(user_argp);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(ap_data.state_flag, AP_STATE_WALK | AP_STATE_READ | AP_STATE_FREQ | AP_STATE_MIG);
    ap_data.state_flag = 1;
    free(ap1);
    free(ap2);
}

TEST_F(AccessIoctlTestKernel, IoctlReadPidFreqFail)
{
    struct access_pid_freq_msg msg = {};
    msg.pid = 1234;
    for (int i = 0; i < SMAP_MAX_NUMNODES; i++) {
        msg.len[i] = 10;
    }
    void *user_argp = &msg;
    ap_data.state_flag = 4;
    struct access_pid *ap1 = (struct access_pid *)malloc(sizeof(struct access_pid));
    struct access_pid *ap2 = (struct access_pid *)malloc(sizeof(struct access_pid));
    ap1->pid = 1234;
    ap2->pid = 124;
    INIT_LIST_HEAD(&ap_data.list);
    list_add_tail(&ap1->node, &ap_data.list);
    list_add_tail(&ap2->node, &ap_data.list);

    MOCKER(copy_from_user).stubs().with(outBoundP((void *)&msg, sizeof(msg))).will(returnValue(0UL));
    MOCKER(update_hist_tracking).stubs();
    MOCKER(read_pid_freq).stubs().will(returnValue(0)).then(returnValue(-EINVAL));

    MOCKER(transfer_frequency_data).stubs().will(returnValue(-EFAULT));
    // transfer_frequency_data fail
    long ret = ioctl_read_pid_freq(user_argp);
    EXPECT_EQ(ret, -EFAULT);
    EXPECT_EQ(ap_data.state_flag, AP_STATE_WALK | AP_STATE_READ | AP_STATE_FREQ);

    ap_data.state_flag = 4;
    // read_pid_freq fail
    ret = ioctl_read_pid_freq(user_argp);
    EXPECT_EQ(ret, -EINVAL);
    EXPECT_EQ(ap_data.state_flag, AP_STATE_WALK | AP_STATE_READ | AP_STATE_FREQ);

    ap_data.state_flag = 4;
    ap1->pid = 123;
    ap2->pid = 124;
    // is_pid_freq_msg_valid fail
    ret = ioctl_read_pid_freq(user_argp);
    EXPECT_EQ(ret, -EINVAL);
    EXPECT_EQ(ap_data.state_flag, AP_STATE_WALK | AP_STATE_READ | AP_STATE_FREQ);

    ap_data.state_flag = 1;
    // check_and_clear_ap_state fail
    ret = ioctl_read_pid_freq(user_argp);
    EXPECT_EQ(ret, -EAGAIN);
    EXPECT_EQ(ap_data.state_flag, AP_STATE_WALK);

    ap_data.state_flag = 1;
    free(ap1);
    free(ap2);
}