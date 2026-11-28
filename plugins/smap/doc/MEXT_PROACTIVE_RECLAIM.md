# SMAP-MEXT Dynamic Proactive Reclaim Architecture & Developer Guide

## 1. Motivation: Breaking the Static Linux Watermark Dilemma

Linux memory reclaim (`mm/vmscan.c` & `mm/page_alloc.c`) relies on static per-zone watermarks (`WMARK_MIN`, `WMARK_LOW`, `WMARK_HIGH` controlled by `vm.min_free_kbytes` and `vm.watermark_scale_factor`):

1. **Low Static Watermarks (Linux Default, e.g., `WMARK_LOW = 80 MB`):** Maximizes steady-state DRAM capacity (`~99.2%`), **but** under sudden multi-GB/s allocation bursts (LLM KV-cache growth, Spark shuffle, compiler link spikes), free memory crosses the `16 MB` gap (`WMARK_LOW -> WMARK_MIN`) in `< 4 ms` while `kswapd` is still ramping scan priority from `DEF_PRIORITY (12)`. Allocating threads stall synchronously inside `__alloc_pages_slowpath -> try_to_free_pages()` (**Direct Reclaim**), spiking P99.5 allocation latency from `0.18 µs` to `715–7,560 µs`.
2. **High Static Watermarks (`vm.watermark_scale_factor = 900..1000`, e.g., `WMARK_LOW = 1.5 GB`):** Absorbs bursts, **but** permanently strands `11–18%` of expensive host DRAM idle 24/7 even when allocation velocity is `0 MB/s`.

**SMAP Dynamic Proactive Reclaim** eliminates both failure modes by pairing **early kernel pressure wakeups + disturbance-observer velocity/acceleration feedforward** with **MEXT predictive prefetching** as a closed-loop safety net.

---

## 2. Architecture & Control Loop

### A. Kernel Early-Pressure Signal (`mext_ring_abi.h` + `mext_fault_ring.c`)
- Extends the 16,384-slot lockless `vmalloc_user` `mmap` ring with `MEXT_EVT_MEM_PRESSURE = 3` (preserving exact 64B header and 32B event binary compatibility).
- Attaches a rate-limited `kprobe` on `wakeup_kswapd` (max 1 event/ms per NUMA node) emitting `(nid, zone_free_pages, wmark_low_pages, order)`.
- `MextEngine::ProcessRingOnce()` forwards `MEXT_EVT_MEM_PRESSURE` via `PressureCallback` to wake `ProactiveReclaimController` in **~110–140 µs**—well before threads reach `__alloc_pages_slowpath`.

### B. Disturbance-Observer Demand Velocity & Dynamic Watermark (`proactive_reclaim.cpp`)
- **Fast Telemetry Reader:** Parses `/proc/meminfo`, `/proc/zoneinfo`, `/proc/vmstat`, and `/proc/pressure/memory` every `10–15 ms` (or immediately on ring/SDK wakeup).
- **Self-Reclaim Compensation (Zero Bang-Bang Hunting):** Reconstructs true uncompensated memory consumption velocity by subtracting pages reclaimed by SMAP (`R_self`) and kernel steals (`pgsteal_*`) during `[t_{k-1}, t_k]`:
  ```text
  A_net(t) = max(0, -( (Free_k - Free_{k-1}) - (R_self + d_pgsteal) ) / dt)
  ```
  An asymmetric EWMA filter (`alpha_up = 0.65` fast attack, `alpha_down = 0.22` smooth release) tracks velocity `A(t)` and acceleration `dA/dt` without bang-bang hunting while `Free` is held flat.
- **Dynamic Lookahead Headroom (`W_dyn`):**
  ```text
  W_dyn(t) = clamp(W_base + alpha * A(t) * tau + 0.5 * beta * max(0, dA/dt) * tau^2, W_base, W_max)
  ```
  where `tau = 80 ms` on NVMe swap and `tau = 35 ms` on synchronous `zram`.
- **4-Regime Urgency & Cold-Age Quantile Stepping:**
  - `RECLAIM_CALM` (`Free >= W_dyn`): `cold_window = 8`, `0` proactive eviction → `~99.2%` usable DRAM.
  - `RECLAIM_PROACTIVE` (`Free < W_dyn`, `TTE > 160 ms`): `cold_window = 5`, gentle proportional drain.
  - `RECLAIM_BURST` (`TTE <= 160 ms` or ring wakeup): `cold_window = 2`, feedforward `A(t)*dt + deficit` batching.
  - `RECLAIM_EMERGENCY` (`Free <= W_low` or `TTE <= 40 ms`): `cold_window = 1`, immediate deficit recovery.

