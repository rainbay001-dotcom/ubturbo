/*
* Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
* Description: SMAP3.0 驱动模块打桩文件
*/
#include <linux/device.h>
#include "bus.h"
#include "stub_core.h"

/* Modify the numa_node attribute to verify the corresponding stub function */
void stub_tracking_enable(struct device *ldev)
{
    ldev->numa_node = 1;
}

void stub_tracking_disable(struct device *ldev)
{
    ldev->numa_node = 2;
}

int stub_tracking_read(struct device *ldev, void *buffer, u32 length)
{
    ldev->numa_node = 5;
    return 1;
}

void stub_tracking_target_node_update(struct device *ldev,
                                      u8 target_node_new)
{
    ldev->numa_node = 7;
}

int stub_tracking_mode_set(struct device *ldev, u8 mode)
{
    ldev->numa_node = 4;
    return 1;
}

int stub_tracking_set_page_size(struct device *ldev, u8 page_size)
{
    ldev->numa_node = 6;
    return 1;
}

int StubTrackingReinitNode(struct device *ldev)
{
    return 0;
}

int stub_tracking_ram_change(struct device *ldev, void __user *argp)
{
    return 0;
}

int stub_tracking_acpi_len_get(struct device *ldev, void __user *argp)
{
    return 0;
}

int stub_tracking_numa_len_get(struct device *ldev, void __user *argp)
{
    return 0;
}

int stub_tracking_iomem_len_get(struct device *ldev, void __user *argp)
{
    return 0;
}

int stub_tracking_acpi_mem_get(struct device *ldev, void __user *argp)
{
    return 0;
}

int stub_tracking_numa_get(struct device *ldev, void __user *argp)
{
    return 0;
}

int stub_tracking_iomem_get(struct device *ldev, void __user *argp)
{
    return 0;
}
