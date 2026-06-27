# MGLRU（Multi-Gen LRU）内核实现调研

> 基础参考：openEuler OLK-6.6 稀疏检出（`vendor/openeuler-kernel/`），pin 在上游 HEAD
> `e156f160bb75`（`!21981 Fix CVE-2026-31415`，2026-04-28）。
> 主要源文件：`mm/vmscan.c`、`include/linux/mmzone.h`、`mm/workingset.c`、
> `include/linux/mm_inline.h`、`mm/Kconfig`，文档 `Documentation/mm/multigen_lru.rst`、
> `Documentation/admin-guide/mm/multigen_lru.rst`。
>
> 本文面向有内核内存管理背景的读者，按 issue #27 的复审意见重写，重点补强：
> ① 关键函数源码与调用关系；② 规格评估（触发时机/频率/开销）；③ 整机多 NUMA
> node 视角；④ 与 smaps 扫描的对比与借鉴；⑤ 设计演进的取舍。所有引用均带
> `file:line`，行号以上述 pin 的检出为准。

---

## 0. TL;DR（结论先行）

- MGLRU 用 **多个"代"（generation）** 替代 active/inactive 双链表，用 **层（tier）**
  在代内做免锁的访问频度区分；冷热判定的"权威"落在 `struct folio->flags` 的
  gen/tier 位上，而不是某个进程的虚拟地址空间扫描结果。
- 整条流水线是一个 **生产者—消费者闭环**：`aging` 生产年轻代（晋升热页、推进
  `max_seq`），`eviction` 消费最老代（驱逐冷页、推进 `min_seq`）。两条反馈回路
  把二者耦合起来：`look_around` 的空间局部性反馈 + 仿 PID 的 refault 反馈。
- 触发与 legacy 回收 **完全复用同一套水线/kswapd 框架**：每个 NUMA node 一个
  `kswapd`，`balance_pgdat()` 按 zone 水线决定是否回收；MGLRU 只是替换了 node 内
  "扫哪些页、扫多少"的策略层（`shrink_node`/`shrink_lruvec` 的分派）。
- 设计演进的核心动机：**正向（按 pid 遍历全部虚拟地址空间）扫描的成本与回收收益
  不成正比**；MGLRU 改成"以 `struct page`/folio 为中心反向定温"，只在回收压力下、
  只对候选页、借 rmap 的空间局部性做 **有界** 的页表回扫，从而把开销和压力对齐。

---

## 1. 概念模型与术语

传统 LRU 维护 active/inactive 两条链，"二次机会"算法靠 `PG_referenced`+`PG_active`
两个 bit。MGLRU 把它一般化：

- **代（generation, gen）**：页按"最近一次被认定访问的时间窗口"落在某一代。
  `max_seq` 是最年轻代序号，`min_seq[type]` 是最老代序号。序号到下标的映射为
  `lru_gen_from_seq(seq) = seq % MAX_NR_GENS`。
  - `MIN_NR_GENS = 2U` —— `include/linux/mmzone.h:363`，做"二次机会"至少要 2 代。
  - `MAX_NR_GENS = 4U` —— `include/linux/mmzone.h:364`，受 `folio->flags` 位宽限制
    （`order_base_2(MAX_NR_GENS+1)=3` bit）。
- **层（tier, tier）**：同一代内按"通过文件描述符被访问的次数"分层，层间移动
  **不需要 LRU 锁**，只对 `folio->flags` 做原子位操作。
  - `MAX_NR_TIERS = 4U` —— `include/linux/mmzone.h:386`。
  - tier 0 = 仅 PTE young 一次；tier `i>0` = 通过 `folio_mark_accessed()` 被额外
    访问了 `2^(i-1)..2^i-1` 次。
- **`folio->flags` 编码**：gen 占 `LRU_GEN_WIDTH` 位（偏移 `LRU_GEN_PGOFF`，
  `mmzone.h:1116`），tier/refs 占 `LRU_REFS_WIDTH` 位（偏移 `LRU_REFS_PGOFF`，
  `mmzone.h:1117`）；`LRU_REFS_FLAGS = PG_referenced|PG_workingset`（`mmzone.h:410`）。
  这是"冷热信息直接挂在页上"的物理体现。

---

## 2. 运行期接口与配置

### 2.1 Kconfig（`mm/Kconfig`）

| 选项 | 行 | 含义 |
|---|---|---|
| `CONFIG_LRU_GEN` | `mm/Kconfig:1307` | 总开关，依赖 `MMU` |
| `CONFIG_LRU_GEN_ENABLED` | `mm/Kconfig:1316` | 是否开机即启用（决定静态键初值） |
| `CONFIG_LRU_GEN_STATS` | `mm/Kconfig:1322` | 保留已驱逐代的历史统计（有内存开销） |

### 2.2 静态键与运行期开关

