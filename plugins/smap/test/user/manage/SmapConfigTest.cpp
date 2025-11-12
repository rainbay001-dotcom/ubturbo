/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: smap smap config ut code
 * Create: 2025-04-06
 */

#include <cstdlib>
#include <fcntl.h>
#include <sys/mman.h>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include "securec.h"
#include "manage/manage.h"
#include "advanced-strategy/scene.h"
#include "manage/smap_config.h"

using namespace std;

class SmapConfigTest : public ::testing::Test {
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

extern "C" size_t CalcNumaPayloadNum(void);
TEST_F(SmapConfigTest, TestCalcNumaPayloadNum)
{
    int ret;
    MOCKER(GetNrLocalNuma).stubs().will(returnValue(2));
    ret = CalcNumaPayloadNum();
    EXPECT_EQ(2 * REMOTE_NUMA_NUM + REMOTE_NUMA_NUM, ret);
}

extern "C" size_t CalcNumaPayloadLen(void);
TEST_F(SmapConfigTest, TestCalcNumaPayloadLen)
{
    int ret;
    int nr = 2;
    MOCKER(CalcNumaPayloadNum).stubs().will(returnValue(nr));
    ret = CalcNumaPayloadLen();
    EXPECT_EQ(nr * CONFIG_NUMA_LEN, ret);
}

extern "C" size_t CalcNumaConfigLen(void);
TEST_F(SmapConfigTest, TestCalcNumaConfigLen)
{
    int ret;
    int len = 10;
    MOCKER(CalcNumaPayloadLen).stubs().will(returnValue(len));
    ret = CalcNumaConfigLen();
    EXPECT_EQ(len + PAYLOAD_HEADER_LEN, ret);
}

extern "C" size_t CalcProcessConfigLen(int nrProcess);
TEST_F(SmapConfigTest, TestCalcProcessConfigLen)
{
    int ret;
    int nrProcess = 4;

    ret = CalcProcessConfigLen(0);
    EXPECT_EQ(0, ret);

    ret = CalcProcessConfigLen(nrProcess);
    EXPECT_EQ(PAYLOAD_HEADER_LEN + (nrProcess * CONFIG_PROC_LEN), ret);
}

extern "C" size_t GetProcessConfigLen(struct PayloadHeader *header);
TEST_F(SmapConfigTest, TestGetProcessConfigLen)
{
    int ret;
    struct PayloadHeader header = { .len = 20 };

    ret = GetProcessConfigLen(&header);
    EXPECT_EQ(header.len + PAYLOAD_HEADER_LEN, ret);
}

extern "C" size_t CalcConfigLen(int nrProcess);
TEST_F(SmapConfigTest, TestCalcConfigLen)
{
    int ret;
    int nrProcess = 2;
    int numaConfigLen = 20;
    int processConfigLen = 30;

    MOCKER(CalcNumaConfigLen).stubs().will(returnValue(numaConfigLen));
    MOCKER(CalcProcessConfigLen).stubs().will(returnValue(processConfigLen));
    ret = CalcConfigLen(nrProcess);
    EXPECT_EQ(CONFIG_HEADER_LEN + numaConfigLen + processConfigLen, ret);
}

extern "C" char *JumpToNumaConfig(char *base);
TEST_F(SmapConfigTest, TestJumpToNumaConfig)
{
    char addr;
    char *ret;

    ret = JumpToNumaConfig(&addr);
    EXPECT_EQ(&addr + CONFIG_HEADER_LEN, ret);
}

extern "C" char *JumpToNumaPayload(char *numaBase);
TEST_F(SmapConfigTest, TestJumpToNumaPayload)
{
    char addr;
    char *ret;

    ret = JumpToNumaPayload(&addr);
    EXPECT_EQ(&addr + PAYLOAD_HEADER_LEN, ret);
}

extern "C" char *JumpToProcessConfig(char *base);
TEST_F(SmapConfigTest, TestJumpToProcessConfig)
{
    size_t numaConfigLen = 20;
    char addr;
    char *ret;

    MOCKER(JumpToNumaConfig).stubs().will(returnValue(&addr));
    MOCKER(CalcNumaConfigLen).stubs().will(returnValue(numaConfigLen));
    ret = JumpToProcessConfig(&addr);
    EXPECT_EQ(&addr + numaConfigLen, ret);
}

extern "C" int RemoveConfig(void);
extern "C" int OpenConfig(void);
extern "C" char *JumpToProcessPayload(char *processBase);
TEST_F(SmapConfigTest, TestJumpToProcessPayload)
{
    char addr;
    char *ret;

    ret = JumpToProcessPayload(&addr);
    EXPECT_EQ(&addr + PAYLOAD_HEADER_LEN, ret);
}

extern "C" bool DoesConfigExist(void);
TEST_F(SmapConfigTest, TestDoesConfigExist)
{
    bool ret;

    MOCKER(access).stubs().will(returnValue(NORMAL_ERR));
    ret = DoesConfigExist();
    EXPECT_FALSE(ret);

    GlobalMockObject::verify();
    MOCKER(access).stubs().will(returnValue(0));
    ret = DoesConfigExist();
    EXPECT_TRUE(ret);
}

extern "C" int RemoveConfig(void);
TEST_F(SmapConfigTest, TestRemoveConfig)
{
    int ret;

    MOCKER(unlink).stubs().will(returnValue(NORMAL_ERR));
    errno = EINVAL;
    ret = RemoveConfig();
    EXPECT_EQ(-EINVAL, ret);

    GlobalMockObject::verify();
    MOCKER(unlink).stubs().will(returnValue(NORMAL_ERR));
    errno = ENOENT;
    ret = RemoveConfig();
    EXPECT_EQ(0, ret);
}

extern "C" int OpenConfig(void);
TEST_F(SmapConfigTest, TestOpenConfig)
{
    int ret;

    MOCKER(reinterpret_cast<int (*)(const char *, int)>(open)).stubs().will(returnValue(-1));
    errno = EINVAL;
    ret = OpenConfig();
    EXPECT_EQ(-EINVAL, ret);

    GlobalMockObject::verify();
    MOCKER(reinterpret_cast<int (*)(const char *, int)>(open)).stubs().will(returnValue(0));
    ret = OpenConfig();
    EXPECT_EQ(0, ret);
}

extern "C" int RemoveAndOpenConfig(bool remove);
TEST_F(SmapConfigTest, TestRemoveAndOpenConfig)
{
    int ret;

    MOCKER(RemoveConfig).expects(once()).will(returnValue(0));
    MOCKER(OpenConfig).stubs().will(returnValue(0));
    ret = RemoveAndOpenConfig(true);
    EXPECT_EQ(0, ret);
}

TEST_F(SmapConfigTest, TestRemoveAndOpenConfigTwo)
{
    int ret;

    MOCKER(RemoveConfig).stubs().will(returnValue(-EINVAL));
    ret = RemoveAndOpenConfig(true);
    EXPECT_EQ(-EINVAL, ret);

    GlobalMockObject::verify();
    MOCKER(RemoveConfig).stubs().will(returnValue(0));
    MOCKER(OpenConfig).stubs().will(returnValue(-ENOENT));
    ret = RemoveAndOpenConfig(true);
    EXPECT_EQ(-ENOENT, ret);
}

extern "C" int TruncateConfig(int fd, size_t len);
TEST_F(SmapConfigTest, TestTruncateConfig)
{
    int ret;
    int fd;
    size_t len;

    MOCKER(ftruncate).stubs().will(returnValue(NORMAL_ERR));
    errno = EINVAL;
    ret = TruncateConfig(fd, len);
    EXPECT_EQ(-EINVAL, ret);

    GlobalMockObject::verify();
    MOCKER(ftruncate).stubs().will(returnValue(0));
    ret = TruncateConfig(fd, len);
    EXPECT_EQ(0, ret);
}

extern "C" char *MapConfig(int fd, size_t len, int prot, int flags);
TEST_F(SmapConfigTest, TestMapConfig)
{
    char *ret;
    char addr;
    int fd;
    size_t len;
    int prot;
    int flags;

    MOCKER(mmap).stubs().will(returnValue(MAP_FAILED));
    ret = MapConfig(fd, len, prot, flags);
    EXPECT_EQ(nullptr, ret);

    GlobalMockObject::verify();
    MOCKER(mmap).stubs().will(returnValue(reinterpret_cast<void *>(&addr)));
    ret = MapConfig(fd, len, prot, flags);
    EXPECT_EQ(&addr, ret);
}

extern "C" void WriteRunMode(char *addr, RunMode mode);
TEST_F(SmapConfigTest, TestWriteRunMode)
{
    struct SmapConfigHeader header = { .runMode = WATERLINE_MODE };
    RunMode runMode = MEM_POOL_MODE;
    WriteRunMode((char *)&header, runMode);
    EXPECT_EQ(header.runMode, runMode);
}

extern "C" void WriteHeader(char *addr, RunMode mode, size_t len);
TEST_F(SmapConfigTest, TestWriteHeader)
{
    int ret;
    RunMode runMode = MEM_POOL_MODE;
    size_t len = 20;
    struct SmapConfigHeader header = { 0 };

    ASSERT_EQ(0, header.ver);
    ASSERT_EQ(0, header.headerLen);
    ASSERT_EQ(0, header.totalLen);
    WriteHeader((char *)&header, runMode, len);
    EXPECT_EQ(SMAP_CONFIG_VER, header.ver);
    EXPECT_EQ(runMode, header.runMode);
    EXPECT_EQ(CONFIG_HEADER_LEN, header.headerLen);
    EXPECT_EQ(len, header.totalLen);
}

extern "C" char *JumpToNumaPayload(char *numaBase);
extern "C" void WriteNumaConfig(char *base);
TEST_F(SmapConfigTest, TestWriteNumaConfig)
{
    int ret;
    int nrLocal = 4;
    struct PayloadHeader header;
    struct ProcessManager manager = {};
    manager.remoteNumaInfo.privateSize[0][0] = 128;
    struct NumaPayload *payload = (struct NumaPayload *)calloc((nrLocal + 1) * REMOTE_NUMA_NUM, sizeof(*payload));

    ASSERT_NE(nullptr, payload);

    MOCKER(GetNrLocalNuma).stubs().will(returnValue(nrLocal));
    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    MOCKER(JumpToNumaPayload).stubs().will(returnValue(reinterpret_cast<char *>(payload)));
    WriteNumaConfig((char *)&header);
    EXPECT_EQ(128, payload[0].size);
    free(payload);
}

extern "C" void UnmapConfig(char *addr, size_t len);
extern "C" int InitSmapConfig(int fd);
TEST_F(SmapConfigTest, TestInitSmapConfigErrOne)
{
    int ret;
    int fd;
    char addr;

    MOCKER(TruncateConfig).stubs().will(returnValue(-ENOENT));
    MOCKER(MapConfig).stubs().will(returnValue(&addr));
    ret = InitSmapConfig(fd);
    EXPECT_EQ(-ENOENT, ret);
}

TEST_F(SmapConfigTest, TestInitSmapConfigErrTwo)
{
    int ret;
    int fd;
    char addr;

    MOCKER(TruncateConfig).stubs().will(returnValue(0));
    MOCKER(MapConfig).stubs().will(returnValue(static_cast<char *>(nullptr)));
    ret = InitSmapConfig(fd);
    EXPECT_EQ(-EBADF, ret);
}

TEST_F(SmapConfigTest, TestInitSmapConfig)
{
    int ret;
    int fd;
    char addr;

    MOCKER(TruncateConfig).stubs().will(returnValue(0));
    MOCKER(MapConfig).stubs().will(returnValue(&addr));
    MOCKER(WriteHeader).stubs().will(ignoreReturnValue());
    MOCKER(WriteNumaConfig).stubs().will(ignoreReturnValue());
    MOCKER(UnmapConfig).stubs().will(ignoreReturnValue());
    ret = InitSmapConfig(fd);
    EXPECT_EQ(0, ret);
}

extern "C" bool IsConfigHeaderValid(struct SmapConfigHeader *header);
TEST_F(SmapConfigTest, TestIsConfigHeaderValid)
{
    bool ret;
    size_t numaConfigLen = 20;
    size_t minLen = CONFIG_HEADER_LEN + numaConfigLen;
    struct SmapConfigHeader header = { .ver = SMAP_CONFIG_VER, .headerLen = CONFIG_HEADER_LEN, .totalLen = minLen };

    MOCKER(CalcNumaConfigLen).stubs().will(returnValue(numaConfigLen));
    ret = IsConfigHeaderValid(&header);
    EXPECT_TRUE(ret);

    header.totalLen--;
    ret = IsConfigHeaderValid(&header);
    EXPECT_FALSE(ret);
}

TEST_F(SmapConfigTest, TestIsConfigHeaderInValid)
{
    bool ret;
    struct SmapConfigHeader header = { .ver = 0 };
    ret = IsConfigHeaderValid(&header);
    EXPECT_FALSE(ret);
}

extern "C" bool IsRunModeValid(RunMode runMode);
TEST_F(SmapConfigTest, TestIsConfigHeaderValidTwo)
{
    bool ret;
    struct SmapConfigHeader header = { .ver = SMAP_CONFIG_VER };
    MOCKER(IsRunModeValid).stubs().will(returnValue(false));
    ret = IsConfigHeaderValid(&header);
    EXPECT_FALSE(ret);

    GlobalMockObject::verify();
    MOCKER(IsRunModeValid).stubs().will(returnValue(true));
    header.headerLen = CONFIG_HEADER_LEN;
    ret = IsConfigHeaderValid(&header);
    EXPECT_FALSE(ret);
}

extern "C" void ReadHeader(char *addr, struct SmapConfigHeader *header);
TEST_F(SmapConfigTest, TestReadHeader)
{
    struct SmapConfigHeader src = {
        .ver = SMAP_CONFIG_VER,
        .runMode = MEM_POOL_MODE,
        .headerLen = CONFIG_HEADER_LEN,
        .totalLen = 30
    };
    struct SmapConfigHeader dest = { 0 };

    ReadHeader((char *)&src, &dest);
    EXPECT_EQ(src.ver, dest.ver);
    EXPECT_EQ(src.runMode, dest.runMode);
    EXPECT_EQ(src.headerLen, dest.headerLen);
    EXPECT_EQ(src.totalLen, dest.totalLen);
}

extern "C" int ParseHeader(int fd, struct SmapConfigHeader *header);
TEST_F(SmapConfigTest, TestParseHeader)
{
    int ret;
    int fd;
    struct SmapConfigHeader header;
    char addr;

    MOCKER(MapConfig).stubs().will(returnValue(static_cast<char *>(nullptr)));
    ret = ParseHeader(fd, &header);
    EXPECT_EQ(-EBADF, ret);

    GlobalMockObject::verify();
    MOCKER(MapConfig).stubs().will(returnValue(&addr));
    MOCKER(ReadHeader).stubs().will(ignoreReturnValue());
    MOCKER(IsConfigHeaderValid).stubs().will(returnValue(true));
    MOCKER(UnmapConfig).expects(once());
    ret = ParseHeader(fd, &header);
    EXPECT_EQ(0, ret);
}

extern "C" bool IsRunModeValid(RunMode runMode);
TEST_F(SmapConfigTest, TestIsRunModeValidErr)
{
    int ret;
    int runMode = -1;

    ret = IsRunModeValid((RunMode)runMode);
    EXPECT_FALSE(ret);

    runMode = MAX_RUN_MODE;
    ret = IsRunModeValid((RunMode)runMode);
    EXPECT_FALSE(ret);
}

TEST_F(SmapConfigTest, TestIsRunModeValidNormal)
{
    int ret;
    int runMode = WATERLINE_MODE;

    ret = IsRunModeValid((RunMode)runMode);
    EXPECT_TRUE(ret);

    runMode = MEM_POOL_MODE;
    ret = IsRunModeValid((RunMode)runMode);
    EXPECT_TRUE(ret);
}

extern "C" void RecoverRunMode(RunMode runMode);
TEST_F(SmapConfigTest, TestRecoverRunMode)
{
    RunMode runMode = MEM_POOL_MODE;

    RecoverRunMode(runMode);
    EXPECT_EQ(runMode, GetRunMode());
    EXPECT_EQ(runMode == WATERLINE_MODE, GetAdaptMem());
}

extern "C" size_t CalcNumaPayloadLen(void);
extern "C" size_t CalcNumaConfigLen(void);
extern "C" bool IsNumaConfigValid(char *numaBase, size_t totalLen);
TEST_F(SmapConfigTest, TestIsNumaConfigValid)
{
    bool ret;
    size_t totalLen;
    size_t numaPayloadLen = 32;
    struct PayloadHeader header = { 0 };

    header.len = (uint32_t)numaPayloadLen;
    totalLen = (size_t)numaPayloadLen + PAYLOAD_HEADER_LEN + CONFIG_HEADER_LEN;
    MOCKER(CalcNumaPayloadLen).stubs().will(returnValue(numaPayloadLen));
    MOCKER(CalcNumaConfigLen).stubs().will(returnValue(numaPayloadLen + PAYLOAD_HEADER_LEN));
    ret = IsNumaConfigValid((char *)&header, totalLen);
    EXPECT_TRUE(ret);

    header.len--;
    ret = IsNumaConfigValid((char *)&header, totalLen);
    EXPECT_FALSE(ret);
}

extern "C" int RecoverNumaConfig(char *numaBase);
TEST_F(SmapConfigTest, TestRecoverNumaConfig)
{
    int ret;
    int nrLocal = 2;
    int local1 = 0;
    int local2 = 1;
    int remote1 = 5;
    int remote2 = 7;
    char numaBase;
    struct ProcessManager manager = {0};
    struct NumaPayload *payload = (struct NumaPayload *)calloc((nrLocal + 1) * REMOTE_NUMA_NUM, sizeof(*payload));
    payload[local1 * REMOTE_NUMA_NUM + remote1 - nrLocal].size = 1024; // local1-remote1 has 1024MB
    payload[local2 * REMOTE_NUMA_NUM + remote2 - nrLocal].size = 512; // local2-remote2 has 512MB

    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    MOCKER(GetNrLocalNuma).stubs().will(returnValue(nrLocal));
    MOCKER(JumpToNumaPayload).stubs().will(returnValue(reinterpret_cast<char *>(payload)));
    MOCKER(CalcRemoteBorrowPages).stubs().will(returnValue((unsigned long)0));
    ret = RecoverNumaConfig(&numaBase);
    EXPECT_EQ(0, ret);
    free(payload);
}

extern "C" void WriteOneNumaConfig(char *numaBase, struct NumaPayload *np);
TEST_F(SmapConfigTest, TestWriteOneNumaConfig)
{
    int nrLocal = 2;
    int index;
    uint8_t localNid = 1;
    uint8_t remoteNid = 6;
    uint32_t size = 128;
    char numaBase;
    struct NumaPayload np = { .local = localNid, .remote = remoteNid, .size = size };
    struct NumaPayload *payload;

    payload = (struct NumaPayload *)calloc((nrLocal + 1) * REMOTE_NUMA_NUM, sizeof(*payload));
    ASSERT_NE(nullptr, payload);
    for (int i = 0; i < nrLocal; i++) {
        for (int j = 0; j < REMOTE_NUMA_NUM; j++) {
            payload[i * REMOTE_NUMA_NUM + j].local = i;
            payload[i * REMOTE_NUMA_NUM + j].remote = j;
            payload[i * REMOTE_NUMA_NUM + j].size = 0;
        }
    }
    index = localNid * REMOTE_NUMA_NUM + remoteNid - nrLocal;
    ASSERT_EQ(0, payload[index].size);

    MOCKER(GetNrLocalNuma).stubs().will(returnValue(nrLocal));
    MOCKER(JumpToNumaPayload).stubs().will(returnValue(reinterpret_cast<char *>(payload)));
    WriteOneNumaConfig(&numaBase, &np);
    EXPECT_EQ(size, payload[index].size);

    free(payload);
}

extern "C" void AssignProcessAttr(ProcessAttr *attr, struct ProcessPayload *payload);
TEST_F(SmapConfigTest, TestAssignProcessAttr)
{
    ProcessAttr attr;
    struct ProcessPayload payload = {
        1025, 25, NORMAL_SCAN, VM_TYPE, PROC_MOVE, 0, 0x11, 200, 1
    };

    memset(&attr, 0, sizeof(ProcessAttr));
    ASSERT_EQ(attr.pid, 0);
    ASSERT_EQ(attr.initLocalMemRatio, 0);
    ASSERT_EQ(attr.type, 0);
    ASSERT_EQ(attr.state, 0);
    ASSERT_EQ(attr.scanType, 0);
    ASSERT_EQ(attr.scanTime, 0);
    ASSERT_EQ(attr.numaAttr.numaNodes, 0);
    AssignProcessAttr(&attr, &payload);
    EXPECT_EQ(attr.pid, payload.pid);
    EXPECT_EQ(attr.initLocalMemRatio, payload.ratio);
    EXPECT_EQ(attr.type, payload.type);
    EXPECT_EQ(attr.state, payload.state);
    EXPECT_EQ(attr.scanType, payload.scanType);
    EXPECT_EQ(attr.scanTime, payload.scanTime);
    ASSERT_EQ(attr.numaAttr.numaNodes, payload.numaNodes);
}

extern "C" size_t CalcNumaConfigLen(void);
extern "C" bool IsProcessConfigValid(char *processBase, size_t totalLen);
TEST_F(SmapConfigTest, TestIsProcessConfigValid)
{
    bool ret;
    int nrProcess = 2;
    size_t totalLen;
    size_t numaConfigLen = 10;
    struct PayloadHeader header = { 0 };

    header.len = (uint32_t)nrProcess * CONFIG_PROC_LEN;
    totalLen = (size_t)header.len + PAYLOAD_HEADER_LEN + CONFIG_HEADER_LEN + numaConfigLen;
    MOCKER(CalcNumaConfigLen).stubs().will(returnValue(numaConfigLen));
    ret = IsProcessConfigValid((char *)&header, totalLen);
    EXPECT_TRUE(ret);

    header.len--;
    ret = IsProcessConfigValid((char *)&header, totalLen);
    EXPECT_FALSE(ret);
}

extern "C" bool HasProcessConfig(size_t totalLen);
TEST_F(SmapConfigTest, TestHasProcessConfig)
{
    bool ret;
    size_t numaConfigLen = 10;
    size_t totalLen = numaConfigLen + CONFIG_HEADER_LEN;

    MOCKER(CalcNumaConfigLen).stubs().will(returnValue(numaConfigLen));
    ret = HasProcessConfig(totalLen);
    EXPECT_FALSE(ret);

    totalLen++;
    ret = HasProcessConfig(totalLen);
    EXPECT_TRUE(ret);
}

extern "C" char *JumpToProcessPayload(char *processBase);
extern "C" int RecoverProcessConfig(char *processBase);
TEST_F(SmapConfigTest, TestRecoverProcessConfig)
{
    int ret;
    int nrProcess = 2;
    ProcessAttr *attr;
    struct PayloadHeader header = { .len = nrProcess * CONFIG_PROC_LEN };
    struct ProcessManager manager = { .processes = nullptr };
    struct ProcessPayload payload[] = {
        { 1025, 25, NORMAL_SCAN, VM_TYPE, { 1 }, { 5 }, 200 },
        { 1026, 15, NORMAL_SCAN, VM_TYPE, { 2 }, { 6 }, 200 }
    };

    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    MOCKER(JumpToProcessPayload).stubs().will(returnValue((char *)payload));
    ret = RecoverProcessConfig((char *)&header);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(nrProcess, manager.nr[VM_TYPE]);
    EXPECT_EQ(payload[0].pid, manager.processes->next->pid);
    EXPECT_EQ(payload[1].pid, manager.processes->pid);

    attr = manager.processes;
    while (attr) {
        LinkedListRemove(&attr, &manager.processes);
        attr = manager.processes;
    }
    ASSERT_EQ(nullptr, manager.processes);
}

TEST_F(SmapConfigTest, TestRecoverProcessConfigTwo)
{
    int ret;
    int nrProcess = 1;
    struct PayloadHeader header = { .len = nrProcess * CONFIG_PROC_LEN };
    struct ProcessManager manager = { .processes = nullptr };
    struct ProcessPayload payload[] = {
        { 1025, 25, NORMAL_SCAN, VM_TYPE, { 1 }, { 5 }, 200 },
    };

    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    MOCKER(JumpToProcessPayload).stubs().will(returnValue((char *)payload));
    MOCKER(calloc).stubs().will(returnValue(static_cast<void *>(nullptr)));
    ret = RecoverProcessConfig((char *)&header);
    EXPECT_EQ(-ENOMEM, ret);
    ASSERT_EQ(nullptr, manager.processes);
}

extern "C" int WriteProcessConfig(char *processBase, struct ProcessPayload *p, int nrPayload);
TEST_F(SmapConfigTest, TestWriteProcessConfig)
{
    int ret;
    int nrProcess = 2;
    struct PayloadHeader header;
    struct ProcessPayload np[] = {
        { 1025, 25, NORMAL_SCAN, VM_TYPE, { 1 }, { 5 }, 200 },
        { 1026, 15, NORMAL_SCAN, VM_TYPE, { 2 }, { 6 }, 200 }
    };
    struct ProcessPayload *payload;

    payload = (struct ProcessPayload *)calloc(nrProcess, sizeof(*payload));
    ASSERT_NE(nullptr, payload);

    MOCKER(JumpToProcessPayload).stubs().will(returnValue(reinterpret_cast<char *>(payload)));
    ret = WriteProcessConfig((char *)&header, np, nrProcess);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(payload[0].pid, np[0].pid);
    EXPECT_EQ(payload[1].pid, np[1].pid);

    free(payload);
}

extern "C" int BuildAllProcessPayload(struct ProcessPayload **payload, int *len);
TEST_F(SmapConfigTest, TestBuildAllProcessPayload)
{
    int ret;
    int nrProcess = 2;
    int len;
    struct ProcessManager manager = { .processes = nullptr, .nr = { 0, 2 } };
    ProcessAttr *attr;
    ProcessAttr *attr1 = nullptr;
    ProcessAttr *attr2 = nullptr;
    struct ProcessPayload *payload = nullptr;

    attr1 = (ProcessAttr *)malloc(sizeof(*attr1));
    attr2 = (ProcessAttr *)malloc(sizeof(*attr2));

    ASSERT_NE(nullptr, attr1);
    ASSERT_NE(nullptr, attr2);

    attr1->type = VM_TYPE;
    attr1->pid = 1025;
    attr1->initLocalMemRatio = 25;
    attr1->state = PROC_IDLE;
    attr1->scanTime = 200;
    attr1->scanType = NORMAL_SCAN;
    attr1->numaAttr.numaNodes = 0x22;
    attr2->type = VM_TYPE;
    attr2->pid = 1026;
    attr2->initLocalMemRatio = 15;
    attr2->state = PROC_IDLE;
    attr2->scanTime = 200;
    attr2->scanType = NORMAL_SCAN;
    attr2->numaAttr.numaNodes = 0x44;

    LinkedListAdd(&manager.processes, &attr1);
    LinkedListAdd(&manager.processes, &attr2);

    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    MOCKER(GetPidType).stubs().will(returnValue(VM_TYPE));
    ret = BuildAllProcessPayload(&payload, &len);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(2, len);
    EXPECT_NE(nullptr, payload);
    EXPECT_EQ(attr2->pid, payload[0].pid);
    EXPECT_EQ(attr1->pid, payload[1].pid);

    attr = manager.processes;
    while (attr) {
        LinkedListRemove(&attr, &manager.processes);
        attr = manager.processes;
    }
    ASSERT_EQ(nullptr, manager.processes);
    free(payload);
}

TEST_F(SmapConfigTest, TestBuildAllProcessPayloadTwo)
{
    int ret;
    int len;
    struct ProcessManager manager = { .processes = nullptr, .nr = { 0, 2 } };
    ProcessAttr *attr;
    ProcessAttr *attr1 = nullptr;
    ProcessAttr *attr2 = nullptr;
    struct ProcessPayload *payload = nullptr;

    attr1 = (ProcessAttr *)malloc(sizeof(*attr1));
    attr2 = (ProcessAttr *)malloc(sizeof(*attr2));

    ASSERT_NE(nullptr, attr1);
    ASSERT_NE(nullptr, attr2);

    attr1->type = PROCESS_TYPE;
    attr1->pid = 1234;
    attr1->initLocalMemRatio = 50;
    attr1->state = PROC_IDLE;
    attr1->scanTime = 2000;
    attr1->scanType = NORMAL_SCAN;
    attr1->numaAttr.numaNodes = 0x22;
    attr2->type = VM_TYPE;
    attr2->pid = 1235;
    attr2->initLocalMemRatio = 40;
    attr2->state = PROC_IDLE;
    attr2->scanTime = 3000;
    attr2->scanType = HAM_SCAN;
    attr2->numaAttr.numaNodes = 0x44;

    LinkedListAdd(&manager.processes, &attr1);
    LinkedListAdd(&manager.processes, &attr2);

    MOCKER(GetProcessManager).stubs().will(returnValue(&manager));
    MOCKER(GetPidType).stubs().will(returnValue(VM_TYPE));
    ret = BuildAllProcessPayload(&payload, &len);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(2, len);
    EXPECT_NE(nullptr, payload);
    EXPECT_EQ(attr2->pid, payload[0].pid);
    EXPECT_EQ(attr1->pid, payload[1].pid);

    attr = manager.processes;
    while (attr) {
        LinkedListRemove(&attr, &manager.processes);
        attr = manager.processes;
    }
    ASSERT_EQ(nullptr, manager.processes);
    free(payload);
}

extern "C" int MapAndWriteProcessConfig(int fd, size_t mapLen, struct ProcessPayload *payload, int nrPayload);
TEST_F(SmapConfigTest, TestMapAndWriteProcessConfig)
{
    int ret;
    int fd;
    int nrPayload = 1;
    char addr;
    char processBase;
    size_t mapLen;
    struct ProcessPayload payload;

    MOCKER(MapConfig).stubs().will(returnValue(static_cast<char *>(nullptr)));
    ret = MapAndWriteProcessConfig(fd, mapLen, &payload, nrPayload);
    EXPECT_EQ(-EBADF, ret);

    GlobalMockObject::verify();
    MOCKER(MapConfig).stubs().will(returnValue(&addr));
    MOCKER(JumpToProcessConfig).stubs().will(returnValue(&processBase));
    MOCKER(WriteProcessConfig).stubs().will(returnValue(-EINVAL));
    ret = MapAndWriteProcessConfig(fd, mapLen, &payload, nrPayload);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(SmapConfigTest, TestMapAndWriteProcessConfigZeroPayload)
{
    int ret;
    int fd;
    int nrPayload = 0;
    char addr;
    size_t mapLen;
    struct ProcessPayload payload;

    MOCKER(MapConfig).stubs().will(returnValue(&addr));
    MOCKER(JumpToProcessConfig).expects(never()).will(returnValue(static_cast<char *>(nullptr)));
    MOCKER(WriteHeader).stubs().will(ignoreReturnValue());
    MOCKER(UnmapConfig).stubs().will(ignoreReturnValue());
    ret = MapAndWriteProcessConfig(fd, mapLen, &payload, nrPayload);
    EXPECT_EQ(0, ret);
}

extern "C" int ChangeProcessConfig(int fd);
TEST_F(SmapConfigTest, TestChangeProcessConfigExtendFile)
{
    int ret;
    int fd;
    size_t oldLen = 30;
    size_t newLen = 48;
    struct SmapConfigHeader header = { .ver = SMAP_CONFIG_VER, .headerLen = CONFIG_HEADER_LEN, .totalLen = oldLen };

    MOCKER(ParseHeader).stubs().with(any(), outBoundP(&header, sizeof(header))).will(returnValue(0));
    MOCKER(BuildAllProcessPayload).stubs().will(returnValue(0));
    MOCKER(CalcConfigLen).stubs().will(returnValue(newLen));
    MOCKER(TruncateConfig).stubs().will(returnValue(0));
    MOCKER(MapAndWriteProcessConfig).stubs().will(returnValue(-ENOENT));
    ret = ChangeProcessConfig(fd);
    EXPECT_EQ(-ENOENT, ret);
}

TEST_F(SmapConfigTest, TestChangeProcessConfigShrinkFile)
{
    int ret;
    int fd;
    size_t oldLen = 30;
    size_t newLen = 20;
    struct SmapConfigHeader header = { .ver = SMAP_CONFIG_VER, .headerLen = CONFIG_HEADER_LEN, .totalLen = oldLen };

    MOCKER(ParseHeader).stubs().with(any(), outBoundP(&header, sizeof(header))).will(returnValue(0));
    MOCKER(BuildAllProcessPayload).stubs().will(returnValue(0));
    MOCKER(CalcConfigLen).stubs().will(returnValue(newLen));
    MOCKER(TruncateConfig).stubs().will(returnValue(-EPERM));
    MOCKER(MapAndWriteProcessConfig).stubs().will(returnValue(0));
    ret = ChangeProcessConfig(fd);
    EXPECT_EQ(0, ret);
}

TEST_F(SmapConfigTest, TestChangeProcessConfig)
{
    int fd;
    MOCKER(ParseHeader).stubs().will(returnValue(-EBADF));
    int ret = ChangeProcessConfig(fd);
    EXPECT_EQ(-EBADF, ret);

    GlobalMockObject::verify();
    MOCKER(ParseHeader).stubs().will(returnValue(0));
    MOCKER(BuildAllProcessPayload).stubs().will(returnValue(-EINVAL));
    ret = ChangeProcessConfig(fd);
    EXPECT_EQ(-EINVAL, ret);
}

extern "C" char *JumpToNumaConfig(char *base);
extern "C" int ParseConfig(int fd, struct SmapConfigHeader *header);
TEST_F(SmapConfigTest, TestParseConfigOne)
{
    int ret;
    int fd;
    char base;
    char numaBase;
    struct SmapConfigHeader header;

    MOCKER(MapConfig).stubs().will(returnValue(static_cast<char *>(nullptr)));
    ret = ParseConfig(fd, &header);
    EXPECT_EQ(-EBADF, ret);

    GlobalMockObject::verify();
    MOCKER(MapConfig).stubs().will(returnValue(reinterpret_cast<char *>(&base)));
    MOCKER(IsRunModeValid).stubs().will(returnValue(true));
    MOCKER(RecoverRunMode).stubs().will(ignoreReturnValue());
    MOCKER(JumpToNumaConfig).stubs().will(returnValue(reinterpret_cast<char *>(&numaBase)));
    MOCKER(IsNumaConfigValid).stubs().will(returnValue(true));
    MOCKER(RecoverNumaConfig).stubs().will(returnValue(-ENOENT));
    MOCKER(UnmapConfig).expects(once());
    ret = ParseConfig(fd, &header);
    EXPECT_EQ(-ENOENT, ret);
}

extern "C" char *JumpToProcessConfig(char *base);
TEST_F(SmapConfigTest, TestParseConfigTwo)
{
    int ret;
    int fd;
    char base;
    char numaBase;
    char processBase;
    struct SmapConfigHeader header;

    MOCKER(MapConfig).stubs().will(returnValue(reinterpret_cast<char *>(&base)));
    MOCKER(IsRunModeValid).stubs().will(returnValue(true));
    MOCKER(RecoverRunMode).stubs().will(ignoreReturnValue());
    MOCKER(JumpToNumaConfig).stubs().will(returnValue(reinterpret_cast<char *>(&numaBase)));
    MOCKER(IsNumaConfigValid).stubs().will(returnValue(true));
    MOCKER(RecoverNumaConfig).stubs().will(returnValue(0));
    MOCKER(HasProcessConfig).stubs().will(returnValue(false));
    MOCKER(RecoverProcessConfig).expects(never());
    MOCKER(UnmapConfig).expects(once());
    ret = ParseConfig(fd, &header);
    EXPECT_EQ(0, ret);

    GlobalMockObject::verify();
    MOCKER(MapConfig).stubs().will(returnValue(reinterpret_cast<char *>(&base)));
    MOCKER(IsRunModeValid).stubs().will(returnValue(true));
    MOCKER(RecoverRunMode).stubs().will(ignoreReturnValue());
    MOCKER(JumpToNumaConfig).stubs().will(returnValue(reinterpret_cast<char *>(&numaBase)));
    MOCKER(IsNumaConfigValid).stubs().will(returnValue(true));
    MOCKER(RecoverNumaConfig).stubs().will(returnValue(0));
    MOCKER(HasProcessConfig).stubs().will(returnValue(true));
    MOCKER(JumpToProcessConfig).stubs().will(returnValue(reinterpret_cast<char *>(&processBase)));
    MOCKER(IsProcessConfigValid).stubs().will(returnValue(true));
    MOCKER(HasProcessConfig).stubs().will(returnValue(true));
    MOCKER(RecoverProcessConfig).stubs().will(returnValue(-EPERM));
    MOCKER(UnmapConfig).expects(once());
    ret = ParseConfig(fd, &header);
    EXPECT_EQ(-EPERM, ret);
}

TEST_F(SmapConfigTest, TestParseConfigThree)
{
    int ret;
    int fd;
    char base;
    char numaBase;
    struct SmapConfigHeader header;
    MOCKER(MapConfig).stubs().will(returnValue(reinterpret_cast<char *>(&base)));
    MOCKER(IsRunModeValid).stubs().will(returnValue(false));
    ret = ParseConfig(fd, &header);
    EXPECT_EQ(-EINVAL, ret);

    GlobalMockObject::verify();
    MOCKER(MapConfig).stubs().will(returnValue(reinterpret_cast<char *>(&base)));
    MOCKER(IsRunModeValid).stubs().will(returnValue(true));
    MOCKER(RecoverRunMode).stubs().will(ignoreReturnValue());
    MOCKER(JumpToNumaConfig).stubs().will(returnValue(reinterpret_cast<char *>(&numaBase)));
    MOCKER(IsNumaConfigValid).stubs().will(returnValue(false));
    ret = ParseConfig(fd, &header);
    EXPECT_EQ(-EINVAL, ret);
}

extern "C" int ReadConfig(int fd);
TEST_F(SmapConfigTest, TestReadConfig)
{
    int ret;
    int fd;

    MOCKER(ParseHeader).stubs().will(returnValue(-ENOENT));
    MOCKER(ParseConfig).expects(never());
    ret = ReadConfig(fd);
    EXPECT_EQ(-ENOENT, ret);

    GlobalMockObject::verify();
    MOCKER(ParseHeader).stubs().will(returnValue(0));
    MOCKER(ParseConfig).stubs().will(returnValue(-EPERM));
    ret = ReadConfig(fd);
    EXPECT_EQ(-EPERM, ret);
}

extern "C" int ChangeRunMode(int fd, RunMode runMode);
TEST_F(SmapConfigTest, TestChangeRunMode)
{
    int ret;
    int fd;
    char addr;
    RunMode runMode;

    MOCKER(MapConfig).stubs().will(returnValue(static_cast<char *>(nullptr)));
    ret = ChangeRunMode(fd, runMode);
    EXPECT_EQ(-EBADF, ret);

    GlobalMockObject::verify();
    MOCKER(MapConfig).stubs().will(returnValue(&addr));
    MOCKER(WriteRunMode).stubs().will(ignoreReturnValue());
    MOCKER(UnmapConfig).stubs().will(ignoreReturnValue());
    ret = ChangeRunMode(fd, runMode);
    EXPECT_EQ(0, ret);
}

extern "C" int ChangeOneNumaConfig(int fd, struct NumaPayload *payload);
TEST_F(SmapConfigTest, TestChangeOneNumaConfig)
{
    int ret;
    int fd;
    struct NumaPayload payload;

    MOCKER(ParseHeader).stubs().will(returnValue(-ENOENT));
    MOCKER(MapConfig).expects(never());
    ret = ChangeOneNumaConfig(fd, &payload);
    EXPECT_EQ(-ENOENT, ret);
}

TEST_F(SmapConfigTest, TestChangeOneNumaConfigTwo)
{
    int ret;
    int fd;
    struct NumaPayload payload;

    MOCKER(ParseHeader).stubs().will(returnValue(0));
    MOCKER(MapConfig).stubs().will(returnValue(static_cast<char *>(nullptr)));
    ret = ChangeOneNumaConfig(fd, &payload);
    EXPECT_EQ(-EBADF, ret);

    char *addr = (char *)malloc(sizeof(char));
    char *numaBase = (char *)malloc(sizeof(char));
    GlobalMockObject::verify();
    MOCKER(ParseHeader).stubs().will(returnValue(0));
    MOCKER(MapConfig).stubs().will(returnValue(addr));
    MOCKER(JumpToNumaConfig).stubs().will(returnValue(numaBase));
    MOCKER(IsNumaConfigValid).stubs().will(returnValue(false));
    ret = ChangeOneNumaConfig(fd, &payload);
    EXPECT_EQ(-EINVAL, ret);
    free(addr);
    free(numaBase);
}

TEST_F(SmapConfigTest, TestChangeOneNumaConfigThree)
{
    int ret;
    int fd;
    struct NumaPayload payload;

    char *addr = (char *)malloc(sizeof(char));
    char *numaBase = (char *)malloc(sizeof(char));
    MOCKER(ParseHeader).stubs().will(returnValue(0));
    MOCKER(MapConfig).stubs().will(returnValue(addr));
    MOCKER(JumpToNumaConfig).stubs().will(returnValue(numaBase));
    MOCKER(IsNumaConfigValid).stubs().will(returnValue(true));
    MOCKER(WriteOneNumaConfig).stubs().will(ignoreReturnValue());
    MOCKER(UnmapConfig).stubs().will(ignoreReturnValue());
    ret = ChangeOneNumaConfig(fd, &payload);
    EXPECT_EQ(0, ret);
    free(addr);
    free(numaBase);
}

extern "C" bool DoesConfigExist(void);
extern "C" int RemoveAndOpenConfig(bool remove);
TEST_F(SmapConfigTest, TestRecoverFromConfig)
{
    int ret;
    int fd = 1;

    MOCKER(DoesConfigExist).stubs().will(returnValue(false));
    MOCKER(RemoveAndOpenConfig).stubs().will(returnValue(fd));
    MOCKER(ReadConfig).expects(never());
    MOCKER(InitSmapConfig).stubs().will(returnValue(-ENOENT));
    MOCKER(close).expects(once()).will(returnValue(0));
    ret = RecoverFromConfig();
    EXPECT_EQ(-ENOENT, ret);

    GlobalMockObject::verify();
    MOCKER(DoesConfigExist).stubs().will(returnValue(true));
    MOCKER(RemoveAndOpenConfig).stubs().will(returnValue(fd));
    MOCKER(ReadConfig).stubs().will(returnValue(0));
    MOCKER(close).expects(once()).will(returnValue(0));
    ret = RecoverFromConfig();
    EXPECT_EQ(0, ret);
}

TEST_F(SmapConfigTest, TestRecoverFromConfigTwo)
{
    int ret;
    int fd = 1;
    MOCKER(DoesConfigExist).stubs().will(returnValue(true));
    MOCKER(RemoveAndOpenConfig).stubs().will(returnValue(-1));
    ret = RecoverFromConfig();
    EXPECT_EQ(-1, ret);

    GlobalMockObject::verify();
    MOCKER(DoesConfigExist).stubs().will(returnValue(true));
    MOCKER(RemoveAndOpenConfig).stubs().will(returnValue(fd)).then(returnValue(-ENOENT));
    MOCKER(ReadConfig).stubs().will(returnValue(1));
    MOCKER(close).expects(once()).will(returnValue(0));
    ret = RecoverFromConfig();
    EXPECT_EQ(-ENOENT, ret);
}

TEST_F(SmapConfigTest, TestSyncRunModeInvalidRunMode)
{
    int ret;
    int fd = 1;
    int runMode = -1;

    ret = SyncRunMode((RunMode)runMode);
    EXPECT_EQ(-EINVAL, ret);

    runMode = MAX_RUN_MODE;
    ret = SyncRunMode((RunMode)runMode);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(SmapConfigTest, TestSyncRunMode)
{
    int ret;
    int fd = 1;
    RunMode runMode = MEM_POOL_MODE;

    MOCKER(DoesConfigExist).stubs().will(returnValue(false));
    MOCKER(RemoveAndOpenConfig).stubs().will(returnValue(fd));
    MOCKER(InitSmapConfig).stubs().will(returnValue(0));
    MOCKER(ChangeRunMode).stubs().will(returnValue(-ENOENT));
    MOCKER(close).expects(once()).will(returnValue(0));
    ret = SyncRunMode(runMode);
    EXPECT_EQ(-ENOENT, ret);
}

TEST_F(SmapConfigTest, TestSyncRunModeTwo)
{
    int ret;
    RunMode runMode = MEM_POOL_MODE;

    MOCKER(DoesConfigExist).stubs().will(returnValue(false));
    MOCKER(RemoveAndOpenConfig).stubs().will(returnValue(-ENOENT));
    ret = SyncRunMode(runMode);
    EXPECT_EQ(-ENOENT, ret);
}

TEST_F(SmapConfigTest, TestSyncRunModeThree)
{
    int ret;
    int fd = 1;
    RunMode runMode = MEM_POOL_MODE;

    MOCKER(DoesConfigExist).stubs().will(returnValue(false));
    MOCKER(RemoveAndOpenConfig).stubs().will(returnValue(fd));
    MOCKER(InitSmapConfig).stubs().will(returnValue(-ENOENT));
    MOCKER(close).stubs().will(returnValue(0));
    ret = SyncRunMode(runMode);
    EXPECT_EQ(-ENOENT, ret);
}

TEST_F(SmapConfigTest, TestSyncOneNumaConfigInvalidLocal)
{
    int ret;
    int nrLocal = 2;
    int local = -1;
    int remote = 6;
    size_t size = 256;

    MOCKER(GetNrLocalNuma).stubs().will(returnValue(nrLocal));
    ret = SyncOneNumaConfig(local, remote, size);
    EXPECT_EQ(-EINVAL, ret);

    local = nrLocal;
    ret = SyncOneNumaConfig(local, remote, size);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(SmapConfigTest, TestSyncOneNumaConfigInvalidRemote)
{
    int ret;
    int nrLocal = 2;
    int local = 1;
    int remote = 1;
    size_t size = 256;

    MOCKER(GetNrLocalNuma).stubs().will(returnValue(nrLocal));
    ret = SyncOneNumaConfig(local, remote, size);
    EXPECT_EQ(-EINVAL, ret);

    remote = nrLocal + REMOTE_NUMA_NUM;
    ret = SyncOneNumaConfig(local, remote, size);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(SmapConfigTest, TestSyncOneNumaConfig)
{
    int ret;
    int fd = 1;
    int nrLocal = 2;
    int local = 1;
    int remote = 6;
    size_t size = 256;

    MOCKER(GetNrLocalNuma).stubs().will(returnValue(nrLocal));
    MOCKER(DoesConfigExist).stubs().will(returnValue(false));
    MOCKER(RemoveAndOpenConfig).stubs().will(returnValue(fd));
    MOCKER(InitSmapConfig).stubs().will(returnValue(0));
    MOCKER(ChangeOneNumaConfig).stubs().will(returnValue(-ENOENT));
    MOCKER(close).expects(once()).will(returnValue(0));
    ret = SyncOneNumaConfig(local, remote, size);
    EXPECT_EQ(-ENOENT, ret);
}

TEST_F(SmapConfigTest, TestSyncOneNumaConfigTwo)
{
    int ret;
    int nrLocal = 2;
    int local = 1;
    int remote = 6;
    size_t size = 256;

    MOCKER(GetNrLocalNuma).stubs().will(returnValue(nrLocal));
    MOCKER(DoesConfigExist).stubs().will(returnValue(false));
    MOCKER(RemoveAndOpenConfig).stubs().will(returnValue(-ENOENT));
    ret = SyncOneNumaConfig(local, remote, size);
    EXPECT_EQ(-ENOENT, ret);
}

TEST_F(SmapConfigTest, TestSyncOneNumaConfigSuccess)
{
    int ret;
    int fd = 1;
    int nrLocal = 2;
    int local = 1;
    int remote = 6;
    size_t size = 256;

    MOCKER(GetNrLocalNuma).stubs().will(returnValue(nrLocal));
    MOCKER(DoesConfigExist).stubs().will(returnValue(true));
    MOCKER(RemoveAndOpenConfig).stubs().will(returnValue(fd));
    MOCKER(ChangeOneNumaConfig).stubs().will(returnValue(0));
    MOCKER(close).expects(once()).will(returnValue(0));
    ret = SyncOneNumaConfig(local, remote, size);
    EXPECT_EQ(0, ret);
}

TEST_F(SmapConfigTest, TestSyncOneNumaConfigThree)
{
    int ret;
    int nrLocal = 2;
    int local = 1;
    int remote = 6;
    size_t size = 256;

    MOCKER(GetNrLocalNuma).stubs().will(returnValue(nrLocal));
    MOCKER(DoesConfigExist).stubs().will(returnValue(false));
    MOCKER(RemoveAndOpenConfig).stubs().will(returnValue(0));
    MOCKER(InitSmapConfig).stubs().will(returnValue(-ENOENT));
    MOCKER(close).stubs().will(returnValue(0));
    ret = SyncOneNumaConfig(local, remote, size);
    EXPECT_EQ(-ENOENT, ret);
}

TEST_F(SmapConfigTest, TestSyncAllProcessConfig)
{
    int ret;
    int fd = 1;

    MOCKER(DoesConfigExist).stubs().will(returnValue(false));
    MOCKER(RemoveAndOpenConfig).stubs().will(returnValue(fd));
    MOCKER(InitSmapConfig).stubs().will(returnValue(0));
    MOCKER(ChangeProcessConfig).stubs().will(returnValue(-ENOENT));
    MOCKER(close).expects(once()).will(returnValue(0));
    ret = SyncAllProcessConfig();
    EXPECT_EQ(-ENOENT, ret);
}

TEST_F(SmapConfigTest, TestSyncAllProcessConfigTwo)
{
    int ret;
    int fd = 1;

    MOCKER(DoesConfigExist).stubs().will(returnValue(false));
    MOCKER(RemoveAndOpenConfig).stubs().will(returnValue(-ENOENT));
    ret = SyncAllProcessConfig();
    EXPECT_EQ(-ENOENT, ret);

    GlobalMockObject::verify();
    MOCKER(DoesConfigExist).stubs().will(returnValue(false));
    MOCKER(RemoveAndOpenConfig).stubs().will(returnValue(fd));
    MOCKER(InitSmapConfig).stubs().will(returnValue(-EBADF));
    MOCKER(close).stubs().will(returnValue(0));
    ret = SyncAllProcessConfig();
    EXPECT_EQ(-EBADF, ret);
}