/*
* Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
* Description: SMAP3.0 驱动模块打桩文件
*/
#ifndef __STUB_BUS_H__
#define __STUB_BUS_H__

#include <linux/device.h>

int stub_tracking_bus_probe(struct tracking_dev *dev);
void stub_tracking_device_remove(struct tracking_dev *dev);

#endif