- 能力静态键数组 `lru_gen_caps[NR_LRU_GEN_CAPS]` —— `mm/vmscan.c:2587`
  （`CONFIG_LRU_GEN_ENABLED` 时 `DEFINE_STATIC_KEY_ARRAY_TRUE`，否则 FALSE）。
- `lru_gen_enabled()` —— `include/linux/mm_inline.h`，本质是
  `static_branch_[un]likely(&lru_gen_caps[LRU_GEN_CORE])`。回收主路径据此分派。
- 能力位：`LRU_GEN_CORE`(0x1) / `LRU_GEN_MM_WALK`(0x2，批量清叶子 PTE young) /
  `LRU_GEN_NONLEAF_YOUNG`(0x4，清非叶 PMD young)。
  - `should_walk_mmu()` —— `mm/vmscan.c:2594` = `arch_has_hw_pte_young() && get_cap(LRU_GEN_MM_WALK)`
  - `should_clear_pmd_young()` —— `mm/vmscan.c:2599` = `arch_has_hw_nonleaf_pmd_young() && get_cap(LRU_GEN_NONLEAF_YOUNG)`

### 2.3 sysfs：`/sys/kernel/mm/lru_gen/`

- 属性组 `lru_gen_attr_group` —— `mm/vmscan.c:5218`。
- `enabled`（RW）—— `enabled_show()` `:5166` / `enabled_store()` `:5183`：读写上面 3 个
  能力位；`CORE` 通过 `lru_gen_change_state()` 切静态分支并做链表迁移。
- `min_ttl_ms`（RW）—— `min_ttl_ms_show()` `:5145` / `min_ttl_ms_store()` `:5151`：
  工作集保护时间（见 §5.1），默认 `0`（禁用）。

### 2.4 debugfs：`/sys/kernel/debug/lru_gen`、`lru_gen_full`

- 读 `lru_gen_seq_show()` —— `mm/vmscan.c:5329`：打印每 memcg×node×gen 的年龄与
  anon/file 页数，可做 **工作集估计（working-set estimation）**。
- 写 `lru_gen_seq_write()` —— `mm/vmscan.c:5488` → `run_cmd()` `:5440`：
  - `+ memcg_id node_id seq [swappiness [opt]]` → `run_aging()` `:5392`：主动产生新代
    （工作集采样）。
  - `- memcg_id node_id seq [swappiness [nr_to_reclaim]]` → `run_eviction()` `:5412`：
    **主动回收（proactive reclaim）** 指定代及更老的页。

这套 debugfs 接口正是"用户态可驱动的冷热采样 + 主动回收"，对上层做内存调控
（含 ubturbo 这类场景）很关键——可在不触发水线的情况下，按需采样/回收。

---

## 3. 核心数据结构（`include/linux/mmzone.h`）

### 3.1 `struct lru_gen_folio`（per-lruvec 的代/层状态）—— `mmzone.h:435`

```c
struct lru_gen_folio {
    unsigned long max_seq;                                   /* :437 最年轻代 */
    unsigned long min_seq[ANON_AND_FILE];                   /* :439 anon/file 各自最老代 */
    unsigned long timestamps[MAX_NR_GENS];                  /* :441 每代诞生 jiffies，用于年龄 */
    struct list_head folios[MAX_NR_GENS][ANON_AND_FILE][MAX_NR_ZONES]; /* :443 [gen][type][zone] */
    long nr_pages[MAX_NR_GENS][ANON_AND_FILE][MAX_NR_ZONES];/* :445 各桶页数(最终一致) */
    /* 下面三组是仿 PID 控制器的统计 */
    unsigned long avg_refaulted[ANON_AND_FILE][MAX_NR_TIERS];/* :447 refault 滑动平均 */
    unsigned long avg_total[ANON_AND_FILE][MAX_NR_TIERS];   /* :449 (evicted+protected) 滑动平均 */
    unsigned long protected[NR_HIST_GENS][ANON_AND_FILE][MAX_NR_TIERS-1]; /* :451 被保护(晋升)页数 */
    atomic_long_t evicted[NR_HIST_GENS][ANON_AND_FILE][MAX_NR_TIERS];     /* :453 */
    atomic_long_t refaulted[NR_HIST_GENS][ANON_AND_FILE][MAX_NR_TIERS];   /* :454 */
    bool enabled;                                           /* :456 */
    u8 gen; u8 seg;                                         /* :459/:461 memcg LRU 中的代/段(CONFIG_MEMCG) */
    struct hlist_nulls_node list;                          /* :463 挂入 per-node memcg LRU */
};
```

内嵌于 `struct lruvec`：`struct lru_gen_folio lrugen;` —— `mmzone.h:656`。
也就是说 **每个 (memcg, node) 的 lruvec 各有一份代/层状态**——这是后面讨论
"整机/多 node"的基本单元。

### 3.2 Bloom filter 与 mm 遍历状态

