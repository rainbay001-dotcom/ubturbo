/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ub-dma memory notifier
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/mmzone.h>
#include <linux/memory.h>
#include <linux/mm.h>
#include <linux/workqueue.h>

#include "segment_manager.h"
#include "ub_dma_log.h"
#include "ub_dma_iomem.h"

struct ram_segment {
    struct list_head node;
    u64 start;
    u64 end;
};

#define UB_DMA_MAX_NUMNODES 22

#define IOMEM_REGISTER_SGE_MS 500

LIST_HEAD(remote_ram_list);

static struct workqueue_struct *iomem_work_queue;
static struct work_struct iomem_register_urma_sge_work;

static int insert_remote_ram(u64 pa_start, u64 pa_end, struct list_head *head)
{
    struct ram_segment *seg;
    u64 start;
    u64 end;
    unsigned long pfn;
    int nid;

    start = pa_start;
    while (start < pa_end) {
        pfn = PHYS_PFN(start);
        if (!pfn_valid(pfn) || !pfn_to_online_page(pfn)) {
            start += MIN_MEMORY_BLOCK_SIZE;
            continue;
        }
        nid = page_to_nid(pfn_to_online_page(pfn));
        if (nid == NUMA_NO_NODE) {
            return -EINVAL;
        }
        end = start + MIN_MEMORY_BLOCK_SIZE - 1;
        if (nid >= UB_DMA_MAX_NUMNODES) {
            start = end + 1;
            continue;
        }
        seg = kmalloc(sizeof(*seg), GFP_KERNEL);
        if (!seg) {
            ub_dma_log_err("kalloc remote ram segment failed\n");
            return -ENOMEM;
        }
        seg->start = start;
        seg->end = end;
        list_add_tail(&seg->node, head);
        start = end + 1;
    }

    return 0;
}

static int update_resource(struct resource *r, void *arg)
{
    int ret = 0;
    struct list_head *head = (struct list_head *)arg;

    if (!r || !arg) {
        ub_dma_log_err("resource or arg is null");
        return -EINVAL;
    }

    if (r->flags & IORESOURCE_SYSRAM_DRIVER_MANAGED) {
        ret = insert_remote_ram(r->start, r->end, head);
        if (ret) {
            return ret;
        }
    }

    return ret;
}

void destory_iomem_workqueue(void)
{
    if (iomem_work_queue) {
        cancel_work_sync(&iomem_register_urma_sge_work);
        flush_workqueue(iomem_work_queue);
        destroy_workqueue(iomem_work_queue);
        iomem_work_queue = NULL;
    }
}

static void iomem_register_urma_sge_fun(struct work_struct *w __always_unused)
{
    struct ram_segment *sge, *tmp_sge;

    list_for_each_entry_safe(sge, tmp_sge, &remote_ram_list, node) {
        if (ub_dma_register_segment(sge->start, sge->end)) {
            ub_dma_log_err("iomem register segment failed.\n");
        }
        list_del(&sge->node);
        kfree(sge);
    }
}

int iomem_urma_sge_init(void)
{
    if (walk_iomem_res_desc(IORES_DESC_NONE, IORESOURCE_SYSTEM_RAM, 0, -1, &remote_ram_list, update_resource)) {
        ub_dma_log_err("scan remote ram failed.\n");
        return -ENOMEM;
    }

    iomem_work_queue = create_singlethread_workqueue("ub_dma_iomem_queue");
    if (!iomem_work_queue) {
        ub_dma_log_err("create iomem work queue failed\n");
        return -ENOMEM;
    }
    INIT_WORK(&iomem_register_urma_sge_work, iomem_register_urma_sge_fun);

    queue_work(iomem_work_queue, &iomem_register_urma_sge_work);

    return 0;
}