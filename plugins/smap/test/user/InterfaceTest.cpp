/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: smap5.0 user smap interface ut code
 */
#include <sys/ioctl.h>
#include <cstdlib>
#include <malloc.h>
#include <string.h>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include "smap_interface.h"
#include "manage/manage.h"
#include "manage/access_ioctl.h"
#include "manage/thread.h"
#include "manage/device.h"
#include "manage/smap_config.h"
#include "strategy/migration.h"
#include "securec.h"
#include "smap_user_log.h"
#include "smap_env.h"

using namespace std;

extern "C" EnvAtomic g_status;
extern "C" struct ProcessManager g_processManager;

class InterfaceTest : public ::testing::Test {
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

    bool EnvMutexIsRelease(EnvMutex *mutex);
};

bool InterfaceTest::EnvMutexIsRelease(EnvMutex *mutex)
{
    if (pthread_mutex_trylock(&mutex->lock)) {
        return false;
    }
    pthread_mutex_unlock(&mutex->lock);
    return true;
}

extern "C" bool IsRatioValid(int ratio);
TEST_F(InterfaceTest, TestIsRatioValidOne)
{
    int ratio = 25;
    bool ret = IsRatioValid(ratio);
    EXPECT_EQ(true, ret);
}

extern "C" bool IsMigOutCountValid(pid_t *pidArr, int len, int pidType);
extern "C" ProcessAttr *GetProcessAttrLocked(pid_t pid);
TEST_F(InterfaceTest, TestIsMigOutCountValid)
{
    int maxNum = MAX_4K_PROCESSES_CNT;
    ProcessManager manager;
    EnvMutexInit(&manager.lock);
    int len = 1;
    pid_t *pidArr = (pid_t *)malloc(sizeof(pid_t) * len);
    ProcessAttr *attr = nullptr;
    MOCKER(LoadMangerNrProcessNum).stubs().will(returnValue(maxNum));
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    MOCKER(GetProcessAttrLocked).stubs().will(returnValue(attr));
    bool ret = IsMigOutCountValid(pidArr, len, 0);
    EXPECT_EQ(false, ret);

    GlobalMockObject::verify();
    maxNum = MAX_2M_PROCESSES_CNT - 1;
    MOCKER(LoadMangerNrVmNum).stubs().will(returnValue(0));
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    MOCKER(GetProcessAttrLocked).stubs().will(returnValue(attr));
    ret = IsMigOutCountValid(pidArr, len, 1);
    EXPECT_EQ(true, ret);
    free(pidArr);
}

extern "C" bool IsCountValid(int count, int max);
TEST_F(InterfaceTest, TestIsCountValid)
{
    int count = 0;
    bool ret = IsCountValid(count, 0);
    EXPECT_EQ(false, ret);

    count = MAX_NR_MIGOUT + 1;
    ret = IsCountValid(count, MAX_NR_MIGOUT);
    EXPECT_EQ(false, ret);
}

TEST_F(InterfaceTest, TestIsCountValidTwo)
{
    int ret;
    int count = MAX_NR_MIGOUT;

    ret = IsCountValid(count, MAX_NR_MIGOUT);
    EXPECT_EQ(true, ret);
}

extern "C" int CheckPidtype(uint32_t pageType);
extern "C" int snprintf_s(char *strDest, unsigned long destMax, unsigned long count, char const *format, ...);
TEST_F(InterfaceTest, TestCheckPidtypeOne)
{
    int ret;
    uint32_t pageType;

    pageType = PAGESIZE_4K;
    ret = CheckPidtype(pageType);
    EXPECT_EQ(-EINVAL, ret);

    pageType = PAGETYPE_4K;
    MOCKER((int (*)(char *, unsigned long, unsigned long, char const *, void *))snprintf_s)
        .stubs()
        .will(returnValue(-1));
    ret = CheckPidtype(pageType);
    EXPECT_EQ(-EINVAL, ret);
}

extern "C" int open(const char *pathname, int flags);
TEST_F(InterfaceTest, TestCheckPidtypeTwo)
{
    int ret;
    uint32_t pageType;

    pageType = PAGETYPE_4K;
    MOCKER((int (*)(char *, unsigned long, unsigned long, char const *, void *))snprintf_s)
        .stubs()
        .will(returnValue(0));
    MOCKER(reinterpret_cast<int (*)(const char *, int)>(open)).stubs().will(returnValue(-1));
    ret = CheckPidtype(pageType);
    EXPECT_EQ(-ENODEV, ret);
}

TEST_F(InterfaceTest, TestCheckPidtypeThree)
{
    int ret;
    uint32_t pageType;

    pageType = PAGETYPE_4K;
    MOCKER((int (*)(char *, unsigned long, unsigned long, char const *, void *))snprintf_s)
        .stubs()
        .will(returnValue(0));

    MOCKER(reinterpret_cast<int (*)(const char *, int)>(open)).stubs().will(returnValue(0));
    MOCKER(reinterpret_cast<int (*)(int, unsigned long, void *)>(ioctl)).stubs().will(returnValue(0));
    ret = CheckPidtype(pageType);
    EXPECT_EQ(0, ret);
}

TEST_F(InterfaceTest, TestCheckPidtypeFour)
{
    int ret;
    uint32_t pageType;

    pageType = PAGETYPE_4K;
    MOCKER((int (*)(char *, unsigned long, unsigned long, char const *, void *))snprintf_s)
        .stubs()
        .will(returnValue(0));

    MOCKER(reinterpret_cast<int (*)(const char *, int)>(open)).stubs().will(returnValue(0));
    MOCKER(reinterpret_cast<int (*)(int, unsigned long, void *)>(ioctl)).stubs().will(returnValue(-1));
    ret = CheckPidtype(pageType);
    EXPECT_EQ(-EINVAL, ret);
}

extern "C" bool IsPidTypeValid(int pidType);
TEST_F(InterfaceTest, TestIsPidTypeValid)
{
    bool ret;
    int pidType = PAGETYPE_4K;
    struct ProcessManager manager;

    manager.tracking.pageSize = PAGESIZE_4K;
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    ret = IsPidTypeValid(pidType);
    EXPECT_EQ(true, ret);
}

TEST_F(InterfaceTest, TestIsPidTypeInValidOne)
{
    bool ret;
    int pidType = PAGETYPE_4K;
    struct ProcessManager manager;

    manager.tracking.pageSize = PAGETYPE_2M;
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    ret = IsPidTypeValid(pidType);
    EXPECT_EQ(false, ret);
}

TEST_F(InterfaceTest, TestIsPidTypeInValidTwo)
{
    bool ret;
    int pidType = PAGETYPE_4K;

    MOCKER(GetProcessManager).stubs().will(returnValue((struct ProcessManager *)nullptr));
    ret = IsPidTypeValid(pidType);
    EXPECT_EQ(false, ret);
}

extern "C" bool IsLocalNidValid(int nid);
TEST_F(InterfaceTest, TestIsLocalNidValid)
{
    bool ret;
    struct ProcessManager manager;

    manager.nrLocalNuma = 4;
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    ret = IsLocalNidValid(0);
    EXPECT_EQ(true, ret);

    ret = IsLocalNidValid(4);
    EXPECT_EQ(false, ret);
}

extern "C" void EnvMutexLock(EnvMutex *mutex);
extern "C" void EnvMutexUnlock(EnvMutex *mutex);
extern "C" int ScanMigrateWork(ThreadCtx *ctx);
extern "C" int InitAllThreads(struct ProcessManager *manager);
extern "C" int InitThread(struct ProcessManager *manager, uint32_t period, WorkFunc workFunc);
TEST_F(InterfaceTest, TestIsRatioValidTwo)
{
    int ratio = 101;
    bool ret = IsRatioValid(ratio);
    EXPECT_EQ(false, ret);
}

TEST_F(InterfaceTest, TestIsRatioValidThree)
{
    int ratio = -1;
    int ret = IsRatioValid(ratio);
    EXPECT_EQ(false, ret);
}

extern "C" int InitVirAPI(void);
TEST_F(InterfaceTest, TestInitVirAPI)
{
    int ret;
    MOCKER(OpenVirHandler).stubs().will(returnValue(-1));
    MOCKER(CloseVirHandler).stubs().will(returnValue(0));
    ret = InitVirAPI();
    EXPECT_EQ(-9, ret);

    GlobalMockObject::verify();
    MOCKER(OpenVirHandler).stubs().will(returnValue(0));
    ret = InitVirAPI();
    EXPECT_EQ(0, ret);
}

TEST_F(InterfaceTest, TestInitAllThreads)
{
    int ret;
    struct ProcessManager pm;

    EnvMutexInit(&pm.threadLock);
    MOCKER(IsHugeMode).stubs().will(returnValue(false));
    MOCKER(InitThread).stubs().will(returnValue(-EPERM));
    MOCKER(DestroyAllThread).expects(once()).will(returnValue(0));
    ret = InitAllThreads(&pm);
    EXPECT_EQ(-EPERM, ret);
}

extern "C" bool IsNidInNumastat(int nid);
extern "C" bool IsRemoteNidValid(int nid);
TEST_F(InterfaceTest, IsRemoteNidValid)
{
    int nid = -1;
    struct ProcessManager pm = { .nrLocalNuma = 2 };
    MOCKER(GetProcessManager).stubs().will(returnValue(&pm));
    bool ret = IsRemoteNidValid(nid);
    EXPECT_EQ(false, ret);
}

TEST_F(InterfaceTest, IsRemoteNidValidOne)
{
    int nid = 3;
    struct ProcessManager pm = {
        .nrLocalNuma = 2,
    };
    MOCKER(GetProcessManager).stubs().will(returnValue(&pm));
    MOCKER(IsNidInNumastat).stubs().will(returnValue(false));
    bool ret = IsRemoteNidValid(nid);
    EXPECT_EQ(false, ret);
}

TEST_F(InterfaceTest, IsRemoteNidValidTwo)
{
    int nid = 4;
    struct ProcessManager pm = {
        .nrLocalNuma = 2,
    };
    MOCKER(GetProcessManager).stubs().will(returnValue(&pm));
    MOCKER(IsNidInNumastat).stubs().will(returnValue(true));
    int ret = IsRemoteNidValid(nid);
    EXPECT_EQ(true, ret);
}

TEST_F(InterfaceTest, IsRemoteNidValidThree)
{
    int nid = 11;
    MOCKER(GetProcessManager).stubs().will(returnValue(static_cast<ProcessManager*>(nullptr)));
    int ret = IsRemoteNidValid(nid);
    EXPECT_EQ(false, ret);
}

TEST_F(InterfaceTest, IsNidInNumastat)
{
    bool ret;
    int nid = 0;
    MOCKER(popen).stubs().will(returnValue(static_cast<FILE *>(nullptr)));
    ret = IsNidInNumastat(nid);
    EXPECT_EQ(false, ret);
}

TEST_F(InterfaceTest, IsNidInNumastatOne)
{
    bool ret;
    int nid = -1;
    ret = IsNidInNumastat(nid);
    EXPECT_EQ(false, ret);
}

TEST_F(InterfaceTest, IsNidInNumastatTrue)
{
    bool ret;
    FILE fp;
    const char *stubNodeStr = "Node 0";
    const char *stubCmdline = "Node 0          Node 1           Total";
    char *nodeStr = (char *)calloc(1, strlen(stubNodeStr) + 1);
    char *cmdline = (char *)calloc(1, strlen(stubCmdline) + 1);

    memcpy(nodeStr, stubNodeStr, strlen(stubNodeStr));
    memcpy(cmdline, stubCmdline, strlen(stubCmdline));

    MOCKER((int (*)(char *, unsigned long, unsigned long, char const *, void *))snprintf_s)
        .stubs()
        .with(outBoundP(static_cast<char*>(nodeStr), sizeof(nodeStr)))
        .will(returnValue(0));
    MOCKER(fgets)
        .stubs()
        .with(outBoundP(static_cast<char*>(cmdline), sizeof(cmdline)))
        .will(returnValue(cmdline));

    ret = IsNidInNumastat(0);
    EXPECT_EQ(true, ret);
    free(nodeStr);
    free(cmdline);
}

extern "C" bool IsMigParaValid(struct MigrateOutPayload *payload, uint32_t *nodeBitmap, int pidType);
extern "C" bool IsPidRemoteNidValid(int *nidArray, int nidCnt, pid_t pid, uint32_t *nodeBitmap, int pidType);
extern "C" bool IsDestNidVaild(int nid, pid_t pid);
TEST_F(InterfaceTest, IsMigParaValid)
{
    struct MigrateOutPayload payload = {
        .destNid = 4,
        .pid = 123,
        .ratio = 25,
        .memSize = 10240,
        .migrateMode = MIG_MEMSIZE_MODE,
    };
    MOCKER(IsRemoteNidValid).stubs().will(returnValue(true));
    MOCKER(IsPidRemoteNidValid).stubs().will(returnValue(true));
    MOCKER(IsDestNidVaild).stubs().will(returnValue(true));
    MOCKER(GetRunMode).stubs().will(returnValue(MEM_POOL_MODE));

    bool ret = IsMigParaValid(&payload, nullptr, PAGETYPE_2M);
    EXPECT_EQ(true, ret);
}

TEST_F(InterfaceTest, IsMigParaValidGreaterThanMigrateMode)
{
    struct MigrateOutPayload payload = {
        .destNid = 4,
        .pid = 123,
        .ratio = 25,
        .memSize = 10240,
    };
    MOCKER(IsRemoteNidValid).stubs().will(returnValue(true));
    MOCKER(IsPidRemoteNidValid).stubs().will(returnValue(true));
    MOCKER(IsDestNidVaild).stubs().will(returnValue(true));
    payload.migrateMode = static_cast<MigrateMode>(2);
    MOCKER(GetRunMode).stubs().will(returnValue(MEM_POOL_MODE));

    bool ret = IsMigParaValid(&payload, nullptr, PAGETYPE_2M);
    EXPECT_EQ(false, ret);
}

TEST_F(InterfaceTest, IsMigParaValidPassNullPtr)
{
    bool ret;
    struct MigrateOutPayload *payload = nullptr;

    ret = IsMigParaValid(payload, nullptr, PAGETYPE_2M);
    EXPECT_EQ(false, ret);
    free(payload);
}

TEST_F(InterfaceTest, IsMigParaValidLessThanMigrateMode)
{
    bool ret;
    struct MigrateOutPayload payload = {
        .destNid = 4,
        .pid = 123,
        .ratio = 25,
        .memSize = 10240,
    };
    payload.migrateMode = static_cast<MigrateMode>(-1);

    MOCKER(IsRemoteNidValid).stubs().will(returnValue(true));
    MOCKER(IsPidRemoteNidValid).stubs().will(returnValue(true));
    MOCKER(IsDestNidVaild).stubs().will(returnValue(true));
    ret = IsMigParaValid(&payload, nullptr, PAGETYPE_2M);
    EXPECT_EQ(false, ret);
}

TEST_F(InterfaceTest, IsMigParaValidWaterlineModeAndMigMemsizeMode)
{
    bool ret;
    struct MigrateOutPayload payload = {
        .destNid = 4,
        .pid = 123,
        .ratio = 25,
        .memSize = 10240,
    };
    MOCKER(IsRemoteNidValid).stubs().will(returnValue(true));
    MOCKER(IsPidRemoteNidValid).stubs().will(returnValue(true));
    MOCKER(IsDestNidVaild).stubs().will(returnValue(true));
    MOCKER(GetRunMode).stubs().will(returnValue(0));
    payload.migrateMode = MIG_MEMSIZE_MODE;

    ret = IsMigParaValid(&payload, nullptr, PAGETYPE_2M);
    EXPECT_EQ(false, ret);
}

TEST_F(InterfaceTest, IsMigParaValidMemPoolModeAndMigRatioMode)
{
    bool ret;
    struct MigrateOutPayload payload = {
        .destNid = 4,
        .pid = 123,
        .ratio = 25,
        .memSize = 10240,
    };
    MOCKER(IsRemoteNidValid).stubs().will(returnValue(true));
    MOCKER(IsPidRemoteNidValid).stubs().will(returnValue(true));
    MOCKER(IsDestNidVaild).stubs().will(returnValue(true));
    MOCKER(GetRunMode).stubs().will(returnValue(1));
    payload.migrateMode = MIG_RATIO_MODE;

    ret = IsMigParaValid(&payload, nullptr, PAGETYPE_2M);
    EXPECT_EQ(false, ret);
}