- `struct lru_gen_mm_state` —— `mmzone.h:480`：`seq`、遍历断点 `head/tail`、
  双缓冲 Bloom filter `filters[NR_BLOOM_FILTERS]`（`NR_BLOOM_FILTERS=2`，`mmzone.h:478`）。
- `struct lru_gen_mm_walk` —— `mmzone.h:493`：一次页表遍历的临时状态
  （`max_seq` 快照、`next_addr` 断点、批量晋升计数 `nr_pages[][][]`、`can_swap`、
  `force_scan`）。
- Bloom filter 宽度 `BLOOM_FILTER_SHIFT = 15` —— `mm/vmscan.c:2702`（即 2^15 bit）。
  内嵌于 lruvec：`struct lru_gen_mm_state mm_state;` —— `mmzone.h:658`。

### 3.3 per-node memcg LRU —— `struct lru_gen_memcg`，`mmzone.h:560`

```c
struct lru_gen_memcg {
    unsigned long seq;                                   /* :562 本 node 的 memcg 代计数 */
    unsigned long nr_memcgs[MEMCG_NR_GENS];              /* :564 每代 memcg 数 */
    struct hlist_nulls_head fifo[MEMCG_NR_GENS][MEMCG_NR_BINS]; /* :566 [gen][bin] FIFO */
    spinlock_t lock;                                     /* :568 */
};
```

`MEMCG_NR_GENS = 3`（`mmzone.h:557`，old/young/读保护）、`MEMCG_NR_BINS = 8`
（`mmzone.h:558`，随机分桶）。挂在 `pgdat->memcg_lru`，**全机每个 NUMA node 一份**。
它把"全局回收时遍历所有 memcg"的最坏复杂度从 O(memcg 数) 降到 O(bins)=O(8)，
即近似 O(1)（见 §6.3）。

---

## 4. 关键路径调用关系与源码

### 4.1 顶层分派（与 legacy 共用入口）

```
balance_pgdat()                         mm/vmscan.c:6802   (每 node 一个 kswapd 调用)
├─ kswapd_age_node()                    mm/vmscan.c:6581
│   └─ lru_gen_age_node()               mm/vmscan.c:3973   ← MGLRU aging 入口
└─ kswapd_shrink_node()                 mm/vmscan.c:6720
    └─ shrink_node()                    mm/vmscan.c:5923
        ├─[lru_gen_enabled && root_reclaim]→ lru_gen_shrink_node()  :4961
        └─[否则] shrink_node_memcgs()/legacy

shrink_lruvec()                         mm/vmscan.c:5677
└─[lru_gen_enabled && !root_reclaim]→ lru_gen_shrink_lruvec()        :4926
```

- 直接回收侧：`shrink_zones()`（`:6108`）按 zonelist 遍历、**按 node 去重** 后对每个
  node 调 `shrink_node()`（`:6187`）。也就是 legacy 与 MGLRU 走 **同一套 node 选择/
  水线框架**，仅在 `shrink_node`/`shrink_lruvec` 内分派到 MGLRU 策略。

### 4.2 Aging 路径（生产年轻代）

调用关系：

```
lru_gen_age_node()                      :3973  (仅 kswapd；min_ttl 工作集保护 + OOM 兜底)
try_to_inc_max_seq()                    :3844  (决定是否推进 max_seq)
├─ should_walk_mmu()                    :2594  (走页表 or 免走)
├─ iterate_mm_list() / get_next_mm      :2931  (遍历待扫 mm；首次 reset_bloom_filter :2989)
│   └─ walk_mm()                        :3631
│       └─ walk_page_range()            :3659  → 通用页表遍历回调:
│           walk_pud_range()            :3589
│           └─ walk_pmd_range()         :3507  (THP 直接处理；两遍法：先收集 young PMD 位图)
│               └─ walk_pte_range()     :3352
│                   ├─ get_pte_pfn()    :3283  (校验/取 PFN)
│                   ├─ pte_young()/ptep_test_and_clear_young()  (清 accessed)
│                   ├─ test_bloom_filter() :2719  (跳过不大可能有 young 的 PTE 表)
│                   ├─ update_bloom_filter():2734
│                   └─ folio_update_gen() :3121 (晋升到 new_gen)
└─ inc_max_seq()                        :3787  (推进 max_seq；对每 type 调 inc_min_seq、reset_ctrl_pos)
```

`should_skip_vma()`（`:3214`）作为 `mm_walk.test_walk` 跳过整段不可回收/不可访问
VMA；`get_pte_pfn()`（`:3283`）过滤零页/devmap/special。

**`lru_gen_age_node()` 源码（min_ttl 保护 + OOM 兜底）**——`mm/vmscan.c:3973`：

