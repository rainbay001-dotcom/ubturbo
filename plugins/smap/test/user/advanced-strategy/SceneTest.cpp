/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: smap5.0 user scene ut code
 */
 
#include <cstdlib>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include "advanced-strategy/scene_info.h"
#include "advanced-strategy/scene.h"

using namespace std;

class SceneTest : public ::testing::Test {
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

extern "C" int PrevIndex(int index, int len);
TEST_F(SceneTest, TestPrevIndex)
{
    int base = 0, period = 5;
    int ret = PrevIndex(base, period);
    EXPECT_EQ(period - 1, ret);
}

extern "C" int NextIndex(int index, int len);
TEST_F(SceneTest, TestNextIndex)
{
    int period = 5;
    int base = period - 1;
    int ret = NextIndex(base, period);
    EXPECT_EQ(0, ret);
}

extern "C" void IncPageIndex(SceneInfo *info);
TEST_F(SceneTest, TestIncPageIndex)
{
    SceneInfo info = { 0 };
    IncPageIndex(&info);
    EXPECT_EQ(1, info.pageInfoIndex);
}

extern "C" int GetDeltaHot(SceneInfo *info, int currIdx);
TEST_F(SceneTest, TestGetDeltaHot)
{
    SceneInfo info = { 0 };
    info.pageInfo[0].nrHot = 10;
    info.pageInfo[1].nrHot = 5;
    int ret = GetDeltaHot(&info, 1);
    EXPECT_EQ(5, ret);
}

extern "C" bool TryFromStableToUnstable(SceneInfo *info, int idx);
TEST_F(SceneTest, TestTryFromStableToUnstable)
{
    SceneInfo info = { 0 };

    bool ret = TryFromStableToUnstable(&info, 0);
    EXPECT_EQ(false, ret);

    MOCKER(GetDeltaHot).stubs().will(returnValue(100));
    info.pageInfo[0].nrHot = 1000;
    info.pageInfo[0].nrL2Hot = 100;
    ret = TryFromStableToUnstable(&info, 0);
    EXPECT_EQ(true, ret);
}

TEST_F(SceneTest, TestTryFromStableToUnstableLittleDeltaHot)
{
    SceneInfo info = { 0 };
    info.pageInfo[0].nrPages = 1000;

    MOCKER(GetDeltaHot).stubs().will(returnValue(1));
    bool ret = TryFromStableToUnstable(&info, 0);
    EXPECT_EQ(false, ret);
}

extern "C" bool TryToStayInUnstable(SceneInfo *info, int idx);
TEST_F(SceneTest, TestTryToStayInUnstable)
{
    SceneInfo info = { 0 };

    bool ret = TryToStayInUnstable(&info, 0);
    EXPECT_EQ(false, ret);

    MOCKER(GetDeltaHot).stubs().will(returnValue(1000));
    ret = TryToStayInUnstable(&info, 0);
    EXPECT_EQ(true, ret);
}

extern "C" bool IsUnstableScene(SceneInfo *info);
TEST_F(SceneTest, TestIsUnstableScene)
{
    bool ret;
    SceneInfo info = { 0 };
    info.lastScene = LIGHT_STABLE_SCENE;
    ret = IsUnstableScene(&info);
    EXPECT_EQ(false, ret);
    info.lastScene = UNSTABLE_SCENE;
    ret = IsUnstableScene(&info);
    EXPECT_EQ(false, ret);
}

TEST_F(SceneTest, TestIsUnstableSceneTwo)
{
    SceneInfo info = { 0 };
    info.lastScene = UNSTABLE_SCENE;
    info.pageInfoIndex = 0;
    info.pageInfo[0].nrHot = 1000;
    info.pageInfo[0].nrL2Hot = 100;
    MOCKER(TryToStayInUnstable).stubs().will(returnValue(true));
    bool ret = IsUnstableScene(&info);
    EXPECT_EQ(true, ret);

    GlobalMockObject::verify();
    MOCKER(TryToStayInUnstable).stubs().will(returnValue(false));
    ret = IsUnstableScene(&info);
    EXPECT_EQ(false, ret);
}

extern "C" bool IsHeavyLoadScene(SceneInfo *info);
TEST_F(SceneTest, TestIsHeavyLoadScene)
{
    bool ret;
    SceneInfo info = { 0 };
    info.pageInfoIndex = 1;
    info.pageInfo[1].nrL1Guarantee = 61;
    info.pageInfo[1].nrPages = 100;
    ret = IsHeavyLoadScene(&info);
    EXPECT_EQ(true, ret);

    info.pageInfo[0].nrL1Guarantee = 60;
    info.pageInfo[0].nrPages = 100;
    info.pageInfo[1].nrL1Guarantee = 60;
    info.pageInfo[1].nrPages = 100;
    info.pageInfo[7].nrL1Guarantee = 60;
    info.pageInfo[7].nrPages = 100;
    ret = IsHeavyLoadScene(&info);
    EXPECT_EQ(true, ret);
}

extern "C" void AnalyzeScene(SceneInfo *info);
TEST_F(SceneTest, TestAnalyzeScene)
{
    SceneInfo info;
    MOCKER(IsUnstableScene).stubs().will(returnValue(true));
    AnalyzeScene(&info);
    EXPECT_EQ(UNSTABLE_SCENE, info.currScene);

    GlobalMockObject::verify();
    MOCKER(IsUnstableScene).stubs().will(returnValue(false));
    MOCKER(IsHeavyLoadScene).stubs().will(returnValue(true));
    AnalyzeScene(&info);
    EXPECT_EQ(HEAVY_STABLE_SCENE, info.currScene);

    GlobalMockObject::verify();
    MOCKER(IsUnstableScene).stubs().will(returnValue(false));
    MOCKER(IsHeavyLoadScene).stubs().will(returnValue(false));
    AnalyzeScene(&info);
    EXPECT_EQ(LIGHT_STABLE_SCENE, info.currScene);
}

extern "C" int StatsL2AvgHotPages(ProcessAttr *process);
TEST_F(SceneTest, TestStatsL2AvgHotPages)
{
    ProcessAttr process;
    process.sceneInfo.pageInfoIndex = 0;
    process.sceneInfo.pageInfo[0].nrL2Hot = 2;
    process.sceneInfo.pageInfo[7].nrL2Hot = 2;
    process.sceneInfo.pageInfo[6].nrL2Hot = 3;
    int ret = StatsL2AvgHotPages(&process);
    EXPECT_EQ(2, ret);
}

extern "C" int StatsMaxHotPages(ProcessAttr *process);
extern "C" uint32_t StatsMaxGuaranteePages(ProcessAttr *process);
extern "C" void StatsGuaranteePages(ProcessAttr *process);
TEST_F(SceneTest, TestStatsGuaranteePages)
{
    ProcessAttr process;
    process.sceneInfo.pageInfoIndex = 0;
    MOCKER(StatsMaxHotPages).stubs().will(returnValue(0));
    MOCKER(StatsMaxGuaranteePages).stubs().will(returnValue(0));
    process.sceneInfo.pageInfo[0].nrL2Hot = 0;
    process.sceneInfo.pageInfo[0].nrPages = 8;
    StatsGuaranteePages(&process);
    EXPECT_EQ(4, process.sceneInfo.pageInfo[0].nrL1Guarantee);
}

extern "C" void CalcMemInfo(ProcessAttr *process);
TEST_F(SceneTest, TestCalcMemInfo)
{
    ProcessAttr process = {};
    process.numaAttr.numaNodes = 0b00010001;
    process.sceneInfo.pageInfoIndex = 0;

    process.sceneInfo.pageInfo[0].nrL1Page = 3;
    process.scanAttr.actCount[0].freqZero = 1;
    process.scanAttr.actCount[0].pageNum = 3;

    process.sceneInfo.pageInfo[0].nrL2Page = 5;
    process.scanAttr.actCount[4].freqZero = 1;
    process.scanAttr.actCount[4].pageNum = 5;

    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));

    CalcMemInfo(&process);
    EXPECT_EQ(6, process.sceneInfo.pageInfo[0].nrHot);
    EXPECT_EQ(8, process.sceneInfo.pageInfo[0].nrPages);
}

