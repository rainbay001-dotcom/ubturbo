// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Create: 2025-2-20
 */
#ifndef __LINUX_RWLOCK_TYPES_H
#define __LINUX_RWLOCK_TYPES_H

typedef struct {
    /* no debug version on UP */
} arch_rwlock_t;

typedef struct {
    arch_rwlock_t raw_lock;
} rwlock_t;

#endif /* __LINUX_RWLOCK_TYPES_H */