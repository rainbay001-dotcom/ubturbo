# MGLRU（Multi-Gen LRU，多代 LRU）内核实现调研

> 调研基线：openEuler OLK-6.6 源码稀疏检出（`vendor/openeuler-kernel/`），
> pin 在上游 HEAD `e156f160bb75`（`!21981 Fix CVE-2026-31415`，2026-04-28）。
> 本文所有代码引用均来自该快照，行号对应快照内容。
>
> 主要源文件：
> - `mm/vmscan.c` —— MGLRU 全部核心算法（aging / eviction / 接口）
> - `include/linux/mmzone.h` —— 核心数据结构与常量
> - `mm/Kconfig` —— 编译期开关
> - `Documentation/mm/multigen_lru.rst` —— 设计文档
> - `Documentation/admin-guide/mm/multigen_lru.rst` —— 管理员/运行时接口文档

---

## 1. 概述

Multi-Gen LRU（MGLRU）是 Linux 内核中传统 active/inactive 双链表 LRU 的
替代实现，目标是优化页面回收（page reclaim），在内存压力下提升性能、降低
kswapd 的 CPU 开销并提高内存超分（overcommit）效率。

设计目标（`Documentation/mm/multigen_lru.rst:13-21`）：

- 良好地表达访问新近度（access recency）
- 尽量利用空间局部性（spatial locality）
- 用快路径（fast paths）做出显而易见的选择
- 简单的自校正启发式（self-correcting heuristics）

核心思想：把可回收页面划分为多个**代（generation）**，每一代代表一组访问
新近度相近的页面。代之间形成一个基于时间的统一参照系，使得跨 memcg、甚至
跨机器（数据中心调度）的页面冷热比较成为可能。

---

## 2. 编译期与运行期开关

### 2.1 Kconfig（`mm/Kconfig:1306-1330`）

| 配置项 | 含义 |
| --- | --- |
| `CONFIG_LRU_GEN` | MGLRU 主开关；依赖 `MMU`，且要求 `64BIT \|\| !SPARSEMEM \|\| SPARSEMEM_VMEMMAP`（保证 `folio->flags` 有足够空闲位）|
| `CONFIG_LRU_GEN_ENABLED` | 默认启用 MGLRU（决定运行期 `enabled` 的默认值）|
| `CONFIG_LRU_GEN_STATS` | 调试用：保留已驱逐代的历史统计，有 per-memcg / per-node 内存开销 |

### 2.2 运行期接口（`Documentation/admin-guide/mm/multigen_lru.rst`）

稳定 ABI：`/sys/kernel/mm/lru_gen/`

- **`enabled`**（kill switch，`mm/vmscan.c:5166-5210`）：位掩码控制三个能力位
  - `0x0001` `LRU_GEN_CORE`：MGLRU 主开关
  - `0x0002` `LRU_GEN_MM_WALK`：批量清理**叶子** PTE 的 accessed 位
  - `0x0004` `LRU_GEN_NONLEAF_YOUNG`：连**非叶**页表项的 accessed 位也清理（x86 等 MMU 置位场景）
  - 也接受 `[yYnN]` 作用于全部能力位
- **`min_ttl_ms`**（`mm/vmscan.c:5145-5164`）：防抖动（thrashing prevention）。写入 `N` 表示保护近 `N` 毫秒的工作集不被驱逐；若该工作集无法保留则触发 OOM。默认 `0`（关闭）。

实验性 / 调试接口：`/sys/kernel/debug/lru_gen`（读写）与 `/sys/kernel/debug/lru_gen_full`（只读，更多统计）
（`mm/vmscan.c:5654-5655` 注册）：

- **工作集估计**：读 `lru_gen` 返回每个 memcg/node 上、按不同时间区间访问的页面数量直方图。
- **主动回收 / 创建代**：写命令
  - `+ memcg_id node_id max_gen_nr [can_swap [force_scan]]` —— 创建新一代（aging）
  - `- memcg_id node_id min_gen_nr [swappiness [nr_to_reclaim]]` —— 驱逐 ≤ `min_gen_nr` 的代（proactive reclaim）

