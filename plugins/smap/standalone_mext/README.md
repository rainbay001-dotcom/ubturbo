# Standalone MEXT (`mext_drv.ko` + `mextd`)

Runs **MEXT Predictive Memory** (Tier-1 Stride/Markov/Co-Eviction + Tier-2 INT8 TinyTransformer) and **Dynamic Proactive Reclaim** completely standalone — **without** loading `smap_tracking_core.ko`, `smap_access_tracking.ko`, `smap_tiering.ko`, or running `ubturbod`.

Eliminates 100% of legacy SMAP's per-process registration overhead (`popen` / `numa_maps` scans), periodic full-PTE page-table walks (`accessed_bit`), and `AP_STATE_MIG` lock contention.

---

## 1. Build

```bash
cd plugins/smap/standalone_mext
make          # builds `mextd` + `kernel/mext_drv.ko` on Linux
make check    # runs built-in standalone verification self-test
```

---

## 2. Benchmark Modes (Zero Modification to Workload / Sandbox Creation)

### Mode E-Standalone: Pure MEXT Predictive Prefetch (No Proactive Reclaim)

```bash
# 1. Unload SMAP modules if loaded (optional, ensures zero SMAP overhead)
rmmod smap_tiering smap_access_tracking smap_histogram_tracking smap_tracking_core 2>/dev/null || true

# 2. Load standalone 15 KB MEXT ring/prefault driver (/dev/mext_ctl)
sudo insmod kernel/mext_drv.ko

# 3. Start mextd in zero-RPC global fault-capture mode
#    - Automatically sets vm.page-cluster=0 (restored on exit)
#    - Automatically selects zram vs NVMe preset from /proc/swaps
#    - Exports /var/run/smap_mext_stats.json every second
sudo ./mextd --track-all --daemon --pidfile /var/run/mextd.pid
```

### Mode G-Standalone: MEXT Prefetch + Dynamic Proactive Reclaim (Cgroup v1 & v2)

```bash
sudo insmod kernel/mext_drv.ko

# Automatically discovers memory-capped sandboxes under /sys/fs/cgroup/memory (v1)
# or /sys/fs/cgroup (v2) while ignoring unconstrained host daemons:
sudo ./mextd --track-all --reclaim --watch-cgroup auto \
    --daemon --pidfile /var/run/mextd.pid
```

### Teardown & Telemetry Inspection

```bash
cat /var/run/smap_mext_stats.json
sudo kill -TERM "$(cat /var/run/mextd.pid)"
sudo rmmod mext_drv
```

---

## 3. Offline Real-Trace Weight Training (`--dump-trace` + `mext_train`)

```bash
# Step 1: Record 1 baseline workload round (captures BOTH swap faults AND cgroup evictions)
sudo insmod kernel/mext_drv.ko
sudo ./mextd --track-all --record-only --dump-trace /var/tmp/mext_round1.trace \
    --daemon --pidfile /var/run/mextd.pid
# ... run 1 round of workload ...
sudo kill -TERM "$(cat /var/run/mextd.pid)"

# Step 2: Train INT8+FP32 TinyTransformer weights offline (~5-15s on CPU)
#         (Automatically strips Linux do_swap_page VM_FAULT_RETRY duplicates from existing traces)
./mext_train --trace /var/tmp/mext_round1.trace --out ./mext_tiny_tf.bin --epochs 8 --horizon 12

# Step 3: Benchmark with pre-trained weights loaded (freezing converged output head)
sudo ./mextd --track-all --weights ./mext_tiny_tf.bin --no-online-adapt \
    --daemon --pidfile /var/run/mextd.pid
```
