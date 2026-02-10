/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ub-dma urma adapter
 */

#ifndef UB_UDMA_URMA_H
#define UB_UDMA_URMA_H

#include <linux/err.h>
#include "ubcore_uapi.h"
#include "ubcore_api.h"

#define UB_DMA_JETTY_DEPTH 1024

typedef void (*ubcore_comp_callback_t)(struct ubcore_jfc *jfc);

struct urma_sge_info {
    struct ubcore_target_seg *sge;
    struct ubcore_target_seg *i_seg;
    uint64_t start_va;
    uint64_t end_va;
};

struct ub_register_sge {
    struct ubcore_target_seg *src_seg;
    struct ubcore_target_seg *dst_seg;
    struct ubcore_target_seg *i_dst_seg;
};

struct urma_mem_trans {
    struct ubcore_device *dev;
    struct ubcore_eid_info eid_info;
    struct ubcore_cr *cr;
    struct ubcore_jfc *server_jfc;
    struct ubcore_jfc *client_jfc;
    struct ubcore_jfs *client_jfs;
    struct ubcore_jfr *server_jfr;
    struct ubcore_tjetty *tjetty;
    struct mutex mutex_lock;
};

int init_urma_mem_trans(ubcore_comp_callback_t jfce_handler);

void release_urma_mem_trans(void);

int urma_register_segment(uint64_t start_va, uint64_t end_va, struct urma_sge_info *sge_info);

void unregister_urma_segment(struct urma_sge_info *sge_info);

int urma_run_send(uint64_t src_va, uint64_t dst_va, struct ubcore_target_seg *src_seg,
    struct ubcore_target_seg *dst_seg, uint32_t len);

struct urma_mem_trans *get_urma_jetty(void);

#endif