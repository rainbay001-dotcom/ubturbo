/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: smap5.0 user separate strategy ut code
 */

#include <cstdlib>
#include <algorithm>
#include <vector>
#include <cstdint>
#include <unordered_set>
#include <gmock/gmock.h>
#include "manage/manage.h"
#include "strategy/strategy.h"
#include "strategy/separate_strategy.h"
#include "strategy/period_config.h"
#include "smap_user_log.h"

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

const int DEFAULT_FREQ_WT_2M = 1;
const int DEFAULT_SLOW_THRED_2M = 2 * DEFAULT_FREQ_WT_2M;
const int MAX_MIGRATE_DIVISOR_2M = 2;

using namespace std;

typedef struct NumaInfo {
    int localNid;
    int remoteNid;
} NumaInfo;

class SeparateStrategyTest : public ::testing::Test {
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

extern "C" int InitSeparateParam(ProcessAttr *process);
TEST_F(SeparateStrategyTest, TestInitSeparateParam)
{
    ProcessAttr process;
    InitSeparateParam(&process);
    EXPECT_EQ(DEFAULT_FREQ_WT_2M, process.separateParam.freqWt);
    EXPECT_EQ(DEFAULT_SLOW_THRED_2M, process.separateParam.slowThred);
}

extern "C" bool ShouldMigrate(ProcessAttr *process);
TEST_F(SeparateStrategyTest, TestShouldMigrateOne)
{
    ProcessAttr process;
    process.separateParam.maxMigrate = 0;
    bool ret = ShouldMigrate(&process);
    EXPECT_FALSE(ret);
}

TEST_F(SeparateStrategyTest, TestShouldMigrateTwo)
{
    ProcessAttr process;
    process.separateParam.maxMigrate = 1;
    process.walkPage.nrPage = 0;
    bool ret = ShouldMigrate(&process);
    EXPECT_FALSE(ret);
}

TEST_F(SeparateStrategyTest, TestShouldMigrateThree)
{
    ProcessAttr process;
    process.separateParam.maxMigrate = 1;
    process.walkPage.nrPage = 1;
    bool ret = ShouldMigrate(&process);
    EXPECT_TRUE(ret);
}

extern "C" int ActcFreqAscFunc(const void *actc1, const void *actc2);
TEST_F(SeparateStrategyTest, TestActcFreqAscFunc)
{
    ActcData *actc1 = (ActcData *)calloc(1, sizeof(ActcData));
    ActcData *actc2 = (ActcData *)calloc(1, sizeof(ActcData));
    int ret;

    actc1->freq = 1;
    actc2->freq = 2;
    actc1->prior = 10;
    actc2->prior = 1;

    ret = ActcFreqAscFunc((const void *)actc1, (const void *)actc2);
    EXPECT_EQ(-1, ret);

    actc1->freq = 2;
    actc2->freq = 1;
    ret = ActcFreqAscFunc((const void *)actc1, (const void *)actc2);
    EXPECT_EQ(1, ret);

    actc1->freq = 1;
    actc2->freq = 1;
    ret = ActcFreqAscFunc((const void *)actc1, (const void *)actc2);
    EXPECT_EQ(-9, ret);

    free(actc1);
    free(actc2);
}

extern "C" int ActcFreqDescFunc(const void *actc1, const void *actc2);
TEST_F(SeparateStrategyTest, TestActcFreqDescFunc)
{
    ActcData *actc1 = (ActcData *)calloc(1, sizeof(ActcData));
    ActcData *actc2 = (ActcData *)calloc(1, sizeof(ActcData));
    int ret;

    actc1->freq = 1;
    actc2->freq = 2;
    actc1->prior = 10;
    actc2->prior = 1;

    ret = ActcFreqDescFunc((const void *)actc1, (const void *)actc2);
    EXPECT_EQ(1, ret);

    actc1->freq = 2;
    actc2->freq = 1;
    ret = ActcFreqDescFunc((const void *)actc1, (const void *)actc2);
    EXPECT_EQ(-1, ret);

    actc1->freq = 1;
    actc2->freq = 1;
    ret = ActcFreqDescFunc((const void *)actc1, (const void *)actc2);
    EXPECT_EQ(9, ret);

    free(actc1);
    free(actc2);
}

extern "C" uint64_t CalcMigrateNumByFreq(ProcessAttr *process);
TEST_F(SeparateStrategyTest, TestCalcMigrateNumByFreqOne)
{
    ProcessAttr process = {};

    process.numaAttr.numaNodes = 0b10011001;
    process.scanAttr.actcLen[0] = 3;
    process.scanAttr.actcLen[1] = 3;
    process.separateParam.maxMigrate = 4;
    process.scanAttr.actCount[0].freqMax = 3;
    process.scanAttr.actCount[1].freqMax = 5;
    process.scanAttr.actCount[1].freqNum = 3;
    process.separateParam.freqWt = 5;
    process.separateParam.slowThred = 1;

    ActcData actcDataL1[3];
    ActcData actcDataL2[3];
    actcDataL1[0].freq = 0;
    actcDataL1[1].freq = 2;
    actcDataL1[2].freq = 3;

    actcDataL2[0].freq = 5;
    actcDataL2[1].freq = 2;
    actcDataL2[2].freq = 3;
    process.scanAttr.actcData[0] = actcDataL1;
    process.scanAttr.actcData[1] = actcDataL2;

    MOCKER(GetNrFreeHugePagesByNode).stubs().will(returnValue(3));
    uint64_t ret = CalcMigrateNumByFreq(&process);
    EXPECT_EQ(0, ret);
}

TEST_F(SeparateStrategyTest, TestCalcMigrateNumByFreqLowSmallerThanHigh)
{
    ProcessAttr process = {};

    process.numaAttr.numaNodes = 0b10011001;
    process.scanAttr.actcLen[0] = 3;
    process.scanAttr.actcLen[1] = 3;
    process.separateParam.maxMigrate = 4;
    process.scanAttr.actCount[0].freqMax = 3;
    process.scanAttr.actCount[1].freqMax = 5;
    process.scanAttr.actCount[1].freqNum = 3;
    process.separateParam.freqWt = 5;
    process.separateParam.slowThred = 1;
    process.enableSwap = true;

    ActcData actcDataL1[3];
    ActcData actcDataL2[3];
    actcDataL1[0].freq = 0;
    actcDataL1[1].freq = 2;
    actcDataL1[2].freq = 3;

    actcDataL2[0].freq = 5;
    actcDataL2[1].freq = 2;
    actcDataL2[2].freq = 3;
    process.scanAttr.actcData[0] = actcDataL1;
    process.scanAttr.actcData[4] = actcDataL2;

    process.scanAttr.actCount[4].freqNum = 100;
    process.scanAttr.actCount[4].freqMax = 1000;

    process.scanAttr.actCount[0].freqNum = 50;
    process.scanAttr.actCount[0].freqMax = 500;

    process.scanAttr.actcLen[0] = 4;
    process.scanAttr.actcLen[4] = 4;

    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    MOCKER(GetNrFreeHugePagesByNode).stubs().will(returnValue(3));

    uint64_t ret = CalcMigrateNumByFreq(&process);
    EXPECT_EQ(1, ret);
}

TEST_F(SeparateStrategyTest, TestCalcMigrateNumByFreqTwo)
{
    ProcessAttr process;
    process.scanAttr.actcLen[0] = 6;
    process.scanAttr.actcLen[1] = 5;
    process.separateParam.maxMigrate = 0;
    MOCKER(GetNrFreeHugePagesByNode).stubs().will(returnValue(1));
    process.scanAttr.actCount[0].freqMax = 0;
    process.scanAttr.actCount[1].freqMax = 1;
    process.separateParam.freqWt = 1;
    process.separateParam.slowThred = 1;
    uint64_t ret = CalcMigrateNumByFreq(&process);
    EXPECT_EQ(0, ret);
}

extern "C" int BaseStrategy(ProcessAttr *process, struct MigList mlist[MAX_NODES][MAX_NODES],
    uint64_t rawMigrateNum, MigrateDirection dir);
TEST_F(SeparateStrategyTest, TestBaseStrategyOne)
{
    ProcessAttr process = {};
    process.numaAttr.numaNodes = 0b10011001;
    struct MigList mlist[MAX_NODES][MAX_NODES] = {};
    uint64_t rawMigrateNum;
    MigrateDirection dir = static_cast<MigrateDirection>(3);
    mlist[0][0].nr = 0;
    mlist[0][0].nr = 0;
    process.scanAttr.actcLen[0] = 0;
    process.scanAttr.actcLen[1] = 0;
    MOCKER(CalcMigrateNumByFreq).stubs().will(returnValue((uint64_t)0));
    int ret = BaseStrategy(&process, mlist, rawMigrateNum, dir);
    EXPECT_EQ(-22, ret);
}

extern "C" int SwapStrategy(ProcessAttr *process, struct MigList mlist[MAX_NODES][MAX_NODES]);
TEST_F(SeparateStrategyTest, TestSwapStrategyOne)
{
    struct MigList mlist[MAX_NODES][MAX_NODES];
    ProcessAttr process;
    MOCKER(BaseStrategy).stubs().will(returnValue(0));
    int ret = SwapStrategy(&process, mlist);
    EXPECT_EQ(0, ret);
}

extern "C" int PromotionStrategy(ProcessAttr *process,
    struct MigList mlist[MAX_NODES][MAX_NODES], uint64_t rawMigrateNum);
TEST_F(SeparateStrategyTest, TestPromotionStrategy)
{
    ProcessAttr process;
    struct MigList mlist[MAX_NODES][MAX_NODES];
    uint64_t rawMigrateNum;
    MOCKER(BaseStrategy).stubs().will(returnValue(0));
    int ret = PromotionStrategy(&process, mlist, rawMigrateNum);
    EXPECT_EQ(0, ret);
}

extern "C" int DemotionStrategy(ProcessAttr *process,
    struct MigList mlist[MAX_NODES][MAX_NODES], uint64_t rawMigrateNum);
TEST_F(SeparateStrategyTest, TestDemotionStrategy)
{
    ProcessAttr process;
    struct MigList mlist[MAX_NODES][MAX_NODES];
    uint64_t rawMigrateNum;
    MOCKER(BaseStrategy).stubs().will(returnValue(0));
    int ret = DemotionStrategy(&process, mlist, rawMigrateNum);
    EXPECT_EQ(0, ret);
}

extern "C" int SeparateStrategy(ProcessAttr *process, struct MigList mlist[MAX_NODES][MAX_NODES]);
void SeparateStrategyinit(ActcData actcData1[2], ActcData actcData2[4], ProcessAttr *process)
{
    actcData1[0].freq = 0;
    actcData1[1].freq = 2;

    actcData2[0].freq = 5;
    actcData2[1].freq = 2;
    actcData2[2].freq = 3;
    actcData2[3].freq = 1;

    process->scanAttr.actCount[0].freqNum = 50;
    process->scanAttr.actCount[0].freqMax = 500;

    process->scanAttr.actCount[4].freqNum = 100;
    process->scanAttr.actCount[4].freqMax = 1000;
}

TEST_F(SeparateStrategyTest, TestSeparateStrategyall)
{
    MOCKER(IsHugeMode).stubs().will(returnValue(true));
    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4)); // 本地4个numa
    MOCKER(GetNrFreeHugePagesByNode).stubs().will(returnValue(100)); // 远端可以100个2M页
    ProcessAttr process = {};
    struct MigList mlist[MAX_NODES][MAX_NODES] = {};
    process.pid = 1;
    process.separateParam.freqWt = 0;
    process.separateParam.maxMigrate = 100;
    process.walkPage.nrPage = 2;
    process.numaAttr.numaNodes = 0b00010001; // 0 4
    process.enableSwap = true;

    ActcData actcData1[2] = {};
    ActcData actcData2[4] = {};
    int len1 = sizeof(actcData1) / sizeof(ActcData);
    int len2 = sizeof(actcData2) / sizeof(ActcData);
    SeparateStrategyinit(actcData1, actcData2, &process);

    process.scanAttr.actcData[0] = actcData1;
    process.scanAttr.actcData[4] = actcData2;
    process.scanAttr.actcLen[0] = len1;
    process.scanAttr.actcLen[4] = len2;

    int ret = SeparateStrategy(&process, mlist);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, mlist[0][4].nr);
    EXPECT_EQ(1, mlist[4][0].nr);
    EXPECT_EQ(0, actcData1[0].freq);
    EXPECT_EQ(2, actcData1[1].freq);
    EXPECT_EQ(5, actcData2[0].freq);
    EXPECT_EQ(3, actcData2[1].freq);
    EXPECT_EQ(2, actcData2[2].freq);
    EXPECT_EQ(1, actcData2[3].freq);
    EXPECT_EQ(SWAP, process.strategyAttr.dir[0]);
    EXPECT_EQ(SWAP, process.strategyAttr.dir[4]);

    SeparateStrategyinit(actcData1, actcData2, &process);
    process.strategyAttr.nrMigratePages[4][0] = 1;
    ret = SeparateStrategy(&process, mlist);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0, mlist[0][4].nr);
    EXPECT_EQ(1, mlist[4][0].nr);
    EXPECT_EQ(0, actcData1[0].freq);
    EXPECT_EQ(2, actcData1[1].freq);
    EXPECT_EQ(5, actcData2[0].freq);
    EXPECT_EQ(3, actcData2[1].freq);
    EXPECT_EQ(2, actcData2[2].freq);
    EXPECT_EQ(1, actcData2[3].freq);
    EXPECT_EQ(PROMOTE, process.strategyAttr.dir[0]);
    EXPECT_EQ(PROMOTE, process.strategyAttr.dir[4]);

    SeparateStrategyinit(actcData1, actcData2, &process);
    process.strategyAttr.nrMigratePages[0][4] = 1;
    ret = SeparateStrategy(&process, mlist);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, mlist[0][4].nr);
    EXPECT_EQ(0, mlist[4][0].nr);
    EXPECT_EQ(0, actcData1[0].freq);
    EXPECT_EQ(2, actcData1[1].freq);
    EXPECT_EQ(5, actcData2[0].freq);
    EXPECT_EQ(3, actcData2[1].freq);
    EXPECT_EQ(2, actcData2[2].freq);
    EXPECT_EQ(1, actcData2[3].freq);
    EXPECT_EQ(DEMOTE, process.strategyAttr.dir[0]);
    EXPECT_EQ(DEMOTE, process.strategyAttr.dir[4]);
}

