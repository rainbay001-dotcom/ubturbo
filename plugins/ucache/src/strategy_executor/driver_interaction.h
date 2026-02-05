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

#ifndef DRIVER_INTERACTION_H
#define DRIVER_INTERACTION_H

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdint>
#include <string>
#include <vector>

namespace turbo::ucache {

struct MigrateInfo {
    int32_t desNid;
    int32_t nid;
    int32_t pid;
};

#define UCACHE_SCAN__MIGRATE_FOLIOS _IOWR(100, 1, struct MigrateInfo *) // 迁移文件页

class DriverInteraction {
public:
    static DriverInteraction &GetInstance()
    {
        static DriverInteraction driverInteraction;
        return driverInteraction;
    }

    uint32_t MigrateFoliosInfo(const int32_t desNid, const int32_t nid, const pid_t pid);

    DriverInteraction(const DriverInteraction &) = delete;
    DriverInteraction &operator=(const DriverInteraction &) = delete;

private:
    int32_t fd;
    dev_t stDev;
    ino_t stIno;
    static constexpr const char *devicePath = "/dev/ucache";

    void CloseDevice();
    bool EnsureDevice();

    DriverInteraction() : fd(-1), stDev(0), stIno(0)
    {
        EnsureDevice();
    }

    ~DriverInteraction()
    {
        CloseDevice();
    }
};
} // namespace turbo::ucache

#endif /* DRIVER_INTERACTION_H */
