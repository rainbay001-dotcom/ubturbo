/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 * See the Mulan PSL v2 for more details.
 */
#ifndef __SWAP_ACCOUNT_H__
#define __SWAP_ACCOUNT_H__

#include <stdint.h>
#include <sys/types.h>
#include "swap_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void InitSwapAccounting(SwapAccounting *sa);
uint64_t ReadVmSwap(pid_t pid);
void UpdateSwapAccounting(pid_t pid, SwapAccounting *sa);

#ifdef __cplusplus
}
#endif

#endif /* __SWAP_ACCOUNT_H__ */
