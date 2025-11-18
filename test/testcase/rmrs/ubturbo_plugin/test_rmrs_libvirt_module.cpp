/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.‘
 */

#include <gmock/gmock.h>
#include <dlfcn.h>
#include <cstring>
#include <iostream>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include "rmrs_libvirt_module.h"


#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

using namespace std;

namespace rmrs::libvirt {
// 测试类
class TestRmrsLibvirtModule : public ::testing::Test {
protected:

    TestRmrsLibvirtModule() {}

    void SetUp() override
    {
        cout << "[TestRmrsLibvirtModule SetUp Begin]" << endl;
        cout << "[TestRmrsLibvirtModule SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[TestRmrsLibvirtModule TearDown Begin]" << endl;
        LibvirtModule::virConnectOpenFunc = nullptr;
        LibvirtModule::virConnectCloseFunc = nullptr;
        LibvirtModule::virConnectListAllDomainsFunc = nullptr;
        LibvirtModule::virDomainGetNameFunc = nullptr;
        LibvirtModule::virDomainGetIDFunc = nullptr;
        LibvirtModule::virDomainGetUUIDStringFunc = nullptr;
        LibvirtModule::virDomainGetInfoFunc = nullptr;
        LibvirtModule::virDomainGetVcpusFunc = nullptr;
        LibvirtModule::virConnectGetHostnameFunc = nullptr;
        LibvirtModule::virDomainFreeFunc = nullptr;
        LibvirtModule::virConnectIsAliveFunc = nullptr;
        LibvirtModule::virEventRegisterDefaultImplFunc = nullptr;
        LibvirtModule::virEventRunDefaultImplFunc = nullptr;
        LibvirtModule::virConnectDomainEventRegisterFunc = nullptr;
        LibvirtModule::virConnectDomainEventDeRegisterFunc = nullptr;

        GlobalMockObject::verify();
        cout << "[TestRmrsLibvirtModule TearDown End]" << endl;
    }
};

unsigned int MockVirDomainGetIDFunc(void *name)
{
    return 0;
}

int MockVirDomainGetUUIDStringFunc(void * a, char *b)
{
    return 0;
}

const char *MockVirDomainGetNameFunc(void *name)
{
    return nullptr;
}

int MockVirConnectListAllDomainsFunc(void *x, void ***y, VirConnectListAllDomainsFlags z)
{
    return 0;
}

int MockVirConnectCloseFunc(void* name)
{
    return 0; // 可以返回任意 void* 值
}

int MockVirDomainGetInfoFunc(void *x, void *y)
{
    return 0;
}

int MockVirDomainGetVcpusFunc(void *a, void *b, int c, unsigned char *d, int e)
{
    return 0;
}

char* MockVirConnectGetHostnameFunc(void *name)
{
    return nullptr;
}

int MockVirDomainFreeFunc(void *name)
{
    return 0;
}

int MockVirEventRegisterDefaultImplFunc()
{
    return 0;
}

int MockVirEventRunDefaultImplFunc()
{
    return 0;
}

int MockVirConnectDomainEventRegisterFunc(void *a, VirConnectDomainEventCallbackFunc b, void *c, void *d)
{
    return 0;
}

int MockVirConnectDomainEventDeRegisterFunc(void *a, VirConnectDomainEventCallbackFunc b)
{
    return 0;
}

TEST_F(TestRmrsLibvirtModule, VirConnectCloseFailWithNullptr)
{
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));
    LibvirtModule obj;
    auto res = obj.VirConnectClose();
    ASSERT_EQ(res, nullptr);
}

TEST_F(TestRmrsLibvirtModule, VirConnectCloseSucceed)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirConnectCloseFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(expectedReturn));
    LibvirtModule obj;
    auto res = obj.VirConnectClose();
    ASSERT_EQ(res, expectedReturn);
}

TEST_F(TestRmrsLibvirtModule, VirConnectCloseSucceedWithSecondCall)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirConnectCloseFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(expectedReturn));
    LibvirtModule obj;
    obj.VirConnectClose();
    auto res = obj.VirConnectClose();
    ASSERT_EQ(res, expectedReturn);
}