运行期能力通过 static key 数组控制
（`mm/vmscan.c:2587-2591`：`DEFINE_STATIC_KEY_ARRAY_*(lru_gen_caps, NR_LRU_GEN_CAPS)`，
`get_cap()` 宏），切换主开关走 `lru_gen_change_state()`（`mm/vmscan.c:5090`）。

---

## 3. 核心数据结构

### 3.1 代与层的常量（`include/linux/mmzone.h:363-386`）

```c
#define MIN_NR_GENS   2U   /* "二次机会"算法至少需要 2 代 */
#define MAX_NR_GENS   4U   /* 最多 4 代，folio->flags 用 order_base_2(MAX_NR_GENS+1) 位存 gen */
#define MAX_NR_TIERS  4U   /* 每代再分多个 tier，用 MAX_NR_TIERS-2 个 folio->flags 空闲位 */
```

- **代（generation）**：滑动窗口 `[MIN_NR_GENS, MAX_NR_GENS]`。`max_seq`（最年轻）
  与 `min_seq[]`（最老）单调递增。`folio->flags` 中的 gen 计数器在页面挂在
  `lrugen->folios[]` 上时存 `gen+1`（取值 `[1, MAX_NR_GENS]`），否则存 0
  （`include/linux/mmzone.h:340-341`、设计文档 `multigen_lru.rst:90-96`）。
- **层（tier）**：同一代内按"通过文件描述符访问 N 次"再分层，页面在
  `order_base_2(N)` 层。**层不占用独立链表**，跨层移动只需对 `folio->flags`
  做原子操作（无需 LRU 锁），开销极低（`include/linux/mmzone.h:366-386`）。
  第一层（N=0,1）由 `PG_referenced` 标记，更高层额外用 `PG_workingset` 标记。

### 3.2 `struct lru_gen_folio`（`include/linux/mmzone.h:435-465`）

每个 `lruvec` 内嵌一个，是 MGLRU 的核心状态：

```c
struct lru_gen_folio {
    unsigned long max_seq;                  /* aging 递增的最年轻代号 */
    unsigned long min_seq[ANON_AND_FILE];   /* eviction 递增的最老代号（anon/file 分开）*/
    unsigned long timestamps[MAX_NR_GENS];  /* 每代出生时间（jiffies），用于工作集保护 */
    struct list_head folios[MAX_NR_GENS][ANON_AND_FILE][MAX_NR_ZONES]; /* 多代 LRU 链表 */
    long nr_pages[MAX_NR_GENS][ANON_AND_FILE][MAX_NR_ZONES];           /* 各代页数，最终一致 */
    unsigned long avg_refaulted[ANON_AND_FILE][MAX_NR_TIERS]; /* refault 指数移动平均 */
    unsigned long avg_total[ANON_AND_FILE][MAX_NR_TIERS];     /* evicted+protected 移动平均 */
    unsigned long protected[NR_HIST_GENS][ANON_AND_FILE][MAX_NR_TIERS - 1];
    atomic_long_t evicted[NR_HIST_GENS][ANON_AND_FILE][MAX_NR_TIERS];
    atomic_long_t refaulted[NR_HIST_GENS][ANON_AND_FILE][MAX_NR_TIERS];
    bool enabled;
#ifdef CONFIG_MEMCG
    u8 gen;                       /* 该 lru_gen_folio 所属的 memcg 代 */
    u8 seg;                       /* 所属链表段（head/tail/default）*/
    struct hlist_nulls_node list; /* per-node memcg LRU 链表节点 */
#endif
};
```

> 关键注释：anon 与 file **共用** `max_seq`（一起 aging），但 `min_seq[]` 分开
> （干净 file 页可不受 swap 约束直接驱逐）。当 swap 受限时，file `min_seq`
> 允许领先 anon（`include/linux/mmzone.h:422-433`）。

### 3.3 页表扫描相关结构

- **`struct lru_gen_mm_state`**（`include/linux/mmzone.h:480-491`）：per-memcg 的
  `mm_struct` 遍历游标 + 双缓冲 **Bloom filter**（`NR_BLOOM_FILTERS=2`，每轮翻转）。