TEST_F(SeparateStrategyTest, TestPeriodConfig)
{
    int ret = GeneratePeriodConfigFile("./example2.config");
    EXPECT_EQ(0, ret);

    PeriodConfigRead("./example2.config");
}

TEST_F(SeparateStrategyTest, TestShouldMigrate)
{
    ProcessAttr process = {};
    process.numaAttr.numaNodes = 0b10011001;
    process.separateParam.maxMigrate = 0;
    bool ret = ShouldMigrate(&process);
    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    EXPECT_EQ(false, ret);

    process.separateParam.maxMigrate = 3;
    process.walkPage.nrPage = 0;
    ret = ShouldMigrate(&process);
    EXPECT_EQ(false, ret);

    process.walkPage.nrPage = 3;
    ret = ShouldMigrate(&process);
    EXPECT_EQ(true, ret);
}

extern "C" void FreeMlist(struct MigList mlist[MAX_NODES][MAX_NODES]);
TEST_F(SeparateStrategyTest, TestFreeMlist)
{
    struct MigList mlist[MAX_NODES][MAX_NODES] = {0};
    mlist[0][0].addr = (uint64_t *)malloc(sizeof(uint64_t));
    FreeMlist(mlist);
    EXPECT_EQ(NULL, mlist[0][0].addr);
}

extern "C" int BaseStrategyInner(ProcessAttr *process, struct MigList mlist[MAX_NODES][MAX_NODES], int from, int to);
TEST_F(SeparateStrategyTest, TestBaseStrategyInner)
{
    ProcessAttr process = {};
    struct MigList mlist[MAX_NODES][MAX_NODES] = {};
    int ret;
    int from = 0;
    int to = 0;
    mlist[from][to].nr = 1;
    process.scanAttr.actcData[0] = NULL;

    ret = BaseStrategyInner(&process, mlist, from, to);
    EXPECT_EQ(ret, -EINVAL);
}

