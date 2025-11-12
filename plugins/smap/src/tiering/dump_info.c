// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: SMAP3.0 dump info module
 */

#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/time.h>
#include <linux/kmod.h>

#include "smap_msg.h"
#include "dump_info.h"

#undef pr_fmt
#define pr_fmt(fmt) "smap_dump_info: " fmt

int check_and_create_dir(char *dir)
{
	struct file *fp = NULL;
	char *argv[] = {
		"/bin/mkdir",
		dir,
		NULL,
	};
	char *envp[] = {
		"HOME=/",
		"TERM=linux",
		"PATH=/sbin:/bin:/usr/sbin:/usr/bin",
		NULL,
	};
	int err;

	if (!dir) {
		return -EINVAL;
	}
	fp = filp_open(dir, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		if (PTR_ERR(fp) != -ENOENT) {
			return PTR_ERR(fp);
		}
	} else {
		filp_close(fp, NULL);
		return 0;
	}

	err = call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC);
	if (err) {
		pr_err("create %s error: %d\n", dir, err);
		return err;
	}
	pr_info("create %s succeed\n", dir);
	return 0;
}
EXPORT_SYMBOL(check_and_create_dir);

int check_filesize(char *filepath, loff_t max_sz, int *exceed)
{
	struct file *fp = NULL;
	loff_t file_sz;

	fp = filp_open(filepath, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		if (PTR_ERR(fp) == -ENOENT) {
			*exceed = 0;
			return 0;
		}
		return PTR_ERR(fp);
	}
	file_sz = i_size_read(file_inode(fp));
	filp_close(fp, NULL);
	*exceed = file_sz >= max_sz ? 1 : 0;
	return 0;
}
EXPORT_SYMBOL(check_filesize);

int rename_file(char *old_name, char *new_name)
{
	char *argv[] = { "/bin/mv", old_name, new_name, NULL };
	char *envp[] = { "HOME=/", "TERM=linux",
			 "PATH=/sbin:/bin:/usr/sbin:/usr/bin", NULL };
	int err = call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC);
	if (err) {
		pr_err("rename %s to %s error: %d\n", old_name, new_name, err);
		return err;
	}
	pr_info("rename %s to %s succeed\n", old_name, new_name);
	return 0;
}
EXPORT_SYMBOL(rename_file);

int backup_file_if_needed(char *dir, char *name, loff_t max_sz)
{
	struct timespec64 realtime;
	struct tm tm;
	char ts_str[TIME_STR_LEN] = { 0 };
	char filepath[FILENAME_LEN] = { 0 };
	char new_fn[FILENAME_LEN] = { 0 };
	int exceed;
	int err, rc;

	rc = scnprintf(filepath, sizeof(filepath), "%s/%s", dir, name);
	if (!rc) {
		pr_err("write filepath: %s/%s failed\n", dir, name);
		return -ENOMEM;
	}
	err = check_filesize(filepath, max_sz, &exceed);
	if (err) {
		pr_err("check file size ret: %d\n", err);
		return err;
	}
	if (exceed == 0) {
		return 0;
	}

	realtime = ktime_to_timespec64(ktime_get_real());
	time64_to_tm(realtime.tv_sec, TZ_OFFSET, &tm);
	rc = scnprintf(ts_str, sizeof(ts_str), "%ld%02d%02d_%02d%02d%02d",
		       tm.tm_year + EPOCH, tm.tm_mon + 1, tm.tm_mday,
		       tm.tm_hour, tm.tm_min, tm.tm_sec);
	if (!rc) {
		pr_err("write time stamp failed\n");
		return -ENOMEM;
	}
	rc = scnprintf(new_fn, sizeof(new_fn), "%s/%s-%s", dir, name, ts_str);
	if (!rc) {
		pr_err("write new file path: %s/%s/%s failed\n", dir, name,
		       ts_str);
		return -ENOMEM;
	}
	err = rename_file(filepath, new_fn);
	if (err) {
		return err;
	}
	return 0;
}
EXPORT_SYMBOL(backup_file_if_needed);

void filter_4k_migrate_info(void)
{
	pr_debug("transhuge %lu, huge %lu, nr_lru %lu, nr_zero_ref %lu\n",
		 nr_abnormal[PAGE_TYPE_TRANSHUGE], nr_abnormal[PAGE_TYPE_HUGE],
		 nr_abnormal[PAGE_TYPE_NOR_LRU],
		 nr_abnormal[PAGE_TYPE_ZERO_REF]);
}