TEST_F(TestRmrsLibvirtModule, VirConnectListAllDomainsFuncFailWithNullptr)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirConnectListAllDomainsFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));
    LibvirtModule obj;
    auto res = obj.VirConnectListAllDomains();
    ASSERT_EQ(res, nullptr);
}

TEST_F(TestRmrsLibvirtModule, VirConnectListAllDomainsFuncSucceed)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirConnectListAllDomainsFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(expectedReturn));
    LibvirtModule obj;
    auto res = obj.VirConnectListAllDomains();
    ASSERT_EQ(res, expectedReturn);
}

TEST_F(TestRmrsLibvirtModule, VirConnectListAllDomainsFuncSucceedWithSecondCall)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirConnectListAllDomainsFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(expectedReturn));
    LibvirtModule obj;
    obj.VirConnectListAllDomains();
    auto res = obj.VirConnectListAllDomains();
    ASSERT_EQ(res, expectedReturn);
}

TEST_F(TestRmrsLibvirtModule, VirDomainGetNameFuncFailWithNullptr)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirDomainGetNameFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));
    LibvirtModule obj;
    auto res = obj.VirDomainGetName();
    ASSERT_EQ(res, nullptr);
}

TEST_F(TestRmrsLibvirtModule, VirDomainGetNameFuncSucceed)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirDomainGetNameFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(expectedReturn));
    LibvirtModule obj;
    auto res = obj.VirDomainGetName();
    ASSERT_EQ(res, expectedReturn);
}

TEST_F(TestRmrsLibvirtModule, VirDomainGetNameFuncSucceedWithSecondCall)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirDomainGetNameFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(expectedReturn));
    LibvirtModule obj;
    obj.VirDomainGetName();
    auto res = obj.VirDomainGetName();
    ASSERT_EQ(res, expectedReturn);
}

TEST_F(TestRmrsLibvirtModule, VirDomainGetIDFuncFailWithNullptr)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirDomainGetIDFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));
    LibvirtModule obj;
    auto res = obj.VirDomainGetID();
    ASSERT_EQ(res, nullptr);
}

TEST_F(TestRmrsLibvirtModule, VirDomainGetIDFuncSucceed)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirDomainGetIDFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(expectedReturn));
    LibvirtModule obj;
    auto res = obj.VirDomainGetID();
    ASSERT_EQ(res, expectedReturn);
}

TEST_F(TestRmrsLibvirtModule, VirDomainGetIDFuncSucceedWithSecondCall)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirDomainGetIDFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(expectedReturn));
    LibvirtModule obj;
    obj.VirDomainGetID();
    auto res = obj.VirDomainGetID();
    ASSERT_EQ(res, expectedReturn);
}

TEST_F(TestRmrsLibvirtModule, VirDomainGetUUIDStringFuncFailWithNullptr)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirDomainGetUUIDStringFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));
    LibvirtModule obj;
    auto res = obj.VirDomainGetUUIDString();
    ASSERT_EQ(res, nullptr);
}

TEST_F(TestRmrsLibvirtModule, VirDomainGetUUIDStringFuncSucceed)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirDomainGetUUIDStringFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(expectedReturn));
    LibvirtModule obj;
    auto res = obj.VirDomainGetUUIDString();
    ASSERT_EQ(res, expectedReturn);
}

TEST_F(TestRmrsLibvirtModule, VirDomainGetUUIDStringFuncSucceedWithSecondCall)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirDomainGetUUIDStringFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(expectedReturn));
    LibvirtModule obj;
    obj.VirDomainGetUUIDString();
    auto res = obj.VirDomainGetUUIDString();
    ASSERT_EQ(res, expectedReturn);
}

TEST_F(TestRmrsLibvirtModule, VirDomainGetInfoFuncFailWithNullptr)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirDomainGetInfoFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));
    LibvirtModule obj;
    auto res = obj.VirDomainGetInfo();
    ASSERT_EQ(res, nullptr);
}

