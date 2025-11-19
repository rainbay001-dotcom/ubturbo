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

#include <stdint.h>
#include <string.h>
#include <sys/param.h>
#include "securec.h"
#include "smap_user_log.h"
#include "manage/manage.h"
#include "separate_strategy.h"
#include "strategy.h"

uint64_t GetNrFreePagesByNode(int nid)
{
    int ret;
    uint64_t nr = 0;
    FILE *file;
    char filename[BUFFER_SIZE];
    char line[BUFFER_SIZE] = { 0 };
    char fmt[BUFFER_SIZE] = "/sys/devices/system/node/node%d/meminfo";
    char seps[] = " ";
    char *token = NULL;
    int tmpCnt = FREE_BYTES_INDEX;

    ret = snprintf_s(filename, BUFFER_SIZE, BUFFER_SIZE, fmt, nid);
    if (ret == -1) {
        SMAP_LOGGER_ERROR("snprintf_s failed.");
        return 0;
    }
    file = fopen(filename, "r");
    if (!file) {
        SMAP_LOGGER_ERROR("fopen %s failed, errno: %d.", filename, errno);
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        if (strstr(line, "MemFree") == NULL) {
            continue;
        }
        token = strtok(line, seps);
        while (token && tmpCnt--) {
            token = strtok(NULL, seps);
        }
        if (sscanf_s(token, "%lu", &nr) <= 0) {
            SMAP_LOGGER_ERROR("sscanf_s failed.");
            nr = 0;
        }
        break;
    }
    nr >>= KB_TO_PAGE_SHIFT;
    nr -= (nr > NR_RESERVED_PAGES ? NR_RESERVED_PAGES : nr);

    ret = fclose(file);
    if (ret) {
        SMAP_LOGGER_ERROR("close %s failed, errno: %d.", filename, errno);
    }
    return nr;
}

uint64_t GetNrFreeHugePagesByNode(int nid)
{
    int ret;
    uint64_t nr = 0;
    FILE *file;
    char filename[BUFFER_SIZE];
    char line[BUFFER_SIZE];
    char fmt[BUFFER_SIZE] = "/sys/devices/system/node/node%d/hugepages/hugepages-2048kB/free_hugepages";

    ret = snprintf_s(filename, BUFFER_SIZE, BUFFER_SIZE, fmt, nid);
    if (ret == -1) {
        SMAP_LOGGER_ERROR("snprintf_s failed.");
        return 0;
    }
    file = fopen(filename, "r");
    if (!file) {
        SMAP_LOGGER_ERROR("fopen %s failed, errno: %d.", filename, errno);
        return 0;
    }

    if (fgets(line, sizeof(line), file)) {
        if (sscanf_s(line, "%llu", &nr) <= 0) {
            SMAP_LOGGER_ERROR("sscanf_s failed.");
            nr = 0;
        }
    }

    ret = fclose(file);
    if (ret) {
        SMAP_LOGGER_ERROR("close %s failed, errno: %d.", filename, errno);
    }
    return nr;
}

int RunStrategy(ProcessAttr *process, struct MigList mlist[MAX_NODES][MAX_NODES], size_t mlistSize)
{
    if (mlistSize < MAX_NODES) {
        SMAP_LOGGER_ERROR("Miglist size is small, size:%zu.", mlistSize);
        return -EINVAL;
    }
    if (!process || CheckActcDataValid(process)) {
        SMAP_LOGGER_ERROR("Invalid pid %d actc.", process ? process->pid : -1);
        return -EINVAL;
    }
    if (IsHugeMode()) {
        if (IsMultiNumaVm(process)) {
            return SeparateStrategyMultiNumaVm(process, mlist);
        } else {
            return SeparateStrategy(process, mlist);
        }
    } else {
        return SeparateStrategy4K(process, mlist);
    }
}
