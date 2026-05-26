# smap 扫描与迁移策略梳理

> 文档基于 `plugins/smap/src/user/` 用户态代码整理，代码版本 master 分支。
> 涵盖范围：扫描周期管理、场景识别、迁移周期管理、迁移策略、自适应内存配比、文件配置覆盖。

---

## 1. 整体工作流

主工作线程调用 `ScanMigrateWork()`（`strategy/migration.c`），每次迁移周期 `ctx->period` ms 触发一次，执行如下步骤：

```
ScanMigrateWork()
  ├─ DisableTracking()          // 暂停内核驱动扫描，读取本次扫描结果
  ├─ CheckAndRemoveInvalidProcess()
  ├─ PerformMigrationPreparation()
  │    ├─ GetRamIsChange()      // 检测内存拓扑变化，有变化则跳过本轮
  │    ├─ CleanStrategyAttribute()
  │    └─ BuildAllPidData()     // 读取各 PID 的 actc 数据
  ├─ UpdateScene()              // 每个 VM 调用 SetProcessSceneAttr()，更新场景
  ├─ ConfigRatios()             // 自适应内存配比调整（水线模式/密度模式）
  ├─ PeriodConfigRead()         // 从配置文件读取手动覆盖参数
  ├─ HandleScene() 或 UpdatePeriodFromConfig()   // 更新扫描/迁移周期
  ├─ PerformMigration()         // 执行实际迁移
  └─ EnableTracking()           // 重新启动内核驱动扫描
```

---

## 2. 关键数据结构

### 2.1 PageInfo（热页快照）

```c
// scene_info.h
typedef struct {
    uint32_t nrPages;       // 进程总页数（L1 + L2）
    uint32_t nrL1Page;      // 本端（Local NUMA）页数
    uint32_t nrL2Page;      // 远端（Remote NUMA）页数
    uint32_t nrHot;         // 总热页数 = nrL1Hot + nrL2Hot
    uint32_t nrL1Hot;       // 本端热页数
    uint32_t nrL2Hot;       // 远端热页数
    uint32_t nrL1Guarantee; // 计算所得的本端保障页数（经平滑、上下界裁剪）
    uint32_t nrL1GuaranteeBk; // nrL1Guarantee 的原始值（用于窗口最大值）
    uint32_t nrL1Planed;    // 自适应配比后规划给本端的页数
} PageInfo;
```

`PageInfo` 以环形队列（深度 `PAGE_INFO_DEPTH = 8`）保存最近 8 次扫描快照，字段 `pageInfoIndex` 指向当前最新位置。

### 2.2 SceneInfo（场景信息）

```c
typedef struct {
    int pageInfoIndex;
    PageInfo pageInfo[PAGE_INFO_DEPTH];  // 历史快照环形队列
    Scene lastScene;   // 上一次场景
    Scene currScene;   // 当前场景
    SceneCycle cycles; // 当前生效的扫描/迁移周期
    LocalMemStatus status; // 本端内存满足情况
} SceneInfo;

typedef struct {
    int scanCycle;   // 扫描周期（ms）
    int migCycle;    // 迁移周期（ms）
} SceneCycle;
```

每个 `ProcessAttr`（进程属性）和全局 `ProcessManager` 各持有一份 `SceneInfo`。

---

## 3. 场景识别

### 3.1 三种场景

```c
// scene_info.h
typedef enum {
    LIGHT_STABLE_SCENE = 0,   // 轻载稳态
    HEAVY_STABLE_SCENE,       // 重载稳态
    UNSTABLE_SCENE,           // 非稳态（热页剧烈变化）
    SCENE_MAX,
} Scene;
```

### 3.2 判定逻辑（`scene.c::AnalyzeScene()`）

优先级：**UNSTABLE > HEAVY_STABLE > LIGHT_STABLE**

```
AnalyzeScene()
  ├─ IsUnstableScene()  → true  → UNSTABLE_SCENE
  ├─ IsHeavyLoadScene() → true  → HEAVY_STABLE_SCENE
  └─ 否则               →         LIGHT_STABLE_SCENE
```

#### IsUnstableScene（非稳态判定）

