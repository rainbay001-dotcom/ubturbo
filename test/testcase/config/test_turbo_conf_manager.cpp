/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.‘
 */

#include <gmock/gmock.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#define private public
#include "turbo_conf_manager.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

using namespace testing;
using namespace turbo::config;
using namespace turbo::common;

void genTestFile()
{
    std::string tmpPath = "/tmp/ubturbo_ut";
    std::string confPath = "/tmp/ubturbo_ut/conf";
    std::string libPath = "/tmp/ubturbo_ut/lib";
    try {
        if (std::filesystem::create_directory(tmpPath) && std::filesystem::create_directory(confPath) &&
            std::filesystem::create_directory(libPath)) {
            std::cout << "Directory created successfully." << std::endl;
        } else {
            std::cout << "Directory created failed." << std::endl;
        }
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "File sys error: " << e.what() << std::endl;
    }

    std::ofstream turboConfFile(confPath + "/ubturbo.conf");
    std::ofstream admissionConfFile(confPath + "/ubturbo_plugin_admission.conf");
    std::ofstream demoConfFile(confPath + "/plugin_demo.conf");
    std::ofstream soFile(libPath + "/libdemo_ubturbo_plugin.so");
    if (turboConfFile.is_open() && admissionConfFile.is_open() && demoConfFile.is_open() && soFile.is_open()) {
        turboConfFile << "log.level=INFO" << std::endl;
        admissionConfFile << "demo=200" << std::endl;
        demoConfFile << "turbo.plugin.name=demo" << std::endl;
        demoConfFile << "turbo.plugin.pkg=libdemo_ubturbo_plugin.so" << std::endl;
        soFile << "mock so" << std::endl;
    } else {
        std::cout << "open file failed." << std::endl;
    }
    turboConfFile.close();
    admissionConfFile.close();
    demoConfFile.close();
    soFile.close();
}

void delTestFile()
{
    std::string tmpPath = "/tmp/ubturbo_ut";
    try {
        std::filesystem::remove_all(tmpPath);
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "Delete Error." << e.what() << std::endl;
    }
}

class TestTurboConf : public ::testing::Test {
protected:
    void SetUp() override
    {
        std::cout << "[Phase SetUp Begin]" << std::endl;
        std::cout << "[Phase SetUp End]" << std::endl;
    }
    void TearDown() override
    {
        std::cout << "[Phase TearDown Begin]" << std::endl;
        GlobalMockObject::verify();
        std::cout << "[Phase TearDown End]" << std::endl;
    }
};

TEST_F(TestTurboConf, InitSuccessTEST)
{
    genTestFile();

    TurboConfManager mgr;
    const std::string config = "/tmp/ubturbo_ut/conf";
    const std::string libDir = "/tmp/ubturbo_ut/lib";
    RetCode ret = mgr.Init(config, libDir);
    EXPECT_EQ(ret, TURBO_OK);

    MOCKER_CPP(&TurboConfManager::CheckPluginNameAndCode, RetCode(*)(void *)).stubs().will(returnValue(TURBO_ERROR));
    ret = mgr.Init(config, libDir);
    EXPECT_EQ(ret, TURBO_ERROR);

    MOCKER_CPP(&TurboConfManager::ParseFile, RetCode(*)(void *)).stubs().will(returnValue(TURBO_ERROR));
    ret = mgr.Init(config, libDir);
    EXPECT_EQ(ret, TURBO_ERROR);
    delTestFile();
}

TEST_F(TestTurboConf, InitFailedTEST)
{
    genTestFile();

    TurboConfManager mgr;
    const std::string config = "/tmp/conf";
    const std::string libDir = "/tmp/lib";
    RetCode ret = mgr.Init(config, libDir);
    EXPECT_EQ(ret, TURBO_ERROR);

    delTestFile();
}

TEST_F(TestTurboConf, InitPluginConfTEST)
{
    TurboConfManager mgr;
    const std::string pluginConf = "/tmp/conf";
    RetCode ret = mgr.InitPluginConf(pluginConf);
    EXPECT_EQ(ret, TURBO_ERROR);
}

