/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.‘
 */

#include <gmock/gmock.h>
#include <iostream>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#define private public
#include "rmrs_libvirt_helper.h"
#undef private

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

using namespace std;
using namespace rmrs::libvirt;

namespace rmrs {

class TestRmrsLibvirtHelper : public ::testing::Test {
protected:

    TestRmrsLibvirtHelper() {}

    void SetUp() override
    {
        cout << "[TestRmrsLibvirtHelper SetUp Begin]" << endl;
        cout << "[TestRmrsLibvirtHelper SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[TestRmrsLibvirtHelper TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[TestRmrsLibvirtHelper TearDown End]" << endl;
    }
};

TEST_F(TestRmrsLibvirtHelper, InitShouldReturnOkWhenAllMethodsReturnOk)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::Init, RmrsResult(*)())
        .stubs()
        .will(returnValue(RMRS_OK));
    MOCKER_CPP(&rmrs::LibvirtHelper::Connect, uint32_t(*)())
        .stubs()
        .will(returnValue(RMRS_OK));
    uint32_t result = libvirtHelper.Init();
    EXPECT_EQ(result, RMRS_OK);
}

TEST_F(TestRmrsLibvirtHelper, InitShouldReturnErrorWhenInitReturnError)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::Init, RmrsResult(*)())
        .stubs()
        .will(returnValue(RMRS_ERROR));
    uint32_t result = libvirtHelper.Init();
    EXPECT_EQ(result, RMRS_ERROR);
}

TEST_F(TestRmrsLibvirtHelper, InitShouldReturnOkWhenConnectReturnError)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::Init, RmrsResult(*)())
        .stubs()
        .will(returnValue(RMRS_OK));
    MOCKER_CPP(&rmrs::LibvirtHelper::Connect, uint32_t(*)())
        .stubs()
        .will(returnValue(RMRS_ERROR));
    uint32_t result = libvirtHelper.Init();
    EXPECT_EQ(result, RMRS_OK);
}

int VirConnectCloseFuncReturnNullptrMockFunc(void* x)
{
    return 0;
}

VirConnectCloseFunc VirConnectCloseInvockFunc()
{
    return reinterpret_cast<VirConnectCloseFunc>(VirConnectCloseFuncReturnNullptrMockFunc);
}

TEST_F(TestRmrsLibvirtHelper, CloseConnShouldReturnErrorWhenVirConnectIsNullptr)
{
    LibvirtHelper libvirtHelper;
    libvirtHelper.virConnect = nullptr;
    MOCKER_CPP(&LibvirtModule::VirConnectClose, VirConnectCloseFunc(*)())
        .stubs()
        .will(invoke(VirConnectCloseInvockFunc));
    uint32_t result = libvirtHelper.CloseConn();
    EXPECT_EQ(result, RMRS_ERROR);
}

TEST_F(TestRmrsLibvirtHelper, IsConnectAliveShouldReturnFalseWhenVirConnectIsAliveIsNullptr)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::VirConnectClose, VirConnectCloseFunc(*)())
        .stubs()
        .will(invoke(VirConnectCloseInvockFunc));
    uint32_t result = libvirtHelper.IsConnectAlive();
    EXPECT_EQ(result, false);
}

int VirConnectIsAliveFuncReturnCorrectMockFunc(void* x)
{
    return 1;
}

VirConnectIsAliveFunc VirConnectIsAliveInvockFunc()
{
    return reinterpret_cast<VirConnectIsAliveFunc>(VirConnectIsAliveFuncReturnCorrectMockFunc);
}

TEST_F(TestRmrsLibvirtHelper, IsConnectAliveShouldReturnTrueWhenVirConnectIsAliveCorrect)
{
    LibvirtHelper libvirtHelper;
    libvirtHelper.virConnect = reinterpret_cast<VirConnectPtr>(0x1234);
    MOCKER_CPP(&LibvirtModule::VirConnectIsAlive, VirConnectIsAliveFunc(*)())
        .stubs()
        .will(invoke(VirConnectIsAliveInvockFunc));
    uint32_t result = libvirtHelper.IsConnectAlive();
    EXPECT_EQ(result, true);
}

