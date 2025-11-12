/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: SMAP kstrtox.h stub
 */
#ifndef _LINUX_KSTRTOX_H
#define _LINUX_KSTRTOX_H

#ifdef __cplusplus
extern "C" {
#endif

int kstrtouint(const char *s, unsigned int base, unsigned int *res);
int kstrtoint(const char *s, unsigned int base, int *res);

int kstrtoull(const char *s, unsigned int base, unsigned long long *res);

#ifdef __cplusplus
}
#endif

#endif /* _LINUX_KSTRTOX_H */
