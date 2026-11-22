# SMAP MEXT Predictive Memory Architecture & Developer Guide

## 1. Overview

**SMAP MEXT Predictive Memory** extends UBTurbo's `smap` plugin from reactive two-tier NUMA migration (`L1 DRAM <-> L2 CXL/UB`) into a **three-tier predictive memory engine (`L1 DRAM <-> L2 CXL/UB <-> L3 NVMe SSD`)**.

Unlike reactive Linux swap (`do_swap_page()` stalling CPU threads for ~20–80 µs per miss), SMAP MEXT combines:
1. **Per-page Cold Offload (`SMAP_MIG_SWAP_OUT`)**: Multi-window AF-bit/ACTC zero-frequency tracking (`ColdTracker`) paired with kernel `reclaim_pages()` (`do_swap_out`) and anon-folio virtual address resolution (`smap_folio_find_vaddr`) that emits eviction-order metadata (`MEXT_EVT_EVICTED`).
2. **Zero-Syscall Lockless Telemetry Ring (`mext_fault_ring.c`)**: `vmalloc_user()` shared memory ring `mmap`'d on `/dev/smap_mig_device`, populated by `do_swap_out()` and a `do_swap_page` `kprobe` (`MEXT_EVT_SWAP_FAULT`).
3. **Multi-Space Delta Representation (`SwapShadowMap` + `mext_tokenizer.h`)**: Encodes fault streams simultaneously in **Virtual Page Delta (`ΔVPN`)** and **Swap Eviction-Order Delta (`ΔEvictSeq`)**, collapsing arbitrary 64-bit heap pointer-chasing jumps into compact `+1..+64` eviction-order offsets when linked structures were swapped out together.
4. **Two-Tier Ensemble Predictor (`Tier1Table` + INT8 `TinyTransformer`)**:
   - **Tier 1 (`tier1_table.cpp`)**: Sub-microsecond per-VMA stride detector, decaying 2-slot Markov delta table, and co-eviction epoch run-length detector.
   - **Tier 2 (`tiny_transformer.cpp`)**: 2-layer causal INT8 Transformer (`d_model=64, H=2, d_ff=128 ReLU², L=32` circular RoPE KV cache, ~100 KB quantized backbone, **~4.5 µs/step** on 1 ARM64 core via NEON `vmull_s8` / `vpadalq_s16`) with live **FP32 output-head SGD adaptation (`OnlineAdaptHead`)** from delayed self-supervised fault horizons.
5. **Confidence-Gated Two-Level Promotion (`mext_engine.cpp`)**:
   - **Medium confidence (`p ∈ [0.25, 0.75)`)**: Batched `process_madvise(pidfd, iov, K, MADV_WILLNEED, 0)` triggering async NVMe reads into the Linux DRAM swap cache without prematurely mutating PTEs.
   - **High confidence (`p ≥ 0.75`)**: `ioctl(SMAP_MIG_PREFAULT)` (`get_user_pages_remote`) installing PTEs ahead of access for ~100 ns zero-fault hits, throttled dynamically by `/proc/pressure/memory` (PSI).

---

## 2. Source Layout