TEST_F(TestRmrsLibvirtHelper, IsConnectAliveShouldReturnFalseWhenVirConnectIsNullptr)
{
    LibvirtHelper libvirtHelper;
    libvirtHelper.virConnect = nullptr;
    MOCKER_CPP(&LibvirtModule::VirConnectIsAlive, VirConnectIsAliveFunc(*)())
        .stubs()
        .will(invoke(VirConnectIsAliveInvockFunc));
    uint32_t result = libvirtHelper.IsConnectAlive();
    EXPECT_EQ(result, false);
}

int VirEventRegisterDefaultImplReturnCorrectMockFunc()
{
    return 0;
}

VirEventRegisterDefaultImplFunc VirEventRegisterDefaultImplFuncReturnCorrectInvockFunc()
{
    return reinterpret_cast<VirEventRegisterDefaultImplFunc>(VirEventRegisterDefaultImplReturnCorrectMockFunc);
}

int VirEventRegisterDefaultImplReturnErrorMockFunc()
{
    return 1;
}

VirEventRegisterDefaultImplFunc VirEventRegisterDefaultImplFuncReturnErrorInvockFunc()
{
    return reinterpret_cast<VirEventRegisterDefaultImplFunc>(VirEventRegisterDefaultImplReturnErrorMockFunc);
}

TEST_F(TestRmrsLibvirtHelper, GetVmBasicInfoReturnErrorWhenGetDomainListError)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&rmrs::LibvirtHelper::GetDomainList, uint32_t(*)(void **, int))
        .stubs()
        .will(returnValue(RMRS_ERROR));
    uint32_t result = libvirtHelper.GetVmBasicInfo(nullptr);
    EXPECT_EQ(result, RMRS_ERROR);
}

VirDomainGetNameFunc VirDomainGetNameReturnNullptrInvokeFunc()
{
    return nullptr;
}

TEST_F(TestRmrsLibvirtHelper, GetVmBasicInfoReturnErrorWhenVirDomainGetNameNullptr)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&rmrs::LibvirtHelper::GetDomainList, uint32_t(*)(void **, int))
        .stubs()
        .will(returnValue(RMRS_OK));
    MOCKER_CPP(&LibvirtModule::VirDomainGetName, VirDomainGetNameFunc(*)())
        .stubs()
        .will(invoke(VirDomainGetNameReturnNullptrInvokeFunc));
    uint32_t result = libvirtHelper.GetVmBasicInfo(nullptr);
    EXPECT_EQ(result, RMRS_ERROR);
}

const char* VirDomainGetNameFuncReturnCorrectMockFunc(void * domain)
{
    return "test";
}

VirDomainGetNameFunc VirDomainGetNameReturnCorrectInvokeFunc()
{
    return reinterpret_cast<VirDomainGetNameFunc>(VirDomainGetNameFuncReturnCorrectMockFunc);
}

uint32_t GetDomainListReturnCorrectInvokeFunc(LibvirtHelper *helper, void **&domains, int &numDomains)
{
    domains = (void **)malloc(sizeof(void *));
    numDomains = 1;
    return RMRS_OK;
}

TEST_F(TestRmrsLibvirtHelper, GetVmBasicInfoReturnErrorWhenGetVmUuidError)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&rmrs::LibvirtHelper::GetDomainList,
               uint32_t(*)(LibvirtHelper * helper, void **&domains, int &numDomains))
        .stubs()
        .will(invoke(GetDomainListReturnCorrectInvokeFunc));
    MOCKER_CPP(&LibvirtModule::VirDomainGetName, VirDomainGetNameFunc(*)())
        .stubs()
        .will(invoke(VirDomainGetNameReturnCorrectInvokeFunc));
    MOCKER_CPP(&rmrs::LibvirtHelper::GetVmUuid, uint32_t(*)(ResourceExport *, void *, VmDomainInfo &))
        .stubs()
        .will(returnValue(RMRS_ERROR));
    MOCKER_CPP(LibvirtHelper::FreeDomains, void (*)(void **, size_t)).stubs().will(ignoreReturnValue());
    ResourceExport vmInfoHandler;
    uint32_t result = libvirtHelper.GetVmBasicInfo(&vmInfoHandler);
    EXPECT_EQ(result, RMRS_ERROR);
}

