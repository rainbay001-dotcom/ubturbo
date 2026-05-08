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
 * smap_tiered_test.c  —  SMAP v3 three-tier (L1/L2/NVMe) acceptance test
 *
 * Registers a process/VM with the v3 three-tier placement policy and
 * monitors its VmRSS + VmSwap every 10 s.  The SMAP background thread
 * drives the actual migration/swap; this tool is the control-plane only.
 *
 * Build (from repo root):
 *   gcc -o smap_tiered_test plugins/smap/test/smap_tiered_test.c \
 *       -I plugins/smap/src/user                                   \
 *       -L <path-to-dir-containing-libsmap.so> -lsmap -lpthread
 *
 * Usage:
 *   ./smap_tiered_test <scan_ms> <migrate_ms> <proc_type> <pid> <l2_ratio> <l3_ratio> [OPTIONS]
 *
 *   scan_ms    : cold-page scan period in ms  [50, 20000], multiple of 50
 *   migrate_ms : migration/swap cycle in ms   [50, 20000]
 *   proc_type  : 0 = normal process (4 K pages)   1 = VM (2 M huge pages)
 *   pid        : target process or VM PID
 *   l2_ratio   : % of process memory to place in L2 DRAM  [0, 100]
 *   l3_ratio   : % of process memory to swap to NVMe      [0, 100]
 *               (l2_ratio + l3_ratio must be <= 100; remainder stays in L1)
 *
 * Options:
 *   --l1-nid <nid>  : L1 (local DRAM) NUMA node ID  (default: 0)
 *   --l2-nid <nid>  : L2 (remote DRAM) NUMA node ID (default: 1)
 *   --l2-size <MB>  : L2 NUMA node capacity in MB   (default: 65536 = 64 GB)
 *
 * Prerequisites:
 *   - smap kernel modules loaded (smap_tracking_core, smap_histogram_tracking,
 *     smap_access_tracking smap_scene=2, smap_tiering smap_scene=2 smap_pgsize=0|1)
 *   - Swap partition active (`swapon -s` shows an entry)
 *   - Root / CAP_SYS_ADMIN
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

#include "smap_interface.h"

#define PERIOD_CONFIG_FILE "/opt/ubturbo/conf/smap/period.config"
#define MONITOR_INTERVAL_SEC 10
#define DEFAULT_L1_NID       0
#define DEFAULT_L2_NID       1
#define DEFAULT_L2_SIZE_MB   65536   /* 64 GB */

static pid_t           g_pid;
static int             g_pid_type;
static volatile int    g_stop;

static void on_signal(int sig)
{
    (void)sig;
    g_stop = 1;
}

static int write_period_config(uint32_t scan_ms, uint32_t migrate_ms)
{
    FILE *f = fopen(PERIOD_CONFIG_FILE, "w");
    if (!f) {
        fprintf(stderr, "[test] open %s: %s\n", PERIOD_CONFIG_FILE, strerror(errno));
        return -1;
    }
    fprintf(f, "smap.scan.period = %u\n",                  scan_ms);
    fprintf(f, "smap.migrate.period = %u\n",               migrate_ms);
    fprintf(f, "smap.remote.freq.percentile = 99\n");
    fprintf(f, "smap.slow.threshold = 2\n");
    fprintf(f, "smap.freq.wt = 0\n");
    fprintf(f, "smap.period.file.config.switch = true\n");
    fclose(f);
    printf("[test] period config written: scan=%u ms  migrate=%u ms\n",
           scan_ms, migrate_ms);
    return 0;
}

static void cleanup_period_config(void)
{
    remove(PERIOD_CONFIG_FILE);
}

