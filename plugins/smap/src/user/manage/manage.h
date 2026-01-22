/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef __MANAGE_H__
#define __MANAGE_H__

#include "virt.h"
#include "smap_env.h"
#include "numa_nodes.h"
#include "advanced-strategy/scene_info.h"

#define LOCAL_NUMA_NUM 4
#define REMOTE_NUMA_NUM 18
#define SHIFT_MB_TO_4K 8
#define SHIFT_MB_TO_2M 1
#define RESERVED_RATIO 0.05
#define RESERVED_MEMORY 200
#define MAX_4K_PROCESSES_CNT 300
#define MAX_2M_PROCESSES_CNT 100
#define MAX_THREADS 10
#define MAX_RES_LEN 4
#define PAGE_SHIFT 12
#define PAGE_SIZE (1UL << PAGE_SHIFT)
#define PAGE_MASK (~(PAGE_SIZE - 1))
#define TWO_MEGA_SHIFT 21
#define TWO_MEGA_SIZE (1UL << TWO_MEGA_SHIFT)
#define TWO_MEGA_MASK (~(TWO_MEGA_SIZE - 1))
#define DEFAULT_FD (-1)

#define RSS_LINE_PREFIX "Rss:"
#define RSS_LINE_PREFIX_LENGTH 4
#define HUGETLB_LINE_PREFIX "Private_Hugetlb:"
#define HUGETLB_LINE_PREFIX_LENGTH 16
#define KB 1024
#define PRESENT (1ULL << 63)
#define PRN_SHIFT ((1ULL << 55) - 1)
#define MAPS_LIN_LEN 2
#define MAPS_MAX_LEN 256

#define BUFFER_SIZE 256
#define PAGEMAP_ENTRY_SIZE 8

#define DEFAULT_L1_NODE (-1)
#define DEFAULT_L2_NODE (-1)
#define DEFAULT_DEST_NODE (-1)

#define KB_TO_2M 11
#define KB_TO_4K 2
#define BIT_TO_BYTE 8
#define BYTES_PER_LONG 8
#define CPU_NUMA_PATH "/sys/devices/system/cpu/cpu%d/node%d"
#define NUMAMAP_HUGE_SUBSTR "kernelpagesize_kB=2048"

#define MMAP_TYPE_STRING_LEN 20
#define MMAP_TYPE_SHARED_SEG1 "memAccess='shared'"
#define MMAP_TYPE_SHARED_SEG2 "access mode='shared'"

#define PERIOD_CONFIG_PATH "/opt/ubturbo/conf/smap/period.config"
#define DEFAULT_NMEMB 1
#define MAX_MIGRATE_BACK_WAIT_TIME 60
#define MIGRATE_BACK_CHECK_PERIOD 1000
#define MAX_FRESH_USED_TIME 20
#define WAIT_FRESH_USED_PERIOD 200
#define MAX_CHECK_ALREADY_FORBIDDEN_TIME 100
#define WAIT_CHECK_ALREADY_FORBIDDEN_PERIOD 200

#define WAIT_PROC_STATE_PERIOD 100
#define WAIT_PROC_STATE_MAX_RETRY 300
#define MAX_NR_MIGRATE_NUMA_RANGE 50

#define PID_CMD_LENGTH 64
#define MAX_LINE_LENGTH 1024

extern EnvAtomic g_forbiddenNodes[MAX_NODES];

typedef uint16_t actc_t;

typedef enum {
    WATERLINE_MODE = 0,
    MEM_POOL_MODE,
    MAX_RUN_MODE,
} RunMode;

typedef enum {
    PROCESS_TYPE = 0,
    VM_TYPE,
    TYPE_MAX,
} PidType;

typedef enum {
    L1,
    L2,
    NR_LEVEL,
} NodeLevel;

typedef enum {
    DEMOTE,
    PROMOTE,
    SWAP,
} MigrateDirection;

typedef enum { MMAP_PARIVATE, MMAP_SHARED, NR_MMAP_TYPE } MmapType;

enum {
    DISABLE_PROCESS_MIGRATE,
    ENABLE_PROCESS_MIGRATE,
};

typedef struct {
    uint64_t addr;
    actc_t freq;
    uint8_t prior;
    bool isWhiteListPage;
} ActcData;

typedef struct {
    uint64_t addr;
    actc_t freq;
    int node;
    uint8_t prior;
} LevelActcData;

