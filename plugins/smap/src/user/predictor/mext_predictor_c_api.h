/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 * Description: Pure-C ABI bridge connecting SMAP's C lifecycle/strategy layer
 *              to the C++17 MEXT Predictive Memory Engine.
 */
#ifndef SMAP_USER_PREDICTOR_MEXT_PREDICTOR_C_API_H
#define SMAP_USER_PREDICTOR_MEXT_PREDICTOR_C_API_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#include "manage/swap_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void MextDefaultConfig(MextConfig *cfg);

int MextEngineStart(int mig_fd, const MextConfig *cfg);
void MextEngineStop(void);
bool MextEngineIsRunning(void);

int MextEngineUpdateConfig(const MextConfig *cfg);
int MextEngineTrackPid(pid_t pid);
void MextEngineUntrackPid(pid_t pid);
int MextEngineQueryStats(MextStats *stats_out);

#ifdef __cplusplus
}
#endif

#endif /* SMAP_USER_PREDICTOR_MEXT_PREDICTOR_C_API_H */
