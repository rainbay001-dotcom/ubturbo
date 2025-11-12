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

#ifndef __CONFIG_FILE_H__
#define __CONFIG_FILE_H__

#include <stdint.h>

#include "manage.h"

#define NORMAL_ERR (-1)

#define SMAP_CONFIG_PATH "/dev/shm/smap_config"
#define SMAP_CONFIG_MODE (0600)

/*
 * Layout of config is as follows:
 * | VER | HEADER_LEN | TOTAL_LEN | = 2B + 2B + 4B
 * | NUMA_PAYLOAD_LEN | NUMA_PAYLOAD | = 4B + 4B * nrLocalNuma * REMOTE_NUMA_NUM
 * | PROCESS_PAYLOAD_LEN | PROCESS_PAYLOAD | = 4B + CONFIG_PROC_SIZE * N
 */
#define SMAP_CONFIG_VER 1
#define CONFIG_HEADER_LEN sizeof(struct SmapConfigHeader)
#define PAYLOAD_HEADER_LEN sizeof(struct PayloadHeader)
#define CONFIG_NUMA_LEN sizeof(struct NumaPayload)
#define CONFIG_PROC_LEN sizeof(struct ProcessPayload)

struct SmapConfigHeader {
    uint8_t ver;
    uint8_t runMode;
    uint16_t headerLen;
    uint32_t totalLen;
};

struct PayloadHeader {
    uint32_t len;
};

struct NumaPayload {
    int16_t local; // L1 NUMA ID，0-255
    int16_t remote; // L2 NUMA ID，0-255
    uint32_t size; // Available size, unit is MB
};

struct OldProcessPayload {
    pid_t pid;
    uint8_t ratio; // remote ratio set by upstream component
    uint8_t scanType;
    uint8_t type;
    uint8_t state;
    uint8_t migrateMode;
    int16_t l1Node[4];
    int16_t l2Node[4];
    uint32_t scanTime;
    uint64_t memSize;
};

struct ProcessPayload {
    pid_t pid;
    uint8_t ratio; // remote ratio set by upstream component
    uint8_t scanType;
    uint8_t type;
    uint8_t state;
    uint8_t migrateMode;
    uint32_t numaNodes;
    uint32_t scanTime;
    uint64_t memSize;
    uint32_t duration;
};

int RecoverFromConfig(void);
int SyncRunMode(RunMode runMode);
int SyncOneNumaConfig(int local, int remote, size_t size);
int SyncAllProcessConfig(void);

#endif /* __CONFIG_FILE_H__ */