TEST_F(SeparateStrategyTest, TestBaseStrategyInnerFromIsNULL)
{
    ProcessAttr process = {};
    ActcData actcArr[2] = {};
    struct MigList mlist[MAX_NODES][MAX_NODES] = {};

    int ret;
    int from = 0;
    int to = 0;
    mlist[from][to].nr = 1;

    process.scanAttr.actcData[0] = actcArr;
    process.scanAttr.actcData[0][0].addr = 1;
    process.scanAttr.actcLen[0] = 1;

    ret = BaseStrategyInner(&process, mlist, from, to);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(process.scanAttr.actcData[0][0].addr, *(mlist[0][0].addr));
}

extern "C" int BaseStrategy(ProcessAttr *process, struct MigList mlist[MAX_NODES][MAX_NODES],
    uint64_t rawMigrateNum, MigrateDirection dir);
TEST_F(SeparateStrategyTest, TestBaseStrategyDirEqualsDemote)
{
    ProcessAttr process = {};
    process.numaAttr.numaNodes = 0b00010001;
    uint64_t rawMigrateNum = 100;
    MigrateDirection dir = DEMOTE;
    struct MigList mlist[MAX_NODES][MAX_NODES] = {};

    for (int i = 0; i < MAX_NODES; i++) {
        process.scanAttr.actcData[i] = (ActcData *)calloc(1, sizeof(ActcData));
    }

    process.scanAttr.actcData[0]->freq = 1;
    process.scanAttr.actcData[1]->freq = 2;
    process.scanAttr.actcData[0]->prior = 10;
    process.scanAttr.actcData[1]->prior = 1;

    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    MOCKER(CalcMigrateNumByFreq).stubs().will(returnValue(1000));
    MOCKER(BaseStrategyInner).stubs().will(returnValue(0));
    MOCKER(FreeMlist).stubs().will(ignoreReturnValue());

    int ret = BaseStrategy(&process, mlist, rawMigrateNum, dir);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1000, mlist[0][4].nr);
    EXPECT_EQ(900, mlist[4][0].nr);

    for (int i = 0; i < MAX_NODES; i++) {
        free(process.scanAttr.actcData[i]);
    }
}

TEST_F(SeparateStrategyTest, TestBaseStrategyDirEqualsPromote)
{
    ProcessAttr process = {};
    process.numaAttr.numaNodes = 0b00010001;
    uint64_t rawMigrateNum = 100;
    MigrateDirection dir = PROMOTE;
    struct MigList mlist[MAX_NODES][MAX_NODES] = {};

    for (int i = 0; i < MAX_NODES; i++) {
        process.scanAttr.actcData[i] = (ActcData *)calloc(1, sizeof(ActcData));
    }

    process.scanAttr.actcData[0]->freq = 1;
    process.scanAttr.actcData[1]->freq = 2;
    process.scanAttr.actcData[0]->prior = 10;
    process.scanAttr.actcData[1]->prior = 1;

    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    MOCKER(CalcMigrateNumByFreq).stubs().will(returnValue(1000));
    MOCKER(BaseStrategyInner).stubs().will(returnValue(0));
    MOCKER(FreeMlist).stubs().will(ignoreReturnValue());

    int ret = BaseStrategy(&process, mlist, rawMigrateNum, dir);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(900, mlist[0][4].nr);
    EXPECT_EQ(1000, mlist[4][0].nr);

    for (int i = 0; i < MAX_NODES; i++) {
        free(process.scanAttr.actcData[i]);
    }
}

TEST_F(SeparateStrategyTest, TestBaseStrategyDirEqualsSWAP)
{
    ProcessAttr process = {};
    process.numaAttr.numaNodes = 0b00010001;
    uint64_t rawMigrateNum = 100;
    MigrateDirection dir = SWAP;
    struct MigList mlist[MAX_NODES][MAX_NODES] = {};

    for (int i = 0; i < MAX_NODES; i++) {
        process.scanAttr.actcData[i] = (ActcData *)calloc(1, sizeof(ActcData));
    }

    process.scanAttr.actcData[0]->freq = 1;
    process.scanAttr.actcData[1]->freq = 2;
    process.scanAttr.actcData[0]->prior = 10;
    process.scanAttr.actcData[1]->prior = 1;

    MOCKER(IsHugeMode).stubs().will(returnValue(true));
    MOCKER(GetNrFreeHugePagesByNode).stubs().will(returnValue(100));
    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    MOCKER(CalcMigrateNumByFreq).stubs().will(returnValue(1000));
    MOCKER(BaseStrategyInner).stubs().will(returnValue(0));
    MOCKER(FreeMlist).stubs().will(ignoreReturnValue());

    int ret = BaseStrategy(&process, mlist, rawMigrateNum, dir);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(100, mlist[0][4].nr);
    EXPECT_EQ(100, mlist[4][0].nr);

    GlobalMockObject::verify();
    MOCKER(IsHugeMode).stubs().will(returnValue(true));
    MOCKER(GetNrFreeHugePagesByNode).stubs().will(returnValue(1000));
    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    MOCKER(CalcMigrateNumByFreq).stubs().will(returnValue(1000));
    MOCKER(BaseStrategyInner).stubs().will(returnValue(0));
    MOCKER(FreeMlist).stubs().will(ignoreReturnValue());

    ret = BaseStrategy(&process, mlist, rawMigrateNum, dir);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1000, mlist[0][4].nr);
    EXPECT_EQ(1000, mlist[4][0].nr);
    
    for (int i = 0; i < MAX_NODES; i++) {
        free(process.scanAttr.actcData[i]);
    }
}


TEST_F(SeparateStrategyTest, TestBaseStrategyTwo)
{
    ProcessAttr process = {};
    process.numaAttr.numaNodes = 0b00010001;
    uint64_t rawMigrateNum = 100;
    MigrateDirection dir = DEMOTE;
    struct MigList mlist[MAX_NODES][MAX_NODES] = {};

    for (int i = 0; i < MAX_NODES; i++) {
        process.scanAttr.actcData[i] = (ActcData *)calloc(1, sizeof(ActcData));
    }

    process.scanAttr.actcData[0]->freq = 2;
    process.scanAttr.actcData[1]->freq = 3;
    process.scanAttr.actcData[0]->prior = 11;
    process.scanAttr.actcData[1]->prior = 10;

    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    MOCKER(CalcMigrateNumByFreq).stubs().will(returnValue(1000));
    MOCKER(BaseStrategyInner).stubs().will(returnValue(0));
    MOCKER(FreeMlist).stubs().will(ignoreReturnValue());

    int ret = BaseStrategy(&process, mlist, rawMigrateNum, dir);
    EXPECT_EQ(0, ret);

    for (int i = 0; i < MAX_NODES; i++) {
        free(process.scanAttr.actcData[i]);
    }
}

TEST_F(SeparateStrategyTest, TestBaseStrategyThree)
{
    ProcessAttr process = {};
    process.numaAttr.numaNodes = 0b00010001;
    uint64_t rawMigrateNum = 100;
    MigrateDirection dir = PROMOTE;
    struct MigList mlist[MAX_NODES][MAX_NODES] = {};

    for (int i = 0; i < MAX_NODES; i++) {
        process.scanAttr.actcData[i] = (ActcData *)calloc(1, sizeof(ActcData));
    }

    process.scanAttr.actcData[0]->freq = 20;
    process.scanAttr.actcData[1]->freq = 100;
    process.scanAttr.actcData[0]->prior = 0;
    process.scanAttr.actcData[1]->prior = 20;

    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    MOCKER(CalcMigrateNumByFreq).stubs().will(returnValue(1000));
    MOCKER(BaseStrategyInner).stubs().will(returnValue(0));
    MOCKER(FreeMlist).stubs().will(ignoreReturnValue());

    int ret = BaseStrategy(&process, mlist, rawMigrateNum, dir);
    EXPECT_EQ(0, ret);

    for (int i = 0; i < MAX_NODES; i++) {
        free(process.scanAttr.actcData[i]);
    }
}