TEST_F(InterfaceTest, IsMigParaValidMigRatioModeWithInvalidRatio)
{
    bool ret;
    struct MigrateOutPayload payload = {
        .destNid = 4,
        .pid = 123,
        .ratio = 25,
        .memSize = 10240,
    };
    MOCKER(IsRemoteNidValid).stubs().will(returnValue(true));
    MOCKER(IsPidRemoteNidValid).stubs().will(returnValue(true));
    MOCKER(IsDestNidVaild).stubs().will(returnValue(true));
    MOCKER(GetRunMode).stubs().will(returnValue(1));
    MOCKER(IsRatioValid).stubs().will(returnValue(false));
    payload.migrateMode = MIG_RATIO_MODE;
    ret = IsMigParaValid(&payload, nullptr, PAGETYPE_2M);
    EXPECT_EQ(false, ret);
}

TEST_F(InterfaceTest, IsMigParaValidInvalidMemSize)
{
    bool ret;
    struct MigrateOutPayload payload = {
        .destNid = 4,
        .pid = 123,
        .ratio = 25,
        .memSize = 10240,
    };
    MOCKER(IsRemoteNidValid).stubs().will(returnValue(true));
    MOCKER(IsPidRemoteNidValid).stubs().will(returnValue(true));
    MOCKER(IsDestNidVaild).stubs().will(returnValue(true));
    MOCKER(GetRunMode).stubs().will(returnValue(1));
    MOCKER(IsRatioValid).stubs().will(returnValue(true));
    payload.migrateMode = MIG_MEMSIZE_MODE;
    payload.memSize = 2049;
    ret = IsMigParaValid(&payload, nullptr, PAGETYPE_2M);
    EXPECT_EQ(false, ret);
}

TEST_F(InterfaceTest, TestIsDestNidVaild)
{
    ProcessAttr attr = {};
    attr.pid = 1;
    attr.numaAttr.numaNodes = 0b00010001;
    MOCKER(GetProcessAttrLocked).stubs().will(returnValue((ProcessAttr *)nullptr));
    bool ret = IsDestNidVaild(1, 1);
    EXPECT_EQ(true, ret);

    GlobalMockObject::verify();
    MOCKER(GetProcessAttrLocked).stubs().will(returnValue(&attr));
    ret = IsDestNidVaild(1, 4);
    EXPECT_EQ(false, ret);

    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    ret = IsDestNidVaild(4, 4);
    EXPECT_EQ(true, ret);
}

extern "C" int CheckMigrateOutMsg(struct MigrateOutMsg *msg, int pidType, int *pidCount);
TEST_F(InterfaceTest, TestCheckMigrateOutMsgInvalidPidTypeAndMsgCount)
{
    struct MigrateOutMsg msg;
    int pidType = PAGETYPE_4K;
    int pidCount = 0;
    g_processManager.tracking.pageSize = PAGESIZE_2M;
    // null input case
    int ret = CheckMigrateOutMsg(nullptr, pidType, &pidCount);
    EXPECT_EQ(-EINVAL, ret);

    // invalid pidType case
    ret = CheckMigrateOutMsg(&msg, pidType, &pidCount);
    EXPECT_EQ(-EINVAL, ret);

    // invalid count case
    g_processManager.tracking.pageSize = PAGESIZE_4K;
    msg.count = MAX_NR_MIGOUT + 1;
    ret = CheckMigrateOutMsg(&msg, pidType, &pidCount);
    EXPECT_EQ(-EINVAL, ret);
    msg.count = 0;
    ret = CheckMigrateOutMsg(&msg, pidType, &pidCount);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestCheckMigrateOutMsgMigOutCount)
{
    struct MigrateOutMsg msg = {.count = 1};
    msg.payload[0].pid = 1234;
    int pidType = PAGETYPE_2M;
    int pidCount = 0;
    g_processManager.tracking.pageSize = PAGESIZE_2M;

    // number of managed and non-managed PID beyond limit
    g_processManager.processes = nullptr;
    g_processManager.nr[VM_TYPE] = MAX_2M_PROCESSES_CNT;
    int ret = CheckMigrateOutMsg(&msg, pidType, &pidCount);
    EXPECT_EQ(-EINVAL, ret);

    // PID has been managed, so not beyond limit
    ProcessAttr processes = {.pid = msg.payload[0].pid, .next = nullptr};
    g_processManager.processes = &processes;
    MOCKER(IsMigParaValid).stubs().will(returnValue(true));
    ret = CheckMigrateOutMsg(&msg, pidType, &pidCount);
    EXPECT_EQ(0, ret);
}

TEST_F(InterfaceTest, TestCheckMigrateOutMsgInvalidDestNid)
{
    struct MigrateOutMsg msg = {.count = 1};
    int pidType = PAGETYPE_2M;
    int pidCount = 0;
    g_processManager.tracking.pageSize = PAGESIZE_2M;
    g_processManager.processes = nullptr;
    g_processManager.nr[VM_TYPE] = 1;
    g_processManager.nrLocalNuma = 4;

    MOCKER(IsNidInNumastat).stubs().will(returnValue(true));

    msg.payload[0].pid = 1234;
    msg.payload[0].destNid = 0;
    int ret = CheckMigrateOutMsg(&msg, pidType, &pidCount);
    EXPECT_EQ(-EINVAL, ret);

    // nid not match numaNodes bitmap
    ProcessAttr processes = {.pid = msg.payload[0].pid, .next = nullptr};
    processes.numaAttr.numaNodes = 0x20;
    g_processManager.processes = &processes;
    ret = CheckMigrateOutMsg(&msg, pidType, &pidCount);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestCheckMigrateOutMsgInvalidMigrateMode)
{
    struct MigrateOutMsg msg = {.count = 1};
    msg.payload[0].pid = 1234;
    msg.payload[0].destNid = 4;
    int pidType = PAGETYPE_2M;
    int pidCount = 0;
    g_processManager.tracking.pageSize = PAGESIZE_2M;
    g_processManager.nr[VM_TYPE] = 1;
    g_processManager.nrLocalNuma = 4;
    ProcessAttr processes = {.pid = msg.payload[0].pid, .next = nullptr};
    processes.numaAttr.numaNodes = 0x10;
    g_processManager.processes = &processes;

    MOCKER(IsNidInNumastat).stubs().will(returnValue(true));
    MOCKER(IsPidRemoteNidValid).stubs().will(returnValue(true));
    MOCKER(IsNodeForbidden).stubs().will(returnValue(true));

    msg.payload[0].migrateMode = (MigrateMode)-1;
    int ret = CheckMigrateOutMsg(&msg, pidType, &pidCount);
    EXPECT_EQ(-EINVAL, ret);

    msg.payload[0].migrateMode = (MigrateMode)(MIG_MEMSIZE_MODE + 1);
    ret = CheckMigrateOutMsg(&msg, pidType, &pidCount);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestCheckMigrateOutMsgCheckMigrateMode)
{
    struct MigrateOutMsg msg = {.count = 1};
    int pidType = PAGETYPE_2M;
    int pidCount = 0;
    g_processManager.tracking.pageSize = PAGESIZE_2M;
    g_processManager.nr[VM_TYPE] = 1;
    g_processManager.nrLocalNuma = 4;
    g_processManager.processes = nullptr;

    MOCKER(IsNidInNumastat).stubs().will(returnValue(true));
    MOCKER(IsPidRemoteNidValid).stubs().will(returnValue(true));
    MOCKER(IsNodeForbidden).stubs().will(returnValue(true));
    MOCKER(GetRunMode).stubs().will(returnValue(MEM_POOL_MODE));

    msg.payload[0].destNid = 4;
    msg.payload[0].migrateMode = MIG_RATIO_MODE;
    int ret = CheckMigrateOutMsg(&msg, pidType, &pidCount);
    EXPECT_EQ(-EINVAL, ret);

    msg.payload[0].migrateMode = MIG_MEMSIZE_MODE;
    msg.payload[0].memSize = 2049;
    ret = CheckMigrateOutMsg(&msg, pidType, &pidCount);
    EXPECT_EQ(-EINVAL, ret);

    msg.payload[0].memSize = 2048;
    ret = CheckMigrateOutMsg(&msg, pidType, &pidCount);
    EXPECT_EQ(0, ret);

    GlobalMockObject::verify();
    MOCKER(IsNidInNumastat).stubs().will(returnValue(true));
    MOCKER(IsPidRemoteNidValid).stubs().will(returnValue(true));
    MOCKER(IsNodeForbidden).stubs().will(returnValue(true));
    MOCKER(GetRunMode).stubs().will(returnValue(WATERLINE_MODE));

    ret = CheckMigrateOutMsg(&msg, pidType, &pidCount);
    EXPECT_EQ(-EINVAL, ret);

    msg.payload[0].migrateMode = MIG_RATIO_MODE;
    msg.payload[0].ratio = 101;
    ret = CheckMigrateOutMsg(&msg, pidType, &pidCount);
    EXPECT_EQ(-EINVAL, ret);

    msg.payload[0].ratio = 75;
    ret = CheckMigrateOutMsg(&msg, pidType, &pidCount);
    EXPECT_EQ(0, ret);
}

extern "C" int IoctlHandler(const void *msg, int pidType, const unsigned long *ioctlCommands);
extern "C" int AddProcessNumaBitMap(struct MigrateOutHashNode *hashMsg, int pidCount, uint32_t *nodeBitmap,
                                    int pidType);
extern "C" int AddProcessesToGlobalManager(struct MigrateOutHashNode *hashMsg, int pidCount, int pidType,
                                           uint32_t *nodeBitmap, bool *hasInvalidPid);
extern "C" int ProcessAddTrackingManage(struct MigrateOutHashNode *hashMsg, int pidCount, int pidType,
                                        uint32_t *nodeBitmap);
TEST_F(InterfaceTest, TestSmapMigrateOut)
{
    int ret;
    struct MigrateOutMsg msg;
    msg.payload[0].pid = 1234;
    msg.count = 1;
    EnvAtomicSet(&g_status, 1);
    MOCKER(AccessIoctlAddPid).stubs().will(returnValue(0));
    MOCKER(AddProcessesToGlobalManager).stubs().will(returnValue(0));
    MOCKER(ProcessAddTrackingManage).stubs().will(returnValue(0));
    MOCKER(IsMigParaValid).stubs().will(returnValue(true));
    MOCKER(IsPidTypeValid).stubs().will(returnValue(true));
    ret = ubturbo_smap_migrate_out(&msg, 0);
    EXPECT_EQ(0, ret);
    EXPECT_TRUE(EnvMutexIsRelease(&g_processManager.lock));
}

TEST_F(InterfaceTest, TestSmapMigrateOutTwo)
{
    int ret;

    struct MigrateOutMsg msg;
    EnvAtomicSet(&g_status, 0);
    ret = ubturbo_smap_migrate_out(&msg, 0);
    EXPECT_EQ(-EPERM, ret);
    EXPECT_TRUE(EnvMutexIsRelease(&g_processManager.lock));
}

TEST_F(InterfaceTest, TestSmapMigrateOutThree)
{
    int ret;
    struct MigrateOutMsg msgc;

    EnvAtomicSet(&g_status, 1);
    MOCKER(CheckMigrateOutMsg).stubs().will(returnValue(-EINVAL));
    ret = ubturbo_smap_migrate_out(&msgc, 0);
    EXPECT_EQ(-EINVAL, ret);
    EnvAtomicSet(&g_status, 0);
    EXPECT_TRUE(EnvMutexIsRelease(&g_processManager.lock));
}

TEST_F(InterfaceTest, TestSmapMigrateOutFour)
{
    int ret;
    struct MigrateOutMsg msgc;
    msgc.count = 1;
    msgc.payload[0].pid = 1234;
    EnvAtomicSet(&g_status, 1);
    MOCKER(AccessIoctlAddPid).stubs().will(returnValue(-EBADF));
    MOCKER(SetProcessLocalNuma).stubs().will(returnValue(0));
    MOCKER(IsMigParaValid).stubs().will(returnValue(true));
    MOCKER(IsPidTypeValid).stubs().will(returnValue(true));
    ret = ubturbo_smap_migrate_out(&msgc, PAGETYPE_2M + 1);
    EXPECT_EQ(-EBADF, ret);
    EnvAtomicSet(&g_status, 0);
    EXPECT_TRUE(EnvMutexIsRelease(&g_processManager.lock));
}

TEST_F(InterfaceTest, TestSmapMigrateOutFive)
{
    int ret;
    struct MigrateOutMsg msgc;
    msgc.count = 1;

    EnvAtomicSet(&g_status, 1);
    MOCKER(AccessIoctlAddPid).stubs().will(returnValue(0));
    MOCKER(IsMigParaValid).stubs().will(returnValue(true));
    MOCKER(AddProcessesToGlobalManager).stubs().will(returnValue(-EINVAL));
    ret = ubturbo_smap_migrate_out(&msgc, PAGETYPE_2M + 1);
    EXPECT_EQ(-EINVAL, ret);
    EnvAtomicSet(&g_status, 0);
    EXPECT_TRUE(EnvMutexIsRelease(&g_processManager.lock));
}

TEST_F(InterfaceTest, TestSmapMigrateBackWithSmapIsNotRunning)
{
    int ret;
    EnvAtomicSet(&g_status, 0);
    ret = ubturbo_smap_migrate_back(nullptr);
    EXPECT_EQ(-EPERM, ret);
}

TEST_F(InterfaceTest, TestSmapMigrateBackWithoutMessage)
{
    int ret;
    EnvAtomicSet(&g_status, 1);
    ret = ubturbo_smap_migrate_back(nullptr);
    EXPECT_EQ(-EINVAL, ret);
}

extern "C" int CheckMigrateBackMsg(struct MigrateBackMsg *msg);
TEST_F(InterfaceTest, TestSmapMigrateBackWithMessageCheckFailed)
{
    int ret;
    struct MigrateBackMsg msg = {};
    EnvAtomicSet(&g_status, 1);
    MOCKER(CheckMigrateBackMsg).stubs().will(returnValue(-EINVAL));
    ret = ubturbo_smap_migrate_back(&msg);
    EXPECT_EQ(-EINVAL, ret);
}
extern "C" bool CheckProcessIdle(int nid);
extern "C" bool IsAllL2NodePidInState(ProcessState state, int l2Node);
TEST_F(InterfaceTest, TestCheckProcessIdle)
{
    MOCKER(IsAllL2NodePidInState).stubs().will(returnValue(true));
    bool ret = CheckProcessIdle(1234);
    EXPECT_EQ(true, ret);
}

extern "C" int CheckMigrateBackMsg(struct MigrateBackMsg *msg);
TEST_F(InterfaceTest, TestCheckMigrateBackMsg)
{
    MigrateBackMsg msg = {};
    msg.count = -1;
    int ret = CheckMigrateBackMsg(&msg);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestSmapMigrateBackWithProcessIdleCheckFailed)
{
    int ret;
    struct MigrateBackMsg msg = {};
    msg.count = 1;
    EnvAtomicSet(&g_status, 1);
    MOCKER(CheckMigrateBackMsg).stubs().will(returnValue(0));
    MOCKER(CheckProcessIdle).stubs().will(returnValue(false));
    ret = ubturbo_smap_migrate_back(&msg);
    EXPECT_EQ(-EAGAIN, ret);
}

TEST_F(InterfaceTest, TestSmapMigrateBackSuccess)
{
    int ret;
    struct MigrateBackMsg msgc = {
        .taskID = 1,
        .count = 1,
        .payload = { { 4, 5, 1 } }
    };

    EnvAtomicSet(&g_status, 1);
    MOCKER(CheckMigrateBackMsg).stubs().will(returnValue(0));
    MOCKER(SetNodeForbidden).stubs().will(ignoreReturnValue());
    MOCKER(IoctlHandler).stubs().will(returnValue(0));
    ret = ubturbo_smap_migrate_back(&msgc);
    EXPECT_EQ(0, ret);
    EnvAtomicSet(&g_status, 0);
}