typedef struct {
    uint16_t freqMin;
    uint16_t freqMax;
    uint32_t freqZero;
    uint64_t freqNum;
    uint64_t pageNum;
    uint64_t freqSum;
} ActCount;

typedef struct {
    uint64_t maxMigrate;
    uint32_t freqWt;
    uint32_t slowThred;
} SeparateParam;

typedef struct {
    int index;
    unsigned short nrVcpu;
    unsigned long long *cpuTime[MAX_RES_LEN];
    struct timeval realTime[MAX_RES_LEN];
    bool isHeavyLoad;
} ResourceInfo;

typedef enum {
    HAM_SCAN,
    NORMAL_SCAN,
    STATISTIC_SCAN,
    SCAN_TYPE_MAX,
} ScanType;

enum ProcessState {
    PROC_IDLE, // 空闲
    PROC_MIGRATE, // 冷热迁移
    PROC_BACK, // 迁回
    PROC_MOVE, // 逃生
};

typedef struct {
    uint32_t localNrPages;
    uint32_t remoteNrPages;
} PagePair;

typedef struct {
    SceneInfo sceneInfo; // 场景：轻载/重载/稳态/非稳态，扫描周期等
    bool enableAdaptMem; // 是否使能自适应，仅对虚机开启
} AdaptMem;

typedef struct {
    uint32_t nrPage; // 进程使用的page数量
    uint32_t nrPages[MAX_NODES]; // 进程在近远端使用的page数量
} WalkPage;

typedef struct {
    uint32_t cpuMask[LOCAL_NUMA_NUM]; // CPU绑定情况
    uint32_t numaNodes; // numa bitmap: 0-unused, 1-used
} NumaAttribute;

typedef struct {
    double initRemoteMemRatio[LOCAL_NUMA_NUM][REMOTE_NUMA_NUM]; // 接口设置的内存比例
    uint64_t memSize[LOCAL_NUMA_NUM][REMOTE_NUMA_NUM]; // 仅密度场景：迁移内存大小,单位为KB
    uint32_t allocRemoteNrPages[LOCAL_NUMA_NUM][REMOTE_NUMA_NUM]; // 根据账本计算的，各本地numa对应的远端page的数量
    uint32_t nrPagesPerLocalNuma[LOCAL_NUMA_NUM]; // 根据账本计算的，各本地numa可支配的内存
    double l2RemoteMemRatio[LOCAL_NUMA_NUM][REMOTE_NUMA_NUM]; // 水线场景，分配远端内存后设置的内存比例
    double l3RemoteMemRatio[LOCAL_NUMA_NUM][REMOTE_NUMA_NUM]; // 水线场景：自适应调整后的远端内存占比
    uint32_t nrMigratePages[MAX_NODES][MAX_NODES]; // 水线场景：消减后的迁移量；密度场景：接口设置的比例
    uint32_t remoteNrPagesAfterMigrate[LOCAL_NUMA_NUM][REMOTE_NUMA_NUM]; // 迁移后记录账本
    MigrateDirection dir[MAX_NODES]; // 算法决策各numa的迁出的方向 demote/promote/swap
    SeparateParam separateParam;
} StrategyAttribute;

typedef struct {
    uint32_t scanTime;
    ScanType scanType; // 标识添加进程组件 HAM/普通冷热
    uint64_t actcLen[MAX_NODES];
    ActcData *actcData[MAX_NODES]; // actc数据
    ActCount actCount[MAX_NODES]; // 统计数据
} ScanAttribute;

typedef struct {
    int domainId; // 虚机pid使用
    MmapType mmapType; // 内存映射模式SHARED/PRIVATE
} VMPidAttribute;

struct ProcessAttribute {
    PidType type; // VM/PID
    pid_t pid;
    enum ProcessState state;
    uint32_t scanTime;
    uint32_t duration; // scanType为统计模式时记录统计时长
    ScanType scanType; // 标识添加进程组件
    time_t scanStart;
    SceneInfo sceneInfo; // 场景：轻载/重载/稳态/非稳态，扫描周期等
    MigrateMode migrateMode; // 内存迁移模式，按照比例或是大小
    int initLocalMemRatio; // 接口设置的内存比例
    int remoteNumaCnt; // 远端numa数量
    bool isLowMem; // 多numa虚机场景，表示目的端内存不够
    bool enableSwap; // 控制是否开启交换，默认开启
    struct { // 迁移相关参数
        int nid;
        uint64_t memSize; // 迁移内存大小,单位为KB
    } migrateParam[REMOTE_NUMA_NUM];
    SeparateParam separateParam;
    NumaAttribute numaAttr;
    WalkPage walkPage;
    AdaptMem adaptMem;
    StrategyAttribute strategyAttr;
    ScanAttribute scanAttr;
    VMPidAttribute vmPidAttr;
    struct ProcessAttribute *next;
};
typedef struct ProcessAttribute ProcessAttr;