TEST_F(SeparateStrategyTest, TestBaseStrategyFour)
{
    ProcessAttr process = {};
    process.numaAttr.numaNodes = 0b00010001;
    uint64_t rawMigrateNum = 100;
    MigrateDirection dir = PROMOTE;
    struct MigList mlist[MAX_NODES][MAX_NODES] = {};

    for (int i = 0; i < MAX_NODES; i++) {
        process.scanAttr.actcData[i] = (ActcData *)calloc(1, sizeof(ActcData));
    }

    process.scanAttr.actcData[0]->freq = 20;
    process.scanAttr.actcData[1]->freq = 100;
    process.scanAttr.actcData[0]->prior = -1;
    process.scanAttr.actcData[1]->prior = -3;

    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    MOCKER(CalcMigrateNumByFreq).stubs().will(returnValue(1000));
    MOCKER(BaseStrategyInner).stubs().will(returnValue(0));
    MOCKER(FreeMlist).stubs().will(ignoreReturnValue());

    int ret = BaseStrategy(&process, mlist, rawMigrateNum, dir);
    EXPECT_EQ(0, ret);

    for (int i = 0; i < MAX_NODES; i++) {
        free(process.scanAttr.actcData[i]);
    }
}

extern "C" void SortActcData(ProcessAttr *process);
TEST_F(SeparateStrategyTest, TestSortActcData)
{
    ProcessAttr process = {};
    process.numaAttr.numaNodes = 0b00010001;
    for (int i = 0; i < MAX_NODES; i++) {
        process.scanAttr.actcData[i] = (ActcData *)calloc(1, sizeof(ActcData));
        process.scanAttr.actcLen[i] = 1;
    }

    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    SortActcData(&process);
    for (int i = 0; i < MAX_NODES; i++) {
        free(process.scanAttr.actcData[i]);
    }
}

TEST_F(SeparateStrategyTest, TestSeparateStrategy4KShouldMigrate)
{
    ProcessAttr process;
    struct MigList mlist[MAX_NODES][MAX_NODES];
    process.separateParam.freqWt = 0;

    MOCKER(InitSeparateParam).stubs().will(ignoreReturnValue());
    MOCKER(CalcMaxMigrate).stubs().will(ignoreReturnValue());
    MOCKER(ShouldMigrate).stubs().will(returnValue(false));
    int ret = SeparateStrategy4K(&process, mlist);

    EXPECT_EQ(0, ret);
}

extern "C" int BuildSelectKMlistAddr(ProcessAttr *process, struct MigList mlist[MAX_NODES][MAX_NODES],
    uint32_t numaOffset[MAX_NODES], int from, int to, SelectionMode mode);
TEST_F(SeparateStrategyTest, TestSeparateStrategy4KAbnormal)
{
    ProcessAttr process;
    struct MigList mlist[MAX_NODES][MAX_NODES];
    process.separateParam.freqWt = 0;
    process.numaAttr.numaNodes = 0b01100011;

    MOCKER(InitSeparateParam).stubs().will(ignoreReturnValue());
    MOCKER(CalcMaxMigrate).stubs().will(ignoreReturnValue());
    MOCKER(ShouldMigrate).stubs().will(returnValue(true));
    MOCKER(SortActcData).stubs().will(ignoreReturnValue());
    MOCKER(BuildSelectKMlistAddr).stubs().will(returnValue(-ENOMEM));
    MOCKER(FreeMlist).stubs().will(ignoreReturnValue());
    int ret = SeparateStrategy4K(&process, mlist);

    EXPECT_EQ(-ENOMEM, ret);
}

TEST_F(SeparateStrategyTest, TestSeparateStrategy4KNormal)
{
    ProcessAttr process;
    struct MigList mlist[MAX_NODES][MAX_NODES];
    process.separateParam.freqWt = 0;

    MOCKER(InitSeparateParam).stubs().will(ignoreReturnValue());
    MOCKER(CalcMaxMigrate).stubs().will(ignoreReturnValue());
    MOCKER(ShouldMigrate).stubs().will(returnValue(true));
    MOCKER(SortActcData).stubs().will(ignoreReturnValue());
    MOCKER(BuildSelectKMlistAddr).stubs().will(returnValue(0));
    MOCKER(FreeMlist).stubs().will(ignoreReturnValue());
    int ret = SeparateStrategy4K(&process, mlist);

    EXPECT_EQ(0, ret);
}

void BuildActcData(int nid, const uint16_t data[], int size, ScanAttribute *scanAttribute)
{
    scanAttribute->actcLen[nid] = size;
    scanAttribute->actCount[nid].freqZero = 0;
    ActcData *actcData = (ActcData *)calloc(size, sizeof(ActcData));
    for (int i = 0; i < size; i++) {
        uint16_t freq = data[i];
        actcData[i].freq = freq;
        actcData[i].addr = nid * 1000000 + i * 10000 + freq;
        scanAttribute->actCount[nid].freqMax = std::max(scanAttribute->actCount[nid].freqMax, freq);
        scanAttribute->actCount[nid].freqMin = std::min(scanAttribute->actCount[nid].freqMin, freq);
        if (freq == 0) {
            scanAttribute->actCount[nid].freqZero++;
        } else {
            scanAttribute->actCount[nid].freqNum++;
        }
        scanAttribute->actCount[nid].freqSum += freq;
    }
    scanAttribute->actcData[nid] = actcData;
}

void initializeMigList(MigList mlist[][MAX_NODES])
{
    for (int i = 0; i < MAX_NODES; ++i) {
        for (int j = 0; j < MAX_NODES; ++j) {
            mlist[i][j].from = i;
            mlist[i][j].to = j;
        }
    }
}

extern "C" uint64_t GetNrFreePagesByNode(int nid);
TEST_F(SeparateStrategyTest, TestSeparateStrategy4KSwapBasic)
{
    ProcessAttr process = {};
    struct MigList mlist[MAX_NODES][MAX_NODES];
    initializeMigList(mlist);
    process.numaAttr.numaNodes = 0b00100001;
    process.walkPage.nrPage = 10;
    process.separateParam.freqWt = 0;
    int nodeL0 = 0;
    int nodeR5 = 5;

    process.strategyAttr.nrMigratePages[nodeL0][nodeR5] = 0;
    process.strategyAttr.nrMigratePages[nodeR5][nodeL0] = 0;

    uint16_t nodeL0Data[] = {0, 2, 3, 0, 8, 1, 4, 0, 0, 0};
    uint16_t nodeR5Data[] = {0, 0, 1, 2, 3, 7, 6, 5, 4, 8};

    BuildActcData(nodeL0, nodeL0Data, sizeof(nodeL0Data) / sizeof(nodeL0Data[0]), &process.scanAttr);
    BuildActcData(nodeR5, nodeR5Data, sizeof(nodeR5Data) / sizeof(nodeR5Data[0]), &process.scanAttr);

    MOCKER(GetNrFreePagesByNode).stubs().will(returnValue(10));
    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));

    int ret = SeparateStrategy4K(&process, mlist);

    EXPECT_EQ(0, ret);
}

TEST_F(SeparateStrategyTest, TestSeparateStrategy4KSwapBasicDemote)
{
    ProcessAttr process = {};
    struct MigList mlist[MAX_NODES][MAX_NODES];
    initializeMigList(mlist);
    process.numaAttr.numaNodes = 0b00100001;
    process.walkPage.nrPage = 10;
    process.separateParam.freqWt = 0;
    int nodeL0 = 0;
    int nodeR5 = 5;

    process.strategyAttr.nrMigratePages[nodeL0][nodeR5] = 2;
    process.strategyAttr.nrMigratePages[nodeR5][nodeL0] = 0;

    uint16_t nodeL0Data[] = {0, 2, 3, 0, 8, 1, 4, 0, 0, 0};
    uint16_t nodeR5Data[] = {0, 0, 1, 2, 3, 7, 6, 5, 4, 8};

    BuildActcData(nodeL0, nodeL0Data, sizeof(nodeL0Data) / sizeof(nodeL0Data[0]), &process.scanAttr);
    BuildActcData(nodeR5, nodeR5Data, sizeof(nodeR5Data) / sizeof(nodeR5Data[0]), &process.scanAttr);

    MOCKER(GetNrFreePagesByNode).stubs().will(returnValue(10));
    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));

    int ret = SeparateStrategy4K(&process, mlist);

    EXPECT_EQ(0, ret);
}

