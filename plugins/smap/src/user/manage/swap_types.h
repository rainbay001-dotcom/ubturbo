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

typedef struct {
    uint64_t current_swap_kb;
    uint64_t last_vm_swap;
    uint64_t total_swap_out_kb;
    uint64_t total_swap_in_kb;
    uint64_t swap_out_failures;
} SwapAccounting;

#endif /* __SWAP_TYPES_H__ */
