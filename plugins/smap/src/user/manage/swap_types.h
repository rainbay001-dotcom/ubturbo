/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef __SWAP_TYPES_H__
#define __SWAP_TYPES_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SWAP_DEVICE_PATH_LEN 256
#define SWAP_DEFAULT_COLD_WINDOW_THRESHOLD 5
#define SWAP_DEFAULT_MAX_PER_CYCLE 1024
#define SWAP_DEFAULT_BATCH_SIZE 64
#define SWAP_DEFAULT_L2_WATERMARK 0.85
#define COLD_TRACKER_MAX_WINDOWS 255
#define SWAP_MAX_NODES 22 /* LOCAL_NUMA_NUM + REMOTE_NUMA_NUM */

typedef enum {
    SWAP_DEV_PARTITION = 0,
    SWAP_DEV_FILE,
} SwapDeviceType;

typedef struct {
    char device_path[SWAP_DEVICE_PATH_LEN];
    SwapDeviceType type;
    uint64_t size_bytes;
    int priority;
    bool enable_discard;
    bool auto_swapon;
} SwapDeviceConfig;

typedef struct {
    uint32_t cold_window_threshold;
    uint64_t max_swap_per_cycle;
    uint64_t max_swap_per_process;
    double l2_watermark_ratio;
    uint32_t batch_size;
    bool swap_enabled;
    bool allow_vm_swap;
} SwapPolicy;

typedef struct {
    uint64_t current_swap_kb;
    uint64_t last_vm_swap;
    uint64_t total_swap_out_kb;
    uint64_t total_swap_in_kb;
    uint64_t swap_out_failures;
    uint64_t max_swap_kb;
} SwapAccounting;

typedef struct {
    uint64_t page_count;
    uint8_t *cold_windows;
    uint8_t threshold;
} ColdTracker;

typedef struct {
    ColdTracker tracker[SWAP_MAX_NODES];
} ProcessColdState;

/* MEXT Predictive Memory runtime configuration & telemetry snapshot */
typedef struct {
    bool enabled;
    bool enable_transformer;
    bool enable_online_adapt;
    bool enable_prefault;
    uint32_t fault_trigger_stride; /* run transformer every N faults (default 4) */
    uint32_t top_k;                /* max pages predicted per step (default 16) */
    float prefetch_threshold;      /* MADV_WILLNEED prob threshold (default 0.25) */
    float prefault_threshold;      /* SMAP_MIG_PREFAULT prob threshold (default 0.75) */
    double psi_stall_limit;        /* throttle prefetch when PSI some avg10 > limit */
    char weights_path[SWAP_DEVICE_PATH_LEN];
} MextConfig;

typedef struct {
    uint64_t faults_processed;
    uint64_t evicts_tracked;
    uint64_t shadow_active_pages;
    uint64_t tier1_predictions;
    uint64_t transformer_predictions;
    uint64_t prefetch_issued;      /* pages submitted via process_madvise(MADV_WILLNEED) */
    uint64_t prefault_issued;      /* pages submitted via SMAP_MIG_PREFAULT */
    uint64_t prediction_hits;      /* predicted pages confirmed by subsequent stream / minor hits */
    uint64_t online_train_steps;   /* live FP32 output-head SGD updates */
    uint64_t kernel_ring_dropped;
    double avg_infer_us;           /* exponential moving average of Transformer step latency */
    double hit_rate;               /* recent precision / hit ratio */
    double psi_some_avg10;         /* current /proc/pressure/memory stall % */
} MextStats;

/* Dynamic Proactive Reclaim urgency regimes */
typedef enum {
    RECLAIM_CALM      = 0, /* Free >= W_dyn and ample TTE: keep working set in DRAM */
    RECLAIM_PROACTIVE = 1, /* Free < W_dyn: gentle out-of-cycle drain of cold pages */
    RECLAIM_BURST     = 2, /* High allocation velocity / low TTE: ramp swap & step cold age */
    RECLAIM_EMERGENCY = 3, /* Imminent WMARK_MIN cliff: max batch & warm-cold fallback */
} ProactiveReclaimUrgency;

/* Dynamic Proactive Reclaim controller runtime configuration */
typedef struct {
    bool enabled;
    bool is_zram_backend;           /* true when swap device is synchronous zram */
    uint32_t poll_interval_ms;      /* fast watchdog sample period (default 15 ms; zram 10 ms) */
    uint32_t lookahead_horizon_ms;  /* drain horizon tau (default 80 ms NVMe; 35 ms zram) */
    double base_headroom_ratio;     /* steady-state floor multiplier over WMARK_LOW (default 1.15) */
    double max_headroom_ratio;      /* max dynamic headroom as fraction of total RAM (default 0.20) */
    double velocity_gain;           /* alpha gain on A(t)*tau (default 1.35) */
    double accel_gain;              /* beta gain on 0.5*max(0,dA/dt)*tau^2 (default 0.50) */
    double psi_full_brake_limit;    /* PSI full avg10 % where thrash brake engages (default 15.0) */
    uint32_t min_cold_window;       /* minimum cold window under burst/emergency (default 1) */
    uint32_t calm_cold_window;      /* relaxed cold window in RECLAIM_CALM (default 8) */
    uint64_t max_pages_per_tick;    /* per-tick eviction cap (default 8192) */
} ProactiveReclaimConfig;

/* Dynamic Proactive Reclaim live telemetry snapshot */
typedef struct {
    uint64_t ticks_total;
    uint64_t proactive_wakeups;         /* kernel MEXT_EVT_MEM_PRESSURE + urgent triggers */
    uint64_t burst_activations;         /* ticks spent in BURST or EMERGENCY */
    uint64_t thrash_brake_events;       /* ticks damped by PSI/uncompensated-refault brake */
    uint64_t pages_reclaimed_proactive; /* cumulative pages evicted proactively */
    uint64_t kernel_allocstalls_observed;
    uint32_t current_urgency;           /* ProactiveReclaimUrgency */
    uint32_t effective_cold_window;     /* live cold_window_threshold [1..255] */
    uint64_t current_free_pages;
    uint64_t dynamic_wmark_pages;       /* W_dyn(t) in pages */
    uint64_t static_wmark_low_pages;
    double alloc_velocity_mbps;         /* compensation-aware demand velocity (MB/s) */
    double alloc_accel_mbps2;           /* smoothed demand acceleration (MB/s^2) */
    double estimated_tte_ms;            /* estimated Time-To-Exhaustion before WMARK_MIN */
    double thrash_brake_scale;          /* [0.20 .. 1.0] active thrash-protection multiplier */
} ProactiveReclaimStats;

#ifdef __cplusplus
}
#endif

#endif /* __SWAP_TYPES_H__ */