| 状态迁移 | 条件 |
|---------|------|
| 稳态 → 非稳态 | 连续 `ENTER_UNSTABLE_WINDOW_SIZE=2` 次均满足：<br>① `nrHot > 0`<br>② `deltaHot ≥ max(nrPages×1%, 100)`<br>③ `nrL2Hot > 5` |
| 保持非稳态 | 在连续 `EXIT_UNSTABLE_WINDOW_SIZE=6` 次窗口中，任一满足：<br>① `deltaHot ≥ max(nrPages×0.2%, 100)`<br>② `nrL2Hot ≥ 5`<br>并且每一次 `nrHot > 0 && nrL2Hot > 0` |
| 退出非稳态 | 上述 6 次窗口全部不满足 |

`deltaHot = abs(currPageinfo->nrHot - prevPageinfo->nrHot)` 为相邻两次热页数之差。

#### IsHeavyLoadScene（重载判定）

连续 `ENTER_HEAVY_WINDOW_SIZE=3` 次，`nrL1Guarantee ≥ nrPages × 60%`（`HEAVY_HOT_RATIO=0.60`）。

`nrL1Guarantee` 是计算所得的本端保障页数（见第 5 节）。

### 3.3 场景状态下的自适应调整

在 `ConfigRatios()` → `AdjustVmMemRatio()` 中，本端内存充足时会主动降低场景：

- `FULL_SATISFIED`（分配页 == 总页）→ 强制设置为 `LIGHT_STABLE_SCENE`
- `SATISFIED`（分配页 > 保障页）→ 场景降一级

---

## 4. 扫描周期管理

### 4.1 当前配置值

```c
// scene_info.h（全部场景相同，当前无差异化）
#define UNSTABLE_SCAN_CYCLE        200  // ms
#define HEAVY_STABLE_SCAN_CYCLE    200  // ms
#define LIGHT_STABLE_SCAN_CYCLE    200  // ms
```

接口层约束（`smap_interface.h`）：

```c
#define MIN_SCAN_TIME   50    // ms，最小扫描周期
#define MAX_SCAN_TIME 2000    // ms，最大扫描周期
// 须为 50 的倍数（SCAN_MULTIPLE=5，period_config 内部使用）
```

### 4.2 更新路径

```
HandleScene()
  ├─ 遍历所有进程：GetProcessSceneAttr(currScene, &sceneInfo)
  │    → 根据场景写入 sceneInfo.cycles.scanCycle / migCycle
  ├─ 如果进程场景发生变化：UpdateScanTime(current)
  │    → AccessIoctlAddPid() 将 scanCycle 写入内核驱动
  └─ 取所有进程 currScene 的最差值 worstScene
       → 若 worstScene 改变：更新 ctx->period（迁移周期）
```

文件配置开关 `fileConfSwitch` 打开时，`UpdateScanTime()` 使用 `GetScanPeriodConfig()`（配置文件中的全局值）而非 `sceneInfo.cycles.scanCycle`。

---

## 5. 保障页数计算（`nrL1Guarantee`）

每轮 `SetProcessSceneAttr()` → `StatsGuaranteePages()` 更新：

```
nrL1Guarantee = StatsMaxHotPages(最近 5 次的 max(nrHot)) × 1.05
              + nrL2Hot × 1.05         （若远端有访问则追加）
nrL1Guarantee = max(nrL1Guarantee, StatsMaxGuaranteePages(最近 5 次最大))
nrL1Guarantee = clamp(nrL1Guarantee, nrPages×50%, nrPages)
```

常量：`GUARANTEE_AFFLUENT_SIZE=1.05`，`MIN_GUARANTEE_SIZE=0.5`，`KEEP_GUARANTEE_WINDOW_SIZE=5`。

---

## 6. 迁移周期管理

### 6.1 当前配置值

```c
// scene_info.h（全部场景相同）
#define UNSTABLE_MIGRATE_CYCLE        2000  // ms
#define HEAVY_STABLE_MIGRATE_CYCLE    2000  // ms
#define LIGHT_STABLE_MIGRATE_CYCLE    2000  // ms
```

