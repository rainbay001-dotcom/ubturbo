# smap 跨虚机频次拉通降冷设计文档

> 对应 Issue: #18  
> 作者: JinDou1210  
> 日期: 2026-05-23  
> 状态: 待评审

---

## 1. 背景与目标

### 1.1 现状问题

当前 smap 的降冷（Demote）策略在**单虚机内部**进行频次排序，选出该虚机内部最冷的 N% 页面迁往 L2：

```
SeparateStrategy()
  └─ DemotionStrategy(rawMigrateNum = nrMigratePages[l1][l2])
       └─ BaseStrategy(..., DEMOTE)
            ├─ qsort(actcData[l1], n, ASC)   // 按 freq 升序
            └─ BaseStrategyInner(...)          // 取前 rawMigrateNum 项
```

**问题场景**：VM-A 空闲（页面 freq 集中在 0–2），VM-B 繁忙（页面 freq 集中在 20–100）。各自按 per-VM 比例选 20%：

- VM-A 迁出 freq=0 的页面 ✓（合理）
- VM-B 迁出 freq=20 的页面 ✗（该频次远高于 VM-A 的"热"页，迁出后 L1 利用率下降）

### 1.2 目标

将所有 2M NORMAL_SCAN 虚机（非 MultiNumaVm）的 L1 页面频次放在一起排序，共同选出全局最冷的 N% 迁往 L2，使 L1 始终保留访问频次最高的页面。

### 1.3 范围

| 包含 | 排除 |
|------|------|
| 2M HugeMode NORMAL_SCAN 虚机 | HAM_SCAN 虚机 |
| SeparateStrategy 路径（单 NUMA 对） | SeparateStrategyMultiNumaVm |
| Demote 方向（L1 → L2） | Promote / Swap 方向 |
| master 分支（无 Swap 特性） | 不同 scanTime 的归一化（Q1 前提排除） |

---

## 2. 前提确认

| 问题 | 确认结论 | 影响 |
|------|---------|------|
| Q1: 不同虚机 scanTime 是否相同？ | 当前只考虑 scanTime 相同的情况 | 无需归一化，raw freq 可直接跨 VM 比较 |
| Q2: 基准 scanTime 来自哪里？ | `GetScanPeriodConfig()`（配置文件 `smap.scan.period`，默认 100ms） | 后续扩展归一化时使用此值作 T_REF |
| Q3: 接口约束？ | 不修改 libsmap.so 对外接口；smap 内部函数签名可按需调整 | 函数签名可新增参数 |
| Q4: 迁出比？ | 忽略 per-VM 传入值；全局迁出比从配置文件读取 | 新增配置项 `smap.global.demote.ratio` |

---

## 3. 方案审核（发现的漏洞）

在将上一版方案与实际代码对照后，发现以下问题，均已在本版设计中修正。

### 3.1 P0：Gate 条件屏蔽全局降冷（Critical）

**问题**：`SeparateStrategy()` 以 `nrMigratePages[l1][l2] > 0` 作为进入 Demote 路径的门槛。在全局模式下，若某 VM 的 per-VM 比例计算结果为 0（该 VM 本周期无需降冷），`DemotionStrategy` 永远不会被调用，全局阈值选出的该 VM 冷页无法被迁移。

**修正**：`SeparateStrategy` 接收 `GlobalDemoteCtx *ctx` 参数；当 ctx 激活时，绕过 `nrMigratePages` 门槛，直接进入 Demote 路径（Promote 路径优先级不变）。

### 3.2 P1：L2 空闲页重复计算（Significant）

**问题**：若 N 个 VM 共用同一 L2 NUMA 节点，`BuildGlobalDemoteCtx` 对该节点调用 N 次 `GetNrFreeHugePagesByNode(l2)`，`total_l2_free` 被放大 N 倍，预算上限约束失效。

**修正**：引入 `bool l2_counted[MAX_NODES]` 位图，每个 L2 节点只计一次。

### 3.3 P2：Phase 1 未过滤非 IDLE 状态 VM（Minor）

**问题**：`PreMigration` 只对 `state == PROC_IDLE` 的 VM 执行实际迁移，但 `BuildGlobalDemoteCtx` 未做同样过滤，导致非 IDLE VM 的页面进入全局直方图，使预算偏大、阈值偏低。