TEST_F(TestRmrsLibvirtHelper, GetVmBasicInfoReturnErrorWhenGetVmStatusError)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&rmrs::LibvirtHelper::GetDomainList,
               uint32_t(*)(LibvirtHelper * helper, void **&domains, int &numDomains))
        .stubs()
        .will(invoke(GetDomainListReturnCorrectInvokeFunc));
    MOCKER_CPP(&LibvirtModule::VirDomainGetName, VirDomainGetNameFunc(*)())
        .stubs()
        .will(invoke(VirDomainGetNameReturnCorrectInvokeFunc));
    MOCKER_CPP(&rmrs::LibvirtHelper::GetVmUuid, uint32_t(*)(ResourceExport *, void *, VmDomainInfo &))
        .stubs()
        .will(returnValue(RMRS_OK));
    MOCKER_CPP(&rmrs::LibvirtHelper::GetVmStatus, uint32_t(*)(void *, VmDomainInfo &, VirDomainInfo &))
        .stubs()
        .will(returnValue(RMRS_ERROR));
    MOCKER_CPP(LibvirtHelper::FreeDomains, void (*)(void **, size_t)).stubs().will(ignoreReturnValue());
    ResourceExport vmInfoHandler;
    uint32_t result = libvirtHelper.GetVmBasicInfo(&vmInfoHandler);
    EXPECT_EQ(result, RMRS_ERROR);
}

TEST_F(TestRmrsLibvirtHelper, GetVmBasicInfoReturnErrorWhenGetVmVCpuInfoError)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&rmrs::LibvirtHelper::GetDomainList,
               uint32_t(*)(LibvirtHelper * helper, void **&domains, int &numDomains))
        .stubs()
        .will(invoke(GetDomainListReturnCorrectInvokeFunc));
    MOCKER_CPP(&LibvirtModule::VirDomainGetName, VirDomainGetNameFunc(*)())
        .stubs()
        .will(invoke(VirDomainGetNameReturnCorrectInvokeFunc));
    MOCKER_CPP(&rmrs::LibvirtHelper::GetVmUuid, uint32_t(*)(ResourceExport *, void *, VmDomainInfo &))
        .stubs()
        .will(returnValue(RMRS_OK));
    MOCKER_CPP(&rmrs::LibvirtHelper::GetVmStatus, uint32_t(*)(void *, VmDomainInfo &, VirDomainInfo &))
        .stubs()
        .will(returnValue(RMRS_OK));
    MOCKER_CPP(&rmrs::LibvirtHelper::GetVmVCpuInfo,
               uint32_t(*)(ResourceExport *, VirDomainInfo, void *, VmDomainInfo &))
        .stubs()
        .will(returnValue(RMRS_ERROR));
    MOCKER_CPP(LibvirtHelper::FreeDomains, void (*)(void **, size_t)).stubs().will(ignoreReturnValue());
    ResourceExport vmInfoHandler;
    uint32_t result = libvirtHelper.GetVmBasicInfo(&vmInfoHandler);
    EXPECT_EQ(result, RMRS_ERROR);
}

TEST_F(TestRmrsLibvirtHelper, GetVmBasicInfoReturnCorrectWhenGetVmVCpuInfoCorrect)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&rmrs::LibvirtHelper::GetDomainList,
               uint32_t(*)(LibvirtHelper * helper, void **&domains, int &numDomains))
        .stubs()
        .will(invoke(GetDomainListReturnCorrectInvokeFunc));
    MOCKER_CPP(&LibvirtModule::VirDomainGetName, VirDomainGetNameFunc(*)())
        .stubs()
        .will(invoke(VirDomainGetNameReturnCorrectInvokeFunc));
    MOCKER_CPP(&rmrs::LibvirtHelper::GetVmUuid, uint32_t(*)(ResourceExport *, void *, VmDomainInfo &))
        .stubs()
        .will(returnValue(RMRS_OK));
    MOCKER_CPP(&rmrs::LibvirtHelper::GetVmStatus, uint32_t(*)(void *, VmDomainInfo &, VirDomainInfo &))
        .stubs()
        .will(returnValue(RMRS_OK));
    MOCKER_CPP(&rmrs::LibvirtHelper::GetVmVCpuInfo,
               uint32_t(*)(ResourceExport *, VirDomainInfo, void *, VmDomainInfo &))
        .stubs()
        .will(returnValue(RMRS_OK));
    MOCKER_CPP(LibvirtHelper::FreeDomains, void (*)(void **, size_t)).stubs().will(ignoreReturnValue());
    ResourceExport vmInfoHandler;
    uint32_t result = libvirtHelper.GetVmBasicInfo(&vmInfoHandler);
    EXPECT_EQ(result, RMRS_OK);
}

