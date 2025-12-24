// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: smap access ioctl module
 */

#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/list.h>

#include "access_acpi_mem.h"
#include "access_iomem.h"
#include "check.h"
#include "access_tracking.h"
#include "access_pid.h"
#include "access_ioctl.h"

#undef pr_fmt
#define pr_fmt(fmt) "access_ioctl: " fmt
#define MAX_NR_MIGOUT 40
#define MAX_NR_REMOVE MAX_NR_MIGOUT

static dev_t ioctl_access_dev;
static struct class *access_class;
static struct cdev access_cdev;
static struct device *access_device;

static int check_msg_validity(struct access_add_pid_msg *msg)
{
	if (!msg) {
		pr_err("null pid message passed to access tracking\n");
		return -EINVAL;
	}
	if (msg->count <= 0 || msg->count > MAX_NR_MIGOUT) {
		pr_err("invalid message count passed to access tracking\n");
		return -EINVAL;
	}
	if (!msg->payload) {
		pr_err("null payload passed to access tracking\n");
		return -EINVAL;
	}
	return 0;
}

static int add_payload(int len, struct access_add_pid_payload *payload,
		       int page_size)
{
	int ret;
	ret = access_add_ham_pid(len, payload);
	if (ret) {
		pr_err("failed to add HAM pid tracking task, ret: %d\n", ret);
		return ret;
	}
	ret = access_add_statistic_pid(len, payload, page_size);
	if (ret) {
		pr_err("failed to add statistic pid tracking task, ret: %d\n",
		       ret);
		return ret;
	}
	ret = access_add_pid(len, payload);
	return ret;
}

static long ioctl_add_pid(void __user *argp)
{
	int ret = 0, i = 0;
	struct access_add_pid_msg msg;
	struct access_add_pid_payload *payload;
	int page_size = get_page_size(get_first_access_dev());

	if (copy_from_user(&msg, argp, sizeof(msg)))
		return -EFAULT;
	if (check_msg_validity(&msg)) {
		pr_err("message of add pid is invalid\n");
		return -EINVAL;
	}
	payload = vzalloc(sizeof(struct access_add_pid_payload) * msg.count);
	if (!payload) {
		pr_err("unable to allocate memory for access pid payload\n");
		return -ENOMEM;
	}
	if (copy_from_user(payload, msg.payload,
			   sizeof(struct access_add_pid_payload) * msg.count)) {
		ret = -EFAULT;
		goto out_free_payload;
	}

	pr_info("adding pid payload:\n");
	for (i = 0; i < msg.count; i++) {
		pr_info("[%d] pid %d, numa_nodes %#x, scan_time %u, ntimes %u, duration %u, type %d\n",
			i, payload[i].pid, payload[i].numa_nodes,
			payload[i].scan_time, payload[i].ntimes,
			payload[i].duration, payload[i].type);
		if (payload[i].type >= MAX_SCAN_TYPE || payload[i].type < 0) {
			pr_err("invalid scan type %d of message payload[%d]\n",
			       payload[i].type, i);
			ret = -EINVAL;
			goto out_free_payload;
		}
		if (payload[i].ntimes == 0) {
			pr_err("invalid scan times %d of message payload[%d]\n",
			       payload[i].ntimes, i);
			ret = -EINVAL;
			goto out_free_payload;
		}
	}
	ret = add_payload(msg.count, payload, page_size);
#ifdef DEBUG
	print_access_pid_list();
	print_access_ham_pid_list();
	print_access_statistic_pid_list();
#endif
out_free_payload:
	vfree(payload);
	return ret;
}

static long ioctl_remove_pid(void __user *argp)
{
	int i;
	struct access_remove_pid_msg msg;
	struct access_remove_pid_payload *payload;
	if (copy_from_user(&msg, argp, sizeof(msg)))
		return -EFAULT;
	if (msg.count <= 0 || msg.count > MAX_NR_REMOVE)
		return -EINVAL;
	if (!msg.payload) {
		pr_err("null payload passed to access remove pid\n");
		return -EINVAL;
	}
	payload = vzalloc(sizeof(struct access_remove_pid_payload) * msg.count);
	if (!payload) {
		pr_err("unable to allocate memory for access pid payload\n");
		return -ENOMEM;
	}
	if (copy_from_user(payload, msg.payload,
			   sizeof(struct access_remove_pid_payload) *
				   msg.count)) {
		vfree(payload);
		return -EFAULT;
	}
	pr_info("remove pid payload\n");
	for (i = 0; i < msg.count; i++)
		pr_info("[%d] pid %d\n", i, payload[i].pid);

	access_remove_pid(msg.count, payload);
	access_remove_ham_pid(msg.count, payload);
	access_remove_statistic_pid(msg.count, payload);
#ifdef DEBUG
	print_access_pid_list();
#endif
	vfree(payload);
	return 0;
}

