/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: smap5.0 user migration ut code
 */

#include <cstdlib>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include <sys/ioctl.h>

#include "manage/manage.h"
#include "manage/thread.h"
#include "manage/device.h"
#include "advanced-strategy/scene_info.h"
#include "strategy/strategy.h"
#include "strategy/migration.h"

using namespace std;

class MigrationTest : public ::testing::Test {
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

extern "C" struct ProcessManager g_processManager;

TEST_F(MigrationTest, TestAddMigListAddMultiSuccess)
{
    int i;
    int ret;
    struct MigrateMsg mMsg = { .cnt = 0 };
    mMsg.migList = (struct MigList *)malloc(sizeof(struct MigList));
    struct MigList mList = { .nr = 2, .from = 0, .to = 1 };

    mList.addr = (uint64_t *)malloc(sizeof(uint64_t) * mList.nr);
    for (i = 0; i < mList.nr; i++) {
        mList.addr[i] = i;
    }

    MOCKER(IsHugeMode).stubs().will(returnValue(true));
    MOCKER(GetNrFreeHugePagesByNode).stubs().will(returnValue((uint64_t)3));
    ret = AddMigList(&mMsg, &mList);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, mMsg.cnt);
    EXPECT_EQ(2, mMsg.migList[0].nr);

    free(mList.addr);
    free(mMsg.migList[0].addr);
    free(mMsg.migList);
}

TEST_F(MigrationTest, TestAddMigListAddMultiAndMoreThanFreePagesSuccess)
{
    int i;
    int ret;
    struct MigrateMsg mMsg = { .cnt = 0 };
    mMsg.migList = (struct MigList *)malloc(sizeof(struct MigList));
    struct MigList mList = { .nr = 2, .from = 0, .to = 1 };

    mList.addr = (uint64_t *)malloc(sizeof(uint64_t) * mList.nr);
    for (i = 0; i < mList.nr; i++) {
        mList.addr[i] = i;
    }

    MOCKER(IsHugeMode).stubs().will(returnValue(true));
    MOCKER(GetNrFreeHugePagesByNode).stubs().will(returnValue((uint64_t)1));
    ret = AddMigList(&mMsg, &mList);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, mMsg.cnt);
    EXPECT_EQ(1, mMsg.migList[0].nr);

    free(mList.addr);
    free(mMsg.migList[0].addr);
    free(mMsg.migList);
}

TEST_F(MigrationTest, TestAddMigListNoPageToMig)
{
    int i;
    int ret;
    struct MigrateMsg mMsg = { .cnt = 0 };
    mMsg.migList = (struct MigList *)malloc(sizeof(struct MigList));
    struct MigList mList = { .nr = 0, .from = 0, .to = 1 };

    ret = AddMigList(&mMsg, &mList);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0, mMsg.cnt);
}

TEST_F(MigrationTest, TestAddMigListNoFreePages)
{
    int ret;
    uint64_t freePageNum = 0;
    struct MigrateMsg mMsg;
    struct MigList mList;

    MOCKER(IsHugeMode).stubs().will(returnValue((bool)0));
    MOCKER(GetNrFreePagesByNode).stubs().will(returnValue((uint64_t)0));
    ret = AddMigList(&mMsg, &mList);
    EXPECT_EQ(0, ret);
}

extern "C" void FreeMigList(struct MigList mList[MAX_NODES][MAX_NODES]);
TEST_F(MigrationTest, TestFreeMigList)
{
    struct MigList mlist[MAX_NODES][MAX_NODES] = { 0 };
    mlist[0][0].addr = (uint64_t *)malloc(sizeof(uint64_t));
    FreeMigList(mlist);
    EXPECT_EQ(nullptr, mlist[0][0].addr);
}

extern "C" void strategy_InitMigList(struct MigList mList[MAX_NODES][MAX_NODES], int pid);
TEST_F(MigrationTest, TestInitMigList)
{
    struct MigList mlist[MAX_NODES][MAX_NODES] = { 0 };
    printf("MAX_NODES in test = %d\n", MAX_NODES);
    mlist[0][0].addr = (uint64_t *)malloc(sizeof(uint64_t));
    mlist[0][0].nr = 1;
    mlist[0][0].pid = 1234;
    mlist[0][0].from = 100;
    mlist[0][0].to = 200;

    strategy_InitMigList(mlist, 12345);

    EXPECT_EQ(0, mlist[0][0].nr);
    EXPECT_EQ(12345, mlist[0][0].pid);
    EXPECT_EQ(0, mlist[0][0].from);
    EXPECT_EQ(0, mlist[0][0].to);
    EXPECT_EQ(nullptr, mlist[0][0].addr);
}

extern "C" int BuildMigrationMsg(ProcessAttr *process, struct MigrateMsg *mMsg, uint64_t *migratePage);
extern "C" bool IsNodeForbidden(int nid);

TEST_F(MigrationTest, TestBuildMigrationMsgActcDataInvalid)
{
    int ret;
    uint64_t pages;
    ProcessAttr process = {};
    struct MigrateMsg mMsg = {};

    ret = BuildMigrationMsg(&process, &mMsg, &pages);
    EXPECT_EQ(-ENODATA, ret);
}

TEST_F(MigrationTest, TestBuildMigrationMsgRunStrategyFail)
{
    int ret;
    int nid = 4;
    uint64_t pages;
    ProcessAttr process = {};
    process.numaAttr.numaNodes = 0b00010001;
    ActcData actc = {};
    process.scanAttr.actcData[0] = &actc;
    struct MigrateMsg mMsg;

    g_processManager.nrLocalNuma = 4;
    EnvAtomicSet(&g_forbiddenNodes[nid], 0);

    MOCKER(RunStrategy).stubs().will(returnValue(-ENOENT));
    ret = BuildMigrationMsg(&process, &mMsg, &pages);
    EXPECT_EQ(-ENOENT, ret);
}