typedef struct {
    uint16_t nrSegment;
    uint32_t nrPages;
    uint64_t startPa;
    uint64_t endPA;
} NodeMem;

struct MigList {
    bool successToUser;
    uint64_t nr;
    uint64_t failedMigNr;
    uint64_t failedIsolatedNr;
    pid_t pid;
    int from;
    int to;
    uint64_t *addr;
};

struct MigPra {
    int pageSize;
    int nrThread;
    bool isMulThread;
};

struct MigrateMsg {
    int cnt;
    struct MigPra mulMig;
    struct MigList *migList;
};

struct MigrateNumaIoctlMsg {
    int srcNid;
    int destNid;
    int count;
    uint64_t memids[MAX_NR_MIGRATE_NUMA_RANGE];
};

struct MigPidRemoteNumaIoctlMsg {
    int srcNid;
    int destNid;
    int pidCnt;
    pid_t *pidList;
    int *migResArray; // 迁移结果
};

// 反向扫描参数，所有process共享
typedef struct {
    uint32_t pageSize;
    uint64_t nrColdPage; // 冷页数量
    uint64_t nrHotPage; // 热页数量
    uint16_t scanPeriod; // 扫描周期
    uint16_t scanMode; // 扫描模式
} TrackingAttr;

typedef struct { // tracking设备与迁移设备的fd
    int nodes[MAX_NODES]; // 每个tracking-node设备的fd
    int migrate; // 迁移字符设备fd
    int access; // access设备
    int lock; // 文件锁，使SmapStart只被初始化一次
} DevFds;

struct RemoteNumaUsedInfo {
    uint64_t size;
    uint64_t used;
    bool ifUsedFreshed;
};

struct RemoteNumaInfo {
    EnvMutex lock;
    uint64_t privateSize[LOCAL_NUMA_NUM][REMOTE_NUMA_NUM];
    uint64_t sharedSize[REMOTE_NUMA_NUM];
    struct RemoteNumaUsedInfo usedInfo[REMOTE_NUMA_NUM];
    struct RemoteNumaUsedInfo privateUsedInfo[LOCAL_NUMA_NUM][REMOTE_NUMA_NUM];
};

struct ProcessManager {
    ProcessAttr *processes;
    uint16_t smapMigTime; // 扫描次数
    SceneInfo sceneInfo;
    uint16_t nr[TYPE_MAX];
    uint16_t nrThread; // 线程数量
    uint16_t nrLocalNuma; // local numa数量
    DevFds fds;
    TrackingAttr tracking; // 反向扫描参数
    void *threadCtx[MAX_THREADS]; // 管理的线程上下文
    struct RemoteNumaInfo remoteNumaInfo; // 借用远端内存数量
    EnvMutex lock;
    EnvMutex threadLock;
};

struct ProcessMemBitmap {
    pid_t pid;
    size_t nrPages[MAX_NODES];
    size_t len[MAX_NODES];
    unsigned long *data[MAX_NODES];
    unsigned long *whiteListBm[MAX_NODES];
    uint32_t vmSize;
    uint32_t *mapping;
    uint32_t mappingOffset;
};

typedef struct {
    pid_t pid;
    uint32_t scanTime;
    uint32_t duration;
    int scanType;
    int count;
    struct {
        int nid;
        int ratio;
        uint64_t memSize;
        MigrateMode migrateMode;
    } numaParam[REMOTE_NUMA_NUM];
} ProcessParam;

uint64_t CalcRemoteBorrowPages(uint64_t size);

void DebugProcessAttr(struct ProcessManager *manager);

int GetNrLocalNuma(void);

int ProcessManagerInit(uint32_t pageType);

int DestroyProcessManager(void);

int LoadMangerNrProcessNum(void);

int LoadMangerNrVmNum(void);

bool PidIsValid(pid_t pid);