**修正**：在 `BuildGlobalDemoteCtx` 的 VM 遍历中增加 `vm->state != PROC_IDLE` 过滤条件。

### 3.4 P3：零频次比例分配整数下溢（Minor，基础版接受）

**问题**：零频次主导时，`vm_count = budget * vm_zero / totalZeroFreqPages`。若 `budget * vm_zero < totalZeroFreqPages`，整数除法得 0，该 VM 分不到配额。

**评估**：当 `thresholdFreq == 0` 时，意味着全局零频次页 ≥ budget。若某 VM 持有的零频次页极少，分到 0 配额是合理的（其冷页贡献微乎其微）。基础版本接受此行为，后续可用 rank-based 算法优化。

---

## 4. 算法复杂度分析

### 4.1 当前 2M Demote 路径（master 分支）

| 步骤 | 函数 | 复杂度 | 备注 |
|------|------|--------|------|
| L1 升序排序 | `qsort(actcData[l1], n₁)` | O(n₁ log n₁) | 有效 |
| L2 降序排序 | `qsort(actcData[l2], n₂)` | O(n₂ log n₂) | **无效**：`enableSwap=false`，L2 地址从不使用 |
| CalcMigrateNumByFreq | — | O(log n₁) | **无效**：必然返回 0 |
| BaseStrategyInner | — | O(k) | k = rawMigrateNum |
| M 个 VM 合计 | | **O(M·n log n)** | 含大量无效开销 |

### 4.2 新方案（全局直方图）复杂度

| 阶段 | 操作 | 复杂度 |
|------|------|--------|
| Phase 1：建全局直方图 | 线性扫描所有 VM 的 L1 actcData | O(N)，N=Σn₁ᵢ |
| Phase 2：FindThreshold | 遍历 256 个桶 | O(256) ≈ 常数 |
| Phase 3：按阈值收集（per VM） | qsort + 线性扫描取 vm_count 项 | O(n₁ᵢ log n₁ᵢ)，合计 O(N log(N/M)) |

> **注**：Phase 3 保留 qsort（Surgical Changes 原则，不移除现有排序逻辑）。  
> L2 的 qsort 和 `CalcMigrateNumByFreq` 调用仍在 `BaseStrategy` 中执行，但 Demote 路径下其结果被全局 ctx 覆盖（原来就是无效开销）。  
> 全局直方图方法不引入 O(N²) 风险，总体不劣于现有复杂度，Phase 1+2 额外开销为 O(N)。

---

## 5. 详细设计

### 5.1 新增数据结构

在 `strategy/separate_strategy.h` 中新增：

```c
#define GLOBAL_DEMOTE_RATIO_DEFAULT 5   /* 默认全局迁出比 5% */
#define GLOBAL_DEMOTE_RATIO_MIN     1
#define GLOBAL_DEMOTE_RATIO_MAX     50

/*
 * 全局降冷上下文，由 BuildGlobalDemoteCtx 构造，传入每个 VM 的降冷路径。
 * thresholdFreq == -1 表示全局阈值未激活，各 VM 降级为 per-VM 模式。
 * thresholdFreq == 0  表示零频次页数量 >= 预算，走比例分配路径。
 * thresholdFreq >  0  表示正常阈值路径。
 */
typedef struct {
    int      thresholdFreq;        /* SELECT_BOTTOM_K 结果；-1: 未激活 */
    uint32_t takeAtThreshold;      /* 阈值频次处的剩余可取数；仅 thresholdFreq > 0 时使用 */
    uint64_t budget;               /* 全局预算总量；thresholdFreq == 0 时用于比例分配 */
    uint64_t totalZeroFreqPages;   /* buckets[0] 的值；thresholdFreq == 0 时使用 */
} GlobalDemoteCtx;
```

`GlobalDemoteCtx` 通过**函数参数链**传递，不使用全局变量。

### 5.2 新增配置项

在 `strategy/period_config.h` 中新增：

```c
uint32_t GetGlobalDemoteRatioConfig(void);
```

在 `strategy/period_config.c` 中新增：