TEST_F(SeparateStrategyTest, TestSeparateStrategy4KSwapBasicPromote)
{
    ProcessAttr process = {};
    struct MigList mlist[MAX_NODES][MAX_NODES];
    initializeMigList(mlist);
    process.numaAttr.numaNodes = 0b00100001;
    process.walkPage.nrPage = 10;
    process.separateParam.freqWt = 0;
    int nodeL0 = 0;
    int nodeR5 = 5;

    process.strategyAttr.nrMigratePages[nodeL0][nodeR5] = 0;
    process.strategyAttr.nrMigratePages[nodeR5][nodeL0] = 2;

    uint16_t nodeL0Data[] = {0, 2, 3, 0, 8, 1, 4, 0, 0, 0};
    uint16_t nodeR5Data[] = {0, 0, 1, 2, 3, 7, 6, 5, 4, 8};

    BuildActcData(nodeL0, nodeL0Data, sizeof(nodeL0Data) / sizeof(nodeL0Data[0]), &process.scanAttr);
    BuildActcData(nodeR5, nodeR5Data, sizeof(nodeR5Data) / sizeof(nodeR5Data[0]), &process.scanAttr);

    MOCKER(GetNrFreePagesByNode).stubs().will(returnValue(10));
    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));

    int ret = SeparateStrategy4K(&process, mlist);

    EXPECT_EQ(0, ret);
}

TEST_F(SeparateStrategyTest, TestSeparateStrategy4KSwapMultiNode)
{
    ProcessAttr process = {};
    struct MigList mlist[MAX_NODES][MAX_NODES];
    initializeMigList(mlist);
    process.numaAttr.numaNodes = 0b00100011;
    process.walkPage.nrPage = 10;
    process.separateParam.freqWt = 0;
    int nodeL0 = 0;
    int nodeL1 = 1;
    int nodeR5 = 5;

    process.strategyAttr.nrMigratePages[nodeL0][nodeR5] = 0;
    process.strategyAttr.nrMigratePages[nodeR5][nodeL0] = 2;
    process.strategyAttr.nrMigratePages[nodeL1][nodeR5] = 0;
    process.strategyAttr.nrMigratePages[nodeR5][nodeL1] = 8;

    uint16_t nodeL0Data[] = {0, 2, 3, 0, 8, 1, 4, 0, 0, 0};
    uint16_t nodeL1Data[] = {0, 0, 4, 6, 8, 0, 0, 0, 0, 0};
    uint16_t nodeR5Data[] = {1, 1, 1, 2, 3, 7, 6, 5, 4, 8};

    BuildActcData(nodeL0, nodeL0Data, sizeof(nodeL0Data) / sizeof(nodeL0Data[0]), &process.scanAttr);
    BuildActcData(nodeL1, nodeL1Data, sizeof(nodeL1Data) / sizeof(nodeL1Data[0]), &process.scanAttr);
    BuildActcData(nodeR5, nodeR5Data, sizeof(nodeR5Data) / sizeof(nodeR5Data[0]), &process.scanAttr);

    MOCKER(GetNrFreePagesByNode).stubs().will(returnValue(20));
    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));

    int ret = SeparateStrategy4K(&process, mlist);

    EXPECT_EQ(0, ret);
    EXPECT_EQ(0, mlist[0][5].nr);
    EXPECT_EQ(2, mlist[5][0].nr);
    EXPECT_EQ(0, mlist[1][5].nr);
    EXPECT_EQ(8, mlist[5][1].nr);
}

TEST_F(SeparateStrategyTest, TestSeparateStrategy4KSwapAndPromoteMultiNode)
{
    ProcessAttr process = {};
    struct MigList mlist[MAX_NODES][MAX_NODES];
    initializeMigList(mlist);
    process.numaAttr.numaNodes = 0b00100011;
    process.walkPage.nrPage = 10;
    process.separateParam.freqWt = 0;
    int nodeL0 = 0;
    int nodeL1 = 1;
    int nodeR5 = 5;

    process.strategyAttr.nrMigratePages[nodeL0][nodeR5] = 0;
    process.strategyAttr.nrMigratePages[nodeR5][nodeL0] = 2;
    process.strategyAttr.nrMigratePages[nodeL1][nodeR5] = 0;
    process.strategyAttr.nrMigratePages[nodeR5][nodeL1] = 8;

    uint16_t nodeL0Data[] = {0, 2, 3, 1, 8, 1, 4, 1, 1, 1};
    uint16_t nodeL1Data[] = {0, 0, 4, 6, 8, 0, 0, 0, 0, 0};
    uint16_t nodeR5Data[] = {1, 1, 1, 2, 3, 7, 6, 5, 4, 8, 1, 1};

    BuildActcData(nodeL0, nodeL0Data, sizeof(nodeL0Data) / sizeof(nodeL0Data[0]), &process.scanAttr);
    BuildActcData(nodeL1, nodeL1Data, sizeof(nodeL1Data) / sizeof(nodeL1Data[0]), &process.scanAttr);
    BuildActcData(nodeR5, nodeR5Data, sizeof(nodeR5Data) / sizeof(nodeR5Data[0]), &process.scanAttr);

    MOCKER(GetNrFreePagesByNode).stubs().will(returnValue(20));
    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));

    int ret = SeparateStrategy4K(&process, mlist);

    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, mlist[0][5].nr);
    EXPECT_EQ(3, mlist[5][0].nr);
    EXPECT_EQ(1, mlist[1][5].nr);
    EXPECT_EQ(9, mlist[5][1].nr);
}

TEST_F(SeparateStrategyTest, TestSeparateStrategy4KSwapLimitByFreePage)
{
    ProcessAttr process = {};
    struct MigList mlist[MAX_NODES][MAX_NODES];
    initializeMigList(mlist);
    process.numaAttr.numaNodes = 0b00100011;
    process.walkPage.nrPage = 10;
    process.separateParam.freqWt = 0;
    int nodeL0 = 0;
    int nodeL1 = 1;
    int nodeR5 = 5;

    process.strategyAttr.nrMigratePages[nodeL0][nodeR5] = 0;
    process.strategyAttr.nrMigratePages[nodeR5][nodeL0] = 0;
    process.strategyAttr.nrMigratePages[nodeL1][nodeR5] = 0;
    process.strategyAttr.nrMigratePages[nodeR5][nodeL1] = 0;

    uint16_t nodeL0Data[] = {0, 2, 3, 0, 8, 1, 4, 0, 0, 0};
    uint16_t nodeL1Data[] = {0, 0, 4, 6, 8, 0, 0, 0, 0, 0};
    uint16_t nodeR5Data[] = {0, 0, 1, 2, 3, 7, 6, 5, 4, 8};

    BuildActcData(nodeL0, nodeL0Data, sizeof(nodeL0Data) / sizeof(nodeL0Data[0]), &process.scanAttr);
    BuildActcData(nodeL1, nodeL1Data, sizeof(nodeL1Data) / sizeof(nodeL1Data[0]), &process.scanAttr);
    BuildActcData(nodeR5, nodeR5Data, sizeof(nodeR5Data) / sizeof(nodeR5Data[0]), &process.scanAttr);

    MOCKER(GetNrFreePagesByNode).stubs().will(returnValue(6));
    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));

    int ret = SeparateStrategy4K(&process, mlist);

    EXPECT_EQ(0, ret);
}

extern "C" void CalculateMigInfo(ProcessAttr *process, RemoteMigInfo remoteMigInfo[REMOTE_NUMA_NUM],
                                 uint64_t nrPages[NR_LEVEL], uint64_t *demoteNum, uint64_t *promoteNum);

TEST_F(SeparateStrategyTest, TestCalculateMigInfo)
{
    int nrLocalNuma = 4;
    ProcessAttr process = {};
    process.remoteNumaCnt = 1;
    process.migrateParam[0].nid = 4;
    int nid = process.migrateParam[0].nid;
    int index = nid - nrLocalNuma;
    process.migrateParam[0].memSize = 4096;
    process.scanAttr.actcLen[nid] = 1;
    uint64_t demoteNum = 0;
    uint64_t promoteNum = 0;
    RemoteMigInfo remoteMigInfo[REMOTE_NUMA_NUM] = { 0 };
    uint64_t nrPages[NR_LEVEL] = { 0 };

    MOCKER(GetNrLocalNuma).stubs().will(returnValue(nrLocalNuma)); // 本地4个numa
    CalculateMigInfo(&process, remoteMigInfo, nrPages, &demoteNum, &promoteNum);
    EXPECT_EQ(DEMOTE, remoteMigInfo[index].dir);
    EXPECT_EQ(1, remoteMigInfo[index].nrMig);
    EXPECT_EQ(remoteMigInfo[index].nrMig, demoteNum);
    EXPECT_EQ(0, promoteNum);

    process.scanAttr.actcLen[nid] = 3;
    demoteNum = 0;
    promoteNum = 0;
    CalculateMigInfo(&process, remoteMigInfo, nrPages, &demoteNum, &promoteNum);
    EXPECT_EQ(PROMOTE, remoteMigInfo[index].dir);
    EXPECT_EQ(1, remoteMigInfo[index].nrMig);
    EXPECT_EQ(0, demoteNum);
    EXPECT_EQ(remoteMigInfo[index].nrMig, promoteNum);

    process.scanAttr.actcLen[nid] = 2;
    demoteNum = 0;
    promoteNum = 0;
    CalculateMigInfo(&process, remoteMigInfo, nrPages, &demoteNum, &promoteNum);
    EXPECT_EQ(SWAP, remoteMigInfo[index].dir);
    EXPECT_EQ(0, remoteMigInfo[index].nrMig);
    EXPECT_EQ(0, demoteNum);
    EXPECT_EQ(0, promoteNum);
}

