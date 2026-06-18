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
#ifndef __COLD_TRACKER_H__
#define __COLD_TRACKER_H__

#include "manage/manage.h"

void InitProcessColdState(ProcessColdState *state);
void DestroyColdTracker(ProcessColdState *state);
void UpdateColdWindowCounters(ProcessAttr *process);
uint64_t SelectSwapCandidates(ProcessAttr *process, uint64_t **addrs_out, uint8_t threshold);

#endif /* __COLD_TRACKER_H__ */