extern "C" int GetProcessSceneAttr(Scene scene, SceneInfo *info);
TEST_F(SceneTest, TestGetProcessSceneAttr)
{
    SceneInfo info = { 0 };
    Scene scene = UNSTABLE_SCENE;
    GetProcessSceneAttr(scene, &info);
    EXPECT_EQ(UNSTABLE_SCAN_CYCLE, info.cycles.scanCycle);
    EXPECT_EQ(UNSTABLE_MIGRATE_CYCLE, info.cycles.migCycle);

    scene = HEAVY_STABLE_SCENE;
    GetProcessSceneAttr(scene, &info);
    EXPECT_EQ(HEAVY_STABLE_SCAN_CYCLE, info.cycles.scanCycle);
    EXPECT_EQ(HEAVY_STABLE_MIGRATE_CYCLE, info.cycles.migCycle);

    scene = LIGHT_STABLE_SCENE;
    GetProcessSceneAttr(scene, &info);
    EXPECT_EQ(LIGHT_STABLE_SCAN_CYCLE, info.cycles.scanCycle);
    EXPECT_EQ(LIGHT_STABLE_MIGRATE_CYCLE, info.cycles.migCycle);
}

extern "C" int InitSceneInfo(SceneInfo *info);
TEST_F(SceneTest, TestInitSceneInfo)
{
    SceneInfo info = { 0 };
    int ret = InitSceneInfo(&info);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(LIGHT_STABLE_SCAN_CYCLE, info.cycles.scanCycle);
    EXPECT_EQ(LIGHT_STABLE_MIGRATE_CYCLE, info.cycles.migCycle);
}