extern "C" int SwapMultiNumaVmStrategy(ProcessAttr *process, struct MigList mlist[MAX_NODES][MAX_NODES],
                                       uint64_t nrPages[NR_LEVEL], int nrLocalNuma);
extern "C" int DemoteMultiNumaVmStrategy(ProcessAttr *process, struct MigList mlist[MAX_NODES][MAX_NODES],
                                         RemoteMigInfo remoteMigInfo[REMOTE_NUMA_NUM], uint64_t nrPages[NR_LEVEL],
                                         uint64_t demoteNum);
extern "C" int PromoteMultiNumaVmStrategy(ProcessAttr *process, struct MigList mlist[MAX_NODES][MAX_NODES],
                                          RemoteMigInfo remoteMigInfo[REMOTE_NUMA_NUM], int nrLocalNuma);
extern "C" int SeparateStrategyMultiNumaVm(ProcessAttr *process, struct MigList mlist[MAX_NODES][MAX_NODES]);
TEST_F(SeparateStrategyTest, TestSeparateStrategyMultiNumaVm)
{
    ProcessAttr process = {};
    struct MigList mlist[MAX_NODES][MAX_NODES];
    initializeMigList(mlist);
    process.separateParam.freqWt = 0;
    for (int i = 0; i < MAX_NODES; i++) {
        process.scanAttr.actcLen[i] = 0;
    }

    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    MOCKER(InitSeparateParam).stubs().will(ignoreReturnValue());
    MOCKER(CalculateMigInfo).stubs().will(ignoreReturnValue());
    MOCKER(SwapMultiNumaVmStrategy).stubs().will(returnValue(0));
    int ret = SeparateStrategyMultiNumaVm(&process, mlist);
    EXPECT_EQ(0, ret);

    GlobalMockObject::verify();
    uint64_t demoteNum = 1;
    uint64_t promoteNum = 0;
    for (int i = 0; i < MAX_NODES; i++) {
        process.scanAttr.actcLen[i] = 1;
    }
    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    MOCKER(InitSeparateParam).stubs().will(ignoreReturnValue());
    MOCKER(CalculateMigInfo).stubs().with(any(), any(), any(), outBoundP(&demoteNum, sizeof(demoteNum)),
         outBoundP(&promoteNum, sizeof(promoteNum)))
         .will(ignoreReturnValue());
    MOCKER(DemoteMultiNumaVmStrategy).stubs().will(returnValue(1));
    ret = SeparateStrategyMultiNumaVm(&process, mlist);
    EXPECT_EQ(1, ret);

    GlobalMockObject::verify();
    demoteNum = 0;
    promoteNum = 1;
    for (int i = 0; i < MAX_NODES; i++) {
        process.scanAttr.actcLen[i] = 1;
    }
    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    MOCKER(InitSeparateParam).stubs().will(ignoreReturnValue());
    MOCKER(CalculateMigInfo).stubs().with(any(), any(), any(), outBoundP(&demoteNum, sizeof(demoteNum)),
         outBoundP(&promoteNum, sizeof(promoteNum)))
         .will(ignoreReturnValue());
    MOCKER(PromoteMultiNumaVmStrategy).stubs().will(returnValue(2));
    ret = SeparateStrategyMultiNumaVm(&process, mlist);
    EXPECT_EQ(2, ret);
}

extern "C" int BuildLevelActcData(ProcessAttr *process, LevelActcData *levelActcData, uint64_t nrPages,
                                  uint64_t *levelActcLen, int level);
extern "C" uint64_t CalcMultiNumaVmSwapNumByFreq(ProcessAttr *process, LevelActcData *levelActcData[NR_LEVEL],
                                                 uint64_t levelActcLen[NR_LEVEL]);
extern "C" int GroupMigPagesByNode(LevelActcData *levelActcData[NR_LEVEL], uint64_t swapNum, uint64_t nrMig[MAX_NODES],
                                   uint64_t *migAddrArray[MAX_NODES]);
extern "C" int BuildSwapMigLists(ProcessAttr *process, struct MigList mlist[MAX_NODES][MAX_NODES],
                                 uint64_t nrMig[MAX_NODES], uint64_t *migAddrArray[MAX_NODES], int nrLocalNuma);
TEST_F(SeparateStrategyTest, TestSwapMultiNumaVmStrategy)
{
    ProcessAttr process = {};
    struct MigList mlist[MAX_NODES][MAX_NODES];
    uint64_t nrPages[NR_LEVEL] = { 0 };
    int nrLocalNuma = 0;

    nrPages[0] = 1;
    MOCKER(calloc).stubs().will(returnValue(static_cast<void *>(nullptr)));
    int ret = SwapMultiNumaVmStrategy(&process, mlist, nrPages, nrLocalNuma);
    EXPECT_EQ(-ENOMEM, ret);

    GlobalMockObject::verify();
    nrPages[0] = 1;
    LevelActcData *levelActcData_1 = (LevelActcData *)calloc(nrPages[0], sizeof(LevelActcData));
    MOCKER(calloc).stubs().will(returnValue(static_cast<void *>(levelActcData_1)));
    MOCKER(BuildLevelActcData).stubs().will(returnValue(1));
    ret = SwapMultiNumaVmStrategy(&process, mlist, nrPages, nrLocalNuma);
    EXPECT_EQ(1, ret);

    GlobalMockObject::verify();
    LevelActcData *levelActcData_2 = (LevelActcData *)calloc(nrPages[0], sizeof(LevelActcData));
    MOCKER(calloc).stubs().will(returnValue(static_cast<void *>(levelActcData_2)));
    MOCKER(BuildLevelActcData).stubs().will(returnValue(0)).then(returnValue(2));
    ret = SwapMultiNumaVmStrategy(&process, mlist, nrPages, nrLocalNuma);
    EXPECT_EQ(2, ret);

    GlobalMockObject::verify();
    LevelActcData *levelActcData_3 = (LevelActcData *)calloc(nrPages[0], sizeof(LevelActcData));
    MOCKER(calloc).stubs().will(returnValue(static_cast<void *>(levelActcData_3)));
    MOCKER(BuildLevelActcData).stubs().will(returnValue(0)).then(returnValue(0));
    MOCKER(CalcMultiNumaVmSwapNumByFreq).stubs().will(returnValue((uint64_t)0));
    ret = SwapMultiNumaVmStrategy(&process, mlist, nrPages, nrLocalNuma);
    EXPECT_EQ(0, ret);

    GlobalMockObject::verify();
    nrPages[1] = 1;
    LevelActcData *levelActcData_4 = (LevelActcData *)calloc(nrPages[0], sizeof(LevelActcData));
    LevelActcData *levelActcData_5 = (LevelActcData *)calloc(nrPages[1], sizeof(LevelActcData));
    MOCKER(calloc).stubs()
        .will(returnValue(static_cast<void *>(levelActcData_4)))
        .then(returnValue(static_cast<void *>(levelActcData_5)));
    MOCKER(BuildLevelActcData).stubs().will(returnValue(0)).then(returnValue(0));
    MOCKER(CalcMultiNumaVmSwapNumByFreq).stubs().will(returnValue((uint64_t)1));
    MOCKER(GroupMigPagesByNode).stubs().will(returnValue(-ENOMEM));
    ret = SwapMultiNumaVmStrategy(&process, mlist, nrPages, nrLocalNuma);
    EXPECT_EQ(-ENOMEM, ret);

    GlobalMockObject::verify();
    LevelActcData *levelActcData_6 = (LevelActcData *)calloc(nrPages[0], sizeof(LevelActcData));
    LevelActcData *levelActcData_7 = (LevelActcData *)calloc(nrPages[1], sizeof(LevelActcData));
    MOCKER(calloc).stubs()
        .will(returnValue(static_cast<void *>(levelActcData_6)))
        .then(returnValue(static_cast<void *>(levelActcData_7)));
    MOCKER(BuildLevelActcData).stubs().will(returnValue(0)).then(returnValue(0));
    MOCKER(CalcMultiNumaVmSwapNumByFreq).stubs().will(returnValue((uint64_t)1));
    MOCKER(GroupMigPagesByNode).stubs().will(returnValue(0));
    MOCKER(BuildSwapMigLists).stubs().will(returnValue(-EINVAL));
    ret = SwapMultiNumaVmStrategy(&process, mlist, nrPages, nrLocalNuma);
    EXPECT_EQ(-EINVAL, ret);

    GlobalMockObject::verify();
    LevelActcData *levelActcData_8 = (LevelActcData *)calloc(nrPages[0], sizeof(LevelActcData));
    LevelActcData *levelActcData_9 = (LevelActcData *)calloc(nrPages[1], sizeof(LevelActcData));
    MOCKER(calloc).stubs()
        .will(returnValue(static_cast<void *>(levelActcData_8)))
        .then(returnValue(static_cast<void *>(levelActcData_9)));
    MOCKER(BuildLevelActcData).stubs().will(returnValue(0)).then(returnValue(0));
    MOCKER(CalcMultiNumaVmSwapNumByFreq).stubs().will(returnValue((uint64_t)1));
    MOCKER(GroupMigPagesByNode).stubs().will(returnValue(0));
    MOCKER(BuildSwapMigLists).stubs().will(returnValue(0));
    ret = SwapMultiNumaVmStrategy(&process, mlist, nrPages, nrLocalNuma);
    EXPECT_EQ(0, ret);
}