extern "C" int CheckSmapRemoveMsg(struct RemoveMsg *msg, int pidType);
TEST_F(InterfaceTest, TestCheckSmapRemoveMsgWithIsCountValidFailed)
{
    int ret;
    struct RemoveMsg msg;
    int pidType = VM_TYPE;
    MOCKER(IsCountValid).stubs().will(returnValue(false));
    ret = CheckSmapRemoveMsg(&msg, pidType);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestCheckSmapRemoveMsgWithIsPidTypeValidFailed)
{
    int ret;
    struct RemoveMsg msg;
    int pidType = VM_TYPE;
    MOCKER(IsCountValid).stubs().will(returnValue(true));
    MOCKER(IsPidTypeValid).stubs().will(returnValue(false));
    ret = CheckSmapRemoveMsg(&msg, pidType);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestSmapRemoveWithSmapIsNotRunning)
{
    EnvAtomicSet(&g_status, 0);
    int ret = ubturbo_smap_remove(nullptr, 0);
    EXPECT_EQ(-EPERM, ret);
}

TEST_F(InterfaceTest, TestSmapRemoveWithoutMessage)
{
    EnvAtomicSet(&g_status, 1);
    int ret = ubturbo_smap_remove(nullptr, 0);
    EXPECT_EQ(-EINVAL, ret);
}

extern "C" int CheckSmapRemoveMsg(struct RemoveMsg *msg, int pidType);
TEST_F(InterfaceTest, TestSmapRemoveWithoutMessageCheckFailed)
{
    struct RemoveMsg msg = {};
    MOCKER(CheckSmapRemoveMsg).stubs().will(returnValue(-EINVAL));
    EnvAtomicSet(&g_status, 1);
    int ret = ubturbo_smap_remove(&msg, 0);
    EXPECT_EQ(-EINVAL, ret);
}

extern "C" int AccessIoctlRemovePid(int len, struct AccessRemovePidPayload *payload);
TEST_F(InterfaceTest, TestSmapRemoveWithoutAccessIoctlRemoveFailed)
{
    struct RemoveMsg msg = {};
    msg.count = 1;
    msg.payload[0].pid = 1024;
    MOCKER(CheckSmapRemoveMsg).stubs().will(returnValue(0));
    MOCKER(AccessIoctlRemovePid).stubs().will(returnValue(-ENOMEM));
    EnvAtomicSet(&g_status, 1);
    int ret = ubturbo_smap_remove(&msg, 0);
    EXPECT_EQ(-ENOMEM, ret);
}

TEST_F(InterfaceTest, TestSmapRemove)
{
    int ret;
    struct RemoveMsg msg;

    EnvAtomicSet(&g_status, 1);
    MOCKER(CheckSmapRemoveMsg).stubs().will(returnValue(0));
    MOCKER(AccessIoctlRemovePid).stubs().will(returnValue(0));
    MOCKER(RemoveManagedProcess).stubs().will(ignoreReturnValue());
    ret = ubturbo_smap_remove(&msg, 0);
    EXPECT_EQ(0, ret);
}

TEST_F(InterfaceTest, TestSmapEnableNodeWithoutSmapIsRunning)
{
    int ret;
    struct EnableNodeMsg msg;
    EnvAtomicSet(&g_status, 0);
    ret = ubturbo_smap_node_enable(&msg);
    EXPECT_EQ(-EPERM, ret);
}

TEST_F(InterfaceTest, TestSmapEnableNodeWithoutMessage)
{
    int ret;
    struct EnableNodeMsg msg;
    EnvAtomicSet(&g_status, 1);
    ret = ubturbo_smap_node_enable(nullptr);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestSmapEnableNodeWithInvalidNid)
{
    int ret;
    struct EnableNodeMsg msg = {};
    msg.nid = MAX_NODES;
    EnvAtomicSet(&g_status, 1);
    ret = ubturbo_smap_node_enable(&msg);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestSmapEnableNodeWithEnable)
{
    int ret;
    struct EnableNodeMsg msg = {};
    msg.nid = 4;
    msg.enable = ENABLE_NUMA_MIG;
    EnvAtomicSet(&g_status, 1);
    ret = ubturbo_smap_node_enable(&msg);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(g_forbiddenNodes[4].counter, 0);
}

TEST_F(InterfaceTest, TestSmapEnableNodeWithDisable)
{
    int ret;
    struct EnableNodeMsg msg = {};
    msg.nid = 4;
    msg.enable = DISABLE_NUMA_MIG;
    EnvAtomicSet(&g_status, 1);
    ret = ubturbo_smap_node_enable(&msg);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(g_forbiddenNodes[4].counter, 1);
}

TEST_F(InterfaceTest, TestSmapEnableNodeWithInvalidEnable)
{
    int ret;
    struct EnableNodeMsg msg = {};
    msg.nid = 4;
    msg.enable = 2;
    EnvAtomicSet(&g_status, 1);
    ret = ubturbo_smap_node_enable(&msg);
    EXPECT_EQ(-EINVAL, ret);
}

void FakeLog(int level, const char *str, const char *moduleName)
{
    return;
}

extern "C" int InitLog(Logfunc extlog);
TEST_F(InterfaceTest, TestInitLog)
{
    int ret;
    Logfunc extlog = FakeLog;

    ret = InitLog(extlog);
    EXPECT_EQ(0, ret);
}

extern "C" void RecoverRemoveInvalidProcess(void);
TEST_F(InterfaceTest, TestRecoverRemoveInvalidProcess)
{
    int ret;
    struct ProcessManager manager = { .nr = { 0, 2 } };
    ProcessAttr *attr;
    ProcessAttr *attr1;
    ProcessAttr *attr2;

    attr1 = (ProcessAttr *)calloc(1, sizeof(*attr1));
    attr2 = (ProcessAttr *)calloc(1, sizeof(*attr2));

    ASSERT_NE(nullptr, attr1);
    ASSERT_NE(nullptr, attr2);

    attr1->type = VM_TYPE;
    attr1->pid = 1025;
    attr2->type = VM_TYPE;
    attr2->pid = 1026;
    EnvMutexInit(&manager.lock);
    LinkedListAdd(&manager.processes, &attr1);
    LinkedListAdd(&manager.processes, &attr2);
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    MOCKER(GetPidType).stubs().will(returnValue(VM_TYPE));
    MOCKER(SyncAllProcessConfig).stubs().will(returnValue(0));
    MOCKER(PidIsValid).stubs().with(eq(attr1->pid)).will(returnValue(false));
    MOCKER(PidIsValid).stubs().with(eq(attr2->pid)).will(returnValue(true));
    RecoverRemoveInvalidProcess();
    EXPECT_EQ(1, manager.nr[VM_TYPE]);
    EXPECT_EQ(attr2->pid, manager.processes->pid);

    attr = manager.processes;
    while (attr) {
        LinkedListRemove(&attr, &manager.processes);
        attr = manager.processes;
    }
}

extern "C" void RecoverAllMMapType(void);
TEST_F(InterfaceTest, TestRecoverAllMMapType)
{
    ProcessManager manager = {};
    ProcessAttr attr = {};
    attr.pid = 1234;
    attr.next = nullptr;
    manager.processes = &attr;
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    MOCKER(VMPreprocess).stubs().will(returnValue(-1));

    RecoverAllMMapType();
}

extern "C" int Recover(void);
extern "C" void RecoverRemoveInvalidProcess(void);
extern "C" int SyncProcessToKernel(void);
TEST_F(InterfaceTest, TestRecover)
{
    int ret;

    MOCKER(RecoverFromConfig).stubs().will(returnValue(-EPERM));
    ret = Recover();
    EXPECT_EQ(-EPERM, ret);

    GlobalMockObject::verify();
    MOCKER(RecoverFromConfig).stubs().will(returnValue(0));
    MOCKER(RecoverRemoveInvalidProcess).stubs().will(ignoreReturnValue());
    MOCKER(SyncProcessToKernel).stubs().will(returnValue(-ENOENT));
    ret = Recover();
    EXPECT_EQ(-ENOENT, ret);
}

TEST_F(InterfaceTest, TestSmapInitWithSmapIsNotRunning)
{
    EnvAtomicSet(&g_status, RUNNING);
    int ret = ubturbo_smap_start(PAGETYPE_4K, nullptr);
    EXPECT_EQ(-EPERM, ret);
}

TEST_F(InterfaceTest, TestSmapInitWithInitLogFailed)
{
    EnvAtomicSet(&g_status, 0);
    MOCKER(InitLog).stubs().will(returnValue(-EINVAL));
    int ret = ubturbo_smap_start(PAGETYPE_4K, nullptr);
    EXPECT_EQ(-EINVAL, ret);
}

extern "C" int CheckPidtype(uint32_t pageType);
TEST_F(InterfaceTest, TestSmapInitWithCheckPidTypeFailed)
{
    EnvAtomicSet(&g_status, 0);
    MOCKER(InitLog).stubs().will(returnValue(0));
    MOCKER(CheckPidtype).stubs().will(returnValue(-EINVAL));
    int ret = ubturbo_smap_start(PAGETYPE_4K, nullptr);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestSmapInitWithProcessManagerInitFailed)
{
    EnvAtomicSet(&g_status, 0);
    MOCKER(InitLog).stubs().will(returnValue(0));
    MOCKER(CheckPidtype).stubs().will(returnValue(0));
    MOCKER(ProcessManagerInit).stubs().will(returnValue(-EINVAL));
    int ret = ubturbo_smap_start(PAGETYPE_4K, nullptr);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestSmapInitWithInitTrackingDevFailed)
{
    EnvAtomicSet(&g_status, 0);
    MOCKER(InitLog).stubs().will(returnValue(0));
    MOCKER(CheckPidtype).stubs().will(returnValue(0));
    MOCKER(ProcessManagerInit).stubs().will(returnValue(0));
    MOCKER(DestroyProcessManager).stubs().will(ignoreReturnValue());
    MOCKER(InitTrackingDev).stubs().will(returnValue(-EINVAL));
    int ret = ubturbo_smap_start(PAGETYPE_4K, nullptr);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestSmapInitWithAccessIoctlRemoveAllPidFailed)
{
    EnvAtomicSet(&g_status, 0);
    MOCKER(InitLog).stubs().will(returnValue(0));
    MOCKER(CheckPidtype).stubs().will(returnValue(0));
    MOCKER(ProcessManagerInit).stubs().will(returnValue(0));
    MOCKER(DestroyProcessManager).stubs().will(ignoreReturnValue());
    MOCKER(InitTrackingDev).stubs().will(returnValue(0));
    MOCKER(AccessIoctlRemoveAllPid).stubs().will(returnValue(-EINVAL));
    int ret = ubturbo_smap_start(PAGETYPE_4K, nullptr);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestSmapInitWithInitVirAPIFailed)
{
    EnvAtomicSet(&g_status, 0);
    MOCKER(InitLog).stubs().will(returnValue(0));
    MOCKER(CheckPidtype).stubs().will(returnValue(0));
    MOCKER(ProcessManagerInit).stubs().will(returnValue(0));
    MOCKER(DestroyProcessManager).stubs().will(ignoreReturnValue());
    MOCKER(InitTrackingDev).stubs().will(returnValue(0));
    MOCKER(AccessIoctlRemoveAllPid).stubs().will(returnValue(0));
    MOCKER(IsHugeMode).stubs().will(returnValue(true));
    MOCKER(InitVirAPI).stubs().will(returnValue(-EINVAL));
    int ret = ubturbo_smap_start(PAGETYPE_4K, nullptr);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestSmapInitWithRecoverFailed)
{
    EnvAtomicSet(&g_status, 0);
    MOCKER(InitLog).stubs().will(returnValue(0));
    MOCKER(CheckPidtype).stubs().will(returnValue(0));
    MOCKER(ProcessManagerInit).stubs().will(returnValue(0));
    MOCKER(DestroyProcessManager).stubs().will(ignoreReturnValue());
    MOCKER(InitTrackingDev).stubs().will(returnValue(0));
    MOCKER(AccessIoctlRemoveAllPid).stubs().will(returnValue(0));
    MOCKER(IsHugeMode).stubs().will(returnValue(false));
    MOCKER(Recover).stubs().will(returnValue(-EINVAL));
    int ret = ubturbo_smap_start(PAGETYPE_4K, nullptr);
    EXPECT_EQ(-EBADF, ret);
}

TEST_F(InterfaceTest, TestSmapInitWithInitAllThreadsFailed)
{
    EnvAtomicSet(&g_status, 0);
    MOCKER(InitLog).stubs().will(returnValue(0));
    MOCKER(CheckPidtype).stubs().will(returnValue(0));
    MOCKER(ProcessManagerInit).stubs().will(returnValue(0));
    MOCKER(DestroyProcessManager).stubs().will(ignoreReturnValue());
    MOCKER(InitTrackingDev).stubs().will(returnValue(0));
    MOCKER(AccessIoctlRemoveAllPid).stubs().will(returnValue(0));
    MOCKER(IsHugeMode).stubs().will(returnValue(false));
    MOCKER(Recover).stubs().will(returnValue(0));
    MOCKER(InitAllThreads).stubs().will(returnValue(-EINVAL));
    int ret = ubturbo_smap_start(PAGETYPE_4K, nullptr);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestSmapInit)
{
    int ret;

    struct ProcessManager manager;
    EnvAtomicSet(&g_status, 0);
    MOCKER(InitLog).stubs().will(returnValue(0));
    MOCKER(CheckPidtype).stubs().will(returnValue(0));
    MOCKER(AccessIoctlRemoveAllPid).stubs().will(returnValue(0));
    MOCKER(ProcessManagerInit).stubs().will(returnValue(0));
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    MOCKER(InitTrackingDev).stubs().will(returnValue(0));
    MOCKER(IsHugeMode).stubs().will(returnValue(false));
    MOCKER(Recover).stubs().will(returnValue(0));
    MOCKER(InitAllThreads).stubs().will(returnValue(0));
    ret = ubturbo_smap_start(PAGETYPE_4K, nullptr);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, EnvAtomicRead(&g_status));
}

extern "C" int EnvAtomicCmpAndSwap(int oldValue, int newValue, EnvAtomic *a);
TEST_F(InterfaceTest, TestSmapStop)
{
    int ret;
    EnvAtomicSet(&g_status, 1);
    MOCKER(EnvMutexLock).stubs().will(ignoreReturnValue());
    MOCKER(DestroyAllThread).stubs().will(ignoreReturnValue());
    MOCKER(EnvMutexUnlock).stubs().will(ignoreReturnValue());
    MOCKER(IsHugeMode).stubs().will(returnValue(false));
    MOCKER(RemoveAllManagedProcess).stubs().will(ignoreReturnValue());
    MOCKER(DeinitTrackingDev).stubs().will(ignoreReturnValue());
    MOCKER(DestroyProcessManager).stubs().will(ignoreReturnValue());
    ret = ubturbo_smap_stop();
    EXPECT_EQ(0, ret);
}

TEST_F(InterfaceTest, TestSmapStopOne)
{
    int ret;
    EnvAtomicSet(&g_status, 1);
    MOCKER(EnvMutexLock).stubs().will(ignoreReturnValue());
    MOCKER(DestroyAllThread).stubs().will(ignoreReturnValue());
    MOCKER(EnvMutexUnlock).stubs().will(ignoreReturnValue());
    MOCKER(IsHugeMode).stubs().will(returnValue(true));
    MOCKER(RemoveAllManagedProcess).stubs().will(ignoreReturnValue());
    MOCKER(DeinitTrackingDev).stubs().will(ignoreReturnValue());
    MOCKER(DestroyProcessManager).stubs().will(ignoreReturnValue());
    ret = ubturbo_smap_stop();
    EXPECT_EQ(0, ret);
}

TEST_F(InterfaceTest, TestIoctlHandlerOne)
{
    int ret;
    const void *msg;
    int pidType = 0;
    const unsigned long *ioctlCommands;

    MOCKER(reinterpret_cast<int (*)(const char *, int)>(open)).stubs().will(returnValue(-1));
    ret = IoctlHandler(msg, pidType, ioctlCommands);
    EXPECT_EQ(-EBADF, ret);
}