TEST_F(MigrationTest, TestBuildMigrationMsgNullPtrOfMigratePage)
{
    int ret;
    int nid = 4;
    uint64_t pages;
    ProcessAttr process = {};
    process.numaAttr.numaNodes = 0b00010001;
    ActcData actc = {};
    process.scanAttr.actcData[0] = &actc;
    struct MigrateMsg mMsg;

    g_processManager.nrLocalNuma = 4;
    EnvAtomicSet(&g_forbiddenNodes[nid], 0);

    MOCKER(RunStrategy).stubs().will(returnValue(0));
    ret = BuildMigrationMsg(&process, &mMsg, nullptr);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(MigrationTest, TestBuildMigrationMsgNoPage)
{
    int ret;
    int nid = 4;
    uint64_t pages;
    ProcessAttr process = {};
    process.numaAttr.numaNodes = 0b00010001;
    ActcData actc = {};
    process.scanAttr.actcData[0] = &actc;
    struct MigrateMsg mMsg;

    g_processManager.nrLocalNuma = 4;
    EnvAtomicSet(&g_forbiddenNodes[nid], 0);

    MOCKER(AddMigList).stubs().will(returnValue(0));
    ret = BuildMigrationMsg(&process, &mMsg, &pages);
    EXPECT_EQ(0, ret);
}

TEST_F(MigrationTest, TestBuildMigrationMsgL2NodeForbidden)
{
    int ret;
    int nid = 4;
    uint64_t pages;
    ProcessAttr process = {};
    process.numaAttr.numaNodes = 0b00010001;
    struct MigrateMsg mMsg;
    ActcData actc = {};
    process.scanAttr.actcData[0] = &actc;

    g_processManager.nrLocalNuma = 4;
    EnvAtomicSet(&g_forbiddenNodes[nid], 1);

    ret = BuildMigrationMsg(&process, &mMsg, &pages);
    EXPECT_EQ(-EPERM, ret);
}

extern "C" uint64_t CalcMigrateNumByFreq(ProcessAttr *process);
extern "C" int RunStrategyStub(ProcessAttr *process, struct MigList mlist[MAX_NODES][MAX_NODES], size_t mlistSize);
TEST_F(MigrationTest, TestBuildMigrationMsgSuccess)
{
    int ret;
    uint64_t pages = 0;
    ProcessAttr process = {};
    process.pid = 1234;
    process.strategyAttr.nrMigratePages[0][0] = 100;
    process.numaAttr.numaNodes = 0b00010001;
    struct MigrateMsg mMsg = {};
    ActcData actc = {};
    process.scanAttr.actcData[0] = &actc;
    EnvAtomicSet(&g_forbiddenNodes[4], 0);

    MOCKER(CheckActcDataValid).stubs().will(returnValue(0));
    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    MOCKER(RunStrategy).stubs().will(invoke(RunStrategyStub));
    MOCKER(AddMigList).stubs().will(returnValue(0));

    ret = BuildMigrationMsg(&process, &mMsg, &pages);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(484, pages);
}

extern "C" int DoMigration(struct MigrateMsg *mMsg, struct ProcessManager *manager);
TEST_F(MigrationTest, TestDoMigration)
{
    struct MigList migList = {
        .nr = 0
    };
    struct MigrateMsg mMsg = {
        .cnt = 1,
        .migList = &migList
    };

    struct ProcessManager manager;
    int ret = DoMigration(&mMsg, &manager);
    EXPECT_EQ(-1, ret);
}

TEST_F(MigrationTest, TestDoMigrationInitialized)
{
    struct MigList migList = {
        .nr = 2
    };

    migList.addr = (uint64_t *)malloc(sizeof(uint64_t) * migList.nr);

    for (int i = 0; i < migList.nr; ++i) {
        migList.addr[i] = 0x1000 + i * 0x1000;
    }

    struct MigrateMsg mMsg = {
        .cnt = 1,
        .migList = &migList
    };

    struct ProcessManager manager;
    int ret = DoMigration(&mMsg, &manager);
    EXPECT_EQ(-1, ret);
}

TEST_F(MigrationTest, DoMigrationMinusCnt)
{
    struct MigList migList = {
        .nr = 0
    };
    struct MigrateMsg mMsg = {
        .cnt = -1,
        .migList = &migList
    };

    struct ProcessManager manager;
    int ret = DoMigration(&mMsg, &manager);
    EXPECT_EQ(-ENOMEM, ret);
}

extern "C" int InitMigrateMsg(struct MigrateMsg *mMsg, struct ProcessManager *manager);
TEST_F(MigrationTest, TestInitMigrateMsg)
{
    struct MigrateMsg mMsg = { .cnt = 1 };
    struct ProcessManager manager = { .nr = { 0, 1 }, .tracking = { .pageSize = 4096 } };
    MOCKER(GetPidType).stubs().will(returnValue(VM_TYPE));
    ASSERT_EQ(nullptr, mMsg.migList);
    int ret = InitMigrateMsg(&mMsg, &manager);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0, mMsg.cnt);
    EXPECT_NE(nullptr, mMsg.migList);
    free(mMsg.migList);
}

extern "C" int UpdateRemoteRamInfo(struct ProcessManager *manager);
TEST_F(MigrationTest, TestUpdateRemoteRamInfoGetRemoteNumaFailed)
{
    int ret;
    struct ProcessManager manager;
    manager.iomMsg.iomemSegArray = NULL;
    manager.nrLocalNuma = 4;
    EnvMutexInit(&manager.lock);
    MOCKER(GetIomemAddresses).stubs().will(returnValue(-1));
    ret = UpdateRemoteRamInfo(&manager);
    EXPECT_EQ(-1, ret);
}

TEST_F(MigrationTest, TestUpdateRemoteRamInfoGetActcLenFailed)
{
    int ret;
    struct ProcessManager manager;
    manager.nrLocalNuma = 4;
    EnvMutexInit(&manager.lock);
    MOCKER(GetIomemAddresses).stubs().will(returnValue(0));
    MOCKER(GetNodeActcLenIomem).stubs().will(returnValue(-1));
    ret = UpdateRemoteRamInfo(&manager);
    EXPECT_EQ(-1, ret);
}