TEST_F(TestRmrsLibvirtModule, VirDomainGetInfoFuncSucceed)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirDomainGetInfoFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(expectedReturn));
    LibvirtModule obj;
    auto res = obj.VirDomainGetInfo();
    ASSERT_EQ(res, expectedReturn);
}

TEST_F(TestRmrsLibvirtModule, VirDomainGetInfoFuncSucceedWithSecondCall)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirDomainGetInfoFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(expectedReturn));
    LibvirtModule obj;
    obj.VirDomainGetInfo();
    auto res = obj.VirDomainGetInfo();
    ASSERT_EQ(res, expectedReturn);
}

TEST_F(TestRmrsLibvirtModule, VirDomainGetVcpusFuncFailWithNullptr)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirDomainGetVcpusFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));
    LibvirtModule obj;
    auto res = obj.VirDomainGetVcpus();
    ASSERT_EQ(res, nullptr);
}

TEST_F(TestRmrsLibvirtModule, VirDomainGetVcpusFuncSucceed)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirDomainGetVcpusFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(expectedReturn));
    LibvirtModule obj;
    auto res = obj.VirDomainGetVcpus();
    ASSERT_EQ(res, expectedReturn);
}

TEST_F(TestRmrsLibvirtModule, VirDomainGetVcpusFuncSucceedWithSecondCall)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirDomainGetVcpusFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(expectedReturn));
    LibvirtModule obj;
    obj.VirDomainGetVcpus();
    auto res = obj.VirDomainGetVcpus();
    ASSERT_EQ(res, expectedReturn);
}

TEST_F(TestRmrsLibvirtModule, VirConnectGetHostnameFuncFailWithNullptr)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirConnectGetHostnameFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));
    LibvirtModule obj;
    auto res = obj.VirConnectGetHostname();
    ASSERT_EQ(res, nullptr);
}

TEST_F(TestRmrsLibvirtModule, VirConnectGetHostnameFuncSucceed)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirConnectGetHostnameFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(expectedReturn));
    LibvirtModule obj;
    auto res = obj.VirConnectGetHostname();
    ASSERT_EQ(res, expectedReturn);
}

TEST_F(TestRmrsLibvirtModule, VirConnectGetHostnameFuncSucceedWithSecondCall)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirConnectGetHostnameFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(expectedReturn));
    LibvirtModule obj;
    obj.VirConnectGetHostname();
    auto res = obj.VirConnectGetHostname();
    ASSERT_EQ(res, expectedReturn);
}

TEST_F(TestRmrsLibvirtModule, VirDomainFreeFuncFailWithNullptr)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirDomainFreeFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));
    LibvirtModule obj;
    auto res = obj.VirDomainFree();
    ASSERT_EQ(res, nullptr);
}

TEST_F(TestRmrsLibvirtModule, VirDomainFreeFuncSucceed)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirDomainFreeFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(expectedReturn));
    LibvirtModule obj;
    auto res = obj.VirDomainFree();
    ASSERT_EQ(res, expectedReturn);
}

TEST_F(TestRmrsLibvirtModule, VirDomainFreeFuncSucceedWithSecondCall)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirDomainFreeFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(expectedReturn));
    LibvirtModule obj;
    obj.VirDomainFree();
    auto res = obj.VirDomainFree();
    ASSERT_EQ(res, expectedReturn);
}

TEST_F(TestRmrsLibvirtModule, VirEventRegisterDefaultImplFuncFailWithNullptr)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirEventRegisterDefaultImplFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));
    LibvirtModule obj;
    auto res = obj.VirEventRegisterDefaultImpl();
    ASSERT_EQ(res, nullptr);
}

TEST_F(TestRmrsLibvirtModule, VirEventRegisterDefaultImplFuncSucceed)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirEventRegisterDefaultImplFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(expectedReturn));
    LibvirtModule obj;
    auto res = obj.VirEventRegisterDefaultImpl();
    ASSERT_EQ(res, expectedReturn);
}

