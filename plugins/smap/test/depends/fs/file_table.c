// SPDX-License-Identifier: GPL-2.0-only
/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
* Description: file_table.c stub
*/

#include <linux/fdtable.h>

bool get_file_rcu(struct file *filp)
{
        return false;
}

struct fdtable *files_fdtable(struct files_struct *files)
{
        return NULL;
}

struct file *files_lookup_fd_rcu(struct files_struct *files, unsigned int fd)
{
        return NULL;
}