TEST_F(InterfaceTest, TestIoctlHandlerTwo)
{
    int ret;
    const void *msg;
    int pidType = 0;
    const unsigned long *ioctlCommands;
    MOCKER(reinterpret_cast<int (*)(const char *, int)>(open)).stubs().will(returnValue(0));
    MOCKER(reinterpret_cast<int (*)(int, unsigned long, void *)>(ioctl)).stubs().will(returnValue(0));
    MOCKER(close).stubs().will(ignoreReturnValue());
    ret = IoctlHandler(msg, pidType, ioctlCommands);
    EXPECT_EQ(0, ret);
}

TEST_F(InterfaceTest, TestIoctlHandlerThree)
{
    int ret;
    const void *msg;
    int pidType = 0;
    const unsigned long *ioctlCommands;
    MOCKER(reinterpret_cast<int (*)(const char *, int)>(open)).stubs().will(returnValue(0));
    MOCKER(reinterpret_cast<int (*)(int, unsigned long, void *)>(ioctl)).stubs().will(returnValue(-1));
    MOCKER(close).stubs().will(ignoreReturnValue());
    ret = IoctlHandler(msg, pidType, ioctlCommands);
    EXPECT_EQ(-EBADF, ret);
}

extern "C" void ubturbo_smap_urgent_migrate_out(uint64_t size);
TEST_F(InterfaceTest, TestSmapUrgentMigrateOut)
{
    int ret;
    uint64_t size = 1;

    EnvAtomicSet(&g_status, 0);
    ubturbo_smap_urgent_migrate_out(size);
    ret = EnvAtomicRead(&g_status);
    EXPECT_EQ(0, ret);
    EnvAtomicSet(&g_status, 1);
}

TEST_F(InterfaceTest, TestNullptrError)
{
    int ret;
    EnvAtomicSet(&g_status, 1);

    ret = ubturbo_smap_migrate_out(nullptr, PAGETYPE_4K);
    EXPECT_EQ(-EINVAL, ret);

    ret = ubturbo_smap_migrate_back(nullptr);
    EXPECT_EQ(-EINVAL, ret);

    ret = ubturbo_smap_remove(nullptr, PAGETYPE_4K);
    EXPECT_EQ(-EINVAL, ret);

    ret = ubturbo_smap_node_enable(nullptr);
    EXPECT_EQ(-EINVAL, ret);

    ret = ubturbo_smap_remote_numa_info_set(nullptr);
    EXPECT_EQ(-EINVAL, ret);
}

extern "C" int SyncProcessToKernel(void);
TEST_F(InterfaceTest, TestSyncProcessToKernel)
{
    ProcessAttr attr = {};
    attr.pid = 123;
    attr.numaAttr.numaNodes = 0b00010001;
    attr.scanTime = 1000;
    attr.scanType = NORMAL_SCAN;
    attr.next = nullptr;
    g_processManager.processes = &attr;

    MOCKER(GetCurrentMaxNrPid).stubs().will(returnValue(1));
    MOCKER(GetProcessManager).stubs().will(returnValue(&g_processManager));
    MOCKER(AccessIoctlAddPid).stubs().will(returnValue(0));

    int ret = SyncProcessToKernel();
    EXPECT_EQ(ret, 0);
}

TEST_F(InterfaceTest, TestSetSmapRemoteNumaInfo)
{
    int ret;
    uint32_t size = 1 * TIB / MIB;
    struct SetRemoteNumaInfoMsg msg;
    struct ProcessManager manager;
    EnvAtomicSet(&g_status, 1);
    MOCKER(IsLocalNidValid).stubs().will(returnValue(true));
    MOCKER(IsRemoteNidValid).stubs().will(returnValue(true));
    msg.size = size;
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    MOCKER(SetRemoteNumaInfo).stubs().will(returnValue(0));
    ret = ubturbo_smap_remote_numa_info_set(&msg);
    EXPECT_EQ(0, ret);
}

TEST_F(InterfaceTest, TestSetSmapRemoteNumaInfoTwo)
{
    int ret;
    uint32_t size = 1 * TIB / MIB;
    struct SetRemoteNumaInfoMsg msg;
    EnvAtomicSet(&g_status, 1);
    EnvAtomicSet(&g_status, 1);
    MOCKER(ubturbo_smap_is_running).stubs().will(returnValue(true));
    MOCKER(IsLocalNidValid).stubs().will(returnValue(false));
    ret = ubturbo_smap_remote_numa_info_set(&msg);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestSetSmapRemoteNumaInfoThree)
{
    int ret;
    uint32_t size = 1 * TIB / MIB;
    struct SetRemoteNumaInfoMsg msg;
    EnvAtomicSet(&g_status, 1);
    MOCKER(ubturbo_smap_is_running).stubs().will(returnValue(false));
    ret = ubturbo_smap_remote_numa_info_set(&msg);
    EXPECT_EQ(-EPERM, ret);
}

TEST_F(InterfaceTest, TestSetSmapRemoteNumaInfoFour)
{
    int ret;
    uint32_t size = 1 * TIB / MIB;
    struct SetRemoteNumaInfoMsg msg;
    EnvAtomicSet(&g_status, 1);
    MOCKER(ubturbo_smap_is_running).stubs().will(returnValue(true));
    MOCKER(IsLocalNidValid).stubs().will(returnValue(true));
    MOCKER(IsRemoteNidValid).stubs().will(returnValue(false));
    ret = ubturbo_smap_remote_numa_info_set(&msg);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestSetSmapRemoteNumaInfoFive)
{
    int ret;
    uint32_t size = 1 * TIB / MIB;
    struct SetRemoteNumaInfoMsg msg;
    msg.size = size;
    EnvAtomicSet(&g_status, 1);
    MOCKER(ubturbo_smap_is_running).stubs().will(returnValue(true));
    MOCKER(IsLocalNidValid).stubs().will(returnValue(true));
    MOCKER(IsRemoteNidValid).stubs().will(returnValue(true));
    MOCKER(SetRemoteNumaInfo).stubs().will(returnValue(-1));
    ret = ubturbo_smap_remote_numa_info_set(&msg);
    EXPECT_EQ(-1, ret);
}

TEST_F(InterfaceTest, TestSetSmapRemoteNumaInfoMsgSizeInvalid)
{
    int ret;
    uint32_t size = 1 * TIB / MIB + 1;
    struct SetRemoteNumaInfoMsg msg;
    msg.size = size;
    EnvAtomicSet(&g_status, 1);
    MOCKER(IsLocalNidValid).stubs().will(returnValue(true));
    MOCKER(IsRemoteNidValid).stubs().will(returnValue(true));

    ret = ubturbo_smap_remote_numa_info_set(&msg);
    EXPECT_EQ(-EINVAL, ret);

    ret = ubturbo_smap_remote_numa_info_set(nullptr);
    EXPECT_EQ(-EINVAL, ret);
}

extern "C" int CheckQueryVMFreqMsgValid(int pid, uint16_t *data,
    uint16_t lengthIn, uint16_t *lengthOut, int dataSource);
extern "C" int QueryVMFreqFromKernel(int pid, uint16_t *data, uint16_t lengthIn, uint16_t *lengthOut);
extern "C" int QueryVMFreqFromUser(int pid, uint16_t *data, uint16_t lengthIn, uint16_t *lengthOut);
extern "C" int ubturbo_smap_freq_query(int pid, uint16_t *data,
    uint32_t lengthIn, uint32_t *lengthOut, int dataSource);
TEST_F(InterfaceTest, TestSmapQueryVmFreqAbnormalOne)
{
    int ret;
    uint32_t lengthOut;
    struct ProcessManager manager;
    EnvAtomicSet(&g_status, 0);
    ret = ubturbo_smap_freq_query(0, nullptr, 0, &lengthOut, 0);
    EXPECT_EQ(-EPERM, ret);

    EnvAtomicSet(&g_status, 1);
    MOCKER(CheckQueryVMFreqMsgValid).stubs().will(returnValue(-EINVAL));
    ret = ubturbo_smap_freq_query(0, nullptr, 0, &lengthOut, 0);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestSmapQueryVmFreqAbnormalTwo)
{
    int ret;
    uint32_t lengthOut;
    uint16_t data[3] = {0};
    struct ProcessManager manager;
    EnvAtomicSet(&g_status, 1);
    MOCKER(CheckQueryVMFreqMsgValid).stubs().will(returnValue(0));
    MOCKER(QueryVMFreqFromUser).stubs().will(returnValue(-EINVAL));
    ret = ubturbo_smap_freq_query(0, data, 0, &lengthOut, 0);
    EXPECT_EQ(-EINVAL, ret);

    MOCKER(QueryVMFreqFromKernel).stubs().will(returnValue(-EINVAL));
    ret = ubturbo_smap_freq_query(0, data, 0, &lengthOut, 1);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestQueryVMFreqFromKernel)
{
    uint16_t data;
    uint16_t lengthOut;

    MOCKER(reinterpret_cast<int (*)(int, unsigned long, void *)>(ioctl)).stubs().will(returnValue(-1));
    int ret = QueryVMFreqFromKernel(0, nullptr, 0, nullptr);
    EXPECT_EQ(-EBADF, ret);

    GlobalMockObject::verify();
    MOCKER(reinterpret_cast<int (*)(int, unsigned long, void *)>(ioctl)).stubs().will(returnValue(0));
    ret = QueryVMFreqFromKernel(1, &data, 1, &lengthOut);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, lengthOut);
}

TEST_F(InterfaceTest, TestQueryVMFreqFromUser)
{
    int ret;
    int pid = 1;
    uint16_t lengthOut;
    uint16_t data[5] = {0};
    size_t dataLen = sizeof(data) / sizeof(uint16_t);
    struct ProcessManager manager;
    ProcessAttr attr = {
        .pid = 1,
        .next = nullptr
    };
    ActcData actcData1[2] = {0};
    ActcData actcData2[4] = {0};
    int len1 = sizeof(actcData1) / sizeof(ActcData);
    int len2 = sizeof(actcData2) / sizeof(ActcData);
    EnvAtomicSet(&g_status, 1);
    attr.scanAttr.actcData[0] = actcData1;
    attr.scanAttr.actcData[1] = actcData2;
    attr.scanAttr.actcLen[0] = len1;
    attr.scanAttr.actcLen[1] = len2;
    manager.processes = &attr;
    manager.tracking.pageSize = PAGESIZE_2M;
    EnvMutexInit(&manager.lock);
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    MOCKER(GetProcessAttrLocked).stubs().will(returnValue(&attr));
    ret = QueryVMFreqFromUser(pid, data, dataLen, &lengthOut);
    EXPECT_EQ(0, ret);
}

TEST_F(InterfaceTest, TestQueryVMFreqFromUserActcLenZero)
{
    int ret;
    int pid = 1;
    uint16_t lengthOut;
    uint16_t data[5] = {0};
    size_t dataLen = sizeof(data) / sizeof(uint16_t);
    struct ProcessManager manager;
    ProcessAttr attr = {
        .pid = 1,
        .next = nullptr
    };
    ActcData *actcData1 = nullptr;
    ActcData *actcData2 = nullptr;
    int len1 = 0;
    int len2 = 0;
    EnvAtomicSet(&g_status, 1);
    attr.scanAttr.actcData[0] = actcData1;
    attr.scanAttr.actcData[1] = actcData2;
    attr.scanAttr.actcLen[0] = len1;
    attr.scanAttr.actcLen[1] = len2;
    manager.processes = &attr;
    manager.tracking.pageSize = PAGESIZE_2M;
    EnvMutexInit(&manager.lock);
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    MOCKER(GetProcessAttrLocked).stubs().will(returnValue(&attr));
    ret = QueryVMFreqFromUser(pid, data, dataLen, &lengthOut);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestQueryVMFreqFromUserNormalOne)
{
    int pid = 1;
    uint16_t lengthOut;
    uint16_t data[5] = {0};
    size_t dataLen = sizeof(data) / sizeof(uint16_t);
    struct ProcessManager manager;
    ProcessAttr attr = {
        .pid = 1,
        .next = nullptr
    };
    EnvAtomicSet(&g_status, 1);
    ActcData actcData1[3] = {
        { .addr = 0, .freq = 0 },
        { .addr = 0, .freq = 1 },
        { .addr = 0, .freq = 2 }
    };
    ActcData actcData2[2] = {
        { .addr = 0, .freq = 3 },
        { .addr = 0, .freq = 4 }
    };
    int len1 = sizeof(actcData1) / sizeof(ActcData);
    int len2 = sizeof(actcData2) / sizeof(ActcData);
    attr.scanAttr.actcData[0] = actcData1;
    attr.scanAttr.actcData[1] = actcData2;
    attr.scanAttr.actcLen[0] = len1;
    attr.scanAttr.actcLen[1] = len2;
    manager.processes = &attr;
    manager.tracking.pageSize = PAGESIZE_2M;
    EnvMutexInit(&manager.lock);
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    MOCKER(GetProcessAttrLocked).stubs().will(returnValue(&attr));
    int ret = QueryVMFreqFromUser(pid, data, dataLen, &lengthOut);
    for (int i = 0; i < dataLen; i++) {
        EXPECT_EQ(i, data[i]);
    }
    EXPECT_EQ(0, ret);
}

TEST_F(InterfaceTest, TestQueryVMFreqFromUserNormalTwo)
{
    int pid = 1;
    uint16_t lengthOut;
    uint16_t data[5] = {0};
    size_t dataLen = sizeof(data) / sizeof(uint16_t);
    struct ProcessManager manager;
    ProcessAttr attr = {
        .pid = 1,
        .next = nullptr
    };
    ActcData actcData1[5] = {
        { .addr = 0, .freq = 0 },
        { .addr = 0, .freq = 1 },
        { .addr = 0, .freq = 2 },
        { .addr = 0, .freq = 3 },
        { .addr = 0, .freq = 4 }
    };
    int len1 = sizeof(actcData1) / sizeof(ActcData);
    EnvAtomicSet(&g_status, 1);
    attr.scanAttr.actcData[0] = actcData1;
    attr.scanAttr.actcData[1] = nullptr;
    attr.scanAttr.actcLen[0] = len1;
    attr.scanAttr.actcLen[1] = 0;
    manager.processes = &attr;
    manager.tracking.pageSize = PAGESIZE_2M;
    EnvMutexInit(&manager.lock);
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    MOCKER(GetProcessAttrLocked).stubs().will(returnValue(&attr));
    int ret = QueryVMFreqFromUser(pid, data, dataLen, &lengthOut);
    for (int i = 0; i < dataLen; i++) {
        EXPECT_EQ(i, data[i]);
    }
    EXPECT_EQ(0, ret);
}

TEST_F(InterfaceTest, TestQueryVMFreqFromUserNormalThree)
{
    int ret;
    int pid = 1;
    uint16_t lengthOut;
    uint16_t data[5] = {0};
    size_t dataLen = sizeof(data) / sizeof(uint16_t);
    struct ProcessManager manager;
    ProcessAttr attr = {
        .pid = 1,
        .next = nullptr
    };
    ActcData actcData1[2] = {
        { .addr = 0, .freq = 0 },
        { .addr = 0, .freq = 1 }
    };
    ActcData actcData2[2] = {
        { .addr = 0, .freq = 2 },
        { .addr = 0, .freq = 3 }
    };
    int len1 = sizeof(actcData1) / sizeof(ActcData);
    int len2 = sizeof(actcData2) / sizeof(ActcData);
    EnvAtomicSet(&g_status, 1);
    attr.scanAttr.actcData[0] = actcData1;
    attr.scanAttr.actcData[1] = actcData2;
    attr.scanAttr.actcLen[0] = len1;
    attr.scanAttr.actcLen[1] = len2;
    manager.processes = &attr;
    manager.tracking.pageSize = PAGESIZE_2M;
    EnvMutexInit(&manager.lock);
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    MOCKER(GetProcessAttrLocked).stubs().will(returnValue(&attr));
    ret = QueryVMFreqFromUser(pid, data, dataLen, &lengthOut);
    EXPECT_EQ(4, lengthOut);
    for (int i = 0; i < lengthOut; i++) {
        EXPECT_EQ(i, data[i]);
    }
    EXPECT_EQ(0, ret);
}