extern "C" int SetProcessSceneAttr(ProcessAttr *process);
TEST_F(SceneTest, TestSetProcessSceneAttr)
{
    int ret = SetProcessSceneAttr(nullptr);
    EXPECT_EQ(-EINVAL, ret);

    ProcessAttr process;
    process.sceneInfo.currScene = LIGHT_STABLE_SCENE;
    process.sceneInfo.lastScene = HEAVY_STABLE_SCENE;
    MOCKER(IncPageIndex).stubs();
    MOCKER(CalcMemInfo).stubs();
    MOCKER(StatsGuaranteePages).stubs();
    MOCKER(AnalyzeScene).stubs();
    ret = SetProcessSceneAttr(&process);
    EXPECT_EQ(0, ret);
}

extern "C" void SetLocalMemStatus(SceneInfo *info);
TEST_F(SceneTest, TestSetLocalMemStatus)
{
    SceneInfo info = { 0 };
    info.pageInfoIndex = 0;
    info.pageInfo[0].nrL1Planed = 1;
    info.pageInfo[0].nrPages = 1;
    SetLocalMemStatus(&info);
    EXPECT_EQ(FULL_SATISFIED, info.status);

    info.pageInfo[0].nrL1Planed = 2;
    info.pageInfo[0].nrL1Guarantee = 1;
    SetLocalMemStatus(&info);
    EXPECT_EQ(SATISFIED, info.status);

    info.pageInfo[0].nrL1Guarantee = 2;
    SetLocalMemStatus(&info);
    EXPECT_EQ(STAY, info.status);

    info.pageInfo[0].nrL1Guarantee = 3;
    SetLocalMemStatus(&info);
    EXPECT_EQ(LESS, info.status);
}

extern "C" int AddList(int arr[], int len);
TEST_F(SceneTest, TestAddList)
{
    int arr[2];
    arr[0] = 1;
    arr[1] = 2;
    int ret = AddList(arr, 2);
    EXPECT_EQ(3, ret);
}