### C. Closed-Loop MEXT Thrash Brake
- Queries `MextEngine::GetStatsSnapshot()` for live swap-fault velocity and `hit_rate`:
  ```text
  uncompensated_fault_pps = fault_rate_pps * (1.0 - mext_hit_rate)
  ```
- When MEXT hides swap-ins (`hit_rate > 85%`), proactive reclaim runs at full gain (`brake_scale = 1.0`).
- Under true random thrashing (`hit_rate ~ 0%` or `PSI full > 15%`) while `Free > W_low`, `brake_scale` smoothly damps eviction (`[0.25 .. 1.0]`) and raises `cold_window >= 3` to protect active working sets.

### D. High-Concurrency Sandbox Fleet Optimizations (`1,200+` Concurrent VMs)
- **8,192-Slot Double-Buffered Kernel PID Filter (`mext_fault_ring.c`):** Supports up to `6,144` concurrent tracked sandboxes (`<75%` load factor) with lockless wait-free `O(1)` kprobe lookups and automatic tombstone (`-1`) compaction across rapid sandbox churn.
- **Cgroup-Evicted Fault Prefetch & Non-Blocking NVMe Dispatch (`mext_engine.cpp`, `mext_fault_ring.c`):** Per-PID shadow completeness gating ensures MEXT predicts and prefetches pages evicted by Linux cgroup `memory.max` / `kswapd` (not only `SMAP_MIG_SWAP_OUT`), while NVMe backends submit all candidates via non-blocking `process_madvise(MADV_WILLNEED)` + `MEXT_PREFAULT_FLAG_NOWAIT` (`FOLL_NOWAIT` + `mmap_read_trylock`) so `MextEngine` never blocks on disk I/O or contends on `mmap_write_lock` during sandbox boot.
- **Zero-Config Auto Per-PID Cgroup v1 & v2 Discovery & Targeted Reclaim (`ProactiveReclaimTrackPid` / `ResolveEffectiveCgroupDir`):** Automatically resolves `/proc/<pid>/cgroup` across unified **Cgroup v2** (`0::` -> `memory.max`, `memory.current`, `memory.events`), legacy **Cgroup v1** (`<id>:...,memory,...:` -> `memory.limit_in_bytes`, `memory.usage_in_bytes`, `memory.failcnt`), and hybrid v1+v2 hosts, walking upward to bind each sandbox to its tightest finite ancestor (`SMAP_RECLAIM_CGROUP` remains available as a manual override). Uses a two-tier hot/cool sampler, skips `ColdTracker` aging for calm sandboxes (`<70%` full), and weights `ExecuteProactiveSwapBatch()` via `ProactiveReclaimGetPidPressureScore(pid)`.
- **`ap_data`-Independent Out-of-Cycle Cold Pageout & External Swap Safety (`MEXT_PREFAULT_FLAG_PAGEOUT_COLD`):** Out-of-cycle bursts reclaim cold anonymous pages directly in `smap_tiering.ko` via `walk_page_range` + `fp_reclaim_pages` with a roving per-PID VMA cursor and `mmap_read_trylock` (emitting `MEXT_EVT_EVICTED` to `SwapShadowMap` without touching `smap_tracking.ko`'s `AP_STATE_MIG` state machine). `SwapDeviceInit` / `SwapDeviceShutdown` also preserve pre-mounted `/proc/swaps` ownership (`activated_by_smap`) and leave `vm.swappiness` intact.
- **64-Slot `pidfd` Ring Cap (`MextEngine::EnsurePidfd`):** Bounds open `pidfd`s to at most `64` descriptors (and raises soft `RLIMIT_NOFILE` on Linux) so `1,200+` `no-lifecycle` sandboxes never hit `EMFILE` (`ulimit -n = 1024`).
- **Standalone Zero-SMAP Mode & Firecracker KVM Optimization (`plugins/smap/standalone_mext/`):** Provides standalone `mext_drv.ko` (`/dev/mext_ctl`, 65,536-slot 2 MiB lockless ring), `mextd` daemon, and `mext_train` offline real-trace trainer (`--dump-trace` -> `mext_tiny_tf.bin`). Tailored for Firecracker KVM Guest-RAM VMAs (`4 GiB` sparse GPA + `/dev/ublkb2` `MAP_PRIVATE` CoW snapshots): pairs **Synergistic Hybrid NVMe Readahead** (`vm.page-cluster=2` 4-page `t=0 ns` in-kernel `swap_vma_readahead` + non-overlapping MEXT `+5..+64` lookahead, non-unit strides, and Co-Eviction), captures kernel cgroup LRU swap-outs via `try_to_unmap_one` with automatic cross-TU `page_vma_mapped_walk` fallback into `SwapShadowMap`, strips dual-vCPU `do_swap_page` `VM_FAULT_RETRY` duplicates, adapts backbone stride under `>100K` events/s 64-core bursts, verifies `is_swap_pte` before GUP, and enforces an 8-second minimum **KVM Stage-2** wrap-around aging generation (`MEXT_PAGEOUT_MIN_WRAP_NS`). On unthrottled Firecracker NVMe clusters (`run-20261127-144731` / `run-20261128-112813`), standalone MEXT reduces SwapIn IO by **18.5%** (`176.25 -> 143.65 GiB`), cgroup stalls by **11.1%**, `10s` timeouts by **9.0%**, and `30s` Command P90 latency by **21.2%** (`6.348 -> 5.003 s`) with **100%** task success.
- **Zero-Config JSON Telemetry (`/var/run/smap_mext_stats.json` or `$SMAP_MEXT_STATS_PATH`):** Periodically exports `total_prefetched_pages`, `hit_rate`, `coverage_rate`, and `faults_processed` for external harnesses (`smap-test`).
- **Abundant-RAM `O(1)` Fast Bypass (`ProactiveReclaimShouldTrackCold`):** When host/cgroup free memory exceeds `max(4 * W_dyn, 10% * TotalRAM)`, periodic scan cycles bypass `manager->lock`, `ColdTracker` allocations, `/proc/<pid>/status` syscalls, and swap-out I/O completely; even inside the `<=10%` pre-warm band, `RECLAIM_CALM` enforces `0` swap-outs until `Free < W_dyn`.
- **Demand-Grown Per-PID Eviction Log (`SwapShadowMap`):** Allocates `0 B` for non-swapping PIDs and starts active swap logs at `1,024` entries (`32 KB`), doubling on demand up to `65,536`.

---

## 3. Public C / SDK APIs (`smap_interface.h`)

```c
/* Auto-starts both MEXT Prefetch and Dynamic Proactive Reclaim (NVMe or zram preset) */
int ubturbo_smap_swap_config(const char *device_path, int priority, bool enable);

/* Enable or disable Dynamic Proactive Reclaim at runtime */
int ubturbo_smap_proactive_reclaim_enable(bool enable);

/* Customize poll interval, lookahead horizon tau, velocity/accel gains, or PSI brake */
int ubturbo_smap_reclaim_config(const ProactiveReclaimConfig *cfg);

/* Query live dynamic watermark W_dyn, velocity (MB/s), TTE (ms), urgency, and brake scale */
int ubturbo_smap_reclaim_query_stats(ProactiveReclaimStats *stats_out);
```

---

## 4. Benchmark Summary (`16 GB` Host, `4.0–5.5 GB/s` Allocation Bursts)

| Swap Backend | Policy | Direct Reclaim Stalls (`allocstall`) | P99.5 Alloc Latency | Calm Usable DRAM | Warm-Cold Re-access (`2048` pages) |
|---|---|---:|---:|---:|---:|
| **NVMe Gen4** | Linux Static Low (`80 MB`) | `6,426` | `7,560.18 µs` | `99.22%` | `174.08 ms` |
| **NVMe Gen4** | Linux Static Scale (`1.5 GB`) | `0` | `0.18 µs` | `88.55%` | `174.08 ms` |
| **NVMe Gen4** | **SMAP Proactive + MEXT** | **`0`** | **`0.18 µs`** | **`99.22%`** | **`1.06 ms` (`164x` faster)** |
| **`zram` DRAM** | Linux Static Low (`80 MB`) | `236` | `715.18 µs` | `99.22%` | `11.26 ms` |
| **`zram` DRAM** | Linux Static Scale (`1.5 GB`) | `0` | `0.18 µs` | `88.55%` | `11.26 ms` |
| **`zram` DRAM** | **SMAP Proactive + MEXT** | **`0`** | **`0.18 µs`** | **`99.22%`** | **`0.33 ms` (`34x` faster)** |

Run all 4 verification & benchmark suites on any Linux or macOS machine:
```bash
git clone -b feat/mext-proactive-reclaim https://github.com/rainbay001-dotcom/ubturbo.git
cd ubturbo
bash plugins/smap/test/predictor/run_mext_tests.sh
```
