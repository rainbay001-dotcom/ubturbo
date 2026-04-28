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

/*
 * smap_swap_test.c - L1->NVMe swap acceptance test
 *
 * Validates that cold pages on L1 DRAM are swapped to NVMe via
 * the SMAP_MIG_SWAP_OUT ioctl. Uses ubturbo_smap_migrate_out with
 * SMAP_NVME_SWAP_NID sentinel to register the process in swapMode
 * (NORMAL_SCAN, L1 tracking, no L1->L2 migration).
 *
 * Build:
 *   gcc -o smap_swap_test plugins/smap/test/smap_swap_test.c \
 *       -I plugins/smap/src/user \
 *       -L <path-to-libsmap.so> -lsmap -lpthread
 *
 * Usage:
 *   ./smap_swap_test <scan_ms> <migrate_ms> <proc_type> <pid> [cold_threshold [max_swap_kb]]
 *
 *   scan_ms         - per-process cold-page scan period in ms [50, 20000], multiple of 50
 *   migrate_ms      - global swap cycle period in ms [50, 20000]
 *   proc_type       - 0 = normal process (4K pages), 1 = VM (2M pages)
 *   pid             - PID of the target process to manage
 *   cold_threshold  - consecutive zero-freq windows before swap [1, 255], default 5
 *   max_swap_kb     - per-process cumulative swap limit in KB; 0 = prohibit swap;
 *                     omit to leave unlimited (smap.swap.max.kb not written)
 *
 * Prerequisites:
 *   - smap kernel modules loaded (smap_tracking_core, smap_histogram_tracking,
 *     smap_access_tracking smap_scene=2, smap_tiering smap_scene=2 smap_pgsize=0)
 *   - Swap partition activated (swapon -s)
 *   - Sufficient privilege (root or CAP_SYS_ADMIN)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

#include "smap_interface.h"

/* Sentinel destNid for ubturbo_smap_migrate_out to trigger L1->NVMe swap mode */
#define SMAP_NVME_SWAP_NID (-2)

#define PERIOD_CONFIG_FILE "/opt/ubturbo/conf/smap/period.config"
#define DEFAULT_COLD_THRESHOLD 5
#define MONITOR_INTERVAL_SEC 10
/* Sentinel: max_swap_kb not specified by caller — do not write the config key */
#define MAX_SWAP_KB_UNSET UINT64_MAX

static pid_t g_target_pid = -1;
static volatile int g_stop = 0;

static void on_signal(int sig)
{
    (void)sig;
    g_stop = 1;
}

static int write_period_config(uint32_t scan_ms, uint32_t migrate_ms,
                               uint32_t cold_threshold, uint64_t max_swap_kb)
{
    FILE *f = fopen(PERIOD_CONFIG_FILE, "w");
    if (!f) {
        fprintf(stderr, "[test] failed to open %s: %s\n",
                PERIOD_CONFIG_FILE, strerror(errno));
        return -1;
    }
    fprintf(f, "smap.scan.period = %u\n",              scan_ms);
    fprintf(f, "smap.migrate.period = %u\n",           migrate_ms);
    fprintf(f, "smap.remote.freq.percentile = 99\n");
    fprintf(f, "smap.slow.threshold = 2\n");
    fprintf(f, "smap.freq.wt = 0\n");
    fprintf(f, "smap.period.file.config.switch = true\n");
    fprintf(f, "smap.swap.cold.threshold = %u\n",      cold_threshold);
    if (max_swap_kb != MAX_SWAP_KB_UNSET) {
        fprintf(f, "smap.swap.max.kb = %llu\n", (unsigned long long)max_swap_kb);
    }
    fclose(f);
    if (max_swap_kb == MAX_SWAP_KB_UNSET) {
        printf("[test] period config written: scan=%ums  migrate=%ums  cold_threshold=%u  max_swap_kb=unlimited\n",
               scan_ms, migrate_ms, cold_threshold);
    } else {
        printf("[test] period config written: scan=%ums  migrate=%ums  cold_threshold=%u  max_swap_kb=%llu\n",
               scan_ms, migrate_ms, cold_threshold, (unsigned long long)max_swap_kb);
    }
    return 0;
}

static void cleanup_period_config(void)
{
    remove(PERIOD_CONFIG_FILE);
}

