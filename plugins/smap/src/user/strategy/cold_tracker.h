/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 * See the Mulan PSL v2 for more details.
 */
#ifndef __COLD_TRACKER_H__
#define __COLD_TRACKER_H__

#include <stdbool.h>
#include <stdint.h>
#include "manage/manage.h"
#include "manage/swap_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void InitProcessColdState(ProcessColdState *state);
void DestroyColdTracker(ProcessColdState *state);
bool ShouldTrackNodeForSwap(ProcessAttr *process, int nid);
void UpdateColdWindowCounters(ProcessAttr *process);
uint64_t CountColdSwapCandidates(ProcessAttr *process, uint8_t threshold);

#ifdef __cplusplus
}
#endif

#endif /* __COLD_TRACKER_H__ */
