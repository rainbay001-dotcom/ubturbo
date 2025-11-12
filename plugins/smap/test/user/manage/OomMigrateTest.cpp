 /*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: smap5.0 user oom migrate ut code
 * Create: 2024-10-25
 */

#include <cstdlib>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include "strategy/migration.h"
#include "smap_env.h"
#include "manage/manage.h"
#include "manage/oom_migrate.h"

using namespace std;

class OomMigrateTest : public ::testing::Test {
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

extern "C" int InitOomMigrateMsg(struct MigrateMsg *mMsg, struct ProcessManager *manager);
TEST_F(OomMigrateTest, TestInitOomMigrateMsg)
{
    int ret;
    struct MigrateMsg mMsg;
    TrackingAttr tracking;
    tracking.pageSize = PAGESIZE_4K;
    struct ProcessManager manager = {
        .nrNuma = 10,
        .nrThread = 2,
    };
    manager.tracking = tracking;
    ret = InitOomMigrateMsg(&mMsg, &manager);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0, mMsg.cnt);
}

extern "C" int InitMigList(struct MigList *mList, uint64_t *pageCount, ProcessAttr *attr);
extern "C" int GetNumaNodesForPid(pid_t pid, int *node);
extern "C" unsigned long GetPidNrPages(pid_t pid);
TEST_F(OomMigrateTest, TestInitMigList)
{
    int ret;
    int node = 0;
    uint64_t pageCount = 500;
    uint64_t sumPages = 1000;
    struct MigrateMsg mMsg;
    struct MigList mList;
    struct ProcessManager manager = {
        .nrNuma = 10,
        .nrThread = 2,
        .nrLocalNuma = 10
    };
    ProcessAttr mockProcess = {};
    mockProcess.strategyAttr.l3RemoteMemRatio[0][0] = 50;

    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    MOCKER(GetNumaNodesForPid).stubs().with(any(), outBoundP(&node, sizeof(node))).will(returnValue(-1));
    ret = InitMigList(&mList, &pageCount, &mockProcess);
    EXPECT_EQ(-1, ret);
    GlobalMockObject::verify();
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    MOCKER(GetNumaNodesForPid).stubs().with(any(), outBoundP(&node, sizeof(node))).will(returnValue(0));
    MOCKER(GetPidNrPages).stubs().will(returnValue(sumPages));
    ret = InitMigList(&mList, &pageCount, &mockProcess);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(pageCount, mList.nr);
}

TEST_F(OomMigrateTest, TestInitMigListSecond)
{
    int ret;
    int node = 0;
    uint64_t pageCount = 500;
    uint64_t sumPages = 1000;
    struct MigrateMsg mMsg;
    struct MigList mList;
    struct ProcessManager manager = {
        .nrNuma = 10,
        .nrThread = 2,
        .nrLocalNuma = 10
    };
    ProcessAttr mockProcess = {};
    mockProcess.strategyAttr.l3RemoteMemRatio[0][0] = 100;

    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    MOCKER(GetNumaNodesForPid).stubs().with(any(), outBoundP(&node, sizeof(node))).will(returnValue(0));
    MOCKER(GetPidNrPages).stubs().will(returnValue(sumPages));
    ret = InitMigList(&mList, &pageCount, &mockProcess);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(500, mList.nr);

    GlobalMockObject::verify();
    node = -1;
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    MOCKER(GetNumaNodesForPid).stubs().with(any(), outBoundP(&node, sizeof(node))).will(returnValue(0));
    ret = InitMigList(&mList, &pageCount, &mockProcess);
    EXPECT_EQ(-EINVAL, ret);
}

extern "C" int OpenPidPagemapFile(pid_t pid, int *pagemapFd);
extern "C" int GetPaddrsFromPagemap(ProcessAttr *attr, int pagemapFd,
    uint64_t *pageCount, struct MigList *mList);