- **`struct lru_gen_mm_walk`**（`include/linux/mmzone.h:493-508`）：一次页表遍历的
  上下文（当前 lruvec、unstable `max_seq`、下一个扫描地址、批量晋升计数、
  `can_swap`、`force_scan`）。
- **MM 统计枚举**（`include/linux/mmzone.h:467-475`）：`MM_LEAF_TOTAL/OLD/YOUNG`、
  `MM_NONLEAF_TOTAL/FOUND/ADDED`，用于 Bloom filter 命中率统计。

### 3.4 Memcg LRU（`include/linux/mmzone.h:557-569`）

```c
#define MEMCG_NR_GENS  3   /* old(seq) / young(seq+1)，第三代防止无锁读时 seq-1 回绕到 young */
#define MEMCG_NR_BINS  8   /* 随机分片，提升并行度 */

struct lru_gen_memcg {
    unsigned long seq;
    unsigned long nr_memcgs[MEMCG_NR_GENS];
    struct hlist_nulls_head fifo[MEMCG_NR_GENS][MEMCG_NR_BINS];
    spinlock_t lock;
};
```

per-node 的"memcg 的 LRU"，仅用于全局回收。四种操作
`MEMCG_LRU_HEAD/TAIL/OLD/YOUNG` 及触发事件详见
`include/linux/mmzone.h:515-556`。把全局回收遍历 memcg 的**最好情况复杂度从
O(n) 降到 O(1)**，最坏仍 O(n)，平均亚线性（设计文档 `multigen_lru.rst:248-251`）。

`struct lruvec` 内嵌 `lrugen` 与 `mm_state`（`include/linux/mmzone.h:654-658`）。

---

## 4. 两大核心流程：Aging 与 Eviction

aging 与 eviction 构成生产者—消费者闭环（page reclaim）。
（设计文档 `multigen_lru.rst:109-110, 264-269`）

### 4.1 Aging（产生年轻代）

入口与关键函数（`mm/vmscan.c`）：

| 函数 | 行号 | 作用 |
| --- | --- | --- |
| `lru_gen_age_node()` | 3973 | kswapd 路径的节点级 aging 入口；遍历 memcg，必要时触发 OOM |
| `try_to_inc_max_seq()` | 3844 | 尝试递增 `max_seq`（产生新一代）|
| `inc_max_seq()` | 3787 | 实际递增 `max_seq` 并滚动滑动窗口 |
| `walk_mm()` | 3631 | 对单个 `mm_struct` 调用 `walk_page_range()` 扫描 PTE |
| `walk_pmd_range()` / `walk_pte_range()` | 3507 / 3352 | 实际页表遍历，找 young PTE |
| `walk_pmd_range_locked()` | 3423/3501 | 非叶项处理（`LRU_GEN_NONLEAF_YOUNG`）|

机制（设计文档 `multigen_lru.rst:112-126`）：
- 当 `max_seq - min_seq + 1` 接近 `MIN_NR_GENS` 时递增 `max_seq`。
- aging 通过**页表遍历**与 **rmap 遍历**寻找 young PTE。
  - 页表遍历：迭代 `lruvec_memcg()->mm_list`，对每个 `mm_struct` 调
    `walk_page_range()`，每轮结束递增 `max_seq`；多个遍历者各取不同
    `mm_struct`，可并行（`multigen_lru.rst:166-170`）。
  - 找到 young PTE 后清 accessed 位，把对应页 gen 更新为 `(max_seq%MAX_NR_GENS)+1`。
- 冷页降级（demotion）随 `max_seq` 递增**自然发生**。

`lru_gen_age_node()` 还承担**工作集保护 + OOM** 职责
（`mm/vmscan.c:3973-4007`）：读取 `lru_gen_min_ttl`，若所有 memcg 的所有代都
比 `min_ttl` 年轻（即工作集放不下），在 `oom_lock` 下调用 `out_of_memory()`。

### 4.2 Rmap/PT 反馈与 Bloom filter