```c
static void lru_gen_age_node(struct pglist_data *pgdat, struct scan_control *sc)
{
    struct mem_cgroup *memcg;
    unsigned long min_ttl = READ_ONCE(lru_gen_min_ttl);
    bool reclaimable = !min_ttl;

    VM_WARN_ON_ONCE(!current_is_kswapd());           /* 只在 kswapd 上下文做 aging */
    set_initial_priority(pgdat, sc);

    memcg = mem_cgroup_iter(NULL, NULL, NULL);
    do {
        struct lruvec *lruvec = mem_cgroup_lruvec(memcg, pgdat);
        mem_cgroup_calculate_protection(NULL, memcg);
        if (!reclaimable)
            reclaimable = lruvec_is_reclaimable(lruvec, sc, min_ttl);
    } while ((memcg = mem_cgroup_iter(NULL, memcg, NULL)));

    /* 若所有 memcg 的每一代都比 min_ttl 年轻 → 判定 thrash，OOM 兜底 */
    if (!reclaimable && mutex_trylock(&oom_lock)) {
        struct oom_control oc = { .gfp_mask = sc->gfp_mask, };
        out_of_memory(&oc);
        mutex_unlock(&oom_lock);
    }
}
```

注意：**aging 的"是否真的推进代"并不在这里强制**，而是在 eviction 侧按需触发
（见 §4.4 的 `get_nr_to_scan`）。`lru_gen_age_node()` 本身主要承担 **工作集保护
（min_ttl）与 OOM 兜底**。

### 4.3 Eviction 路径（消费最老代）

```
try_to_shrink_lruvec()                  :4771  (主循环)
├─ get_nr_to_scan()                     :4720  → should_run_aging() :4648
│                                              → (需要时) try_to_inc_max_seq() :3844
└─ evict_folios()                       :4557
    ├─ isolate_folios()                 :4515
    │   ├─ get_type_to_scan()           :4487  ┐ 仿 PID：按 refault 比 + swappiness
    │   ├─ get_tier_idx()               :4467  ┘ 选 type / tier 截断
    │   │   ├─ read_ctrl_pos()          :3052
    │   │   └─ positive_ctrl_err()      :3105
    │   └─ scan_folios()                :4385
    │       └─ sort_folio()             :4273  (过滤：不可回收/已晋升/受保护/脏/锁→folio_inc_gen 晋升)
    │           └─ folio_inc_gen()      :3144
    └─ try_to_inc_min_seq()             :3744  (最老代空了→推进 min_seq；reset_ctrl_pos)
        └─ inc_min_seq()                :3708
```

### 4.4 Aging↔Eviction 的"代数调度"——`should_run_aging()`

这是理解"何时扫描/回收"的关键，直接给出源码 ——`mm/vmscan.c:4648`：

```c
static bool should_run_aging(struct lruvec *lruvec, unsigned long max_seq,
                 struct scan_control *sc, bool can_swap, unsigned long *nr_to_scan)
{
    ...
    /* 完全没有冷页：必须先 aging（nr_to_scan=0 表示"别驱逐，去 aging"） */
    if (min_seq[!can_swap] + MIN_NR_GENS > max_seq) {
        *nr_to_scan = 0;
        return true;
    }
    /* 统计 total/young/old 三类页数 ... */

    *nr_to_scan = total >> sc->priority;          /* 本轮可驱逐量 = 总量 >> 优先级 */

    /*
     * aging 尽量"懒"以降开销；eviction 在代数到 MIN_NR_GENS 时会 stall。
     * 因此理想代数是 MIN_NR_GENS+1。
     */
    if (min_seq[!can_swap] + MIN_NR_GENS < max_seq)
        return false;                              /* 代数够，不必 aging */

    /* 也希望各代页数均匀（每代约 1/(MIN_NR_GENS+1)） */
    if (young * MIN_NR_GENS > total)              /* 热页占比过高 → aging */
        return true;
    if (old * (MIN_NR_GENS + 2) < total)         /* 冷页占比过低 → aging */
        return true;
    return false;
}
```

配合 `get_nr_to_scan()`（`:4720`）的关键决策：

```c
    if (mem_cgroup_below_min(...)) return -1;                 /* 受 memcg.min 保护，跳过 */
    if (!should_run_aging(...&nr_to_scan)) return nr_to_scan; /* 直接进入 eviction */
    if (sc->priority == DEF_PRIORITY) return nr_to_scan;      /* 默认优先级"懒 aging" */
    return try_to_inc_max_seq(...) ? -1 : 0;                  /* 压力大才真正 aging */
```

> 工程含义：**默认优先级（`DEF_PRIORITY=12`）下回避主动 aging**，避免轻压力时
> 反复扫页表；只有回收推进缓慢、优先级被抬高（priority 递减）时，才付出 aging
> （页表遍历）的代价。这把"扫描开销"与"内存压力"对齐——见 §5。

### 4.5 两条反馈回路