static unsigned long read_proc_status_kb(pid_t pid, const char *field)
{
    char path[64];
    char line[256];
    unsigned long kb = 0;
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    FILE *f = fopen(path, "r");
    if (!f)
        return 0;
    size_t flen = strlen(field);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, field, flen) == 0) {
            sscanf(line + flen, "%lu", &kb);
            break;
        }
    }
    fclose(f);
    return kb;
}

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s <scan_ms> <migrate_ms> <proc_type> <pid> <l2_ratio> <l3_ratio> [OPTIONS]\n"
        "\n"
        "  scan_ms    : scan period in ms        [50, 20000], multiple of 50\n"
        "  migrate_ms : migration/swap cycle ms  [50, 20000]\n"
        "  proc_type  : 0 = process (4K)   1 = VM (2M)\n"
        "  pid        : target PID\n"
        "  l2_ratio   : %% of memory for L2 DRAM  [0, 100]\n"
        "  l3_ratio   : %% of memory for NVMe swap [0, 100]\n"
        "               (l2_ratio + l3_ratio must be <= 100)\n"
        "\n"
        "Options:\n"
        "  --l1-nid <nid>  L1 NUMA node ID          (default: %d)\n"
        "  --l2-nid <nid>  L2 NUMA node ID          (default: %d)\n"
        "  --l2-size <MB>  L2 NUMA capacity in MB   (default: %d)\n",
        prog, DEFAULT_L1_NID, DEFAULT_L2_NID, DEFAULT_L2_SIZE_MB);
}