### 6.2 更新路径

`HandleScene()` 在 `worstScene` 发生变化时更新 `ctx->period`：

```c
ctx->period = manager->sceneInfo.cycles.migCycle;
```

`ctx->period` 即线程休眠时间（ms），控制 `ScanMigrateWork()` 的调用频率。

文件配置开关打开时，`UpdatePeriodFromConfig()` 将 `ctx->period` 覆盖为配置文件中的 `migratePeriod`。

---

## 7. 文件配置覆盖（`period_config.c`）

配置文件路径：`/opt/ubturbo/conf/smap/period.config`

| 配置项 | 范围 | 默认值 | 说明 |
|--------|------|--------|------|
| `smap.scan.period` | 50–200 ms，须为 5 的倍数 | 100 ms | 全局统一扫描周期，覆盖场景值 |
| `smap.migrate.period` | 500–2000 ms | 1000 ms | 全局迁移循环周期 |
| `smap.remote.freq.percentile` | 1–100 | 99 | 远端频率百分位，用于过滤离群值 |
| `smap.slow.threshold` | 0–40 | 2 | 频率差阈值（控制 swap 数量） |
| `smap.freq.wt` | 0–65535 | 0（自动） | 频率权重，0 表示动态计算 |
| `smap.period.file.config.switch` | true/false | false | **总开关**，false 时以上配置全部不生效 |

`fileConfSwitch` 为 false 时，以上所有值均被忽略，系统使用场景驱动的自适应值。

---

## 8. 自适应内存配比（`ConfigRatios()`）

仅对 VM 类型进程、水线模式（`WATERLINE_MODE`）生效。

```
ConfigRatios()
  └─ ConfigMultiVmRatioInGroups()   // 按 (L1 NUMA, L2 NUMA) 分组
       ├─ ProcessMultiNumaVmNode()  // 多 NUMA VM 直接复制 l2→l3 配比
       └─ ConfigMultiVmRatio()
            ├─ 计算每个 VM 当前实际本端页数与 nrL1Guarantee 之差（surplus）
            ├─ BalanceSurpluses()   // 在同组 VM 间平衡盈余/不足
            └─ AdjustVmMemRatio()
                 ├─ nrL1Planed = nrL1Guarantee + surplus
                 ├─ 更新 LocalMemStatus
                 ├─ FULL_SATISFIED → currScene = LIGHT_STABLE_SCENE
                 ├─ SATISFIED      → currScene -= 1
                 └─ UpdateMemRatio() → 写入 l3RemoteMemRatio
```

结果 `l3RemoteMemRatio` 在 `CalProcessNuma()` 中转换为 `nrMigratePages`（实际迁移数量）。

---

## 9. 迁移策略（`separate_strategy.c`）

`RunStrategy()` 根据页面大小分派策略：

```
RunStrategy()
  ├─ IsHugeMode() && IsMultiNumaVm() → SeparateStrategyMultiNumaVm()
  ├─ IsHugeMode()                    → SeparateStrategy()         // 2M 页
  └─ 否则                            → SeparateStrategy4K()       // 4K 页
```

### 9.1 2M 大页策略（`SeparateStrategy`）

| 迁移方向 | 触发条件 | 行为 |
|---------|---------|------|
| Demote（本→远） | `nrMigratePages[L1→L2] > 0` | 降迁 raw 数量 + swap 数量 |
| Promote（远→本） | `nrMigratePages[L2→L1] > 0` | 提升 raw 数量 + swap 数量 |
| Swap（互换） | 两者均为 0 | 纯基于频率的热冷互换 |

Swap 数量由 `CalcMigrateNumByFreq()` 计算：
- L1 actcData 升序排（冷→热），L2 actcData 降序排（热→冷）
- 二分查找满足 `freqL1 × freqWt + slowThred < freqL2` 的最大页数
- 受限于 L1/L2 有效长度、远端空闲大页数、`maxMigrate`

`freqWt = max(l2FreqMax/l1FreqMax, 1)`（动态计算，可被配置文件覆盖）

`slowThred = separateParam.slowThred × freqWt`（默认 `slowThred=2`）