extern "C" void UpdateMemRatio(ProcessAttr *process);
TEST_F(SceneTest, TestUpdateMemRatio)
{
    ProcessAttr process;
    double ret = 70;
    process.numaAttr.numaNodes = 0b00010001;
    process.sceneInfo.pageInfoIndex = 0;
    process.sceneInfo.pageInfo[0].nrL1Planed = 30;
    process.sceneInfo.pageInfo[0].nrPages = 100;
    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    UpdateMemRatio(&process);
    EXPECT_NEAR(ret, process.strategyAttr.l3RemoteMemRatio[0][0], 0.001);
}

extern "C" void ClearList(int arr[], int len);
TEST_F(SceneTest, TestClearList)
{
    int arr[2];
    arr[0] = 1;
    arr[1] = 2;
    ClearList(arr, 1);
    EXPECT_EQ(0, arr[0]);
    EXPECT_EQ(2, arr[1]);
}

extern "C" void DistributeExtraPages(int arr[], int len);
TEST_F(SceneTest, TestDistributeExtraPages)
{
    int arr[4];
    arr[0] = 2;
    arr[1] = 2;
    arr[2] = -1;
    arr[3] = -1;
    DistributeExtraPages(arr, 4);
    EXPECT_EQ(1, arr[0]);
    EXPECT_EQ(1, arr[1]);
    EXPECT_EQ(0, arr[2]);
    EXPECT_EQ(0, arr[3]);
}

TEST_F(SceneTest, TestDistributeExtraPagesTwo)
{
    int arr[3];
    arr[0] = 2;
    arr[1] = 2;
    arr[2] = -1;
    DistributeExtraPages(arr, 3);
    EXPECT_EQ(2, arr[0]);
    EXPECT_EQ(1, arr[1]);
    EXPECT_EQ(0, arr[2]);
}

extern "C" void DistributeInsufficientPages(int arr[], int len);
TEST_F(SceneTest, TestDistributeInsufficientPages)
{
    int arr[4];
    arr[0] = -2;
    arr[1] = -2;
    arr[2] = 1;
    arr[3] = 1;
    DistributeInsufficientPages(arr, 4);
    EXPECT_EQ(-1, arr[0]);
    EXPECT_EQ(-1, arr[1]);
    EXPECT_EQ(0, arr[2]);
    EXPECT_EQ(0, arr[3]);
}

TEST_F(SceneTest, TestDistributeInsufficientPagesTwo)
{
    int arr[3];
    arr[0] = 2;
    arr[1] = -2;
    arr[2] = -1;
    DistributeInsufficientPages(arr, 3);
    EXPECT_EQ(0, arr[0]);
    EXPECT_EQ(-1, arr[1]);
    EXPECT_EQ(0, arr[2]);
}

extern "C" void BalanceSurpluses(int arr[], int len);
TEST_F(SceneTest, TestBalanceSurpluses)
{
    int arr[4] = { 0 };
    arr[0] = 1;
    arr[1] = -1;
    BalanceSurpluses(arr, 4);
    EXPECT_EQ(0, arr[0]);

    arr[0] = 1;
    arr[1] = 0;
    MOCKER(DistributeExtraPages).stubs().with(outBoundP(arr, sizeof(arr)), any());
    BalanceSurpluses(arr, 4);

    arr[2] = -3;
    MOCKER(DistributeInsufficientPages).stubs().with(outBoundP(arr, sizeof(arr)), any());
    BalanceSurpluses(arr, 4);
}