TEST_F(TestRmrsLibvirtModule, VirEventRegisterDefaultImplFuncSucceedWithSecondCall)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirEventRegisterDefaultImplFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(expectedReturn));
    LibvirtModule obj;
    obj.VirEventRegisterDefaultImpl();
    auto res = obj.VirEventRegisterDefaultImpl();
    ASSERT_EQ(res, expectedReturn);
}

TEST_F(TestRmrsLibvirtModule, VirEventRunDefaultImplFuncFailWithNullptr)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirEventRunDefaultImplFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));
    LibvirtModule obj;
    auto res = obj.VirEventRunDefaultImpl();
    ASSERT_EQ(res, nullptr);
}

TEST_F(TestRmrsLibvirtModule, VirEventRunDefaultImplFuncSucceed)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirEventRunDefaultImplFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(expectedReturn));
    LibvirtModule obj;
    auto res = obj.VirEventRunDefaultImpl();
    ASSERT_EQ(res, expectedReturn);
}

TEST_F(TestRmrsLibvirtModule, VirEventRunDefaultImplFuncSucceedWithSecondCall)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirEventRunDefaultImplFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(expectedReturn));
    LibvirtModule obj;
    obj.VirEventRunDefaultImpl();
    auto res = obj.VirEventRunDefaultImpl();
    ASSERT_EQ(res, expectedReturn);
}

TEST_F(TestRmrsLibvirtModule, VirConnectDomainEventRegisterFuncFailWithNullptr)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirConnectDomainEventRegisterFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));
    LibvirtModule obj;
    auto res = obj.VirConnectDomainEventRegister();
    ASSERT_EQ(res, nullptr);
}

TEST_F(TestRmrsLibvirtModule, VirConnectDomainEventRegisterFuncSucceed)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirConnectDomainEventRegisterFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(expectedReturn));
    LibvirtModule obj;
    auto res = obj.VirConnectDomainEventRegister();
    ASSERT_EQ(res, expectedReturn);
}

TEST_F(TestRmrsLibvirtModule, VirConnectDomainEventRegisterFuncSucceedWithSecondCall)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirConnectDomainEventRegisterFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(expectedReturn));
    LibvirtModule obj;
    obj.VirConnectDomainEventRegister();
    auto res = obj.VirConnectDomainEventRegister();
    ASSERT_EQ(res, expectedReturn);
}

TEST_F(TestRmrsLibvirtModule, VirConnectDomainEventDeRegisterFuncFailWithNullptr)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirConnectDomainEventDeRegisterFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));
    LibvirtModule obj;
    auto res = obj.VirConnectDomainEventDeRegister();
    ASSERT_EQ(res, nullptr);
}

TEST_F(TestRmrsLibvirtModule, VirConnectDomainEventDeRegisterFuncSucceed)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirConnectDomainEventDeRegisterFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(expectedReturn));
    LibvirtModule obj;
    auto res = obj.VirConnectDomainEventDeRegister();
    ASSERT_EQ(res, expectedReturn);
}

TEST_F(TestRmrsLibvirtModule, VirConnectDomainEventDeRegisterFuncSucceedWithSecondCall)
{
    void* expectedReturn = reinterpret_cast<void*>(&MockVirConnectDomainEventDeRegisterFunc);
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(expectedReturn));
    LibvirtModule obj;
    obj.VirConnectDomainEventDeRegister();
    auto res = obj.VirConnectDomainEventDeRegister();
    ASSERT_EQ(res, expectedReturn);
}

TEST_F(TestRmrsLibvirtModule, InitShouldReturnOkWhenAllMethodsReturnOk)
{
    LibvirtModule libvirtModule;
    static int dummyHandle;
    MOCKER_CPP(dlopen, void *(*)(const char *, int)).stubs().will(returnValue(reinterpret_cast<void *>(&dummyHandle)));
    uint32_t result = libvirtModule.Init();
    EXPECT_EQ(result, RMRS_OK);
}

TEST_F(TestRmrsLibvirtModule, InitShouldReturnErrorWhenDlopenReturnNullptr)
{
    LibvirtModule libvirtModule;
    MOCKER_CPP(dlopen, void *(*)(const char *, int)).stubs().will(returnValue((void*)nullptr));
    uint32_t result = libvirtModule.Init();
    EXPECT_EQ(result, RMRS_ERROR);
}

