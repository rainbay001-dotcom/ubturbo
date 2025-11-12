// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: linux notifier.h stub
 */
#ifndef _LINUX_NOTIFIER_H
#define _LINUX_NOTIFIER_H

// Declare the notifier_block structure
struct notifier_block;

// Define a function pointer type notifier_fn_t for notification callback functions
typedef int (*notifier_fn_t)(struct notifier_block *nb, unsigned long action, void *data);

// Define the notifier_block structure for implementing a linked list notification mechanism
struct notifier_block {
        // notifier_call is a function pointer to the specific callback function
        notifier_fn_t notifier_call;

        // next pointer to link to the next notifier_block structure, forming a linked list
        struct notifier_block *next;

        // priority indicates the priority of this notifier_block
        int priority;
};
#define NOTIFY_DONE 0x0000
#define NOTIFY_OK 0x0001
#define NOTIFY_STOP_MASK 0x8000
#define NOTIFY_STOP             (NOTIFY_OK|NOTIFY_STOP_MASK)
#define NOTIFY_BAD (NOTIFY_STOP_MASK|0x0002)

#endif
