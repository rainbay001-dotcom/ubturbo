/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include "linux_mock.h"

bool folio_test_hugetlb(const struct folio *folio)
{
    return false;
}

bool folio_test_dirty(const struct folio *folio)
{
    return false;
}

int migrate_pages(struct list_head *from, new_folio_t get_new_folio, free_folio_t put_new_folio, unsigned long private,
    enum migrate_mode mode, int reason, unsigned int *ret_succeeded)
{
    return 0;
}

void *idr_get_next(struct idr *idr, int *pos)
{
    return NULL;
}

void *idr_remove(struct idr *idr, unsigned long id)
{
    return NULL;
}

int hlist_empty(const struct hlist_head *h)
{
    return !h->first;
}

void page_cache_ra_unbounded(struct readahead_control *ractl, unsigned long nr_to_read, unsigned long lookahead_count)
{
    return;
}

struct task_struct *kthread_run(int (*threadfn)(void *data), void *data, const char namefmt[])
{
    return NULL;
}

bool IS_ERR_OR_NULL(void *arg)
{
    return true;
}

long strnlen_user(const char __user *str, long n)
{
    return 10;
}

bool PTR_ERR_OR_ZERO(void *arg)
{
    return true;
}

int register_kretprobe(struct kprobe *kp)
{
    return 0;
}

void unregister_kretprobe(struct kprobe *kp)
{
    return;
}

struct task_struct *get_current(void)
{
    struct task_struct cur = {
        .files = NULL,
        .mm = NULL,
    };
    return &cur;
}

int atomic_cmpxchg(volatile int *x, int y, int z)
{
    return 0;
}

int atomic_read(volatile int *x)
{
    return 0;
}

struct folio* __folio_alloc(gfp_t gfp, unsigned int order, int nid, nodemask_t *nmask)
{
    return NULL;
}