int main(int argc, char *argv[])
{
    if (argc < 5 || argc > 7) {
        fprintf(stderr,
                "Usage: %s <scan_ms> <migrate_ms> <proc_type> <pid> [cold_threshold [max_swap_kb]]\n"
                "\n"
                "  scan_ms        : cold-page scan period in ms  [50, 20000], multiple of 50\n"
                "  migrate_ms     : swap cycle period in ms      [50, 20000]\n"
                "  proc_type      : 0 = normal process (4K)  1 = VM (2M)\n"
                "  pid            : target process PID\n"
                "  cold_threshold : consecutive zero-freq windows [1, 255], default %d\n"
                "  max_swap_kb    : per-process cumulative swap limit in KB; 0 = prohibit swap;\n"
                "                   omit to leave unlimited\n",
                argv[0], DEFAULT_COLD_THRESHOLD);
        return 1;
    }

    uint32_t scan_ms       = (uint32_t)atoi(argv[1]);
    uint32_t migrate_ms    = (uint32_t)atoi(argv[2]);
    int      proc_type     = atoi(argv[3]);
    pid_t    target_pid    = (pid_t)atoi(argv[4]);
    uint32_t cold_threshold = (argc >= 6) ? (uint32_t)atoi(argv[5]) : DEFAULT_COLD_THRESHOLD;
    uint64_t max_swap_kb   = (argc == 7) ? (uint64_t)strtoull(argv[6], NULL, 10) : MAX_SWAP_KB_UNSET;

    if (scan_ms < 50 || scan_ms > 20000 || scan_ms % 50 != 0) {
        fprintf(stderr, "[test] scan_ms must be a multiple of 50 in [50, 20000]\n");
        return 1;
    }
    if (migrate_ms < 50 || migrate_ms > 20000) {
        fprintf(stderr, "[test] migrate_ms must be in [50, 20000]\n");
        return 1;
    }
    if (proc_type < 0 || proc_type > 1) {
        fprintf(stderr, "[test] proc_type must be 0 (normal) or 1 (VM)\n");
        return 1;
    }
    if (kill(target_pid, 0) != 0) {
        fprintf(stderr, "[test] pid %d invalid: %s\n", target_pid, strerror(errno));
        return 1;
    }
    if (cold_threshold < 1 || cold_threshold > 255) {
        fprintf(stderr, "[test] cold_threshold must be in [1, 255]\n");
        return 1;
    }

    g_target_pid = target_pid;

    /* Step 1: Write period config */
    if (write_period_config(scan_ms, migrate_ms, cold_threshold, max_swap_kb) < 0) {
        return 1;
    }
    atexit(cleanup_period_config);

    /* Step 2: Start SMAP */
    uint32_t page_type = (proc_type == 1) ? PAGETYPE_HUGE : PAGETYPE_NORMAL;
    int ret = ubturbo_smap_start(page_type, NULL);
    if (ret != 0) {
        fprintf(stderr, "[test] ubturbo_smap_start failed: %d\n", ret);
        return 1;
    }
    printf("[test] SMAP started  page_type=%s\n",
           (proc_type == 1) ? "2M/huge" : "4K/normal");

    /* Step 3: Register process in NVMe swap mode via SMAP_NVME_SWAP_NID sentinel */
    struct MigrateOutMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.count = 1;
    msg.payload[0].pid   = target_pid;
    msg.payload[0].count = 1;
    msg.payload[0].inner[0].destNid = SMAP_NVME_SWAP_NID;
    /* ratio/memSize/migrateMode are unused for NVMe swap mode */

    ret = ubturbo_smap_migrate_out(&msg, proc_type);
    if (ret != 0) {
        fprintf(stderr, "[test] ubturbo_smap_migrate_out failed: %d\n", ret);
        ubturbo_smap_stop();
        return 1;
    }
    printf("[test] pid %d registered in NVMe swap mode (SMAP_NVME_SWAP_NID=%d)\n",
           target_pid, SMAP_NVME_SWAP_NID);
    printf("[test] Monitoring — press Ctrl+C to stop\n\n");

    /* Step 4: Monitor VmSwap until Ctrl+C */
    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);

    while (!g_stop) {
        /* Read VmSwap from /proc/<pid>/status */
        char path[64];
        snprintf(path, sizeof(path), "/proc/%d/status", target_pid);
        FILE *f = fopen(path, "r");
        if (!f) {
            fprintf(stderr, "[test] pid %d gone, stopping.\n", target_pid);
            break;
        }
        char line[256];
        unsigned long vm_swap_kb = 0;
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "VmSwap:", 7) == 0) {
                sscanf(line + 7, "%lu", &vm_swap_kb);
                break;
            }
        }
        fclose(f);
        printf("[test] pid %d  VmSwap: %lu kB\n", target_pid, vm_swap_kb);
        sleep(MONITOR_INTERVAL_SEC);
    }

    /* Step 5: Cleanup */
    printf("\n[test] stopping — removing pid %d from SMAP\n", target_pid);

    struct RemoveMsg rmsg;
    memset(&rmsg, 0, sizeof(rmsg));
    rmsg.count = 1;
    rmsg.payload[0].pid = target_pid;
    rmsg.payload[0].count = 0;
    ubturbo_smap_remove(&rmsg, proc_type);

    ubturbo_smap_stop();
    printf("[test] done.\n");
    return 0;
}
