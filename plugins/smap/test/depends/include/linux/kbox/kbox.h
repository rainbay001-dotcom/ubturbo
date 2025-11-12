/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2014-2019.
 * Description: main header for kbox
 */

#ifndef __KBOX_DEPENDS_H__
#define __KBOX_DEPENDS_H__

#ifdef __KERNEL__
#include <asm/types.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/rtc.h>
#endif

#define KBOX_NULL_REGION_ID 0xffff
#define KBOX_DEFAULT_REG_ID 0
#define KBOX_REGION_NAME_LEN 64
#define TMP_NAME_LEN 32

struct kbox_region {
	int rfd;
	struct module *mod;
	int size;
	char name[TMP_NAME_LEN];
};

#define KBOX_IOC_MAGIC  'k'
#define KBOX_IOC_CREATE_REGION _IOWR(KBOX_IOC_MAGIC, 1,  char)

#ifdef __KERNEL__
static inline int kbox_write(int fd, const char *buf, unsigned size) { return size; }
static inline int kbox_register_region(struct kbox_region r) { return -1; }
#endif

#endif