TEST_F(TestTurboConf, ParseFileSuccessTEST)
{
    TurboConfManager mgr;
    const std::string filePath = "/proc/version";
    std::unordered_map<std::string, std::string> confMap;
    MOCKER_CPP(&TurboConfManager::ParseLine, RetCode(*)(void *)).stubs().will(returnValue(TURBO_OK));
    RetCode ret = mgr.ParseFile(filePath, confMap);
    EXPECT_EQ(ret, TURBO_OK);
}

TEST_F(TestTurboConf, ParseFileFailedTEST)
{
    TurboConfManager mgr;
    const std::string filePath = "/proc/version";
    const std::string filePath1 = "";
    std::unordered_map<std::string, std::string> confMap;
    MOCKER_CPP(&TurboConfManager::ParseLine, RetCode(*)(void *)).stubs().will(returnValue(TURBO_ERROR));
    RetCode ret = mgr.ParseFile(filePath, confMap);
    EXPECT_EQ(ret, TURBO_ERROR);

    ret = mgr.ParseFile(filePath1, confMap);
    EXPECT_EQ(ret, TURBO_ERROR);
}

TEST_F(TestTurboConf, ParseLineCommentSuccessTEST)
{
    TurboConfManager mgr;
    std::string line = "#";
    size_t lineCount = 1;
    std::string key;
    std::string val;
    RetCode ret = mgr.ParseLine(line, lineCount, key, val);
    EXPECT_EQ(ret, TURBO_OK);
}

TEST_F(TestTurboConf, ParseLineSuccessTEST)
{
    TurboConfManager mgr;
    std::string line = "12345";
    size_t lineCount = 1;
    std::string key;
    std::string val;
    RetCode ret = mgr.ParseLine(line, lineCount, key, val);
    EXPECT_EQ(ret, TURBO_OK);
}

TEST_F(TestTurboConf, ParseLineCountFailedTEST)
{
    TurboConfManager mgr;
    std::string line = "#";
    size_t lineCount = 1001;
    std::string key;
    std::string val;
    RetCode ret = mgr.ParseLine(line, lineCount, key, val);
    EXPECT_EQ(ret, TURBO_ERROR);
}

TEST_F(TestTurboConf, ParseInvaildLineFailedTEST)
{
    TurboConfManager mgr;
    std::string line = "=45";
    size_t lineCount = 1;
    std::string key;
    std::string val;
    RetCode ret = mgr.ParseLine(line, lineCount, key, val);
    EXPECT_EQ(ret, TURBO_ERROR);
}

TEST_F(TestTurboConf, ParseInvaildKeyFailedTEST)
{
    TurboConfManager mgr;
    std::string line = "123@4=5";
    size_t lineCount = 1;
    std::string key;
    std::string val;
    RetCode ret = mgr.ParseLine(line, lineCount, key, val);
    EXPECT_EQ(ret, TURBO_ERROR);
}

TEST_F(TestTurboConf, ParseInvaildValFailedTEST)
{
    TurboConfManager mgr;
    std::string line = "123=4+t";
    size_t lineCount = 1;
    std::string key;
    std::string val;
    RetCode ret = mgr.ParseLine(line, lineCount, key, val);
    EXPECT_EQ(ret, TURBO_ERROR);
}

TEST_F(TestTurboConf, GetConfSuccessTEST)
{
    TurboConfManager mgr;
    const std::string section = "a";
    const std::string configKey = "b";
    std::string configVal = "c";
    mgr.lineContentMap.insert({"a@b", "c"});
    RetCode ret = mgr.GetConf(section, configKey, configVal);
    EXPECT_EQ(ret, TURBO_OK);
}

TEST_F(TestTurboConf, GetConfFailedTEST)
{
    TurboConfManager mgr;
    const std::string section = "a";
    const std::string configKey = "b";
    std::string configVal = "c";
    mgr.lineContentMap.insert({"a@d", "c"});
    RetCode ret = mgr.GetConf(section, configKey, configVal);
    EXPECT_EQ(ret, TURBO_ERROR);
}

