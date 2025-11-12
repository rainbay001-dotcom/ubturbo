/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: smap5.0 user allocation ut code
 */

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include "manage/manage.h"
#include "strategy/separate_strategy.h"
#include "strategy/strategy.h"
#include "smap_user_log.h"

using namespace std;

class StrategyTest : public ::testing::Test {
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

extern "C" int RunStrategy(ProcessAttr *process, struct MigList mlist[MAX_NODES][MAX_NODES], size_t mlistSize);
TEST_F(StrategyTest, TestRunStrategyOne)
{
    size_t mlistSize = NR_LEVEL;
    ProcessAttr process;
    ActcData actcData = { 0 };
    process.scanAttr.actcData[0] = &actcData;
    struct MigList mlist[MAX_NODES][MAX_NODES] = {};
    MOCKER(IsHugeMode).stubs().will(returnValue(true));
    MOCKER(SeparateStrategy).stubs().will(returnValue(0));
    MOCKER(SeparateStrategy4K).stubs().will(returnValue(0));
    int ret = RunStrategy(&process, mlist, mlistSize);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(StrategyTest, TestRunStrategyTwo)
{
    size_t mlistSize = NR_LEVEL;
    ProcessAttr process;
    ActcData actcData = { 0 };
    process.scanAttr.actcData[0] = &actcData;
    struct MigList mlist[MAX_NODES][MAX_NODES] = {};
    MOCKER(IsHugeMode).stubs().will(returnValue(false));
    MOCKER(SeparateStrategy).stubs().will(returnValue(0));
    MOCKER(SeparateStrategy4K).stubs().will(returnValue(0));
    int ret = RunStrategy(&process, mlist, mlistSize);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(StrategyTest, TestRunStrategyThree)
{
    size_t mlistSize = 0;
    ProcessAttr process;
    struct MigList mlist[MAX_NODES][MAX_NODES] = {};
    MOCKER(SeparateStrategy).stubs().will(returnValue(0));
    MOCKER(SeparateStrategy4K).stubs().will(returnValue(0));
    int ret = RunStrategy(&process, mlist, mlistSize);
    EXPECT_EQ(-EINVAL, ret);
}

extern "C" int snprintf_s(char *strDest, unsigned long destMax, unsigned long count, const char *format, ...);
extern "C" FILE *fopen(const char *__restrict __filename, const char *__restrict __modes);
extern "C" char *fgets(char *__restrict __s, int __n, FILE *__restrict __stream);
extern "C" int fclose(FILE *__stream);
extern "C" int sscanf_s(const char *buffer, const char *format, ...);
TEST_F(StrategyTest, TestGetNrFreePagesByNode)
{
    int nid = 1;
    int ret;
    MOCKER((int (*)(char *, unsigned long, unsigned long, char const *, void *))snprintf_s)
        .stubs()
        .will(returnValue(-1));
    ret = GetNrFreePagesByNode(nid);
    EXPECT_EQ(0, ret);
}

TEST_F(StrategyTest, TestGetNrFreePagesByNodeTwo)
{
    int nid = 1;
    int ret;
    MOCKER((int (*)(char *, unsigned long, unsigned long, char const *, void *))snprintf_s)
        .stubs()
        .will(returnValue(0));
    MOCKER(fopen).stubs().will(returnValue(static_cast<FILE *>(nullptr)));
    ret = GetNrFreePagesByNode(nid);
    EXPECT_EQ(0, ret);
}

TEST_F(StrategyTest, TestGetNrFreePagesByNodeThree)
{
    int nid = 1;
    int ret;
    MOCKER((int (*)(char *, unsigned long, unsigned long, char const *, void *))snprintf_s)
        .stubs()
        .will(returnValue(0));
    static FILE fake_file;
    MOCKER(fopen).stubs().will(returnValue(&fake_file));
    MOCKER(fgets).stubs().will(returnValue(static_cast<char *>(nullptr)));
    MOCKER(fclose).stubs().will(returnValue(0));
    ret = GetNrFreePagesByNode(nid);
    EXPECT_EQ(0, ret);
}

TEST_F(StrategyTest, TestGetNrFreePagesByNodeFour)
{
    int nid = 1;
    int ret;
    MOCKER((int (*)(char *, unsigned long, unsigned long, char const *, void *))snprintf_s)
        .stubs()
        .will(returnValue(0));
    static FILE fake_file;
    MOCKER(fopen).stubs().will(returnValue(&fake_file));
    MOCKER(fgets).stubs().will(returnValue(static_cast<char *>("1"))).then(returnValue((static_cast<char *>(nullptr))));
    MOCKER(fclose).stubs().will(returnValue(-1));
    ret = GetNrFreePagesByNode(nid);
    EXPECT_EQ(0, ret);
}

TEST_F(StrategyTest, TestGetNrFreeHugePagesByNode)
{
    int nid = 1;
    int ret;
    MOCKER((int (*)(char *, unsigned long, unsigned long, char const *, void *))snprintf_s)
        .stubs()
        .will(returnValue(-1));
    ret = GetNrFreeHugePagesByNode(nid);
    EXPECT_EQ(0, ret);
}

TEST_F(StrategyTest, TestGetNrFreeHugePagesByNodeTwo)
{
    int nid = 1;
    int ret;
    MOCKER((int (*)(char *, unsigned long, unsigned long, char const *, void *))snprintf_s)
        .stubs()
        .will(returnValue(0));
    MOCKER(fopen).stubs().will(returnValue(static_cast<FILE *>(nullptr)));
    ret = GetNrFreeHugePagesByNode(nid);
    EXPECT_EQ(0, ret);
}

TEST_F(StrategyTest, TestGetNrFreeHugePagesByNodeThree)
{
    int nid = 1;
    int ret;
    MOCKER((int (*)(char *, unsigned long, unsigned long, char const *, void *))snprintf_s)
        .stubs()
        .will(returnValue(0));
    static FILE fake_file;
    MOCKER(fopen).stubs().will(returnValue(&fake_file));
    MOCKER(fgets).stubs().will(returnValue(static_cast<char *>(nullptr)));
    MOCKER(fclose).stubs().will(returnValue(0));
    ret = GetNrFreeHugePagesByNode(nid);
    EXPECT_EQ(0, ret);
}

TEST_F(StrategyTest, TestGetNrFreeHugePagesByNodeFour)
{
    int nid = 1;
    int ret;
    static FILE fake_file;
    MOCKER((int (*)(char *, unsigned long, unsigned long, char const *, void *))snprintf_s)
        .stubs()
        .will(returnValue(0));
    MOCKER(fopen).stubs().will(returnValue(&fake_file));
    MOCKER(fgets).stubs().will(returnValue(static_cast<char *>("1")));
    MOCKER((int (*)(char const *, char const *, void *))sscanf_s)
        .stubs()
        .will(returnValue(0));
    MOCKER(fclose).stubs().will(returnValue(-1));
    ret = GetNrFreeHugePagesByNode(nid);
    EXPECT_EQ(0, ret);
}