TEST_F(InterfaceTest, DataNull)
{
    int pid = 123;
    uint16_t *data = nullptr;
    uint16_t lengthIn = 5;
    uint16_t *lengthOut = nullptr;
    int dataSource = STATISTIC_DATA_SOURCE;
    struct ProcessManager manager;
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    int result = CheckQueryVMFreqMsgValid(pid, data, lengthIn, lengthOut, dataSource);
    EXPECT_EQ(-EINVAL, result);
}

TEST_F(InterfaceTest, LengthInZero)
{
    int pid = 123;
    uint16_t data[10] = {0};
    uint16_t lengthIn = 0;
    uint16_t lengthOut = 0;
    int dataSource = STATISTIC_DATA_SOURCE;
    struct ProcessManager manager;
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    int result = CheckQueryVMFreqMsgValid(pid, data, lengthIn, &lengthOut, dataSource);
    EXPECT_EQ(-EINVAL, result);
}

TEST_F(InterfaceTest, DataSourceInvalid)
{
    int pid = 123;
    uint16_t data[10] = {0};
    uint16_t lengthIn = 5;
    uint16_t lengthOut = 0;
    int dataSource = MAX_SOURCE;
    struct ProcessManager manager;
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    int result = CheckQueryVMFreqMsgValid(pid, data, lengthIn, &lengthOut, dataSource);
    EXPECT_EQ(-EINVAL, result);
}

TEST_F(InterfaceTest, PidNotManaged)
{
    int pid = 123;
    uint16_t data[10] = {0};
    uint16_t lengthIn = 5;
    uint16_t lengthOut = 0;
    int dataSource = STATISTIC_DATA_SOURCE;
    struct ProcessManager manager;
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    // 模拟GetProcessAttr返回NULL
    MOCKER(GetProcessAttr).stubs().will(returnValue((ProcessAttribute*)nullptr));

    int result = CheckQueryVMFreqMsgValid(pid, data, lengthIn, &lengthOut, dataSource);
    EXPECT_EQ(-EINVAL, result);
}

TEST_F(InterfaceTest, DataSourceMismatch)
{
    int pid = 123;
    uint16_t data[10] = {0};
    uint16_t lengthIn = 5;
    uint16_t lengthOut = 0;
    int dataSource = STATISTIC_DATA_SOURCE;
    struct ProcessManager manager;
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    ProcessAttr attr;
    attr.scanType = NORMAL_SCAN;

    // 模拟GetProcessAttr返回attr
    MOCKER(GetProcessAttr).stubs().will(returnValue(&attr));

    int result = CheckQueryVMFreqMsgValid(pid, data, lengthIn, &lengthOut, dataSource);
    EXPECT_EQ(-EINVAL, result);
}

TEST_F(InterfaceTest, TimeCheckFailed)
{
    int pid = 123;
    uint16_t data[10] = {0};
    uint16_t lengthIn = 5;
    uint16_t lengthOut = 0;
    int dataSource = STATISTIC_DATA_SOURCE;
    struct ProcessManager manager;
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    ProcessAttr attr;
    attr.scanType = STATISTIC_SCAN;
    attr.scanStart = time(nullptr) - 10;  // 设置scanStart为10秒前
    attr.duration = 15;  // 持续时间为15秒

    // 模拟GetProcessAttr返回attr
    MOCKER(GetProcessAttr).stubs().will(returnValue(&attr));

    int result = CheckQueryVMFreqMsgValid(pid, data, lengthIn, &lengthOut, dataSource);
    EXPECT_EQ(-EAGAIN, result);
}

TEST_F(InterfaceTest, TimeCheckSuccess)
{
    int pid = 123;
    uint16_t data[10] = {0};
    uint16_t lengthIn = 5;
    uint16_t lengthOut = 0;
    int dataSource = STATISTIC_DATA_SOURCE;
    struct ProcessManager manager;
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    ProcessAttr attr;
    attr.scanType = STATISTIC_SCAN;
    attr.scanStart = time(nullptr) - 10;  // 设置scanStart为10秒前
    attr.duration = 5;  // 持续时间为15秒

    // 模拟GetProcessAttr返回attr
    MOCKER(GetProcessAttr).stubs().will(returnValue(&attr));

    int result = CheckQueryVMFreqMsgValid(pid, data, lengthIn, &lengthOut, dataSource);
    EXPECT_EQ(0, result);
}

extern "C" bool IsPidArrValid(pid_t *pidArr, int len, bool ignoreUnmanaged);
TEST_F(InterfaceTest, TestSmapEnableProcessMigrateNotRun)
{
    pid_t pidArr[] = { 1 };
    EnvAtomicSet(&g_status, 0);
    int ret = ubturbo_smap_process_migrate_enable(pidArr, 1, DISABLE_PROCESS_MIGRATE, 0);
    EXPECT_EQ(-EPERM, ret);
}

TEST_F(InterfaceTest, TestSmapEnableProcessMigrateInvalidPidType)
{
    pid_t pidArr[] = { 1 };
    EnvAtomicSet(&g_status, 1);
    int ret = ubturbo_smap_process_migrate_enable(pidArr, 1, DISABLE_PROCESS_MIGRATE, 0);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestSmapEnableProcessMigrateInvalidEnable)
{
    pid_t pidArr[] = { 1 };
    EnvAtomicSet(&g_status, 1);
    int ret = ubturbo_smap_process_migrate_enable(pidArr, 1, -1, 0);
    EXPECT_EQ(-EINVAL, ret);

    ret = ubturbo_smap_process_migrate_enable(pidArr, 1, 2, 0);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestSmapEnableProcessMigrateIsPidArrValid)
{
    pid_t pidArr[] = { 1 };
    EnvAtomicSet(&g_status, 1);
    MOCKER(IsPidArrValid).stubs().will(returnValue(false));
    int ret = ubturbo_smap_process_migrate_enable(pidArr, 0, DISABLE_PROCESS_MIGRATE, 0);
    EXPECT_EQ(-EINVAL, ret);
}

extern "C" bool PidIsValid(pid_t pid);
TEST_F(InterfaceTest, TestSmapEnableProcessMigrateNormal)
{
    pid_t pidArr[] = { 1 };
    EnvAtomicSet(&g_status, 1);
    MOCKER(IsPidArrValid).stubs().will(returnValue(true));
    MOCKER(EnableProcessMigrate).stubs().will(returnValue(-ENOENT));
    int ret = ubturbo_smap_process_migrate_enable(pidArr, 1, DISABLE_PROCESS_MIGRATE, 0);
    EXPECT_EQ(-ENOENT, ret);
}

TEST_F(InterfaceTest, TestIsPidArrValid)
{
    pid_t pidArr[] = { 1 };
    bool ret = IsPidArrValid(pidArr, 0, true);
    EXPECT_EQ(false, ret);

    ret = IsPidArrValid(nullptr, 0, true);
    EXPECT_EQ(false, ret);
}

TEST_F(InterfaceTest, TestIsPidArrValidNormal)
{
    pid_t pidArr[] = { 1 };
    ProcessAttr pid1;
    MOCKER(GetProcessAttr).stubs().will(returnValue(&pid1));
    MOCKER(PidIsValid).stubs().will(returnValue(true));
    bool ret = IsPidArrValid(pidArr, 1, true);
    EXPECT_EQ(true, ret);
}

TEST_F(InterfaceTest, TestIsPidArrValidOne)
{
    pid_t pidArr[] = { 1 };
    bool ret = IsPidArrValid(nullptr, 1, true);
    EXPECT_EQ(false, ret);
}

TEST_F(InterfaceTest, TestIsPidArrValidTwo)
{
    pid_t pidArr[] = { 1 };
    ProcessAttr pid1;
    MOCKER(GetProcessAttr).stubs().will(returnValue(&pid1));
    MOCKER(PidIsValid).stubs().will(returnValue(false));
    bool ret = IsPidArrValid(pidArr, 1, true);
    EXPECT_EQ(false, ret);
}

TEST_F(InterfaceTest, TestIsPidArrValidThree)
{
    pid_t pidArr[] = { 1, 1 };
    ProcessAttr pid1;
    MOCKER(GetProcessAttr).stubs().will(returnValue(&pid1));
    MOCKER(PidIsValid).stubs().will(returnValue(true));
    bool ret = IsPidArrValid(pidArr, 2, true);
    EXPECT_EQ(false, ret);
}

extern "C" int CheckMigrateNumaMsg(struct MigrateNumaMsg *msg);
TEST_F(InterfaceTest, CheckMigrateNumaMsgFail)
{
    // msg is nullptr
    struct MigrateNumaMsg msg;
    int ret = CheckMigrateNumaMsg(nullptr);
    EXPECT_EQ(-EINVAL, ret);

    // src = dst
    msg.srcNid = 5;
    msg.destNid = 5;
    ret = CheckMigrateNumaMsg(&msg);
    EXPECT_EQ(-EINVAL, ret);

    // src is invalid 1
    MOCKER(IsNidInNumastat).stubs().will(returnValue(true));
    msg.srcNid = 1;
    g_processManager.nrLocalNuma = 4;
    ret = CheckMigrateNumaMsg(&msg);
    EXPECT_EQ(-EINVAL, ret);

    // src is invalid 2
    msg.srcNid = g_processManager.nrLocalNuma + REMOTE_NUMA_BITS;
    ret = CheckMigrateNumaMsg(&msg);
    EXPECT_EQ(-EINVAL, ret);

    // dst is invalid 1
    msg.srcNid = 5;
    msg.destNid = 1;
    ret = CheckMigrateNumaMsg(&msg);
    EXPECT_EQ(-EINVAL, ret);

    // dst is invalid 2
    msg.destNid = g_processManager.nrLocalNuma + REMOTE_NUMA_BITS;
    ret = CheckMigrateNumaMsg(&msg);
    EXPECT_EQ(-EINVAL, ret);

    // count is invalid 1
    msg.destNid = 6;
    msg.count = 0;
    ret = CheckMigrateNumaMsg(&msg);
    EXPECT_EQ(-EINVAL, ret);

    // count is invalid 2
    msg.count = 1 + MAX_NR_MIGNUMA;
    ret = CheckMigrateNumaMsg(&msg);
    EXPECT_EQ(-EINVAL, ret);

    // src move not allowed
    msg.count = 1;
    MOCKER(IsRemoteNumaMoveAllowed)
        .stubs()
        .will(returnValue(0))
        .then(returnValue(1))
        .then(returnValue(0));
    ret = CheckMigrateNumaMsg(&msg);
    EXPECT_EQ(-EINVAL, ret);

    // dst move not allowed
    ret = CheckMigrateNumaMsg(&msg);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, CheckMigrateNumaMsgSuccess)
{
    struct MigrateNumaMsg msg;
    msg.srcNid = 5;
    msg.destNid = 6;
    msg.count = 1;
    g_processManager.nrLocalNuma = 4;
    MOCKER(IsNidInNumastat).stubs().will(returnValue(true));
    MOCKER(IsRemoteNumaMoveAllowed).stubs().will(returnValue(1));
    int ret = CheckMigrateNumaMsg(&msg);
    EXPECT_EQ(0, ret);
}

TEST_F(InterfaceTest, TestSmapMigrateRemoteNumaNotRun)
{
    struct MigrateNumaMsg msg;
    EnvAtomicSet(&g_status, 0);
    int ret = ubturbo_smap_remote_numa_migrate(&msg);
    EXPECT_EQ(-EPERM, ret);
}

TEST_F(InterfaceTest, TestSmapMigrateRemoteNumaInvalidMsg)
{
    struct MigrateNumaMsg msg;
    EnvAtomicSet(&g_status, 1);
    MOCKER(CheckMigrateNumaMsg).stubs().will(returnValue(-EINVAL));
    int ret = ubturbo_smap_remote_numa_migrate(&msg);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestSmapMigrateRemoteNumaFailMigrateRemoteNuma)
{
    struct MigrateNumaMsg msg;
    EnvAtomicSet(&g_status, 1);
    MOCKER(CheckMigrateNumaMsg).stubs().will(returnValue(0));
    MOCKER(MigrateRemoteNuma).stubs().will(returnValue(-ENOENT));
    int ret = ubturbo_smap_remote_numa_migrate(&msg);
    EXPECT_EQ(-ENOENT, ret);
}

TEST_F(InterfaceTest, TestSmapMigrateRemoteNumaSuccess)
{
    struct MigrateNumaMsg msg;
    EnvAtomicSet(&g_status, 1);
    MOCKER(CheckMigrateNumaMsg).stubs().will(returnValue(0));
    MOCKER(MigrateRemoteNuma).stubs().will(returnValue(0));
    MOCKER(ChangePidRemoteByNuma).stubs().will(returnValue(-ENOMEM)).then(returnValue(0));
    int ret = ubturbo_smap_remote_numa_migrate(&msg);
    EXPECT_EQ(-ENOMEM, ret);

    ret = ubturbo_smap_remote_numa_migrate(&msg);
    EXPECT_EQ(0, ret);
}

TEST_F(InterfaceTest, TestSmapMigrateRemoteNumaNotAllowedTwo)
{
    int srcNid = 4;
    int destNid = 100;
    struct MigrateNumaMsg msg = { .srcNid = srcNid, .destNid = destNid, .count = 100 };
    EnvAtomicSet(&g_status, 1);
    MOCKER(IsPidTypeValid).stubs().will(returnValue(true));
    MOCKER(IsRemoteNidValid).stubs().will(returnValue(true));
    MOCKER(IsRemoteNumaMoveAllowed).stubs().with(eq(srcNid)).will(returnValue(0));
    MOCKER(IsRemoteNumaMoveAllowed).stubs().with(eq(destNid)).will(returnValue(1));
    int ret = ubturbo_smap_remote_numa_migrate(&msg);
    EXPECT_EQ(-EINVAL, ret);

    msg.srcNid = destNid;
    msg.destNid = srcNid;
    ret = ubturbo_smap_remote_numa_migrate(&msg);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestSmapMigrateRemoteNumaNotAllowedThree)
{
    int srcNid = 100;
    int destNid = 99;
    struct MigrateNumaMsg msg = { .srcNid = srcNid, .destNid = destNid, .count = 100 };
    EnvAtomicSet(&g_status, 1);
    MOCKER(IsPidTypeValid).stubs().will(returnValue(true));
    MOCKER(IsRemoteNidValid).stubs().will(returnValue(true));
    MOCKER(IsRemoteNumaMoveAllowed).stubs().with(eq(srcNid)).will(returnValue(0));
    MOCKER(IsRemoteNumaMoveAllowed).stubs().with(eq(destNid)).will(returnValue(1));
    int ret = ubturbo_smap_remote_numa_migrate(&msg);
    EXPECT_EQ(-EINVAL, ret);

    msg.srcNid = destNid;
    msg.destNid = srcNid;
    ret = ubturbo_smap_remote_numa_migrate(&msg);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestSmapMigrateRemoteNumaNormal)
{
    struct MigrateNumaMsg msg = { .srcNid = 4, .destNid = 5, .count = 1 };
    EnvAtomicSet(&g_status, 1);
    MOCKER(IsPidTypeValid).stubs().will(returnValue(true));
    MOCKER(IsRemoteNidValid).stubs().will(returnValue(true));
    MOCKER(IsRemoteNumaMoveAllowed).stubs().will(returnValue(1));
    MOCKER(CheckMigrateNumaMsg).stubs().will(returnValue(0));
    MOCKER(MigrateRemoteNuma).stubs().will(returnValue(-ENOENT));
    MOCKER(ChangePidRemoteByNuma).expects(never());
    int ret = ubturbo_smap_remote_numa_migrate(&msg);
    EXPECT_EQ(-ENOENT, ret);

    GlobalMockObject::verify();
    MOCKER(IsPidTypeValid).stubs().will(returnValue(true));
    MOCKER(IsRemoteNidValid).stubs().will(returnValue(true));
    MOCKER(IsRemoteNumaMoveAllowed).stubs().will(returnValue(1));
    MOCKER(CheckMigrateNumaMsg).stubs().will(returnValue(0));
    MOCKER(MigrateRemoteNuma).stubs().will(returnValue(0));
    MOCKER(ChangePidRemoteByNuma).expects(once()).will(returnValue(0));
    ret = ubturbo_smap_remote_numa_migrate(&msg);
    EXPECT_EQ(0, ret);
}