```c
/* PeriodConfig 结构新增字段 */
typedef struct {
    ...
    uint32_t globalDemoteRatio;   /* 全局迁出比，单位 % */
} PeriodConfig;

/* 配置文件键名：smap.global.demote.ratio */
/* 取值范围：[1, 50]，默认值：5 */
uint32_t GetGlobalDemoteRatioConfig(void)
{
    return g_periodConfig.globalDemoteRatio;
}
```

### 5.3 Phase 1+2：BuildGlobalDemoteCtx（新函数）

位置：`strategy/separate_strategy.c`，在 `strategy/separate_strategy.h` 中声明。

调用时机：`PreMigration()` 中 per-VM 循环开始前，持有 `manager->lock` 期间。

```c
/*
 * Phase 1: 遍历所有符合条件的 VM，将 L1 页面频次计入全局直方图。
 * Phase 2: 调用 FindThreshold 确定全局降冷阈值，填充 ctx。
 *
 * 筛选条件（VM 同时满足）：
 *   - scanType == NORMAL_SCAN
 *   - IsHugeMode() && !IsMultiNumaVm(vm)
 *   - state == PROC_IDLE（与 PreMigration 迁移条件对齐）
 *   - GetAttrL1(vm) >= 0 && GetAttrL2(vm) >= 0
 */
void BuildGlobalDemoteCtx(struct ProcessManager *manager, GlobalDemoteCtx *ctx)
{
    ctx->thresholdFreq      = -1;
    ctx->takeAtThreshold    = 0;
    ctx->budget             = 0;
    ctx->totalZeroFreqPages = 0;

    if (!IsHugeMode()) {
        return;
    }

    uint32_t buckets[STRATEGY_ACTC_MAX_FREQ] = {0};
    uint64_t total_pages   = 0;
    uint64_t total_l2_free = 0;
    bool     l2_counted[MAX_NODES] = {false};   /* 防止同一 L2 节点重复计算 */

    /* Phase 1: 建全局归一化频次直方图 */
    for (ProcessAttr *vm = manager->processes; vm != NULL; vm = vm->next) {
        if (vm->scanType != NORMAL_SCAN) {
            continue;
        }
        if (!IsHugeMode() || IsMultiNumaVm(vm)) {
            continue;
        }
        if (vm->state != PROC_IDLE) {          /* 与 PreMigration 保持一致 */
            continue;
        }
        int l1 = GetAttrL1(vm);
        int l2 = GetAttrL2(vm);
        if (l1 < 0 || l2 < 0) {
            continue;
        }

        /* Q1 前提：所有 VM scanTime 相同，raw freq 可直接比较，无需归一化 */
        for (uint64_t i = 0; i < vm->scanAttr.actcLen[l1]; i++) {
            if (vm->scanAttr.actcData[l1][i].isWhiteListPage) {
                continue;
            }
            int freq = MIN((int)vm->scanAttr.actcData[l1][i].freq, STRATEGY_ACTC_MAX_FREQ - 1);
            buckets[freq]++;
            total_pages++;
        }

        /* 每个 L2 节点只计一次空闲页（修正 P1 漏洞） */
        if (!l2_counted[l2]) {
            total_l2_free += GetNrFreeHugePagesByNode(l2);
            l2_counted[l2] = true;
        }
    }

    if (total_pages == 0) {
        return;
    }

    /* Phase 2: 计算全局预算，找阈值 */
    uint32_t ratio = GetGlobalDemoteRatioConfig();
    uint64_t budget = total_pages * ratio / 100;
    budget = MIN(budget, total_l2_free);
    if (budget == 0) {
        return;
    }

    ctx->budget             = budget;
    ctx->totalZeroFreqPages = (uint64_t)buckets[0];

    /* 复用现有 FindThreshold，SELECT_BOTTOM_K 模式 */
    FindThreshold(SELECT_BOTTOM_K, budget, buckets,
                  &ctx->thresholdFreq, &ctx->takeAtThreshold);
}
```

**复杂度**：Phase 1 = O(N)，Phase 2 = O(256) ≈ 常数。  
**内存**：`buckets[]` = 256 × 4 = 1 KB，栈上分配，函数返回后释放。

### 5.4 Phase 3：BaseStrategy 修改（Demote 分支）

位置：`strategy/separate_strategy.c`，函数 `BaseStrategy()`，新增 `GlobalDemoteCtx *ctx` 参数。