`lru_gen_look_around()`（`mm/vmscan.c:4020`，声明 `include/linux/mmzone.h:511`）
利用空间局部性减少 rmap 开销：当 eviction 走 rmap 命中一个 young PTE 时，
扫描其**相邻 PTE** 并晋升热页；若扫描"缓存行高效"，把指向该 PTE 表的 PMD 项
加入 **Bloom filter**（`multigen_lru.rst:181-209`）。

Bloom filter 形成 eviction → aging 的反馈回路：eviction 路径把"热且密集"的
PMD 地址放入 filter；aging 路径据此判定哪些 PTE 范围值得扫描。Bloom filter
是概率性的，假阳性的代价仅是多扫一段 PTE。

### 4.3 Eviction（消费老代）

关键函数（`mm/vmscan.c`）：

| 函数 | 行号 | 作用 |
| --- | --- | --- |
| `evict_folios()` | 4557 | 驱逐入口：隔离 + `shrink_folio_list()` |
| `isolate_folios()` | 4515 | 选定 type/tier 后隔离一批 folio |
| `scan_folios()` | 4385 | 扫描某 type/zone 的代链表，调用 `sort_folio()` |
| `sort_folio()` | 4273 | 对单个 folio 分类：晋升 / 保护到下一代 / 加入隔离批次 |
| `inc_min_seq()` | 3708 | 某代链表空后递增 `min_seq` |
| `try_to_inc_min_seq()` | 3744 | 尝试推进 `min_seq` |

机制（设计文档 `multigen_lru.rst:128-143`）：
- 当 `min_seq%MAX_NR_GENS` 指向的链表变空时递增 `min_seq`。
- 选择驱逐的 type 与 tier：先比 `min_seq[]` 选更老的 type；若一样老，选第一层
  refault 百分比更低者。第一层是"单次使用、未映射、干净"的页，最适合驱逐。
- `sort_folio()` 按 gen 计数器对页排序（aging 若已通过页表发现该页被访问并更新
  了 gen）；若页通过文件描述符多次访问且 PID 反馈检测到该层有异常 refault，
  则把它移动到 `min_seq+1`（保护）。

### 4.4 调度：should_run_aging 与 get_nr_to_scan

`should_run_aging()`（`mm/vmscan.c:4648-4713`）决定该 lruvec 是否需要先 aging：

- 若 `min_seq[!can_swap] + MIN_NR_GENS > max_seq`（完全没有冷页），`nr_to_scan=0`
  并返回 `true`（必须 aging）。
- 统计 `young`（`seq==max_seq`）、`old`（`seq+MIN_NR_GENS==max_seq`）、`total`。
- `nr_to_scan = total >> sc->priority`。
- 理想代数是 `MIN_NR_GENS+1`；理想分布是每代约占 `1/(MIN_NR_GENS+1)`：
  - `young * MIN_NR_GENS > total` → 热页太多，需 aging（`true`）
  - `old * (MIN_NR_GENS + 2) < total` → 冷页太少，需 aging（`true`）

`get_nr_to_scan()`（`mm/vmscan.c:4720-4738`）：
- memcg 低于 min 保护线 → 返回 -1（跳过）。
- 不需要 aging → 返回 `nr_to_scan`。
- 默认优先级（`DEF_PRIORITY`）下跳过 aging 路径（aging 偏惰性以降开销）。
- 否则调 `try_to_inc_max_seq()`：成功返回 -1（本轮跳过），失败返回 0。

`should_abort_scan()`（`mm/vmscan.c:4740-4769`）：全局回收下，已达回收目标或所有
合格 zone 水位安全时让 kswapd 提前退出。

整体收缩走 `try_to_shrink_lruvec()`（`mm/vmscan.c:4771`）→
`lru_gen_shrink_lruvec()`（`mm/vmscan.c:4926`，`CONFIG_LRU_GEN` 关闭时为
`mm/vmscan.c:5667` 的空实现），最终接入通用 `shrink_lruvec()`（`mm/vmscan.c:5677`）。

### 4.5 PID 控制器（refault 反馈）

设计文档 `multigen_lru.rst:211-221`：仿 PID 控制器的反馈回路监控 anon/file 两种
type 在各 tier 上的 refault，决定同一代同时存在两种 type 时驱逐哪一种。