int main(int argc, char *argv[])
{
    if (argc < 7) {
        usage(argv[0]);
        return 1;
    }

    uint32_t scan_ms    = (uint32_t)atoi(argv[1]);
    uint32_t migrate_ms = (uint32_t)atoi(argv[2]);
    int      proc_type  = atoi(argv[3]);
    pid_t    target_pid = (pid_t)atoi(argv[4]);
    int      l2_ratio   = atoi(argv[5]);
    int      l3_ratio   = atoi(argv[6]);

    int l1_nid     = DEFAULT_L1_NID;
    int l2_nid     = DEFAULT_L2_NID;
    int l2_size_mb = DEFAULT_L2_SIZE_MB;

    for (int i = 7; i < argc; i++) {
        if (strcmp(argv[i], "--l1-nid") == 0 && i + 1 < argc) {
            l1_nid = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--l2-nid") == 0 && i + 1 < argc) {
            l2_nid = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--l2-size") == 0 && i + 1 < argc) {
            l2_size_mb = atoi(argv[++i]);
        } else {
            fprintf(stderr, "[test] unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    /* Validate */
    if (scan_ms < 50 || scan_ms > 20000 || scan_ms % 50 != 0) {
        fprintf(stderr, "[test] scan_ms must be a multiple of 50 in [50, 20000]\n");
        return 1;
    }
    if (migrate_ms < 50 || migrate_ms > 20000) {
        fprintf(stderr, "[test] migrate_ms must be in [50, 20000]\n");
        return 1;
    }
    if (proc_type != 0 && proc_type != 1) {
        fprintf(stderr, "[test] proc_type must be 0 (process) or 1 (VM)\n");
        return 1;
    }
    if (kill(target_pid, 0) != 0) {
        fprintf(stderr, "[test] pid %d not found: %s\n", target_pid, strerror(errno));
        return 1;
    }
    if (l2_ratio < 0 || l2_ratio > 100 || l3_ratio < 0 || l3_ratio > 100) {
        fprintf(stderr, "[test] ratios must be in [0, 100]\n");
        return 1;
    }
    if (l2_ratio + l3_ratio > 100) {
        fprintf(stderr, "[test] l2_ratio + l3_ratio = %d exceeds 100\n",
                l2_ratio + l3_ratio);
        return 1;
    }

    g_pid      = target_pid;
    g_pid_type = (proc_type == 1) ? INPUT_VM : INPUT_PROCESS;

    /* Step 1: period config */
    if (write_period_config(scan_ms, migrate_ms) < 0)
        return 1;
    atexit(cleanup_period_config);

    /* Step 2: start SMAP */
    uint32_t page_type = (proc_type == 1) ? PAGETYPE_HUGE : PAGETYPE_NORMAL;
    int ret = ubturbo_smap_start(page_type, NULL);
    if (ret != 0) {
        fprintf(stderr, "[test] ubturbo_smap_start failed: %d\n", ret);
        return 1;
    }
    printf("[test] SMAP started  page_type=%s  l1_nid=%d  l2_nid=%d  l2_size=%d MB\n",
           (proc_type == 1) ? "2M/huge" : "4K/normal", l1_nid, l2_nid, l2_size_mb);

    /* Step 3: enable L2 NUMA node */
    struct EnableNodeMsg en = { .enable = ENABLE_NUMA_MIG, .nid = l2_nid };
    ret = ubturbo_smap_node_enable(&en);
    if (ret != 0) {
        fprintf(stderr, "[test] ubturbo_smap_node_enable(nid=%d) failed: %d\n",
                l2_nid, ret);
        ubturbo_smap_stop();
        return 1;
    }

    /*
     * Step 4: set L2 NUMA size.
     * The API expects size in MB (validated against REMOTE_NUMA_MEMORY_MAX = 1 TiB in MB).
     * srcNid=-1 means all local NUMA nodes share this remote node.
     */
    struct SetRemoteNumaInfoMsg ni = {
        .srcNid  = -1,
        .destNid = l2_nid,
        .size    = (uint64_t)l2_size_mb,   /* MB, as expected by the API */
    };
    ret = ubturbo_smap_remote_numa_info_set(&ni);
    if (ret != 0) {
        fprintf(stderr, "[test] ubturbo_smap_remote_numa_info_set failed: %d\n", ret);
        ubturbo_smap_stop();
        return 1;
    }

    /* Step 5: register process with three-tier ratios */
    struct MigrateOutMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.count = 1;
    msg.payload[0].pid       = target_pid;
    msg.payload[0].srcNid    = l1_nid;
    msg.payload[0].nvmeRatio = l3_ratio;
    if (l2_ratio > 0) {
        msg.payload[0].count              = 1;
        msg.payload[0].inner[0].destNid     = l2_nid;
        msg.payload[0].inner[0].ratio       = l2_ratio;
        msg.payload[0].inner[0].migrateMode = MIG_RATIO_MODE;
    }

    ret = ubturbo_smap_migrate_out(&msg, g_pid_type);
    if (ret != 0) {
        fprintf(stderr, "[test] ubturbo_smap_migrate_out failed: %d\n", ret);
        ubturbo_smap_stop();
        return 1;
    }
    printf("[test] pid %d registered  L1=%d%%  L2=%d%%  NVMe=%d%%\n",
           target_pid, 100 - l2_ratio - l3_ratio, l2_ratio, l3_ratio);
    printf("[test] Monitoring VmRSS + VmSwap every %d s (Ctrl-C to stop)\n\n",
           MONITOR_INTERVAL_SEC);

    /* Step 6: monitor until signal or PID disappears */
    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);

    while (!g_stop) {
        if (kill(target_pid, 0) != 0) {
            printf("[test] pid %d gone, stopping.\n", target_pid);
            break;
        }
        unsigned long rss_kb  = read_proc_status_kb(target_pid, "VmRSS:");
        unsigned long swap_kb = read_proc_status_kb(target_pid, "VmSwap:");
        printf("[test] pid %-6d  VmRSS: %8lu kB  VmSwap: %lu kB\n",
               target_pid, rss_kb, swap_kb);
        sleep(MONITOR_INTERVAL_SEC);
    }

    /* Step 7: remove process and stop */
    printf("\n[test] removing pid %d from SMAP\n", target_pid);
    struct RemoveMsg rmsg;
    memset(&rmsg, 0, sizeof(rmsg));
    rmsg.count             = 1;
    rmsg.payload[0].pid   = target_pid;
    rmsg.payload[0].count = 0;
    ubturbo_smap_remove(&rmsg, g_pid_type);

    ubturbo_smap_stop();
    printf("[test] done.\n");
    return 0;
}
