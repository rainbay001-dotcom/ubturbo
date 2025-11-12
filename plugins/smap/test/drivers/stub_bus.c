/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: SMAP3.0 驱动模块打桩文件
 */
#include <linux/device.h>
#include "bus.h"
#include "stub_bus.h"

/* Modify the target_node attribute to verify the corresponding stub function */
int stub_tracking_bus_probe(struct tracking_dev *dev)
{
    dev->target_node = 1;
    return 0;
}

void stub_tracking_device_remove(struct tracking_dev *dev)
{
    dev->target_node = 2;
}
