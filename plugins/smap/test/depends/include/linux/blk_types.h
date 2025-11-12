/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Block data types and constants.  Directly include this file only to
 * break include dependency loop.
 */
#ifndef __LINUX_BLK_TYPES_H
#define __LINUX_BLK_TYPES_H

enum stat_group {
	STAT_READ = 0,
	STAT_WRITE = 1,
	STAT_DISCARD = 2,
	STAT_FLUSH = 3,
	NR_STAT_GROUPS = 4
};

#endif
