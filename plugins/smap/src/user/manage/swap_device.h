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
#ifndef __SWAP_DEVICE_H__
#define __SWAP_DEVICE_H__

#include "swap_types.h"

int SwapDeviceInit(SwapDeviceConfig *cfg);
void SwapDeviceShutdown(SwapDeviceConfig *cfg);
bool IsSwapDeviceActive(const char *path);
int ReadSwapDeviceStatus(const char *path, uint64_t *total_kb, uint64_t *used_kb);
void TuneKernelSwapParams(void);
void ValidateZswapConfig(void);

#endif /* __SWAP_DEVICE_H__ */
