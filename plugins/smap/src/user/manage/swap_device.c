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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "smap_user_log.h"
#include "securec.h"
#include "swap_device.h"

#define PROC_SWAPS_PATH "/proc/swaps"
#define SWAP_LINE_LEN 512
#define SYSCTL_PATH_LEN 128
#define SYSCTL_VAL_LEN 32

static int WriteFile(const char *path, const char *value)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        SMAP_LOGGER_ERROR("Failed to open %s for writing: %s", path, strerror(errno));
        return -errno;
    }
    int ret = fputs(value, f);
    fclose(f);
    return (ret >= 0) ? 0 : -EIO;
}

static int WriteSysctl(const char *param, const char *value)
{
    char path[SYSCTL_PATH_LEN];
    int ret = snprintf_s(path, sizeof(path), sizeof(path), "/proc/sys/%s", param);
    if (ret < 0) {
        return -EINVAL;
    }
    return WriteFile(path, value);
}

static bool ReadFileBool(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        return false;
    }
    char buf[SYSCTL_VAL_LEN];
    bool result = false;
    if (fgets(buf, sizeof(buf), f)) {
        result = (buf[0] == '1' || buf[0] == 'Y' || buf[0] == 'y');
    }
    fclose(f);
    return result;
}

bool IsSwapDeviceActive(const char *path)
{
    if (!path || path[0] == '\0') {
        return false;
    }

    FILE *f = fopen(PROC_SWAPS_PATH, "r");
    if (!f) {
        return false;
    }

    char line[SWAP_LINE_LEN];
    bool found = false;
    /* Skip header line */
    if (fgets(line, sizeof(line), f) == NULL) {
        fclose(f);
        return false;
    }

    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, path)) {
            found = true;
            break;
        }
    }
    fclose(f);
    return found;
}

int ReadSwapDeviceStatus(const char *path, uint64_t *total_kb, uint64_t *used_kb)
{
    if (!path || !total_kb || !used_kb) {
        return -EINVAL;
    }

    *total_kb = 0;
    *used_kb = 0;

    FILE *f = fopen(PROC_SWAPS_PATH, "r");
    if (!f) {
        return -ENOENT;
    }

    char line[SWAP_LINE_LEN];
    /* Skip header */
    if (fgets(line, sizeof(line), f) == NULL) {
        fclose(f);
        return -ENOENT;
    }

    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, path)) {
            char dev[SWAP_DEVICE_PATH_LEN];
            char type[32];
            uint64_t size, used;
            int priority;
            if (sscanf(line, "%255s %31s %lu %lu %d", dev, type, &size, &used, &priority) >= 4) {
                *total_kb = size;
                *used_kb = used;
                fclose(f);
                return 0;
            }
        }
    }
    fclose(f);
    return -ENOENT;
}

int SwapDeviceInit(SwapDeviceConfig *cfg)
{
    if (!cfg || cfg->device_path[0] == '\0') {
        SMAP_LOGGER_ERROR("SwapDeviceInit: invalid config");
        return -EINVAL;
    }

    if (access(cfg->device_path, F_OK) != 0) {
        SMAP_LOGGER_ERROR("Swap device %s does not exist", cfg->device_path);
        return -ENODEV;
    }

    if (IsSwapDeviceActive(cfg->device_path)) {
        SMAP_LOGGER_INFO("Swap device %s already active", cfg->device_path);
        return 0;
    }

    /* Format if partition type and not yet formatted */
    if (cfg->type == SWAP_DEV_PARTITION) {
        char cmd[SWAP_LINE_LEN];
        int ret = snprintf_s(cmd, sizeof(cmd), sizeof(cmd), "mkswap %s 2>/dev/null", cfg->device_path);
        if (ret < 0) {
            return -EINVAL;
        }
        ret = system(cmd);
        if (ret != 0) {
            SMAP_LOGGER_ERROR("mkswap %s failed: %d", cfg->device_path, ret);
            return -EIO;
        }
        SMAP_LOGGER_INFO("Formatted %s as swap", cfg->device_path);
    }

    /* Enable swap */
    char cmd[SWAP_LINE_LEN];
    int ret;
    if (cfg->enable_discard) {
        ret = snprintf_s(cmd, sizeof(cmd), sizeof(cmd),
                         "swapon -p %d -o discard=pages %s", cfg->priority, cfg->device_path);
    } else {
        ret = snprintf_s(cmd, sizeof(cmd), sizeof(cmd),
                         "swapon -p %d %s", cfg->priority, cfg->device_path);
    }
    if (ret < 0) {
        return -EINVAL;
    }

    ret = system(cmd);
    if (ret != 0) {
        SMAP_LOGGER_ERROR("swapon %s failed: %d", cfg->device_path, ret);
        return -EIO;
    }

    SMAP_LOGGER_INFO("Swap device %s activated with priority %d", cfg->device_path, cfg->priority);
    return 0;
}

void SwapDeviceShutdown(SwapDeviceConfig *cfg)
{
    if (!cfg || cfg->device_path[0] == '\0') {
        return;
    }

    if (!IsSwapDeviceActive(cfg->device_path)) {
        return;
    }

    char cmd[SWAP_LINE_LEN];
    int ret = snprintf_s(cmd, sizeof(cmd), sizeof(cmd), "swapoff %s", cfg->device_path);
    if (ret < 0) {
        return;
    }

    ret = system(cmd);
    if (ret != 0) {
        SMAP_LOGGER_ERROR("swapoff %s failed: %d", cfg->device_path, ret);
    } else {
        SMAP_LOGGER_INFO("Swap device %s deactivated", cfg->device_path);
    }
}

void TuneKernelSwapParams(void)
{
    /* Low swappiness: let SMAP control what gets swapped */
    if (WriteSysctl("vm/swappiness", "10") == 0) {
        SMAP_LOGGER_INFO("Set vm.swappiness=10");
    }

    /* No readahead clustering (NVMe is fast enough) */
    if (WriteSysctl("vm/page-cluster", "0") == 0) {
        SMAP_LOGGER_INFO("Set vm.page-cluster=0");
    }

    /* VMA-based readahead (better hit rate on NVMe) */
    if (WriteFile("/sys/kernel/mm/swap/vma_ra_enabled", "1") == 0) {
        SMAP_LOGGER_INFO("Enabled VMA-based swap readahead");
    }
}

void ValidateZswapConfig(void)
{
    if (ReadFileBool("/sys/module/zswap/parameters/enabled")) {
        SMAP_LOGGER_INFO("Zswap is enabled");
        if (WriteFile("/sys/module/zswap/parameters/writeback_enabled", "Y") == 0) {
            SMAP_LOGGER_INFO("Ensured zswap writeback is enabled");
        }
    }
}
