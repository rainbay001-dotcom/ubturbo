// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * Description: memory stub
 */

#include <linux/mm.h>

unsigned long copy_from_user(void *to, const void *from, unsigned long n)
{
	return 0;
}

unsigned long copy_to_user(void *to, const void *from, unsigned long n)
{
	return 0;
}

bool get_page_unless_zero(struct page *page)
{
	return true;
}

bool __folio_test_movable(struct folio *folio)
{
	return true;
}

void put_page(struct page *page)
{
}

void folio_put(struct folio *folio)
{
}

unsigned long compound_nr(struct page *page)
{
	return 0;
}

int __PageMovable(struct page *page)
{
	return 0;
}

unsigned long page_to_pfn(struct page *page)
{
	return 0;
}

bool PageAnon(struct page *page)
{
        return true;
}