extern "C" bool g_adaptLocalMem;
extern "C" void SetAdaptMem(bool flag);
TEST_F(SceneTest, TestSetAdaptMem)
{
    g_adaptLocalMem = true;
    SetAdaptMem(false);
    EXPECT_EQ(false, g_adaptLocalMem);
}
extern "C" bool IsReadyForAdapt(ProcessAttr *attr);
extern "C" void AdjustVmMemRatio(struct ProcessManager *manager, int *surpluses, int len);
TEST_F(SceneTest, TestAdjustVmMemRatio)
{
    int arr[2];
    int len = 1;
    struct ProcessManager manager;
    ProcessAttr current;
    current.next = NULL;
    manager.processes = &current;
    manager.nr[1] = 1;
    arr[0] = 1;
    arr[1] = 2;
    MOCKER(BalanceSurpluses).stubs();
    MOCKER(SetLocalMemStatus).stubs();
    MOCKER(IsReadyForAdapt).stubs().will(returnValue(true));
    current.sceneInfo.pageInfoIndex = 0;
    current.sceneInfo.pageInfo[0].nrL1Guarantee = 2;
    current.sceneInfo.status = FULL_SATISFIED;
    MOCKER(UpdateMemRatio).stubs();
    AdjustVmMemRatio(&manager, arr, len);
    EXPECT_EQ(LIGHT_STABLE_SCENE, current.sceneInfo.currScene);
    EXPECT_EQ(3, current.sceneInfo.pageInfo[0].nrL1Planed);

    current.sceneInfo.status = SATISFIED;
    current.sceneInfo.currScene = UNSTABLE_SCENE;
    AdjustVmMemRatio(&manager, arr, len);
    EXPECT_EQ(HEAVY_STABLE_SCENE, current.sceneInfo.currScene);
}

TEST_F(SceneTest, TestIsReadyForAdapt)
{
    bool ret;
    ProcessAttr attr;
    attr.adaptMem.enableAdaptMem = false;
    ret = IsReadyForAdapt(&attr);
    EXPECT_EQ(false, ret);

    attr.adaptMem.enableAdaptMem = true;
    attr.scanAttr.actcData[0] = NULL;
    ret = IsReadyForAdapt(&attr);
    EXPECT_EQ(false, ret);
}

TEST_F(SceneTest, TestIsReadyForAdaptTwo)
{
    bool ret;
    ProcessAttr attr;
    ActcData actcData;
    attr.adaptMem.enableAdaptMem = true;
    attr.scanAttr.actcData[0] = &actcData;
    g_adaptLocalMem = false;
    ret = IsReadyForAdapt(&attr);
    EXPECT_EQ(false, ret);

    g_adaptLocalMem = true;
    attr.type = PROCESS_TYPE;
    ret = IsReadyForAdapt(&attr);
    EXPECT_EQ(false, ret);
}

extern "C" void ConfigMultiVmRatio(struct ProcessManager *manager);
TEST_F(SceneTest, TestConfigMultiVmRatio)
{
    struct ProcessManager manager;
    manager.nr[VM_TYPE] = 0;
    ConfigMultiVmRatio(&manager);

    manager.nr[VM_TYPE] = 1;
    ProcessAttr current;
    manager.processes = &current;
    current.next = NULL;
    MOCKER(IsReadyForAdapt).stubs().will(returnValue(true));
    current.sceneInfo.pageInfoIndex = 0;
    current.sceneInfo.pageInfo[0].nrPages = 10;
    current.strategyAttr.l3RemoteMemRatio[0][0] = 100;
    current.sceneInfo.pageInfo[0].nrL1Guarantee = 11;
    MOCKER(AdjustVmMemRatio).stubs();
    ConfigMultiVmRatio(&manager);
    EXPECT_EQ(100, current.strategyAttr.l3RemoteMemRatio[0][0]);
}

extern "C" void GetMaxNuma(struct ProcessManager *manager, int *maxL1node, int *maxL2node);
TEST_F(SceneTest, TestGetMaxNuma)
{
    ProcessAttr attr = {};
    attr.numaAttr.numaNodes = 0b10011001;
    struct ProcessManager manager = {};
    manager.processes = &attr;
    int maxL1node = 0;
    int maxL2node = 0;

    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    GetMaxNuma(&manager, &maxL1node, &maxL2node);
    EXPECT_EQ(0, maxL1node);
    EXPECT_EQ(4, maxL2node);
}