**(a) 空间局部性反馈：`lru_gen_look_around()`（rmap→页表）**——`mm/vmscan.c:4020`。
eviction 在 `shrink_folio_list()` 走 rmap 反查映射时，对命中 young PTE 的页，
顺手扫描其 **同一 PMD 内相邻的 `MIN_LRU_BATCH` 个 PTE**，把其中的热页一并晋升；
若这次扫描"缓存行高效"，就把该 PMD 记进 Bloom filter，供后续 aging 的页表遍历
快速定位"值得扫的 PMD"：

```c
    start = max(addr & PMD_MASK, vma->vm_start);
    end   = min(addr | ~PMD_MASK, vma->vm_end - 1) + 1;
    ... /* 把窗口裁到 MIN_LRU_BATCH 个页以内，保证"有界" */
    for (i = 0, addr = start; addr != end; i++, addr += PAGE_SIZE) {
        pte_t ptent = ptep_get(pte + i);
        pfn = get_pte_pfn(ptent, vma, addr);
        if (pfn == -1 || !pte_young(ptent)) continue;     /* 只看 young */
        ...
        ptep_test_and_clear_young(vma, addr, pte + i);     /* 清 accessed */
        young++;
        if (walk) { old_gen = folio_update_gen(folio, new_gen); ... }  /* 批量晋升 */
        ...
    }
    /* feedback from rmap walkers to page table walkers */
    if (suitable_to_scan(i, young))
        update_bloom_filter(lruvec, max_seq, pvmw->pmd);   /* :4120 反哺 aging */
```

这条回路让 eviction 与 aging 互相"喂数据"：eviction 顺路发现的热页直接晋升、
并标记"这片 PMD 值得 aging 细扫"，避免 aging 盲扫整张页表。

**(b) refault 反馈（仿 PID 控制器）**：被驱逐页若很快 refault，说明判冷判错。
`lru_gen_refault()`（`mm/workingset.c:280`）在缺页激活时按 shadow entry 还原
tier，原子累加 `lrugen->refaulted[hist][type][tier]`。下一轮 `read_ctrl_pos()`
（`:3052`）/`positive_ctrl_err()`（`:3105`）据此在 anon/file 之间、在 tier 之间
重新分配驱逐压力，`get_swappiness()`（`:2645`）作为增益。这相当于以 refault 率为
反馈量、以驱逐选择为控制量的闭环。

---

## 5. 规格评估：触发时机 / 频率 / 单轮开销

### 5.1 触发时机（三类）

| 触发源 | 路径 | 时机 |
|---|---|---|
| 后台回收 | `kswapd()`→`balance_pgdat()`→`lru_gen_age_node()`/`shrink_node` | 某 zone free 跌破 `WMARK_HIGH`（`wakeup_kswapd()` `:7217`） |
| 直接回收 | 分配慢路径→`shrink_zones()`→`shrink_node()`→`lru_gen_shrink_node()` | 分配跌破 `WMARK_MIN` 且后台来不及 |
| 主动 | debugfs `run_aging()`/`run_eviction()`（`:5392`/`:5412`） | 用户态按需采样/回收 |

`min_ttl`（§2.3）是一个 **工作集保护阈值**：`lru_gen_age_node()` 若发现所有 memcg
的每一代都比 `min_ttl` 年轻（即近期工作集都还"烫"），宁可走 OOM 也不驱逐——
用于抵御 thrash（默认 0=关闭）。

### 5.2 不同水线下的"频率/强度"

MGLRU 不自定义触发频率，而是 **复用 zone 水线 + kswapd 调度**，因此频率特性与
legacy 一致，差异在"每次被唤醒后做多少"：

- **kswapd 唤醒**：`wakeup_kswapd()`（`:7217`）在 free<`WMARK_HIGH` 时唤醒；
  `pgdat_balanced()`（`:6629`）以"任一合格 zone≥`high_wmark`"为均衡判据决定退出。
- **回收目标**：`kswapd_shrink_node()`（`:6720`）按各 zone `high_wmark_pages +
  SWAP_CLUSTER_MAX` 设定 `sc->nr_to_reclaim`。
- **强度随优先级放大**：单轮可驱逐量 `nr_to_scan = total >> sc->priority`
  （`should_run_aging` `:4690`）。压力越大、`sc->priority` 越小（从 `DEF_PRIORITY=12`
  递减到 0），单轮扫描/驱逐量按 2 的幂放大；同时只有在 priority<DEF_PRIORITY 时才
  会触发主动 aging（§4.4）。
- **abort 条件**：`should_abort_scan()`（`:4740`）在 root 回收达标
  （`nr_reclaimed ≥ max(nr_to_reclaim, compact_gap)`）或所有合格 zone 已安全时尽早收手。

> 直观总结：**水线高（轻压力）→ 仅"懒 aging"+小批驱逐**；**水线低（重压力）→
> priority 递减 → 触发页表 aging + 指数放大驱逐量**，直到达标或 abort。

### 5.3 单轮开销（与 legacy 对比的定性分析）

