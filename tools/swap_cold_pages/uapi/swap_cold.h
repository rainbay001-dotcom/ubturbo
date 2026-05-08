/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * UAPI header shared between the swap_cold_driver kernel module and the
 * swap_cold_pages user-space tool.
 */
#ifndef _UAPI_SWAP_COLD_H
#define _UAPI_SWAP_COLD_H

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/ioctl.h>
#else
/*
 * Use the kernel UAPI types header from the system sysroot.  This avoids
 * re-typedef-ing __u32/__u64, which would conflict when any standard header
 * (e.g. <signal.h>) has already pulled in <asm-generic/int-ll64.h>.
 */
#include <linux/types.h>
#include <sys/ioctl.h>
#endif

/* Device node created by misc_register */
#define SWAP_COLD_DEV   "/dev/swap_cold_pages"

/* Maximum VA entries accepted in a single ioctl call */
#define SWAP_MAX_PAGES  65536U

#define SWAP_IOC_MAGIC   'C'

/**
 * struct swap_pageout_req - ioctl payload
 * @pid:      PID of the target process whose pages are to be reclaimed
 * @nr_pages: number of VA entries in the array pointed to by @vas_ptr
 * @vas_ptr:  user-space pointer (cast to u64) to an array of @nr_pages
 *            uint64_t virtual addresses; each address is the base of a
 *            2 MiB huge page in the target process
 */
struct swap_pageout_req {
	__u32 pid;
	__u32 nr_pages;
	__u64 vas_ptr;
};

#define SWAP_IOC_PAGEOUT _IOW(SWAP_IOC_MAGIC, 1, struct swap_pageout_req)

#endif /* _UAPI_SWAP_COLD_H */