VirConnectGetHostnameFunc VirConnectGetHostnameReturnNullptrInvokeFunc()
{
    return nullptr;
}

TEST_F(TestRmrsLibvirtHelper, GetHostNameReturnErrorWhenVirConnectGetHostnameNullptr)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::VirConnectGetHostname, VirConnectGetHostnameFunc(*)())
        .stubs()
        .will(invoke(VirConnectGetHostnameReturnNullptrInvokeFunc));
    std::string hostName = "test";
    uint32_t result = libvirtHelper.GetHostName(hostName);
    EXPECT_EQ(result, RMRS_ERROR);
}

char *VirConnectGetHostnameNullptrMockFunc(void *conn)
{
    return nullptr;
}

VirConnectGetHostnameFunc VirConnectGetHostnameReturnEmptyInvokeFunc()
{
    return reinterpret_cast<VirConnectGetHostnameFunc>(VirConnectGetHostnameNullptrMockFunc);
}

TEST_F(TestRmrsLibvirtHelper, GetHostNameReturnErrorWhenHostNameNullptr)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::VirConnectGetHostname, VirConnectGetHostnameFunc(*)())
        .stubs()
        .will(invoke(VirConnectGetHostnameReturnEmptyInvokeFunc));
    std::string hostName = "test";
    uint32_t result = libvirtHelper.GetHostName(hostName);
    EXPECT_EQ(result, RMRS_ERROR);
}

char *VirConnectGetHostnameMockFunc(void *conn)
{
    return "test";
}

VirConnectGetHostnameFunc VirConnectGetHostnameInvokeFunc()
{
    return reinterpret_cast<VirConnectGetHostnameFunc>(VirConnectGetHostnameMockFunc);
}

TEST_F(TestRmrsLibvirtHelper, GetHostNameReturnOkWhenHostNameOk)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::VirConnectGetHostname, VirConnectGetHostnameFunc(*)())
        .stubs()
        .will(invoke(VirConnectGetHostnameInvokeFunc));
    std::string hostName = "test";
    uint32_t result = libvirtHelper.GetHostName(hostName);
    EXPECT_EQ(result, RMRS_OK);
}

VirDomainGetUUIDStringFunc VirDomainGetUUIDStringReturnNullptrInvokeFunc()
{
    return nullptr;
}

TEST_F(TestRmrsLibvirtHelper, GetVmUuidReturnErrorWhenVirDomainGetUUIDStringNullptr)
{
    LibvirtHelper libvirtHelper;
    vector<std::string> *vmIdList;
    MOCKER_CPP(&rmrs::ResourceExport::GetUuidList, vector<std::string> * (*)()).stubs().will(returnValue(vmIdList));

    MOCKER_CPP(LibvirtModule::VirDomainGetUUIDString, VirDomainGetUUIDStringFunc(*)())
        .stubs()
        .will(invoke(VirDomainGetUUIDStringReturnNullptrInvokeFunc));
    ResourceExport resourceExport;
    VmDomainInfo vmDomainInfo;
    uint32_t result = libvirtHelper.GetVmUuid(&resourceExport, nullptr, vmDomainInfo);
    EXPECT_EQ(result, RMRS_ERROR);
}

int VirDomainGetUUIDStringErrorFuncMockFunc(void *domain, char *uuid)
{
    return -1;
}

