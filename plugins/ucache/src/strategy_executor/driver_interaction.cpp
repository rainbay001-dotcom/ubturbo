/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * ucache is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "driver_interaction.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstring>
#include <sys/stat.h>
#include <cerrno>
#include <string>
#include <vector>
#include "ucache_turbo_config.h"
#include "ucache_turbo_error.h"

using namespace turbo::log;

namespace turbo::ucache {

// 文件页迁移,迁移到的目标desNid,扫描的本段nid
uint32_t DriverInteraction::MigrateFoliosInfo(const int32_t desNid, const int32_t nid, const pid_t pid)
{
    if (!EnsureDevice()) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Failed to migrate folios, " << devicePath << " is invalid";
        return UCACHE_ERR;
    }

    struct MigrateInfo migrateInfo = {desNid, nid, pid};
    if (ioctl(fd, UCACHE_SCAN__MIGRATE_FOLIOS, &migrateInfo) < 0) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Failed to scan migrate folios";
        return UCACHE_ERR;
    }
    return UCACHE_OK;
}

bool DriverInteraction::EnsureDevice()
{
    struct stat fileStat;

    // 检查设备文件是否存在并获取信息
    if (stat(devicePath, &fileStat) == -1) {
        UBTURBO_LOG_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Failed to stat device: " << devicePath << ". err is " << strerror(errno);
        CloseDevice();
        return false;
    }

    // 如果设备信息未变化且fd有效，直接返回
    if (fd >= 0 && fileStat.st_dev == stDev && fileStat.st_ino == stIno) {
        return true;
    }

    // 关闭旧设备并尝试打开新设备
    CloseDevice();
    UBTURBO_LOG_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Try to reopened device: " << devicePath;
    fd = open(devicePath, O_RDWR);
    if (fd >= 0) {
        stDev = fileStat.st_dev;
        stIno = fileStat.st_ino;
        UBTURBO_LOG_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Device: " << devicePath << "reopened successfully, st_dev is: " << stDev << ", st_ino is: " << stIno;
        return true;
    }
    UBTURBO_LOG_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
        << "Device: " << devicePath << "failed to reopen, err is " << strerror(errno);
    return false;
}

void DriverInteraction::CloseDevice()
{
    if (fd >= 0) {
        close(fd);
        fd = -1;
        stDev = 0;
        stIno = 0;
    }
}

} // namespace turbo::ucache
