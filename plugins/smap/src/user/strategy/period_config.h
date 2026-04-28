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

#ifndef PERIOD_CONFIG_H
#define PERIOD_CONFIG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t GetScanPeriodConfig(void);

uint32_t GetMigratePeriodConfig(void);

uint32_t GetRemoteFreqPercentileConfig(void);

uint32_t GetSlowThresholdConfig(void);

uint64_t GetFreqWtConfig(void);

uint32_t GetColdWindowThresholdConfig(void);

uint64_t GetMaxSwapKbConfig(void);

bool GetFileConfSwitchConfig(void);

int32_t GeneratePeriodConfigFile(const char *configFile);

bool GetScanPeriodChanged(void);

bool GetMigratePeriodChanged(void);

void SetScanPeriodChanged(bool val);

void SetMigratePeriodChanged(bool val);

void PeriodConfigRead(const char *configFile);

#ifdef __cplusplus
}
#endif

#endif