把开销拆成"aging（产生代）"与"eviction（驱逐）"两部分：

- **Eviction 开销**：`scan_folios()`→`sort_folio()` 是 **O(被扫页数)** 的链表遍历，
  代内/层间移动只做 `folio->flags` 原子操作，**不持 LRU 锁做重活**；批量受
  `MAX_LRU_BATCH` 限制。与 legacy 的 `shrink_*_list` 同量级，但少了 active↔inactive
  的来回搬运与 refcount 抖动。
- **Aging 开销**：核心是页表遍历 `walk_pmd_range`/`walk_pte_range`。三处把它压下来：
  1. **按需**：默认优先级不 aging（§4.4），轻压力下开销≈0。
  2. **有界回扫**：`look_around` 只扫一个 PMD 内 `MIN_LRU_BATCH` 个 PTE
     （`:4054` 把窗口裁界），是 **常数级** 而非"整地址空间"。
  3. **Bloom filter 剪枝**：`test_bloom_filter()`（`:2719`）让 aging 只细扫
     "曾被 look_around 标记过有 young 的 PMD"，跳过大片冷页表。
  4. **批量清 young**：`LRU_GEN_MM_WALK`/`LRU_GEN_NONLEAF_YOUNG` 允许批量、含非叶
     PMD 的 young 清理，减少逐 PTE 的原子开销。
- **元数据内存开销**：每 lruvec 增加 `lru_gen_folio`（gen×type×zone 的链表与计数 +
  PID 统计）与 `lru_gen_mm_state`（含 2×32Kbit Bloom filter）。`CONFIG_LRU_GEN_STATS`
  会额外保留历史代统计（`NR_HIST_GENS`）。

> 一句话：MGLRU 把"扫描成本"从"正比于地址空间"改成"正比于回收压力 × 候选页"，
> 并用 Bloom filter + look_around 把页表遍历限制在"有理由扫"的小范围。

---

## 6. 整机 / 多 NUMA node 视角

### 6.1 每 node 独立的线程与数据结构

- **每个 NUMA node 一个 `kswapd`**：`kswapd(void *p)`（`:7131`）里 `p` 就是一个
  `pg_data_t *pgdat`（`:7135`），线程绑定到该 node，循环调
  `balance_pgdat(pgdat,...)`（`:7199`）。回收是 **per-pgdat** 的。
- **每 node 一份 memcg LRU**：`pgdat->memcg_lru`（`struct lru_gen_memcg`，§3.3）。
- **每 (memcg,node) 一份 `lru_gen_folio`/`mm_state`**：即 gen/tier/Bloom 状态是
  **node 局部** 的，天然 NUMA 友好——一个 node 的 aging/eviction 不触碰另一个 node
  的链表与计数。

### 6.2 多 node 之间如何"均衡"

关键在于：**node 间的均衡不归 MGLRU 管，而是分配器/回收框架的既有职责**，MGLRU
只在被选定的 node 内部干活。具体三条线：

1. **后台**：哪个 node 缺页就唤醒哪个 node 的 kswapd（`wakeup_kswapd()` `:7217`
   按 `zone->zone_pgdat` 定位）。各 node kswapd 独立运行、互不阻塞，是"按 node 自治"
   的均衡。
2. **直接回收**：`shrink_zones()`（`:6108`）沿 **zonelist 顺序** 遍历，按 node 去重
   （`:6162`/`:6184`）后逐 node 调 `shrink_node()`（`:6187`）。zonelist 顺序本身编码
   了 NUMA 距离/`node_reclaim` 策略——即"先近后远"的节点偏好。
3. **node 局部回收**：`node_reclaim()`（`:7463`）在 `node_reclaim_mode` 下，分配优先
   就地回收本 node 而非立刻跨 node 取页。

> 因此从整机看：MGLRU = "全局水线/zonelist 框架（决定**在哪个 node 回收、回收多少**）"
> ＋ "每 node 内的 MGLRU 策略（决定**在该 node 扫哪些页、驱逐哪些页**）"。
> node 间均衡仍由 watermark + zonelist + `node_reclaim_mode` 这套既有机制承担，
> MGLRU 没有、也不需要引入跨 node 的全局协调者。

### 6.3 全局回收时的 memcg 遍历：近似 O(1)

当一个 node 上有大量 memcg 时，legacy 的 `shrink_node_memcgs()` 要遍历全部 memcg。
MGLRU 用 per-node memcg LRU 把它变成 **对若干 bin 的有界遍历**：

`shrink_many()`（`mm/vmscan.c:4856`）核心：