extern "C" void FindEnoughPageToMigrate(uint64_t *pageCount, ProcessAttr *attr, struct MigrateMsg *mMsg);
TEST_F(OomMigrateTest, TestFindEnoughPageToMigrate)
{
    int ret;
    struct MigrateMsg mMsg;
    mMsg.cnt = 0;
    struct MigList mList;
    uint64_t nrPages = 10;
    mList.addr = (uint64_t *)malloc(sizeof(uint64_t) * nrPages);
    mList.nr = nrPages;
    for (int i = 0; i < mList.nr; i++) {
        mList.addr[i] = i;
    }
    uint64_t pageCount = 500;
    ProcessAttr mockProcess;
    MOCKER(InitMigList).stubs().with(outBoundP(&mList, sizeof(struct MigList)), any(), any()).will(returnValue(0));
    MOCKER(OpenPidPagemapFile).stubs().will(returnValue(0));
    MOCKER(GetPaddrsFromPagemap).stubs().will(returnValue(0));
    MOCKER(close).stubs().will(ignoreReturnValue());
    MOCKER(AddMigList).stubs().will(returnValue(0)).then(returnValue(0));
    FindEnoughPageToMigrate(&pageCount, &mockProcess, &mMsg);
    EXPECT_EQ(0, mMsg.cnt);
}

TEST_F(OomMigrateTest, TestFindEnoughPageToMigrateSecond)
{
    int ret;
    struct MigrateMsg mMsg;
    struct MigList mList;
    mList.nr = 0;
    uint64_t pageCount = 500;
    ProcessAttr mockProcess;
    MOCKER(InitMigList).stubs().with(outBoundP(&mList, sizeof(struct MigList)), any(), any()).will(returnValue(0));
    MOCKER(OpenPidPagemapFile).expects(never());
    FindEnoughPageToMigrate(&pageCount, &mockProcess, &mMsg);
    EXPECT_EQ(0, mList.nr);
}

TEST_F(OomMigrateTest, TestFindEnoughPageToMigrateThird)
{
    int ret;
    struct MigrateMsg mMsg;
    struct MigList mList;
    mList.nr = 1;
    uint64_t pageCount = 500;
    ProcessAttr mockProcess;

    MOCKER(InitMigList).stubs().with(outBoundP(&mList, sizeof(struct MigList)), any(), any()).will(returnValue(-1));
    MOCKER(OpenPidPagemapFile).expects(never());
    FindEnoughPageToMigrate(&pageCount, &mockProcess, &mMsg);
    EXPECT_EQ(1, mList.nr);
}

TEST_F(OomMigrateTest, TestFindEnoughPageToMigrateForth)
{
    int ret;
    struct MigrateMsg mMsg;
    struct MigList mList;
    mList.nr = 1;
    mList.addr = (uint64_t *)malloc(sizeof(uint64_t) * mList.nr);
    uint64_t pageCount = 500;
    ProcessAttr mockProcess;

    MOCKER(InitMigList).stubs().with(outBoundP(&mList, sizeof(struct MigList)), any(), any()).will(returnValue(0));
    MOCKER(OpenPidPagemapFile).stubs().will(returnValue(-1));
    MOCKER(GetPaddrsFromPagemap).expects(never());
    FindEnoughPageToMigrate(&pageCount, &mockProcess, &mMsg);
    EXPECT_EQ(1, mList.nr);
}

TEST_F(OomMigrateTest, TestFindEnoughPageToMigrateFifth)
{
    int ret;
    struct MigrateMsg mMsg;
    struct MigList mList;
    mList.nr = 1;
    mList.addr = (uint64_t *)malloc(sizeof(uint64_t) * mList.nr);
    uint64_t pageCount = 500;
    ProcessAttr mockProcess;

    MOCKER(InitMigList).stubs().with(outBoundP(&mList, sizeof(struct MigList)), any(), any()).will(returnValue(0));
    MOCKER(OpenPidPagemapFile).stubs().will(returnValue(0));
    MOCKER(GetPaddrsFromPagemap).stubs().will(returnValue(-1));
    MOCKER(close).stubs().will(ignoreReturnValue());
    MOCKER(AddMigList).expects(never());
    FindEnoughPageToMigrate(&pageCount, &mockProcess, &mMsg);
    EXPECT_EQ(1, mList.nr);
}

