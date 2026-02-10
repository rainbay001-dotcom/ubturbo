/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ub-dma manager
 */

#ifndef UB_DMA_SEGMENT_MANAGER_H
#define UB_DMA_SEGMENT_MANAGER_H

#include "urma.h"

struct urma_trans_segment_info {
    uint64_t addr;
    uint32_t len;
    struct ubcore_target_seg *sge;
};

int urma_meta_sge_init(void);

int get_urma_trans_segment(struct urma_trans_segment_info *src_info, struct urma_trans_segment_info *dst_info);

int ub_dma_register_segment(u64 pa_start, u64 pa_end);

int ub_dma_unregister_segment(u64 start_pa, u64 end_pa);

void ub_dma_unregister_all_segment(void);

#endif