int IsQemuTask(pid_t pid);

PidType GetPidType(struct ProcessManager *manager);

ProcessAttr *GetProcessAttr(pid_t pid);

int VMPreprocess(pid_t pid, ProcessAttr *attr);

int SetProcessLocalNuma(pid_t pid, uint32_t *nodeBitmap, bool hugeFlag);
int SetLocalNumaByCpu(pid_t pid, uint32_t *nodeBitmap);

int ProcessAddManage(ProcessParam *param, uint32_t *nodeBitmap);

void CheckAndRemoveInvalidProcess(void);

void RemoveManagedProcess(int nr, pid_t *pidArr);

pid_t *QueryManagedProcess(PidType type);

int MigrateMemoryBack(pid_t pid, int srcNid, int desNid, uint64_t paStart, uint64_t paEnd);

int BuildAllPidData(void);

int SetRemoteNumaInfo(int srcNid, int destNid, uint64_t size);

struct ProcessManager *GetProcessManager(void);

unsigned long GetPidNrPages(pid_t pid);

int GetNumaNodesForPid(pid_t pid, int *node);

void RemoveAllManagedProcess(void);

bool IsHugeMode(void);

bool IsHugeAligned(uint64_t addr);

int IsHugePageRange(const char *line);

bool CheckReadyMigrateBack(int destNid);

int IsPidArrRemoteNumaMatch(pid_t *pidArr, int len, int nid);

RunMode GetRunMode(void);
void SetRunMode(RunMode runMode);

void LinkedListAdd(ProcessAttr **head, ProcessAttr **add);
void LinkedListRemove(ProcessAttr **remove, ProcessAttr **head);

static inline bool IsNodeInvalid(int nid)
{
    return nid < 0 || nid >= MAX_NODES;
}

static inline bool IsDestNodeInvalid(int nid)
{
    if (nid == DEFAULT_DEST_NODE) {
        return false;
    }
    return IsNodeInvalid(nid);
}

static inline void SetNodeForbidden(int nid)
{
    EnvAtomicSet(&g_forbiddenNodes[nid], 1);
}

static inline void ClearNodeForbidden(int nid)
{
    EnvAtomicSet(&g_forbiddenNodes[nid], 0);
}

static inline bool IsNodeForbidden(int nid)
{
    return EnvAtomicRead(&g_forbiddenNodes[nid]);
}

static inline void SaveNodeForbidden(EnvAtomic *a, int len)
{
    for (int i = 0; i < len; i++) {
        EnvAtomicSet(&a[i], EnvAtomicRead(&g_forbiddenNodes[i]));
    }
}

static inline void RecoverNodeForbidden(EnvAtomic *a, int len)
{
    for (int i = 0; i < len; i++) {
        EnvAtomicSet(&g_forbiddenNodes[i], EnvAtomicRead(&a[i]));
    }
}

int EnableProcessMigrate(pid_t *pidArr, int len, int enable);
int IsRemoteNumaMigrateBackAllowed(int nid);
int IsRemoteNumaMoveAllowed(int nid);
int ChangePidRemoteByNuma(int srcNid, int destNid);
int IsPidArrayStateChangeReady(pid_t *pidArr, int len, int enable);
int IsPidArrInState(pid_t *pidArr, int len, enum ProcessState state);
bool IsAllL2NodePidInState(enum ProcessState state, int l2Node);
int ChangePidRemoteByPid(struct MigPidRemoteNumaIoctlMsg *msg);
ProcessAttr *GetProcessAttrLocked(pid_t pid);

bool MigOutIsDone(ProcessAttr *attr, bool *isMultiNumaPid);
FILE *OpenNumaMaps(pid_t pid);

static inline uint64_t KBTo2M(uint64_t memSize)
{
    return memSize >> KB_TO_2M;
}

static inline uint64_t KBTo4K(uint64_t memSize)
{
    return memSize >> KB_TO_4K;
}

static inline int GetCurrentMaxNrPid(void)
{
    return IsHugeMode() ? MAX_2M_PROCESSES_CNT : MAX_4K_PROCESSES_CNT;
}

/* L1 numaNodes helper functions */
static inline int GetAttrL1(ProcessAttr *attr)
{
    return GetL1(attr->numaAttr.numaNodes);
}

static inline void SetAttrL1(ProcessAttr *attr, int nid)
{
    SetL1(&attr->numaAttr.numaNodes, nid);
}

