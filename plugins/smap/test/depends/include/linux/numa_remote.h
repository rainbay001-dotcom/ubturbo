/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024  Huawei Technologies Co., Ltd.
 * Author: Liu Shixin <liushixin2@huawei.com>
 */
#ifndef _LINUX_REMOTE_MEMORY_H_
#define _LINUX_REMOTE_MEMORY_H_

static inline bool numa_is_remote_node(int nid)
{
    return false;
}

#endif /* _LINUX_REMOTE_MEMORY_H_ */
