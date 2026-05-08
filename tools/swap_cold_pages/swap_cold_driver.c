// SPDX-License-Identifier: GPL-2.0-only
/*
 * swap_cold_driver.c — kernel module for cold-page eviction via ioctl
 *
 * Accepts a PID and an array of 2 MiB huge-page VAs from user space,
 * looks up each page via get_page_from_vaddr(), stages it with
 * add_page_for_swap(), and calls reclaim_pages() to push the batch to swap.
 *
 * All three helpers are EXPORT_SYMBOL_GPL'd by mm/etmem.c (openEuler OLK-6.6).
 *
 * Build:
 *   make KERNEL_SRC=/path/to/kernel/source module
 *
 * Usage:
 *   insmod swap_cold_driver.ko
 *   # device node /dev/swap_cold_pages appears automatically
 */
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/mm.h>
#include <linux/pid.h>
#include <linux/vmalloc.h>
#include <linux/capability.h>
#include <linux/etmem.h>

#include "uapi/swap_cold.h"

/* Prototypes from mm/etmem.c — declared in <linux/etmem.h> */
extern struct page *get_page_from_vaddr(struct mm_struct *mm,
					unsigned long vaddr);
extern int add_page_for_swap(struct page *page, struct list_head *pagelist);
extern unsigned long reclaim_pages(struct list_head *folio_list,
				   bool ignore_references);

static long do_swap_pageout(struct swap_pageout_req __user *ureq)
{
	struct swap_pageout_req req;
	struct task_struct *task;
	struct mm_struct *mm;
	__u64 *vas;
	LIST_HEAD(pagelist);
	struct page *page;
	__u32 i;
	long ret = 0;

	if (copy_from_user(&req, ureq, sizeof(req)))
		return -EFAULT;

	if (req.nr_pages == 0)
		return 0;
	if (req.nr_pages > SWAP_MAX_PAGES)
		return -E2BIG;

	vas = vmalloc(array_size(req.nr_pages, sizeof(__u64)));
	if (!vas)
		return -ENOMEM;

	if (copy_from_user(vas, u64_to_user_ptr(req.vas_ptr),
			   array_size(req.nr_pages, sizeof(__u64)))) {
		ret = -EFAULT;
		goto free_vas;
	}

	rcu_read_lock();
	task = find_task_by_vpid((pid_t)req.pid);
	if (!task) {
		rcu_read_unlock();
		ret = -ESRCH;
		goto free_vas;
	}
	mm = get_task_mm(task);
	rcu_read_unlock();

	if (!mm) {
		ret = -ESRCH;
		goto free_vas;
	}

	for (i = 0; i < req.nr_pages; i++) {
		page = get_page_from_vaddr(mm, (unsigned long)vas[i]);
		if (!page)
			continue;
		add_page_for_swap(page, &pagelist);
		put_page(page);
	}

	if (!list_empty(&pagelist))
		reclaim_pages(&pagelist, false);

	mmput(mm);
free_vas:
	vfree(vas);
	return ret;
}

static long swap_cold_ioctl(struct file *file, unsigned int cmd,
			    unsigned long arg)
{
	if (cmd != SWAP_IOC_PAGEOUT)
		return -ENOTTY;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	return do_swap_pageout((struct swap_pageout_req __user *)arg);
}

static const struct file_operations swap_cold_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= swap_cold_ioctl,
};

static struct miscdevice swap_cold_dev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "swap_cold_pages",
	.fops	= &swap_cold_fops,
	.mode	= 0600,
};

static int __init swap_cold_init(void)
{
	int ret = misc_register(&swap_cold_dev);

	if (ret)
		pr_err("swap_cold: failed to register misc device: %d\n", ret);
	else
		pr_info("swap_cold: device /dev/%s registered\n",
			swap_cold_dev.name);
	return ret;
}

static void __exit swap_cold_exit(void)
{
	misc_deregister(&swap_cold_dev);
	pr_info("swap_cold: device unregistered\n");
}

module_init(swap_cold_init);
module_exit(swap_cold_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cold-page eviction via ioctl + reclaim_pages (openEuler OLK-6.6)");
MODULE_AUTHOR("ubturbo");