extern "C" void ConfigMultiVmRatioInGroups(struct ProcessManager *manager);
TEST_F(SceneTest, TestConfigMultiVmRatioInGroups)
{
    struct ProcessManager *manager = GetProcessManager();
    ProcessAttr current;
    current.next = nullptr;
    current.numaAttr.numaNodes = 31;
    manager->processes = &current;
    MOCKER(ConfigMultiVmRatio).stubs();
    ConfigMultiVmRatioInGroups(manager);
    EXPECT_EQ(false, current.adaptMem.enableAdaptMem);
}

TEST_F(SceneTest, TestConfigMultiVmRatioInGroupsTwo)
{
    struct ProcessManager *manager = GetProcessManager();
    ProcessAttr current;
    current.next = nullptr;
    current.scanType = NORMAL_SCAN;
    current.numaAttr.numaNodes = 0b00010001;
    manager->processes = &current;
    MOCKER(GetNrLocalNuma).stubs().will(returnValue(4));
    MOCKER(ConfigMultiVmRatio).stubs();
    EXPECT_EQ(false, current.adaptMem.enableAdaptMem);
}

extern "C" void ConfigMultiProcessRatio(struct ProcessManager *manager);
TEST_F(SceneTest, TestConfigMultiProcessRatio)
{
    struct ProcessManager manager = {};
    ProcessAttr current = {};
    manager.processes = &current;
    current.next = NULL;
    current.scanType = NORMAL_SCAN;
    current.strategyAttr.l2RemoteMemRatio[0][0] = 30;
    current.strategyAttr.l3RemoteMemRatio[0][0] = 10;

    ConfigMultiProcessRatio(&manager);
    EXPECT_EQ(30, current.strategyAttr.l3RemoteMemRatio[0][0]);
}

extern "C" void ConfigRatios(struct ProcessManager *manager);
TEST_F(SceneTest, TestConfigRatios)
{
    struct ProcessManager manager = {};
    ProcessAttr current = {};
    manager.processes = &current;
    manager.tracking.pageSize = PAGESIZE_4K;
    current.next = NULL;
    current.scanType = NORMAL_SCAN;
    current.strategyAttr.l2RemoteMemRatio[0][0] = 30;
    current.strategyAttr.l3RemoteMemRatio[0][0] = 10;

    ConfigMultiProcessRatio(&manager);
    EXPECT_EQ(30, current.strategyAttr.l3RemoteMemRatio[0][0]);
}

extern "C" bool GetAdaptMem(void);
TEST_F(SceneTest, TetsGetAdaptMem)
{
    g_adaptLocalMem = true;
    bool ret = GetAdaptMem();
    EXPECT_EQ(ret, true);

    g_adaptLocalMem = false;
    ret = GetAdaptMem();
    EXPECT_EQ(ret, false);
}

extern "C" uint32_t StatsMaxGuaranteePages(ProcessAttr *process);
TEST_F(SceneTest,  TestStatsMaxGuaranteePages)
{
    ProcessAttr attr = {};
    attr.sceneInfo.pageInfoIndex = 0;
    attr.sceneInfo.pageInfo[0].nrL1GuaranteeBk = 10;
    MOCKER(PrevIndex).stubs().will(returnValue(0));
    uint32_t ret =  StatsMaxGuaranteePages(&attr);
    EXPECT_EQ(10, ret);
}

extern "C" int StatsMaxHotPages(ProcessAttr *process);
TEST_F(SceneTest,  TestStatsMaxHotPages)
{
    ProcessAttr attr = {};
    attr.sceneInfo.pageInfoIndex = 0;
    attr.sceneInfo.pageInfo[0].nrHot = 20;
    MOCKER(PrevIndex).stubs().will(returnValue(0));
    int ret = StatsMaxHotPages(&attr);
    EXPECT_EQ(20, ret);
}