VirDomainGetUUIDStringFunc VirDomainGetUUIDStringReturnErrorInvokeFunc()
{
    return reinterpret_cast<VirDomainGetUUIDStringFunc>(VirDomainGetUUIDStringErrorFuncMockFunc);
}

TEST_F(TestRmrsLibvirtHelper, GetVmUuidReturnErrorWhenVirDomainGetUUIDStringError)
{
    LibvirtHelper libvirtHelper;
    vector<std::string> *vmIdList;
    MOCKER_CPP(&rmrs::ResourceExport::GetUuidList, vector<std::string> *(*)())
        .stubs()
        .will(returnValue(vmIdList));

    MOCKER_CPP(LibvirtModule::VirDomainGetUUIDString, VirDomainGetUUIDStringFunc(*)())
        .stubs()
        .will(invoke(VirDomainGetUUIDStringReturnErrorInvokeFunc));
    ResourceExport resourceExport;
    VmDomainInfo vmDomainInfo;
    uint32_t result = libvirtHelper.GetVmUuid(&resourceExport, nullptr, vmDomainInfo);
    EXPECT_EQ(result, RMRS_ERROR);
}

int VirDomainGetUUIDStringCorrectFuncMockFunc(void *domain, char *uuid)
{
    return 1;
}

VirDomainGetUUIDStringFunc VirDomainGetUUIDStringReturnOkInvokeFunc()
{
    return reinterpret_cast<VirDomainGetUUIDStringFunc>(VirDomainGetUUIDStringCorrectFuncMockFunc);
}

VirDomainGetInfoFunc VirDomainGetInfoReturnNullptrInvokeFunc()
{
    return nullptr;
}

TEST_F(TestRmrsLibvirtHelper, GetVmStatusReturnErrorWhenVirDomainGetInfoNullptr)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::VirDomainGetInfo, VirDomainGetInfoFunc(*)())
        .stubs()
        .will(invoke(VirDomainGetInfoReturnNullptrInvokeFunc));
    VmDomainInfo vmInfo;
    VirDomainInfo info;
    uint32_t result = libvirtHelper.GetVmStatus(nullptr, vmInfo, info);
    EXPECT_EQ(result, RMRS_ERROR);
}

int VirDomainGetInfoFuncReturnErrorMockFunc(void* domain, void* info)
{
    return -1;
}

VirDomainGetInfoFunc VirDomainGetInfoReturnErrorInvokeFunc()
{
    return reinterpret_cast<VirDomainGetInfoFunc>(VirDomainGetInfoFuncReturnErrorMockFunc);
}

TEST_F(TestRmrsLibvirtHelper, GetVmStatusReturnErrorWhenVirDomainGetInfoError)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::VirDomainGetInfo, VirDomainGetInfoFunc(*)())
        .stubs()
        .will(invoke(VirDomainGetInfoReturnErrorInvokeFunc));
    VmDomainInfo vmInfo;
    VirDomainInfo info;
    uint32_t result = libvirtHelper.GetVmStatus(nullptr, vmInfo, info);
    EXPECT_EQ(result, RMRS_ERROR);
}

int VirDomainGetInfoFuncReturnCorrectMockFunc(void* domain, void* info)
{
    return 1;
}

VirDomainGetInfoFunc VirDomainGetInfoReturnCorrectInvokeFunc()
{
    return reinterpret_cast<VirDomainGetInfoFunc>(VirDomainGetInfoFuncReturnCorrectMockFunc);
}

TEST_F(TestRmrsLibvirtHelper, GetVmStatusReturnCorrectWhenVirDomainGetInfoCorrect)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::VirDomainGetInfo, VirDomainGetInfoFunc(*)())
        .stubs()
        .will(invoke(VirDomainGetInfoReturnCorrectInvokeFunc));
    VmDomainInfo vmInfo;
    VirDomainInfo info;
    uint32_t result = libvirtHelper.GetVmStatus(nullptr, vmInfo, info);
    EXPECT_EQ(result, RMRS_OK);
}

