// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: linux namei.c stub
 */
#include <linux/path.h>

int kern_path(const char *name, unsigned int flags, struct path *path)
{
    return 0;
}

void path_put(const struct path *path)
{
}