extern "C" int EnvMutexInit(EnvMutex *mutex);
extern "C" void FindPidMigrateSize(uint64_t size);
TEST_F(OomMigrateTest, TestFindPidMigrateSizeAbnormalOne)
{
    uint64_t size = 0;
    struct MigrateMsg mMsg = {
        .cnt = 1
    };
    struct ProcessManager manager;
    ProcessAttr attr = {
        .pid = 1,
        .next = nullptr
    };
    manager.processes = &attr;
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    MOCKER(InitOomMigrateMsg)
        .stubs()
        .with(outBoundP(&mMsg, sizeof(struct MigrateMsg)), any())
        .will(returnValue(1));
    FindPidMigrateSize(size);
    EXPECT_EQ(1, mMsg.cnt);
}

extern "C" int DoMigration(struct MigrateMsg *mMsg, struct ProcessManager *manager);
TEST_F(OomMigrateTest, TestFindPidMigrateSizeNormalOne)
{
    uint64_t size = 0;
    struct MigrateMsg mMsg = {
        .cnt = 0
    };
    struct ProcessManager manager;
    ProcessAttr attr = {};
    attr.pid = 1;
    attr.state = PROC_IDLE;
    attr.walkPage.nrPages[0] = 0;
    attr.walkPage.nrPages[1] = 0;
    attr.next = nullptr;
    manager.tracking.pageSize = PAGESIZE_4K;
    manager.processes = &attr;
    uint64_t pageCount = size / manager.tracking.pageSize;
    EnvMutexInit(&manager.lock);
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    MOCKER(InitOomMigrateMsg).stubs().will(returnValue(-1));
    MOCKER(FindEnoughPageToMigrate);
    MOCKER(DoMigration).stubs().will(returnValue(1));
    FindPidMigrateSize(size);
}

extern "C" int GetPaddrFromMemRange(int pagemapFd, unsigned long start, unsigned long end,
    struct MigList *mList, uint64_t *pageCount);
extern "C" FILE *fopen(const char *__restrict __filename, const char *__restrict __modes);
extern "C" char *fgets(char *__restrict __s, int __n, FILE *__restrict __stream);
extern "C" int fclose(FILE *__stream);
extern "C" int sscanf_s(const char *buffer, const char *format, ...);
extern "C" int snprintf_s(char *strDest, size_t destMax, size_t count, const char *format,
                           ...);
extern "C" int GetPaddrsFromPagemap(ProcessAttr *attr, int pagemapFd,
    uint64_t *pageCount, struct MigList *mList);
TEST_F(OomMigrateTest, TestGetPaddrsFromPagemap)
{
    int ret;
    pid_t pid = 123;
    int pagemapFd = 1;
    ProcessAttr mockProcess = { .pid = pid };

    struct MigList mList;
    mList.nr = 0;
    uint64_t pageCount = 500;

    MOCKER((int (*)(char *, unsigned long, unsigned long, char const *, void *))snprintf_s)
        .stubs()
        .will(returnValue(0));
    static FILE fake_file;
    MOCKER(fopen).stubs().will(returnValue(&fake_file));
    MOCKER(fgets).stubs().will(returnValue(static_cast<char *>("1"))).then(returnValue((static_cast<char *>(nullptr))));
    MOCKER((int (*)(char *, char const *, unsigned long *, unsigned long *))sscanf_s)
        .stubs()
        .will(returnValue(MAPS_LIN_LEN));
    MOCKER(IsHugeMode).stubs().will(returnValue(false));
    MOCKER(GetPaddrFromMemRange).stubs().will(returnValue(0));
    MOCKER(fclose).stubs().will(returnValue(0));
    ret = GetPaddrsFromPagemap(&mockProcess, pagemapFd, &pageCount, &mList);
    EXPECT_EQ(0, ret);
}