```c
    gen = get_memcg_gen(READ_ONCE(pgdat->memcg_lru.seq));   /* 当前老代 */
    bin = first_bin = get_random_u32_below(MEMCG_NR_BINS);   /* 随机起始 bin，摊平热点 */
restart:
    hlist_nulls_for_each_entry_rcu(lrugen, pos, &pgdat->memcg_lru.fifo[gen][bin], list) {
        ...
        op = shrink_one(lruvec, sc);                         /* :4898 回收一个 memcg */
        if (should_abort_scan(lruvec, sc)) break;            /* 达标即止 */
    }
    if (op) lru_gen_rotate_memcg(lruvec, op);                /* 按回收结果轮转代/bin */
    ...
    bin = get_memcg_bin(bin + 1);                            /* 轮转下一个 bin */
    if (bin != first_bin) goto restart;
```

- memcg 以 `hlist_nulls` 挂在 `[gen][bin]`，`MEMCG_NR_GENS=3` 区分 old/young/读保护，
  `MEMCG_NR_BINS=8` 随机分桶。
- 回收完依据结果用 `lru_gen_rotate_memcg()`（`:4143`）把 memcg 在代/bin 间轮转；
  最老代清空时推进 `pgdat->memcg_lru.seq`（`:4184`）。
- online/offline/release：`lru_gen_online_memcg()` `:4189` / `lru_gen_offline_memcg()`
  `:4214` / `lru_gen_release_memcg()` `:4225`。
- **复杂度**：每轮全局回收最多走 O(`MEMCG_NR_BINS`)=O(8) 个桶，且达标即 abort，
  把"遍历所有 memcg"的最坏 O(M) 降到与 M 无关的近似 O(1)。这是 MGLRU 在容器密集
  场景相对 legacy 的关键整机收益。

---

## 7. 与 smaps 扫描的对比及借鉴意义

> 说明：本仓库的稀疏内核检出 **不含 `fs/proc/task_mmu.c`**（sparse 列表仅
> `drivers/ub/`、`include/`、`mm/`、`kernel/sched/`、`Documentation/ub/` 等，
> 见 `CLAUDE.md`）。本节 smaps 侧基于通用 Linux 实现的机制描述，不带本仓 `file:line`；
> 若需精确行号，应按 `CLAUDE.md` 的流程刷新检出以纳入 `fs/`。

### 7.1 smaps 扫描的机制与成本

`/proc/<pid>/smaps`、`smaps_rollup` 经 `show_smap()`/`smap_gather_stats()` 用
`walk_page_vma()` + `smaps_pte_range()`（`smaps_walk_ops`）**正向** 遍历目标进程的
**每个 VMA、每个 PTE**，统计 Rss/Pss/Referenced/Anonymous/Swap 等；以 `pte_young()`
判访问、`pte_dirty()` 判脏；`/proc/<pid>/clear_refs` 用来清 young 位以便下次采样。
特征：

- **方向**：从 **pid/task 侧正向** 遍历虚拟地址空间。
- **粒度/锁**：每次读持 `mmap_read_lock()`，遍历该进程 **全部** PTE。
- **复杂度**：O(进程虚拟地址空间)；与是否有回收压力、页是否冷热 **无关**。
- **模型**：用户态 **拉取（pull）**，按需、全量、与回收解耦。

### 7.2 二者的本质差异

| 维度 | smaps 正向扫描 | MGLRU |
|---|---|---|
| 起点 | pid → VMA → PTE（虚拟地址空间） | folio（物理页）/ lruvec |
| 触发 | 用户态读，与压力无关 | 回收压力驱动（水线/优先级），按需 |
| 范围 | 整进程地址空间，全量 | 候选页 + Bloom 剪枝 + look_around 有界回扫 |
| 锁 | `mmap_read_lock` 全程 | 代/层免锁原子位；页表回扫窗口极小 |
| 复杂度 | O(地址空间) | O(压力 × 候选页)，memcg 遍历近似 O(1) |
| 反馈 | 无（一次性快照） | look_around + refault 双闭环 |

### 7.3 对 smaps（及类 smaps 冷热采样）的借鉴意义

如果要在 smaps 一类"页面冷热/工作集采样"上借鉴 MGLRU，可提炼为：

1. **有界、增量代替全量**：smaps 每次全量 walk 整个地址空间昂贵。可仿
   `look_around` 的"PMD 内有界窗口 + Bloom filter 剪枝"，只对"近期出现过 young 的
   片区"增量采样，把开销从 O(地址空间) 降到 O(热点片区)。
2. **把冷热状态固化到页上、按代分桶**：smaps 每次都要重算；MGLRU 把 gen/tier 编码进
   `folio->flags`，采样结果是 **可累积的状态** 而非一次性快照。冷热采样可借鉴"分代/
   分桶 + 滑动平均"来获得稳定的工作集估计（正是 debugfs `lru_gen` working-set
   estimation 的做法）。
3. **以物理页为中心 + rmap 反查**：smaps 锁 `mmap_lock` 正向遍历，多进程共享页会被
   重复统计且互相阻塞。以 folio 为中心、需要时再用 rmap 反查映射，天然对共享页/多
   进程更省、锁竞争更小。
