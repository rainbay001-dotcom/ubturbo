/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: smap5.0 user device ut code
 */

#include <cstdlib>
#include <cerrno>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include "manage/manage.h"
#include "manage/device.h"
#include "securec.h"


using namespace std;


class DeviceTest : public ::testing::Test {
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

extern "C" {
int ConfigTrackingDev(int *trackingFds, uint32_t pageSize);
int EnableTracking(struct ProcessManager *manager);
int OpenAndFlockFd(int fd);
}
TEST_F(DeviceTest, TestIInitTrackingDevOne)
{
    struct ProcessManager pm = {
        .nrThread = 0
    };
    int ret;

    MOCKER(reinterpret_cast<int (*)(const char *, int)>(open)).stubs().will(returnValue(-1));
    ret = InitTrackingDev(&pm);
    EXPECT_EQ(-ENODEV, ret);
}

TEST_F(DeviceTest, TestIInitTrackingDevTwo)
{
    struct ProcessManager pm = {
        .nrThread = 0
    };
    int ret;

    MOCKER((int (*)(char *, unsigned long, unsigned long, char const *, void *))snprintf_s)
        .stubs()
        .will(returnValue(0))
        .then(returnValue(-1));
    MOCKER(OpenAndFlockFd).stubs().will(returnValue(0));
    MOCKER(reinterpret_cast<int (*)(const char *, int)>(open)).stubs().will(returnValue(0));
    ret = InitTrackingDev(&pm);
    EXPECT_EQ(-EINVAL, ret);
    GlobalMockObject::verify();

    MOCKER(OpenAndFlockFd).stubs().will(returnValue(0));
    MOCKER(reinterpret_cast<int (*)(const char *, int)>(open)).stubs().will(returnValue(0));
    MOCKER((int (*)(const char *, int))access).stubs().will(returnValue(-1));
    errno = 0;
    ret = InitTrackingDev(&pm);
    EXPECT_EQ(-ENODEV, ret);
}

extern "C" int ConfigureTrackingDevices(struct ProcessManager *manager);
TEST_F(DeviceTest, TestIInitTrackingDevThree)
{
    int ret;
    struct ProcessManager pm = {
        .nrThread = 0
    };

    MOCKER((int (*)(char *, unsigned long, unsigned long, char const *, void *))snprintf_s)
        .stubs()
        .will(returnValue(0));
    MOCKER(reinterpret_cast<int (*)(const char *, int)>(open)).stubs().will(returnValue(0));
    MOCKER((int (*)(const char *, int))access).stubs().will(returnValue(0));
    MOCKER(ConfigureTrackingDevices).stubs().will(returnValue(0));
    MOCKER(EnableTracking).stubs().will(returnValue(0));
    MOCKER(OpenAndFlockFd).stubs().will(returnValue(0));
    ret = InitTrackingDev(&pm);
    EXPECT_EQ(0, ret);
}

extern "C" int SendCmdToAllNodes(int fds[], unsigned long cmd, int arg);
TEST_F(DeviceTest, TestEnableTracking)
{
    struct ProcessManager pm = {
        .nrThread = 0
    };
    int ret;

    MOCKER(SendCmdToAllNodes).stubs().will(returnValue(0));
    ret = EnableTracking(&pm);
    EXPECT_EQ(0, ret);
}

TEST_F(DeviceTest, TestDisableTracking)
{
    struct ProcessManager pm = {
        .nrThread = 0
    };
    int ret;

    MOCKER(SendCmdToAllNodes).stubs().will(returnValue(0));
    ret = DisableTracking(&pm);
    EXPECT_EQ(0, ret);
}

TEST_F(DeviceTest, TestSendCmdToAllNodes)
{
    int fds[100] = {0};
    int ret;

    MOCKER(reinterpret_cast<int (*)(int, unsigned long, void *)>(ioctl)).stubs().will(returnValue(-1));
    ret = SendCmdToAllNodes(fds, 0, 0);
    EXPECT_EQ(-EBADF, ret);
    GlobalMockObject::verify();

    MOCKER(reinterpret_cast<int (*)(int, unsigned long, void *)>(ioctl)).stubs().will(returnValue(0));
    ret = SendCmdToAllNodes(fds, 0, 0);
    EXPECT_EQ(0, ret);
}

TEST_F(DeviceTest, TestSendCmdToAllNodesGroup)
{
    int fds[100] = {-1};
    int ret;

    MOCKER(reinterpret_cast<int (*)(int, unsigned long, void *)>(ioctl)).stubs().will(returnValue(0));
    ret = SendCmdToAllNodes(fds, 0, 0);
    EXPECT_EQ(0, ret);
}

TEST_F(DeviceTest, TestReadTracking)
{
    struct ProcessManager pm = {
        .nrNuma = 0
    };
    int ret;
    MOCKER(read).stubs().will(returnValue(0));
    ret = ReadTracking(&pm);
    EXPECT_EQ(0, ret);
}

extern "C" ssize_t read(int fd, void *buf, size_t count);
TEST_F(DeviceTest, TestReadTrackingTwo)
{
    ProcessManager *pm = (ProcessManager *)malloc(sizeof(ProcessManager));
    pm->fds.nodes[0] = 1;
    pm->nodeActcLen[0] = 1;
    pm->tracking.nodeActc[0] = (uint16_t *)malloc(sizeof(uint16_t) * pm->nodeActcLen[0]);
    int ret;
    MOCKER(read).stubs().will(returnValue(-1));
    ret = ReadTracking(pm);
    EXPECT_EQ(-1, ret);
    free(pm);
}

TEST_F(DeviceTest, TestConfigTrackingDev)
{
    int fds[100] = {0};
    int ret;

    MOCKER(SendCmdToAllNodes).stubs().will(returnValue(0));
    ret = ConfigTrackingDev(fds, PAGESIZE_2M);
    EXPECT_EQ(0, ret);

    ret = ConfigTrackingDev(fds, PAGESIZE_4K);
    EXPECT_EQ(0, ret);
}

extern "C" int DisableTracking(struct ProcessManager *manager);
TEST_F(DeviceTest, TestDeinitTrackingDev)
{
    struct ProcessManager pm = {
        .nrNuma = 1
    };
    pm.nodeActcLen[0] = 1;
    pm.tracking.nodeActc[0] = (uint16_t *)malloc(sizeof(uint16_t) * pm.nodeActcLen[0]);

    MOCKER(DisableTracking).stubs().will(returnValue(0));
    DeinitTrackingDev(&pm);
    EXPECT_EQ(0, pm.nrNuma);

    pm.fds.nodes[0] = -1;
    pm.fds.migrate = -1;
    DeinitTrackingDev(&pm);
    EXPECT_EQ(0, pm.nrNuma);
}

extern "C" uint64_t Calc2MCount(uint64_t range);
TEST_F(DeviceTest, TestCalc2MCountOne)
{
    uint64_t range = 0;
    uint64_t ret = Calc2MCount(range);
    EXPECT_EQ(0, ret);
}

TEST_F(DeviceTest, TestCalc2MCountTwo)
{
    uint64_t range = 1;
    uint64_t ret = Calc2MCount(range);
    EXPECT_EQ(1, ret);
}

extern "C" uint64_t Calc4KCount(uint64_t range);
TEST_F(DeviceTest, TestCalc4KCountOne)
{
    uint64_t range = 0;
    uint64_t ret = Calc4KCount(range);
    EXPECT_EQ(0, ret);
}

TEST_F(DeviceTest, TestCalc4KCountTwo)
{
    uint64_t range = 3;
    uint64_t ret = Calc4KCount(range);
    EXPECT_EQ(1, ret);
}

extern "C" int FindFdByNode(int fds[], int fdsLength);
TEST_F(DeviceTest, TestFindFdByNode)
{
    int fds[1] = {-5};
    int fdsLength = 1;
    int ret = FindFdByNode(fds, fdsLength);
    EXPECT_EQ(-EINVAL, ret);

    fds[0] = 0;
    ret = FindFdByNode(fds, fdsLength);
    EXPECT_EQ(0, ret);
}

extern "C" void FreeNodeActcData(struct ProcessManager *manager);
TEST_F(DeviceTest, TestFreeNodeActcData)
{
    struct ProcessManager manager;
    for (int i = 0; i < MAX_NODES; i++) {
        manager.tracking.nodeActc[i] = new uint16_t(1);
    }
    FreeNodeActcData(&manager);
    for (int i = 0; i < MAX_NODES; i++) {
        EXPECT_EQ(NULL, manager.tracking.nodeActc[i]);
    }
}

extern "C" void FreeAddrMemory(struct ProcessManager *manager);
extern "C" int GetTrackingAddr(struct ProcessManager *manager);
extern "C" int GetNodeActcLenIomem(int len, uint64_t *nodeActcLen, IomemMsg *msg, uint16_t nrLocalNuma);
TEST_F(DeviceTest, TestGetNodeActcLenIomemOne)
{
    int len = 0;
    uint64_t nodeActcLen[10];
    IomemMsg msg;
    uint16_t nrLocalNuma;
    int ret = GetNodeActcLenIomem(len, nodeActcLen, &msg, nrLocalNuma);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(DeviceTest, TestGetNodeActcLenIomemTwo)
{
    int len = 1;
    uint64_t nodeActcLen[10];
    uint16_t nrLocalNuma = 0;
    IomemMsg *msg = (IomemMsg *)malloc(sizeof(IomemMsg) * 1);
    msg->cnt = 1;
    IomemSeg *iomemSegArray = (IomemSeg *)malloc(sizeof(IomemSeg) *10);
    msg->iomemSegArray = iomemSegArray;
    msg->iomemSegArray[0].node = 0;
    msg->iomemSegArray[0].start = 0;
    msg->iomemSegArray[0].end = 10;
    int ret = GetNodeActcLenIomem(len, nodeActcLen, msg, nrLocalNuma);
    EXPECT_EQ(0, ret);
    free(msg);
    free(iomemSegArray);
}

extern "C" int GetNodeActcLenAcpi(int len, uint64_t *nodeActcLen, AcpiMsg *msg);
TEST_F(DeviceTest, TestGetNodeActcLenAcpiOne)
{
    int len = 0;
    uint64_t nodeActcLen [10];
    AcpiMsg msg;
    int ret = GetNodeActcLenAcpi(len, nodeActcLen, &msg);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(DeviceTest, TestGetNodeActcLenAcpiTwo)
{
    uint64_t nodeActcLen[10];
    int len = 1;
    AcpiMsg *msg = (AcpiMsg *)malloc(sizeof(AcpiMsg) * 1);
    msg->cnt = 1;
    AcpiSeg *acpiSegArray = (AcpiSeg *)malloc(sizeof(AcpiSeg) * 1);
    msg->acpiSegArray = acpiSegArray;
    msg->acpiSegArray[0].node = 0;
    msg->acpiSegArray[0].start = 0;
    msg->acpiSegArray[0].end = 10;
    int ret = GetNodeActcLenAcpi(len, nodeActcLen, msg);
    EXPECT_EQ(0, ret);
    free(msg);
    free(acpiSegArray);
}

TEST_F(DeviceTest, TestGetNodeActcLenAcpiThree)
{
    uint64_t nodeActcLen[10] = {0};
    int len = 1;
    AcpiMsg *msg = (AcpiMsg *)malloc(sizeof(AcpiMsg) * len);
    msg->cnt = 1;
    AcpiSeg *acpiSegArray = (AcpiSeg *)malloc(sizeof(AcpiSeg) * 1);
    msg->acpiSegArray = acpiSegArray;
    msg->acpiSegArray[0].node = 0;
    msg->acpiSegArray[0].start = 0;
    msg->acpiSegArray[0].end = 2097151;
    MOCKER(IsHugeMode).stubs().will(returnValue(true));
    int ret = GetNodeActcLenAcpi(len, nodeActcLen, msg);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, nodeActcLen[0]);
    free(acpiSegArray);
    free(msg);
}

TEST_F(DeviceTest, TestGetNodeActcLenAcpiFour)
{
    uint64_t nodeActcLen [10] = {0};
    int len = 1;
    AcpiMsg *msg = (AcpiMsg *)malloc(sizeof(AcpiMsg) * 1);
    msg->cnt = 1;
    AcpiSeg *acpiSegArray = (AcpiSeg *)malloc(sizeof(AcpiSeg) * 1);
    msg->acpiSegArray = acpiSegArray;
    msg->acpiSegArray[0].node = 0;
    msg->acpiSegArray[0].start = 0;
    msg->acpiSegArray[0].end = 20971519;
    MOCKER(IsHugeMode).stubs().will(returnValue(true));
    int ret = GetNodeActcLenAcpi(len, nodeActcLen, msg);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(10, nodeActcLen[0]);
    free(acpiSegArray);
    free(msg);
}

TEST_F(DeviceTest, TestGetNodeActcLenAcpiFive)
{
    uint64_t nodeActcLen [10] = {0};
    int len = 1;
    AcpiMsg *msg = (AcpiMsg *)malloc(sizeof(AcpiMsg) * 1);
    msg->cnt = 1;
    AcpiSeg *acpiSegArray = (AcpiSeg *)malloc(sizeof(AcpiSeg) * 1);
    msg->acpiSegArray = acpiSegArray;
    msg->acpiSegArray[0].node = 0;
    msg->acpiSegArray[0].start = 0;
    msg->acpiSegArray[0].end = 4095;
    MOCKER(IsHugeMode).stubs().will(returnValue(false));
    int ret = GetNodeActcLenAcpi(len, nodeActcLen, msg);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, nodeActcLen[0]);
    free(acpiSegArray);
    free(msg);
}

TEST_F(DeviceTest, TestGetNodeActcLenAcpiSix)
{
    uint64_t nodeActcLen [10] = {0};
    int len = 1;
    AcpiMsg *msg = (AcpiMsg *)malloc(sizeof(AcpiMsg) * 1);
    msg->cnt = 1;
    AcpiSeg *acpiSegArray = (AcpiSeg *)malloc(sizeof(AcpiSeg) * 1);
    msg->acpiSegArray = acpiSegArray;
    msg->acpiSegArray[0].node = 0;
    msg->acpiSegArray[0].start = 0;
    msg->acpiSegArray[0].end = 40959;
    MOCKER(IsHugeMode).stubs().will(returnValue(false));
    int ret = GetNodeActcLenAcpi(len, nodeActcLen, msg);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(10, nodeActcLen[0]);
    free(acpiSegArray);
    free(msg);
}

extern "C" int GetNodeActcLen(struct ProcessManager *manager);
TEST_F(DeviceTest, TestGetNodeActcLenOne)
{
    struct ProcessManager manager;
    MOCKER(GetNodeActcLenIomem).stubs().will(returnValue(-1));
    int ret = GetNodeActcLen(&manager);
    EXPECT_EQ(-1, ret);
}

TEST_F(DeviceTest, TestGetNodeActcLenTwo)
{
    struct ProcessManager manager;
    MOCKER(GetNodeActcLenIomem).stubs().will(returnValue(0));
    MOCKER(GetNodeActcLenAcpi).stubs().will(returnValue(1));
    int ret = GetNodeActcLen(&manager);
    EXPECT_EQ(1, ret);
}

TEST_F(DeviceTest, TestGetNodeActcLenThree)
{
    struct ProcessManager manager;
    MOCKER(GetNodeActcLenIomem).stubs().will(returnValue(0));
    MOCKER(GetNodeActcLenAcpi).stubs().will(returnValue(0));
    int ret = GetNodeActcLen(&manager);
    EXPECT_EQ(0, ret);
}

extern "C" int InitNodeActcData(struct ProcessManager *manager);
TEST_F(DeviceTest, TestInitNodeActcData)
{
    struct ProcessManager manager;
    MOCKER(GetNodeActcLen).stubs().will(returnValue(1));
    int ret = InitNodeActcData(&manager);
    EXPECT_EQ(1, ret);
}

TEST_F(DeviceTest, TestInitNodeActcDataTwo)
{
    struct ProcessManager manager;
    MOCKER(GetNodeActcLen).stubs().will(returnValue(0));
    MOCKER(FreeNodeActcData).stubs().will(ignoreReturnValue());
    int ret = InitNodeActcData(&manager);
    EXPECT_EQ(-ENOMEM, ret);
    for (int i = 0; i < MAX_NODES; i++) {
        manager.nodeActcLen[i] = 0;
    }
    ret = InitNodeActcData(&manager);
    EXPECT_EQ(0, ret);
}

extern "C" void FreeAddrMemory(struct ProcessManager *manager);
TEST_F(DeviceTest, TestFreeAddrMemory)
{
    struct ProcessManager manager;
    AcpiSeg *acpiSegArray = (AcpiSeg*)malloc(sizeof(AcpiSeg) * 10);
    manager.acpiMsg.acpiSegArray = acpiSegArray;
    IomemSeg *iomemSegArray = (IomemSeg *)malloc(sizeof(IomemSeg) * 10);
    manager.iomMsg.iomemSegArray = iomemSegArray;
    FreeAddrMemory(&manager);
    EXPECT_EQ(NULL, manager.acpiMsg.acpiSegArray);
    EXPECT_EQ(NULL, manager.iomMsg.iomemSegArray);
}

extern "C" void GetLocalNrNuma(struct ProcessManager *manager);
TEST_F(DeviceTest, TestGetLocalNrNuma)
{
    struct ProcessManager manager;
    manager.acpiMsg.cnt = 1;
    AcpiSeg acpiSegArray[10];
    manager.acpiMsg.acpiSegArray = acpiSegArray;
    manager.acpiMsg.acpiSegArray[0].node = 1;
    GetLocalNrNuma(&manager);
    EXPECT_EQ(2, manager.nrLocalNuma);

    acpiSegArray[0].node = -1;
    GetLocalNrNuma(&manager);
    EXPECT_EQ(0, manager.nrLocalNuma);
}

extern "C" int GetAcpiAddresses(struct ProcessManager *manager);
TEST_F(DeviceTest, TestGetAcpiAddressesOne)
{
    struct ProcessManager manager;
    MOCKER(FindFdByNode).stubs().will(returnValue(-1));
    int ret = GetAcpiAddresses(&manager);
    EXPECT_EQ(-1, ret);
}

TEST_F(DeviceTest, TestGetAcpiAddressesTwo)
{
    struct ProcessManager manager;
    MOCKER(FindFdByNode).stubs().will(returnValue(0));
    MOCKER(reinterpret_cast<int (*)(int, int, int *)>(ioctl)).stubs().will(returnValue(-1));
    int ret = GetAcpiAddresses(&manager);
    EXPECT_EQ(-1, ret);
}

TEST_F(DeviceTest, TestGetAcpiAddressesThree)
{
    struct ProcessManager manager = {
        .acpiMsg = { .cnt = 1 }
    };
    MOCKER(FindFdByNode).stubs().will(returnValue(0));
    MOCKER(reinterpret_cast<int (*)(int, int, int *)>(ioctl)).stubs().will(returnValue(0));
    MOCKER(GetLocalNrNuma).stubs().will(ignoreReturnValue());
    int ret = GetAcpiAddresses(&manager);
    free(manager.acpiMsg.acpiSegArray);
    EXPECT_EQ(0, ret);
}

TEST_F(DeviceTest, TestGetAcpiAddressesFour)
{
    struct ProcessManager manager = {
        .acpiMsg = { .cnt = 1 }
    };
    MOCKER(FindFdByNode).stubs().will(returnValue(0));
    MOCKER(reinterpret_cast<int (*)(int, int, int *)>(ioctl)).stubs().will(returnValue(0)).then(returnValue(-1));
    int ret = GetAcpiAddresses(&manager);
    EXPECT_EQ(-1, ret);
}

extern "C" int GetIomemAddresses(struct ProcessManager *manager);
TEST_F(DeviceTest, TestGetIomemAddressesOne)
{
    struct ProcessManager manager;
    MOCKER(FindFdByNode).stubs().will(returnValue(-1));
    int ret = GetIomemAddresses(&manager);
    EXPECT_EQ(-1, ret);
}

TEST_F(DeviceTest, TestGetIomemAddressesTwo)
{
    struct ProcessManager manager;
    MOCKER(FindFdByNode).stubs().will(returnValue(0));
    MOCKER(reinterpret_cast<int (*)(int, int, int *)>(ioctl)).stubs().will(returnValue(-1));
    int ret = GetIomemAddresses(&manager);
    EXPECT_EQ(-1, ret);
}

TEST_F(DeviceTest, TestGetIomemAddressesThree)
{
    struct ProcessManager manager = {
        .iomMsg = { .cnt = 1 }
    };
    MOCKER(FindFdByNode).stubs().will(returnValue(0));
    MOCKER(reinterpret_cast<int (*)(int, int, int *)>(ioctl)).stubs().will(returnValue(0));
    int ret = GetIomemAddresses(&manager);
    free(manager.iomMsg.iomemSegArray);
    EXPECT_EQ(0, ret);
}

TEST_F(DeviceTest, TestGetIomemAddressesFour)
{
    struct ProcessManager manager = {
        .iomMsg = { .cnt = 0 }
    };
    IomemSeg *iomemSegArray = (IomemSeg *)malloc(sizeof(IomemSeg));
    manager.iomMsg.iomemSegArray = iomemSegArray;
    MOCKER(FindFdByNode).stubs().will(returnValue(0));
    MOCKER(reinterpret_cast<int (*)(int, int, int *)>(ioctl)).stubs().will(returnValue(0));
    int ret = GetIomemAddresses(&manager);
    EXPECT_EQ(0, ret);
}

extern "C" int GetTrackingAddr(struct ProcessManager *manager);
TEST_F(DeviceTest, TestGetTrackingAddr)
{
    struct ProcessManager manager;
    manager.nrNuma = 0;
    MOCKER(FreeAddrMemory).stubs().will(ignoreReturnValue());
    MOCKER(GetAcpiAddresses).stubs().will(returnValue(1));
    int ret = GetTrackingAddr(&manager);
    EXPECT_EQ(1, ret);
}

TEST_F(DeviceTest, TestGetTrackingAddrTwo)
{
    struct ProcessManager manager;
    manager.nrNuma = 0;
    MOCKER(FreeAddrMemory).stubs().will(ignoreReturnValue());
    MOCKER(GetAcpiAddresses).stubs().will(returnValue(0));
    MOCKER(GetIomemAddresses).stubs().will(returnValue(1));
    int ret = GetTrackingAddr(&manager);
    EXPECT_EQ(1, ret);
}

TEST_F(DeviceTest, TestGetTrackingAddrThree)
{
    struct ProcessManager manager;
    manager.nrNuma = 0;
    MOCKER(FreeAddrMemory).stubs().will(ignoreReturnValue());
    MOCKER(GetAcpiAddresses).stubs().will(returnValue(0));
    MOCKER(GetIomemAddresses).stubs().will(returnValue(0));
    int ret = GetTrackingAddr(&manager);
    EXPECT_EQ(0, ret);
}

TEST_F(DeviceTest, TestConfigureTrackingDevicesOne)
{
    struct ProcessManager manager;
    manager.nrNuma = 0;
    int ret = ConfigureTrackingDevices(&manager);
    EXPECT_EQ(-ENODEV, ret);
}

TEST_F(DeviceTest, TestConfigureTrackingDevicesTwo)
{
    struct ProcessManager manager;
    manager.nrNuma = 1;
    MOCKER(ConfigTrackingDev).stubs().will(returnValue(-1));
    int ret = ConfigureTrackingDevices(&manager);
    EXPECT_EQ(-1, ret);
}

TEST_F(DeviceTest, TestConfigureTrackingDevicesThree)
{
    struct ProcessManager manager;
    manager.nrNuma = 1;
    MOCKER(ConfigTrackingDev).stubs().will(returnValue(0));
    MOCKER(GetTrackingAddr).stubs().will(returnValue(-1));
    int ret = ConfigureTrackingDevices(&manager);
    EXPECT_EQ(-1, ret);
}

TEST_F(DeviceTest, TestConfigureTrackingDevicesFour)
{
    struct ProcessManager manager;
    manager.nrNuma = 1;
    MOCKER(ConfigTrackingDev).stubs().will(returnValue(0));
    MOCKER(GetTrackingAddr).stubs().will(returnValue(0));
    MOCKER(InitNodeActcData).stubs().will(returnValue(0));
    int ret = ConfigureTrackingDevices(&manager);
    EXPECT_EQ(0, ret);
}

TEST_F(DeviceTest, TestConfigureTrackingDevicesFive)
{
    struct ProcessManager manager;
    manager.nrNuma = 1;
    MOCKER(ConfigTrackingDev).stubs().will(returnValue(0));
    MOCKER(GetTrackingAddr).stubs().will(returnValue(0));
    MOCKER(InitNodeActcData).stubs().will(returnValue(1));
    MOCKER(FreeAddrMemory).stubs().will(ignoreReturnValue());
    int ret = ConfigureTrackingDevices(&manager);
    EXPECT_EQ(1, ret);
}

extern "C" int GetRamIsChange(struct ProcessManager *manager, int *change);
TEST_F(DeviceTest, TestGetRamIsChangeOne)
{
    struct ProcessManager manager;
    int *change;
    MOCKER(FindFdByNode).stubs().will(returnValue(-1));
    int ret = GetRamIsChange(&manager, change);
    EXPECT_EQ(-1, ret);
}

TEST_F(DeviceTest, TestGetRamIsChangeTwo)
{
    struct ProcessManager manager;
    int *change;
    MOCKER(FindFdByNode).stubs().will(returnValue(0));
    MOCKER(reinterpret_cast<int (*)(int, int, int *)>(ioctl)).stubs().will(returnValue(-1));
    int ret = GetRamIsChange(&manager, change);
    EXPECT_EQ(-1, ret);
}

TEST_F(DeviceTest, TestGetRamIsChangeThree)
{
    struct ProcessManager manager;
    int *change;
    MOCKER(FindFdByNode).stubs().will(returnValue(0));
    MOCKER(reinterpret_cast<int (*)(int, int, int *)>(ioctl)).stubs().will(returnValue(0));
    int ret = GetRamIsChange(&manager, change);
    EXPECT_EQ(0, ret);
}