static inline bool EqualToAttrL1(ProcessAttr *attr, int nid)
{
    return EqualToL1(attr->numaAttr.numaNodes, nid);
}

static inline bool NotEqualToAttrL1(ProcessAttr *attr, int nid)
{
    return !EqualToAttrL1(attr, nid);
}

static inline bool InAttrL1(ProcessAttr *attr, int nid)
{
    return InL1(attr->numaAttr.numaNodes, nid);
}

static inline bool NotInAttrL1(ProcessAttr *attr, int nid)
{
    return !InAttrL1(attr, nid);
}

static inline uint64_t GetL1ActcLen(ProcessAttr *attr)
{
    int nid = GetAttrL1(attr);
    return (nid == NUMA_NO_NODE) ? 0 : attr->scanAttr.actcLen[nid];
}

static inline ActCount *GetL1ActCount(ProcessAttr *attr)
{
    int nid = GetAttrL1(attr);
    return (nid == NUMA_NO_NODE) ? NULL : &attr->scanAttr.actCount[nid];
}

static inline ActcData *GetL1ActcData(ProcessAttr *attr)
{
    int nid = GetAttrL1(attr);
    return (nid == NUMA_NO_NODE) ? NULL : attr->scanAttr.actcData[nid];
}

/* L2 numaNodes helper functions */
static inline int GetAttrL2(ProcessAttr *attr)
{
    int offset = LOCAL_NUMA_BITS - GetNrLocalNuma();
    return GetL2(attr->numaAttr.numaNodes) - offset;
}

static inline void SetAttrL2(ProcessAttr *attr, int nid)
{
    int offset = LOCAL_NUMA_BITS - GetNrLocalNuma();
    SetL2(&attr->numaAttr.numaNodes, nid + offset);
}

static inline void AddAttrL2(ProcessAttr *attr, int nid)
{
    int offset = LOCAL_NUMA_BITS - GetNrLocalNuma();
    AddL2(&attr->numaAttr.numaNodes, nid + offset);
}

static inline void SetL2ByNid(uint32_t *nodes, int nid)
{
    int offset = LOCAL_NUMA_BITS - GetNrLocalNuma();
    SetL2(nodes, nid + offset);
}

static inline void AddL2ByNid(uint32_t *nodes, int nid)
{
    int offset = LOCAL_NUMA_BITS - GetNrLocalNuma();
    AddL2(nodes, nid + offset);
}

static inline bool EqualToAttrL2(ProcessAttr *attr, int nid)
{
    int offset = LOCAL_NUMA_BITS - GetNrLocalNuma();
    return EqualToL2(attr->numaAttr.numaNodes, nid + offset);
}

static inline bool NotEqualToAttrL2(ProcessAttr *attr, int nid)
{
    return !EqualToAttrL2(attr, nid);
}

static inline bool InAttrL2(ProcessAttr *attr, int nid)
{
    int offset = LOCAL_NUMA_BITS - GetNrLocalNuma();
    return InL2(attr->numaAttr.numaNodes, nid + offset);
}

static inline bool NotInAttrL2(ProcessAttr *attr, int nid)
{
    return !InAttrL2(attr, nid);
}

static inline uint64_t GetL2ActcLen(ProcessAttr *attr)
{
    int nid = GetAttrL2(attr);
    return (nid == NUMA_NO_NODE) ? 0 : attr->scanAttr.actcLen[nid];
}

static inline ActCount *GetL2ActCount(ProcessAttr *attr)
{
    int nid = GetAttrL2(attr);
    return (nid == NUMA_NO_NODE) ? NULL : &attr->scanAttr.actCount[nid];
}

static inline ActcData *GetL2ActcData(ProcessAttr *attr)
{
    int nid = GetAttrL2(attr);
    return (nid == NUMA_NO_NODE) ? NULL : attr->scanAttr.actcData[nid];
}

static inline bool IsNumaMapLineHuge(char *line)
{
    char *substr = strstr(line, NUMAMAP_HUGE_SUBSTR);
    return substr != NULL;
}

static inline bool IsMultiNumaVm(ProcessAttr *process)
{
    return process->type == VM_TYPE && (process->remoteNumaCnt > 1 || GetL1Count(process->numaAttr.numaNodes) > 1);
}

bool IsMemoryLow(pid_t pid);
#endif /* __MANAGE_H__ */
