/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: SMAP 测试代码
 */

#include <random>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>

#include "mig_init.h"
#include "common.h"
#include "err.h"
#include "iomem.h"


using namespace std;

class MigInitTest : public ::testing::Test {
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

extern "C" void free_migrate_list_addr(int len, struct mig_list *mlist);
TEST_F(MigInitTest, FreeMigrateListAddr)
{
    struct mig_list mlist[1];
    mlist[0].addr = (u64 *)malloc(sizeof(u64));
    ASSERT_NE(nullptr, mlist[0].addr);
    free_migrate_list_addr(0, mlist);
    ASSERT_NE(nullptr, mlist[0].addr);
    free_migrate_list_addr(1, mlist);
    EXPECT_EQ(nullptr, mlist[0].addr);
}

extern "C" void free_migrate_list(struct mig_list **mlist);
TEST_F(MigInitTest, FreeMigrateList)
{
    struct mig_list *mlist = (struct mig_list *)malloc(sizeof(struct mig_list));
    ASSERT_NE(nullptr, mlist);
    free_migrate_list(&mlist);
    EXPECT_EQ(nullptr, mlist);
}

extern "C" int create_migrate_list(struct migrate_msg *msg, struct mig_list **mlist);
TEST_F(MigInitTest, CreateMigrateList)
{
    struct mig_list *mlist;
    struct migrate_msg msg;
    msg.cnt = 1;
    msg.mig_list = (struct mig_list *)malloc(sizeof(struct mig_list));

    MOCKER(copy_from_user).stubs().will(returnValue(0UL));
    int ret = create_migrate_list(&msg, &mlist);
    EXPECT_EQ(0, ret);
    free_migrate_list(&mlist);

    GlobalMockObject::verify();
    MOCKER(copy_from_user).stubs().will(returnValue(1UL));
    ret = create_migrate_list(&msg, &mlist);
    EXPECT_EQ(-EFAULT, ret);
    free(msg.mig_list);
}

extern "C" int init_migrate_list_addr(int len, struct mig_list *mlist);
TEST_F(MigInitTest, InitMigrateListAddrInvalidParameter)
{
    struct mig_list *mlist = nullptr;
    int ret = init_migrate_list_addr(0, mlist);
    EXPECT_EQ(-EINVAL, ret);

    ret = init_migrate_list_addr(1, mlist);
    EXPECT_EQ(-EINVAL, ret);
    
    mlist = (struct mig_list *)malloc(sizeof(struct mig_list));
    ASSERT_NE(nullptr, mlist);
    mlist[0].nr = 0;
    ret = init_migrate_list_addr(1, mlist);
    EXPECT_EQ(-EINVAL, ret);
    free(mlist);
}

TEST_F(MigInitTest, InitMigrateListAddrVzallocExceptionBranch)
{
    struct mig_list *mlist = nullptr;
    mlist = (struct mig_list *)malloc(sizeof(struct mig_list) * 2);
    ASSERT_NE(nullptr, mlist);
    u64 addr1[1] = { 0x1 };
    u64 addr2[1] = { 0x2 };
    u64 *tmp = (u64 *)malloc(sizeof(u64));
    ASSERT_NE(nullptr, tmp);

    mlist[0].nr = 1;
    mlist[0].addr = addr1;
    mlist[1].nr = 1;
    mlist[1].addr = addr2;
    MOCKER(vzalloc).stubs().will(returnValue((void *)tmp)).then(returnValue((void *)nullptr));
    MOCKER(copy_from_user).stubs().will(returnValue(0UL));
    int ret = init_migrate_list_addr(2, mlist);
    EXPECT_EQ(-ENOMEM, ret);
    EXPECT_EQ(nullptr, mlist[0].addr);
    free(mlist);
}

TEST_F(MigInitTest, InitMigrateListAddrCopyExceptionBranch)
{
    u64 *tmp = (u64 *)malloc(sizeof(u64));
    ASSERT_NE(nullptr, tmp);
    struct mig_list *mlist = nullptr;
    mlist = (struct mig_list *)malloc(sizeof(struct mig_list));
    ASSERT_NE(nullptr, mlist);

    u64 addr[1] = { 0x1 };
    mlist[0].nr = 1;
    mlist[0].addr = addr;
    MOCKER(vzalloc).stubs().will(returnValue((void *)tmp));
    MOCKER(copy_from_user).stubs().will(returnValue(1UL));
    MOCKER(vfree).stubs().will(ignoreReturnValue());
    int ret = init_migrate_list_addr(1, mlist);
    EXPECT_EQ(-EFAULT, ret);
    free(mlist);
}

extern "C" int convert_migrate_list(int len, struct mig_list *mlist);
extern "C" int build_migrate_list(struct migrate_msg *msg, struct mig_list **mlist);
TEST_F(MigInitTest, BuildMigrateListSuccess)
{
    struct migrate_msg msg;
    struct mig_list *mlist;

    MOCKER(create_migrate_list).stubs().will(returnValue(0));
    MOCKER(init_migrate_list_addr).stubs().will(returnValue(0));
    MOCKER(convert_migrate_list).stubs().will(returnValue(0));
    int ret = build_migrate_list(&msg, &mlist);
    EXPECT_EQ(0, ret);
}

extern "C" unsigned long copy_from_user(void *to, const void *from, unsigned long n);
extern "C" int __ioctl_migrate(void __user *argp);
TEST_F(MigInitTest, __IoctlMigrateCopyFromUserError)
{
    MOCKER(copy_from_user).stubs().will(returnValue(1UL));
    int ret = __ioctl_migrate(NULL);
    EXPECT_EQ(-EFAULT, ret);
}

extern "C" bool is_migrate_msg_valid(struct migrate_msg *msg);
extern "C" int build_migrate_list(struct migrate_msg *msg, struct mig_list **mlist);
extern "C" void free_migrate_list_addr(int len, struct mig_list *mlist);
extern "C" void free_migrate_list(struct mig_list **mlist);
extern "C" unsigned int smap_pgsize;
TEST_F(MigInitTest, isMigrateMsgValidFailed)
{
    smap_pgsize = HUGE_PAGE;
    struct mig_list migList[1];
    struct mig_pra migPar = {.page_size = TWO_MEGA_SIZE, .nr_thread = MAX_NR_MIGRATE_THREADS + 1,
                             .is_mul_thread = false};
    struct migrate_msg msg = {.cnt = -1, .mul_mig = migPar, .mig_list = migList};
    bool ret = is_migrate_msg_valid(&msg);
    EXPECT_EQ(false, ret);

    msg.cnt = MAX_2M_MIGMSG_CNT + 1;
    ret = is_migrate_msg_valid(&msg);
    EXPECT_EQ(false, ret);

    msg.mul_mig.page_size = PAGE_SIZE;
    ret = is_migrate_msg_valid(&msg);
    EXPECT_EQ(false, ret);

    msg.mul_mig.is_mul_thread = true;
    ret = is_migrate_msg_valid(&msg);
    EXPECT_EQ(false, ret);
}

TEST_F(MigInitTest, isMigrateMsgValidSuccess)
{
    smap_pgsize = NORMAL_PAGE;
    struct mig_list migList[1];
    struct mig_pra migPar = {.page_size = PAGE_SIZE, .nr_thread = 1,
                             .is_mul_thread = false};
    struct migrate_msg msg = {.cnt = 1, .mul_mig = migPar, .mig_list = migList};
    bool ret = is_migrate_msg_valid(&msg);
    EXPECT_EQ(true, ret);

    msg.mul_mig.is_mul_thread = true;
    msg.mul_mig.nr_thread = 2;
    ret = is_migrate_msg_valid(&msg);
    EXPECT_EQ(true, ret);

    smap_pgsize = HUGE_PAGE;
    msg.mul_mig.page_size = TWO_MEGA_SIZE;
    ret = is_migrate_msg_valid(&msg);
    EXPECT_EQ(true, ret);
}

TEST_F(MigInitTest, convertMigrateListTest)
{
    int len = 1024;
    u64 addr[1024];
    struct mig_list mlist;
    mlist.nr = len;
    mlist.addr = addr;
    // generate random addresses with fixed seed
    mt19937_64 gen(1234);
    uniform_int_distribution<u64> dist(0x1000, 0xFFFFF);
    for (int i = 0; i < len; ++i) {
        addr[i] = dist(gen);
    }
    MOCKER(convert_pos_to_paddr_sorted).stubs().will(ignoreReturnValue());
    int ret = convert_migrate_list(1, nullptr);
    EXPECT_EQ(-EINVAL, ret);

    // check whether addr sorted
    ret = convert_migrate_list(1, &mlist);
    EXPECT_EQ(0, ret);
    for (int i = 1; i < len; ++i) {
        EXPECT_LE(addr[i - 1], addr[i]);
    }
}

TEST_F(MigInitTest, __IoctlMigrateMigrateMsgInvalid)
{
    MOCKER(copy_from_user).stubs().will(returnValue(0UL));
    MOCKER(is_migrate_msg_valid).stubs().will(returnValue(false));
    int ret = __ioctl_migrate(NULL);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(MigInitTest, __IoctlMigrateBuildMigrateListError)
{
    MOCKER(copy_from_user).stubs().will(returnValue(0UL));
    MOCKER(is_migrate_msg_valid).stubs().will(returnValue(true));
    MOCKER(build_migrate_list).stubs().will(returnValue(-EFAULT));
    int ret = __ioctl_migrate(NULL);
    EXPECT_EQ(-EFAULT, ret);
}

TEST_F(MigInitTest, __IoctlMigrateDoMigrateError)
{
    MOCKER(copy_from_user).stubs().will(returnValue(0UL));
    MOCKER(is_migrate_msg_valid).stubs().will(returnValue(true));
    MOCKER(build_migrate_list).stubs().will(returnValue(0));
    MOCKER(do_migrate).stubs().will(returnValue(-EFAULT));
    MOCKER(filter_4k_migrate_info).stubs().will(returnValue(0UL));
    MOCKER(copy_to_user).stubs().will(returnValue(0UL));
    MOCKER(free_migrate_list_addr).stubs().will(ignoreReturnValue());
    MOCKER(free_migrate_list).stubs().will(ignoreReturnValue());
    int ret = __ioctl_migrate(NULL);
    EXPECT_EQ(-EFAULT, ret);
}

TEST_F(MigInitTest, __IoctlMigrateOK)
{
    MOCKER(copy_from_user).stubs().will(returnValue(0UL));
    MOCKER(is_migrate_msg_valid).stubs().will(returnValue(true));
    MOCKER(build_migrate_list).stubs().will(returnValue(0));
    MOCKER(do_migrate).stubs().will(returnValue(0));
    MOCKER(filter_4k_migrate_info).stubs().will(returnValue(0UL));
    MOCKER(copy_to_user).stubs().will(returnValue(0UL));
    MOCKER(free_migrate_list_addr).stubs().will(ignoreReturnValue());
    MOCKER(free_migrate_list).stubs().will(ignoreReturnValue());
    int ret = __ioctl_migrate(NULL);
    EXPECT_EQ(0, ret);
}

extern "C" int __ioctl_migrate_numa(void __user *argp);
extern "C" int nr_local_numa;
TEST_F(MigInitTest, __IoctlMigrateNumaWithValidMsgCount)
{
    nr_local_numa = 4;
    struct migrate_numa_msg msg = {.src_nid = 4, .dest_nid = 5, .count = 2};
    MOCKER(copy_from_user).stubs()
        .with(outBoundP((void*)&msg, sizeof(msg)))
        .will(returnValue(0UL));
    MOCKER(find_range_by_memid).stubs().will(returnValue(0));
    MOCKER(smap_is_remote_addr_valid).stubs().will(returnValue(0));
    MOCKER(smap_migrate_numa).stubs().will(returnValue(0UL));
    // success case
    int ret = __ioctl_migrate_numa(&msg);
    EXPECT_EQ(0, ret);

    // invalid numa nid case
    nr_local_numa = 5;
    ret = __ioctl_migrate_numa(&msg);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(MigInitTest, __IoctlMigrateNumaWithInvalidMsgCount)
{
    nr_local_numa = 4;
    u64 memids[MAX_NR_MIGNUMA];
    struct migrate_numa_msg msg = {.src_nid = 4, .dest_nid = 5, .count = MAX_NR_MIGNUMA + 1};
    MOCKER(copy_from_user).stubs()
        .with(outBoundP((void*)&msg, sizeof(msg)))
        .will(returnValue(0UL));
    MOCKER(find_range_by_memid).stubs().will(returnValue(0));
    MOCKER(smap_is_remote_addr_valid).stubs().will(returnValue(0));
    MOCKER(smap_migrate_numa).stubs().will(returnValue(0UL));
    int ret = __ioctl_migrate_numa(&msg);
    EXPECT_EQ(-EINVAL, ret);
}

extern "C" int check_mig_msg(struct migrate_pid_remote_numa_msg *msg);
TEST_F(MigInitTest, CheckMigMsgNidError)
{
    struct migrate_pid_remote_numa_msg msg;

    nr_local_numa = 4;
    msg.src_nid = 1;
    msg.dest_nid = 5;
    int ret = check_mig_msg(&msg);
    EXPECT_EQ(-EINVAL, ret);

    msg.src_nid = 4;
    msg.dest_nid = 1;
    ret = check_mig_msg(&msg);
    EXPECT_EQ(-EINVAL, ret);

    msg.src_nid = 4;
    msg.dest_nid = 4;
    ret = check_mig_msg(&msg);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(MigInitTest, CheckMigMsgCntError)
{
    struct migrate_pid_remote_numa_msg msg;

    msg.src_nid = 4;
    msg.dest_nid = 5;
    msg.pid_cnt = 0;
    int ret = check_mig_msg(&msg);
    EXPECT_EQ(-EINVAL, ret);

    msg.pid_cnt = 10;
    msg.pid_list = nullptr;
    msg.mig_res_array = nullptr;
    ret = check_mig_msg(&msg);
    EXPECT_EQ(-EINVAL, ret);
}

extern "C" int __ioctl_migrate_pid_remote_numa(void __user *argp);
TEST_F(MigInitTest, __IoctlMigratePidRemoteNuma)
{
    struct migrate_pid_remote_numa_msg msg;

    msg.pid_cnt = 1;
    MOCKER(copy_from_user).stubs().will(returnValue(0UL));
    MOCKER(check_mig_msg).stubs().will(returnValue(0));
    MOCKER(kzalloc).stubs().will(returnValue(static_cast<void*>(nullptr)));
    int ret = __ioctl_migrate_pid_remote_numa(nullptr);
    EXPECT_EQ(-ENOMEM, ret);
}

extern "C" int __ioctl_check_pagesize(void __user *argp);
TEST_F(MigInitTest, __IoctlCheckPagesizeNormal)
{
    uint32_t pageType = 1;
    MOCKER(copy_from_user)
        .stubs()
        .with(outBoundP(static_cast<void*>(&pageType), sizeof(uint32_t)))
        .will(returnValue(0UL));
    int ret = __ioctl_check_pagesize(NULL);
    EXPECT_EQ(0, ret);
}

TEST_F(MigInitTest, __IoctlCheckPagesizeAbnormalOne)
{
    MOCKER(copy_from_user)
        .stubs()
        .will(returnValue(1UL));
    int ret = __ioctl_check_pagesize(NULL);
    EXPECT_EQ(-EFAULT, ret);
}

TEST_F(MigInitTest, __IoctlCheckPagesizeAbnormalTwo)
{
    uint32_t pageType = 0;
    MOCKER(copy_from_user)
        .stubs()
        .with(outBoundP(static_cast<void*>(&pageType), sizeof(uint32_t)))
        .will(returnValue(0UL));
    int ret = __ioctl_check_pagesize(NULL);
    EXPECT_EQ(-EINVAL, ret);
}

extern "C" void walkpage_and_migrate(struct migrate_pid_remote_numa_msg *msg, pid_t *pid_arry,
    int *mig_res);
TEST_F(MigInitTest, walkpage_and_migrate_Success)
{
    struct pagemapread pm = { 0 };
    pm.mig_info.mig_cnt = 1;
    struct migrate_pid_remote_numa_msg msg = {
        .src_nid = 4,
        .dest_nid = 5,
        .pid_cnt = 2,
    };
    pid_t pid_arry[] = {1234, 5678};
    int successful_pids[2] = {0};

    MOCKER(get_node_page_cnt_iomem).stubs().will(returnValue(1));
    MOCKER(walk_pid_pagemap).stubs()
        .with(outBoundP(&pm, sizeof(pm))).will(ignoreReturnValue());
    MOCKER(smap_migrate).stubs().will(returnValue(0));

    walkpage_and_migrate(&msg, pid_arry, successful_pids);

    EXPECT_EQ(successful_pids[0], 1);
    EXPECT_EQ(successful_pids[1], 1);
}

extern "C" long smu_mig_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
TEST_F(MigInitTest, SmuMigIoctrl)
{
    int ret = 0;
    unsigned int cmd = 0;
    cmd = 0x100;
    ret = smu_mig_ioctl(nullptr, cmd, 0);
    EXPECT_EQ(-EINVAL, ret);

    cmd = 0xB900;
    ret = smu_mig_ioctl(nullptr, cmd, 0);
    EXPECT_EQ(-ENOTTY, ret);
}

extern "C" int smu_mig_open(struct inode *inode, struct file *file);
TEST_F(MigInitTest, SmuMigOpen)
{
    int ret = 1;
    ret = smu_mig_open(nullptr, nullptr);
    EXPECT_EQ(0, ret);
}

extern "C" int smu_mig_ioctl_release(struct inode *inode, struct file *file);
TEST_F(MigInitTest, SmuMigIoctlRelase)
{
    int ret = 1;
    ret = smu_mig_ioctl_release(nullptr, nullptr);
    EXPECT_EQ(0, ret);
}

extern "C" int init_mig_dev(void);
extern "C" bool IS_ERR(const void *ptr);
extern "C" void class_destroy(struct class_stub *cls);
extern "C" long __must_check PTR_ERR(__force const void *ptr);
TEST_F(MigInitTest, InitMigDevOne)
{
    int ret = 0;
    MOCKER(alloc_chrdev_region).stubs().will(returnValue(-1));
    ret = init_mig_dev();
    EXPECT_EQ(-1, ret);
}

TEST_F(MigInitTest, InitMigDevTwo)
{
    int ret = 0;
    MOCKER(alloc_chrdev_region).stubs().will(returnValue(0));
    MOCKER(cdev_init).stubs().will(ignoreReturnValue());
    MOCKER(cdev_add).stubs().will(returnValue(1));
    MOCKER(unregister_chrdev_region).stubs().will(ignoreReturnValue());
    ret = init_mig_dev();
    EXPECT_EQ(1, ret);
}

TEST_F(MigInitTest, InitMigDevThree)
{
    int ret = 0;
    MOCKER(alloc_chrdev_region).stubs().will(returnValue(0));
    MOCKER(cdev_init).stubs().will(ignoreReturnValue());
    MOCKER(cdev_add).stubs().will(returnValue(0));
    MOCKER(IS_ERR).stubs().will(returnValue(true));
    MOCKER(PTR_ERR).stubs().will(returnValue(2));
    MOCKER(cdev_del).stubs().will(ignoreReturnValue());
    ret = init_mig_dev();
    EXPECT_EQ(2, ret);
}

TEST_F(MigInitTest, InitMigDevFour)
{
    int ret = 0;
    MOCKER(alloc_chrdev_region).stubs().will(returnValue(0));
    MOCKER(cdev_init).stubs().will(ignoreReturnValue());
    MOCKER(cdev_add).stubs().will(returnValue(0));
    MOCKER(IS_ERR)
        .stubs()
        .will(returnValue(false))
        .then(returnValue(true));
    MOCKER(PTR_ERR).stubs().will(returnValue(3));
    MOCKER(class_destroy).stubs().will(ignoreReturnValue());
    ret = init_mig_dev();
    EXPECT_EQ(3, ret);
}

TEST_F(MigInitTest, InitMigDevFive)
{
    int ret = -1;
    MOCKER(alloc_chrdev_region).stubs().will(returnValue(0));
    MOCKER(cdev_init).stubs().will(ignoreReturnValue());
    MOCKER(cdev_add).stubs().will(returnValue(0));
    MOCKER(IS_ERR)
        .stubs()
        .will(returnValue(false))
        .then(returnValue(false));
    ret = init_mig_dev();
    EXPECT_EQ(0, ret);
}

extern "C" void exit_mig_dev(void);
TEST_F(MigInitTest, ExitMigDev)
{
    MOCKER(device_destroy).stubs().will(ignoreReturnValue());
    MOCKER(class_destroy).stubs().will(ignoreReturnValue());
    MOCKER(cdev_del).stubs().will(ignoreReturnValue());
    MOCKER(unregister_chrdev_region).stubs().will(ignoreReturnValue());
    exit_mig_dev();
}

extern "C" int init_migrate();
TEST_F(MigInitTest, InitMigrate)
{
    int ret;
    MOCKER(init_mig_dev).stubs().will(returnValue(1));
    ret = init_migrate();
    EXPECT_EQ(1, ret);
    GlobalMockObject::verify();

    MOCKER(init_mig_dev).stubs().will(returnValue(0));
    ret = init_migrate();
    EXPECT_EQ(0, ret);
    GlobalMockObject::verify();
}

extern "C" void exit_migrate();
TEST_F(MigInitTest, ExitMigrate)
{
    MOCKER(exit_mig_dev).stubs().will(ignoreReturnValue());
    exit_migrate();
    GlobalMockObject::verify();
}

extern "C" struct page *pfn_to_online_page(unsigned long pfn);
extern "C" int PageHuge(struct page *page);
extern "C" int is_filter_4k(struct page *page, int page_size);
extern "C" int smap_check_huge_page_for_migration(struct page *page, pid_t pid);

// __ioctl_migrate end-to-end test MOCKER here
// msg->mig_list, msg->mig_list->addr should be alloced by caller
static void IoctlMigrateE2ETestMock(struct migrate_msg *msg, unsigned int pageSize,
    unsigned int mockSuccessMig4KPageNrEachMigList, unsigned int mockCanMig2MPageNrEachMigList)
{
    smap_pgsize = pageSize;
    if (pageSize == HUGE_PAGE) {
        MOCKER(PageHuge).stubs().will(returnValue(1));
        MOCKER(is_filter_4k).stubs().will(returnValue(-1));
        for (int i = 0; i < msg->cnt; ++i) {
            MOCKER(smap_check_huge_page_for_migration).stubs()
                .with(any(), eq(msg->mig_list[i].pid))
                .will(repeat(0, mockCanMig2MPageNrEachMigList))
                .then(returnValue(1));
        }
    } else {
        MOCKER(PageHuge).stubs().will(returnValue(0));
    }
    struct mig_list *migList = msg->mig_list;
    struct page page = {0};
    MOCKER(copy_from_user).stubs()
        .with(outBoundP(static_cast<void*>(msg), sizeof(struct migrate_msg*)))
        .will(returnValue(0UL));
    MOCKER(build_migrate_list).stubs()
        .with(any(), outBoundP(static_cast<struct mig_list**>(&migList), sizeof(struct mig_list**)))
        .will(returnValue(0));
    MOCKER(pfn_to_online_page).stubs().will(returnValue(&page));
    MOCKER(IS_ERR).stubs().will(returnValue(false));
    MOCKER(isolate_and_migrate_folios).stubs()
        .with(any(), any(), any(), any(), any(), any(),
             outBoundP(static_cast<unsigned int*>(&mockSuccessMig4KPageNrEachMigList), sizeof(unsigned int*)))
        .will(returnValue(0));
    MOCKER(free_migrate_list_addr).stubs().will(ignoreReturnValue());
    MOCKER(free_migrate_list).stubs().will(ignoreReturnValue());
}

// __ioctl_migrate end-to-end test
TEST_F(MigInitTest, __IoctlMigrateE2ETest)
{
    struct mig_list *migList;
    int ret;
    int cnt = 2;
    void *argp;
    struct migrate_msg msg;
    EXPECT_NE(nullptr, migList);
    migList = (struct mig_list*)vzalloc(cnt * sizeof(struct mig_list));
    u64 startAddr = 0x100000000;
    int addCnt = 0;
    for (int i = 0; i < cnt; ++i) {
        migList[i].from = 0;
        migList[i].to = 5;
        migList[i].pid = 105428 + i;
        migList[i].nr = 5120 * (i + 1);  // 5120, 10240, 15360 etc
        migList[i].addr = (u64*)vzalloc(migList[i].nr * sizeof(u64));
        EXPECT_NE(nullptr, migList[i].addr);
        for (int j = 0; j < migList[i].nr; ++j) {
            migList[i].addr[j] = startAddr + 4096 * (addCnt);
            ++addCnt;
        }
    }
    msg.cnt = cnt;
    msg.mig_list = migList;
    msg.mul_mig.page_size = TWO_MEGA_SIZE;
    msg.mul_mig.is_mul_thread = false;

    // 10 huge page can migrate, 5120 page migrate success
    IoctlMigrateE2ETestMock(&msg, HUGE_PAGE, 5120, 10);
    ret = __ioctl_migrate(argp);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(5110, migList[0].failed_pre_migrated_nr);
    EXPECT_EQ(0, migList[0].failed_mig_nr);
    EXPECT_EQ(10230, migList[1].failed_pre_migrated_nr);
    EXPECT_EQ(0, migList[1].failed_mig_nr);

    GlobalMockObject::verify();
    // 10 huge page can migrate, 0 page migrate success
    IoctlMigrateE2ETestMock(&msg, HUGE_PAGE, 0, 10);
    ret = __ioctl_migrate(argp);
    EXPECT_EQ(20, ret);
    EXPECT_EQ(5110, migList[0].failed_pre_migrated_nr);
    EXPECT_EQ(10, migList[0].failed_mig_nr);
    EXPECT_EQ(10230, migList[1].failed_pre_migrated_nr);
    EXPECT_EQ(10, migList[1].failed_mig_nr);
}
