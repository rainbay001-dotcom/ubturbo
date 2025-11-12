/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: smap memory notifier
 */

#ifndef _MEMORY_NOTIFIER_H
#define _MEMORY_NOTIFIER_H

extern int tracking_core_reinit_actc_buffer(int nid);
int memory_notifier_init(void);
void memory_notifier_exit(void);

#endif /* _MEMORY_NOTIFIER_H */