```text
plugins/smap/
├── src/
│   ├── tiering/
│   │   ├── mext_ring_abi.h          # Shared kernel/user lockless ring & ioctl ABI
│   │   ├── mext_fault_ring.h/.c     # vmalloc_user mmap ring, do_swap_page kprobe, SMAP_MIG_PREFAULT
│   │   ├── rmap.h/.c                # smap_folio_find_vaddr() anon folio VA lookup before reclaim
│   │   ├── mig_init.h/.c            # SMAP_MIG_SWAP_OUT + SMAP_MIG_PREFAULT + SMAP_MIG_MEXT_CTRL
│   │   └── smap_migrate_wrapper.h/.c# Resolves reclaim_pages / folio_isolate_lru / folio_putback_lru
│   └── user/
│       ├── manage/
│       │   ├── swap_types.h         # SwapPolicy, ColdTracker, MextConfig, MextStats
│       │   ├── swap_device.h/.c     # Safe swapon(2)/swapoff(2) + vm.page-cluster=0 tuning
│       │   └── swap_account.h/.c    # Per-PID /proc/[pid]/status VmSwap tracking
│       ├── strategy/
│       │   ├── cold_tracker.h/.c    # Dual-topology (L1+L3 and L1+L2+L3) cold window counters
│       │   └── swap_out.h/.c        # Budget-capped SMAP_MIG_SWAP_OUT executor
│       └── predictor/
│           ├── mext_tokenizer.h     # ΔVPN + ΔEvictSeq + VMA + Δt multi-space tokenizer
│           ├── swap_shadow_map.h/.cpp # FlatVpnHashTable + 64K circular eviction log per PID
│           ├── tier1_table.h/.cpp   # Per-VMA stride + Markov + co-eviction burst predictor
│           ├── tiny_transformer.h/.cpp # INT8 NEON 2-layer Transformer + AdamW trainer + online SGD
│           ├── mext_engine.h/.cpp   # Ring consumer, Noisy-OR ensemble, PSI gate, MADV_WILLNEED/PREFAULT
│           └── mext_predictor_c_api.h # Pure-C lifecycle & telemetry bridge for libsmap.so
├── tools/
│   └── mext_train_cli.cpp           # Standalone C++17 trace trainer & MXTF weight exporter
└── test/predictor/
    └── test_mext_predictor.cpp      # Unit, numerical gradient, concurrency & latency benchmark suite
scripts/
└── train_mext_predictor.py          # Offline trainer (PyTorch GPU/NPU + native C++17 backend)
```

---

## 3. Public C / SDK Interfaces (`smap_interface.h`)

```c
/* Enable NVMe backing swap device and auto-start MEXT predictive engine */
int ubturbo_smap_swap_config(const char *device_path, int priority, bool enable);

/* Set consecutive zero-frequency scan windows required before swap-out */
int ubturbo_smap_swap_set_threshold(uint32_t threshold);

/* Update MEXT thresholds, trigger stride, PSI limit, or hot-load custom MXTF weights */
int ubturbo_smap_mext_config(const MextConfig *cfg);

/* Query live MEXT telemetry ("RAM View": hit rate, avg_infer_us, prefetches, prefaults, PSI) */
int ubturbo_smap_mext_query_stats(MextStats *stats_out);
```

---

## 4. Quick Start for Teammates

### A. 10-Second Standalone Verification & Benchmarks (Any Linux or macOS Machine)
```bash
git clone -b feat/mext-predictive-memory https://github.com/rainbay001-dotcom/ubturbo.git
cd ubturbo
bash plugins/smap/test/predictor/run_mext_tests.sh
```
This compiles and runs all three suites automatically:
1. `test_mext_predictor` — 6-suite unit, numerical gradient, lockless ring concurrency & CPU latency verification.
2. `bench_mext_perf` — 50% DRAM + 50% NVMe event-driven timing benchmark across 4 workloads.
3. `test_mext_zram` — Live two-thread real `zlib` 4 KB page compression/decompression + `zram` offload benchmark.

### B. Full Kernel `.ko` + `libsmap.so` Deployment on openEuler OLK-6.6
```bash
# 1. Build kernel modules (requires openEuler kernel-devel headers)
make -C plugins/smap/src/drivers
make -C plugins/smap/src/tiering
sudo insmod plugins/smap/src/drivers/smap_tracking.ko
sudo insmod plugins/smap/src/tiering/smap_tiering.ko

# 2. Build SMAP user-space shared library (libsmap.so)
git submodule update --init --recursive
sh plugins/smap/build.sh

# 3. Enable either NVMe partition OR /dev/zram0 via SDK:
#    ubturbo_smap_swap_config("/dev/nvme0n1p2", 10, true);   // NVMe tier
#    ubturbo_smap_swap_config("/dev/zram0",    100, true);   // zram tier (auto-applies MextApplyZramPreset)
```

### C. Offline Model Pre-Training (`MXTF` Weight Export)
```bash
python3 scripts/train_mext_predictor.py --out /opt/ubturbo/conf/smap/mext_tiny_tf.bin --epochs 20
```
