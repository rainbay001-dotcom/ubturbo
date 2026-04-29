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
 * 4K 虚机迁移能力验证脚本
 *
 * 验证目标：通过 libsmap.so 公开接口，验证 SMAP 对 4K 页虚机（stage-2 页表扫描）
 *            的冷热感知与内存迁移能力。
 *
 * 前提条件：
 *   1. 内核模块已加载，且 smap_mode=1（VM_MODE，默认值）：
 *        insmod ubturbo_tiering.ko smap_mode=1
 *      内核模块 smap_pgsize 设置为 0（NORMAL_PAGE，4K，默认值）：
 *        insmod ubturbo_access.ko smap_pgsize=0
 *   2. 目标虚机使用 4K 页（非 hugetlbfs 大页），正在运行中
 *   3. 远端 NUMA 节点已配置并可用
 *
 * 编译示例（安装 libsmap.so 及头文件后）：
 *   gcc -o test_4k_vm_migration test_4k_vm_migration.c \
 *       -I/usr/include/smap -lsmap -lpthread
 *
 * 用法：
 *   ./test_4k_vm_migration <qemu_pid> <remote_nid> [ratio]
 *     qemu_pid:   4K 虚机对应的 QEMU 进程 PID
 *     remote_nid: 目标远端 NUMA 节点 ID
 *     ratio:      迁移比例（0-100），默认 25
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "smap_interface.h"

/* 频次查询缓冲区条目数，覆盖约 256MB 内存（4K 页）*/
#define FREQ_BUF_ENTRIES  65536U
#define DEFAULT_RATIO     25
/* 等待至少一个扫描周期完成，单位 ms */
#define SCAN_WAIT_MS      3000

static void smap_log_fn(int level, const char *str, const char *module)
{
    printf("[SMAP][%d][%s] %s\n", level, module ? module : "?", str ? str : "");
}

