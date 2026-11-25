// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * Description: Standalone MEXT Predictive Memory & Proactive Pageout driver
 *              (/dev/mext_ctl). Zero dependency on smap_tracking_core.ko,
 *              smap_access_tracking.ko, smap_tiering.ko, or ubturbod.
 */

#include <linux/compat.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#include "smap_migrate_wrapper.h"
#include "mext_fault_ring.h"

#undef pr_fmt
#define pr_fmt(fmt) "mext_drv: " fmt

#define MEXT_DRV_NAME  "mext_ctl"
#define MEXT_IOC_MAGIC 0xB9

/*
 * Support both ioctl numberings transparently:
 *   - (5, 6) used on standalone / feat/mext-proactive-reclaim
 *   - (6, 7) used on wsheng999/ubturbo feat/mext (d0a6e649)
 * Note: sizeof(struct mext_prefault_msg) == 24 != sizeof(struct mext_ctrl_msg) == 40,
 * so all four 32-bit _IOWR encodings are strictly distinct.
 */
#define MEXT_DRV_PREFAULT_V1 _IOWR(MEXT_IOC_MAGIC, 5, struct mext_prefault_msg)
#define MEXT_DRV_PREFAULT_V2 _IOWR(MEXT_IOC_MAGIC, 6, struct mext_prefault_msg)
#define MEXT_DRV_CTRL_V1     _IOWR(MEXT_IOC_MAGIC, 6, struct mext_ctrl_msg)
#define MEXT_DRV_CTRL_V2     _IOWR(MEXT_IOC_MAGIC, 7, struct mext_ctrl_msg)

/* Symbols referenced by mext_fault_ring.c cold pageout path */
unsigned long (*fp_reclaim_pages)(struct list_head *folio_list,
				  bool ignore_references);
bool (*fp_folio_isolate_lru)(struct folio *folio);
void (*fp_folio_putback_lru)(struct folio *folio);

static int mext_resolve_reclaim_symbols(void)
{
	unsigned long (*ksym_lookup)(const char *name) = NULL;
	struct kprobe kp = {
		.symbol_name = "kallsyms_lookup_name",
	};

	if (register_kprobe(&kp) < 0) {
		pr_warn("kallsyms_lookup_name unavailable; MEXT prefetch active, cold pageout disabled\n");
		return 0;
	}
	ksym_lookup = (unsigned long (*)(const char *))kp.addr;
	unregister_kprobe(&kp);
	if (!ksym_lookup)
		return 0;

	fp_reclaim_pages = (unsigned long (*)(struct list_head *, bool))
		ksym_lookup("reclaim_pages");
	fp_folio_isolate_lru = (bool (*)(struct folio *))
		ksym_lookup("folio_isolate_lru");
	fp_folio_putback_lru = (void (*)(struct folio *))
		ksym_lookup("folio_putback_lru");

	if (!fp_reclaim_pages || !fp_folio_isolate_lru || !fp_folio_putback_lru)
		pr_warn("reclaim symbols partial (reclaim=%p isolate=%p putback=%p); prefetch remains active\n",
			fp_reclaim_pages, fp_folio_isolate_lru,
			fp_folio_putback_lru);
	return 0;
}

static long mext_drv_ioctl(struct file *filp, unsigned int cmd,
			   unsigned long arg)
{
	void __user *argp = (void __user *)arg;

	if (_IOC_TYPE(cmd) != MEXT_IOC_MAGIC)
		return -ENOTTY;

	switch (cmd) {
	case MEXT_DRV_PREFAULT_V1:
	case MEXT_DRV_PREFAULT_V2:
		return mext_ioctl_prefault(argp);
	case MEXT_DRV_CTRL_V1:
	case MEXT_DRV_CTRL_V2:
		return mext_ioctl_ctrl(argp);
	default:
		return -ENOTTY;
	}
}

static const struct file_operations mext_drv_fops = {
	.owner          = THIS_MODULE,
	.mmap           = mext_fault_ring_mmap,
	.unlocked_ioctl = mext_drv_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = compat_ptr_ioctl,
#endif
};

static struct miscdevice mext_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = MEXT_DRV_NAME,
	.fops  = &mext_drv_fops,
	.mode  = 0600,
};

static int __init mext_drv_init(void)
{
	int ret;

	(void)mext_resolve_reclaim_symbols();

	ret = mext_fault_ring_init();
	if (ret)
		return ret;

	ret = misc_register(&mext_miscdev);
	if (ret) {
		pr_err("failed to register /dev/%s: %d\n", MEXT_DRV_NAME, ret);
		mext_fault_ring_exit();
		return ret;
	}

	pr_info("/dev/%s ready (standalone MEXT fault ring + prefault + cold pageout)\n",
		MEXT_DRV_NAME);
	return 0;
}

static void __exit mext_drv_exit(void)
{
	misc_deregister(&mext_miscdev);
	mext_fault_ring_exit();
	pr_info("/dev/%s unloaded\n", MEXT_DRV_NAME);
}

module_init(mext_drv_init);
module_exit(mext_drv_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Standalone MEXT Predictive Swap Prefetch & Proactive Reclaim Driver");
