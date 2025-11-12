/*
* Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
* Description: SMAP3.0 驱动模块打桩文件
*/
#ifndef __STUB_CORE_H__
#define __STUB_CORE_H__

#include <linux/device.h>

void stub_tracking_enable(struct device *ldev);

void stub_tracking_disable(struct device *ldev);

int stub_tracking_read(struct device *ldev, void *buffer, u32 length);

void stub_tracking_target_node_update(struct device *ldev, u8 target_node_new);

int stub_tracking_mode_set(struct device *ldev, u8 mode);

int stub_tracking_set_page_size(struct device *ldev, u8 page_size);

int StubTrackingReinitNode(struct device *ldev);
#endif