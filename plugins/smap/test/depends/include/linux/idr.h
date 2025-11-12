// SPDX-License-Identifier: GPL-2.0-only
/*
* Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
* Description: SMAP3.0 Tiering Memory Solution: tracking_dev
*/

#ifndef TEST_IDR_H
#define TEST_IDR_H

struct ida {
};

#define IDA_INIT(name)	{}
#define DEFINE_IDA(name)	struct ida name = IDA_INIT(name)

void ida_free(struct ida *, unsigned int id);

#define ida_simple_get(ida, start, end, gfp)	\
			ida_alloc_range(ida, start, (end) - 1, gfp)

#define ida_simple_remove(ida, id)	ida_free(ida, id)

void ida_destroy(struct ida *ida);

#endif // TEST_IDR_H