TEST_F(MigrationTest, TestUpdateRemoteRamInfoSuccess)
{
    int ret;
    g_processManager.nrLocalNuma = 4;
    g_processManager.iomMsg.iomemSegArray = (IomemSeg *)malloc(sizeof(IomemSeg) * 1);

    for (int i = 0; i < 16; i++) {
        g_processManager.nodeActcLen[i] = 100 + (i * 100);
    }

    MOCKER(GetIomemAddresses).stubs().will(returnValue(0));
    MOCKER(GetNodeActcLenIomem).stubs().will(returnValue(0));
    ret = UpdateRemoteRamInfo(&g_processManager);
    EXPECT_EQ(0, ret);
}

extern "C" int PerformMigrationPreparation(struct ProcessManager *manager);
extern "C" int GetRamIsChange(struct ProcessManager *manager, int *change);
extern "C" int BuildAllPidData(void);
extern "C" int CleanStrategyAttribute(struct ProcessManager *manager);
TEST_F(MigrationTest, TestPerformMigrationPreparationOK)
{
    int ret;
    ThreadCtx *ctx = (ThreadCtx *)malloc(sizeof(ThreadCtx));
    ctx->processManager = (ProcessManager *)malloc(sizeof(ProcessManager));
    ctx->processManager->processes = (ProcessAttr *)malloc(sizeof(ProcessAttr));
    ctx->processManager->processes->next = NULL;

    MOCKER(GetRamIsChange).stubs().will(returnValue(0));
    MOCKER(BuildAllPidData).stubs().will(returnValue(0));
    MOCKER(CleanStrategyAttribute).stubs().will(returnValue(0));
    ret = PerformMigrationPreparation(ctx->processManager);
    EXPECT_EQ(0, ret);

    free(ctx->processManager->processes);
    free(ctx->processManager);
    free(ctx);
}

