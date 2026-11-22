/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 * See the Mulan PSL v2 for more details.
 */
#ifndef __SWAP_DEVICE_H__
#define __SWAP_DEVICE_H__

#include <stdbool.h>
#include <stdint.h>
#include "swap_types.h"

#ifdef __cplusplus
extern "C" {
#endif

bool IsZramDevicePath(const char *path);
bool IsSwapDeviceActive(const char *path);
int ReadSwapDeviceStatus(const char *path, uint64_t *total_kb, uint64_t *used_kb);
int SwapDeviceInit(SwapDeviceConfig *cfg);
void SwapDeviceShutdown(SwapDeviceConfig *cfg);
void TuneKernelSwapParams(void);
void ValidateZswapConfig(void);

#ifdef __cplusplus
}
#endif

#endif /* __SWAP_DEVICE_H__ */
