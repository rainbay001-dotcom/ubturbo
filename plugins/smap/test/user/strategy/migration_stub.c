/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Scene-related operations
 * Create: 2025-6-17
 */
#include <stdint.h>
#include <stdio.h>

#include "user/manage/manage.h"

#define LOCAL_NUMA_BITS 4
#define REMOTE_NUMA_BITS 18
#define MAX_NODES (LOCAL_NUMA_BITS + REMOTE_NUMA_BITS)

int RunStrategyStub(ProcessAttr *process, struct MigList mlist[MAX_NODES][MAX_NODES], size_t mlistSize)
{
    int i, j;

    for (i = 0; i < MAX_NODES; i++) {
        for (j = 0; j < MAX_NODES; j++) {
            mlist[i][j].nr = 1;
        }
    }

    return 0;
}