TEST_F(InterfaceTest, TestSmapMigrateRemoteNumaNormalTwo)
{
    struct MigrateNumaMsg msg = { .srcNid = 4, .destNid = 100, .count = 10 };
    EnvAtomicSet(&g_status, 1);
    MOCKER(CheckMigrateNumaMsg).stubs().will(returnValue(0));
    MOCKER(MigrateRemoteNuma).stubs().will(returnValue(-ENOENT));
    int ret = ubturbo_smap_remote_numa_migrate(&msg);
    EXPECT_EQ(-ENOENT, ret);

    GlobalMockObject::verify();
    MOCKER(CheckMigrateNumaMsg).stubs().will(returnValue(0));
    MOCKER(MigrateRemoteNuma).stubs().will(returnValue(0));
    MOCKER(ChangePidRemoteByNuma).expects(once()).will(returnValue(0));
    ret = ubturbo_smap_remote_numa_migrate(&msg);
    EXPECT_EQ(0, ret);
}

TEST_F(InterfaceTest, TestSmapMigrateRemoteNumaNormalThree)
{
    struct MigrateNumaMsg msg = { .srcNid = 100, .destNid = 5, .count = 10000 };
    EnvAtomicSet(&g_status, 1);
    MOCKER(IsPidTypeValid).stubs().will(returnValue(true));
    MOCKER(IsRemoteNidValid).stubs().will(returnValue(true));
    MOCKER(IsRemoteNumaMoveAllowed).stubs().will(returnValue(1));
    MOCKER(MigrateRemoteNuma).stubs().will(returnValue(-ENOENT));
    MOCKER(ChangePidRemoteByNuma).expects(never());
    int ret = ubturbo_smap_remote_numa_migrate(&msg);
    EXPECT_EQ(-EINVAL, ret);

    GlobalMockObject::verify();
    MOCKER(IsPidTypeValid).stubs().will(returnValue(true));
    MOCKER(IsRemoteNidValid).stubs().will(returnValue(true));
    MOCKER(IsRemoteNumaMoveAllowed).stubs().will(returnValue(1));
    MOCKER(MigrateRemoteNuma).stubs().will(returnValue(0));
    MOCKER(ChangePidRemoteByNuma).stubs().will(returnValue(0));
    ret = ubturbo_smap_remote_numa_migrate(&msg);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestSmapMigratePidRemoteNumaNotRun)
{
    pid_t pidArr[] = { 1 };
    int srcNid = 4;
    int dstNid = 5;
    EnvAtomicSet(&g_status, 0);
    int ret = ubturbo_smap_pid_remote_numa_migrate(pidArr, 1, srcNid, dstNid);
    EXPECT_EQ(-EPERM, ret);
}

TEST_F(InterfaceTest, TestSmapMigratePidRemoteEqSrcDst)
{
    pid_t pidArr[] = { 1 };
    int srcNid = 4;
    int dstNid = 4;
    EnvAtomicSet(&g_status, 1);
    int ret = ubturbo_smap_pid_remote_numa_migrate(pidArr, 1, srcNid, dstNid);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestSmapMigratePidRemoteNumaInvalidSrcNid)
{
    pid_t pidArr[] = { 1 };
    // srcnid less than nrLocalNuma
    int srcNid = 3;
    int dstNid = 5;
    EnvAtomicSet(&g_status, 1);
    g_processManager.nrLocalNuma = 4;
    int ret = ubturbo_smap_pid_remote_numa_migrate(pidArr, 1, srcNid, dstNid);
    EXPECT_EQ(-EINVAL, ret);

    // srcnid greater than nrLocalNuma
    srcNid = g_processManager.nrLocalNuma + REMOTE_NUMA_BITS;
    ret = ubturbo_smap_pid_remote_numa_migrate(pidArr, 1, srcNid, dstNid);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestSmapMigratePidRemoteNumaInvaliDestNid)
{
    pid_t pidArr[] = { 1 };
    int srcNid = 4;
    int dstNid = 3;
    EnvAtomicSet(&g_status, 1);
    g_processManager.nrLocalNuma = 4;
    MOCKER(IsNidInNumastat).stubs().will(returnValue(true));
    // destnid less than nrLocalNuma
    int ret = ubturbo_smap_pid_remote_numa_migrate(pidArr, 1, srcNid, dstNid);
    EXPECT_EQ(-EINVAL, ret);

    // destnid greater than nrLocalNuma
    dstNid = g_processManager.nrLocalNuma + REMOTE_NUMA_BITS;
    ret = ubturbo_smap_pid_remote_numa_migrate(pidArr, 1, srcNid, dstNid);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestSmapMigratePidRemoteNumaInvalidLenPid)
{
    pid_t pidArr[] = { 1 };
    int srcNid = 4;
    int dstNid = 5;
    EnvAtomicSet(&g_status, 1);
    MOCKER(IsNidInNumastat).stubs().will(returnValue(true));
    // len = 0
    int ret = ubturbo_smap_pid_remote_numa_migrate(pidArr, 0, srcNid, dstNid);
    EXPECT_EQ(-EINVAL, ret);

    // len = 1 + MAX_2M_PROCESSES_CNT
    ret = ubturbo_smap_pid_remote_numa_migrate(pidArr, 1 + MAX_2M_PROCESSES_CNT, srcNid, dstNid);
    EXPECT_EQ(-EINVAL, ret);

    // pid = nullptr
    ret = ubturbo_smap_pid_remote_numa_migrate(nullptr, 1, srcNid, dstNid);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestSmapMigratePidRemoteNumaInvalidScan)
{
    pid_t pidArr[] = { 1 };
    int srcNid = 4;
    int dstNid = 5;
    ProcessAttr attr = {};
    attr.pid = 1;
    attr.next = nullptr;
    attr.scanType = HAM_SCAN;
    g_processManager.processes = &attr;

    EnvAtomicSet(&g_status, 1);
    MOCKER(IsNidInNumastat).stubs().will(returnValue(true));
    MOCKER(PidIsValid).stubs().will(returnValue(true));

    int ret = ubturbo_smap_pid_remote_numa_migrate(pidArr, 1, srcNid, dstNid);
    EXPECT_EQ(-EINVAL, ret);
}

extern "C" int IsPidArrRemoteNumaMatch(pid_t *pidArr, int len, int nid);
TEST_F(InterfaceTest, TestSmapMigratePidRemoteNumaIsPidArrRemoteNumaMatchFail)
{
    pid_t pidArr[] = { 1 };
    ProcessAttr attr = {};
    attr.pid = 1;
    attr.scanType = NORMAL_SCAN;
    attr.next = nullptr;
    attr.numaAttr.numaNodes = 0b01100000; // 5 6
    g_processManager.processes = &attr;
    int srcNid = 4;
    int dstNid = 5;
    EnvAtomicSet(&g_status, 1);
    MOCKER(IsNidInNumastat).stubs().will(returnValue(true));
    MOCKER(PidIsValid).stubs().will(returnValue(true));

    // 0b01100000 not eq 4 5
    int ret = ubturbo_smap_pid_remote_numa_migrate(pidArr, 1, srcNid, dstNid);
    EXPECT_EQ(-ENXIO, ret);

    g_processManager.processes = nullptr;
}

TEST_F(InterfaceTest, TestSmapMigratePidRemoteNumaIsPidArrIsPidArrInStateFail)
{
    pid_t pidArr[] = { 1 };
    ProcessAttr attr = {};
    attr.pid = 1;
    attr.scanType = NORMAL_SCAN;
    attr.next = nullptr;
    attr.numaAttr.numaNodes = 0b00110000; // 4 5
    attr.state = PROC_IDLE;
    g_processManager.processes = &attr;
    int srcNid = 4;
    int dstNid = 5;
    EnvAtomicSet(&g_status, 1);
    MOCKER(IsNidInNumastat).stubs().will(returnValue(true));
    MOCKER(PidIsValid).stubs().will(returnValue(true));

    int ret = ubturbo_smap_pid_remote_numa_migrate(pidArr, 1, srcNid, dstNid);
    EXPECT_EQ(-EINVAL, ret);
    g_processManager.processes = nullptr;
}

TEST_F(InterfaceTest, TestSmapMigratePidRemoteNumaIsPidArrMigratePidRemoteNumaFail)
{
    pid_t pidArr[] = { 1 };
    ProcessAttr attr = {};
    attr.pid = 1;
    attr.scanType = NORMAL_SCAN;
    attr.next = nullptr;
    attr.numaAttr.numaNodes = 0b00110000; // 4 5
    attr.state = PROC_MOVE;
    g_processManager.processes = &attr;
    int srcNid = 4;
    int dstNid = 5;
    struct ProcessManager manager;
    EnvMutexInit(&manager.lock);
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    EnvAtomicSet(&g_status, 1);
    MOCKER(IsPidTypeValid).stubs().will(returnValue(true));
    MOCKER(IsRemoteNidValid).stubs().with(eq(dstNid)).will(returnValue(true));
    MOCKER(IsRemoteNidValid).stubs().with(eq(srcNid)).will(returnValue(true));
    MOCKER(IsPidArrValid).stubs().will(returnValue(true));
    MOCKER(IsPidArrRemoteNumaMatch).stubs().will(returnValue(1));

    int ret = ubturbo_smap_pid_remote_numa_migrate(pidArr, 1, srcNid, dstNid);
    EXPECT_EQ(1, ret);
    g_processManager.processes = nullptr;
}

TEST_F(InterfaceTest, TestSmapMigratePidRemoteNumaIsPidArrChangePidRemoteByPid)
{
    pid_t pidArr[] = { 1 };
    ProcessAttr attr = {};
    attr.pid = 1;
    attr.scanType = NORMAL_SCAN;
    attr.next = nullptr;
    attr.numaAttr.numaNodes = 0b00110000; // 4 5
    attr.state = PROC_MOVE;
    g_processManager.processes = &attr;
    int srcNid = 4;
    int dstNid = 5;
    struct ProcessManager manager;
    EnvMutexInit(&manager.lock);
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    EnvAtomicSet(&g_status, 1);
    MOCKER(IsPidTypeValid).stubs().will(returnValue(true));
    MOCKER(IsRemoteNidValid).stubs().with(eq(dstNid)).will(returnValue(true));
    MOCKER(IsRemoteNidValid).stubs().with(eq(srcNid)).will(returnValue(true));
    MOCKER(IsPidArrValid).stubs().will(returnValue(true));
    MOCKER(IsPidArrRemoteNumaMatch).stubs().will(returnValue(0));
    MOCKER(IsPidArrInState).stubs().will(returnValue(1));
    MOCKER(reinterpret_cast<int (*)(int, unsigned long, void *)>(ioctl)).stubs().will(returnValue(0));
    MOCKER(ChangePidRemoteByPid).stubs().will(returnValue(0));
    int ret = ubturbo_smap_pid_remote_numa_migrate(pidArr, 1, srcNid, dstNid);
    EXPECT_EQ(-REMOTE_MIG_FAIL, ret);
    g_processManager.processes = nullptr;
}

extern "C" bool GetAdaptMem(void);
extern "C" bool MigOutIsDone(pid_t pid, bool *isMultiNumaPid);
extern "C" int ubturbo_smap_migrate_out_sync(struct MigrateOutMsg *msg, int pidType, uint64_t maxWaitTime);
TEST_F(InterfaceTest, TestSmapMigrateOutSyncErrMaxWaitTime)
{
    int ret;
    struct MigrateOutMsg msg;
    int pidType = VM_TYPE;
    uint64_t maxWaitTime = 1000;
    ret = ubturbo_smap_migrate_out_sync(&msg, pidType, maxWaitTime);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestSmapMigrateOutSyncErrRunMode)
{
    int ret;
    struct MigrateOutMsg msg;
    int pidType = VM_TYPE;
    uint64_t maxWaitTime = 10000;
    MOCKER(GetRunMode).stubs().will(returnValue(0));
    ret = ubturbo_smap_migrate_out_sync(&msg, pidType, maxWaitTime);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestSmapMigrateOutSyncErrPidType)
{
    int ret;
    struct MigrateOutMsg msg;
    int pidType = 0;
    uint64_t maxWaitTime = 10000;
    MOCKER(GetRunMode).stubs().will(returnValue(1));
    ret = ubturbo_smap_migrate_out_sync(&msg, pidType, maxWaitTime);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestSmapMigrateOutSyncFailSmapMigrateOut)
{
    int ret;
    struct MigrateOutMsg msg;
    int pidType = VM_TYPE;
    uint64_t maxWaitTime = 10000;
    MOCKER(GetRunMode).stubs().will(returnValue(1));
    MOCKER(ubturbo_smap_migrate_out).stubs().will(returnValue(-EPERM));
    ret = ubturbo_smap_migrate_out_sync(&msg, pidType, maxWaitTime);
    EXPECT_EQ(-EPERM, ret);
}

TEST_F(InterfaceTest, TestSmapMigrateOutSyncSuccess)
{
    int ret;
    struct MigrateOutMsg msg;
    msg.count = 1;
    msg.payload[0].pid = 1;
    msg.payload[0].ratio = 25;
    int pidType = VM_TYPE;
    uint64_t maxWaitTime = 10000;

    MOCKER(GetRunMode).stubs().will(returnValue(1));
    MOCKER(ubturbo_smap_migrate_out).stubs().will(returnValue(0));
    MOCKER(MigOutIsDone).stubs().will(returnValue(true));
    ret = ubturbo_smap_migrate_out_sync(&msg, pidType, maxWaitTime);
    EXPECT_EQ(0, ret);
}

TEST_F(InterfaceTest, TestSmapMigrateOutSyncFail)
{
    int ret;
    struct MigrateOutMsg msg;
    msg.count = 1;
    msg.payload[0].pid = 1;
    msg.payload[0].ratio = 25;
    int pidType = VM_TYPE;
    uint64_t maxWaitTime = 10000;

    MOCKER(GetRunMode).stubs().will(returnValue(1));
    MOCKER(ubturbo_smap_migrate_out).stubs().will(returnValue(0));
    MOCKER(MigOutIsDone).stubs().will(returnValue(false));
    ret = ubturbo_smap_migrate_out_sync(&msg, pidType, maxWaitTime);
    EXPECT_EQ(-EBUSY, ret);
}

extern "C" bool ubturbo_smap_is_running(void);
extern "C" int SyncRunMode(RunMode runMode);
extern "C" int ubturbo_smap_run_mode_set(int runMode);
TEST_F(InterfaceTest, TestSetSmapRunModeOne)
{
    int runMode = 1;
    MOCKER(ubturbo_smap_is_running).stubs().will(returnValue(false));
    int ret = ubturbo_smap_run_mode_set(runMode);
    EXPECT_EQ(-EPERM, ret);
}

TEST_F(InterfaceTest, TestSetSmapRunModeTwo)
{
    int runMode = -1;
    MOCKER(ubturbo_smap_is_running).stubs().will(returnValue(true));
    int ret = ubturbo_smap_run_mode_set(runMode);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestSetSmapRunMode4KSuccess)
{
    int runMode = 0;
    g_processManager.tracking.pageSize = PAGESIZE_4K;
    MOCKER(ubturbo_smap_is_running).stubs().will(returnValue(true));
    MOCKER(SyncRunMode).stubs().will(returnValue(0));
    int ret = ubturbo_smap_run_mode_set(runMode);
    EXPECT_EQ(0, ret);
    g_processManager.tracking.pageSize = 0;
}

TEST_F(InterfaceTest, TestSetSmapRunMode4KFail)
{
    int runMode = 0;
    g_processManager.tracking.pageSize = PAGESIZE_4K;
    MOCKER(ubturbo_smap_is_running).stubs().will(returnValue(true));
    MOCKER(SyncRunMode).stubs().will(returnValue(-EINVAL));
    int ret = ubturbo_smap_run_mode_set(runMode);
    EXPECT_EQ(-EBADF, ret);
    g_processManager.tracking.pageSize = 0;
}