TEST_F(TestRmrsLibvirtHelper, GetVmVCpuInfoReturnErrorWhenGetVirDomainVCpusError)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&rmrs::LibvirtHelper::GetVirDomainVCpus, uint32_t(*)(void*, VirDomainInfo, VirVcpuInfo**, int&))
        .stubs()
        .will(returnValue(RMRS_ERROR));
    ResourceExport vmInfoHandler;
    VirDomainInfo info;
    info.nrVirtCpu = 1;
    VmDomainInfo vmInfo;
    uint32_t result = libvirtHelper.GetVmVCpuInfo(&vmInfoHandler, info, nullptr, vmInfo);
    EXPECT_EQ(result, RMRS_ERROR);
}


uint32_t GetVirDomainVCpusReturnNegativeNumInvokeFunc(void *domain, VirDomainInfo info,
    VirVcpuInfo **virCpu, int &cpuNums)
{
    cpuNums = -1;
    return RMRS_OK;
}

TEST_F(TestRmrsLibvirtHelper, GetVmVCpuInfoReturnCorrectWhenGetVirDomainVCpusReturnNegativeNum)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&rmrs::LibvirtHelper::GetVirDomainVCpus, uint32_t(*)(void *domain, VirDomainInfo info,
        VirVcpuInfo **virCpu, int &cpuNums))
        .stubs()
        .will(invoke(GetVirDomainVCpusReturnNegativeNumInvokeFunc));
    ResourceExport vmInfoHandler;
    void *domain = reinterpret_cast<void*>(0x1);  // 非 nullptr，但不能解引用
    VirDomainInfo info;
    info.nrVirtCpu = 1;
    VmDomainInfo vmInfo;
    uint32_t result = libvirtHelper.GetVmVCpuInfo(&vmInfoHandler, info, domain, vmInfo);
    EXPECT_EQ(result, RMRS_OK);
}

uint32_t GetVirDomainVCpusReturnPositiveNumInvokeFunc(void *domain, VirDomainInfo info,
    VirVcpuInfo **virCpu, int &cpuNums)
{
    (*virCpu)[0].cpu = 0;
    cpuNums = 1;
    return RMRS_OK;
}

TEST_F(TestRmrsLibvirtHelper, GetVmVCpuInfoReturnCorrectWhenGetVirDomainVCpusReturnPositiveNum)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&rmrs::LibvirtHelper::GetVirDomainVCpus, uint32_t(*)(void *domain, VirDomainInfo info,
        VirVcpuInfo **virCpu, int &cpuNums))
        .stubs()
        .will(invoke(GetVirDomainVCpusReturnPositiveNumInvokeFunc));
    ResourceExport vmInfoHandler;
    void *domain = reinterpret_cast<void*>(0x1);  // 非 nullptr，但不能解引用
    VirDomainInfo info;
    info.nrVirtCpu = 1;
    VmDomainInfo vmInfo;
    uint32_t result = libvirtHelper.GetVmVCpuInfo(&vmInfoHandler, info, domain, vmInfo);
    EXPECT_EQ(result, RMRS_OK);
}

VirDomainGetVcpusFunc VirDomainGetVcpusReturnNullptrInvokeFunc()
{
    return nullptr;
}

TEST_F(TestRmrsLibvirtHelper, GetVirDomainVCpusReturnErrorWhenVirDomainGetVcpusNullptr)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::VirDomainGetVcpus, VirDomainGetVcpusFunc(*)())
        .stubs()
        .will(invoke(VirDomainGetVcpusReturnNullptrInvokeFunc));
    VirDomainInfo info;
    int cpuNums = 1;
    uint32_t result = libvirtHelper.GetVirDomainVCpus(nullptr, info, nullptr, cpuNums);
    EXPECT_EQ(result, RMRS_ERROR);
}

int VirDomainGetVcpusFuncMockFuncReturnError(void *domain, void *virCpu, int nrVirtCpu, unsigned char *cpuMaps,
    int cpuMapLen)
{
    return -1;
}

VirDomainGetVcpusFunc VirDomainGetVcpusReturnErrorInvokeFunc()
{
    return reinterpret_cast<VirDomainGetVcpusFunc>(VirDomainGetVcpusFuncMockFuncReturnError);
}