static void print_freq_stats(const uint16_t *data, uint32_t len)
{
    uint64_t hot = 0;
    for (uint32_t i = 0; i < len; i++) {
        if (data[i] > 0)
            hot++;
    }
    uint64_t cold = len - hot;
    double pct = len ? (double)hot / len * 100.0 : 0.0;
    printf("  queried pages : %u\n", len);
    printf("  hot  (freq>0) : %lu  (%.1f%%)\n", hot, pct);
    printf("  cold (freq=0) : %lu  (%.1f%%)\n", cold, 100.0 - pct);
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <qemu_pid> <remote_nid> [ratio]\n", argv[0]);
        return 1;
    }

    pid_t qemu_pid  = (pid_t)atoi(argv[1]);
    int   remote_nid = atoi(argv[2]);
    int   ratio      = (argc >= 4) ? atoi(argv[3]) : DEFAULT_RATIO;

    if (qemu_pid <= 0 || remote_nid < 0 || ratio < 0 || ratio > 100) {
        fprintf(stderr, "Invalid arguments: pid=%d nid=%d ratio=%d\n",
                qemu_pid, remote_nid, ratio);
        return -EINVAL;
    }

    printf("=== 4K VM Migration Test ===\n");
    printf("QEMU PID   : %d\n", qemu_pid);
    printf("Remote NID : %d\n", remote_nid);
    printf("Ratio      : %d%%\n\n", ratio);

    int ret = 0;

    /* Step 1: 初始化 SMAP，指定 4K 页模式（PAGETYPE_NORMAL = 0）*/
    printf("[1] ubturbo_smap_start(PAGETYPE_NORMAL) ...\n");
    ret = ubturbo_smap_start(PAGETYPE_NORMAL, smap_log_fn);
    if (ret != 0) {
        fprintf(stderr, "    FAILED: ret=%d (errno=%d)\n", ret, errno);
        return ret;
    }
    printf("    OK\n\n");

    /* Step 2: 使能目标远端 NUMA 节点 */
    printf("[2] ubturbo_smap_node_enable(nid=%d, enable=1) ...\n", remote_nid);
    struct EnableNodeMsg enable_msg = {
        .enable = ENABLE_NUMA_MIG,
        .nid    = remote_nid,
    };
    ret = ubturbo_smap_node_enable(&enable_msg);
    if (ret != 0) {
        fprintf(stderr, "    FAILED: ret=%d\n", ret);
        goto stop;
    }
    printf("    OK\n\n");

    /* Step 3: 提交 4K 虚机迁移请求
     *
     * pidType = PAGETYPE_NORMAL (0)：告知 SMAP 当前为 4K 页模式。
     * 内核侧将通过 stage-2 页表（而非 QEMU 进程自身页表）完成访问频次扫描，
     * 识别虚机内存冷热分布后，执行物理页迁移至远端 NUMA。
     */
    printf("[3] ubturbo_smap_migrate_out(pid=%d, nid=%d, ratio=%d%%) ...\n",
           qemu_pid, remote_nid, ratio);
    struct MigrateOutMsg mig_msg;
    memset(&mig_msg, 0, sizeof(mig_msg));
    mig_msg.count                       = 1;
    mig_msg.payload[0].srcNid           = -1;           /* 不指定迁出源节点 */
    mig_msg.payload[0].pid              = qemu_pid;
    mig_msg.payload[0].count            = 1;
    mig_msg.payload[0].inner[0].destNid     = remote_nid;
    mig_msg.payload[0].inner[0].ratio       = ratio;
    mig_msg.payload[0].inner[0].migrateMode = MIG_RATIO_MODE;

    ret = ubturbo_smap_migrate_out(&mig_msg, PAGETYPE_NORMAL);
    if (ret != 0) {
        fprintf(stderr, "    FAILED: ret=%d\n", ret);
        goto remove;
    }
    printf("    OK\n\n");

    /* Step 4: 等待扫描周期完成后再查询 */
    printf("[4] Waiting %d ms for scan cycle ...\n\n", SCAN_WAIT_MS);
    usleep((useconds_t)SCAN_WAIT_MS * 1000U);

    /* Step 5: 查询 4K 页冷热频次数据
     *
     * 每个 uint16_t 对应一个 4K 页的访问计数。
     * FREQ_BUF_ENTRIES 控制单次查询上限，超出部分被截断（lengthOut < lengthIn）。
     */
    printf("[5] ubturbo_smap_freq_query(pid=%d, entries=%u) ...\n",
           qemu_pid, FREQ_BUF_ENTRIES);
    uint16_t *freq_buf = calloc(FREQ_BUF_ENTRIES, sizeof(uint16_t));
    if (!freq_buf) {
        fprintf(stderr, "    calloc failed\n");
        ret = -ENOMEM;
        goto remove;
    }
    uint32_t out_len = 0;
    int freq_ret = ubturbo_smap_freq_query(qemu_pid, freq_buf,
                                            FREQ_BUF_ENTRIES, &out_len,
                                            NORMAL_DATA_SOURCE);
    if (freq_ret != 0) {
        fprintf(stderr, "    FAILED: ret=%d\n", freq_ret);
    } else {
        printf("    OK\n");
        print_freq_stats(freq_buf, out_len);
    }
    free(freq_buf);
    printf("\n");

remove:
    /* Step 6: 移除虚机的迁移跟踪 */
    printf("[6] ubturbo_smap_remove(pid=%d) ...\n", qemu_pid);
    struct RemoveMsg rm_msg;
    memset(&rm_msg, 0, sizeof(rm_msg));
    rm_msg.count                  = 1;
    rm_msg.payload[0].pid         = qemu_pid;
    rm_msg.payload[0].count       = 1;
    rm_msg.payload[0].nid[0]      = remote_nid;
    int rm_ret = ubturbo_smap_remove(&rm_msg, PAGETYPE_NORMAL);
    if (rm_ret != 0)
        fprintf(stderr, "    FAILED: ret=%d\n", rm_ret);
    else
        printf("    OK\n");
    printf("\n");

stop:
    /* Step 7: 停止 SMAP，释放资源 */
    printf("[7] ubturbo_smap_stop() ...\n");
    int stop_ret = ubturbo_smap_stop();
    if (stop_ret != 0)
        fprintf(stderr, "    FAILED: ret=%d\n", stop_ret);
    else
        printf("    OK\n");

    printf("\n=== Test %s ===\n", (ret == 0) ? "PASSED" : "FAILED");
    return ret;
}
