// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: pagewalk stub.
 */

#include <linux/pagewalk.h>

int walk_page_range(struct mm_struct *mm, unsigned long start,
	unsigned long end, const struct mm_walk_ops *ops, void *private_data)
{
	return 0;
}