TEST_F(InterfaceTest, TestSetSmapRunMode2MSuccess)
{
    int runMode = 1;
    g_processManager.tracking.pageSize = PAGESIZE_2M;
    MOCKER(ubturbo_smap_is_running).stubs().will(returnValue(true));
    MOCKER(SyncRunMode).stubs().will(returnValue(0));
    int ret = ubturbo_smap_run_mode_set(runMode);
    EXPECT_EQ(0, ret);
    g_processManager.tracking.pageSize = 0;
}

TEST_F(InterfaceTest, TestSetSmapRunMode2MFail)
{
    int runMode = 1;
    g_processManager.tracking.pageSize = PAGESIZE_2M;
    MOCKER(ubturbo_smap_is_running).stubs().will(returnValue(true));
    MOCKER(SyncRunMode).stubs().will(returnValue(-EINVAL));
    int ret = ubturbo_smap_run_mode_set(runMode);
    EXPECT_EQ(-EBADF, ret);
    g_processManager.tracking.pageSize = 0;
}

extern "C" int CheckAddProcessTrackingMsg(pid_t *pidArr, uint32_t *scanTime, uint32_t *duration, int len, int scanType);
TEST_F(InterfaceTest, TestCheckAddProcessTrackingMsgNullPtr)
{
    struct ProcessManager manager;
    pid_t pidArr[1] = { 1 };
    uint32_t scanTime[1] = { MIN_SCAN_TIME };
    uint32_t duration[1] = { 1 };
    int ret = CheckAddProcessTrackingMsg(nullptr, nullptr, nullptr, 1, 0);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestCheckAddProcessTrackingMsgLenCheck)
{
    ProcessAttr current;
    pid_t pidArr[1] = { 1 };
    uint32_t scanTime[1] = { MIN_SCAN_TIME + 1 };
    uint32_t duration[1] = { 1 };

    int ret = CheckAddProcessTrackingMsg(pidArr, scanTime, duration, -1, 0);
    EXPECT_EQ(-EINVAL, ret);
    ret = CheckAddProcessTrackingMsg(pidArr, scanTime, duration, MAX_NR_MIGOUT + 1, 0);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestCheckAddProcessTrackingMsgPidInvalid)
{
    struct ProcessManager manager;
    manager.processes = nullptr;
    pid_t pidArr[1] = { 1 };
    uint32_t scanTime[1] = { MIN_SCAN_TIME + 1 };
    uint32_t duration[1] = { 1 };

    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    MOCKER(IsMigOutCountValid).stubs().will(returnValue(true));
    MOCKER(EnvMutexLock).stubs().will(ignoreReturnValue());
    MOCKER(EnvMutexUnlock).stubs().will(ignoreReturnValue());
    MOCKER(PidIsValid).stubs().will(returnValue(false));
    int ret = CheckAddProcessTrackingMsg(pidArr, scanTime, duration, 1, 0);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestCheckAddProcessTrackingMsgStateInvalid)
{
    struct ProcessManager manager;
    ProcessAttr current;
    pid_t pidArr[1] = { 123 };
    uint32_t scanTime[1] = { MIN_SCAN_TIME + 1 };
    uint32_t duration[1] = { 1 };

    manager.processes = &current;
    current.pid = pidArr[0];
    current.state = PROC_BACK;
    current.next = nullptr;
    EnvMutexInit(&manager.lock);
    MOCKER(IsMigOutCountValid).stubs().will(returnValue(true));
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    MOCKER(PidIsValid).stubs().will(returnValue(true));
    int ret = CheckAddProcessTrackingMsg(pidArr, scanTime, duration, 1, 0);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestCheckAddProcessTrackingMsgScanTimeInvalid)
{
    struct ProcessManager manager;
    manager.processes = nullptr;
    pid_t pidArr[1] = { 1 };
    uint32_t scanTime[1] = { MAX_SCAN_TIME + 1 };
    uint32_t duration[1] = { 1 };

    EnvMutexInit(&manager.lock);
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    MOCKER(IsMigOutCountValid).stubs().will(returnValue(true));
    MOCKER(EnvMutexLock).stubs().will(ignoreReturnValue());
    MOCKER(EnvMutexUnlock).stubs().will(ignoreReturnValue());
    MOCKER(PidIsValid).stubs().will(returnValue(true));
    int ret = CheckAddProcessTrackingMsg(pidArr, scanTime, duration, 1, 0);
    EXPECT_EQ(-EINVAL, ret);
    scanTime[0] = MIN_SCAN_TIME - 1;
    ret = CheckAddProcessTrackingMsg(pidArr, scanTime, duration, 1, 0);
    EXPECT_EQ(-EINVAL, ret);
    scanTime[0] = MIN_SCAN_TIME * 2 + 1;
    ret = CheckAddProcessTrackingMsg(pidArr, scanTime, duration, 1, 0);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestCheckAddProcessTrackingMsgDurationInvalid)
{
    struct ProcessManager manager;
    manager.processes = nullptr;
    pid_t pidArr[1] = { 1 };
    uint32_t scanTime[1] = { MAX_SCAN_TIME + 1 };
    uint32_t duration[1] = { 0 };

    EnvMutexInit(&manager.lock);
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    MOCKER(IsMigOutCountValid).stubs().will(returnValue(true));
    MOCKER(EnvMutexLock).stubs().will(ignoreReturnValue());
    MOCKER(EnvMutexUnlock).stubs().will(ignoreReturnValue());
    MOCKER(PidIsValid).stubs().will(returnValue(true));
    int ret = CheckAddProcessTrackingMsg(pidArr, scanTime, duration, 1, 0);
    EXPECT_EQ(-EINVAL, ret);
    duration[0] = MAX_SCAN_DURATION_SEC;
    ret = CheckAddProcessTrackingMsg(pidArr, scanTime, duration, 1, 0);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestCheckAddProcessTrackingMsgScanTypeInvalid)
{
    struct ProcessManager manager;
    manager.processes = nullptr;
    pid_t pidArr[1] = { 1 };
    uint32_t scanTime[1] = { MIN_SCAN_TIME };
    uint32_t duration[1] = { 1 };

    EnvMutexInit(&manager.lock);
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    MOCKER(IsMigOutCountValid).stubs().will(returnValue(true));
    MOCKER(EnvMutexLock).stubs().will(ignoreReturnValue());
    MOCKER(EnvMutexUnlock).stubs().will(ignoreReturnValue());
    MOCKER(PidIsValid).stubs().will(returnValue(true));
    int ret = CheckAddProcessTrackingMsg(pidArr, scanTime, duration, 1, SCAN_TYPE_MAX);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestCheckAddProcessTrackingMsgSuccess)
{
    struct ProcessManager *manager = (struct ProcessManager *)malloc(sizeof(struct ProcessManager));
    ASSERT_NE(nullptr, manager);
    pid_t pidArr[1] = { 1 };
    ProcessAttr current;
    uint32_t scanTime[1] = { MIN_SCAN_TIME };
    uint32_t duration[1] = { 1 };

    manager->processes = &current;
    current.pid = pidArr[0];
    current.state = PROC_MOVE;
    current.next = nullptr;
    EnvMutexInit(&manager->lock);
    MOCKER(GetProcessManager).stubs().will(returnValue(manager));
    MOCKER(IsMigOutCountValid).stubs().will(returnValue(true));
    MOCKER(EnvMutexLock).stubs().will(ignoreReturnValue());
    MOCKER(EnvMutexUnlock).stubs().will(ignoreReturnValue());
    MOCKER(PidIsValid).stubs().will(returnValue(true));
    int ret = CheckAddProcessTrackingMsg(pidArr, scanTime, duration, 1, 0);
    EXPECT_EQ(0, ret);
    free(manager);
}

extern "C" int AddProcessTracking(pid_t *pidArr, uint32_t *scanTime, uint32_t *duration, int len, int scanType);
TEST_F(InterfaceTest, AddProcessTrackingLenEqulasZero)
{
    int ret;
    int len = 0;
    pid_t pid = 1234;
    uint32_t scanTime = 100;
    uint32_t duration = 1;
    int scanType = 0;
    ProcessAttr process = {};
    process.numaAttr.numaNodes = 0b00010001;

    MOCKER(IsHugeMode).stubs().will(returnValue(true));
    MOCKER(AccessIoctlAddPid).stubs().will(returnValue(-1));

    ret = AddProcessTracking(&pid, &scanTime, &duration, len, scanType);
    EXPECT_EQ(-1, ret);
}

TEST_F(InterfaceTest, AddProcessTrackingSetProcessLocalNumaZero)
{
    int ret;
    int len = 1;
    pid_t pid = 1234;
    uint32_t scanTime = 100;
    uint32_t duration = 1;
    int scanType = 0;
    ProcessAttr process = {};
    process.numaAttr.numaNodes = 0b00010001;

    MOCKER(IsHugeMode).stubs().will(returnValue(true));
    MOCKER(AccessIoctlAddPid).stubs().will(returnValue(-1));
    MOCKER(SetProcessLocalNuma).stubs().will(returnValue(-2));
    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));

    ret = AddProcessTracking(&pid, &scanTime, &duration, len, scanType);
    EXPECT_EQ(-2, ret);
}

TEST_F(InterfaceTest, AddProcessTrackingNormalScanNullPtr)
{
    int ret;
    int len = 1;
    pid_t pid = 1234;
    uint32_t scanTime = 100;
    uint32_t duration = 1;
    int scanType = 1;
    ProcessAttr process = {};
    process.numaAttr.numaNodes = 0b00010001;

    MOCKER(IsHugeMode).stubs().will(returnValue(true));
    MOCKER(AccessIoctlAddPid).stubs().will(returnValue(-1));
    MOCKER(SetProcessLocalNuma).stubs().will(returnValue(0));
    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    MOCKER(GetProcessAttrLocked).stubs().will(returnValue((ProcessAttribute*)nullptr));

    ret = AddProcessTracking(&pid, &scanTime, &duration, len, scanType);
    EXPECT_EQ(-22, ret);
}

TEST_F(InterfaceTest, AddProcessTrackingNormalScanInvalidRemoteValid)
{
    int ret;
    int len = 1;
    pid_t pid = 1234;
    uint32_t scanTime = 100;
    uint32_t duration = 1;
    int scanType = 1;
    ProcessAttr process = {};
    process.numaAttr.numaNodes = 0b00010001;

    MOCKER(IsHugeMode).stubs().will(returnValue(true));
    MOCKER(AccessIoctlAddPid).stubs().will(returnValue(-1));
    MOCKER(SetProcessLocalNuma).stubs().will(returnValue(0));
    MOCKER(GetProcessAttrLocked).stubs().will(returnValue(&process));
    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    MOCKER(IsRemoteNidValid).stubs().will(returnValue(false));

    ret = AddProcessTracking(&pid, &scanTime, &duration, len, scanType);
    EXPECT_EQ(-22, ret);
}

TEST_F(InterfaceTest, AddProcessTrackingNormalScanSuccess)
{
    int ret;
    int len = 1;
    pid_t pid = 1234;
    uint32_t scanTime = 100;
    uint32_t duration = 1;
    int scanType = 1;
    ProcessAttr process = {};
    process.numaAttr.numaNodes = 0b00010001;

    MOCKER(IsHugeMode).stubs().will(returnValue(true));
    MOCKER(AccessIoctlAddPid).stubs().will(returnValue(0));
    MOCKER(SetProcessLocalNuma).stubs().will(returnValue(0));
    MOCKER(GetProcessAttrLocked).stubs().will(returnValue(&process));
    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    MOCKER(IsRemoteNidValid).stubs().will(returnValue(true));

    ret = AddProcessTracking(&pid, &scanTime, &duration, len, scanType);
    EXPECT_EQ(0, ret);
}

TEST_F(InterfaceTest, AddProcessTrackingHAMScanNullPtrFailed)
{
    int ret;
    int len = 1;
    pid_t pid = 1234;
    uint32_t scanTime = 100;
    uint32_t duration = 1;
    int scanType = 0;
    ProcessAttr process = {};
    process.numaAttr.numaNodes = 0b00010001;

    MOCKER(IsHugeMode).stubs().will(returnValue(true));
    MOCKER(SetProcessLocalNuma).stubs().will(returnValue(0));
    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    MOCKER(GetProcessAttrLocked).stubs().will(returnValue((ProcessAttribute*)nullptr));
    MOCKER(AccessIoctlAddPid).stubs().will(returnValue(-1));

    ret = AddProcessTracking(&pid, &scanTime, &duration, len, scanType);
    EXPECT_EQ(-1, ret);
}

TEST_F(InterfaceTest, AddProcessTrackingHAMScanFailed)
{
    int ret;
    int len = 1;
    pid_t pid = 1234;
    uint32_t scanTime = 100;
    uint32_t duration = 1;
    int scanType = 0;
    ProcessAttr process = {};
    process.numaAttr.numaNodes = 0b00010001;

    MOCKER(IsHugeMode).stubs().will(returnValue(true));
    MOCKER(SetProcessLocalNuma).stubs().will(returnValue(0));
    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    MOCKER(GetProcessAttrLocked).stubs().will(returnValue(&process));
    MOCKER(AccessIoctlAddPid).stubs().will(returnValue(-1));

    ret = AddProcessTracking(&pid, &scanTime, &duration, len, scanType);
    EXPECT_EQ(-1, ret);
}

TEST_F(InterfaceTest, AddProcessTrackingHAMScanSuccess)
{
    int ret;
    int len = 1;
    pid_t pid = 1234;
    uint32_t scanTime = 100;
    uint32_t duration = 1;
    int scanType = 0;
    ProcessAttr process = {};
    process.numaAttr.numaNodes = 0b00010001;

    MOCKER(IsHugeMode).stubs().will(returnValue(true));
    MOCKER(SetProcessLocalNuma).stubs().will(returnValue(0));
    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    MOCKER(GetProcessAttrLocked).stubs().will(returnValue(&process));
    MOCKER(AccessIoctlAddPid).stubs().will(returnValue(0));

    ret = AddProcessTracking(&pid, &scanTime, &duration, len, scanType);
    EXPECT_EQ(0, ret);
}

TEST_F(InterfaceTest, TestSmapAddProcessTrackingFailed)
{
    pid_t pidArr[] = { 1 };
    uint32_t scanTime[] = { MIN_SCAN_TIME };
    uint32_t duration[] = { 1 };
    EnvAtomicSet(&g_status, 0);
    int ret = ubturbo_smap_process_tracking_add(nullptr, nullptr, nullptr, 1, 0);
    EXPECT_EQ(-EPERM, ret);
    // mutex should be unlocked in any case
    EXPECT_TRUE(EnvMutexIsRelease(&g_processManager.lock));

    EnvAtomicSet(&g_status, 1);
    MOCKER(CheckAddProcessTrackingMsg).stubs()
        .will(returnValue(-EINVAL))
        .then(returnValue(0));
    MOCKER(AddProcessTracking).stubs().will(returnValue(-EINVAL));
    ret = ubturbo_smap_process_tracking_add(pidArr, scanTime, duration, 1, 0);
    EXPECT_EQ(-EINVAL, ret);
    EXPECT_TRUE(EnvMutexIsRelease(&g_processManager.lock));
    ret = ubturbo_smap_process_tracking_add(pidArr, scanTime, duration, 1, 0);
    EXPECT_EQ(-EINVAL, ret);
    EXPECT_TRUE(EnvMutexIsRelease(&g_processManager.lock));
}

