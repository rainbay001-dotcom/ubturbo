/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.‘
 */

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.h>
#include <mockcpp/mokc.h>
#include <cstdlib>
#include "turbo_conf.h"
#include "turbo_error.h"
#define private public
#include <dlfcn.h>
#include "turbo_plugin_manager.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

// mock
void *dlsym(void *__restrict __handle, const char *__restrict __name);
void *dlopen(const char *__file, int __mode);
int dlclose(void *__handle);

namespace turbo::plugin {

void HandleFuncMock()
{
    return;
}
uint32_t InitFuncMock(const uint16_t)
{
    return TURBO_OK;
}
void DeInitFuncMock(void)
{
    return;
}

using namespace turbo::common;
using namespace turbo::config;
class TestTurboPluginManager : public ::testing::Test {
protected:
    std::string pluginNameMock = "testPlugin";
    std::string fileNameMock = "testFile.so";
    std::string pluginPath = "/tmp";
    const uint16_t moduleCodeMock = 888;
    TurboPluginManager &pMgr = TurboPluginManager::GetInstance();
    void SetUp() override {}
    void TearDown() override
    {
        pMgr.loadedPluginModules.clear();
        GlobalMockObject::verify();
    }
};

TEST_F(TestTurboPluginManager, GetInitFunctionShouldReturnNullWhenHandleIsNull)
{
    std::string funcNameMock = "testFunc";
    pMgr.loadedPluginModules = {{"testPlugin", nullptr}};
    EXPECT_EQ(pMgr.GetInitFunction(pluginNameMock, funcNameMock), nullptr);
}

TEST_F(TestTurboPluginManager, GetInitFunctionShouldReturnNullWhenFuncIsNull)
{
    std::string funcNameMock = "testFunc";
    pMgr.loadedPluginModules[pluginNameMock] = (void *)HandleFuncMock;
    MOCKER(dlsym).stubs().will(returnValue((void *)nullptr));
    EXPECT_EQ(pMgr.GetInitFunction(pluginNameMock, funcNameMock), nullptr);
}

TEST_F(TestTurboPluginManager, InitPluginShouldReturnErrWhenInitFuncIsNull)
{
    MOCKER_CPP(&TurboPluginManager::GetInitFunction,
               TurboPluginInitFunc(*)(const std::string &pluginName, const std::string &funcName))
        .stubs()
        .will(returnValue((TurboPluginInitFunc) nullptr));
    EXPECT_EQ(pMgr.InitPlugin(pluginNameMock, moduleCodeMock), TURBO_ERROR);
}

TEST_F(TestTurboPluginManager, InitPluginShouldReturnErrWhenInitFuncErr)
{
    MOCKER_CPP(&TurboPluginManager::GetInitFunction,
               TurboPluginInitFunc(*)(const std::string &pluginName, const std::string &funcName))
        .stubs()
        .will(returnValue((TurboPluginInitFunc)InitFuncMock));
    MOCKER(InitFuncMock).stubs().will(returnValue(TURBO_ERROR));
    EXPECT_EQ(pMgr.InitPlugin(pluginNameMock, moduleCodeMock), TURBO_ERROR);
}

TEST_F(TestTurboPluginManager, InitPluginShouldReturnOKWhenInitFuncOK)
{
    MOCKER_CPP(&TurboPluginManager::GetInitFunction,
               TurboPluginInitFunc(*)(const std::string &pluginName, const std::string &funcName))
        .stubs()
        .will(returnValue((TurboPluginInitFunc)InitFuncMock));
    EXPECT_EQ(pMgr.InitPlugin(pluginNameMock, moduleCodeMock), TURBO_OK);
}

TEST_F(TestTurboPluginManager, LoadPluginShouldReturnErrWhenPluginHasBeenLoaded)
{
    pMgr.loadedPluginModules[pluginNameMock] = (void *)HandleFuncMock;
    EXPECT_EQ(pMgr.LoadPlugin(pluginNameMock, fileNameMock), TURBO_ERROR);
}

TEST_F(TestTurboPluginManager, LoadPluginShouldReturnErrWhenDlopenIsNull)
{
    MOCKER(dlopen).stubs().will(returnValue((void *)nullptr));
    EXPECT_EQ(pMgr.LoadPlugin(pluginNameMock, pluginPath), TURBO_ERROR);
}

TEST_F(TestTurboPluginManager, LoadPluginShouldReturnErrWhenRealpathIsNull)
{
    MOCKER(dlopen).stubs().will(returnValue((void *)nullptr));
    EXPECT_EQ(pMgr.LoadPlugin(pluginNameMock, fileNameMock), TURBO_ERROR);
}

TEST_F(TestTurboPluginManager, LoadPluginShouldReturnOKWhenDlopenIsNotNull)
{
    MOCKER(dlopen).stubs().will(returnValue((void *)InitFuncMock));
    EXPECT_EQ(pMgr.LoadPlugin(pluginNameMock, pluginPath), TURBO_OK);
}

TEST_F(TestTurboPluginManager, LoadAndInitPluginShouldReturnErrWhenLoadPluginErr)
{
    TurboPluginConf pluginConfMock{.name = pluginNameMock, .soPath = fileNameMock, .moduleCode = moduleCodeMock};
    MOCKER_CPP(&TurboPluginManager::LoadPlugin, RetCode(*)(const std::string &pluginName, const std::string &fileName))
        .stubs()
        .will(returnValue(TURBO_ERROR));
    EXPECT_EQ(pMgr.LoadAndInitPlugin(pluginConfMock), TURBO_ERROR);
}

TEST_F(TestTurboPluginManager, LoadAndInitPluginShouldReturnErrWhenInitPluginErr)
{
    TurboPluginConf pluginConfMock{.name = pluginNameMock, .soPath = fileNameMock, .moduleCode = moduleCodeMock};
    MOCKER_CPP(&TurboPluginManager::LoadPlugin, RetCode(*)(const std::string &pluginName, const std::string &fileName))
        .stubs()
        .will(returnValue(TURBO_OK));
    MOCKER_CPP(&TurboPluginManager::InitPlugin, RetCode(*)(const std::string &pluginName, const uint16_t &moduleCode))
        .stubs()
        .will(returnValue(TURBO_ERROR));
    EXPECT_EQ(pMgr.LoadAndInitPlugin(pluginConfMock), TURBO_ERROR);
}

TEST_F(TestTurboPluginManager, LoadAndInitPluginShouldReturnOK)
{
    TurboPluginConf pluginConfMock{.name = pluginNameMock, .soPath = fileNameMock, .moduleCode = moduleCodeMock};
    MOCKER_CPP(&TurboPluginManager::LoadPlugin, RetCode(*)(const std::string &pluginName, const std::string &fileName))
        .stubs()
        .will(returnValue(TURBO_OK));
    MOCKER_CPP(&TurboPluginManager::InitPlugin, RetCode(*)(const std::string &pluginName, const uint16_t &moduleCode))
        .stubs()
        .will(returnValue(TURBO_OK));
    EXPECT_EQ(pMgr.LoadAndInitPlugin(pluginConfMock), TURBO_OK);
}

TEST_F(TestTurboPluginManager, LoadAndInitPluginsTestSucc)
{
    TurboPluginConf pluginConfMock1{.name = "testPlugin1", .soPath = "testFile1.so", .moduleCode = 888};
    TurboPluginConf pluginConfMock2{.name = "testPlugin2", .soPath = "testFile2.so", .moduleCode = 889};
    TurboPluginConf pluginConfMock3{.name = "testPlugin3", .soPath = "testFile3.so", .moduleCode = 890};
    TurboConfManager &confMgr = TurboConfManager::GetInstance();
    confMgr.allPluginConf = {pluginConfMock1, pluginConfMock2, pluginConfMock3};
    MOCKER_CPP(&TurboPluginManager::LoadAndInitPlugin, RetCode(*)(const TurboPluginConf &pluginConf))
        .stubs()
        .will(returnValue(TURBO_OK));
    pMgr.LoadAndInitPlugins();
}

TEST_F(TestTurboPluginManager, LoadAndInitPluginsTestContinue)
{
    TurboPluginConf pluginConfMock1{.name = "testPlugin1", .soPath = "testFile1.so", .moduleCode = 888};
    TurboPluginConf pluginConfMock2{.name = "testPlugin2", .soPath = "testFile2.so", .moduleCode = 889};
    TurboPluginConf pluginConfMock3{.name = "testPlugin3", .soPath = "testFile3.so", .moduleCode = 890};
    TurboConfManager &confMgr = TurboConfManager::GetInstance();
    confMgr.allPluginConf = {pluginConfMock1, pluginConfMock2, pluginConfMock3};
    MOCKER_CPP(&TurboPluginManager::LoadAndInitPlugin, RetCode(*)(const TurboPluginConf &pluginConf))
        .stubs()
        .will(returnValue(TURBO_ERROR));
    pMgr.LoadAndInitPlugins();
}

TEST_F(TestTurboPluginManager, DeInitializePluginShouldReturnErrWhenPluginNotFound)
{
    pMgr.loadedPluginModules[pluginNameMock] = (void *)HandleFuncMock;
    std::string pluginNameMockNotExist = "NotExistPlugin";
    EXPECT_EQ(pMgr.DeInitializePlugin(pluginNameMockNotExist), TURBO_ERROR);
}

TEST_F(TestTurboPluginManager, DeInitializePluginShouldReturnErrWhenDeInitFoundButDlcloseErr)
{
    pMgr.loadedPluginModules[pluginNameMock] = (void *)HandleFuncMock;
    MOCKER(dlsym).stubs().will(returnValue((void *)DeInitFuncMock));
    MOCKER(dlclose).stubs().will(returnValue(-1));
    EXPECT_EQ(pMgr.DeInitializePlugin(pluginNameMock), TURBO_ERROR);
}

TEST_F(TestTurboPluginManager, DeInitializePluginShouldReturnErrWhenDeInitNotFoundButDlcloseErr)
{
    pMgr.loadedPluginModules[pluginNameMock] = (void *)HandleFuncMock;
    MOCKER(dlsym).stubs().will(returnValue((void *)nullptr));
    MOCKER(dlclose).stubs().will(returnValue(-1));
    EXPECT_EQ(pMgr.DeInitializePlugin(pluginNameMock), TURBO_ERROR);
}

TEST_F(TestTurboPluginManager, DeInitializePluginShouldReturnOKWhenDeInitFoundAndDlcloseOK)
{
    pMgr.loadedPluginModules[pluginNameMock] = (void *)HandleFuncMock;
    MOCKER(dlsym).stubs().will(returnValue((void *)DeInitFuncMock));
    MOCKER(dlclose).stubs().will(returnValue(0));
    EXPECT_EQ(pMgr.DeInitializePlugin(pluginNameMock), TURBO_OK);
}

TEST_F(TestTurboPluginManager, DeInitializePluginsTest)
{
    pMgr.loadedPluginModules[pluginNameMock] = (void *)HandleFuncMock;
    MOCKER(dlsym).stubs().will(returnValue((void *)DeInitFuncMock));
    MOCKER(dlclose).stubs().will(returnValue(0));
    pMgr.DeInitializePlugins();
}

TEST_F(TestTurboPluginManager, InitTest)
{
    EXPECT_EQ(pMgr.Init(), TURBO_OK);
}
} // namespace turbo::plugin