**修改前（原 DEMOTE 分支）**：
```c
if (dir == DEMOTE) {
    mlist[l1Node][l2Node].nr = MAX(SwapMigrateNum, rawMigrateNum);
    mlist[l2Node][l1Node].nr = mlist[l1Node][l2Node].nr - rawMigrateNum;
}
```

**修改后**：
```c
if (dir == DEMOTE) {
    if (ctx != NULL && ctx->thresholdFreq >= 0) {
        /* 全局阈值模式：忽略 rawMigrateNum，按全局阈值决定本 VM 迁出量 */
        uint64_t vm_count = 0;
        uint64_t n = (uint64_t)GetL1ActcLen(process);
        ActcData *data = process->scanAttr.actcData[l1Node];

        if (ctx->thresholdFreq == 0) {
            /* 零频次主导：按本 VM 零频次页占比分配配额 */
            uint64_t vm_zero = 0;
            for (uint64_t i = 0; i < n; i++) {
                if (!data[i].isWhiteListPage && data[i].freq == 0) {
                    vm_zero++;
                } else if (!data[i].isWhiteListPage && data[i].freq > 0) {
                    break;  /* qsort 升序后，非白名单页频次严格单调递增 */
                }
            }
            if (ctx->totalZeroFreqPages > 0) {
                vm_count = ctx->budget * vm_zero / ctx->totalZeroFreqPages;
            }
        } else {
            /* 正常阈值：freq < threshold 全取，freq == threshold 共享配额 */
            uint32_t take_rem = ctx->takeAtThreshold;
            for (uint64_t i = 0; i < n; i++) {
                if (data[i].isWhiteListPage) {
                    continue;
                }
                int freq = data[i].freq;
                if (freq < ctx->thresholdFreq) {
                    vm_count++;
                } else if (freq == ctx->thresholdFreq && take_rem > 0) {
                    vm_count++;
                    take_rem--;
                } else {
                    break;  /* qsort 升序，后续无更小 freq */
                }
            }
            ctx->takeAtThreshold = take_rem;  /* 串行执行，直接更新共享计数 */
        }

        mlist[l1Node][l2Node].nr = vm_count;
        mlist[l2Node][l1Node].nr = 0;
    } else {
        /* per-VM 降级模式（全局 ctx 未激活） */
        mlist[l1Node][l2Node].nr = MAX(SwapMigrateNum, rawMigrateNum);
        mlist[l2Node][l1Node].nr = mlist[l1Node][l2Node].nr - rawMigrateNum;
    }
}
```

**正确性说明**：
- `qsort(actcData[l1], ..., ActcFreqAscFunc)` 将**所有白名单页排至尾部**，非白名单页按 freq 升序排列。因此 `break` 优化在非白名单序列中有效。
- `BaseStrategyInner`（后续不变）取 `mlist[l1][l2].nr` 个地址，即 `actcData[l1][0..vm_count-1]` 的地址——恰好是 qsort 后最冷的 vm_count 个非白名单页。
- `ctx->takeAtThreshold` 在 per-VM 串行循环中更新，无并发问题。

### 5.5 SeparateStrategy 修改（修正 P0 漏洞）

位置：`strategy/separate_strategy.c`，函数 `SeparateStrategy()`，新增 `GlobalDemoteCtx *ctx` 参数。

**修改前**：
```c
int SeparateStrategy(ProcessAttr *process, struct MigList mlist[MAX_NODES][MAX_NODES])
{
    ...
    if (process->strategyAttr.nrMigratePages[l1Node][l2Node] > 0) {
        return DemotionStrategy(process, mlist, nrMigratePages[l1][l2]);
    } else if (process->strategyAttr.nrMigratePages[l2Node][l1Node] > 0) {
        return PromotionStrategy(process, mlist, nrMigratePages[l2][l1]);
    }
    return SwapStrategy(process, mlist);
}
```

