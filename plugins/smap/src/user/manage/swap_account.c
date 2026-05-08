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

#include <stdio.h>
#include <string.h>
#include "securec.h"
#include "swap_account.h"

#define STATUS_LINE_LEN 256
#define STATUS_PATH_LEN 64
#define VMSWAP_PREFIX "VmSwap:"
#define VMSWAP_PREFIX_LEN 7

uint64_t ReadVmSwap(pid_t pid)
{
    char path[STATUS_PATH_LEN];
    int ret = snprintf_s(path, sizeof(path), sizeof(path), "/proc/%d/status", pid);
    if (ret < 0) {
        return 0;
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        return 0;
    }

    char line[STATUS_LINE_LEN];
    uint64_t swap_kb = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, VMSWAP_PREFIX, VMSWAP_PREFIX_LEN) == 0) {
            unsigned long value;
            if (sscanf(line + VMSWAP_PREFIX_LEN, "%lu", &value) == 1) {
                swap_kb = value;
            }
            break;
        }
    }

    fclose(f);
    return swap_kb;
}