TEST_F(TestRmrsLibvirtHelper, GetVirDomainVCpusReturnErrorWhenVirDomainGetVcpusError)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::VirDomainGetVcpus, VirDomainGetVcpusFunc(*)())
        .stubs()
        .will(invoke(VirDomainGetVcpusReturnErrorInvokeFunc));
    VirDomainInfo info;
    int cpuNums = 1;
    VirVcpuInfo *virCpu = nullptr;
    uint32_t result = libvirtHelper.GetVirDomainVCpus(nullptr, info, &virCpu, cpuNums);
    EXPECT_EQ(result, RMRS_ERROR);
}

int VirDomainGetVcpusFuncMockFuncReturnCorrect(void * domain, void * virCpu, int nrVirtCpu, unsigned char * cpuMaps,
    int cpuMapLen)
{
    return 1;
}

VirDomainGetVcpusFunc VirDomainGetVcpusReturnCorrectInvokeFunc()
{
    return reinterpret_cast<VirDomainGetVcpusFunc>(VirDomainGetVcpusFuncMockFuncReturnCorrect);
}

TEST_F(TestRmrsLibvirtHelper, GetVirDomainVCpusReturnCorrectWhenVirDomainGetVcpusCorrect)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::VirDomainGetVcpus, VirDomainGetVcpusFunc(*)())
        .stubs()
        .will(invoke(VirDomainGetVcpusReturnCorrectInvokeFunc));
    VirDomainInfo info;
    int cpuNums = 1;
    VirVcpuInfo *virCpu = nullptr;
    uint32_t result = libvirtHelper.GetVirDomainVCpus(nullptr, info, &virCpu, cpuNums);
    EXPECT_EQ(result, RMRS_OK);
}

TEST_F(TestRmrsLibvirtHelper, UpdateVmMemInfoOnNumaWhentNumaIdExistInVmNumaMemInfos)
{
    int memSize1 = 1024;
    int memSize2 = 2048;
    LibvirtHelper libvirtHelper;
    ResourceExport vmInfoHandler;
    uint16_t numaId = 0;
    uint64_t mem = memSize1;
    vmInfoHandler.vmNumaMaxMemInfos.insert(std::make_pair(numaId, mem));
    VmDomainInfo vmInfo;
    VmCpuInfo vmCpuInfo;
    vmCpuInfo.cpuNumaId = numaId;
    vmInfo.cpuInfos.push_back(vmCpuInfo);
    vmInfo.metaData.maxMem = memSize1;
    libvirtHelper.UpdateVmMemInfoOnNuma(&vmInfoHandler, vmInfo);
    uint64_t info = (*vmInfoHandler.GetVmNumaMaxMemInfos())[numaId];
    EXPECT_EQ(info, static_cast<uint64_t>(memSize2));
}

TEST_F(TestRmrsLibvirtHelper, UpdateVmMemInfoOnNumaWhentNumaIdNotExistInVmNumaMemInfos)
{
    int memSize = 1024;
    LibvirtHelper libvirtHelper;
    ResourceExport vmInfoHandler;
    uint16_t numaId = 0;
    VmDomainInfo vmInfo;
    VmCpuInfo vmCpuInfo;
    vmCpuInfo.cpuNumaId = numaId;
    vmInfo.cpuInfos.push_back(vmCpuInfo);
    vmInfo.metaData.maxMem = memSize;
    libvirtHelper.UpdateVmMemInfoOnNuma(&vmInfoHandler, vmInfo);
    uint64_t info = (*vmInfoHandler.GetVmNumaMaxMemInfos())[numaId];
    EXPECT_EQ(info, static_cast<uint64_t>(memSize));
}

TEST_F(TestRmrsLibvirtHelper, FreeDomainsReturnWhenDomainNullptr)
{
    LibvirtHelper libvirtHelper;
    void **domains = nullptr;
    libvirtHelper.FreeDomains(domains, 1);
}

VirDomainFreeFunc VirDomainFreeReturnNullptrInvokeFunc()
{
    return nullptr;
}


int VirDomainFreeCorrectMockFunc(void *domain)
{
    return RMRS_OK;
}

VirDomainFreeFunc VirDomainFreeReturnCorrectInvokeFunc()
{
    return reinterpret_cast<VirDomainFreeFunc>(VirDomainFreeCorrectMockFunc);
}