TEST_F(TestTurboConf, GetAllConfSuccessTEST)
{
    TurboConfManager mgr;
    mgr.lineContentMap.insert({"a", "b"});
    std::unordered_map<std::string, std::string> ret = mgr.GetAllConf();
    EXPECT_EQ(ret.size(), 1);
}

TEST_F(TestTurboConf, ClearSuccessTEST)
{
    TurboConfManager mgr;
    mgr.lineContentMap.insert({"a", "b"});
    mgr.Clear();
    EXPECT_EQ(mgr.lineContentMap.size(), 0);
}

TEST_F(TestTurboConf, GetAllPluginConfSuccessTEST)
{
    TurboConfManager mgr;
    TurboPluginConf testConf = {.name = "a", .moduleCode = 201};
    mgr.allPluginConf.push_back(testConf);
    std::vector<TurboPluginConf> ret = mgr.GetAllPluginConf();
    EXPECT_EQ(ret.size(), 1);
}

TEST_F(TestTurboConf, CheckPluginNameAndCodeSuccessTEST)
{
    TurboConfManager mgr;
    const std::string pluginName = "test";
    uint16_t moduleCode = 200;
    TurboPluginConf testConf = {.name = "a", .moduleCode = 201};
    mgr.allPluginConf.push_back(testConf);
    RetCode ret = mgr.CheckPluginNameAndCode(pluginName, moduleCode);
    EXPECT_EQ(ret, TURBO_OK);
}

TEST_F(TestTurboConf, CheckPluginNameAndCodeEmptyTEST)
{
    TurboConfManager mgr;
    const std::string pluginName = "";
    uint16_t moduleCode = 200;
    TurboPluginConf testConf = {.name = "a", .moduleCode = 201};
    mgr.allPluginConf.push_back(testConf);
    RetCode ret = mgr.CheckPluginNameAndCode(pluginName, moduleCode);
    EXPECT_EQ(ret, TURBO_ERROR);
}

TEST_F(TestTurboConf, CheckPluginNameAndCodeCodeFailedTEST)
{
    TurboConfManager mgr;
    const std::string pluginName = "test";
    uint16_t moduleCode = 199;
    TurboPluginConf testConf = {.name = "a", .moduleCode = 200};
    mgr.allPluginConf.push_back(testConf);
    RetCode ret = mgr.CheckPluginNameAndCode(pluginName, moduleCode);
    EXPECT_EQ(ret, TURBO_ERROR);
}

TEST_F(TestTurboConf, CheckPluginNameAndCodeNameRepeatTEST)
{
    TurboConfManager mgr;
    const std::string pluginName = "test";
    uint16_t moduleCode = 200;
    TurboPluginConf testConf = {.name = "test", .moduleCode = 201};
    mgr.allPluginConf.push_back(testConf);
    RetCode ret = mgr.CheckPluginNameAndCode(pluginName, moduleCode);
    EXPECT_EQ(ret, TURBO_ERROR);
}

TEST_F(TestTurboConf, CheckPluginNameAndCodeCodeRepeatTEST)
{
    TurboConfManager mgr;
    const std::string pluginName = "test";
    uint16_t moduleCode = 200;
    TurboPluginConf testConf = {.name = "a", .moduleCode = 200};
    mgr.allPluginConf.push_back(testConf);
    RetCode ret = mgr.CheckPluginNameAndCode(pluginName, moduleCode);
    EXPECT_EQ(ret, TURBO_ERROR);
}

TEST_F(TestTurboConf, LeftTrimSuccessTEST)
{
    std::string line = " a=b";
    LeftTrim(line);
    std::string expect = "a=b";
    EXPECT_EQ(line, expect);
}

TEST_F(TestTurboConf, RightTrimSuccessTEST)
{
    std::string line = "a=b ";
    RightTrim(line);
    std::string expect = "a=b";
    EXPECT_EQ(line, expect);
}

TEST_F(TestTurboConf, ParseConfigKeyAndValueSuccessTEST)
{
    std::string line = "a=b";
    std::pair<std::string, std::string> ret = ParseConfigKeyAndValue(line);
    std::pair<std::string, std::string> expect = std::make_pair("a", "b");
    EXPECT_EQ(ret, expect);
}