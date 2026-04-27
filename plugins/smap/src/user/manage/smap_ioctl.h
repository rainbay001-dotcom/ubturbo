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

#ifndef __SMAP_IOCTL_H__
#define __SMAP_IOCTL_H__

/*
 * cmd 0->Tracking able config:
 * arg 0->Tracking disable
 * arg 1->Tracking enable
 */

#define SMAP_DEVICE "/dev/smap_device"

#define SMAP_IOCTL_TRACKING_CMD _IOW('N', 0, unsigned long)

#define SMAP_IOCTL_MODE_SET_CMD _IOW('N', 1, unsigned long)

#define SMAP_IOCTL_MAP_MODE_CMD _IOW('N', 2, unsigned long)

#define SMAP_IOCTL_GET_SIZE_CMD _IOR('N', 3, unsigned long)

#define SMAP_IOCTL_PAGE_SIZE_SET_CMD _IOW('N', 4, unsigned long)

#define SMAP_IOCTL_RAM_CHANGE _IOW('N', 5, int)

#define SMAP_MIG_MAGIC 0xB9
#define SMAP_MIG_MIGRATE _IOW(SMAP_MIG_MAGIC, 0, struct MigrateMsg)
#define SMAP_CHECK_PAGESIZE _IOW(SMAP_MIG_MAGIC, 1, uint32_t)
#define SMAP_MIG_MIGRATE_NUMA _IOW(SMAP_MIG_MAGIC, 2, struct MigrateNumaIoctlMsg)
#define SMAP_MIG_PID_REMOTE_NUMA _IOW(SMAP_MIG_MAGIC, 3, struct MigPidRemoteNumaIoctlMsg)
#define SMAP_MIG_SWAP_OUT _IOW(SMAP_MIG_MAGIC, 4, struct MigrateMsg)

#define SMAP_MAGIC 0xBA
#define SMAP_MIGRATE_BACK _IOW(SMAP_MAGIC, 0, struct MigrateBackMsg)

#endif /* __SMAP_IOCTL_H__ */
