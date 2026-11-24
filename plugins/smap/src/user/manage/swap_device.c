/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 * See the Mulan PSL v2 for more details.
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/swap.h>
#endif

#include "smap_user_log.h"
#include "securec.h"
#include "swap_device.h"

#define PROC_SWAPS_PATH "/proc/swaps"
#define SWAP_LINE_LEN 512
#define SYSCTL_PATH_LEN 128
#define SYSCTL_VAL_LEN 32

static bool IsSafeDevicePath(const char *path)
{
    if (!path || path[0] != '/') {
        return false;
    }
    if (strstr(path, "..") != NULL) {
        return false;
    }
    for (const char *p = path; *p != '\0'; p++) {
        unsigned char c = (unsigned char)*p;
        if (!(isalnum(c) || c == '/' || c == '_' || c == '-' || c == '.')) {
            return false;
        }
    }
    return true;
}

#ifdef __linux__
static int RunExecve(const char *prog, char *const argv[])
{
    pid_t pid = fork();
    if (pid < 0) {
        return -errno;
    }
    if (pid == 0) {
        char *envp[] = { "PATH=/usr/sbin:/sbin:/usr/bin:/bin", NULL };
        execve(prog, argv, envp);
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        return -errno;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        return 0;
    }
    return -EIO;
}
#endif

static int WriteFile(const char *path, const char *value)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        SMAP_LOGGER_WARNING("Failed to open %s for writing: %s", path, strerror(errno));
        return -errno;
    }
    int ret = fputs(value, f);
    fclose(f);
    return (ret >= 0) ? 0 : -EIO;
}