- **以"代"而非墙钟时间为时间域**（CPU 在不同内存压力下扫描速率不同）。
- 每产生新一代就计算一次移动平均（`avg_refaulted` / `avg_total`，见 3.2），
  避免永久锁死在次优状态。
- 目标：使 anon/file 的 refault 百分比按 `swappiness` 等比平衡。

---

## 5. 关键设计要点小结

1. **代 = 统一的时间参照系**：让冷热判断在 memcg / 机器之间可比，服务于数据中心
   的内存超分与作业调度。
2. **两条访问通道区别对待**（设计文档 `multigen_lru.rst:50-78`）：
   - 页表通道（mapped）默认保护更强（accessed 位近似、TLB flush 成本高、缺页惩罚大）。
   - 文件描述符通道默认按"无时间局部性"处理，除非观测到异常 refault。
3. **两种扫描方式互补**：rmap 遍历精准但对大量 mapped 页 CPU 成本高；页表遍历可
   一次性扫光地址空间的 young PTE 但地址空间可能太稀疏。Bloom filter 让二者形成
   反馈、各取所长。
4. **层间移动免锁**：tier 仅靠 `folio->flags` 原子位操作，缓冲访问路径开销可忽略；
   代间移动才需 LRU 锁。
5. **Memcg LRU 提升全局回收可扩展性**：分片（随机起点）+ 最终公平（direct reclaim
   可随时 bail out），平均亚线性遍历复杂度。
6. **工作集保护 + OOM 兜底**：`min_ttl_ms` 是可调的"压力释放阀"，直接接 OOM killer，
   对应用与内存大小不敏感，配置简单。

### MGLRU 可拆解为（设计文档 `multigen_lru.rst:253-269`）

- Generations（代）
- Rmap walks（rmap 遍历）
- 经 `mm_struct` 链表的 Page table walks（页表遍历）
- 用于 rmap/PT 反馈的 Bloom filters
- 用于 refault 反馈的 PID controller

aging 与 eviction 是生产者—消费者：eviction 通过代上的滑动窗口驱动 aging；aging
内部 rmap 遍历通过把热且密集的页表插入 Bloom filter 驱动页表遍历；eviction 内部
PID 控制器以 refault 为反馈选择驱逐的 type 与保护的 tier。

---

## 6. 源码索引（便于后续查阅）

| 主题 | 位置 |
| --- | --- |
| Kconfig 开关 | `mm/Kconfig:1306-1330` |
| 代/层常量与说明 | `include/linux/mmzone.h:335-386` |
| `lru_gen_folio` | `include/linux/mmzone.h:435-465` |
| mm walk / Bloom filter 结构 | `include/linux/mmzone.h:467-508` |
| Memcg LRU 结构与语义 | `include/linux/mmzone.h:513-569` |
| lruvec 内嵌 lrugen | `include/linux/mmzone.h:654-658` |
| 能力 static key | `mm/vmscan.c:2587-2591` |
| 页表遍历 | `mm/vmscan.c:3352-3708`（`walk_pte/pmd_range`、`walk_mm`）|
| min/max_seq 递增 | `mm/vmscan.c:3708-3971` |
| 节点级 aging + OOM | `mm/vmscan.c:3973-4007` |
| `lru_gen_look_around` | `mm/vmscan.c:4020` |
| sort/scan/isolate/evict | `mm/vmscan.c:4273-4646` |
| 调度（should_run_aging / get_nr_to_scan）| `mm/vmscan.c:4648-4738` |
| shrink_lruvec 接入 | `mm/vmscan.c:4771-4954`、`5677` |
| `enabled` / `min_ttl_ms` sysfs | `mm/vmscan.c:5090-5221` |
| debugfs 接口 | `mm/vmscan.c:5223-5655` |
| 设计文档 | `Documentation/mm/multigen_lru.rst` |
| 管理员/运行时文档 | `Documentation/admin-guide/mm/multigen_lru.rst` |

---

*本调研基于 `vendor/openeuler-kernel/`（OLK-6.6，pin `e156f160bb75`）只读快照，
由 @claude 整理。如需基于更新的上游快照复核，请按 `CLAUDE.md` 所述刷新 kernel 参考。*
