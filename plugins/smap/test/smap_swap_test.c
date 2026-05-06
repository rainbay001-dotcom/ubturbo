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
 *   ./smap_swap_test <scan_ms> <migrate_ms> <proc_type> <pid> [cold_threshold]
 *
 *   scan_ms         - per-process cold-page scan period in ms [50, 2000], multiple of 50
 *   migrate_ms      - global swap cycle period in ms [50, 2000]
 *   proc_type       - 0 = normal process (4K pages), 1 = VM (2M pages)
 *   pid             - PID of the target process to manage
 *   cold_threshold  - consecutive zero-freq windows before swap [1, 255], default 5
 *
 * Press Ctrl+C to stop; the managed PID is removed before exit.
 *
 * Prerequisites:
 *   - SMAP kernel module loaded (smap_tiering.ko with smap_scene=2)
 *   - NVMe swap partition active (swapon already done externally)
 *   - Sufficient privilege (root or CAP_SYS_ADMIN)
 *
 * What to observe:
 *   - "pid X swapped N pages to NVMe" in SMAP log
 *   - Growth in VmSwap field of /proc/<pid>/status
 *   - swap usage via `swapon --show` or `cat /proc/swaps`
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>

#include "smap_interface.h"
#include "smap_env.h"

#define PERIOD_CONFIG_DIR  "/opt/ubturbo/conf/smap"
#define PERIOD_CONFIG_FILE "/opt/ubturbo/conf/smap/period.config"

#define DEFAULT_COLD_THRESHOLD 5

static volatile int g_running = 1;
static pid_t        g_target_pid = -1;
static int          g_pid_input_type = INPUT_PROCESS;

static void on_signal(int signo)
{
    (void)signo;
    g_running = 0;
}

static int write_period_config(uint32_t scan_ms, uint32_t migrate_ms,
                                uint32_t cold_threshold)
{
    if (mkdir(PERIOD_CONFIG_DIR, 0755) < 0 && errno != EEXIST) {
        fprintf(stderr, "[test] mkdir %s: %s\n", PERIOD_CONFIG_DIR, strerror(errno));
        return -1;
    }

    FILE *f = fopen(PERIOD_CONFIG_FILE, "w");
    if (!f) {
        fprintf(stderr, "[test] fopen %s: %s\n", PERIOD_CONFIG_FILE, strerror(errno));
        return -1;
    }

    fprintf(f, "smap.scan.period = %u\n",              scan_ms);
    fprintf(f, "smap.migrate.period = %u\n",           migrate_ms);
    fprintf(f, "smap.remote.freq.percentile = 99\n");
    fprintf(f, "smap.slow.threshold = 2\n");
    fprintf(f, "smap.freq.wt = 0\n");
    fprintf(f, "smap.period.file.config.switch = true\n");
    fprintf(f, "smap.swap.cold.threshold = %u\n",      cold_threshold);

    fclose(f);
    printf("[test] period config written: scan=%ums  migrate=%ums  cold_threshold=%u\n",
           scan_ms, migrate_ms, cold_threshold);
    return 0;
}

static void cleanup_period_config(void)
{
    remove(PERIOD_CONFIG_FILE);
}

static void print_vm_swap(pid_t pid)
{
    char path[64];
    char line[256];

    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    FILE *f = fopen(path, "r");
    if (!f) {
        return;
    }
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "VmSwap:", 7) == 0) {
            printf("[test] pid %d  %s", pid, line);
            break;
        }
    }
    fclose(f);
}

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s <scan_ms> <migrate_ms> <proc_type> <pid> [cold_threshold]\n"
        "\n"
        "  scan_ms        : cold-page scan period in ms  [50, 2000], multiple of 50\n"
        "  migrate_ms     : swap cycle period in ms      [50, 2000]\n"
        "  proc_type      : 0 = normal process (4K)   1 = VM (2M)\n"
        "  pid            : target process PID\n"
        "  cold_threshold : consecutive zero-freq windows before swap [1, 255], default %d\n",
        prog, DEFAULT_COLD_THRESHOLD);
}