TEST_F(TestRmrsLibvirtHelper, FreeDomainsReturnWhenVirDomainFreeCorrect)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::VirDomainFree, VirDomainFreeFunc(*)())
        .stubs()
        .will(invoke(VirDomainFreeReturnCorrectInvokeFunc));
    void **domains = (void **)malloc(sizeof(void *));
    libvirtHelper.FreeDomains(domains, 1);
}

VirConnectListAllDomainsFunc VirConnectListAllDomainsReturnNullptrInvokeFunc()
{
    return nullptr;
}

TEST_F(TestRmrsLibvirtHelper, GetDomainListReturnErrorWhenVirConnectListAllDomainsNullptr)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::VirConnectListAllDomains, VirConnectListAllDomainsFunc(*)())
        .stubs()
        .will(invoke(VirConnectListAllDomainsReturnNullptrInvokeFunc));
    void **domains = nullptr;
    int numDomains = 1;
    uint32_t result = libvirtHelper.GetDomainList(domains, numDomains);
    EXPECT_EQ(result, RMRS_ERROR);
}

int VirConnectListAllDomainsFuncReturnErrorMockFunc(void * virConnect, void *** domainPtr,
    VirConnectListAllDomainsFlags flag)
{
    return -1;
}

VirConnectListAllDomainsFunc VirConnectListAllDomainsReturnErrorInvokeFunc()
{
    return reinterpret_cast<VirConnectListAllDomainsFunc>(VirConnectListAllDomainsFuncReturnErrorMockFunc);
}

TEST_F(TestRmrsLibvirtHelper, GetDomainListReturnErrorWhenVirConnectListAllDomainsError)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::VirConnectListAllDomains, VirConnectListAllDomainsFunc(*)())
        .stubs()
        .will(invoke(VirConnectListAllDomainsReturnNullptrInvokeFunc));
    void **domains = (void **)malloc(sizeof(void *));
    int numDomains = 1;
    uint32_t result = libvirtHelper.GetDomainList(domains, numDomains);
    EXPECT_EQ(result, RMRS_ERROR);
}

int VirConnectListAllDomainsFuncReturnCorrectMockFunc(void * virConnect, void *** domainPtr,
    VirConnectListAllDomainsFlags flag)
{
    return -1;
}

VirConnectListAllDomainsFunc VirConnectListAllDomainsReturnCorrectInvokeFunc()
{
    return reinterpret_cast<VirConnectListAllDomainsFunc>(VirConnectListAllDomainsFuncReturnCorrectMockFunc);
}

int VirConnectListAllDomainsFuncReturnCorrectMockFunc2(void * virConnect, void *** domainPtr,
    VirConnectListAllDomainsFlags flag)
{
    return 1;
}

VirConnectListAllDomainsFunc VirConnectListAllDomainsReturnCorrectInvokeFunc2()
{
    return reinterpret_cast<VirConnectListAllDomainsFunc>(VirConnectListAllDomainsFuncReturnCorrectMockFunc2);
}

TEST_F(TestRmrsLibvirtHelper, GetDomainListReturnErrorWhenDomainsNullptr)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::VirConnectListAllDomains, VirConnectListAllDomainsFunc(*)())
        .stubs()
        .will(invoke(VirConnectListAllDomainsReturnCorrectInvokeFunc));
    void **domains = nullptr;
    int numDomains = 1;
    uint32_t result = libvirtHelper.GetDomainList(domains, numDomains);
    EXPECT_EQ(result, RMRS_ERROR);
}

TEST_F(TestRmrsLibvirtHelper, GetDomainListReturnOkWhenVirConnectListAllDomainsCorrect)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::VirConnectListAllDomains, VirConnectListAllDomainsFunc(*)())
        .stubs()
        .will(invoke(VirConnectListAllDomainsReturnCorrectInvokeFunc2));
    void **domains = reinterpret_cast<void **>(0x01);
    int numDomains = 1;
    uint32_t result = libvirtHelper.GetDomainList(domains, numDomains);
    EXPECT_EQ(result, RMRS_OK);
}

} // namespace rmrs