### 9.2 4K 小页策略（`SeparateStrategy4K`）

逻辑与 2M 类似，但处理多 L1/L2 NUMA 对组合，对每对 `(localNid, remoteNid)` 独立执行 swap/demote/promote，并用 bucket-sort（`BuildSelectKMlistAddr`）而非 qsort 来高效选取 top-K/bottom-K 页。

### 9.3 多 NUMA VM 策略（`SeparateStrategyMultiNumaVm`）

将所有 L1/L2 的 actcData 合并为 `LevelActcData`，统一按频率排序后执行 swap/demote/promote，最后按源节点分组构建迁移列表。

---

## 10. 多线程迁移

当迁移量较大时，`SetMigrateThreadNum()` 自动开启多线程（仅限 2M 大页模式）：

| 条件 | 线程数 |
|------|--------|
| `isForcedSingleThread`（共享内存映射） | 1 |
| 迁移量 ≤ 40 页 | 1 |
| 40 < 迁移量 ≤ 400 页 | 4 |
| 迁移量 > 400 页 | 8 |

---

## 11. 关键常量汇总

| 常量 | 文件 | 值 | 含义 |
|------|------|-----|------|
| `ENTER_UNSTABLE_THRESHOLD_RATIO` | `scene_info.h` | 0.01 (1%) | 进入非稳态的热页变化率阈值 |
| `ENTER_UNSTABLE_THRESHOLD_NUM` | `scene_info.h` | 100 | 进入非稳态的热页变化绝对值阈值 |
| `EXIT_UNSTABLE_THRESHOLD_RATIO` | `scene_info.h` | 0.002 (0.2%) | 退出非稳态的热页变化率阈值 |
| `EXIT_UNSTABLE_L2HOT_THRESHOLD_NUM` | `scene_info.h` | 5 | 远端热页数阈值（<5 则不视为非稳态）|
| `ENTER_UNSTABLE_WINDOW_SIZE` | `scene_info.h` | 2 | 进入非稳态所需连续窗口数 |
| `EXIT_UNSTABLE_WINDOW_SIZE` | `scene_info.h` | 6 | 保持非稳态的观察窗口数 |
| `HEAVY_HOT_RATIO` | `scene_info.h` | 0.60 | 重载场景本端保障页占比阈值 |
| `ENTER_HEAVY_WINDOW_SIZE` | `scene_info.h` | 3 | 进入重载场景所需连续窗口数 |
| `PAGE_INFO_DEPTH` | `scene_info.h` | 8 | 历史热页快照深度 |
| `GUARANTEE_AFFLUENT_SIZE` | `scene_info.h` | 1.05 | 保障页数超配系数 |
| `MIN_GUARANTEE_SIZE` | `scene_info.h` | 0.5 | 保障页数最低占比（50%）|
| `KEEP_GUARANTEE_WINDOW_SIZE` | `scene_info.h` | 5 | 保障页数平滑窗口 |
| `L2_CHECK_WINDOW_SIZE` | `scene_info.h` | 3 | 远端热页均值窗口 |

---

## 12. 策略现状小结与自适应周期的设计切入点

**现状**：三种场景的扫描周期（200 ms）和迁移周期（2000 ms）当前**完全相同**，场景识别机制已完备但尚未体现在周期差异上。

**切入点**：若要实现 idle VM 的扫描周期自适应，可在以下位置介入：

1. **`scene.c::GetProcessSceneAttr()`** — 为某个场景（或新增 idle 判断）赋予不同的 `scanCycle` 值，最小改动。
2. **`migration.c::HandleScene()`** — 在已有场景处理之后，对热页特征符合"静止"条件的 VM 直接修改其 `sceneInfo.cycles.scanCycle`，不影响场景状态机。
3. **`migration.c::HandleScene()` 中的 `worstScene`** — 控制全局迁移周期 `ctx->period`，当所有 VM 均 idle 时可拉长。

可利用的热页字段：`nrHot`（热页总数）、`deltaHot`（热页变化量 = `abs(curr - prev)`）、`nrL2Hot`（远端热页）。这三者在每轮 `SetProcessSceneAttr()` 后即可读取。