void *VirConnectOpenMock(const char *data)
{
    return nullptr;
}

TEST_F(TestRmrsLibvirtModule, VirConnectOpenShouldReturnOkWhenAllMethodsReturnOk)
{
    LibvirtModule libvirtModule;
    MOCKER_CPP(dlsym, void *(*)(void *, const char *))
        .stubs()
        .will(returnValue(reinterpret_cast<void *>(VirConnectOpenMock)));
    VirConnectOpenFunc result = libvirtModule.VirConnectOpen();
    EXPECT_NE(result, nullptr);
}

TEST_F(TestRmrsLibvirtModule, VirConnectOpenShouldReturnErrorWhenDlsymReturnNullptr)
{
    LibvirtModule libvirtModule;
    MOCKER_CPP(dlsym, void *(*)(void *, const char *))
        .stubs()
        .will(returnValue((void*)nullptr));
    VirConnectOpenFunc result = libvirtModule.VirConnectOpen();
    EXPECT_EQ(result, nullptr);
}

int VirConnectCloseMock(void *data)
{
    return 0;
}

TEST_F(TestRmrsLibvirtModule, VirConnectCloseShouldReturnOkWhenAllMethodsReturnOk)
{
    LibvirtModule libvirtModule;
    MOCKER_CPP(dlsym, void *(*)(void *, const char *))
        .stubs()
        .will(returnValue(reinterpret_cast<void *>(VirConnectCloseMock)));
    VirConnectCloseFunc result = libvirtModule.VirConnectClose();
    EXPECT_NE(result, nullptr);
}

TEST_F(TestRmrsLibvirtModule, VirConnectCloseShouldReturnErrorWhenDlsymReturnNullptr)
{
    LibvirtModule libvirtModule;
    MOCKER_CPP(dlsym, void *(*)(void *, const char *))
        .stubs()
        .will(returnValue((void*)nullptr));
    VirConnectCloseFunc result = libvirtModule.VirConnectClose();
    EXPECT_EQ(result, nullptr);
}

int VirConnectListAllDomainsMock(void *data, void ***ptr, VirConnectListAllDomainsFlags)
{
    return 0;
}

TEST_F(TestRmrsLibvirtModule, VirConnectListAllDomainsShouldReturnOkWhenAllMethodsReturnOk)
{
    LibvirtModule libvirtModule;
    MOCKER_CPP(dlsym, void *(*)(void *, const char *))
        .stubs()
        .will(returnValue(reinterpret_cast<void *>(VirConnectListAllDomainsMock)));
    VirConnectListAllDomainsFunc result = libvirtModule.VirConnectListAllDomains();
    EXPECT_NE(result, nullptr);
}

TEST_F(TestRmrsLibvirtModule, VirConnectListAllDomainsShouldReturnErrorWhenDlsymReturnNullptr)
{
    LibvirtModule libvirtModule;
    MOCKER_CPP(dlsym, void *(*)(void *, const char *))
        .stubs()
        .will(returnValue((void*)nullptr));
    VirConnectListAllDomainsFunc result = libvirtModule.VirConnectListAllDomains();
    EXPECT_EQ(result, nullptr);
}

const char *VirDomainGetNameMock(void *data)
{
    return nullptr;
}

TEST_F(TestRmrsLibvirtModule, VirDomainGetNameShouldReturnOkWhenAllMethodsReturnOk)
{
    LibvirtModule libvirtModule;
    MOCKER_CPP(dlsym, void *(*)(void *, const char *))
        .stubs()
        .will(returnValue(reinterpret_cast<void *>(VirDomainGetNameMock)));
    VirDomainGetNameFunc result = libvirtModule.VirDomainGetName();
    EXPECT_NE(result, nullptr);
}

TEST_F(TestRmrsLibvirtModule, VirDomainGetNameShouldReturnErrorWhenDlsymReturnNullptr)
{
    LibvirtModule libvirtModule;
    MOCKER_CPP(dlsym, void *(*)(void *, const char *))
        .stubs()
        .will(returnValue((void*)nullptr));
    VirDomainGetNameFunc result = libvirtModule.VirDomainGetName();
    EXPECT_EQ(result, nullptr);
}

