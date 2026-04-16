/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
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

#define SWAP_DEVICE_PATH_LEN 256
#define SWAP_DEFAULT_COLD_WINDOW_THRESHOLD 5
#define SWAP_DEFAULT_MAX_PER_CYCLE 1024
#define SWAP_DEFAULT_BATCH_SIZE 64
#define SWAP_DEFAULT_L2_WATERMARK 0.85
#define SWAP_YIELD_US 1000

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

#define COLD_TRACKER_MAX_WINDOWS 255

typedef struct {
    ColdTracker tracker[18]; /* REMOTE_NUMA_NUM */
} ProcessColdState;

#endif /* __SWAP_TYPES_H__ */
