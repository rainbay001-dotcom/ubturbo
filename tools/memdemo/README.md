# memdemo — SMAP 扫描准确性度量工具

`memdemo` 是一个 **真值（ground-truth）生成器**：按指定方式申请内存、按指定模型访问，
并输出"哪些页是热/冷"的已知真值（二进制位图）。把这份真值与 SMAP 的扫描结果做差异比较，
就能量化扫描的准确率（precision / recall）。

本工具为 **独立构建**，不参与 ubturbo 主程序的编译，互不影响。

---

## 功能对照（issue 需求）

| 需求 | 实现 |
|---|---|
| 1. 指定 4K / 2M 页面 | `--page-size 4k\|2m`（2M 走 `MAP_HUGETLB\|MAP_HUGE_2MB`） |
| 2. 普通进程 / 模拟虚机 | `--mode process\|vm` |
| 3. 虚机：KVM 注册 memslot 以通过 SMAP 虚机检测 | `--mode vm`：`/dev/kvm` → `KVM_CREATE_VM` → `KVM_SET_USER_MEMORY_REGION`，进程存活期间保持 VM fd 打开 |
| 4. 访存模型 | `--pattern uniform\|random\|gaussian\|zipf` |

目标架构：**arm64 (aarch64)**，与 SMAP 的 `kvm_pgtable` stage-2 代码路径一致。

---

## 构建

```bash
cmake -S tools/memdemo -B tools/memdemo/build
cmake --build tools/memdemo/build
# 产物：tools/memdemo/build/memdemo
```

---

## 用法

```text
memdemo [options]

  --page-size <4k|2m>     页面粒度（默认 4k）
  --mode <process|vm>     普通进程 或 KVM 虚机（默认 process）
  --pattern <name>        uniform | random | gaussian | zipf（默认 uniform）
  --size <MB>             区域总大小，单位 MiB（默认 8，最小 2）
  --hot-ratio <0..1>      热区占比（默认 0.1）
  --iterations <n>        访问次数（默认 100 × 页数）
  --duration <sec>        按时长运行（与 --iterations 二选一，时长优先）
  --seed <n>              随机种子，保证可复现（默认 1）
  --hot-threshold <n>     访问次数 >= n 即判为热页（默认取均值）
  --gpa-base <bytes>      虚机 memslot 的 GPA 基址（默认 4 GiB）
  --out <path>            真值位图输出文件（默认 memdemo.bitmap）
  --help                  显示帮助
```

### 示例

```bash
# 普通进程，4K 页，高斯热点
./memdemo --page-size 4k --mode process --pattern gaussian \
          --size 64 --hot-ratio 0.05 --out gt.bitmap

# 模拟虚机，2M 大页，zipf 热点，GPA 置于 4GiB
./memdemo --page-size 2m --mode vm --pattern zipf \
          --size 128 --gpa-base 0x100000000 --out gt.bitmap
```

---

## SMAP 虚机检测的硬约束（已在工具内满足）

来自 `plugins/smap/src/drivers/access_pid.c`：

- 进程必须持有一个 **`/dev/kvm` 派生的 VM fd**（`get_kvm_file_from_task` 查找名为 `kvm-vm` 的 fd）。
- `scan_kvm_gfn()` 过滤 memslot：
  - `npages >= (1 << HUGE_TO_4K_SHIFT)` → memslot **至少 2 MiB**；
  - `base_gfn >= (1 << GB_TO_4K_SHIFT)` → GPA **必须 ≥ 1 GiB**（默认 `--gpa-base` 为 4 GiB）。

因此 `--mode vm` 下 `--size` 不小于 2、`--gpa-base` 不小于 1 GiB 且页对齐；
参数解析阶段会做校验。

> 当前为 **轻量版（变体 A）**：注册 memslot + 保持 VM fd 即可通过 SMAP 的虚机识别；
> 访存在 host 侧对 `userspace_addr` 映射进行，**不创建 vCPU**。
> 若后续需要让 SMAP 在虚机大页路径上采到 stage-2 AF 位，可迭代为变体 B（最小 vCPU 让 guest 真正访存）。

---

## 运行前置条件

- **2M 大页**：需预留 hugepages，例如
  `echo <N> > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages`，通常需要 root。
- **虚机模式**：需要对 `/dev/kvm` 的读写权限。

---

## 真值位图格式

输出文件为小端二进制：定长文件头 + 紧凑位图。位图每页 1 bit，
bit `i`（字节内 **LSB 优先**）为 1 表示第 `i` 页为热页。

| 偏移 | 大小 | 字段 |
|---|---|---|
| 0  | 8 | magic `"MEMDEMOB"` |
| 8  | 4 | version（=1） |
| 12 | 4 | page_size（字节） |
| 16 | 8 | page_count（即 bit 数） |
| 24 | 8 | gpa_base（process 模式为 0） |
| 32 | 4 | pattern 枚举（0=uniform,1=random,2=gaussian,3=zipf） |
| 36 | 4 | mode 枚举（0=process,1=vm） |
| 40 | 8 | 实际使用的 hot_threshold |

文件头之后是 `ceil(page_count / 8)` 字节的位图数据。

### 解析示例（Python）

```python
import struct

with open("gt.bitmap", "rb") as f:
    hdr = f.read(48)
    magic, ver, psize, pcount, gpa, pat, mode, thr = struct.unpack(
        "<8sIIQQIIQ", hdr)
    assert magic == b"MEMDEMOB"
    bits = f.read()

def is_hot(i):
    return (bits[i >> 3] >> (i & 7)) & 1

hot = sum(is_hot(i) for i in range(pcount))
print(f"pages={pcount} hot={hot} page_size={psize} threshold={thr}")
```

把 `is_hot(i)` 与 SMAP 对同一 PID 报告的热页集合逐页比较，即可计算
precision / recall 等准确率指标。

---

## 访存模型说明

- **uniform**：轮询整片区域，每页命中次数相同 → 真值全热。
- **random**：在所有页上均匀随机访问。
- **gaussian**：以热区中心为均值的正态分布（`sigma = hot_pages / 6`）。
- **zipf**：Zipf(s≈1.07) 分布，少数低序号页占据绝大多数访问，最贴近真实热点倾斜。

热页判定：访问次数 `>= hot_threshold`（默认取全页均值，可用 `--hot-threshold` 覆盖）。