unsigned int VirDomainGetIDMock(void *data)
{
    return 0;
}

TEST_F(TestRmrsLibvirtModule, VirDomainGetIDShouldReturnOkWhenAllMethodsReturnOk)
{
    LibvirtModule libvirtModule;
    MOCKER_CPP(dlsym, void *(*)(void *, const char *))
        .stubs()
        .will(returnValue(reinterpret_cast<void *>(VirDomainGetIDMock)));
    VirDomainGetIDFunc result = libvirtModule.VirDomainGetID();
    EXPECT_NE(result, nullptr);
}

TEST_F(TestRmrsLibvirtModule, VirDomainGetIDShouldReturnErrorWhenDlsymReturnNullptr)
{
    LibvirtModule libvirtModule;
    MOCKER_CPP(dlsym, void *(*)(void *, const char *))
        .stubs()
        .will(returnValue((void*)nullptr));
    VirDomainGetIDFunc result = libvirtModule.VirDomainGetID();
    EXPECT_EQ(result, nullptr);
}

int VirDomainGetUUIDStringMock(void *data, char *ptr)
{
    return 0;
}

TEST_F(TestRmrsLibvirtModule, VirDomainGetUUIDStringShouldReturnOkWhenAllMethodsReturnOk)
{
    LibvirtModule libvirtModule;
    MOCKER_CPP(dlsym, void *(*)(void *, const char *))
        .stubs()
        .will(returnValue(reinterpret_cast<void *>(VirDomainGetUUIDStringMock)));
    VirDomainGetUUIDStringFunc result = libvirtModule.VirDomainGetUUIDString();
    EXPECT_NE(result, nullptr);
}

TEST_F(TestRmrsLibvirtModule, VirDomainGetUUIDStringShouldReturnErrorWhenDlsymReturnNullptr)
{
    LibvirtModule libvirtModule;
    MOCKER_CPP(dlsym, void *(*)(void *, const char *))
        .stubs()
        .will(returnValue((void*)nullptr));
    VirDomainGetUUIDStringFunc result = libvirtModule.VirDomainGetUUIDString();
    EXPECT_EQ(result, nullptr);
}

int VirDomainGetInfoMock(void *data, void *ptr)
{
    return 0;
}

TEST_F(TestRmrsLibvirtModule, VirDomainGetInfoShouldReturnOkWhenAllMethodsReturnOk)
{
    LibvirtModule libvirtModule;
    MOCKER_CPP(dlsym, void *(*)(void *, const char *))
        .stubs()
        .will(returnValue(reinterpret_cast<void *>(VirDomainGetInfoMock)));
    VirDomainGetInfoFunc result = libvirtModule.VirDomainGetInfo();
    EXPECT_NE(result, nullptr);
}

TEST_F(TestRmrsLibvirtModule, VirDomainGetInfoShouldReturnErrorWhenDlsymReturnNullptr)
{
    LibvirtModule libvirtModule;
    MOCKER_CPP(dlsym, void *(*)(void *, const char *))
        .stubs()
        .will(returnValue((void*)nullptr));
    VirDomainGetInfoFunc result = libvirtModule.VirDomainGetInfo();
    EXPECT_EQ(result, nullptr);
}

int VirDomainGetVcpusMock(void *data, void *ptr, int n, unsigned char *m, int q)
{
    return 0;
}

TEST_F(TestRmrsLibvirtModule, VirDomainGetVcpusShouldReturnOkWhenAllMethodsReturnOk)
{
    LibvirtModule libvirtModule;
    MOCKER_CPP(dlsym, void *(*)(void *, const char *))
        .stubs()
        .will(returnValue(reinterpret_cast<void *>(VirDomainGetVcpusMock)));
    VirDomainGetVcpusFunc result = libvirtModule.VirDomainGetVcpus();
    EXPECT_NE(result, nullptr);
}