static int WriteSysctl(const char *param, const char *value)
{
    char path[SYSCTL_PATH_LEN];
    int ret = snprintf_s(path, sizeof(path), sizeof(path) - 1, "/proc/sys/%s", param);
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

bool IsZramDevicePath(const char *path)
{
    if (!path) {
        return false;
    }
    return strncmp(path, "/dev/zram", 9) == 0;
}

bool IsSwapDeviceActive(const char *path)
{
    if (!IsSafeDevicePath(path)) {
        return false;
    }

    FILE *f = fopen(PROC_SWAPS_PATH, "r");
    if (!f) {
        return false;
    }

    char line[SWAP_LINE_LEN];
    bool found = false;
    if (fgets(line, sizeof(line), f) == NULL) {
        fclose(f);
        return false;
    }

    while (fgets(line, sizeof(line), f)) {
        char dev[SWAP_DEVICE_PATH_LEN] = { 0 };
        if (sscanf(line, "%255s", dev) == 1 && strcmp(dev, path) == 0) {
            found = true;
            break;
        }
    }
    fclose(f);
    return found;
}

bool DetectActiveSwapDevice(char *path_out, size_t path_len)
{
    FILE *f = fopen(PROC_SWAPS_PATH, "r");
    if (!f) {
        return false;
    }

    char line[SWAP_LINE_LEN];
    if (fgets(line, sizeof(line), f) == NULL) {
        fclose(f);
        return false;
    }

    bool found = false;
    while (fgets(line, sizeof(line), f)) {
        char dev[SWAP_DEVICE_PATH_LEN] = { 0 };
        if (sscanf(line, "%255s", dev) == 1 && IsSafeDevicePath(dev)) {
            if (path_out && path_len > 0) {
                (void)snprintf_s(path_out, path_len, path_len - 1, "%s", dev);
            }
            found = true;
            break;
        }
    }
    fclose(f);
    return found;
}

int ReadSwapDeviceStatus(const char *path, uint64_t *total_kb, uint64_t *used_kb)
{
    if (!IsSafeDevicePath(path) || !total_kb || !used_kb) {
        return -EINVAL;
    }

    *total_kb = 0;
    *used_kb = 0;

    FILE *f = fopen(PROC_SWAPS_PATH, "r");
    if (!f) {
        return -ENOENT;
    }

    char line[SWAP_LINE_LEN];
    if (fgets(line, sizeof(line), f) == NULL) {
        fclose(f);
        return -ENOENT;
    }

    while (fgets(line, sizeof(line), f)) {
        char dev[SWAP_DEVICE_PATH_LEN] = { 0 };
        char type[32] = { 0 };
        unsigned long size = 0;
        unsigned long used = 0;
        int priority = 0;
        if (sscanf(line, "%255s %31s %lu %lu %d", dev, type, &size, &used, &priority) >= 4 &&
            strcmp(dev, path) == 0) {
            *total_kb = (uint64_t)size;
            *used_kb = (uint64_t)used;
            fclose(f);
            return 0;
        }
    }
    fclose(f);
    return -ENOENT;
}

int SwapDeviceInit(SwapDeviceConfig *cfg)
{
    if (!cfg || !IsSafeDevicePath(cfg->device_path)) {
        SMAP_LOGGER_ERROR("SwapDeviceInit: invalid device path");
        return -EINVAL;
    }

    if (access(cfg->device_path, F_OK) != 0) {
        SMAP_LOGGER_ERROR("Swap device %s does not exist", cfg->device_path);
        return -ENODEV;
    }

    cfg->activated_by_smap = false;
    if (IsSwapDeviceActive(cfg->device_path)) {
        SMAP_LOGGER_INFO("Swap device %s already active (external ownership preserved)",
                         cfg->device_path);
        return 0;
    }

#ifdef __linux__
    int flags = 0;
    if (cfg->priority >= 0) {
        flags |= SWAP_FLAG_PREFER | ((cfg->priority << SWAP_FLAG_PRIO_SHIFT) & SWAP_FLAG_PRIO_MASK);
    }
#ifdef SWAP_FLAG_DISCARD
    if (cfg->enable_discard) {
        flags |= SWAP_FLAG_DISCARD;
    }
#endif
    if (swapon(cfg->device_path, flags) == 0) {
        cfg->activated_by_smap = true;
        SMAP_LOGGER_INFO("Swap device %s activated via swapon(2) with priority %d",
                         cfg->device_path, cfg->priority);
        return 0;
    }
    int saved_err = errno;
    if (saved_err == EINVAL && cfg->type == SWAP_DEV_PARTITION && cfg->auto_swapon) {
        char *mkswap_argv[] = { "/usr/sbin/mkswap", cfg->device_path, NULL };
        if (access("/usr/sbin/mkswap", X_OK) != 0) {
            mkswap_argv[0] = "/sbin/mkswap";
        }
        if (RunExecve(mkswap_argv[0], mkswap_argv) == 0 && swapon(cfg->device_path, flags) == 0) {
            cfg->activated_by_smap = true;
            SMAP_LOGGER_INFO("Formatted and activated swap device %s", cfg->device_path);
            return 0;
        }
    }
    SMAP_LOGGER_ERROR("swapon(%s) failed: %s", cfg->device_path, strerror(saved_err));
    return -saved_err;
#else
    return -ENOSYS;
#endif
}

void SwapDeviceShutdown(SwapDeviceConfig *cfg)
{
    if (!cfg || !IsSafeDevicePath(cfg->device_path)) {
        return;
    }
    /*
     * Never call swapoff(2) on a host swap device that was already active
     * before SMAP started — tearing down pre-existing swap starves subsequent
     * oversubscribed workloads.
     */
    if (!cfg->activated_by_smap) {
        return;
    }
    if (!IsSwapDeviceActive(cfg->device_path)) {
        cfg->activated_by_smap = false;
        return;
    }
#ifdef __linux__
    if (swapoff(cfg->device_path) != 0) {
        SMAP_LOGGER_ERROR("swapoff(%s) failed: %s", cfg->device_path, strerror(errno));
    } else {
        cfg->activated_by_smap = false;
        SMAP_LOGGER_INFO("Swap device %s deactivated", cfg->device_path);
    }
#endif
}

void TuneKernelSwapParams(void)
{
    /*
     * Leave vm.swappiness untouched so kernel/cgroup fallback reclaim can
     * always swap anonymous pages during bursts; only disable blind swap
     * cluster readahead so MEXT's predictor controls swap-in lookahead.
     */
    if (WriteSysctl("vm/page-cluster", "0") == 0) {
        SMAP_LOGGER_INFO("Set vm.page-cluster=0");
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
