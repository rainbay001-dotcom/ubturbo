/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef __UCACHE_MIGRATE_H__
#define __UCACHE_MIGRATE_H__

#include <linux/mm_types.h>

#ifdef __cplusplus
extern "C" {
#endif
int ucache_scan_folios(int nid, pid_t pid, struct folio **folios,
		       unsigned int *nr_folios);

struct migrate_success {
	int32_t des_nid;
	uint32_t nr_folios;
	uint32_t nr_succeeded;
};

int ucache_migrate_folios(int des_nid, struct folio **folios,
			  unsigned int nr_folios);

struct migrate_success *get_migrate_success(int nid);

#ifdef __cplusplus
}
#endif

#endif
