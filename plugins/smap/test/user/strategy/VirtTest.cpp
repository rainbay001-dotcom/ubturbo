/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: smap5.0 user allocation ut code
 */

#include <stdio.h>
#include <cstdlib>
#include <dlfcn.h>
#include <sys/time.h>
#include <libvirt/libvirt.h>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include "securec.h"
#include "manage/manage.h"
#include "manage/virt.h"
#include "smap_user_log.h"

using namespace std;

class VirtTest : public ::testing::Test {
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

extern "C" int ExtractIdFromCmdline(char *cmdline, int len, int *id);
TEST_F(VirtTest, TestExtractIdFromCmdline)
{
    int ret;
    char cmdline[64] = "domain-134-test_vm/";
    int mockId = 134;
    int id;
    MOCKER((int (*)(char const *, char const *, void *))sscanf_s)
        .stubs()
        .with(any(), any(), outBoundP((void*)&mockId, sizeof(int)))
        .will(returnValue(3));
    ret = ExtractIdFromCmdline(cmdline, sizeof(cmdline), &id);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(mockId, id);

    ret = ExtractIdFromCmdline(nullptr, 0, &id);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(VirtTest, TestExtractIdFromCmdlineInvalidLen)
{
    int ret;
    char cmdline[64] = "domain-134-test_vm/";
    int id;

    ret = ExtractIdFromCmdline(nullptr, 4097, &id);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(VirtTest, TestReadDomainIdByPid)
{
    int ret;
    pid_t pid = 0;
    int id;
    FILE *fp;
    char tmp[] = "domain-134-test_vm/";
    MOCKER(popen).stubs().will(returnValue(static_cast<FILE *>(nullptr)));
    errno = EBADF;
    ret = ReadDomainIdByPid(pid, &id);
    EXPECT_EQ(-errno, ret);
    errno = 0;

    GlobalMockObject::verify();
    MOCKER(popen).stubs().will(returnValue(fp));
    MOCKER(fgets).stubs().will(returnValue(static_cast<char*>(tmp)));
    MOCKER(ExtractIdFromCmdline).stubs().will(returnValue(-EINVAL));
    MOCKER(pclose).stubs().will(returnValue(0));
    ret = ReadDomainIdByPid(pid, &id);
    EXPECT_EQ(-EINVAL, ret);
}

extern "C" int snprintf_s(char *strDest, size_t destMax, size_t count, const char *format, ...);
TEST_F(VirtTest, TestReadDomainIdByPidTwo)
{
    int ret;
    pid_t pid = 0;
    int id;
    MOCKER(reinterpret_cast<int (*)(char *, unsigned long, unsigned long, char const *, void *)>(snprintf_s))
        .stubs()
        .will(returnValue(-1));
    ret = ReadDomainIdByPid(pid, &id);
    EXPECT_EQ(-1, ret);
}

int StubVirConnectClose(virConnectPtr conn)
{
    return 0;
}

extern "C" int(*g_virConnectClose)(virConnectPtr conn);
TEST_F(VirtTest, TestCloseVirConn)
{
    virConnect conn;
    virConnectPtr virConn = &conn;
    g_virConnectClose = StubVirConnectClose;

    CloseVirConn(&virConn);
    EXPECT_EQ(nullptr, virConn);
}

virConnectPtr stubFunc(const char *name)
{
    return nullptr;
}
extern "C" virConnectPtr(*g_virConnectOpen)(const char *name);
TEST_F(VirtTest, TestOpenVirConn)
{
    int ret;
    virConnect conn;
    virConnectPtr virConn = &conn;
    ret = OpenVirConn(&virConn);
    EXPECT_EQ(0, ret);
    virConn = nullptr;
    g_virConnectOpen = stubFunc;
    ret = OpenVirConn(&virConn);
    EXPECT_EQ(-ENOTCONN, ret);
}

TEST_F(VirtTest, TestOpenVirConnTwo)
{
    virConnect conn;
    virConnectPtr virConn = nullptr;
    g_virConnectOpen = stubFunc;
    MOCKER(stubFunc).expects(once()).will(returnValue(&conn));
    int ret = OpenVirConn(&virConn);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(&conn, virConn);
}

extern "C" void *g_virtHandler;
TEST_F(VirtTest, TestCloseVirHandler)
{
    virConnect conn;
    g_virtHandler = &conn;
    MOCKER(dlclose).expects(once()).will(returnValue(0));
    CloseVirHandler();
    EXPECT_EQ(nullptr, g_virtHandler);
}

TEST_F(VirtTest, TestOpenVirHandler)
{
    int ret;
    virConnect conn;
    g_virtHandler = &conn;
    MOCKER(dlopen).expects(never());
    ret = OpenVirHandler();
    EXPECT_EQ(0, ret);
    g_virtHandler = nullptr;
}

TEST_F(VirtTest, TestOpenVirHandlerTwo)
{
    g_virtHandler = nullptr;
    MOCKER(dlopen).stubs().will(returnValue(static_cast<void *>(nullptr)));
    int ret = OpenVirHandler();
    EXPECT_EQ(-ENOENT, ret);

    GlobalMockObject::verify();
    virConnect conn;
    MOCKER(dlopen).stubs().will(returnValue(static_cast<void *>(&conn)));
    MOCKER(dlsym).stubs().will(returnValue(static_cast<void *>(nullptr)));
    ret = OpenVirHandler();
    EXPECT_EQ(-ENOENT, ret);
    g_virtHandler = nullptr;
}

TEST_F(VirtTest, TestOpenVirHandlerThree)
{
    virConnect conn;
    MOCKER(dlopen).stubs().will(returnValue(static_cast<void *>(&conn)));
    MOCKER(dlsym).stubs().will(returnValue(static_cast<void *>(&conn))).then(returnValue(static_cast<void *>(nullptr)));
    int ret = OpenVirHandler();
    EXPECT_EQ(-ENOENT, ret);
    g_virtHandler = nullptr;
}

TEST_F(VirtTest, TestOpenVirHandlerFour)
{
    virConnect conn;
    MOCKER(dlopen).stubs().will(returnValue(static_cast<void *>(&conn)));
    MOCKER(dlsym).stubs().will(returnValue(static_cast<void *>(&conn)));
    int ret = OpenVirHandler();
    EXPECT_EQ(0, ret);
    g_virtHandler = nullptr;
}

TEST_F(VirtTest, TestOpenVirHandlerFive)
{
    virConnect conn;
    MOCKER(dlopen).stubs().will(returnValue(static_cast<void *>(&conn)));
    MOCKER(dlsym).stubs().will(returnValue(static_cast<void *>(&conn)))
                         .then(returnValue(static_cast<void *>(&conn)))
                         .then(returnValue(static_cast<void *>(nullptr)));
    int ret = OpenVirHandler();
    EXPECT_EQ(-ENOENT, ret);
    g_virtHandler = nullptr;
}

TEST_F(VirtTest, TestOpenVirHandlerSix)
{
    virConnect conn;
    MOCKER(dlopen).stubs().will(returnValue(static_cast<void *>(&conn)));
    MOCKER(dlsym).stubs().will(returnValue(static_cast<void *>(&conn)))
                         .then(returnValue(static_cast<void *>(&conn)))
                         .then(returnValue(static_cast<void *>(&conn)))
                         .then(returnValue(static_cast<void *>(nullptr)));
    int ret = OpenVirHandler();
    EXPECT_EQ(-ENOENT, ret);
    g_virtHandler = nullptr;
}

TEST_F(VirtTest, TestOpenVirHandlerSeven)
{
    virConnect conn;
    MOCKER(dlopen).stubs().will(returnValue(static_cast<void *>(&conn)));
    MOCKER(dlsym).stubs().will(returnValue(static_cast<void *>(&conn)))
                         .then(returnValue(static_cast<void *>(&conn)))
                         .then(returnValue(static_cast<void *>(&conn)))
                         .then(returnValue(static_cast<void *>(&conn)))
                         .then(returnValue(static_cast<void *>(nullptr)));
    int ret = OpenVirHandler();
    EXPECT_EQ(-ENOENT, ret);
    g_virtHandler = nullptr;
}

TEST_F(VirtTest, TestOpenVirHandlerEight)
{
    virConnect conn;
    MOCKER(dlopen).stubs().will(returnValue(static_cast<void *>(&conn)));
    MOCKER(dlsym).stubs().will(returnValue(static_cast<void *>(&conn)))
                         .then(returnValue(static_cast<void *>(&conn)))
                         .then(returnValue(static_cast<void *>(&conn)))
                         .then(returnValue(static_cast<void *>(&conn)))
                         .then(returnValue(static_cast<void *>(&conn)))
                         .then(returnValue(static_cast<void *>(nullptr)));
    int ret = OpenVirHandler();
    EXPECT_EQ(-ENOENT, ret);
    g_virtHandler = nullptr;
}

virConnectPtr StubVirConnectOpen(const char *name)
{
    return nullptr;
}

int StubVirConnectIsDead(virConnectPtr conn)
{
    return 0;
}
int StubVirConnectIsAlive(virConnectPtr conn)
{
    return 1;
}
virDomainPtr StubVirDomainLookupByID(virConnectPtr conn, int id)
{
    return nullptr;
}
int StubVirDomainGetInfo(virDomainPtr domain, virDomainInfoPtr info)
{
    return 0;
}
int StubVirDomainGetVcpus(virDomainPtr domain, virVcpuInfoPtr info, int maxinfo, unsigned char *cpumaps, int maplen)
{
    return -1;
}
int StubVirDomainFree(virDomainPtr domain)
{
    return 0;
}
char *StubVirDomainGetXMLDesc(virDomainPtr domain, unsigned int flags)
{
    return nullptr;
}
extern "C" virDomainPtr(*g_virDomainLookupByID)(virConnectPtr conn, int id);
extern "C" virConnectPtr(*g_virConnectOpen)(const char *name);
extern "C" int(*g_virDomainGetInfo)(virDomainPtr domain, virDomainInfoPtr info);
extern "C" int(*g_virConnectIsAlive)(virConnectPtr conn);
extern "C" int(*g_virDomainGetVcpus)
    (virDomainPtr domain, virVcpuInfoPtr info, int maxinfo, unsigned char *cpumaps, int maplen);
extern "C" int(*g_virDomainFree)(virDomainPtr domain);
extern "C" char *(*g_virDomainGetXMLDesc)(virDomainPtr domain, unsigned int flags);

TEST_F(VirtTest, TestCheckVirConn)
{
    int ret;
    virConnectPtr virConn = nullptr;
    MOCKER(OpenVirConn).stubs().will(returnValue(-ENOTCONN));
    ret = CheckVirConn(&virConn);
    EXPECT_EQ(-ENOTCONN, ret);
}

TEST_F(VirtTest, TestCheckVirConnTwo)
{
    int ret;
    virConnect conn;
    virConnectPtr virConn = &conn;
    g_virConnectIsAlive = StubVirConnectIsDead;
    g_virConnectClose = StubVirConnectClose;
    MOCKER(OpenVirConn).stubs().will(returnValue(-ENOTCONN));
    ret = CheckVirConn(&virConn);
    EXPECT_EQ(-ENOTCONN, ret);
}

TEST_F(VirtTest, TestCheckVirConnThree)
{
    int ret;
    virConnect conn;
    virConnectPtr virConn = &conn;
    g_virConnectIsAlive = StubVirConnectIsAlive;
    ret = CheckVirConn(&virConn);
    EXPECT_EQ(0, ret);

    g_virConnectIsAlive = StubVirConnectIsDead;
    g_virConnectClose = StubVirConnectClose;
    MOCKER(OpenVirConn).stubs().will(returnValue(0));
    ret = CheckVirConn(&virConn);
    EXPECT_EQ(0, ret);
}

TEST_F(VirtTest, TestGetXMLByDomainIdOpenConnFailed)
{
    int domainId;
    char *xml = nullptr;

    g_virConnectOpen = StubVirConnectOpen;
    
    xml = GetXMLByDomainId(domainId);
    EXPECT_EQ(nullptr, xml);
}

TEST_F(VirtTest, TestGetXMLByDomainIdGetDomainFailed)
{
    int domainId;
    char *xml = nullptr;
    virConnect conn;

    g_virDomainLookupByID = StubVirDomainLookupByID;
    g_virConnectOpen = StubVirConnectOpen;
    MOCKER(StubVirConnectOpen).stubs().will(returnValue(&conn));

    xml = GetXMLByDomainId(domainId);
    EXPECT_EQ(nullptr, xml);
}

TEST_F(VirtTest, TestGetXMLByDomainIdGetXMLFailed)
{
    int domainId;
    char *xml = nullptr;
    virConnect conn;
    virDomain domain;

    g_virConnectOpen = StubVirConnectOpen;
    g_virDomainLookupByID = StubVirDomainLookupByID;
    g_virDomainGetXMLDesc = StubVirDomainGetXMLDesc;
    g_virDomainFree = StubVirDomainFree;
    g_virConnectClose = StubVirConnectClose;
    MOCKER(StubVirConnectOpen).stubs().will(returnValue(&conn));
    MOCKER(StubVirDomainLookupByID).stubs().will(returnValue(&domain));

    xml = GetXMLByDomainId(domainId);
    EXPECT_EQ(nullptr, xml);
}