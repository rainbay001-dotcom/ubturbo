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
#ifndef __SWAP_ACCOUNT_H__
#define __SWAP_ACCOUNT_H__

#include <unistd.h>
#include "swap_types.h"

void InitSwapAccounting(SwapAccounting *sa);
uint64_t ReadVmSwap(pid_t pid);
void UpdateSwapAccounting(pid_t pid, SwapAccounting *sa);

#endif /* __SWAP_ACCOUNT_H__ */
