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
#define RESERVED_RATIO 0.05
#define RESERVED_MEMORY 200
#define MAX_4K_PROCESSES_CNT 300
#define MAX_2M_PROCESSES_CNT 100
#define MAX_THREADS 10
#define MAX_RES_LEN 4
#define PAGE_SHIFT 12
#define PAGE_SIZE (1UL << PAGE_SHIFT)
#define DEFAULT_FD (-1)

#define RSS_LINE_PREFIX "Rss:"
#define RSS_LINE_PREFIX_LENGTH 4
#define HUGETLB_LINE_PREFIX "Private_Hugetlb:"
#define HUGETLB_LINE_PREFIX_LENGTH 16

#define PRESENT (1ULL << 63)
#define PRN_SHIFT ((1ULL << 55) - 1)
#define MAPS_LIN_LEN 2
#define MAPS_MAX_LEN 256

#define BUFFER_SIZE 256
#define PAGEMAP_ENTRY_SIZE 8

#define DEFAULT_L1_NODE (-1)
#define DEFAULT_L2_NODE (-1)
#define DEFAULT_DEST_NODE (-1)

#define BIT_TO_BYTE 8
#define CPU_NUMA_PATH "/sys/devices/system/cpu/cpu%d/node%d"
#define NUMAMAP_HUGE_2M_SUBSTR "kernelpagesize_kB=2048"

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

/* 单页冷热访问频次数据，由 ACTC（Access Tracking Counter）扫描产生 */
typedef struct {
    uint64_t addr;          /* 页面虚拟地址 */
    actc_t freq;            /* 访问频次计数 */
    uint8_t prior;          /* 迁移优先级（值越大优先级越高） */
    bool isWhiteListPage;   /* 是否为白名单页（白名单页不参与迁出） */
} ActcData;

/* 带 NUMA 节点信息的冷热数据，用于跨级别迁移决策 */
typedef struct {
    uint64_t addr;  /* 页面虚拟地址 */
    actc_t freq;    /* 访问频次计数 */
    int node;       /* 页面当前所在 NUMA 节点 ID */
    uint8_t prior;  /* 迁移优先级 */
} LevelActcData;

/* 单个 NUMA 节点的访问频次统计汇总 */
typedef struct {
    uint16_t freqMin;   /* 最小访问频次 */
    uint16_t freqMax;   /* 最大访问频次 */
    uint32_t freqZero;  /* 访问频次为 0 的页面数（冷页数） */
    uint64_t freqNum;   /* 有效频次样本总数 */
    uint64_t pageNum;   /* 统计的页面总数 */
    uint64_t freqSum;   /* 所有页面访问频次之和，用于计算平均值 */
} ActCount;

/* 冷热分离策略参数 */
typedef struct {
    uint64_t maxMigrate;  /* 单次迁移上限（页数） */
    uint32_t freqWt;      /* 频次权重，影响冷热分界阈值计算 */
    uint32_t slowThred;   /* 慢速迁移阈值，低于此值时降速迁移 */
} SeparateParam;

