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
#ifndef __SMAP_INNER_INTERFACE_H__
#define __SMAP_INNER_INTERFACE_H__

#include "smap_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    DISABLE_MIG_INFO,
    ENABLE_MIG_INFO,
};

enum {
    DISABLE_ADAPT_MEM,
    ENABLE_ADAPT_MEM,
};

struct VmRatio {
    pid_t pid;
    double ratio;
};

struct VmRatioMsg {
    int nrVm;
    struct VmRatio vr[MAX_NR_MIGOUT];
};

int SmapEnableAdaptMem(int flag);
int SmapQueryVmMemRatio(struct VmRatioMsg *vrMsg);

#ifdef __cplusplus
}
#endif

#endif /* __SMAP_INNER_INTERFACE_H__ */