static long ioctl_remove_all_pid(void __user *argp)
{
	access_remove_all_pid();
#ifdef DEBUG
	print_access_pid_list();
#endif
	return 0;
}

static bool is_pid_freq_msg_valid(struct access_pid_freq_msg *msg)
{
	int i;

	if (find_access_pid(msg->pid) == NULL) {
		return false;
	}
	for (i = 0; i < ARRAY_SIZE(msg->len); i++) {
		if (msg->len[i] > MAX_NR_PAGE_PER_NUMA)
			return false;
	}

	return true;
}

static int transfer_frequency_data(struct access_pid_freq_msg *msg,
				   actc_t **data)
{
	int i;
	for (i = 0; i < SMAP_MAX_NUMNODES; i++) {
		if (msg->len[i] == 0)
			continue;
		if (copy_to_user(msg->freq[i], data[i],
				 sizeof(actc_t) * msg->len[i])) {
			pr_err("failed to copy frequency of pid: %d on node%d to user\n",
			       msg->pid, i);
			return -EFAULT;
		}
	}
	return 0;
}

static long ioctl_read_pid_freq(void __user *argp)
{
	int ret;
	int i;
	struct access_pid_freq_msg msg;
	actc_t *freq[SMAP_MAX_NUMNODES] = { 0 };

	pr_debug("read pid frequency\n");
	if (!check_and_clear_ap_state(&ap_data, AP_STATE_FREQ)) {
		pr_err("read frequency of access pid is not allowed\n");
		return -EAGAIN;
	}

	if (copy_from_user(&msg, argp, sizeof(msg))) {
		ret = -EFAULT;
		goto out_ret;
	}

	if (!is_pid_freq_msg_valid(&msg)) {
		pr_err("invalid pid: %d passed to access read frequency\n",
		       msg.pid);
		ret = -EINVAL;
		goto out_ret;
	}
	for (i = 0; i < SMAP_MAX_NUMNODES; i++) {
		if (msg.len[i] == 0) {
			continue;
		}
		freq[i] = vzalloc(msg.len[i] * sizeof(actc_t));
		if (!freq[i]) {
			ret = -ENOMEM;
			goto out;
		}
	}

	ret = read_pid_freq(msg.pid, msg.len, freq);
	if (ret) {
		pr_err("failed to read frequency of pid: %d\n", msg.pid);
		goto out;
	}

	ret = transfer_frequency_data(&msg, freq);
out:
	for (i = 0; i < ARRAY_SIZE(freq); i++)
		vfree(freq[i]);
out_ret:
	if (ret)
		set_ap_whole_state(&ap_data, AP_STATE_WALK | AP_STATE_READ |
						     AP_STATE_FREQ);
	else
		set_ap_whole_state(&ap_data, AP_STATE_WALK | AP_STATE_READ |
						     AP_STATE_FREQ |
						     AP_STATE_MIG);
	return ret;
}

#ifndef BYTES_PER_LONG
#define BYTES_PER_LONG 8
#endif

