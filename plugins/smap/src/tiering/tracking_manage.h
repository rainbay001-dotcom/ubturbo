/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: SMAP3.0 Tiering Memory Solution: SMAP TRACKING_MANAGE
 */

#ifndef _TRACKING_MANAGE_H
#define _TRACKING_MANAGE_H

#define QEMU_NAME_LEN 30

#include "numa.h"
#include "smap_migrate_pages.h"
#include "smap_debugfs.h"
#include "smap_migrate_wrapper.h"
#include "mig_init.h"

int is_smap_args_valid(void);

#endif /* _TRACKING_MANAGE_H */