extern "C" int BuildDemoteMultiNumaMigLists(uint64_t *migAddrArray[MAX_NODES], uint64_t nrMig[MAX_NODES],
                                            struct MigList mlist[MAX_NODES][MAX_NODES],
                                            RemoteMigInfo remoteMigInfo[REMOTE_NUMA_NUM]);
TEST_F(SeparateStrategyTest, TestDemoteMultiNumaVmStrategy)
{
    ProcessAttr process = {};
    struct MigList mlist[MAX_NODES][MAX_NODES];
    RemoteMigInfo remoteMigInfo[REMOTE_NUMA_NUM];
    uint64_t nrPages[NR_LEVEL] = { 0 };
    uint64_t demoteNum = 0;
    uint64_t l1ActcLen = 0;

    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    MOCKER(calloc).stubs().will(returnValue(static_cast<void *>(nullptr)));
    int ret = DemoteMultiNumaVmStrategy(&process, mlist, remoteMigInfo, nrPages, demoteNum);
    EXPECT_EQ(-ENOMEM, ret);

    GlobalMockObject::verify();
    nrPages[L1] = 1;
    LevelActcData *l1ActcData_1 = (LevelActcData *)calloc(nrPages[L1], sizeof(LevelActcData));
    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    MOCKER(calloc).stubs().will(returnValue(static_cast<void *>(l1ActcData_1)));
    MOCKER(BuildLevelActcData).stubs().will(returnValue(1));
    ret = DemoteMultiNumaVmStrategy(&process, mlist, remoteMigInfo, nrPages, demoteNum);
    EXPECT_EQ(1, ret);

    GlobalMockObject::verify();
    demoteNum = 1;
    l1ActcLen = 1;
    LevelActcData *l1ActcData_2 = (LevelActcData *)calloc(nrPages[L1], sizeof(LevelActcData));
    uint64_t *migAddrArray_1 = (uint64_t *)calloc(1, sizeof(LevelActcData));
    l1ActcData_2[0].node = 0;

    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    MOCKER(calloc).stubs()
        .will(returnValue(static_cast<void *>(l1ActcData_2)))
        .then(returnValue(static_cast<void *>(migAddrArray_1)));
    MOCKER(BuildLevelActcData).stubs()
        .with(any(), any(), any(), outBoundP(&l1ActcLen, sizeof(uint64_t)))
        .will(returnValue(0));
    MOCKER(BuildDemoteMultiNumaMigLists).stubs().will(returnValue(0));
    ret = DemoteMultiNumaVmStrategy(&process, mlist, remoteMigInfo, nrPages, demoteNum);
    EXPECT_EQ(0, ret);
}

const int MULTI_NUMA_VM_OOM = 66;
TEST_F(SeparateStrategyTest, TestPromoteMultiNumaVmStrategy)
{
    ProcessAttr process = {};
    struct MigList mlist[MAX_NODES][MAX_NODES];
    RemoteMigInfo remoteMigInfo[REMOTE_NUMA_NUM] = {};
    int nrLocalNuma = 0;

    int ret = PromoteMultiNumaVmStrategy(&process, mlist, remoteMigInfo, nrLocalNuma);
    EXPECT_EQ(0, ret);

    remoteMigInfo[0].nrMig = 1;
    remoteMigInfo[0].dir = PROMOTE;
    ret = PromoteMultiNumaVmStrategy(&process, mlist, remoteMigInfo, nrLocalNuma);
    EXPECT_EQ(-MULTI_NUMA_VM_OOM, ret);

    nrLocalNuma = 1;
    process.numaAttr.numaNodes = 0b0000001;
    MOCKER(GetNrFreeHugePagesByNode).stubs().will(returnValue((uint64_t)1));
    ret = PromoteMultiNumaVmStrategy(&process, mlist, remoteMigInfo, nrLocalNuma);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0, remoteMigInfo[0].nrMig);
}

TEST_F(SeparateStrategyTest, TestBuildLevelActcData)
{
    ProcessAttr process = {};
    LevelActcData *levelActcData = (LevelActcData *)calloc(1, sizeof(LevelActcData));
    uint64_t nrPages = 1;
    uint64_t levelActcLen = 0;
    int level = 0;

    for (int i = 0; i < MAX_NODES; i++) {
        process.scanAttr.actcLen[i] = 0;
    }
    int ret = BuildLevelActcData(&process, levelActcData, nrPages, &levelActcLen, level);
    EXPECT_EQ(0, ret);

    process.scanAttr.actcLen[5] = 1;
    level = L2;
    process.scanAttr.actcData[5] = (ActcData *)calloc(1, sizeof(ActcData));
    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    ret = BuildLevelActcData(&process, levelActcData, nrPages, &levelActcLen, level);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(levelActcData[0].node, 5);
    EXPECT_EQ(levelActcLen, 1);

    free(levelActcData);
    free(process.scanAttr.actcData[5]);
}

extern "C" int BuildMigListForDirection(MigrateDirection dir, uint64_t *migAddrArray[MAX_NODES],
                                        uint64_t nrMig[MAX_NODES], uint64_t destFreeList[MAX_NODES],
                                        struct MigList mlist[MAX_NODES][MAX_NODES]);
TEST_F(SeparateStrategyTest, TestBuildSwapMigLists)
{
    ProcessAttr process = {};
    struct MigList mlist[MAX_NODES][MAX_NODES];
    uint64_t nrMig[MAX_NODES] = { 0 };
    uint64_t *migAddrArray[MAX_NODES] = { NULL };
    int nrLocalNuma = 4;

    process.numaAttr.numaNodes = 0b0000001;
    MOCKER(GetNrFreeHugePagesByNode).stubs().will(returnValue((uint64_t)1));
    MOCKER(BuildMigListForDirection).stubs().will(returnValue(-ENOMEM));
    int ret = BuildSwapMigLists(&process, mlist, nrMig, migAddrArray, nrLocalNuma);
    EXPECT_EQ(-ENOMEM, ret);

    GlobalMockObject::verify();
    process.numaAttr.numaNodes = 0b0100001;
    MOCKER(GetNrFreeHugePagesByNode).stubs().will(returnValue((uint64_t)1));
    MOCKER(BuildMigListForDirection).stubs()
        .will(returnValue(0))
        .then(returnValue(-MULTI_NUMA_VM_OOM));
    ret = BuildSwapMigLists(&process, mlist, nrMig, migAddrArray, nrLocalNuma);
    EXPECT_EQ(-MULTI_NUMA_VM_OOM, ret);
}

