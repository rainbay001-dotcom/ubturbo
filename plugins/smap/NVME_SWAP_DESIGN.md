# SMAP NVMe Swap Option B Implementation Plan

## Context

SMAP manages L1 (local DRAM) ↔ L2 (remote CXL/PMEM) page migration but cannot spill coldest pages to persistent storage. When both tiers are full, the system OOM-kills. Option B adds an L3 tier (NVMe swap) entirely in user-space via `process_madvise(MADV_PAGEOUT)` + kernel `swapon`, requiring zero kernel module changes. Design doc: `Docs-repo/linux-memory-compression/smap_nvme_swap_option_b.html`.

## Branch

Create `feat/nvme-swap` branch in `/Users/ray/Documents/Repo/ubturbo` from current `master`.

## Build System

`aux_source_directory()` in `plugins/smap/src/user/CMakeLists.txt` auto-discovers all `.c` files in `manage/` and `strategy/` — no CMake changes needed. Just drop new `.c` files in place.

## Implementation Steps

### Step 1: Data Structures (`manage/manage.h`)

Add after line 229 (before `struct ProcessAttribute *next`):

```c
// Swap accounting
typedef struct {
    uint64_t current_swap_kb;
    uint64_t last_vm_swap;
    uint64_t total_swap_out_kb;
    uint64_t total_swap_in_kb;
    uint64_t swap_out_failures;
    uint64_t max_swap_kb;       // per-process limit, 0=unlimited
} SwapAccounting;

typedef struct {
    uint64_t page_count;
    uint8_t *cold_windows;      // cold_windows[i] per L2 page
    uint8_t threshold;           // default 5
} ColdTracker;

typedef struct {
    ColdTracker tracker[REMOTE_NUMA_NUM];
} ProcessColdState;
```

Add to `ProcessAttribute`:
- `SwapAccounting swapAccounting;`
- `ProcessColdState coldState;`
- `int swap_pidfd;` // cached pidfd, init -1

Extend `MigrateDirection` enum (line 96-99): add `SWAP_OUT`, `SWAP_IN`.

Add swap policy and device config structs (can be in a new `manage/swap_types.h`):

```c
typedef struct {
    char device_path[256];
    enum { SWAP_PARTITION, SWAP_FILE } type;
    uint64_t size_bytes;
    int priority;
    bool enable_discard;
    bool auto_swapon;
} SwapDeviceConfig;

typedef struct {
    uint32_t cold_window_threshold;  // default 5
    uint64_t max_swap_per_cycle;     // default 1024
    uint64_t max_swap_per_process;   // 0=unlimited
    double l2_watermark_ratio;       // default 0.85
    uint32_t batch_size;             // default 64
    bool swap_enabled;
    bool allow_vm_swap;              // default false
} SwapPolicy;
```

Add to `ProcessManager` struct (line 318-331): `SwapPolicy swapPolicy;`, `SwapDeviceConfig swapDevice;`.

### Step 2: Cold Window Tracker (`strategy/cold_tracker.c` + `.h`) — NEW FILE

Functions:
- `void InitColdTracker(ProcessColdState *state)` — zero all trackers
- `void DestroyColdTracker(ProcessColdState *state)` — free cold_windows arrays
- `void UpdateColdWindowCounters(ProcessAttr *process)` — for each L2 node: if freq=0 → increment (saturate at 255), else reset to 0. Rebuild tracker if page count changed.
- `uint64_t SelectSwapCandidates(ProcessAttr *process, uint64_t **addrs_out, uint8_t threshold)` — collect phys addrs where cold_windows >= threshold, return count.

Reuse existing `actcData[nid]` arrays — cold_tracker indexes parallel to these.

### Step 3: Swap Device Manager (`manage/swap_device.c` + `.h`) — NEW FILE