**修改后**：
```c
int SeparateStrategy(ProcessAttr *process, struct MigList mlist[MAX_NODES][MAX_NODES],
                     GlobalDemoteCtx *ctx)
{
    ...
    int l1Node = GetAttrL1(process);
    int l2Node = GetAttrL2(process);
    ...

    /* Promote 优先，不受全局模式影响 */
    if (process->strategyAttr.nrMigratePages[l2Node][l1Node] > 0) {
        return PromotionStrategy(process, mlist, process->strategyAttr.nrMigratePages[l2Node][l1Node]);
    }

    /* 全局降冷模式：绕过 per-VM nrMigratePages 门槛（修正 P0） */
    if (ctx != NULL && ctx->thresholdFreq >= 0) {
        return DemotionStrategy(process, mlist, 0, ctx);
        /*
         * rawMigrateNum=0 在全局模式下被忽略（ctx != NULL 分支）。
         * vm_count 完全由全局阈值计算，可为 0（该 VM 本轮无需降冷）。
         */
    }

    /* per-VM 降冷模式 */
    if (process->strategyAttr.nrMigratePages[l1Node][l2Node] > 0) {
        return DemotionStrategy(process, mlist, process->strategyAttr.nrMigratePages[l1Node][l2Node], NULL);
    }

    return SwapStrategy(process, mlist);
}
```

### 5.6 函数参数链调整

由于 `SeparateStrategy`、`DemotionStrategy`、`BaseStrategy` 签名新增 `GlobalDemoteCtx *ctx` 参数，调用链同步更新：

```
PreMigration(manager, mMsg, migratePages)
  ├─ 构建 GlobalDemoteCtx ctx
  │  BuildGlobalDemoteCtx(manager, &ctx)        ← 新增调用（per-VM 循环前）
  │
  └─ per-VM 循环
       └─ BuildMigrationMsg(process, mMsg, migratePages)
            └─ RunStrategy(process, migList, MAX_NODES)
                 └─ SeparateStrategy(process, mlist, &ctx)  ← 新增参数
                      ├─ DemotionStrategy(process, mlist, rawMigrateNum, ctx)  ← 新增参数
                      │    └─ BaseStrategy(process, mlist, rawMigrateNum, DEMOTE, ctx)  ← 新增参数
                      ├─ PromotionStrategy(process, mlist, rawMigrateNum)  ← 不变
                      └─ SwapStrategy(process, mlist)                       ← 不变
```

`ctx` 通过参数链透传，`RunStrategy` 和 `BuildMigrationMsg` 不感知 ctx（透传即可），或直接在 `ScanMigrateWork → PerformMigration → PreMigration` 中传递。

> **实现简化选项**：`BuildGlobalDemoteCtx` 与 `CollectByGlobalThreshold` 在 `PreMigration` 中直接使用，可以将 ctx 存为 `PreMigration` 的本地变量，只需通过参数链透传给 `SeparateStrategy`，`RunStrategy` 和 `BuildMigrationMsg` 签名分别增加一个 `GlobalDemoteCtx *ctx` 透传参数即可。

### 5.7 PreMigration 修改

```c
static int PreMigration(struct ProcessManager *manager, struct MigrateMsg *mMsg, uint64_t *migratePages)
{
    ...
    EnvMutexLock(&manager->lock);
    ret = InitMigrateMsg(mMsg, manager);
    ...

    /* 新增：全局降冷上下文（per-VM 循环前计算一次） */
    GlobalDemoteCtx ctx;
    BuildGlobalDemoteCtx(manager, &ctx);

    for (current = manager->processes; current; current = current->next) {
        if (current->scanType != NORMAL_SCAN) {
            continue;
        }
        NumaMigReduceDeal(current);
        if (current->state != PROC_IDLE) {
            continue;
        }
        current->state = PROC_MIGRATE;
        ret = BuildMigrationMsg(current, mMsg, migratePages, &ctx);   /* 透传 ctx */
        ...
    }
    ...
}
```

---

## 6. 修改位置汇总