static size_t calc_bitmap_len(void)
{
	int i;
	size_t buf_len = 0;

	/*
	 * Each process's information layout is as follows:
	 * +----------+------------------------------------------------------+
	 * | PID (4B) |        NR_NODE0_PAGE-NR_NODEn_PAGE (n * 8B)          |
	 * +----------------------------------------------+------------------+
	 * | NODE0_BITMAP_LEN-NODEn_BITMAP_LEN (n * 8B) |   VM_SIZE (4B)     |
	 * +-----------------------------------------------------------------+
	 * | NODE0_BITMAP-NODEn_BITMAP (size is dependent on bitmap length)  |
	 * +-----------------------------------------------------------------+
	 * |         MAPPING (size is dependent on process vm size)          |
	 * +-----------------------------------------------------------------+
	 */
	struct access_pid *ap;
	down_read(&ap_data.lock);
	list_for_each_entry(ap, &ap_data.list, node) {
		if (ap->type != NORMAL_SCAN) {
			continue;
		}
		buf_len += sizeof(pid_t);
		buf_len += sizeof(size_t) * SMAP_MAX_NUMNODES;
		buf_len += sizeof(size_t) * SMAP_MAX_NUMNODES;
		buf_len += sizeof(u32);
		for (i = 0; i < SMAP_MAX_NUMNODES; i++) {
			buf_len += sizeof(unsigned long) * ap->bm_len[i];
			buf_len += sizeof(unsigned long) * ap->bm_len[i];
		}
		buf_len += sizeof(u32) * ap->info.vm_size;
	}
	up_read(&ap_data.lock);

	return buf_len;
}

static long ioctl_walk_pagemap(void __user *argp)
{
	int ret;
	size_t buf_len;
	if (!check_and_clear_ap_state(&ap_data, AP_STATE_WALK)) {
		pr_err("walk pagemap of access pid is not allowed\n");
		return -EAGAIN;
	}
	buf_len = calc_bitmap_len();
	if (!buf_len || copy_to_user(argp, &buf_len, sizeof(buf_len))) {
		ret = -EFAULT;
		set_ap_whole_state(&ap_data, AP_STATE_WALK);
		return ret;
	} else {
		set_ap_whole_state(&ap_data, AP_STATE_WALK | AP_STATE_READ);
	}
	ret = hist_actc_data_reinit();
	if (!ret) {
		update_hist_tracking();
	}
	return ret;
}

static inline void write_bitmap_pid(char **buffer, struct access_pid *ap)
{
	if (unlikely(!buffer || !(*buffer) || !ap)) {
		pr_err("invalid buffer or access pid passed to write bitmap\n");
		return;
	}
	memcpy(*buffer, &ap->pid, sizeof(ap->pid));
	*buffer += sizeof(ap->pid);
}

static inline void write_bitmap_nrpage(char **buffer, struct access_pid *ap)
{
	int i;
	if (unlikely(!buffer || !(*buffer) || !ap)) {
		pr_err("invalid buffer or access pid passed to write page number\n");
		return;
	}
	for (i = 0; i < SMAP_MAX_NUMNODES; i++) {
		memcpy(*buffer, &ap->page_num[i], sizeof(ap->page_num[i]));
		*buffer += sizeof(ap->page_num[i]);
	}
}

static inline void write_bitmap_bmlen(char **buffer, struct access_pid *ap)
{
	int i;
	if (unlikely(!buffer || !(*buffer) || !ap)) {
		pr_err("invalid buffer or access pid passed to write bitmap length\n");
		return;
	}
	for (i = 0; i < SMAP_MAX_NUMNODES; i++) {
		memcpy(*buffer, &ap->bm_len[i], sizeof(ap->bm_len[i]));
		*buffer += sizeof(ap->bm_len[i]);
	}
}

static inline void write_bitmap_vmsize(char **buffer, struct access_pid *ap)
{
	if (unlikely(!buffer || !(*buffer) || !ap)) {
		pr_err("invalid buffer or access pid passed to write VM size\n");
		return;
	}
	memcpy(*buffer, &ap->info.vm_size, sizeof(ap->info.vm_size));
	*buffer += sizeof(ap->info.vm_size);
}

static void write_bitmap_paddrbm(char **buffer, struct access_pid *ap)
{
	int i;
	if (unlikely(!buffer || !(*buffer) || !ap)) {
		pr_err("invalid buffer or access pid passed to write physical address bitmap\n");
		return;
	}
	for (i = 0; i < SMAP_MAX_NUMNODES; i++) {
		size_t length = sizeof(unsigned long) * ap->bm_len[i];
		if (length == 0 || !ap->paddr_bm[i]) {
			pr_debug("no need to write pid %d paddr_bm[%d]\n",
				 ap->pid, i);
			continue;
		}
		memcpy(*buffer, ap->paddr_bm[i], length);
		*buffer += length;
	}
}