Functions:
- `int SwapDeviceInit(SwapDeviceConfig *cfg)` — validate path, mkswap if needed, swapon with priority + discard
- `void SwapDeviceShutdown(SwapDeviceConfig *cfg)` — swapoff
- `bool IsSwapDeviceActive(const char *path)` — parse `/proc/swaps`
- `int ReadSwapDeviceStatus(const char *path, uint64_t *total_kb, uint64_t *used_kb)` — parse `/proc/swaps`
- `void TuneKernelSwapParams(void)` — set vm.swappiness=10, vm.page-cluster=0, vma_ra_enabled=1
- `void ValidateZswapConfig(void)` — ensure writeback enabled if zswap active

Uses `system()` for mkswap/swapon (acceptable for init-time operations). Uses file reads for /proc/swaps, sysctl writes.

### Step 4: Swap Accounting (`manage/swap_account.c` + `.h`) — NEW FILE

Functions:
- `uint64_t ReadVmSwap(pid_t pid)` — parse `/proc/[pid]/status` for "VmSwap:" line. Reuse pattern from `ProcessSmapsFile()` at manage.c:288.
- `void UpdateSwapAccounting(ProcessAttr *process)` — read VmSwap, detect swap-in via delta with last_vm_swap, update cumulative stats.
- `void InitSwapAccounting(SwapAccounting *sa)` — zero all fields.

### Step 5: Swap-Out Executor (`strategy/swap_out.c` + `.h`) — NEW FILE

Functions:
- `int DoSwapOut(ProcessAttr *process, uint64_t *phys_addrs, uint64_t nr_pages)` — core execution:
  1. `GetOrOpenPidfd(process)` — pidfd_open() with caching in process->swap_pidfd
  2. Convert phys addrs to virtual addrs via `/proc/pid/pagemap` (reverse lookup using existing patterns from `oom_migrate.c:37-59`)
  3. Build batched `struct iovec[]` (batch_size=64)
  4. Call `syscall(SYS_process_madvise, pidfd, iov, n, MADV_PAGEOUT, 0)`
  5. Handle partial success, EAGAIN retry, ESRCH (process died)
  6. `usleep(1000)` between batches
- `static int GetOrOpenPidfd(ProcessAttr *process)` — cache pidfd
- `static void ClosePidfd(ProcessAttr *process)` — cleanup
- `static uint64_t PhysToVirt(ProcessAttr *process, uint64_t phys_addr)` — scan /proc/pid/pagemap to find VA for given PA

**PhysToVirt approach:** The existing `actcData[nid][i].addr` stores physical addresses. We need virtual addresses for process_madvise. Two options:
- **A**: Scan /proc/pid/pagemap fully to build PA→VA mapping (expensive but complete)
- **B**: Store VA alongside PA in actcData (requires kernel driver change — not Option B)
- **C**: Use /proc/pid/maps + pagemap to walk all VMAs and match PAs

Go with **A/C hybrid**: Build a hash map of PA→VA by walking /proc/pid/maps + pagemap once per cycle for swap candidates only. Reuse `GetPaddrsFromPagemap()` pattern from oom_migrate.c.

### Step 6: Integration into ScanMigrateWork (`strategy/migration.c`)

Insert Phase 2 block at **line 681** (after `PerformMigration()`, before `out:` label):

```c
    // === Phase 2: L2 → L3 NVMe swap-out ===
    if (manager->swapPolicy.swap_enabled && IsSwapDeviceActive(manager->swapDevice.device_path)) {
        for (current = manager->processes; current; current = current->next) {
            UpdateColdWindowCounters(current);
            if (!current->enableSwap) continue;
            if (current->type == VM_TYPE && !manager->swapPolicy.allow_vm_swap) continue;

            uint64_t *swap_addrs = NULL;
            uint64_t nr = SelectSwapCandidates(current, &swap_addrs,
                              manager->swapPolicy.cold_window_threshold);
            if (nr > 0) {
                nr = MIN(nr, manager->swapPolicy.max_swap_per_cycle);
                int swapped = DoSwapOut(current, swap_addrs, nr);
                SMAP_LOGGER_INFO("pid %d swapped %d/%lu pages to NVMe", current->pid, swapped, nr);
            }
            free(swap_addrs);
            UpdateSwapAccounting(current);
        }
    }
```