TEST_F(OomMigrateTest, TestGetPaddrsFromPagemapSecond)
{
    int ret;
    pid_t pid = 123;
    int pagemapFd = 1;
    ProcessAttr mockProcess = { .pid = pid };
    struct MigList mList;
    uint64_t pageCount = 500;
    MOCKER((int (*)(char *, unsigned long, unsigned long, char const *, void *))snprintf_s)
        .stubs()
        .will(returnValue(-1));
    ret = GetPaddrsFromPagemap(&mockProcess, pagemapFd, &pageCount, &mList);
    EXPECT_EQ(-1, ret);

    GlobalMockObject::verify();
    MOCKER((int (*)(char *, unsigned long, unsigned long, char const *, void *))snprintf_s)
        .stubs()
        .will(returnValue(0));
    MOCKER(fopen).stubs().will(returnValue(static_cast<FILE *>(nullptr)));
    ret = GetPaddrsFromPagemap(&mockProcess, pagemapFd, &pageCount, &mList);
    EXPECT_EQ(-ENODEV, ret);
}

TEST_F(OomMigrateTest, TestGetPaddrsFromPagemapThird)
{
    int ret;
    pid_t pid = 123;
    int pagemapFd = 1;
    ProcessAttr mockProcess = { .pid = pid };
    struct MigList mList;
    uint64_t pageCount = 500;

    MOCKER((int (*)(char *, unsigned long, unsigned long, char const *, void *))snprintf_s)
        .stubs()
        .will(returnValue(0));
    static FILE fake_file;
    MOCKER(fopen).stubs().will(returnValue(&fake_file));
    MOCKER(fgets).stubs().will(returnValue(static_cast<char *>("1"))).then(returnValue((static_cast<char *>(nullptr))));
    MOCKER((int (*)(char *, char const *, unsigned long *, unsigned long *))sscanf_s)
        .stubs()
        .will(returnValue(MAPS_LIN_LEN));
    MOCKER(IsHugeMode).stubs().will(returnValue(true));
    MOCKER(IsHugePageRange).stubs().will(returnValue(0));

    MOCKER(fclose).stubs().will(returnValue(-1));
    ret = GetPaddrsFromPagemap(&mockProcess, pagemapFd, &pageCount, &mList);
    EXPECT_EQ(-1, ret);
}

TEST_F(OomMigrateTest, TestGetPaddrsFromPagemapForth)
{
    int ret;
    pid_t pid = 123;
    int pagemapFd = 1;
    ProcessAttr mockProcess = { .pid = pid };

    struct MigList mList;
    mList.nr = 0;
    uint64_t pageCount = 500;

    MOCKER((int (*)(char *, unsigned long, unsigned long, char const *, void *))snprintf_s)
        .stubs()
        .will(returnValue(0));
    static FILE fake_file;
    MOCKER(fopen).stubs().will(returnValue(&fake_file));
    MOCKER(fgets).stubs().will(returnValue(static_cast<char *>("1"))).then(returnValue((static_cast<char *>(nullptr))));
    MOCKER((int (*)(char *, char const *, unsigned long *, unsigned long *))sscanf_s)
        .stubs()
        .will(returnValue(MAPS_LIN_LEN));
    MOCKER(IsHugeMode).stubs().will(returnValue(false));
    MOCKER(GetPaddrFromMemRange).stubs().will(returnValue(-1));
    MOCKER(fclose).stubs().will(returnValue(0));
    ret = GetPaddrsFromPagemap(&mockProcess, pagemapFd, &pageCount, &mList);
    EXPECT_EQ(-1, ret);
}

