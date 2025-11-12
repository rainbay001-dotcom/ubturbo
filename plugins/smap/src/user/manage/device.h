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
#ifndef __DEVICE_H__
#define __DEVICE_H__

#include <sys/stat.h>

#include "manage.h"

#define TIERING_PATH "/dev/smap_mig_device"
#define NODE_PATH "/dev/smap_node%d"
#define MS_PER_SEC 1000
#define US_PER_MSEC 1000
#define NS_PER_USEC 1000
#define US_PER_SEC (US_PER_MSEC * MS_PER_SEC)
#define SCAN_TIME_4K LIGHT_STABLE_SCAN_CYCLE
#define SCAN_TIME_2M LIGHT_STABLE_SCAN_CYCLE
#define SCAN_PERIOD_UNIT_MS 50
#define SMAP_LOCK_PERM (S_IRUSR | S_IWUSR | S_IRGRP)

int EnableTracking(struct ProcessManager *manager);

int DisableTracking(struct ProcessManager *manager);

int ReadTracking(struct ProcessManager *manager);

int InitTrackingDev(struct ProcessManager *manager);

void DeinitTrackingDev(struct ProcessManager *manager);

void FreeAddrMemory(struct ProcessManager *manager);

void FreeNodeActcData(struct ProcessManager *manager);

int GetRamIsChange(struct ProcessManager *manager, int *change);

int GetNodeActcLenIomem(int len, uint64_t *nodeActcLen, IomemMsg *msg, uint16_t nrLocalNuma);

int GetIomemAddresses(struct ProcessManager *manager);

#endif /* __DEVICE_H__ */