### Step 7: Init & Cleanup Wiring

**ProcessManagerInit()** (manage.c:135): Add default swap policy init:
```c
g_processManager.swapPolicy.cold_window_threshold = 5;
g_processManager.swapPolicy.max_swap_per_cycle = 1024;
g_processManager.swapPolicy.batch_size = 64;
g_processManager.swapPolicy.l2_watermark_ratio = 0.85;
g_processManager.swapPolicy.swap_enabled = false; // off by default
g_processManager.swapPolicy.allow_vm_swap = false;
```

**FreeProceccesAttr()** (manage.c:219-228): Add after ResetActcData (line 225):
```c
DestroyColdTracker(&attr->coldState);
ClosePidfd(attr);
```

### Step 8: SDK API (optional, lower priority)

Add to `smap_interface.c` / `smap_interface.h`:
- `int ubturbo_smap_swap_config_set(SwapDeviceConfig *dev, SwapPolicy *policy)`
- `int ubturbo_smap_swap_enable(bool enable)`

This can be deferred — for initial testing, hardcode swap config or load from config file.

## Files to Create (5 new)

| File | Location | Purpose |
|------|----------|---------|
| `swap_types.h` | `manage/` | SwapPolicy, SwapDeviceConfig, SwapAccounting, ColdTracker structs |
| `swap_device.c` + `.h` | `manage/` | NVMe swap device lifecycle (swapon/swapoff/status) |
| `swap_account.c` + `.h` | `manage/` | Per-process VmSwap tracking via /proc |
| `cold_tracker.c` + `.h` | `strategy/` | Multi-window cold page tracking |
| `swap_out.c` + `.h` | `strategy/` | process_madvise executor with pidfd + batching |

## Files to Modify (3 existing)

| File | Changes |
|------|---------|
| `manage/manage.h` | Add SWAP_OUT/SWAP_IN enum values, add SwapAccounting + ProcessColdState + swap_pidfd to ProcessAttribute, add SwapPolicy + SwapDeviceConfig to ProcessManager |
| `manage/manage.c` | Init swap fields in ProcessManagerInit():135, cleanup in FreeProceccesAttr():225 |
| `strategy/migration.c` | Insert Phase 2 swap-out block after line 680 in ScanMigrateWork() |

## Key Reusable Patterns

- `/proc/pid/status` parsing: `ProcessSmapsFile()` at manage.c:288 — same strncmp + sscanf pattern
- `/proc/pid/pagemap` reading: `GetPaddrFromMemRange()` at oom_migrate.c:37 — lseek + read 8-byte entries
- `/proc/pid/maps` parsing: `GetPaddrsFromPagemap()` at oom_migrate.c:61 — fgets + sscanf
- Mutex pattern: `EnvMutexLock/Unlock` from smap_env.h
- Atomic state: `EnvAtomic` from smap_env.h
- Secure string ops: `snprintf_s`, `memset_s` from securec.h

## Verification

1. **Build**: `cd plugins/smap && mkdir -p build && cd build && cmake .. && make` — must compile cleanly
2. **Unit test**: Create a test process, enable swap, verify cold_tracker increments on synthetic freq=0 data
3. **Integration test** (requires NVMe + Linux 5.10+):
   - `mkswap /dev/nvme0n1p3 && swapon /dev/nvme0n1p3`
   - Start SMAP with swap_enabled=true
   - Run a process with known cold pages
   - Verify VmSwap increases after cold_window_threshold cycles
   - Kill process, verify cleanup
4. **No regression**: Existing L1↔L2 migration must work identically when swap_enabled=false