TEST_F(TestRmrsLibvirtModule, VirDomainGetVcpusShouldReturnErrorWhenDlsymReturnNullptr)
{
    LibvirtModule libvirtModule;
    MOCKER_CPP(dlsym, void *(*)(void *, const char *))
        .stubs()
        .will(returnValue((void*)nullptr));
    VirDomainGetVcpusFunc result = libvirtModule.VirDomainGetVcpus();
    EXPECT_EQ(result, nullptr);
}

char *VirConnectGetHostnameMock(void *data)
{
    return nullptr;
}

TEST_F(TestRmrsLibvirtModule, VirConnectGetHostnameShouldReturnOkWhenAllMethodsReturnOk)
{
    LibvirtModule libvirtModule;
    MOCKER_CPP(dlsym, void *(*)(void *, const char *))
        .stubs()
        .will(returnValue(reinterpret_cast<void *>(VirConnectGetHostnameMock)));
    VirConnectGetHostnameFunc result = libvirtModule.VirConnectGetHostname();
    EXPECT_NE(result, nullptr);
}

TEST_F(TestRmrsLibvirtModule, VirConnectGetHostnameShouldReturnErrorWhenDlsymReturnNullptr)
{
    LibvirtModule libvirtModule;
    MOCKER_CPP(dlsym, void *(*)(void *, const char *))
        .stubs()
        .will(returnValue((void*)nullptr));
    VirConnectGetHostnameFunc result = libvirtModule.VirConnectGetHostname();
    EXPECT_EQ(result, nullptr);
}

int VirDomainFreeMock(void *data)
{
    return 0;
}

TEST_F(TestRmrsLibvirtModule, VirDomainFreeShouldReturnOkWhenAllMethodsReturnOk)
{
    LibvirtModule libvirtModule;
    MOCKER_CPP(dlsym, void *(*)(void *, const char *))
        .stubs()
        .will(returnValue(reinterpret_cast<void *>(VirDomainFreeMock)));
    VirDomainFreeFunc result = libvirtModule.VirDomainFree();
    EXPECT_NE(result, nullptr);
}

TEST_F(TestRmrsLibvirtModule, VirDomainFreeShouldReturnErrorWhenDlsymReturnNullptr)
{
    LibvirtModule libvirtModule;
    MOCKER_CPP(dlsym, void *(*)(void *, const char *))
        .stubs()
        .will(returnValue((void*)nullptr));
    VirDomainFreeFunc result = libvirtModule.VirDomainFree();
    EXPECT_EQ(result, nullptr);
}

int VirConnectIsAliveMock(void *data)
{
    return 0;
}

TEST_F(TestRmrsLibvirtModule, VirConnectIsAliveShouldReturnOkWhenAllMethodsReturnOk)
{
    LibvirtModule libvirtModule;
    MOCKER_CPP(dlsym, void *(*)(void *, const char *))
        .stubs()
        .will(returnValue(reinterpret_cast<void *>(VirConnectIsAliveMock)));
    VirConnectIsAliveFunc result = libvirtModule.VirConnectIsAlive();
    EXPECT_NE(result, nullptr);
}

TEST_F(TestRmrsLibvirtModule, VirConnectIsAliveShouldReturnErrorWhenDlsymReturnNullptr)
{
    LibvirtModule libvirtModule;
    MOCKER_CPP(dlsym, void *(*)(void *, const char *))
        .stubs()
        .will(returnValue((void*)nullptr));
    VirConnectIsAliveFunc result = libvirtModule.VirConnectIsAlive();
    EXPECT_EQ(result, nullptr);
}

int VirEventRegisterDefaultImplMock()
{
    return 0;
}

TEST_F(TestRmrsLibvirtModule, VirEventRegisterDefaultImplShouldReturnOkWhenAllMethodsReturnOk)
{
    LibvirtModule libvirtModule;
    MOCKER_CPP(dlsym, void *(*)(void *, const char *))
        .stubs()
        .will(returnValue(reinterpret_cast<void *>(VirEventRegisterDefaultImplMock)));
    VirEventRegisterDefaultImplFunc result = libvirtModule.VirEventRegisterDefaultImpl();
    EXPECT_NE(result, nullptr);
}

