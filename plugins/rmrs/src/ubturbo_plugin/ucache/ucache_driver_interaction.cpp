/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * rmrs is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "ucache_driver_interaction.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "rmrs_config.h"
#include "rmrs_error.h"
#include "turbo_logger.h"

using namespace turbo::log;
using namespace rmrs;

namespace ucache {

uint32_t DriverInteraction::GetMigrateSuccess(struct MigrateSuccess &queryArg)
{
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
        << "[ucache] Get migrate success desNid: " << queryArg.nid << ".";
    if (!EnsureDevice()) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ucache] Failed to get migrate success, ucache_device is invalid.";
        return RMRS_ERROR;
    }

    if (ioctl(fd, UCACHE_QUERY_MIGRATE_SUCCESS, &queryArg)) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ucache] Failed to get migrate success." << "ERROR=" << strerror(errno) << ".";
        return RMRS_ERROR;
    }
    return RMRS_OK;
}

// 文件页迁移,迁移到的目标desNid,扫描的本段nid，容器内进程pid
uint32_t DriverInteraction::MigrateFoliosInfo(const int32_t desNid, const int32_t nid, const pid_t pid)
{
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
        << "[ucache] Migrate folios info desNid: " << desNid << ", nid=" << nid << ", pid=" << pid << ".";
    if (!EnsureDevice()) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ucache] Failed to migrate folios, ucache_device is invalid.";
        return RMRS_ERROR;
    }

    struct MigrateInfo migrateInfo = {desNid, nid, pid};
    if (ioctl(fd, UCACHE_SCAN_MIGRATE_FOLIOS, &migrateInfo) < 0) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] Failed to scan migrate folios.";
        return RMRS_ERROR;
    }
    return RMRS_OK;
}

bool DriverInteraction::EnsureDevice()
{
    struct stat fileStat;

    // 检查设备文件是否存在并获取信息
    if (stat(devicePath, &fileStat) == -1) {
        UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ucache] Failed to stat ucache_device. Error=" << strerror(errno) << ".";
        CloseDevice();
        return false;
    }

    // 如果设备信息未变化且fd有效，直接返回
    if (fd >= 0 && fileStat.st_dev == stDev && fileStat.st_ino == stIno) {
        return true;
    }

    // 关闭旧设备并尝试打开新设备
    CloseDevice();
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] Try to reopened ucache_device.";
    fd = open(devicePath, O_RDWR);
    if (fd >= 0) {
        stDev = fileStat.st_dev;
        stIno = fileStat.st_ino;
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] reopened ucache_device successfully.";
        return true;
    }
    UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
        << "[ucache] Failed to reopen ucache_device. Error=" << strerror(errno) << ".";
    return false;
}

void DriverInteraction::CloseDevice()
{
    if (fd >= 0) {
        int ret = close(fd);
        if (ret != 0) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[ucache] Failed to close ucache_device. Error=" << strerror(errno) << ".";
            return;
        }
        fd = -1;
        stDev = 0;
        stIno = 0;
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] Successfully closed ucache_device.";
        return;
    }
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] ucache_device is already closed.";
}

} // namespace ucache