TEST_F(InterfaceTest, TestSmapAddProcessTrackingCheckAddManage)
{
    g_processManager.tracking.pageSize = PAGESIZE_2M;
    pid_t pidArr[] = { 1 };
    uint32_t scanTime[] = { MIN_SCAN_TIME };
    uint32_t duration[] = { 1 };
    EnvAtomicSet(&g_status, 1);
    MOCKER(CheckAddProcessTrackingMsg).stubs().will(returnValue(0));
    MOCKER(GetProcessAttrLocked).stubs().will(returnValue((ProcessAttr *)nullptr));
    MOCKER(AccessIoctlAddPid).stubs().will(returnValue(0));
    MOCKER(ProcessAddManage).stubs().will(returnValue(-EINVAL));
    MOCKER(SetProcessLocalNuma).stubs().will(returnValue(0));
    int ret = ubturbo_smap_process_tracking_add(pidArr, scanTime, duration, 1, 0);
    EXPECT_EQ(-EINVAL, ret);
    EXPECT_TRUE(EnvMutexIsRelease(&g_processManager.lock));

    GlobalMockObject::verify();
    MOCKER(CheckAddProcessTrackingMsg).stubs().will(returnValue(0));
    MOCKER(GetProcessAttrLocked).stubs().will(returnValue((ProcessAttr *)nullptr));
    MOCKER(AccessIoctlAddPid).stubs().will(returnValue(0));
    MOCKER(ProcessAddManage).stubs().will(returnValue(0));
    MOCKER(SetProcessLocalNuma).stubs().will(returnValue(0));
    ret = ubturbo_smap_process_tracking_add(pidArr, scanTime, duration, 1, 0);
    EXPECT_EQ(0, ret);
    EXPECT_TRUE(EnvMutexIsRelease(&g_processManager.lock));
}

extern "C" int CheckRemoveProcessTrackingMsg(pid_t *pidArr, int len);
TEST_F(InterfaceTest, TestCheckRemoveProcessTrackingMsgFailed)
{
    // null ptr input
    pid_t pidArr[] = { 1 };
    int ret = CheckRemoveProcessTrackingMsg(nullptr, 1);
    EXPECT_EQ(-EINVAL, ret);

    // invalid len
    ret = CheckRemoveProcessTrackingMsg(pidArr, 0);
    EXPECT_EQ(-EINVAL, ret);
    ret = CheckRemoveProcessTrackingMsg(pidArr, MAX_NR_REMOVE + 1);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestCheckRemoveProcessTrackingMsgCheckProcess)
{
    struct ProcessManager manager;
    ProcessAttr current;
    pid_t *pidArr = (pid_t *)malloc(sizeof(*pidArr));
    ASSERT_NE(nullptr, pidArr);
    pidArr[0] = 1;

    // to-remove pid not exist in ProcessManager
    EnvMutexInit(&manager.lock);
    manager.processes = &current;
    current.pid = pidArr[0] + 1;
    current.state = PROC_IDLE;
    current.next = nullptr;
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    MOCKER(EnvMutexLock).stubs().will(ignoreReturnValue());
    int ret = CheckRemoveProcessTrackingMsg(pidArr, 1);
    EXPECT_EQ(0, ret);

    // pid scanType check
    current.pid = pidArr[0];
    current.scanType = NORMAL_SCAN;
    ret = CheckRemoveProcessTrackingMsg(pidArr, 1);
    EXPECT_EQ(-EINVAL, ret);

    current.scanType = HAM_SCAN;
    ret = CheckRemoveProcessTrackingMsg(pidArr, 1);
    EXPECT_EQ(0, ret);
    free(pidArr);
}

TEST_F(InterfaceTest, TestCheckRemoveProcessTrackingMsgPidStateCheck)
{
    struct ProcessManager manager;
    ProcessAttr current;
    pid_t pidArr[] = { 1 };

    manager.processes = &current;
    current.pid = pidArr[0];
    current.state = PROC_MIGRATE;
    current.scanType = HAM_SCAN;
    current.next = nullptr;
    EnvMutexInit(&manager.lock);
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    int ret = CheckRemoveProcessTrackingMsg(pidArr, 1);
    EXPECT_EQ(-EINVAL, ret);

    current.state = PROC_BACK;
    ret = CheckRemoveProcessTrackingMsg(pidArr, 1);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestSmapRemoveProcessTrackingOne)
{
    pid_t pidArr[] = { 1 };
    struct ProcessManager manager;
    EnvAtomicSet(&g_status, 0);
    int ret = ubturbo_smap_process_tracking_remove(nullptr, 1, 0);
    EXPECT_EQ(-EPERM, ret);

    EnvAtomicSet(&g_status, 1);
    MOCKER(CheckRemoveProcessTrackingMsg).stubs().will(returnValue(-1));
    ret = ubturbo_smap_process_tracking_remove(pidArr, 1, 0);
    EXPECT_EQ(-1, ret);
}

TEST_F(InterfaceTest, TestSmapRemoveProcessTrackingTwo)
{
    pid_t pidArr[] = { 1 };
    struct ProcessManager manager;
    EnvAtomicSet(&g_status, 1);
    EnvMutexInit(&manager.lock);
    MOCKER(CheckRemoveProcessTrackingMsg).stubs().will(returnValue(0));
    MOCKER(AccessIoctlRemovePid).stubs().will(returnValue(-1));
    int ret = ubturbo_smap_process_tracking_remove(pidArr, 1, 0);
    EXPECT_EQ(-1, ret);

    GlobalMockObject::verify();
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    MOCKER(CheckRemoveProcessTrackingMsg).stubs().will(returnValue(0));
    MOCKER(AccessIoctlRemovePid).stubs().will(returnValue(0));
    MOCKER(RemoveManagedProcess).stubs();
    ret = ubturbo_smap_process_tracking_remove(pidArr, 1, 0);
    EXPECT_EQ(0, ret);
}

TEST_F(InterfaceTest, TestSmapQueryProcessConfigInvalidNid)
{
    int ret;
    int nid;
    int inLen;
    int outLen;
    struct OldProcessPayload payload[4] = {};

    inLen = sizeof(payload) / sizeof(payload[0]);
    MOCKER(IsRemoteNidValid).stubs().will(returnValue(false));
    ret = ubturbo_smap_process_config_query(nid, payload, inLen, &outLen);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestSmapQueryProcessConfigNullResult)
{
    int ret;
    int nid;
    int inLen;
    int outLen;
    struct OldProcessPayload payload[4] = {};

    inLen = sizeof(payload) / sizeof(payload[0]);
    MOCKER(IsRemoteNidValid).stubs().will(returnValue(true));
    ret = ubturbo_smap_process_config_query(nid, nullptr, inLen, &outLen);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestSmapQueryProcessConfigInvalidLen)
{
    int ret;
    int nid;
    int inLen;
    int outLen;
    struct OldProcessPayload payload[4] = {};

    inLen = 0;
    MOCKER(IsRemoteNidValid).stubs().will(returnValue(true));
    ret = ubturbo_smap_process_config_query(nid, payload, inLen, &outLen);
    EXPECT_EQ(-EINVAL, ret);

    inLen = sizeof(payload) / sizeof(payload[0]);
    ret = ubturbo_smap_process_config_query(nid, payload, inLen, nullptr);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestSmapQueryProcessConfigNormal)
{
    int ret;
    int inLen;
    int outLen;
    int nrLocalNuma = 2;
    int nid = nrLocalNuma + 1;
    struct OldProcessPayload payload[2] = {};
    struct ProcessManager manager = {
        .processes = nullptr,
        .nrLocalNuma = 4,
    };
    ProcessAttr *attr;
    ProcessAttr *attr1;
    ProcessAttr *attr2;

    attr1 = (ProcessAttr *)calloc(1, sizeof(*attr1));
    attr2 = (ProcessAttr *)calloc(1, sizeof(*attr2));

    ASSERT_NE(nullptr, attr1);
    ASSERT_NE(nullptr, attr2);

    attr1->type = VM_TYPE;
    attr1->pid = 1025;
    attr1->state = PROC_IDLE;
    attr1->scanTime = LIGHT_STABLE_SCAN_CYCLE;
    attr1->scanType = NORMAL_SCAN;
    attr1->numaAttr.numaNodes = 0b00010010;
    attr1->initLocalMemRatio = 25;
    attr2->type = VM_TYPE;
    attr2->pid = 1026;
    attr2->state = PROC_MOVE;
    attr2->scanTime = LIGHT_STABLE_SCAN_CYCLE;
    attr2->scanType = NORMAL_SCAN;
    attr2->numaAttr.numaNodes = 0b00100001;
    attr2->initLocalMemRatio = 25;

    EnvAtomicSet(&g_status, 0);
    ret = ubturbo_smap_process_config_query(5, payload, inLen, &outLen);
    EXPECT_EQ(-EPERM, ret);

    EnvAtomicSet(&g_status, 1);
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    MOCKER(IsRemoteNidValid).stubs().will(returnValue(true));
    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    LinkedListAdd(&manager.processes, &attr1);
    LinkedListAdd(&manager.processes, &attr2);
    EnvMutexInit(&manager.lock);
    inLen = sizeof(payload) / sizeof(payload[0]);
    ret = ubturbo_smap_process_config_query(5, payload, inLen, &outLen);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, outLen);
    EXPECT_EQ(attr2->pid, payload[0].pid);
    EXPECT_EQ(attr2->type, payload[0].type);
    EXPECT_EQ(attr2->state, payload[0].state);
    EXPECT_EQ(attr2->scanTime, payload[0].scanTime);
    EXPECT_EQ(attr2->scanType, payload[0].scanType);
    EXPECT_EQ(attr2->strategyAttr.initRemoteMemRatio[0][0], payload[0].ratio);
    EXPECT_EQ(GetAttrL1(attr2), payload[0].l1Node[0]);
    EXPECT_EQ(GetAttrL2(attr2), payload[0].l2Node[0]);
    EXPECT_EQ(NUMA_NO_NODE, payload[0].l1Node[1]);
    EXPECT_EQ(NUMA_NO_NODE, payload[0].l2Node[1]);

    attr = manager.processes;
    while (attr) {
        LinkedListRemove(&attr, &manager.processes);
        attr = manager.processes;
    }
}

TEST_F(InterfaceTest, TestProcessAddTrackingManageNullMsg)
{
    int ret = ProcessAddTrackingManage(nullptr, 0, VM_TYPE, nullptr);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestSmapQueryProcessConfigNormalTwo)
{
    int ret;
    int inLen;
    int outLen;
    int nrLocalNuma = 2;
    int nid = nrLocalNuma + 1;
    ProcessAttr *attr;
    ProcessAttr *attr1;
    ProcessAttr *attr2;
    struct OldProcessPayload payload[2] = {};
    struct ProcessManager manager = {
        .processes = nullptr,
        .nrLocalNuma = 4,
    };
    attr1 = (ProcessAttr *)calloc(1, sizeof(*attr1));
    attr2 = (ProcessAttr *)calloc(1, sizeof(*attr2));

    attr1->type = PROCESS_TYPE;
    attr1->pid = 1063;
    attr1->state = PROC_IDLE;
    attr1->scanTime = HEAVY_STABLE_SCAN_CYCLE;
    attr1->scanType = NORMAL_SCAN;
    attr1->numaAttr.numaNodes = 0b00010010;
    attr1->initLocalMemRatio = 50;
    attr2->type = PROCESS_TYPE;
    attr2->pid = 1026;
    attr2->state = PROC_MOVE;
    attr2->scanTime = HEAVY_STABLE_SCAN_CYCLE;
    attr2->scanType = NORMAL_SCAN;
    attr2->numaAttr.numaNodes = 0b00100001;
    attr2->initLocalMemRatio = 50;

    EnvAtomicSet(&g_status, 0);
    ret = ubturbo_smap_process_config_query(5, payload, inLen, &outLen);
    EXPECT_EQ(-EPERM, ret);

    EnvAtomicSet(&g_status, 1);
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    MOCKER(IsRemoteNidValid).stubs().will(returnValue(true));
    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    LinkedListAdd(&manager.processes, &attr1);
    LinkedListAdd(&manager.processes, &attr2);
    EnvMutexInit(&manager.lock);
    inLen = sizeof(payload) / sizeof(payload[0]);
    ret = ubturbo_smap_process_config_query(5, payload, inLen, &outLen);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, outLen);
    EXPECT_EQ(attr2->pid, payload[0].pid);
    EXPECT_EQ(attr2->type, payload[0].type);
    EXPECT_EQ(attr2->state, payload[0].state);
    EXPECT_EQ(attr2->scanTime, payload[0].scanTime);
    EXPECT_EQ(attr2->scanType, payload[0].scanType);
    free(attr1);
    free(attr2);
}

TEST_F(InterfaceTest, TestSmapQueryProcessConfigNormalThree)
{
    int ret;
    int inLen;
    int outLen;
    int nrLocalNuma = 2;
    int nid = nrLocalNuma + 1;
    ProcessAttr *attr;
    ProcessAttr *attr1;
    ProcessAttr *attr2;
    struct OldProcessPayload payload[2] = {};
    struct ProcessManager manager = {
        .processes = nullptr,
        .nrLocalNuma = 4,
    };
    attr1 = (ProcessAttr *)calloc(1, sizeof(*attr1));
    attr2 = (ProcessAttr *)calloc(1, sizeof(*attr2));

    attr1->type = PROCESS_TYPE;
    attr1->pid = 2048;
    attr1->state = PROC_IDLE;
    attr1->scanTime = HEAVY_STABLE_SCAN_CYCLE;
    attr1->scanType = NORMAL_SCAN;
    attr1->numaAttr.numaNodes = 0b01000010;
    attr1->initLocalMemRatio = 50;
    attr2->type = PROCESS_TYPE;
    attr2->pid = 3000;
    attr2->state = PROC_MIGRATE;
    attr2->scanTime = LIGHT_STABLE_SCAN_CYCLE;
    attr2->scanType = NORMAL_SCAN;
    attr2->numaAttr.numaNodes = 0b00100001;
    attr2->initLocalMemRatio = 50;

    EnvAtomicSet(&g_status, 0);
    ret = ubturbo_smap_process_config_query(5, payload, inLen, &outLen);
    EXPECT_EQ(-EPERM, ret);

    EnvAtomicSet(&g_status, 1);
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    MOCKER(IsRemoteNidValid).stubs().will(returnValue(true));
    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    LinkedListAdd(&manager.processes, &attr1);
    LinkedListAdd(&manager.processes, &attr2);
    EnvMutexInit(&manager.lock);
    inLen = sizeof(payload) / sizeof(payload[0]);
    ret = ubturbo_smap_process_config_query(5, payload, inLen, &outLen);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, outLen);
    EXPECT_EQ(attr2->pid, payload[0].pid);
    EXPECT_EQ(attr2->type, payload[0].type);
    EXPECT_EQ(attr2->state, payload[0].state);
    EXPECT_EQ(attr2->scanTime, payload[0].scanTime);
    EXPECT_EQ(attr2->scanType, payload[0].scanType);
    free(attr1);
    free(attr2);
}

TEST_F(InterfaceTest, TestSmapQueryRemoteNumaFreqExceptionBranch)
{
    EnvAtomicSet(&g_status, 0);
    int ret = ubturbo_smap_remote_numa_freq_query(nullptr, nullptr, 1);
    EXPECT_EQ(-EPERM, ret);

    EnvAtomicSet(&g_status, 1);
    ret = ubturbo_smap_remote_numa_freq_query(nullptr, nullptr, 1);
    EXPECT_EQ(-EINVAL, ret);

    uint16_t numa;
    uint64_t freq;
    ret = ubturbo_smap_remote_numa_freq_query(&numa, &freq, 19);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InterfaceTest, TestSmapQueryRemoteNumaFreq)
{
    uint64_t freq[2] = { 0 };
    uint16_t numa[2] = { 4, 5 };
    struct ProcessManager manager = {
        .processes = nullptr,
        .nrLocalNuma = 4,
    };
    ProcessAttr attr1;
    ProcessAttr attr2;
    manager.processes = &attr1;
    attr1.next = &attr2;
    attr2.next = nullptr;

    attr1.numaAttr.numaNodes = 0x11;
    attr1.scanAttr.actCount[4].freqSum = 10;
    attr2.numaAttr.numaNodes = 0x11;
    attr2.scanAttr.actCount[4].freqSum = 10;

    EnvMutexInit(&manager.lock);
    MOCKER(IsRemoteNidValid).stubs().will(returnValue(true));
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    int ret = ubturbo_smap_remote_numa_freq_query(numa, freq, 2);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(20, freq[0]);
    EXPECT_EQ(0, freq[1]);
}
