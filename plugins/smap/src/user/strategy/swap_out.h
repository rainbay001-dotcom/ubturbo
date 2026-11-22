/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 * See the Mulan PSL v2 for more details.
 */
#ifndef __SWAP_OUT_H__
#define __SWAP_OUT_H__

#include <stdint.h>
#include "manage/manage.h"

#ifdef __cplusplus
extern "C" {
#endif

int DoSwapOut(ProcessAttr *process, uint64_t max_pages, uint8_t threshold);
uint64_t ExecuteProactiveSwapBatch(uint64_t target_pages, uint8_t threshold);

#ifdef __cplusplus
}
#endif

#endif /* __SWAP_OUT_H__ */