TEST_F(TestRmrsLibvirtModule, VirEventRegisterDefaultImplShouldReturnErrorWhenDlsymReturnNullptr)
{
    LibvirtModule libvirtModule;
    MOCKER_CPP(dlsym, void *(*)(void *, const char *))
        .stubs()
        .will(returnValue((void*)nullptr));
    VirEventRegisterDefaultImplFunc result = libvirtModule.VirEventRegisterDefaultImpl();
    EXPECT_EQ(result, nullptr);
}

int VirEventRunDefaultImplMock()
{
    return 0;
}

TEST_F(TestRmrsLibvirtModule, VirEventRunDefaultImplShouldReturnOkWhenAllMethodsReturnOk)
{
    LibvirtModule libvirtModule;
    MOCKER_CPP(dlsym, void *(*)(void *, const char *))
        .stubs()
        .will(returnValue(reinterpret_cast<void *>(VirEventRegisterDefaultImplMock)));
    VirEventRunDefaultImplFunc result = libvirtModule.VirEventRunDefaultImpl();
    EXPECT_NE(result, nullptr);
}

TEST_F(TestRmrsLibvirtModule, VirEventRunDefaultImplShouldReturnErrorWhenDlsymReturnNullptr)
{
    LibvirtModule libvirtModule;
    MOCKER_CPP(dlsym, void *(*)(void *, const char *))
        .stubs()
        .will(returnValue((void*)nullptr));
    VirEventRunDefaultImplFunc result = libvirtModule.VirEventRunDefaultImpl();
    EXPECT_EQ(result, nullptr);
}

int VirConnectDomainEventRegisterMock(void *data, VirConnectDomainEventCallbackFunc ptr, void *q, void *m)
{
    return 0;
}

TEST_F(TestRmrsLibvirtModule, VirConnectDomainEventRegisterShouldReturnOkWhenAllMethodsReturnOk)
{
    LibvirtModule libvirtModule;
    MOCKER_CPP(dlsym, void *(*)(void *, const char *))
        .stubs()
        .will(returnValue(reinterpret_cast<void *>(VirConnectDomainEventRegisterMock)));
    VirConnectDomainEventRegisterFunc result = libvirtModule.VirConnectDomainEventRegister();
    EXPECT_NE(result, nullptr);
}

TEST_F(TestRmrsLibvirtModule, VirConnectDomainEventRegisterShouldReturnErrorWhenDlsymReturnNullptr)
{
    LibvirtModule libvirtModule;
    MOCKER_CPP(dlsym, void *(*)(void *, const char *))
        .stubs()
        .will(returnValue((void*)nullptr));
    VirConnectDomainEventRegisterFunc result = libvirtModule.VirConnectDomainEventRegister();
    EXPECT_EQ(result, nullptr);
}

int VirConnectDomainEventDeRegisterMock(void *data, VirConnectDomainEventCallbackFunc ptr)
{
    return 0;
}

TEST_F(TestRmrsLibvirtModule, VirConnectDomainEventDeRegisterShouldReturnOkWhenAllMethodsReturnOk)
{
    LibvirtModule libvirtModule;
    MOCKER_CPP(dlsym, void *(*)(void *, const char *))
        .stubs()
        .will(returnValue(reinterpret_cast<void *>(VirConnectDomainEventDeRegisterMock)));
    VirConnectDomainEventDeRegisterFunc result = libvirtModule.VirConnectDomainEventDeRegister();
    EXPECT_NE(result, nullptr);
}

TEST_F(TestRmrsLibvirtModule, VirConnectDomainEventDeRegisterShouldReturnErrorWhenDlsymReturnNullptr)
{
    LibvirtModule libvirtModule;
    MOCKER_CPP(dlsym, void *(*)(void *, const char *))
        .stubs()
        .will(returnValue((void*)nullptr));
    VirConnectDomainEventDeRegisterFunc result = libvirtModule.VirConnectDomainEventDeRegister();
    EXPECT_EQ(result, nullptr);
}

}