static void write_bitmap_white_list(char **buffer, struct access_pid *ap)
{
	int i;
	if (unlikely(!buffer || !(*buffer) || !ap)) {
		pr_err("invalid buffer or access pid passed to write white list\n");
		return;
	}
	for (i = 0; i < SMAP_MAX_NUMNODES; i++) {
		size_t length = sizeof(unsigned long) * ap->bm_len[i];
		if (length == 0 || !ap->white_list_bm[i]) {
			pr_debug("no need to write pid %d white_list_bm[%d]\n",
				 ap->pid, i);
			continue;
		}
		memcpy(*buffer, ap->white_list_bm[i], length);
		*buffer += length;
	}
}

static void write_bitmap_mappig(char **buffer, struct access_pid *ap)
{
	size_t length;
	if (unlikely(!buffer || !(*buffer) || !ap)) {
		pr_err("invalid buffer or access pid passed to write VM mapping info\n");
		return;
	}
	if (!ap->info.mapping) {
		pr_debug("no need to write pid %d mapping\n", ap->pid);
		return;
	}
	length = sizeof(u32) * ap->info.vm_size;
	memcpy(*buffer, ap->info.mapping, length);
	*buffer += length;
}

static void write_bitmap_buffer(char **buffer)
{
	struct access_pid *ap;

	if (unlikely(!buffer || !(*buffer))) {
		pr_err("invalid buffer passed to write bitmap buffer\n");
		return;
	}
	down_read(&ap_data.lock);
	list_for_each_entry(ap, &ap_data.list, node) {
		if (ap->type != NORMAL_SCAN)
			continue;

		write_bitmap_pid(buffer, ap);
		write_bitmap_nrpage(buffer, ap);
		write_bitmap_bmlen(buffer, ap);
		write_bitmap_vmsize(buffer, ap);
		write_bitmap_paddrbm(buffer, ap);
		write_bitmap_white_list(buffer, ap);
		write_bitmap_mappig(buffer, ap);
	}
	up_read(&ap_data.lock);
}

static size_t read_bitmap(char __user *buf, size_t cnt, loff_t *loff)
{
	size_t buf_len;
	char *bitmap_buf, *tmp_buf;

	pr_debug("reading bitmap\n");
	buf_len = calc_bitmap_len();
	if (buf_len == 0 || buf_len != cnt)
		return 0;
	bitmap_buf = vmalloc(buf_len);
	if (!bitmap_buf)
		return 0;

	tmp_buf = bitmap_buf;
	write_bitmap_buffer(&bitmap_buf);
	bitmap_buf = tmp_buf;

	/* ram_changed indicate user space hasn't fetch the newest iomem range */
	if (ram_changed()) {
		vfree(bitmap_buf);
		return 0;
	}

	if (copy_to_user(buf, bitmap_buf, buf_len)) {
		vfree(bitmap_buf);
		return 0;
	}
	vfree(bitmap_buf);
	return buf_len;
}

static int smap_access_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int smap_access_release(struct inode *inode, struct file *file)
{
	return 0;
}

static void update_tracking_data(actc_t *tracking_data,
				 struct statistics_tracking_info *stat_info,
				 struct tracking_info_payload *payload_info)
{
	u64 j;
	u32 i;
	payload_info->length =
		payload_info->length > (stat_info->page_num[L1] +
					stat_info->page_num[L2]) ?
			(stat_info->page_num[L1] + stat_info->page_num[L2]) :
			payload_info->length;
	for (i = 0; i < payload_info->length; i++) {
		for (j = 0; j < stat_info->window_num; j++)
			tracking_data[i] += stat_info->sliding_windows[j][i];
	}
}

static long ioctl_get_tracking(void __user *argp)
{
	int ret = 0;
	struct tracking_info_payload msg;
	actc_t *tracking_data;
	struct statistics_tracking_info *tmp;
	pr_info("Receive ioctl get tracking\n");
	if (copy_from_user(&msg, argp, sizeof(msg)))
		return -EFAULT;

	if (msg.length == 0) {
		pr_err("invalid message length passed to get tracking data\n");
		return -EINVAL;
	}

	if (!msg.data) {
		pr_err("null buffer passed to get tracking data\n");
		return -EINVAL;
	}
	tracking_data = vzalloc(sizeof(actc_t) * msg.length);
	if (!tracking_data) {
		pr_err("unable to allocate memory for tracking data payload\n");
		return -ENOMEM;
	}

	spin_lock(&statistic_lock);
	list_for_each_entry(tmp, &statistic_pid_list, node) {
		if (tmp->pid == msg.pid)
			update_tracking_data(tracking_data, tmp, &msg);
	}
	spin_unlock(&statistic_lock);
	if (copy_to_user(argp, &msg, sizeof(msg))) {
		pr_err("failed to copy message to user space\n");
		ret = -EFAULT;
		goto out_free_payload;
	}
	if (copy_to_user(msg.data, tracking_data,
			 sizeof(actc_t) * msg.length)) {
		pr_err("failed to copy tracking data to user space buffer\n");
		ret = -EFAULT;
	}
	pr_info("Exit ioctl get tracking, ret: %d, outlen: %d\n", ret,
		msg.length);
out_free_payload:
	vfree(tracking_data);
	return ret;
}

