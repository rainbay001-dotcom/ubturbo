/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 * Description: Pure-C ABI bridge connecting SMAP's C strategy/lifecycle layer
 *              to the C++17 Dynamic Proactive Reclaim Controller.
 */
#ifndef SMAP_USER_RECLAIM_PROACTIVE_RECLAIM_C_API_H
#define SMAP_USER_RECLAIM_PROACTIVE_RECLAIM_C_API_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#include "manage/swap_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t (*ProactiveEvictFn)(uint64_t target_pages, uint8_t cold_window_threshold);

void ProactiveReclaimDefaultConfig(ProactiveReclaimConfig *cfg);
void ProactiveReclaimApplyZramPreset(ProactiveReclaimConfig *cfg);

int ProactiveReclaimStart(const ProactiveReclaimConfig *cfg, ProactiveEvictFn evict_fn);
void ProactiveReclaimStop(void);
bool ProactiveReclaimIsRunning(void);

int ProactiveReclaimUpdateConfig(const ProactiveReclaimConfig *cfg);
void ProactiveReclaimTriggerUrgent(uint64_t size_bytes);

int ProactiveReclaimTrackPid(pid_t pid);
void ProactiveReclaimUntrackPid(pid_t pid);
uint32_t ProactiveReclaimGetPidPressureScore(pid_t pid);
bool ProactiveReclaimShouldAgePidCold(pid_t pid);

uint8_t ProactiveReclaimRecommendedColdWindow(void);
uint32_t ProactiveReclaimCurrentUrgency(void);
bool ProactiveReclaimShouldTrackCold(void);
int ProactiveReclaimQueryStats(ProactiveReclaimStats *stats_out);

#ifdef __cplusplus
}
#endif

#endif /* SMAP_USER_RECLAIM_PROACTIVE_RECLAIM_C_API_H */