4. **采样与回收解耦但共享元数据**：MGLRU 的 debugfs `run_aging`/`run_eviction` 既能
   纯采样也能驱动回收，复用同一套代/层元数据。类 smaps 工具可据此做"低开销采样 →
   按需触发主动回收"的闭环，而不是"采样归采样、回收归回收"。

---

## 8. 设计演进：为何"从 struct page 反向定温"而非"按 pid 正向全扫"

这是 issue 第 5 点，也是最值得展开的"高视角"问题。可从四个角度回答"为什么演进成
今天这样"。

### 8.1 成本必须与收益对齐——正向全扫做不到

回收的目标函数是"以最小 CPU/锁成本腾出最多可回收页"。**按 pid 正向遍历所有进程的
全部虚拟地址空间**有几个结构性缺陷：

- **开销正比于地址空间，而非压力**：稀疏大地址空间（典型 64 位进程、大量 mmap、
  保留但未触碰区）会让正向扫描做大量无用功；而回收真正关心的是"有多少**物理页**
  可腾出"。MGLRU 的 `nr_to_scan = total >> priority`、默认优先级不 aging、Bloom 剪枝
  都是在把成本钉到"压力 × 候选物理页"上（§5）。
- **共享页重复计数**：一页被 N 个进程映射，正向扫描会在 N 个地址空间里各遇到一次；
  以页为中心 + rmap 只需按需反查，避免重复。
- **锁与并发**：正向扫描要持 `mmap_lock`，与缺页/`mmap`/`munmap` 争锁；进程越多越
  糟。MGLRU 代/层移动免 LRU 锁，页表回扫窗口被裁到一个 PMD 内的常数页。

### 8.2 回收的天然索引是物理页，不是虚拟地址

回收最终要操作的是 `struct page`/folio（取消映射、回写、释放）。把冷热判定也建立在
folio 上（gen/tier 编码进 `folio->flags`，§1/§3.1），使得"判定"与"动作"在同一索引域，
无需从虚拟地址再映射回物理页。换言之：**回收是"物理页问题"，就该用物理页做主键。**

### 8.3 但纯物理侧信息不足——所以"反向定温"要借一点正向

只看 `PG_referenced`/`PG_active` 两个 bit 的 active/inactive 模型，信息太粗，容易误判
（典型 file/anon 失衡、流式 I/O 冲刷工作集）。MGLRU 的折中很关键：

- **主键仍是物理页**（代/层），但允许在 **回收压力下、对候选页** 借 rmap 做 **有界**
  的页表回扫（`look_around`），并用 Bloom filter 把"值得正向细扫的 PMD"标出来。
- 这是"以反向（物理页）为主、正向（页表）为辅且有界"的混合：既拿到 PTE young 这种
  只有正向才有的细粒度访问信息，又不付"全地址空间正向扫"的代价。

### 8.4 用反馈闭环替代"精确但昂贵"的扫描

与其追求一次扫描就精确判冷热（昂贵），不如 **先低成本判断、再用 refault 反馈纠偏**：

- `look_around` 把 eviction 顺路发现的热页晋升、并反哺 aging（空间局部性闭环）。
- `lru_gen_refault` + 仿 PID 控制器（`read_ctrl_pos`/`positive_ctrl_err`/swappiness）
  以 refault 率为反馈，在 anon/file 与各 tier 间动态再分配驱逐压力（§4.5b）。

控制论视角：**用闭环反馈把"判定误差"摊到多轮里收敛**，比"单轮开环精确判定"在工程上
更省、更稳。这正是 MGLRU 区别于"周期性全量正向扫描"的根本设计哲学。

### 8.5 小结

> 演进的方向可以概括为：**主键从虚拟地址迁到物理页**（回收的天然索引）；**扫描从
> 全量正向改为压力驱动、有界、按需的混合**（Bloom + look_around 借正向之长而避其短）；
> **判定从单轮开环改为多轮反馈闭环**（look_around 空间局部性 + refault 仿 PID）。
> 三者共同把"扫描成本"与"回收收益/内存压力"对齐——这就是 MGLRU 长成今天样子的原因。

---

## 9. 参考（本仓内可读）

- 源码：`mm/vmscan.c`、`include/linux/mmzone.h`、`mm/workingset.c`、
  `include/linux/mm_inline.h`、`mm/Kconfig`。
- 官方文档：`Documentation/mm/multigen_lru.rst`（设计）、
  `Documentation/admin-guide/mm/multigen_lru.rst`（运维/接口）。
- 不在稀疏检出内（需刷新才能精确引用）：`fs/proc/task_mmu.c`（smaps）。

> 范围声明：本次为纯内核源码**调研**，仅在 ubturbo 仓库内新增/更新本调研文档，
> 未改动 ubturbo 其他源码，也未向只读的 kernel 参考写入任何内容（符合 `CLAUDE.md`
> 的 scope 约束）。
