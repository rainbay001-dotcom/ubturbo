/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.‘
 */
#include <gmock/gmock.h>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include "turbo_rmrs_plugin.cpp"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))
namespace rmrs {
// 测试类
class TestTurboRmrsPlugin : public ::testing::Test {
protected:
    void SetUp() override
    {
    }
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(TestTurboRmrsPlugin, TurboPluginInitFailed1)
{
    MOCKER_CPP(&RmrsConfig::Init, RmrsResult(*)(const uint16_t))
        .stubs()
        .will(returnValue(1));
    auto ret = TurboPluginInit(0);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestTurboRmrsPlugin, TurboPluginInitFailed2)
{
    MOCKER_CPP(&RmrsConfig::Init, RmrsResult(*)(const uint16_t))
        .stubs()
        .will(returnValue(0));
    MOCKER(CallbackManager::Init).stubs().will(returnValue(1));
    auto ret = TurboPluginInit(0);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestTurboRmrsPlugin, TurboPluginInitFailed3)
{
    MOCKER_CPP(&RmrsConfig::Init, RmrsResult(*)(const uint16_t))
        .stubs()
        .will(returnValue(0));
    MOCKER(CallbackManager::Init).stubs().will(returnValue(0));
    MOCKER(rmrs::exports::ResourceExport::Init).stubs().will(returnValue(1));
    auto ret = TurboPluginInit(0);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestTurboRmrsPlugin, TurboPluginInitFailed4)
{
    MOCKER_CPP(&RmrsConfig::Init, RmrsResult(*)(const uint16_t))
        .stubs()
        .will(returnValue(0));
    MOCKER(CallbackManager::Init).stubs().will(returnValue(0));
    MOCKER(rmrs::exports::ResourceExport::Init).stubs().will(returnValue(0));
    MOCKER(SmapModule::Init).stubs().will(returnValue(1));
    auto ret = TurboPluginInit(0);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestTurboRmrsPlugin, TurboPluginInitSucceed)
{
    MOCKER_CPP(&RmrsConfig::Init, RmrsResult(*)(const uint16_t))
        .stubs()
        .will(returnValue(0));
    MOCKER(CallbackManager::Init).stubs().will(returnValue(0));
    MOCKER(rmrs::exports::ResourceExport::Init).stubs().will(returnValue(0));
    MOCKER(SmapModule::Init).stubs().will(returnValue(0));
    MOCKER_CPP(&RmrsReboot, void(*)())
        .stubs()
        .will(ignoreReturnValue());
    auto ret = TurboPluginInit(0);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestTurboRmrsPlugin, TurboPluginDeInitSucceed)
{
    EXPECT_NO_THROW(TurboPluginDeInit());
}

TEST_F(TestTurboRmrsPlugin, TurboPluginConfigInitSucceed)
{
    auto ret = RmrsConfig::Instance().Init(0);
    EXPECT_EQ(ret, RMRS_OK);
}

TEST_F(TestTurboRmrsPlugin, RmrsRebootFailed1)
{
    MOCKER_CPP((bool(*)(const std::filesystem::path &))(std::filesystem::exists),
        bool(*)(const std::filesystem::path &))
        .stubs().will(returnValue(false));
    EXPECT_NO_THROW(RmrsReboot());
}

TEST_F(TestTurboRmrsPlugin, RmrsRebootFailed2)
{
    MOCKER_CPP((bool(*)(const std::filesystem::path &))(std::filesystem::exists),
        bool(*)(const std::filesystem::path &))
        .stubs().will(returnValue(true));
    MOCKER_CPP((void(std::ifstream::*)(const std::string &, std::ios_base::openmode))(&std::ifstream::open),
        void(*)(std::ifstream*, const std::string &, std::ios_base::openmode))
        .stubs().will(ignoreReturnValue());
    MOCKER_CPP((bool(std::ifstream::*)())(&std::ifstream::is_open),
        bool(*)(std::ifstream*)).stubs().will(returnValue(false));
    EXPECT_NO_THROW(RmrsReboot());
}

std::ifstream g_fin;

static void ifstreamOpenForTest(std::ifstream *module, const std::string &, std::ios_base::openmode)
{
    *module = std::move(g_fin);
    return ;
}

TEST_F(TestTurboRmrsPlugin, RmrsRebootFailed3)
{
    rmrs::serialization::MigrateStrategyResult migrateStrategyResult;
    rmrs::serialization::VMMigrateOutParam param;
    migrateStrategyResult.vmInfoList.push_back(param);

    RmrsOutStream builder;
    builder << migrateStrategyResult;
    TurboByteBuffer req;
    req = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};

    std::string path = "/tmp/record_for_test.txt";
    std::ofstream fout;
    fout.open(path, std::ios::out | std::ios::binary);
    std::string text((char *)builder.GetBufferPointer(), builder.GetSize());
    fout << text;
    fout.close();
    g_fin.open(path, std::ios::in | std::ios::binary);

    MOCKER_CPP((bool(*)(const std::filesystem::path &))(std::filesystem::exists),
        bool(*)(const std::filesystem::path &))
        .stubs().will(returnValue(true));
    MOCKER_CPP((void(std::ifstream::*)(const std::string &, std::ios_base::openmode))(&std::ifstream::open),
               void (*)(std::ifstream *, const std::string &, std::ios_base::openmode))
        .stubs()
        .will(invoke(ifstreamOpenForTest));
    MOCKER_CPP(&RmrsSmapHelper::MigrateColdDataToRemoteNumaSync, RmrsResult(*)(std::vector<uint16_t> &,
        std::vector<pid_t> &, std::vector<uint64_t>, uint64_t waitTime)).stubs().will(returnValue(0));
    MOCKER_CPP((void(std::ofstream::*)(const std::string &, std::ios_base::openmode))(&std::ofstream::open),
        void(*)(std::ofstream*, const std::string &, std::ios_base::openmode))
        .stubs().will(ignoreReturnValue());
    MOCKER_CPP((void(std::ofstream::*)())(&std::ofstream::close), void(*)(std::ofstream*))
        .stubs().will(ignoreReturnValue());
    EXPECT_NO_THROW(RmrsReboot());
    delete[] req.data;
}

} // namespace rmrs