TEST_F(OomMigrateTest, TestGetPaddrFromMemRange)
{
    int ret;
    int pagemapFd = 1;
    unsigned long start = 100;
    unsigned long end = 200;
    struct MigList mList;
    uint64_t pageCount = 10;

    MOCKER(lseek).stubs().will(returnValue((off_t)-1));
    ret = GetPaddrFromMemRange(pagemapFd, start, end, &mList, &pageCount);
    EXPECT_EQ(-EINVAL, ret);
}

extern "C" ssize_t read(int fd, void *buf, size_t count);
TEST_F(OomMigrateTest, TestGetPaddrFromMemRangeSecond)
{
    int ret;
    int pagemapFd = 1;
    unsigned long start = 100;
    unsigned long end = 200;
    struct MigList mList;
    uint64_t pageCount = 10;

    MOCKER(lseek).stubs().will(returnValue(0));
    MOCKER(read).stubs().will(returnValue(0));
    ret = GetPaddrFromMemRange(pagemapFd, start, end, &mList, &pageCount);
    EXPECT_EQ(-EINVAL, ret);
}

extern "C" void OomGetPaddr(uint64_t entry, struct MigList *mList, uint64_t *pageCount);
TEST_F(OomMigrateTest, TestOomGetPaddr)
{
    uint64_t entry = 1ULL << 63;
    struct MigList mList = {0};
    mList.nr = 1;
    mList.addr = (uint64_t *)malloc(sizeof(uint64_t) * mList.nr);
    uint64_t pageCount = 1;
    MOCKER(IsHugeMode).stubs().will(returnValue(false));
    MOCKER(IsHugeAligned).stubs().will(returnValue(false));
    OomGetPaddr(entry, &mList, &pageCount);
    EXPECT_EQ(0, pageCount);
    free(mList.addr);
}

TEST_F(OomMigrateTest, TestGetPaddrFromMemRangeThird)
{
    int ret;
    int pagemapFd = 1;
    unsigned long start = 100;
    unsigned long end = 200;
    struct MigList mList;
    mList.nr = 0;
    uint64_t pageCount = 10;
    MOCKER(lseek).stubs().will(returnValue(0));
    MOCKER(read).stubs().will(returnValue(PAGEMAP_ENTRY_SIZE));
    MOCKER(OomGetPaddr).stubs().will(ignoreReturnValue());

    ret = GetPaddrFromMemRange(pagemapFd, start, end, &mList, &pageCount);
    EXPECT_EQ(0, ret);

    mList.nr = 1;
    ret = GetPaddrFromMemRange(pagemapFd, start, end, &mList, &pageCount);
    EXPECT_EQ(0, ret);
}

extern "C" int open(const char *pathname, int flags);
extern "C" int OpenPidPagemapFile(pid_t pid, int *pagemapFd);
TEST_F(OomMigrateTest, TestOpenPidPagemapFile)
{
    int ret;
    int pagemapFd = 1;
    pid_t pid = 1;
    MOCKER((int (*)(char *, unsigned long, unsigned long, char const *, void *))snprintf_s)
        .stubs()
        .will(returnValue(-1));
    ret = OpenPidPagemapFile(pid, &pagemapFd);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(OomMigrateTest, TestOpenPidPagemapFileSecond)
{
    int ret;
    int pagemapFd = 1;
    pid_t pid = 1;

    MOCKER((int (*)(char *, unsigned long, unsigned long, char const *, void *))snprintf_s)
        .stubs()
        .will(returnValue(0));
    MOCKER(open).stubs().will(returnValue(-1));
    ret = OpenPidPagemapFile(pid, &pagemapFd);
    EXPECT_EQ(-ENODEV, ret);
}

TEST_F(OomMigrateTest, TestOpenPidPagemapFileThird)
{
    int ret;
    int pagemapFd = 1;
    pid_t pid = 1;
    MOCKER((int (*)(char *, unsigned long, unsigned long, char const *, void *))snprintf_s)
        .stubs()
        .will(returnValue(0));
    MOCKER(open).stubs().will(returnValue(0));
    ret = OpenPidPagemapFile(pid, &pagemapFd);
    EXPECT_EQ(0, ret);
}
