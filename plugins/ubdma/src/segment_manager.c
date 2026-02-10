/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ub-dma manager
 */

#include <linux/kernel.h>
#include <linux/hashtable.h>

#include "ub_dma_log.h"
#include "segment_manager.h"

#define URMA_SGE_HASH_BIT 9
#define HASH_KEY_MASK_BIT 27

struct urma_sge_segment_manager {
    DECLARE_HASHTABLE(sge_meta_hashtable, URMA_SGE_HASH_BIT);
    rwlock_t sge_meta_lock;
};

struct urma_sge_meta_node {
    struct hlist_node node;
    struct urma_sge_info info;
};

static struct urma_sge_segment_manager *g_mgr;

int urma_meta_sge_init(void)
{
    g_mgr = kzalloc(sizeof(struct urma_sge_segment_manager), GFP_KERNEL);
    if (!g_mgr) {
        ub_dma_log_err("alloc urma meta segment manager failed.\n");
        return -ENOMEM;
    }
    hash_init(g_mgr->sge_meta_hashtable);
    rwlock_init(&g_mgr->sge_meta_lock);
 
    return 0;
}

static int check_mem_info(u64 pa_start, u64 pa_end)
{
    unsigned long pfn_start, pfn_end;
    int start_nid, end_nid;

    if (pa_end <= pa_start) {
        ub_dma_log_err("pa_end is less than pa_start.\n");
        return -EINVAL;
    }
    pfn_start = PHYS_PFN(pa_start);
    pfn_end = PHYS_PFN(pa_end);
    if (!pfn_valid(pfn_start) || !pfn_to_online_page(pfn_start) ||
        !pfn_valid(pfn_end) || !pfn_to_online_page(pfn_end)) {
        ub_dma_log_err("pfn start or pfn end is invaild.\n");
        return -EINVAL;
    }
    start_nid = pfn_to_nid(pfn_start);
    end_nid = pfn_to_nid(pfn_end);
    if (start_nid != end_nid) {
        ub_dma_log_err("start node id(%d) is not equal to end node id(%d).\n", start_nid, end_nid);
        return -EINVAL;
    }

    return 0;
}

static inline bool is_target_segment(u64 start_va, u64 end_va, u64 addr, u32 len)
{
    return start_va <= addr && end_va >= (addr + len - 1) ? true: false;
}

int get_urma_trans_segment(struct urma_trans_segment_info *src_info, struct urma_trans_segment_info *dst_info)
{
    struct urma_sge_meta_node *sge;
    u64 src_key = src_info->addr >> HASH_KEY_MASK_BIT;
    u64 dst_key = dst_info->addr >> HASH_KEY_MASK_BIT;
    
    write_lock(&g_mgr->sge_meta_lock);
    hash_for_each_possible(g_mgr->sge_meta_hashtable, sge, node, src_key) {
        if (sge->info.start_va <= src_info->addr && sge->info.end_va >= (src_info->addr + src_info->len - 1)) {
            src_info->sge = sge->info.sge;
            break;
        }
    }
    hash_for_each_possible(g_mgr->sge_meta_hashtable, sge, node, dst_key) {
        if (sge->info.start_va <= dst_info->addr && sge->info.end_va >= (dst_info->addr + dst_info->len - 1)) {
            dst_info->sge = sge->info.i_seg;
            break;
        }
    }
    if (!src_info->sge || !dst_info->sge) {
        write_unlock(&g_mgr->sge_meta_lock);
        ub_dma_log_err("fali to get urma trans sgemnet.\n");
        return -EINVAL;
    }

    write_unlock(&g_mgr->sge_meta_lock);
    return 0;
}

int ub_dma_register_segment(u64 pa_start, u64 pa_end)
{
    int ret;
    u64 start_va, end_va;
    u64 key;
    struct urma_sge_meta_node *sge;

    ret = check_mem_info(pa_start, pa_end);
    if (ret) {
        return ret;
    }

    sge = kzalloc(sizeof(struct urma_sge_meta_node), GFP_KERNEL);
    if (!sge) {
        ub_dma_log_err("alloc segment failed.\n");
        return -ENOMEM;
    }

    start_va = (u64)phys_to_virt(pa_start);
    end_va = (u64)phys_to_virt(pa_end);
    write_lock(&g_mgr->sge_meta_lock);
    ret = urma_register_segment(start_va, end_va, &sge->info);
    if (ret) {
        write_unlock(&g_mgr->sge_meta_lock);
        ub_dma_log_err("register segment failed.\n");
        kfree(sge);
        return ret;
    }

    sge->info.start_va = start_va;
    sge->info.end_va = end_va;
    INIT_HLIST_NODE(&sge->node);
    key =  start_va >> HASH_KEY_MASK_BIT;
    hash_add(g_mgr->sge_meta_hashtable, &sge->node, key);
    write_unlock(&g_mgr->sge_meta_lock);

    return 0;
}

int ub_dma_unregister_segment(u64 start_pa, u64 end_pa)
{
    struct urma_sge_meta_node *sge;
    struct hlist_node *tmp;
    u64 start_va, end_va;
    u64 key;

    start_va = (u64)phys_to_virt(start_pa);
    end_va = (u64)phys_to_virt(end_pa);
    key = start_va >> HASH_KEY_MASK_BIT;
    write_lock(&g_mgr->sge_meta_lock);
    hash_for_each_possible_safe(g_mgr->sge_meta_hashtable, sge, tmp, node, key) {
        unregister_urma_segment(&sge->info);
        hash_del(&sge->node);
        kfree(sge);
        write_unlock(&g_mgr->sge_meta_lock);
        return 0;
    }

    write_unlock(&g_mgr->sge_meta_lock);
    ub_dma_log_err("unregister segment failed.\n");

    return -ENOENT;
}

void ub_dma_unregister_all_segment(void)
{
    struct urma_sge_meta_node *sge;
    struct hlist_node *tmp;
    int bkt;

    write_lock(&g_mgr->sge_meta_lock);
    hash_for_each_safe(g_mgr->sge_meta_hashtable, bkt, tmp, sge, node) {
        unregister_urma_segment(&sge->info);
        hash_del(&sge->node);
        kfree(sge);
    }
    write_unlock(&g_mgr->sge_meta_lock);
    kfree(g_mgr);
    g_mgr = NULL;
}