TEST_F(MigrationTest, TestPerformMigrationPreparationEmptyProcesses)
{
    int ret;
    int change = 0;
    struct ProcessManager manager = { .processes = nullptr };

    MOCKER(GetRamIsChange).stubs().with(any(), outBoundP(&change, sizeof(change))).will(returnValue(0));
    MOCKER(CleanStrategyAttribute).stubs().will(returnValue(0));
    MOCKER(BuildAllPidData).stubs().will(returnValue(0));
    ret = PerformMigrationPreparation(&manager);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(MigrationTest, TestPerformMigrationPreparationBuildError)
{
    int ret;
    int change = 0;
    ProcessAttr process;
    struct ProcessManager manager = { .processes = &process };

    MOCKER(GetRamIsChange).stubs().with(any(), outBoundP(&change, sizeof(change))).will(returnValue(0));
    MOCKER(CleanStrategyAttribute).stubs().will(returnValue(0));
    MOCKER(BuildAllPidData).stubs().will(returnValue(-ENOMEM));
    ret = PerformMigrationPreparation(&manager);
    EXPECT_EQ(-ENOMEM, ret);
}

TEST_F(MigrationTest, TestPerformMigrationPreparationGetChangedError)
{
    int ret;
    struct ProcessManager manager;

    MOCKER(GetRamIsChange).stubs().will(returnValue(-1));
    ret = PerformMigrationPreparation(&manager);
    EXPECT_EQ(-1, ret);
}

TEST_F(MigrationTest, TestPerformMigrationPreparationRamChanged)
{
    int ret;
    int change = 1;
    struct ProcessManager manager;

    MOCKER(GetRamIsChange).stubs().with(any(), outBoundP(&change, sizeof(change))).will(returnValue(0));
    ret = PerformMigrationPreparation(&manager);
    EXPECT_EQ(-EBUSY, ret);
}

extern "C" int ScanMigrateWork(ThreadCtx *ctx);
extern "C" int PerformMigration(struct ProcessManager *manager);
extern "C" int HandleScene(ThreadCtx *ctx);
extern "C" void UpdateScene(struct ProcessManager *manager);
TEST_F(MigrationTest, TestScanMigrateWork)
{
    int ret;
    ProcessAttr process = { .pid = 1025 };
    struct ProcessManager manager = { .processes = &process };
    ThreadCtx ctx = { .processManager = &manager };

    MOCKER(DisableTracking).stubs().will(returnValue(0));
    MOCKER(CheckAndRemoveInvalidProcess).stubs();
    MOCKER(PerformMigrationPreparation).stubs().will(returnValue(0));
    MOCKER(UpdateScene).stubs();
    MOCKER(HandleScene).stubs().will(returnValue(0));
    MOCKER(EnvMutexLock).stubs().will(ignoreReturnValue());
    MOCKER(EnvMutexUnlock).stubs().will(ignoreReturnValue());
    MOCKER(PerformMigration).stubs().will(returnValue(0));
    ret = ScanMigrateWork(&ctx);
    EXPECT_EQ(0, ret);
}

TEST_F(MigrationTest, TestScanMigrateWorkOne)
{
    int ret;
    ProcessAttr process = { .pid = 1025 };
    struct ProcessManager manager = { .processes = &process };
    ThreadCtx ctx = { .processManager = &manager };

    MOCKER(DisableTracking).stubs().will(returnValue(-1));
    ret = ScanMigrateWork(&ctx);
    EXPECT_EQ(-1, ret);
}

TEST_F(MigrationTest, TestScanMigrateWorkTwo)
{
    int ret;
    ProcessAttr process = { .pid = 1025 };
    struct ProcessManager manager = { .processes = &process };
    ThreadCtx ctx = { .processManager = &manager };

    MOCKER(DisableTracking).stubs().will(returnValue(0));
    MOCKER(CheckAndRemoveInvalidProcess).stubs();
    MOCKER(PerformMigrationPreparation).stubs().will(returnValue(-1));
    ret = ScanMigrateWork(&ctx);
    EXPECT_EQ(-1, ret);
}

extern "C" int SetMigrateThreadNum(struct MigrateMsg *mMsg, uint64_t migratePages, bool isForcedSingleThread);
TEST_F(MigrationTest, TestSetMigrateThreadNum)
{
    int ret;
    struct MigrateMsg mMsg = { 0 };
    uint64_t migratePages = LESS_MIG_OUT_2M_PAGE_THRE + 1;
    mMsg.mulMig.pageSize = PAGESIZE_2M;
    ret = SetMigrateThreadNum(&mMsg, migratePages, 0);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(true, mMsg.mulMig.isMulThread);
    EXPECT_EQ(LESS_THREAD_MIG_OUT, mMsg.mulMig.nrThread);

    migratePages = MORE_MIG_OUT_2M_PAGE_THRE + 1;
    ret = SetMigrateThreadNum(&mMsg, migratePages, 0);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(MORE_THREAD_MIG_OUT, mMsg.mulMig.nrThread);
}

TEST_F(MigrationTest, TestSetMigrateThreadNumTwo)
{
    int ret;
    struct MigrateMsg mMsg = { 0 };
    uint64_t migratePages = LESS_MIG_OUT_2M_PAGE_THRE + 1;
    ret = SetMigrateThreadNum(nullptr, migratePages, 0);
    EXPECT_EQ(-EINVAL, ret);

    mMsg.mulMig.pageSize = PAGESIZE_4K;
    ret = SetMigrateThreadNum(&mMsg, migratePages, 0);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(false, mMsg.mulMig.isMulThread);
    EXPECT_EQ(SIG_THREAD_MIG_OUT, mMsg.mulMig.nrThread);
}

TEST_F(MigrationTest, TestSetMigrateThreadNumThree)
{
    int ret;
    struct MigrateMsg mMsg = { 0 };
    uint64_t migratePages = LESS_MIG_OUT_2M_PAGE_THRE + 1;

    ret = SetMigrateThreadNum(&mMsg, migratePages, 1);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(false, mMsg.mulMig.isMulThread);
    EXPECT_EQ(1, mMsg.mulMig.nrThread);
}

TEST_F(MigrationTest, TestSetMigrateThreadNumPage2M)
{
    int ret;
    struct MigrateMsg mMsg = { 0 };
    mMsg.mulMig.pageSize = PAGESIZE_2M;
    uint64_t migratePages = LESS_MIG_OUT_2M_PAGE_THRE + 1;

    ret = SetMigrateThreadNum(&mMsg, migratePages, 1);
    EXPECT_EQ(0, ret);
}

extern "C" long CalcDurationUs(struct timeval start, struct timeval end);
TEST_F(MigrationTest, TestCalcDurationUs)
{
    struct timeval start = { 0 };
    struct timeval end = { 0 };
    start.tv_sec = 1;
    start.tv_usec = 100;
    end.tv_sec = 2;
    end.tv_usec = 100;

    long duration = CalcDurationUs(start, end);
    EXPECT_EQ(1000000, duration);
}

extern "C" void CalProcessNuma(StrategyAttribute *strategyAttr);
TEST_F(MigrationTest, TestCalProcessNuma)
{
    ProcessAttr attr = {};
    attr.strategyAttr.nrPagesPerLocalNuma[0] = 100;
    attr.strategyAttr.allocRemoteNrPages[0][0] = 200;
    attr.strategyAttr.l3RemoteMemRatio[0][0] = 50;

    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    CalProcessNuma(&attr.strategyAttr);
    EXPECT_EQ(150, attr.strategyAttr.nrMigratePages[4][0]);
}

typedef struct {
    int numaId;
    int amount;
} NumaMemReduce;

extern "C" int CompareMigIn(const void *a, const void *b);
TEST_F(MigrationTest, TestCompareMigIn)
{
    NumaMemReduce* a = (NumaMemReduce *)malloc(sizeof(NumaMemReduce));
    NumaMemReduce* b = (NumaMemReduce *)malloc(sizeof(NumaMemReduce));
    a->amount = 100;
    b->amount = 200;

    int ret = CompareMigIn(a, b);
    EXPECT_EQ(-100, ret);
    free(a);
    free(b);
}

extern "C" int CompareMigOut(const void *b, const void *a);
TEST_F(MigrationTest, TestCompareMigOut)
{
    NumaMemReduce* a = (NumaMemReduce *)malloc(sizeof(NumaMemReduce));
    NumaMemReduce* b = (NumaMemReduce *)malloc(sizeof(NumaMemReduce));
    a->amount = 200;
    b->amount = 100;

    int ret = CompareMigIn(a, b);
    EXPECT_EQ(100, ret);
    free(a);
    free(b);
}

extern "C" void NumaSwapReduce(StrategyAttribute *strategyAttr, int32_t *numaMemSwap);
TEST_F(MigrationTest, TestNumaSwapReduce)
{
    ProcessAttr attr = {};
    int32_t* numaMemSwap = (int32_t*)calloc((LOCAL_NUMA_BITS + REMOTE_NUMA_BITS), sizeof(int32_t));
    numaMemSwap[0] = 100;
    numaMemSwap[1] = -200;

    MOCKER(GetNrLocalNuma).stubs().will(returnValue(LOCAL_NUMA_BITS));
    NumaSwapReduce(&attr.strategyAttr, numaMemSwap);
    EXPECT_EQ(100, attr.strategyAttr.nrMigratePages[1][0]);
    free(numaMemSwap);
}

extern "C" void NumaSwapMemPool(ProcessAttr *current);
TEST_F(MigrationTest, TestNumaSwapMemPool)
{
    int l2Node = 4;
    ProcessAttr attr = {};
    attr.strategyAttr.allocRemoteNrPages[0][0] = 100;
    attr.scanAttr.actcLen[l2Node] = 5;
    attr.strategyAttr.memSize[0][0] = 4096; // 2 hugepage
    MOCKER(IsHugeMode).stubs().will(returnValue(true));
    MOCKER(GetNrLocalNuma).stubs().will(returnValue(l2Node));

    // migNum > 0 case
    NumaSwapMemPool(&attr);
    EXPECT_EQ(5, attr.strategyAttr.nrMigratePages[l2Node][0]);

    attr.scanAttr.actcLen[l2Node] = 100;
    NumaSwapMemPool(&attr);
    EXPECT_EQ(98, attr.strategyAttr.nrMigratePages[l2Node][0]);

    // migNum < 0 case
    attr.scanAttr.actcLen[0] = 100;
    attr.strategyAttr.memSize[0][0] = 307200; // 150 hugepage
    NumaSwapMemPool(&attr);
    EXPECT_EQ(50, attr.strategyAttr.nrMigratePages[0][l2Node]);

    attr.scanAttr.actcLen[0] = 10;
    NumaSwapMemPool(&attr);
    EXPECT_EQ(10, attr.strategyAttr.nrMigratePages[0][l2Node]);
}

extern "C" void NumaMigReduceDeal(ProcessAttr *current);
TEST_F(MigrationTest, TestNumaMigReduceDeal)
{
    ProcessAttr attr = {};
    attr.strategyAttr.nrPagesPerLocalNuma[0] = 100;
    attr.strategyAttr.allocRemoteNrPages[0][0] = 200;
    attr.strategyAttr.l3RemoteMemRatio[0][0] = 50;

    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    MOCKER(GetRunMode).stubs().will(returnValue(0));
    NumaMigReduceDeal(&attr);
    EXPECT_EQ(150, attr.strategyAttr.nrMigratePages[4][0]);
}

extern "C" int PreMigration(struct ProcessManager *manager, struct MigrateMsg *mMsg,
    uint64_t *migratePages);
TEST_F(MigrationTest, TestPerformMigration)
{
    int ret;
    ProcessAttr process = { .pid = 1025 };
    struct ProcessManager manager = { .processes = &process };

    MOCKER(PreMigration).stubs().will(returnValue(-ENOMEM));
    ret = PerformMigration(&manager);
    EXPECT_EQ(-ENOMEM, ret);
}

extern "C" void PrintMigSpeed(struct ProcessManager *manager, uint64_t nr, struct timeval start, struct timeval end);
extern "C" void PostMigration(struct ProcessManager *manager, struct MigrateMsg *mMsg);
TEST_F(MigrationTest, TestPerformMigrationSecond)
{
    int ret;
    ProcessAttr process = { .pid = 1025 };
    struct ProcessManager manager = { .processes = &process };

    MOCKER(PreMigration).stubs().will(returnValue(0));
    MOCKER(DoMigration).stubs().will(returnValue(0));
    MOCKER(PrintMigSpeed).stubs().will(returnValue(0));
    MOCKER(PostMigration).stubs().will(ignoreReturnValue());
    ret = PerformMigration(&manager);
    EXPECT_EQ(0, ret);
}

extern "C" int AccessIoctlAddPid(int len, struct AccessAddPidPayload *payload);
extern "C" int UpdateScanTime(ProcessAttr *process);
TEST_F(MigrationTest, TestUpdateScanTime)
{
    int ret;
    ProcessAttr process;
    process.pid = 123;
    process.numaAttr.numaNodes = 0b00010001;
    process.scanType = NORMAL_SCAN;
    process.sceneInfo.cycles.scanCycle = 4;

    MOCKER(AccessIoctlAddPid).stubs().will(returnValue(0));
    ret = UpdateScanTime(&process);
    EXPECT_EQ(0, ret);
}

extern "C" void UpdateScene(struct ProcessManager *manager);
TEST_F(MigrationTest, TestUpdateScene)
{
    struct ProcessManager manager = { 0 };
    ProcessAttr current = {};
    manager.processes = &current;
    current.type = VM_TYPE;
    current.scanType = NORMAL_SCAN;
    current.sceneInfo.currScene = LIGHT_STABLE_SCENE;
    current.sceneInfo.lastScene = HEAVY_STABLE_SCENE;
    UpdateScene(&manager);
    EXPECT_EQ(current.sceneInfo.pageInfoIndex, 1);
    EXPECT_EQ(current.sceneInfo.pageInfo[1].nrHot, 0);
    EXPECT_EQ(current.sceneInfo.pageInfo[1].nrL1Guarantee, 0);
    EXPECT_EQ(current.sceneInfo.currScene, HEAVY_STABLE_SCENE);
}

extern "C" int HandleScene(ThreadCtx *ctx);
TEST_F(MigrationTest, TestHandleScene)
{
    struct ProcessManager manager = { 0 };
    ProcessAttr current = {};
    ThreadCtx ctx = {};
    manager.processes = &current;
    ctx.processManager = &manager;
    current.next = nullptr;
    current.sceneInfo.currScene = UNSTABLE_SCENE;

    MOCKER(EnvMutexLock).stubs().will(ignoreReturnValue());
    MOCKER(EnvMutexUnlock).stubs().will(ignoreReturnValue());
    MOCKER(UpdateScanTime).stubs().will(returnValue(0));

    int ret = HandleScene(&ctx);
    EXPECT_EQ(0, ret);
}

extern "C" uint32_t GetScanPeriodConfig(void);
extern "C" void UpdateAllProcessScanTime(ThreadCtx *ctx);
TEST_F(MigrationTest, TestUpdateAllProcessScanTime)
{
    struct ProcessManager manager = { 0 };
    ProcessAttr current = {};
    ThreadCtx ctx = {};
    manager.processes = &current;
    ctx.processManager = &manager;
    current.next = nullptr;
    current.sceneInfo.currScene = UNSTABLE_SCENE;

    MOCKER(EnvMutexLock).stubs().will(ignoreReturnValue());
    MOCKER(EnvMutexUnlock).stubs().will(ignoreReturnValue());
    MOCKER(UpdateScanTime).stubs().will(returnValue(0));
    MOCKER(GetScanPeriodConfig).stubs().will(returnValue(0));

    UpdateAllProcessScanTime(&ctx);
}

extern "C" bool GetFileConfSwitchConfig(void);
extern "C" bool GetMigratePeriodChanged(void);
extern "C" uint32_t GetMigratePeriodConfig(void);
extern "C" bool GetScanPeriodChanged(void);
extern "C" void UpdatePeriodFromConfig(ThreadCtx *ctx);
TEST_F(MigrationTest, TestUodatePeriodFromConfig)
{
    ThreadCtx ctx = {};
    ctx.period = 500;
    MOCKER(GetFileConfSwitchConfig).stubs().will(returnValue(true));
    MOCKER(GetScanPeriodChanged).stubs().will(returnValue(false));
    MOCKER(GetMigratePeriodChanged).stubs().will(returnValue(true));
    MOCKER(GetMigratePeriodConfig).stubs().will(returnValue(1000));

    UpdatePeriodFromConfig(&ctx);
    EXPECT_EQ(1000, ctx.period);
}

extern "C" void UpdateMigResult(struct MigrateMsg *mMsg, struct ProcessManager *manager);
TEST_F(MigrationTest, TestUpdateMigResultLocalToRemote)
{
    ProcessAttr attr = {};
    attr.next = NULL;
    attr.pid = 123;
    attr.numaAttr.numaNodes = 0b00010001;
    attr.strategyAttr.remoteNrPagesAfterMigrate[0][0] = 100;

    struct MigrateMsg mMsg = {};
    mMsg.cnt = 1;
    mMsg.migList = (struct MigList*)malloc(sizeof(struct MigList) * 1);
    mMsg.migList[0].pid = 123;
    mMsg.migList[0].from = 0;
    mMsg.migList[0].to = 4;
    mMsg.migList[0].nr = 100;
    mMsg.migList[0].failedMigNr = 30;
    mMsg.migList[0].successToUser = true;

    ProcessManager manager = {};
    manager.nrLocalNuma = 4;
    manager.processes = &attr;
    MOCKER(GetProcessAttrLocked).stubs().will(returnValue(&attr));
    UpdateMigResult(&mMsg, &manager);
    EXPECT_EQ(170, attr.strategyAttr.remoteNrPagesAfterMigrate[0][0]);
}

TEST_F(MigrationTest, TestUpdateMigResultRemoteToLocalUnexpectedMigCount)
{
    ProcessAttr attr = {};
    attr.next = NULL;
    attr.pid = 123;
    attr.numaAttr.numaNodes = 0b00010001;
    attr.strategyAttr.remoteNrPagesAfterMigrate[0][0] = 100;

    struct MigrateMsg mMsg = {};
    mMsg.cnt = 1;
    mMsg.migList = (struct MigList*)malloc(sizeof(struct MigList) * 1);
    mMsg.migList[0].pid = 123;
    mMsg.migList[0].from = 4;
    mMsg.migList[0].to = 0;
    mMsg.migList[0].nr = 1000;
    mMsg.migList[0].failedMigNr = 30;
    mMsg.migList[0].successToUser = true;

    ProcessManager manager = {};
    manager.nrLocalNuma = 4;
    manager.processes = &attr;
    MOCKER(GetProcessAttrLocked).stubs().will(returnValue(&attr));
    UpdateMigResult(&mMsg, &manager);
    EXPECT_EQ(0, attr.strategyAttr.remoteNrPagesAfterMigrate[0][0]);
}

TEST_F(MigrationTest, TestUpdateMigResultRemoteToLocalExpectedMigCount)
{
    ProcessAttr attr = {};
    attr.next = NULL;
    attr.pid = 123;
    attr.numaAttr.numaNodes = 0b00010001;
    attr.strategyAttr.remoteNrPagesAfterMigrate[0][0] = 100;

    struct MigrateMsg mMsg = {};
    mMsg.cnt = 1;
    mMsg.migList = (struct MigList*)malloc(sizeof(struct MigList) * 1);
    mMsg.migList[0].pid = 123;
    mMsg.migList[0].from = 4;
    mMsg.migList[0].to = 0;
    mMsg.migList[0].nr = 100;
    mMsg.migList[0].failedMigNr = 30;
    mMsg.migList[0].successToUser = true;

    ProcessManager manager = {};
    manager.nrLocalNuma = 4;
    manager.processes = &attr;
    MOCKER(GetProcessAttrLocked).stubs().will(returnValue(&attr));
    UpdateMigResult(&mMsg, &manager);
    EXPECT_EQ(30, attr.strategyAttr.remoteNrPagesAfterMigrate[0][0]);
}
TEST_F(MigrationTest, TestUpdateMigResultTwo)
{
    ProcessAttr attr = {};
    attr.next = NULL;
    attr.pid = 123;
    attr.numaAttr.numaNodes = 0b00010001;
    attr.strategyAttr.remoteNrPagesAfterMigrate[0][1] = 0;

    struct MigrateMsg mMsg = {};
    mMsg.cnt = 1;
    mMsg.migList = (struct MigList*)malloc(sizeof(struct MigList) * 1);
    mMsg.migList[0].pid = 123;
    mMsg.migList[0].from = 0;
    mMsg.migList[0].to = 5;
    mMsg.migList[0].nr = 100;
    mMsg.migList[0].failedMigNr = 40;
    mMsg.migList[0].successToUser = true;

    ProcessManager manager = {};
    manager.nrLocalNuma = 4;
    manager.processes = &attr;
    MOCKER(GetProcessAttrLocked).stubs().will(returnValue(&attr));
    UpdateMigResult(&mMsg, &manager);
    EXPECT_EQ(60, attr.strategyAttr.remoteNrPagesAfterMigrate[0][1]);
    free(mMsg.migList);
}

TEST_F(MigrationTest, TestUpdateMigResultThree)
{
    ProcessAttr attr = {};
    attr.next = NULL;
    attr.pid = 123;
    attr.numaAttr.numaNodes = 0b00010001;
    attr.strategyAttr.remoteNrPagesAfterMigrate[0][1] = 0;

    struct MigrateMsg mMsg = {};
    mMsg.cnt = 1;
    mMsg.migList = (struct MigList*)malloc(sizeof(struct MigList) * 1);
    mMsg.migList[0].pid = 123;
    mMsg.migList[0].from = 0;
    mMsg.migList[0].to = 5;
    mMsg.migList[0].nr = 100;
    mMsg.migList[0].failedMigNr = 40;
    mMsg.migList[0].successToUser = false;

    ProcessManager manager = {};
    manager.nrLocalNuma = 4;
    manager.processes = &attr;
    MOCKER(GetProcessAttrLocked).stubs().will(returnValue(&attr));
    UpdateMigResult(&mMsg, &manager);
    EXPECT_EQ(0, attr.strategyAttr.remoteNrPagesAfterMigrate[0][1]);
    free(mMsg.migList);
}

TEST_F(MigrationTest, TestUpdateMigResultFour)
{
    ProcessAttr attr = {};
    attr.next = NULL;
    attr.pid = 123;
    attr.numaAttr.numaNodes = 0b00100001;
    attr.strategyAttr.remoteNrPagesAfterMigrate[0][0] = 1000;

    struct MigrateMsg mMsg = {};
    mMsg.cnt = 1;
    mMsg.migList = (struct MigList*)malloc(sizeof(struct MigList) * 1);
    mMsg.migList[0].pid = 123;
    mMsg.migList[0].from = 0;
    mMsg.migList[0].to = 4;
    mMsg.migList[0].nr = 100;
    mMsg.migList[0].failedMigNr = 1;
    mMsg.migList[0].successToUser = true;

    ProcessManager manager = {};
    manager.nrLocalNuma = 4;
    manager.processes = &attr;
    MOCKER(GetProcessAttrLocked).stubs().will(returnValue(&attr));
    UpdateMigResult(&mMsg, &manager);
    EXPECT_EQ(1099, attr.strategyAttr.remoteNrPagesAfterMigrate[0][0]);
    free(mMsg.migList);
}

TEST_F(MigrationTest, TestUpdateMigResultFive)
{
    ProcessAttr attr = {};
    ProcessAttr attr2 = {};
    attr.next = &attr2;
    attr.pid = 123;
    attr2.pid = 1234;
    attr.numaAttr.numaNodes = 0b00110001;
    attr2.numaAttr.numaNodes = 0b00110001;
    attr.strategyAttr.remoteNrPagesAfterMigrate[0][0] = 100;
    attr2.strategyAttr.remoteNrPagesAfterMigrate[0][1] = 100;

    struct MigrateMsg mMsg = {};
    mMsg.cnt = 2;
    mMsg.migList = (struct MigList*)malloc(sizeof(struct MigList) * 2);
    mMsg.migList[0].pid = 123;
    mMsg.migList[0].from = 0;
    mMsg.migList[0].to = 4;
    mMsg.migList[0].nr = 100;
    mMsg.migList[0].failedMigNr = 1;
    mMsg.migList[0].successToUser = true;

    mMsg.migList[1].pid = 1234;
    mMsg.migList[1].from = 0;
    mMsg.migList[1].to = 5;
    mMsg.migList[1].nr = 100;
    mMsg.migList[1].failedMigNr = 99;
    mMsg.migList[1].successToUser = true;

    ProcessManager manager = {};
    manager.nrLocalNuma = 4;
    manager.processes = &attr;
    g_processManager.processes = &attr;
    UpdateMigResult(&mMsg, &manager);
    EXPECT_EQ(199, attr.strategyAttr.remoteNrPagesAfterMigrate[0][0]);
    EXPECT_EQ(101, attr2.strategyAttr.remoteNrPagesAfterMigrate[0][1]);
    free(mMsg.migList);
}

TEST_F(MigrationTest, TestUpdateMigResultSix)
{
    ProcessAttr attr = {};
    attr.next = NULL;
    attr.pid = 123;
    attr.numaAttr.numaNodes = 0b00100001;
    attr.strategyAttr.remoteNrPagesAfterMigrate[0][0] = 100;

    struct MigrateMsg mMsg = {};
    mMsg.cnt = 1;
    mMsg.migList = (struct MigList*)malloc(sizeof(struct MigList) * 1);
    mMsg.migList[0].pid = 123;
    mMsg.migList[0].from = 4;
    mMsg.migList[0].to = 0;
    mMsg.migList[0].nr = 100;
    mMsg.migList[0].failedMigNr = 30;
    mMsg.migList[0].successToUser = true;

    ProcessManager manager = {};
    manager.nrLocalNuma = 4;
    manager.processes = &attr;
    MOCKER(GetProcessAttrLocked).stubs().will(returnValue(&attr));
    UpdateMigResult(&mMsg, &manager);
    EXPECT_EQ(30, attr.strategyAttr.remoteNrPagesAfterMigrate[0][0]);
    free(mMsg.migList);
}

TEST_F(MigrationTest, TestUpdateMigResultSeven)
{
    ProcessAttr attr = {};
    ProcessAttr attr2 = {};
    attr.next = &attr2;
    attr.pid = 123;
    attr2.pid = 1234;
    attr.numaAttr.numaNodes = 0b00110001;
    attr2.numaAttr.numaNodes = 0b00110001;
    attr.strategyAttr.remoteNrPagesAfterMigrate[0][0] = 100;
    attr2.strategyAttr.remoteNrPagesAfterMigrate[0][1] = 100;

    struct MigrateMsg mMsg = {};
    mMsg.cnt = 2;
    mMsg.migList = (struct MigList*)malloc(sizeof(struct MigList) * 2);
    mMsg.migList[0].pid = 123;
    mMsg.migList[0].from = 4;
    mMsg.migList[0].to = 0;
    mMsg.migList[0].nr = 100;
    mMsg.migList[0].failedMigNr = 1;
    mMsg.migList[0].successToUser = true;

    mMsg.migList[1].pid = 1234;
    mMsg.migList[1].from = 5;
    mMsg.migList[1].to = 0;
    mMsg.migList[1].nr = 100;
    mMsg.migList[1].failedMigNr = 99;
    mMsg.migList[1].successToUser = true;

    ProcessManager manager = {};
    manager.nrLocalNuma = 4;
    manager.processes = &attr;
    g_processManager.processes = &attr;
    UpdateMigResult(&mMsg, &manager);
    EXPECT_EQ(1, attr.strategyAttr.remoteNrPagesAfterMigrate[0][0]);
    EXPECT_EQ(99, attr2.strategyAttr.remoteNrPagesAfterMigrate[0][1]);
    free(mMsg.migList);
}

TEST_F(MigrationTest, TestUpdateMigResultEight)
{
    ProcessAttr attr = {};
    ProcessAttr attr2 = {};
    attr.next = &attr2;
    attr.pid = 123;
    attr2.pid = 1234;
    attr.numaAttr.numaNodes = 0b00110001;
    attr2.numaAttr.numaNodes = 0b00110001;
    attr.strategyAttr.remoteNrPagesAfterMigrate[0][0] = 100;
    attr2.strategyAttr.remoteNrPagesAfterMigrate[0][1] = 100;

    struct MigrateMsg mMsg = {};
    mMsg.cnt = 2;
    mMsg.migList = (struct MigList*)malloc(sizeof(struct MigList) * 2);
    mMsg.migList[0].pid = 123;
    mMsg.migList[0].from = 0;
    mMsg.migList[0].to = 4;
    mMsg.migList[0].nr = 100;
    mMsg.migList[0].failedMigNr = 1;
    mMsg.migList[0].successToUser = true;

    mMsg.migList[1].pid = 1234;
    mMsg.migList[1].from = 5;
    mMsg.migList[1].to = 0;
    mMsg.migList[1].nr = 100;
    mMsg.migList[1].failedMigNr = 99;
    mMsg.migList[1].successToUser = true;

    ProcessManager manager = {};
    manager.nrLocalNuma = 4;
    manager.processes = &attr;
    g_processManager.processes = &attr;
    UpdateMigResult(&mMsg, &manager);
    EXPECT_EQ(199, attr.strategyAttr.remoteNrPagesAfterMigrate[0][0]);
    EXPECT_EQ(99, attr2.strategyAttr.remoteNrPagesAfterMigrate[0][1]);
}

extern "C" int MigrateRemoteNuma(struct ProcessManager *manager, struct MigrateNumaIoctlMsg *msg);
TEST_F(MigrationTest, TestMigrateRemoteNumaOne)
{
    struct ProcessManager manager;
    struct MigrateNumaIoctlMsg msg = {
        .srcNid = 4,
        .destNid = 5,
        .count = 1,
        .memids = { 1 }
    };
    MOCKER(reinterpret_cast<int (*)(int, unsigned long, void *)>(ioctl)).stubs().will(returnValue(-ENOMEM));
    int ret = MigrateRemoteNuma(&manager, &msg);
    EXPECT_EQ(-ENOMEM, ret);
}

extern "C" int CleanStrategyAttribute(struct ProcessManager *manager);
TEST_F(MigrationTest, TestCleanStrateryAttribute)
{
    struct ProcessManager manager;
    ProcessAttr current;
    manager.processes = &current;

    current.next = NULL;
    EnvMutexInit(&manager.lock);
    int ret = CleanStrategyAttribute(&manager);
    EXPECT_EQ(0, ret);
}

extern "C" void PrintMigSpeed(struct ProcessManager *manager, uint64_t nr, struct timeval start, struct timeval end);
TEST_F(MigrationTest, TestPrintMigSpeed)
{
    struct timeval start = { 0 };
    struct timeval end = { 0 };
    struct ProcessManager manager;
    manager.tracking.pageSize = PAGESIZE_4K;
    MOCKER(CalcDurationUs).stubs().will(returnValue(static_cast<long>(100)));
    PrintMigSpeed(&manager, 1000, start, end);

    manager.tracking.pageSize = PAGESIZE_2M;
    MOCKER(CalcDurationUs).stubs().will(returnValue(static_cast<long>(100)));
    PrintMigSpeed(&manager, 1000, start, end);
}

extern "C" int PreMigration(struct ProcessManager *manager, struct MigrateMsg *mMsg,
    uint64_t *migratePages);
TEST_F(MigrationTest, TestPreMigration)
{
    struct ProcessManager manager = {};
    ProcessAttr current = {};
    struct MigrateMsg mMsg = {};
    uint64_t migratePages = {};

    MOCKER(InitMigrateMsg).stubs().will(returnValue(-1));
    EnvMutexInit(&manager.lock);
    int ret = PreMigration(&manager, nullptr, nullptr);
    EXPECT_EQ(-1, ret);

    GlobalMockObject::verify();
    manager.processes = &current;
    current.pid = 1;
    current.next = NULL;
    current.scanType = NORMAL_SCAN;
    current.state = PROC_IDLE;
    MOCKER(InitMigrateMsg).stubs().will(returnValue(0));
    MOCKER(NumaMigReduceDeal).stubs();
    MOCKER(BuildMigrationMsg).stubs().will(returnValue(0));
    MOCKER(SetMigrateThreadNum).stubs().will(ignoreReturnValue());
    ret = PreMigration(&manager, &mMsg, &migratePages);
    EXPECT_EQ(0, ret);
}

TEST_F(MigrationTest, TestPreMigrationTwo)
{
    struct ProcessManager manager = {};
    ProcessAttr current = {};
    struct MigrateMsg mMsg = {};
    uint64_t migratePages = {};

    MOCKER(InitMigrateMsg).stubs().will(returnValue(-1));
    EnvMutexInit(&manager.lock);
    int ret = PreMigration(&manager, nullptr, nullptr);
    EXPECT_EQ(-1, ret);

    GlobalMockObject::verify();
    manager.processes = &current;
    current.pid = 2;
    current.next = NULL;
    current.scanType = NORMAL_SCAN;
    current.state = PROC_MIGRATE;
    MOCKER(InitMigrateMsg).stubs().will(returnValue(0));
    MOCKER(NumaMigReduceDeal).stubs();
    MOCKER(BuildMigrationMsg).stubs().will(returnValue(0));
    MOCKER(SetMigrateThreadNum).stubs().will(ignoreReturnValue());
    ret = PreMigration(&manager, &mMsg, &migratePages);
    EXPECT_EQ(0, ret);
}

extern "C" void PostMigration(struct ProcessManager *manager, struct MigrateMsg *mMsg);
TEST_F(MigrationTest, TestPostMigration)
{
    struct ProcessManager manager;
    ProcessAttr current;
    struct MigrateMsg mMsg;
    manager.processes = &current;
    current.pid = 1;
    current.next = NULL;
    current.state = PROC_MIGRATE;

    MOCKER(UpdateMigResult).stubs();
    mMsg.migList = (struct MigList *)malloc(sizeof(struct MigList));
    EnvMutexInit(&manager.lock);
    PostMigration(&manager, &mMsg);
    EXPECT_EQ(PROC_IDLE, current.state);
    free(mMsg.migList);
}

TEST_F(MigrationTest, TestPostMigrationTwo)
{
    struct ProcessManager manager;
    ProcessAttr current;
    struct MigrateMsg mMsg;
    manager.processes = &current;
    current.pid = 1;
    current.next = NULL;
    current.state = PROC_IDLE;

    MOCKER(UpdateMigResult).stubs();
    mMsg.migList = (struct MigList *)malloc(sizeof(struct MigList));
    EnvMutexInit(&manager.lock);
    PostMigration(&manager, &mMsg);
    EXPECT_EQ(PROC_IDLE, current.state);
    free(mMsg.migList);
}