int main(int argc, char *argv[])
{
    if (argc < 5 || argc > 6) {
        usage(argv[0]);
        return 1;
    }

    uint32_t scan_ms       = (uint32_t)atoi(argv[1]);
    uint32_t migrate_ms    = (uint32_t)atoi(argv[2]);
    int      proc_type     = atoi(argv[3]);
    pid_t    target_pid    = (pid_t)atoi(argv[4]);
    uint32_t cold_threshold = (argc == 6) ? (uint32_t)atoi(argv[5]) : DEFAULT_COLD_THRESHOLD;

    if (scan_ms < 50 || scan_ms > 2000 || scan_ms % 50 != 0) {
        fprintf(stderr, "[test] scan_ms must be a multiple of 50 in [50, 2000]\n");
        return 1;
    }
    if (migrate_ms < 50 || migrate_ms > 2000) {
        fprintf(stderr, "[test] migrate_ms must be in [50, 2000]\n");
        return 1;
    }
    if (proc_type != 0 && proc_type != 1) {
        fprintf(stderr, "[test] proc_type must be 0 (normal) or 1 (VM)\n");
        return 1;
    }
    if (kill(target_pid, 0) < 0) {
        fprintf(stderr, "[test] pid %d invalid: %s\n", target_pid, strerror(errno));
        return 1;
    }
    if (cold_threshold < 1 || cold_threshold > 255) {
        fprintf(stderr, "[test] cold_threshold must be in [1, 255]\n");
        return 1;
    }

    g_target_pid     = target_pid;
    g_pid_input_type = (proc_type == 0) ? INPUT_PROCESS : INPUT_VM;

    /* Step 1: Write period config */
    if (write_period_config(scan_ms, migrate_ms, cold_threshold) < 0) {
        return 1;
    }
    atexit(cleanup_period_config);

    /* Step 2: Initialize SMAP */
    uint32_t page_type = (proc_type == 1) ? PAGETYPE_HUGE : PAGETYPE_NORMAL;
    int ret = ubturbo_smap_start(page_type, NULL);
    if (ret != 0) {
        fprintf(stderr, "[test] ubturbo_smap_start failed: %d\n", ret);
        return 1;
    }
    printf("[test] SMAP started  page_type=%s\n",
           page_type == PAGETYPE_HUGE ? "2M/huge (VM)" : "4K/normal");

    /* Step 3: Register signal handler */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* Step 4: Register process in swapMode via ubturbo_smap_migrate_out.
     *
     * Using SMAP_NVME_SWAP_NID as destNid signals the SMAP library to:
     *   - Use NORMAL_SCAN (fills actcData for cold-page tracking)
     *   - Set swapMode=true (skip L1->L2 migration in Phase 1)
     *   - Track L1 NUMA nodes directly for swap candidates
     *   - Trigger SMAP_MIG_SWAP_OUT ioctl in Phase 2 each cycle
     */
    struct MigrateOutMsg mig_msg;
    memset(&mig_msg, 0, sizeof(mig_msg));
    mig_msg.count = 1;
    mig_msg.payload[0].pid   = target_pid;
    mig_msg.payload[0].count = 1;
    mig_msg.payload[0].inner[0].destNid = SMAP_NVME_SWAP_NID;

    ret = ubturbo_smap_migrate_out(&mig_msg, g_pid_input_type);
    if (ret != 0) {
        fprintf(stderr, "[test] ubturbo_smap_migrate_out failed: %d\n", ret);
        ubturbo_smap_stop();
        return 1;
    }
    printf("[test] pid %d registered in NVMe swap mode (SMAP_NVME_SWAP_NID=%d)\n",
           target_pid, SMAP_NVME_SWAP_NID);

    printf("[test] Monitoring — press Ctrl+C to stop\n\n");

    /* Step 5: Poll until interrupted, print VmSwap every 10s */
    int tick = 0;
    while (g_running) {
        sleep(1);
        tick++;
        if (tick % 10 == 0) {
            print_vm_swap(target_pid);
        }
    }

    /* Step 6: Cleanup */
    printf("\n[test] Shutting down...\n");

    struct RemoveMsg remove_msg;
    memset(&remove_msg, 0, sizeof(remove_msg));
    remove_msg.count          = 1;
    remove_msg.payload[0].pid = target_pid;
    remove_msg.payload[0].count = 0;

    ret = ubturbo_smap_remove(&remove_msg, g_pid_input_type);
    if (ret != 0) {
        fprintf(stderr, "[test] ubturbo_smap_remove failed: %d\n", ret);
    } else {
        printf("[test] pid %d removed\n", target_pid);
    }

    print_vm_swap(target_pid);

    ret = ubturbo_smap_stop();
    if (ret != 0) {
        fprintf(stderr, "[test] ubturbo_smap_stop failed: %d\n", ret);
    } else {
        printf("[test] SMAP stopped\n");
    }

    return 0;
}