| 文件 | 函数 / 区域 | 改动内容 | 行数估计 |
|------|------------|---------|---------|
| `strategy/period_config.h` | 新增 `GetGlobalDemoteRatioConfig()` 声明 | +1 行 |
| `strategy/period_config.c` | `PeriodConfig` 结构新增 `globalDemoteRatio` 字段；读取/校验/accessor | +25 行 |
| `strategy/separate_strategy.h` | 新增 `GlobalDemoteCtx` 结构体定义；新增 `BuildGlobalDemoteCtx()` 声明 | +20 行 |
| `strategy/separate_strategy.c` | 新增 `BuildGlobalDemoteCtx()` | +50 行 |
| `strategy/separate_strategy.c` | `SeparateStrategy()` 签名 + 逻辑（修正 P0） | +10 行 |
| `strategy/separate_strategy.c` | `DemotionStrategy()` 签名（透传 ctx） | +2 行 |
| `strategy/separate_strategy.c` | `BaseStrategy()` 签名 + DEMOTE 分支（全局阈值路径） | +35 行 |
| `strategy/strategy.c` | `RunStrategy()` 签名（透传 ctx 至 SeparateStrategy） | +3 行 |
| `strategy/strategy.h` | `RunStrategy` 声明更新 | +1 行 |
| `strategy/migration.c` | `PreMigration()`：新增 `BuildGlobalDemoteCtx` 调用 | +3 行 |
| `strategy/migration.c` | `BuildMigrationMsg()`：新增 `ctx` 参数透传至 RunStrategy | +2 行 |
| `strategy/migration.h` | `BuildMigrationMsg` 声明更新（如对外暴露） | +1 行 |

**不涉及**：内核迁移 ioctl、`MigList` 结构、Promote 路径、Swap 路径、4K 路径、MultiNumaVm 路径、libsmap.so 对外接口。

---

## 7. 额外开销汇总

| 开销项 | 大小 / 说明 |
|--------|------------|
| `GlobalDemoteCtx` | 28 字节，栈上分配（PreMigration 本地变量） |
| `buckets[]` in BuildGlobalDemoteCtx | 256 × 4 = **1 KB**，栈上，每迁移周期一次，函数返回即释放 |
| `l2_counted[]` in BuildGlobalDemoteCtx | MAX_NODES × 1 = ≤16 字节，栈上 |
| Phase 1 遍历时间 | O(N)，N = 全局 L1 页总数，单次内存读写 |
| Phase 2 FindThreshold | O(256) ≈ 常数 |
| Phase 3 per-VM 扫描 | O(n₁ᵢ)，合计 O(N)（已包含在现有 qsort 遍历中） |
| GetNrFreeHugePagesByNode 调用次数 | 最多 MAX_NODES 次（修正 P1 后去重），原来为 VM 个数次 |
| **qsort 是否消除** | 否，保留原有排序（Surgical Changes 原则） |
| **净收益** | Phase 1+2 新增 O(N) 遍历；Demote Phase 3 从取前 N 项改为按阈值线性扫描，复杂度量级不变 |

---

## 8. 边界情况处理

| 场景 | 处理方式 |
|------|---------|
| 无符合条件的 VM | `total_pages == 0`，`ctx->thresholdFreq = -1`，所有 VM 降级为 per-VM 模式 |
| L2 无空闲大页 | `total_l2_free = 0` → `budget = 0`，ctx 未激活，降级 per-VM |
| `GetGlobalDemoteRatioConfig()` 超出范围 | 读取时钳位到 `[1, 50]` |
| VM 无 L1 页面（actcLen=0） | Phase 1 循环体不执行；Phase 3 vm_count=0，无迁移 |
| 全部页面为白名单页 | Phase 1 不计入 buckets；Phase 3 vm_count=0，无迁移 |
| `thresholdFreq == 0`，某 VM 无零频次页 | `vm_zero=0`，`vm_count=0`，该 VM 本轮不迁出 |
| Promote VM 不参与全局降冷 | `SeparateStrategy` 中 Promote 优先分支先返回，不进入全局降冷路径 |

---

## 9. 本期范围外

| 项目 | 原因 |
|------|------|
| `SeparateStrategyMultiNumaVm` | 用户明确排除 |
| `SeparateStrategy4K`（4K 路径） | 用户明确排除（仅针对 2M 虚机） |
| HAM_SCAN 路径 | 用户明确排除 |
| Promote / Swap 跨 VM 拉通 | 本期范围仅 Demote |
| 不同 scanTime 的归一化 | Q1 前提排除，预留接口：`CROSS_VM_FREQ_REF_MS` 后续扩展 |
| per-VM 迁移下限（兜底） | 用户明确排除 |
| 零频次分配精度优化（rank-based） | 基础版本接受 P3 下溢行为，后续子任务 |
