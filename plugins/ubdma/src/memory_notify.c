/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ub-dma memory notifiter
 */
#include <asm/page-def.h>
#include <linux/notifier.h>
#include <linux/memory.h>
#include <linux/mm.h>
#include <linux/workqueue.h>

#include "segment_manager.h"
#include "ub_dma_log.h"
#include "memory_notifier.h"

#define UB_DMA_MN_CALLBACK_PRI 100

#define WQ_MAX_THREADS 1

struct mem_nf_work_info {
    struct work_struct work;
    u64 start_pa;
    u64 end_pa;
};

static struct workqueue_struct *mem_nf_wq;

static void mem_nf_register_sge(struct work_struct *w __always_unused)
{
    struct mem_nf_work_info *info = container_of(w, struct mem_nf_work_info, work);

    if (ub_dma_register_segment(info->start_pa, info->end_pa)) {
        ub_dma_log_err("mem notify register segment failed\n");
    }
    kfree(info);
}

static int memory_notifier_cb(struct memory_notify *mnb, unsigned long action)
{
    int ret = 0;
    struct page *page;
    unsigned long start_pfn = mnb->start_pfn;
    unsigned long end_pfn = mnb->start_pfn + mnb->nr_pages - 1;
    u64 start_pa;
    u64 end_pa;
    struct mem_nf_work_info *info;

    if (!pfn_valid(start_pfn)) {
        ub_dma_log_err("start_pfn %#lx is invalid\n", start_pfn);
        return -EINVAL;
    }
    page = pfn_to_online_page(start_pfn);
    if (!page) {
        ub_dma_log_err("find pfn %#lx page falied\n", start_pfn);
        return -EINVAL;
    }
    start_pa = PFN_PHYS(start_pfn);
    end_pa = PFN_PHYS(end_pfn) + PAGE_SIZE - 1;
    info = kzalloc(sizeof(*info), GFP_KERNEL);
    if (!info) {
        ub_dma_log_err("alloc mem_nf_work_info failed\n");
        ret = -ENOMEM;
        return ret;
    }
    info->start_pa = start_pa;
    info->end_pa = end_pa;
    INIT_WORK(&info->work, mem_nf_register_sge);
    queue_work(mem_nf_wq, &info->work);

    return ret;
}

static void mem_nf_unregister_sge(struct work_struct *w __always_unused)
{
    struct mem_nf_work_info *info = container_of(w, struct mem_nf_work_info, work);

    if (ub_dma_unregister_segment(info->start_pa, info->end_pa)) {
        ub_dma_log_err("mem notify unregister segment failed\n");
    }

    kfree(info);
}

static int pre_offline_notifier_cb(struct memory_notify *mnb, unsigned long action)
{
    unsigned long start_pfn = mnb->start_pfn;
    unsigned long end_pfn = mnb->start_pfn + mnb->nr_pages - 1;
    u64 start_pa;
    u64 end_pa;
    struct page *page;
    struct mem_nf_work_info *info;

    page = pfn_to_online_page(start_pfn);
    if (!page) {
        ub_dma_log_err("find pfn %#lx page falied\n", start_pfn);
        return -EINVAL;
    }
    start_pa = (uint64_t)PFN_PHYS(start_pfn);
    end_pa = (uint64_t)PFN_PHYS(end_pfn) + PAGE_SIZE - 1;

    info = kzalloc(sizeof(*info), GFP_KERNEL);
    if (!info) {
        ub_dma_log_err("alloc mem_nf_work_info failed\n");
        return -ENOMEM;
    }
    info->start_pa = start_pa;
    info->end_pa = end_pa;
    INIT_WORK(&info->work, mem_nf_unregister_sge);
    queue_work(mem_nf_wq, &info->work);

    return 0;
}

static int ub_dma_memory_notifier(struct notifier_block *self, unsigned long action, void *arg)
{
    struct memory_notify *mnb = (struct memory_notify *)arg;
    int ret = 0;

    switch (action) {
        case MEM_ONLINE:
            ret = memory_notifier_cb(mnb, action);
            break;
        case MEM_GOING_OFFLINE:
            ret = pre_offline_notifier_cb(mnb, action);
        default:
            break;
    }

    ub_dma_log_info("handle memory notify ret: %d\n", ret);
    return NOTIFY_OK;
}

static struct notifier_block ub_dma_nb = {
    .notifier_call = ub_dma_memory_notifier,
    .priority = UB_DMA_MN_CALLBACK_PRI,
};

int memory_notifier_init(void)
{
    int ret;

    mem_nf_wq = alloc_workqueue("my_queue", WQ_CPU_INTENSIVE, WQ_MAX_THREADS);
    if (!mem_nf_wq) {
        ub_dma_log_err("alloc mem_nf_wq failed\n");
        ret = -ENOMEM;
    }

    ret = register_memory_notifier(&ub_dma_nb);
    if (ret) {
        ub_dma_log_err("register memory notifier failed\n");
        destroy_workqueue(mem_nf_wq);
        return ret;
    }

    return 0;
}

void memory_notifier_exit(void)
{
    destroy_workqueue(mem_nf_wq);
    unregister_memory_notifier(&ub_dma_nb);
    ub_dma_log_info("unregister memory notifier\n");
}