/* 虚机 CPU 资源使用信息，用于判断虚机负载 */
typedef struct {
    int index;                              /* 资源信息采样序号（滚动 MAX_RES_LEN 个槽） */
    unsigned short nrVcpu;                  /* 虚机 vCPU 数量 */
    unsigned long long *cpuTime[MAX_RES_LEN]; /* 各采样时刻的 vCPU 运行时间数组 */
    struct timeval realTime[MAX_RES_LEN];   /* 各采样时刻的挂钟时间 */
    bool isHeavyLoad;                       /* 当前是否处于重载状态 */
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

/* 本地/远端页面数量对，用于描述一对 NUMA 节点间的页面分布 */
typedef struct {
    uint32_t localNrPages;  /* 本地 NUMA 节点上的页面数 */
    uint32_t remoteNrPages; /* 远端 NUMA 节点上的页面数 */
} PagePair;

typedef struct {
    SceneInfo sceneInfo; // 场景：轻载/重载/稳态/非稳态，扫描周期等
    bool enableAdaptMem; // 是否使能自适应，仅对虚机开启
} AdaptMem;

/* 进程页面统计，由遍历 pagemap 获得 */
typedef struct {
    uint32_t nrPage;              /* 进程使用的页面总数 */
    uint32_t nrPages[MAX_NODES];  /* 进程在各 NUMA 节点（本地+远端）上的页面数 */
} WalkPage;

/* 进程的 NUMA 拓扑属性 */
typedef struct {
    uint32_t cpuMask[LOCAL_NUMA_NUM]; /* 每个本地 NUMA 对应的 CPU 绑定掩码 */
    uint32_t numaNodes;               /* NUMA 节点 bitmap：低位为本地，高位为远端（0=未使用，1=使用中） */
} NumaAttribute;

/* 迁移策略属性，记录各 NUMA 节点间的内存分配与迁移计划 */
typedef struct {
    double initRemoteMemRatio[LOCAL_NUMA_NUM][REMOTE_NUMA_NUM];      /* 接口设置的初始远端内存比例（本地×远端矩阵） */
    uint64_t memSize[LOCAL_NUMA_NUM][REMOTE_NUMA_NUM];               /* 仅密度场景：迁移内存大小，单位 KB */
    uint32_t allocRemoteNrPages[LOCAL_NUMA_NUM][REMOTE_NUMA_NUM];    /* 账本计算所得：各本地 NUMA 分配的远端页面数 */
    uint32_t nrPagesPerLocalNuma[LOCAL_NUMA_NUM];                    /* 账本计算所得：各本地 NUMA 可支配的总页面数 */
    double l2RemoteMemRatio[LOCAL_NUMA_NUM][REMOTE_NUMA_NUM];        /* 水线场景：分配远端内存后实际生效的比例 */
    double l3RemoteMemRatio[LOCAL_NUMA_NUM][REMOTE_NUMA_NUM];        /* 水线场景：自适应调整后的远端内存占比 */
    uint32_t nrMigratePages[MAX_NODES][MAX_NODES];                   /* 水线场景：经消减后的实际迁移页数；密度场景：接口设置比例 */
    uint32_t remoteNrPagesAfterMigrate[LOCAL_NUMA_NUM][REMOTE_NUMA_NUM]; /* 迁移完成后更新的账本（远端页面数） */
    MigrateDirection dir[MAX_NODES];                                 /* 算法决策各 NUMA 节点的迁移方向（demote/promote/swap） */
    SeparateParam separateParam;                                     /* 冷热分离策略参数 */
} StrategyAttribute;

/* ACTC 扫描属性，保存每次扫描产生的冷热页数据 */
typedef struct {
    uint32_t scanTime;             /* 扫描间隔（毫秒） */
    ScanType scanType;             /* 扫描类型（HAM/普通冷热/统计） */
    uint64_t actcLen[MAX_NODES];   /* 各 NUMA 节点的 actcData 数组有效长度 */
    ActcData *actcData[MAX_NODES]; /* 各 NUMA 节点的冷热页数据数组 */
    ActCount actCount[MAX_NODES];  /* 各 NUMA 节点的访问频次统计汇总 */
} ScanAttribute;

/* 虚机进程专属属性 */
typedef struct {
    int domainId;       /* libvirt domain ID，虚机进程使用 */
    MmapType mmapType;  /* 内存映射模式：SHARED（共享内存）或 PRIVATE（私有内存） */
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

/* 单个 NUMA 节点的物理内存段描述 */
typedef struct {
    uint16_t nrSegment; /* 内存段数量 */
    uint32_t nrPages;   /* 该节点的总页面数 */
    uint64_t startPa;   /* 起始物理地址 */
    uint64_t endPA;     /* 结束物理地址 */
} NodeMem;

/* 单次迁移任务的地址列表及结果 */
struct MigList {
    bool successToUser;        /* 迁移结果是否已回传给调用方 */
    uint64_t nr;               /* 本次迁移的页面地址数量 */
    uint64_t failedMigNr;      /* 迁移失败的页面数（内核 move_pages 返回错误） */
    uint64_t failedIsolatedNr; /* 隔离失败的页面数（无法从 LRU 隔离） */
    pid_t pid;                 /* 目标进程 PID */
    int from;                  /* 迁出的源 NUMA 节点 ID */
    int to;                    /* 迁入的目标 NUMA 节点 ID */
    uint64_t *addr;            /* 待迁移页面的虚拟地址数组 */
};

/* 迁移执行参数 */
struct MigPra {
    int pageSize;     /* 页面大小（字节），如 4096 或 2MB */
    int nrThread;     /* 并发迁移线程数 */
    bool isMulThread; /* 是否启用多线程迁移 */
};

/* 下发给迁移模块的完整迁移消息 */
struct MigrateMsg {
    int cnt;              /* migList 中有效的迁移任务数量 */
    struct MigPra mulMig; /* 迁移执行参数 */
    struct MigList *migList; /* 迁移任务列表数组 */
};

/* 用于 ioctl 的远端 NUMA 内存段迁移消息 */
struct MigrateNumaIoctlMsg {
    int srcNid;                              /* 源远端 NUMA 节点 ID */
    int destNid;                             /* 目标远端 NUMA 节点 ID */
    int count;                               /* memids 中有效条目数量 */
    uint64_t memids[MAX_NR_MIGRATE_NUMA_RANGE]; /* 待迁移的内存段 ID 数组 */
};

/* 单个进程的远端 NUMA 迁移参数 */
struct MigPayload {
    pid_t pid;          /* 目标进程 PID */
    int srcNid;         /* 源远端 NUMA 节点 ID */
    int destNid;        /* 目标远端 NUMA 节点 ID */
    int ratio;          /* 迁移比例（百分比） */
    int keepRatio;      /* 迁移完成后保留在远端的比例 */
    uint64_t memSize;   /* 迁移内存大小（KB），密度模式使用 */
    bool isRatioMode;   /* true: 按比例迁移；false: 按内存大小迁移 */
    uint64_t successCnt; /* 本次成功迁移的页面数 */
};

/* 批量进程远端 NUMA 迁移的 ioctl 消息 */
struct MigPidRemoteNumaIoctlMsg {
    int pidCnt;               /* payloads 数组中的进程数量 */
    struct MigPayload *payloads; /* 各进程的迁移参数数组 */
    int *migResArray;         /* 迁移结果数组（与 payloads 一一对应，0 成功，非 0 失败） */
};

/* 反向扫描（ACTC tracking）参数，由所有被管理进程共享 */
typedef struct {
    uint32_t pageSize;      /* 扫描的页面大小（字节） */
    uint64_t nrColdPage;    /* 统计到的冷页数量 */
    uint64_t nrHotPage;     /* 统计到的热页数量 */
    uint16_t scanPeriod;    /* 扫描周期（毫秒） */
    uint16_t scanMode;      /* 扫描模式（对应 ScanType 枚举） */
} TrackingAttr;

/* 字符设备文件描述符集合，集中管理 SMAP 依赖的内核接口 */
typedef struct {
    int nodes[MAX_NODES]; /* 各 NUMA 节点的 tracking 字符设备 fd */
    int migrate;          /* 迁移字符设备 fd */
    int access;           /* access 统计设备 fd */
    int lock;             /* 文件锁 fd，保证 SmapStart 只初始化一次 */
} DevFds;

/* 单个远端 NUMA 节点的内存使用统计 */
struct RemoteNumaUsedInfo {
    uint64_t size;         /* 该远端 NUMA 节点的总内存大小（字节） */
    uint64_t used;         /* 当前已使用的内存大小（字节） */
    bool ifUsedFreshed;    /* used 字段是否已在本轮刷新（防止读到过期数据） */
};

/* 远端 NUMA 内存借用账本，记录各本地 NUMA 借出的远端内存量 */
struct RemoteNumaInfo {
    EnvMutex lock;                                                     /* 账本访问互斥锁 */
    uint64_t privateSize[LOCAL_NUMA_NUM][REMOTE_NUMA_NUM];             /* 各本地 NUMA 借用各远端 NUMA 的私有内存大小（字节） */
    uint64_t sharedSize[REMOTE_NUMA_NUM];                              /* 各远端 NUMA 上共享内存的总大小（字节） */
    struct RemoteNumaUsedInfo usedInfo[REMOTE_NUMA_NUM];               /* 各远端 NUMA 的整体使用情况 */
    struct RemoteNumaUsedInfo privateUsedInfo[LOCAL_NUMA_NUM][REMOTE_NUMA_NUM]; /* 各本地 NUMA 在各远端 NUMA 上的私有使用情况 */
};

/* 全局进程管理器，持有所有被 SMAP 管理的进程及运行时状态 */
struct ProcessManager {
    ProcessAttr *processes;              /* 被管理进程链表头 */
    uint16_t smapMigTime;                /* 累计扫描/迁移次数 */
    SceneInfo sceneInfo;                 /* 全局场景信息（轻/重载、稳态等） */
    uint16_t nr[TYPE_MAX];               /* 各类型（PROCESS/VM）进程数量 */
    uint16_t nrThread;                   /* 当前工作线程数量 */
    uint16_t nrLocalNuma;                /* 本地 NUMA 节点数量 */
    DevFds fds;                          /* 内核字符设备文件描述符集合 */
    TrackingAttr tracking;               /* 反向扫描全局参数 */
    void *threadCtx[MAX_THREADS];        /* 各工作线程上下文指针数组 */
    struct RemoteNumaInfo remoteNumaInfo; /* 远端内存借用账本 */
    EnvMutex lock;                       /* 进程链表访问锁 */
    EnvMutex threadLock;                 /* 线程上下文访问锁 */
};

/* 单个进程在各 NUMA 节点上的页面 bitmap，用于精细化迁移决策 */
struct ProcessMemBitmap {
    pid_t pid;                          /* 进程 PID */
    size_t nrPages[MAX_NODES];          /* 各 NUMA 节点上的有效页面数 */
    size_t len[MAX_NODES];              /* 各 NUMA 节点 bitmap 的 unsigned long 数组长度 */
    unsigned long *data[MAX_NODES];     /* 各 NUMA 节点的页面存在性 bitmap */
    unsigned long *whiteListBm[MAX_NODES]; /* 各 NUMA 节点的白名单页 bitmap（不迁移这些页） */
    uint32_t vmSize;                    /* 虚机内存映射段总大小（页数） */
    uint32_t *mapping;                  /* 虚机物理页到宿主机 pfn 的映射表 */
    uint32_t mappingOffset;             /* mapping 数组的起始偏移（页单位） */
};

/* 添加进程到 SMAP 管理时传入的参数 */
typedef struct {
    pid_t pid;          /* 进程 PID */
    uint32_t scanTime;  /* 扫描间隔（毫秒） */
    uint32_t duration;  /* 统计模式下的持续时长（毫秒） */
    int scanType;       /* 扫描类型（对应 ScanType 枚举） */
    int count;          /* numaParam 中有效的远端 NUMA 配置数量 */
    struct {
        int nid;                /* 远端 NUMA 节点 ID */
        int ratio;              /* 迁移比例（百分比） */
        uint64_t memSize;       /* 迁移内存大小（KB），密度模式使用 */
        MigrateMode migrateMode; /* 迁移模式（按比例或按大小） */
    } numaParam[REMOTE_NUMA_NUM]; /* 各目标远端 NUMA 节点的迁移参数 */
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

uint32_t GetNormalPageSize(void);

uint32_t GetHugePageSize(void);

uint32_t GetPageSize(void);

ProcessAttr *GetProcessAttr(pid_t pid);

int VMPreprocess(pid_t pid, ProcessAttr *attr);

int SetProcessLocalNuma(pid_t pid, uint32_t *nodeBitmap, bool hugeFlag);
int SetLocalNumaByCpu(pid_t pid, uint32_t *nodeBitmap);

int ProcessAddManage(ProcessParam *param, uint32_t *nodeBitmap);

void CheckAndRemoveInvalidProcess(void);

void RemoveManagedProcess(int nr, pid_t *pidArr);

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

RunMode GetRunMode(void);
void SetRunMode(RunMode runMode);

void LinkedListAdd(ProcessAttr **head, ProcessAttr **add);
void LinkedListRemove(ProcessAttr **remove, ProcessAttr **head);

/* 判断 NUMA 节点 ID 是否越界（无效） */
static inline bool IsNodeInvalid(int nid)
{
    return nid < 0 || nid >= MAX_NODES;
}

/* 判断目标 NUMA 节点 ID 是否无效（-1 表示不指定目标，视为合法） */
static inline bool IsDestNodeInvalid(int nid)
{
    if (nid == DEFAULT_DEST_NODE) {
        return false;
    }
    return IsNodeInvalid(nid);
}

/* 将 NUMA 节点 nid 标记为禁止迁入（全局禁止表） */
static inline void SetNodeForbidden(int nid)
{
    EnvAtomicSet(&g_forbiddenNodes[nid], 1);
}

/* 解除 NUMA 节点 nid 的禁止迁入标记 */
static inline void ClearNodeForbidden(int nid)
{
    EnvAtomicSet(&g_forbiddenNodes[nid], 0);
}

/* 查询 NUMA 节点 nid 是否处于禁止迁入状态 */
static inline bool IsNodeForbidden(int nid)
{
    return EnvAtomicRead(&g_forbiddenNodes[nid]);
}

/* 将当前全局禁止表保存到数组 a 中（用于临时切换后恢复） */
static inline void SaveNodeForbidden(EnvAtomic *a, int len)
{
    for (int i = 0; i < len; i++) {
        EnvAtomicSet(&a[i], EnvAtomicRead(&g_forbiddenNodes[i]));
    }
}

/* 从数组 a 恢复全局禁止表 */
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

/* 将内存大小（KB）转换为大页（2MB）数量 */
static inline uint64_t KBToHugePage(uint64_t memSize)
{
    int size = GetHugePageSize();
    return memSize / (size / KIB);
}

/* 将内存大小（KB）转换为普通页（4KB）数量 */
static inline uint64_t KBToNormalPage(uint64_t memSize)
{
    int size = GetNormalPageSize();
    return memSize / (size / KIB);
}

/* 根据当前页面模式返回最大可管理进程数 */
static inline int GetCurrentMaxNrPid(void)
{
    return IsHugeMode() ? MAX_2M_PROCESSES_CNT : MAX_4K_PROCESSES_CNT;
}

/* L1（本地 NUMA）操作辅助函数，操作 ProcessAttr 的 numaAttr.numaNodes 字段 */
/* 获取进程属性中的本地 NUMA（L1）节点 ID */
static inline int GetAttrL1(ProcessAttr *attr)
{
    return GetL1(attr->numaAttr.numaNodes);
}

/* 将进程属性的本地 NUMA 设为唯一节点 nid */
static inline void SetAttrL1(ProcessAttr *attr, int nid)
{
    SetL1(&attr->numaAttr.numaNodes, nid);
}

/* 判断进程属性的本地 NUMA 是否等于 nid */
static inline bool EqualToAttrL1(ProcessAttr *attr, int nid)
{
    return EqualToL1(attr->numaAttr.numaNodes, nid);
}

/* 判断进程属性的本地 NUMA 是否不等于 nid */
static inline bool NotEqualToAttrL1(ProcessAttr *attr, int nid)
{
    return !EqualToAttrL1(attr, nid);
}

/* 判断本地 NUMA nid 是否在进程属性的节点集合中 */
static inline bool InAttrL1(ProcessAttr *attr, int nid)
{
    return InL1(attr->numaAttr.numaNodes, nid);
}

/* 判断本地 NUMA nid 是否不在进程属性的节点集合中 */
static inline bool NotInAttrL1(ProcessAttr *attr, int nid)
{
    return !InAttrL1(attr, nid);
}

/* 获取进程本地 NUMA（L1）节点上的 ACTC 数据有效长度 */
static inline uint64_t GetL1ActcLen(ProcessAttr *attr)
{
    int nid = GetAttrL1(attr);
    return (nid == NUMA_NO_NODE) ? 0 : attr->scanAttr.actcLen[nid];
}

/* 获取进程本地 NUMA（L1）节点上的频次统计结构指针 */
static inline ActCount *GetL1ActCount(ProcessAttr *attr)
{
    int nid = GetAttrL1(attr);
    return (nid == NUMA_NO_NODE) ? NULL : &attr->scanAttr.actCount[nid];
}

/* 获取进程本地 NUMA（L1）节点上的冷热数据数组指针 */
static inline ActcData *GetL1ActcData(ProcessAttr *attr)
{
    int nid = GetAttrL1(attr);
    return (nid == NUMA_NO_NODE) ? NULL : attr->scanAttr.actcData[nid];
}

/*
 * L2（远端 NUMA）操作辅助函数
 * 注意：numaNodes bitmap 中远端 NUMA 的位位置 = nid + offset，
 * offset = LOCAL_NUMA_BITS - nrLocalNuma，用于将稀疏的物理 NUMA ID 映射到紧凑的 bitmap 位置。
 */
/* 获取进程属性中的远端 NUMA（L2）节点 ID */
static inline int GetAttrL2(ProcessAttr *attr)
{
    int offset = LOCAL_NUMA_BITS - GetNrLocalNuma();
    return GetL2(attr->numaAttr.numaNodes) - offset;
}

/* 将进程属性的远端 NUMA 设为唯一节点 nid */
static inline void SetAttrL2(ProcessAttr *attr, int nid)
{
    int offset = LOCAL_NUMA_BITS - GetNrLocalNuma();
    SetL2(&attr->numaAttr.numaNodes, nid + offset);
}

/* 在进程属性中追加远端 NUMA nid */
static inline void AddAttrL2(ProcessAttr *attr, int nid)
{
    int offset = LOCAL_NUMA_BITS - GetNrLocalNuma();
    AddL2(&attr->numaAttr.numaNodes, nid + offset);
}

/* 将 nodes bitmap 的远端 NUMA 设为唯一节点 nid */
static inline void SetL2ByNid(uint32_t *nodes, int nid)
{
    int offset = LOCAL_NUMA_BITS - GetNrLocalNuma();
    SetL2(nodes, nid + offset);
}

/* 在 nodes bitmap 中追加远端 NUMA nid */
static inline void AddL2ByNid(uint32_t *nodes, int nid)
{
    int offset = LOCAL_NUMA_BITS - GetNrLocalNuma();
    AddL2(nodes, nid + offset);
}

/* 判断进程属性的远端 NUMA 是否等于 nid */
static inline bool EqualToAttrL2(ProcessAttr *attr, int nid)
{
    int offset = LOCAL_NUMA_BITS - GetNrLocalNuma();
    return EqualToL2(attr->numaAttr.numaNodes, nid + offset);
}

/* 判断进程属性的远端 NUMA 是否不等于 nid */
static inline bool NotEqualToAttrL2(ProcessAttr *attr, int nid)
{
    return !EqualToAttrL2(attr, nid);
}

/* 判断远端 NUMA nid 是否在进程属性的节点集合中 */
static inline bool InAttrL2(ProcessAttr *attr, int nid)
{
    int offset = LOCAL_NUMA_BITS - GetNrLocalNuma();
    return InL2(attr->numaAttr.numaNodes, nid + offset);
}

/* 判断远端 NUMA nid 是否不在进程属性的节点集合中 */
static inline bool NotInAttrL2(ProcessAttr *attr, int nid)
{
    return !InAttrL2(attr, nid);
}

/* 获取进程远端 NUMA（L2）节点上的 ACTC 数据有效长度 */
static inline uint64_t GetL2ActcLen(ProcessAttr *attr)
{
    int nid = GetAttrL2(attr);
    return (nid == NUMA_NO_NODE) ? 0 : attr->scanAttr.actcLen[nid];
}

/* 获取进程远端 NUMA（L2）节点上的频次统计结构指针 */
static inline ActCount *GetL2ActCount(ProcessAttr *attr)
{
    int nid = GetAttrL2(attr);
    return (nid == NUMA_NO_NODE) ? NULL : &attr->scanAttr.actCount[nid];
}

/* 获取进程远端 NUMA（L2）节点上的冷热数据数组指针 */
static inline ActcData *GetL2ActcData(ProcessAttr *attr)
{
    int nid = GetAttrL2(attr);
    return (nid == NUMA_NO_NODE) ? NULL : attr->scanAttr.actcData[nid];
}

/* 判断 numamap 文本行是否描述的是 2MB 大页内存段 */
static inline bool IsNumaMapLineHuge(char *line)
{
    char *substr = strstr(line, NUMAMAP_HUGE_2M_SUBSTR);
    return substr != NULL;
}

/* 判断进程是否为多远端 NUMA 虚机（有多个远端节点或多个本地节点） */
static inline bool IsMultiNumaVm(ProcessAttr *process)
{
    return process->type == VM_TYPE && (process->remoteNumaCnt > 1 || GetL1Count(process->numaAttr.numaNodes) > 1);
}

bool IsMemoryLow(pid_t pid);
#endif /* __MANAGE_H__ */