TEST_F(SeparateStrategyTest, TestBuildMigListForDirection)
{
    MigrateDirection dir;
    uint64_t *migAddrArray[MAX_NODES] = { NULL };
    uint64_t nrMig[MAX_NODES] = { 0 };
    uint64_t destFreeList[MAX_NODES] = { 0 };
    struct MigList mlist[MAX_NODES][MAX_NODES] = {};

    dir = SWAP;
    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    int ret = BuildMigListForDirection(dir, migAddrArray, nrMig, destFreeList, mlist);
    EXPECT_EQ(-EINVAL, ret);

    GlobalMockObject::verify();
    dir = PROMOTE;
    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    ret = BuildMigListForDirection(dir, migAddrArray, nrMig, destFreeList, mlist);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0, mlist[4][0].nr);
}

TEST_F(SeparateStrategyTest, TestBuildMigListForDirectionPromote)
{
    MigrateDirection dir;
    uint64_t *migAddrArray[MAX_NODES] = { NULL };
    uint64_t nrMig[MAX_NODES] = { 0 };
    uint64_t destFreeList[MAX_NODES] = { 0 };
    struct MigList mlist[MAX_NODES][MAX_NODES] = {};
    dir = PROMOTE;
    nrMig[4] = 1;
    migAddrArray[4] = (uint64_t *)calloc(nrMig[4], sizeof(uint64_t));
    destFreeList[0] = 1;

    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    int ret = BuildMigListForDirection(dir, migAddrArray, nrMig, destFreeList, mlist);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, mlist[4][0].nr);
    EXPECT_EQ(0, destFreeList[0]);
    EXPECT_NE(nullptr, mlist[4][0].addr);
    free(migAddrArray[4]);
    free(mlist[4][0].addr);
}

TEST_F(SeparateStrategyTest, TestGroupMigPagesByNode)
{
    LevelActcData *levelActcData[NR_LEVEL] = { NULL };
    uint64_t swapNum = 0;
    uint64_t nrMig[MAX_NODES] = { 0 };
    uint64_t *migAddrArray[MAX_NODES] = { NULL };

    for (int i = 0; i < MAX_NODES; i++) {
        nrMig[i] = 1;
    }
    int ret = GroupMigPagesByNode(levelActcData, swapNum, nrMig, migAddrArray);
    EXPECT_EQ(0, ret);

    swapNum = 1;
    levelActcData[0] = (LevelActcData *)calloc(nrMig[0], sizeof(LevelActcData));
    levelActcData[1] = (LevelActcData *)calloc(nrMig[1], sizeof(LevelActcData));
    levelActcData[0][0].node = 0;
    levelActcData[1][0].node = 5;
    ret = GroupMigPagesByNode(levelActcData, swapNum, nrMig, migAddrArray);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(levelActcData[0][0].addr, migAddrArray[0][0]);
    EXPECT_EQ(levelActcData[1][0].addr, migAddrArray[5][0]);

    for (int i = 0; i < MAX_NODES; i++) {
        if (!migAddrArray[i]) {
            free(migAddrArray[i]);
        }
    }
    free(levelActcData[0]);
    free(levelActcData[1]);
}

extern "C" uint64_t CalMultiNumaVmLowMigrateNum(uint64_t migrateNum, uint64_t freqWt, uint32_t slowThred,
                                                LevelActcData *levelActcData[NR_LEVEL]);
TEST_F(SeparateStrategyTest, TestCalcMultiNumaVmSwapNumByFreq)
{
    ProcessAttr process = {};
    LevelActcData *levelActcData[NR_LEVEL] = { NULL };
    uint64_t levelActcLen[NR_LEVEL] = { 1 };

    for (int i = 0; i < NR_LEVEL; i++) {
        levelActcData[i] = (LevelActcData *)calloc(1, sizeof(LevelActcData));
    }

    process.numaAttr.numaNodes = 0b1000001;
    MOCKER(GetNrFreeHugePagesByNode).stubs().will(returnValue((uint64_t)1));
    MOCKER(CalMultiNumaVmLowMigrateNum).stubs().will(returnValue((uint64_t)1));
    uint64_t ret = CalcMultiNumaVmSwapNumByFreq(&process, levelActcData, levelActcLen);
    EXPECT_EQ(1, ret);
}

TEST_F(SeparateStrategyTest, TestCalMultiNumaVmLowMigrateNum)
{
    uint64_t migrateNum = 1;
    uint64_t freqWt = 0;
    uint32_t slowThred = 0;
    LevelActcData *levelActcData[NR_LEVEL] = { NULL };

    for (int i = 0; i < NR_LEVEL; i++) {
        levelActcData[i] = (LevelActcData *)calloc(1, sizeof(LevelActcData));
    }

    uint64_t ret = CalMultiNumaVmLowMigrateNum(migrateNum, freqWt, slowThred, levelActcData);
    EXPECT_EQ(0, ret);
}

extern "C" int FreqDescFunc(const void *actc1, const void *actc2);
TEST_F(SeparateStrategyTest, TestFreqDescFunc)
{
    LevelActcData *levelActcData_1 = (LevelActcData *)calloc(1, sizeof(LevelActcData));
    LevelActcData *levelActcData_2 = (LevelActcData *)calloc(1, sizeof(LevelActcData));

    // a1->freq < a2->freq
    levelActcData_1->freq = 1;
    levelActcData_2->freq = 2;
    int ret = FreqDescFunc(levelActcData_1, levelActcData_2);
    EXPECT_EQ(1, ret);

    // a1->freq > a2->freq
    levelActcData_1->freq = 2;
    levelActcData_2->freq = 1;
    ret = FreqDescFunc(levelActcData_1, levelActcData_2);
    EXPECT_EQ(-1, ret);

    // a1->freq == a2->freq
    levelActcData_1->freq = 1;
    levelActcData_2->freq = 1;
    levelActcData_1->prior = 3;
    levelActcData_2->prior = 1;
    ret = FreqDescFunc(levelActcData_1, levelActcData_2);
    EXPECT_EQ(2, ret);

    free(levelActcData_1);
    free(levelActcData_2);
}

extern "C" int FreqAscFunc(const void *actc1, const void *actc2);
TEST_F(SeparateStrategyTest, TestFreqAscFunc)
{
    LevelActcData *levelActcData_1 = (LevelActcData *)calloc(1, sizeof(LevelActcData));
    LevelActcData *levelActcData_2 = (LevelActcData *)calloc(1, sizeof(LevelActcData));

    levelActcData_1->freq = 1;
    levelActcData_2->freq = 2;
    int ret = FreqAscFunc(levelActcData_1, levelActcData_2);
    EXPECT_EQ(-1, ret);

    levelActcData_1->freq = 2;
    levelActcData_2->freq = 1;
    ret = FreqAscFunc(levelActcData_1, levelActcData_2);
    EXPECT_EQ(1, ret);

    levelActcData_1->freq = 1;
    levelActcData_2->freq = 1;
    levelActcData_1->prior = 3;
    levelActcData_2->prior = 1;
    ret = FreqAscFunc(levelActcData_1, levelActcData_2);
    EXPECT_EQ(-2, ret);
}

TEST_F(SeparateStrategyTest, TestBuildDemoteMultiNumaMigLists)
{
    uint64_t *migAddrArray[MAX_NODES] = { NULL };
    uint64_t nrMig[MAX_NODES] = { 0 };
    struct MigList mlist[MAX_NODES][MAX_NODES] = {};
    RemoteMigInfo remoteMigInfo[REMOTE_NUMA_NUM] = {};
    remoteMigInfo[0].dir = DEMOTE;
    remoteMigInfo[0].nrMig = 1;

    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    MOCKER(GetNrFreeHugePagesByNode).stubs().will(returnValue(2));
    MOCKER(BuildMigListForDirection).stubs().will(returnValue(-ENOMEM));
    int ret = BuildDemoteMultiNumaMigLists(migAddrArray, nrMig, mlist, remoteMigInfo);
    EXPECT_EQ(-ENOMEM, ret);
}