static long smap_access_ioctl(struct file *file, unsigned int cmd,
			      unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int rc = 0;

	if (_IOC_TYPE(cmd) != SMAP_ACCESS_MAGIC)
		return -EINVAL;

	pr_debug("enter smap_access_ioctl, nr %u\n", _IOC_NR(cmd));
	switch (cmd) {
	case SMAP_ACCESS_ADD_PID:
		return ioctl_add_pid(argp);
	case SMAP_ACCESS_REMOVE_PID:
		return ioctl_remove_pid(argp);
	case SMAP_ACCESS_REMOVE_ALL_PID:
		return ioctl_remove_all_pid(argp);
	case SMAP_ACCESS_WALK_PAGEMAP:
		return ioctl_walk_pagemap(argp);
	case SMAP_ACCESS_GET_TRACKING:
		return ioctl_get_tracking(argp);
	case SMAP_ACCESS_READ_PID_FREQ:
		return ioctl_read_pid_freq(argp);
	default:
		rc = -ENOTTY;
	}
	pr_debug("exit smap_access_ioctl, rc %d\n", rc);

	return rc;
}

static ssize_t smap_access_read(struct file *file, char __user *buf, size_t cnt,
				loff_t *loff)
{
	size_t ret;

	if (!check_and_clear_ap_state(&ap_data, AP_STATE_READ)) {
		pr_err("read bitmap of access pid is not allowed\n");
		return 0;
	}

	ret = read_bitmap(buf, cnt, loff);
	if (ret == 0)
		set_ap_whole_state(&ap_data, AP_STATE_WALK);
	else
		set_ap_whole_state(&ap_data, AP_STATE_WALK | AP_STATE_READ |
						     AP_STATE_FREQ);
	return ret;
}

static struct file_operations smap_access_fops = {
	.owner = THIS_MODULE,
	.open = smap_access_open,
	.read = smap_access_read,
	.unlocked_ioctl = smap_access_ioctl,
	.release = smap_access_release,
};

void access_dev_exit(void)
{
	device_destroy(access_class, ioctl_access_dev);
	class_destroy(access_class);
	cdev_del(&access_cdev);
	unregister_chrdev_region(ioctl_access_dev, NR_MINOR);
}

int access_dev_init(void)
{
	int rc = alloc_chrdev_region(&ioctl_access_dev, BASE_MINOR, NR_MINOR,
				     ACCESS_DEV);
	if (rc < 0) {
		pr_err("unable to allocate access character device region\n");
		return rc;
	}

	cdev_init(&access_cdev, &smap_access_fops);

	rc = cdev_add(&access_cdev, ioctl_access_dev, 1);
	if (rc) {
		pr_err("unable to add access device to the system\n");
		goto err_cdev;
	}
	access_class = class_create(ACCESS_CLASS);
	if (IS_ERR(access_class)) {
		pr_err("unable to create the access class\n");
		rc = PTR_ERR(access_class);
		goto err_class;
	}

	access_device = device_create(access_class, NULL, ioctl_access_dev,
				      NULL, ACCESS_DEVICE);
	if (IS_ERR(access_device)) {
		pr_err("unable to create the access device\n");
		rc = PTR_ERR(access_device);
		goto err_device;
	}

	return 0;

err_device:
	class_destroy(access_class);
err_class:
	cdev_del(&access_cdev);
err_cdev:
	unregister_chrdev_region(ioctl_access_dev, NR_MINOR);
	return rc;
}

void access_ioctl_exit(void)
{
	access_remove_all_pid();
	access_dev_exit();
}

int access_ioctl_init(void)
{
	return access_dev_init();
}
