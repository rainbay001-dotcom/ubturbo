// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: SMAP : ub_hist
 */

#include <asm/io.h>
#include <linux/acpi.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/capability.h>
#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/minmax.h>
#include <linux/uaccess.h>

#include "ub_hist.h"

#define PLATFORM_MAX_CPU_NUM 2
#define UDIE_MAX_BA_NUM 4
#define UDIE_MAX_CACHE_TYPE 2
#define UDIE_PHY_ADDR_RANGE_SIZE (1024 * GB)

#define UDIE_PHY_ADDR_STS_REGS_OFFSET 0x4000
#define BA_CTRL_REG_INIT 0x000001C1
#define BA_ECC_REG_INIT 0x00000000
#define BA_THRESH_REG_INIT 0x00FF0000

enum {
	BA_CTRL_REG_OFFSET = 0x0,
	BA_ECC_REG_OFFSET = 0x4,
	BA_THRESH_REG_OFFSET = 0x8,
};
enum {
	BA_TYPE_0 = 0,
	BA_TYPE_1 = 1,
	BA_TYPE_2 = 2,
	BA_TYPE_3 = 3,
};

struct mirror_data_mirror {
	spinlock_t context_lock;
	struct ub_hist_ba_result result;
	bool updated;
	bool reader_waiting;
	size_t read_offset;
	wait_queue_head_t wq;
};

struct ub_hist_ba_device {
	struct device *dev;
	void __iomem *base_addr;
	struct list_head list;
	struct ub_hist_ba_info info;
};

static LIST_HEAD(ub_hist_ba_list);
static DEFINE_SPINLOCK(ub_hist_ba_list_lock);
static DEFINE_MUTEX(dev_sta_mutex);
static struct mirror_data_mirror *mirror_data;

static struct ub_hist_ba_device *ub_hist_find_dev_by_tag(uint64_t ba_tag)
{
	unsigned long flags;
	struct ub_hist_ba_device *ba_dev;

	spin_lock_irqsave(&ub_hist_ba_list_lock, flags);
	list_for_each_entry(ba_dev, &ub_hist_ba_list, list) {
		if (ba_dev->info.ba_tag == ba_tag) {
			spin_unlock_irqrestore(&ub_hist_ba_list_lock, flags);
			return ba_dev;
		}
	}
	pr_err("failed to find ba device by ba_tag 0x%llx\n", ba_tag);
	ba_dev = NULL;
	spin_unlock_irqrestore(&ub_hist_ba_list_lock, flags);
	return ba_dev;
}

static inline u32 ub_hist_read_reg(struct ub_hist_ba_device *ba_dev, u32 off)
{
	void __iomem *reg = ba_dev->base_addr + off;
	u32 out = readl(reg);

	return le32_to_cpu(*(__le32 *)&out);
}

static inline void ub_hist_write_reg(struct ub_hist_ba_device *ba_dev, u32 off,
				     u32 val)
{
	void __iomem *reg = ba_dev->base_addr + off;
	__le32 val_le = cpu_to_le32(val);

	writel(*(u32 *)&val_le, reg);
}

static int ub_hist_ba_init(struct ub_hist_ba_device *ba_dev)
{
#define BA_ECC_REG_INIT_CHECK_MASK 0x3F
	union ub_hist_ba_reg_ctrl reg_ctrl;
	union ub_hist_ba_reg_thresh reg_thresh;
	union ub_hist_ba_reg_ecc reg_ecc = {
		.smap_1bit_ecc_int_src = 1,
		.smap_2bit_ecc_int_src = 1,
	};

	reg_ctrl.val = ub_hist_read_reg(ba_dev, BA_CTRL_REG_OFFSET);
	if (!reg_ctrl.ram_init_done) {
		dev_err(ba_dev->dev, "hist ba[0x%llx] init state error\n",
			ba_dev->info.ba_tag);
		return -EBUSY;
	}

	ub_hist_write_reg(ba_dev, BA_CTRL_REG_OFFSET, BA_CTRL_REG_INIT);
	reg_ctrl.val = ub_hist_read_reg(ba_dev, BA_CTRL_REG_OFFSET);
	if (reg_ctrl.val != BA_CTRL_REG_INIT) {
		dev_err(ba_dev->dev,
			"hist ba[0x%llx] ctrl reg mismatch(%d:%d)\n",
			ba_dev->info.ba_tag, reg_ctrl.val, BA_CTRL_REG_INIT);
		return -EBUSY;
	}

	ub_hist_write_reg(ba_dev, BA_ECC_REG_OFFSET, reg_ecc.val);
	reg_ecc.val = ub_hist_read_reg(ba_dev, BA_ECC_REG_OFFSET);
	if ((reg_ecc.val & BA_ECC_REG_INIT_CHECK_MASK) != BA_ECC_REG_INIT) {
		dev_err(ba_dev->dev, "hist ba[%lld] ecc reg mismatch(%d:%d)\n",
			ba_dev->info.ba_tag,
			reg_ecc.val & BA_ECC_REG_INIT_CHECK_MASK,
			BA_ECC_REG_INIT);
		return -EBUSY;
	}

	ub_hist_write_reg(ba_dev, BA_THRESH_REG_OFFSET, BA_THRESH_REG_INIT);
	reg_thresh.val = ub_hist_read_reg(ba_dev, BA_THRESH_REG_OFFSET);
	if ((reg_thresh.val & BA_THRESH_REG_INIT) != BA_THRESH_REG_INIT) {
		dev_err(ba_dev->dev,
			"hist ba[0x%llx] intr reg mismatch(%d:%d)\n",
			ba_dev->info.ba_tag, reg_thresh.val,
			BA_THRESH_REG_INIT);
		return -EBUSY;
	}

	return 0;
}

static int ub_hist_rd_clr_sts(struct ub_hist_ba_device *ba_dev, u32 *buf,
			      size_t len)
{
	unsigned int i;
	union ub_hist_ba_reg_ctrl reg_ctrl;

	reg_ctrl.val = ub_hist_read_reg(ba_dev, BA_CTRL_REG_OFFSET);
	if (reg_ctrl.sts_enable) {
		pr_err("regs cannot be read when statistics are enabled\n");
		return -EBUSY;
	}

	if (buf) {
		for (i = 0; i < BA_STS_WORD_COUNT; i++) {
			buf[i] = ub_hist_read_reg(
				ba_dev, UDIE_PHY_ADDR_STS_REGS_OFFSET +
						i * sizeof(u32));
		}
	} else {
		for (i = 0; i < BA_STS_WORD_COUNT; i++) {
			ub_hist_read_reg(ba_dev, UDIE_PHY_ADDR_STS_REGS_OFFSET +
							 i * sizeof(u32));
		}
	}

	return 0;
}

int ub_hist_lock_device(void)
{
	if (!mutex_trylock(&dev_sta_mutex)) {
		return -EBUSY;
	}

	return 0;
}

void ub_hist_unlock_device(void)
{
	mutex_unlock(&dev_sta_mutex);
}

int ub_hist_query_ba_count(void)
{
	int ba_count = 0;
	struct ub_hist_ba_device *ba_dev;

	spin_lock(&ub_hist_ba_list_lock);
	list_for_each_entry(ba_dev, &ub_hist_ba_list, list) {
		ba_count++;
	}
	spin_unlock(&ub_hist_ba_list_lock);
	return ba_count;
}

int ub_hist_query_ba_tags(uint64_t *p_tags, int count)
{
	int ret = 0, ba_count = 0;
	struct ub_hist_ba_device *ba_dev;

	spin_lock(&ub_hist_ba_list_lock);
	list_for_each_entry(ba_dev, &ub_hist_ba_list, list) {
		if (ba_count >= count) {
			ret = -EINVAL;
			spin_unlock(&ub_hist_ba_list_lock);
			return ret;
		}
		p_tags[ba_count] = ba_dev->info.ba_tag;
		ba_count++;
	}

	if (ba_count < count) {
		ret = -EINVAL;
	}
	spin_unlock(&ub_hist_ba_list_lock);
	return ret;
}

int ub_hist_query_ba_info(uint64_t ba_tag, struct ub_hist_ba_info *ba_info)
{
	struct ub_hist_ba_device *ba_dev;

	if (!ba_info) {
		pr_err("invalid ba info pointer\n");
		return -EINVAL;
	}

	ba_dev = ub_hist_find_dev_by_tag(ba_tag);
	if (!ba_dev) {
		return -ENODEV;
	}

	memcpy(ba_info, &ba_dev->info, sizeof(struct ub_hist_ba_info));

	return 0;
}

int ub_hist_set_state(struct ub_hist_ba_config *config, uint64_t ba_tag)
{
	struct ub_hist_ba_device *ba_dev;

	if (!config) {
		pr_err("invalid config pointer\n");
		return -EINVAL;
	}

	ba_dev = ub_hist_find_dev_by_tag(ba_tag);
	if (!ba_dev) {
		return -ENODEV;
	}

	if (config->mask & BA_CTRL_REG_CFG_MASK) {
		ub_hist_write_reg(ba_dev, BA_CTRL_REG_OFFSET,
				  config->regs.reg_ctrl.val);
		pr_debug("ba %llu, write cfg val %#x", ba_dev->info.ba_tag,
			 config->regs.reg_ctrl.val);
	}
	if (config->mask & BA_ECC_REG_CFG_MASK) {
		ub_hist_write_reg(ba_dev, BA_ECC_REG_OFFSET,
				  config->regs.reg_ecc.val);
		pr_debug("ba %llu, write ecc val %#x", ba_dev->info.ba_tag,
			 config->regs.reg_ecc.val);
	}
	if (config->mask & BA_THRESH_REG_CFG_MASK) {
		ub_hist_write_reg(ba_dev, BA_THRESH_REG_OFFSET,
				  config->regs.reg_thresh.val);
		pr_debug("ba %llu, write thre val %#x", ba_dev->info.ba_tag,
			 config->regs.reg_thresh.val);
	}

	return 0;
}

int ub_hist_get_state(struct ub_hist_ba_config *config, uint64_t ba_tag)
{
	struct ub_hist_ba_device *ba_dev;

	if (!config) {
		pr_err("invalid config pointer\n");
		return -EINVAL;
	}

	ba_dev = ub_hist_find_dev_by_tag(ba_tag);
	if (!ba_dev) {
		return -ENODEV;
	}

	if (config->mask & BA_CTRL_REG_CFG_MASK) {
		config->regs.reg_ctrl.val =
			ub_hist_read_reg(ba_dev, BA_CTRL_REG_OFFSET);
	}
	if (config->mask & BA_ECC_REG_CFG_MASK) {
		config->regs.reg_ecc.val =
			ub_hist_read_reg(ba_dev, BA_ECC_REG_OFFSET);
	}
	if (config->mask & BA_THRESH_REG_CFG_MASK) {
		config->regs.reg_thresh.val =
			ub_hist_read_reg(ba_dev, BA_THRESH_REG_OFFSET);
	}

	return 0;
}

int ub_hist_get_statistic_result(struct ub_hist_ba_result *result)
{
	int ret;
	struct ub_hist_ba_device *ba_dev;

	if (!result) {
		pr_err("invalid result pointer\n");
		return -EINVAL;
	}

	ba_dev = ub_hist_find_dev_by_tag(result->ba_tag);
	if (!ba_dev) {
		return -ENODEV;
	}

	ret = ub_hist_rd_clr_sts(ba_dev, (u32 *)result->buffer,
				 BA_STS_WORD_COUNT);
	if (ret) {
		return ret;
	}

	spin_lock(&mirror_data->context_lock);
	if (!mirror_data->reader_waiting) {
		spin_unlock(&mirror_data->context_lock);
		return ret;
	}

	mirror_data->updated = true;
	mirror_data->reader_waiting = false;
	memcpy(&mirror_data->result, result, sizeof(struct ub_hist_ba_result));
	spin_unlock(&mirror_data->context_lock);
	wake_up_interruptible(&mirror_data->wq);
	return ret;
}

static long ub_hist_ioctl_info_check(unsigned int cmd, unsigned long arg)
{
	int ret = 0, ba_count;
	uint64_t ba_tag;
	struct ub_hist_ba_info info;
	void *result = NULL;
	int __user *p = (int __user *)arg;

	switch (cmd) {
	case UB_HIST_IOCTL_QUERY_BA_COUNT:
		ba_count = ub_hist_query_ba_count();
		if (put_user(ba_count, p)) {
			return -EFAULT;
		}
		break;

	case UB_HIST_IOCTL_QUERY_BA_TAGS:
		if (get_user(ba_count, p)) {
			return -EFAULT;
		}
		if (ba_count <= 0 || ba_count > UDIE_MAX_BA_NUM) {
			return -EINVAL;
		}
		result = kcalloc(ba_count, sizeof(uint64_t), GFP_KERNEL);
		if (!result) {
			return -ENOMEM;
		}
		ret = ub_hist_query_ba_tags(result, ba_count);
		p = (void *)p + sizeof(struct ub_hist_ba_tags);
		if (ret ||
		    copy_to_user(p, result, sizeof(uint64_t) * ba_count)) {
			ret = -EFAULT;
		}
		break;

	case UB_HIST_IOCTL_QUERY_BA_INFO:
		if (copy_from_user(&ba_tag, p, sizeof(uint64_t))) {
			return -EFAULT;
		}
		ret = ub_hist_query_ba_info(ba_tag, &info);
		if (ret || copy_to_user(p, &info, sizeof(info))) {
			ret = -EFAULT;
		}
		break;
	default:
		ret = -EINVAL;
	}

	if (result) {
		kfree(result);
	}
	return ret;
}

static long ub_hist_ioctl_do_work(unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	uint64_t ba_tag;
	struct ub_hist_reg_config reg_config;
	struct ub_hist_ba_result *result = NULL;
	int __user *p = (int __user *)arg;

	switch (cmd) {
	case UB_HIST_IOCTL_SET_REGS:
		if (copy_from_user(&reg_config, p,
				   sizeof(struct ub_hist_reg_config))) {
			return -EFAULT;
		}
		return ub_hist_set_state(&reg_config.config, reg_config.ba_tag);

	case UB_HIST_IOCTL_GET_REGS:
		if (copy_from_user(&reg_config, p,
				   sizeof(struct ub_hist_reg_config))) {
			return -EFAULT;
		}
		ret = ub_hist_get_state(&reg_config.config, reg_config.ba_tag);
		if (ret || copy_to_user(p, &reg_config,
					sizeof(struct ub_hist_reg_config))) {
			return -EFAULT;
		}
		break;

	case UB_HIST_IOCTL_GET_STATISTIC_RESULT:
		if (get_user(ba_tag, p++)) {
			return -EFAULT;
		}
		result = kzalloc(sizeof(struct ub_hist_ba_result), GFP_KERNEL);
		if (!result) {
			return -ENOMEM;
		}
		result->ba_tag = ba_tag;
		if (ub_hist_get_statistic_result(result) ||
		    copy_to_user(p, result->buffer, BA_STS_TOTAL_LEHGTH)) {
			ret = -EFAULT;
		}
		break;
	default:
		ret = -EINVAL;
	}

	if (result) {
		kfree(result);
	}
	return ret;
}

static long ub_hist_ioctl(struct file *filp, unsigned int cmd,
			  unsigned long arg)
{
	long ret = 0;

	switch (cmd) {
	case UB_HIST_IOCTL_QUERY_BA_COUNT:
	case UB_HIST_IOCTL_QUERY_BA_TAGS:
	case UB_HIST_IOCTL_QUERY_BA_INFO:
		ret = ub_hist_ioctl_info_check(cmd, arg);
		break;
	case UB_HIST_IOCTL_SET_REGS:
	case UB_HIST_IOCTL_GET_REGS:
	case UB_HIST_IOCTL_GET_STATISTIC_RESULT:
		ret = ub_hist_ioctl_do_work(cmd, arg);
		break;
	default:
		return -ENOTTY;
	}

	return ret;
}

static int ub_hist_open(struct inode *inode, struct file *filp)
{
	if (!capable(CAP_SYS_ADMIN)) {
		return -EPERM;
	}

	return ub_hist_lock_device();
}

static int ub_hist_release(struct inode *inode, struct file *filp)
{
	ub_hist_unlock_device();
	return 0;
}

static struct file_operations ub_hist_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = ub_hist_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ub_hist_ioctl,
#endif
	.open = ub_hist_open,
	.release = ub_hist_release,
};

static struct miscdevice ub_hist_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ub_hist",
	.fops = &ub_hist_fops,
};

static ssize_t device_config_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	unsigned long flags;
	struct ub_hist_ba_device *ba_dev;
	ssize_t total_len = 0;

	if (!capable(CAP_SYS_ADMIN)) {
		return -EPERM;
	}

	spin_lock_irqsave(&ub_hist_ba_list_lock, flags);
	list_for_each_entry(ba_dev, &ub_hist_ba_list, list) {
		total_len += snprintf(buf + total_len, PAGE_SIZE,
				      "ba_tag:0x%llx\n", ba_dev->info.ba_tag);
		total_len += snprintf(
			buf + total_len, PAGE_SIZE,
			"\tcontrol reg:0x%08x, ecc reg:0x%08x, thresh reg:0x%08x\n",
			ub_hist_read_reg(ba_dev, BA_CTRL_REG_OFFSET),
			ub_hist_read_reg(ba_dev, BA_ECC_REG_OFFSET),
			ub_hist_read_reg(ba_dev, BA_THRESH_REG_OFFSET));
		total_len += snprintf(buf + total_len, PAGE_SIZE,
				      "\tcc_start:0x%llx, cc_end:0x%llx\n",
				      ba_dev->info.cc_range.start,
				      ba_dev->info.cc_range.end);
		total_len += snprintf(buf + total_len, PAGE_SIZE,
				      "\tnc_start:0x%llx, nc_end:0x%llx\n",
				      ba_dev->info.nc_range.start,
				      ba_dev->info.nc_range.end);
	}
	spin_unlock_irqrestore(&ub_hist_ba_list_lock, flags);
	return total_len;
}

static unsigned int print_mirror_buffer(uint16_t *arr, size_t offset,
					size_t batch, char *buf)
{
#define COUNT_PER_LINE 8
	size_t i, j;
	unsigned int used = 0;

	for (i = 0; i < batch; i += COUNT_PER_LINE) {
		used += (unsigned int)snprintf(buf + used, PAGE_SIZE, "%08lx: ",
					       (unsigned int)i + offset);
		for (j = 0; j < COUNT_PER_LINE; j++) {
			used += (unsigned int)snprintf(buf + used, PAGE_SIZE,
						       "%02x ",
						       arr[offset + i + j]);
		}
		used += (unsigned int)snprintf(buf + used, PAGE_SIZE, "\n");
	}
	return used;
}

static int statistic_result_context_update_pre(size_t *l_offset,
					       bool *l_updated, char *buf)
{
#define DUMP_BATCH 896
	if (!capable(CAP_SYS_ADMIN)) {
		return -EPERM;
	}

	spin_lock(&mirror_data->context_lock);
	if (mirror_data->reader_waiting) {
		spin_unlock(&mirror_data->context_lock);
		return snprintf(buf, PAGE_SIZE, "device is busy.\n");
	}

	*l_offset = mirror_data->read_offset;
	*l_updated = mirror_data->updated;
	if ((*l_offset) == 0 && (*l_updated) == false) {
		mirror_data->reader_waiting = true;
	} else if (mirror_data->read_offset < BA_STS_VALUE_COUNT) {
		mirror_data->read_offset += DUMP_BATCH;
	}
	spin_unlock(&mirror_data->context_lock);
	return 0;
}

void statistic_result_context_update_post(void)
{
	spin_lock(&mirror_data->context_lock);
	mirror_data->updated = false;
	mirror_data->read_offset = 0;
	spin_unlock(&mirror_data->context_lock);
}

static ssize_t statistic_result_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
#define WAIT_TIME_MS 10000 /* 10s */
	int ret;
	bool l_updated;
	size_t l_offset, batch_size;
	ssize_t total_len = 0;

	ret = statistic_result_context_update_pre(&l_offset, &l_updated, buf);
	if (ret) {
		return ret;
	}
	/* Async thread may set mirror_data->updated = true; code handles it. */
	if (l_offset == 0 && l_updated == false) {
		ret = wait_event_interruptible_timeout(
			mirror_data->wq, mirror_data->updated == true,
			msecs_to_jiffies(WAIT_TIME_MS));
		spin_lock(&mirror_data->context_lock);
		if (ret <= 0 || mirror_data->updated == false) {
			mirror_data->reader_waiting = false;
			spin_unlock(&mirror_data->context_lock);
			return snprintf(buf, PAGE_SIZE, "data not ready!\n");
		}
		l_offset = mirror_data->read_offset;
		if (mirror_data->read_offset < BA_STS_VALUE_COUNT) {
			mirror_data->read_offset += DUMP_BATCH;
		}
		spin_unlock(&mirror_data->context_lock);
	}

	if (l_offset >= BA_STS_TOTAL_LEHGTH) {
		return 0;
	} else if (l_offset == 0) {
		total_len = snprintf(buf, PAGE_SIZE,
				     "========== ba_tag [0x%llx] ==========\n",
				     mirror_data->result.ba_tag);
	}
	batch_size = min_t(size_t, BA_STS_VALUE_COUNT - l_offset, DUMP_BATCH);
	total_len += (ssize_t)print_mirror_buffer(
		(uint16_t *)mirror_data->result.buffer, l_offset, batch_size,
		buf + total_len);
	/* the one who read tail of the buffer should do context reset */
	if (l_offset + DUMP_BATCH >= BA_STS_VALUE_COUNT) {
		statistic_result_context_update_post();
	}

	return total_len;
}

static ssize_t statistic_result_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t len)
{
	if (!capable(CAP_SYS_ADMIN)) {
		return -EPERM;
	}

	spin_lock(&mirror_data->context_lock);
	if (mirror_data->reader_waiting) {
		spin_unlock(&mirror_data->context_lock);
		return 0;
	}

	mirror_data->updated = false;
	mirror_data->read_offset = 0;
	spin_unlock(&mirror_data->context_lock);
	wake_up_interruptible(&mirror_data->wq);
	return 0;
}

static DEVICE_ATTR(device_config, 0640, device_config_show, NULL);
static DEVICE_ATTR(statistic_result, 0640, statistic_result_show,
		   statistic_result_store);

static struct attribute *debug_attrs[] = { &dev_attr_statistic_result.attr,
					   &dev_attr_device_config.attr, NULL };

static struct attribute_group debug_attr_group = {
	.name = "debug",
	.attrs = debug_attrs,
};

static int ub_hist_init_sysfs(void)
{
	int ret;
	struct device *dev = ub_hist_miscdev.this_device;

	mirror_data = kzalloc(sizeof(*mirror_data), GFP_KERNEL);
	if (!mirror_data) {
		return -ENOMEM;
	}
	init_waitqueue_head(&mirror_data->wq);
	spin_lock_init(&mirror_data->context_lock);
	ret = sysfs_create_group(&dev->kobj, &debug_attr_group);
	if (ret) {
		kfree(mirror_data);
		mirror_data = NULL;
		pr_err("failed to create sysfs node for attr group\n");
	}

	return ret;
}

static void ub_hist_deinit_sysfs(void)
{
	struct device *dev = ub_hist_miscdev.this_device;
	sysfs_remove_group(&dev->kobj, &debug_attr_group);
	kfree(mirror_data);
	mirror_data = NULL;
}

static int ub_hist_get_ba_resource(struct platform_device *pdev,
				   struct ub_hist_ba_device *ba_dev)
{
#define DEVICE_MEM_LEN 0x8000
	int ret;
	uint64_t value;

	if (!ACPI_COMPANION(&pdev->dev)) {
		pr_err("ACPI companion not found\n");
		return -ENODEV;
	}

	ret = device_property_read_u64(&pdev->dev, "ba_reg_base_addr", &value);
	if (ret) {
		pr_err("Failed to read reg_base_addr: %d\n", ret);
		return ret;
	}

	ba_dev->info.ba_tag = value;
	ba_dev->base_addr = ioremap(value, DEVICE_MEM_LEN);
	if (!ba_dev->base_addr) {
		pr_err("Failed to remap reg_base_addr: 0x%llx\n", value);
		return -ENOMEM;
	}
	pr_debug("reg_base_addr: 0x%llx\n", value);

	ret = device_property_read_u64(&pdev->dev, "cc_mem_base_addr", &value);
	if (ret) {
		pr_err("Failed to read cc_mem_base_addr: %d\n", ret);
		iounmap(ba_dev->base_addr);
		return ret;
	}
	ba_dev->info.cc_range.start = value;
	pr_debug("cc_mem_base_addr: 0x%llx\n", value);

	ret = device_property_read_u64(&pdev->dev, "cc_mem_size", &value);
	if (ret) {
		pr_err("Failed to read cc_mem_size: %d\n", ret);
		iounmap(ba_dev->base_addr);
		return ret;
	}
	ba_dev->info.cc_range.end = ba_dev->info.cc_range.start + value;
	pr_debug("cc_mem_size: 0x%llx\n", value);

	ret = device_property_read_u64(&pdev->dev, "nc_mem_base_addr", &value);
	if (ret) {
		pr_err("Failed to read nc_mem_base_addr: %d\n", ret);
		iounmap(ba_dev->base_addr);
		return ret;
	}
	ba_dev->info.nc_range.start = value;
	pr_debug("nc_mem_base_addr: 0x%llx\n", value);

	ret = device_property_read_u64(&pdev->dev, "nc_mem_size", &value);
	if (ret) {
		pr_err("Failed to read nc_mem_size: %d\n", ret);
		iounmap(ba_dev->base_addr);
		return ret;
	}
	ba_dev->info.nc_range.end = ba_dev->info.nc_range.start + value;
	pr_debug("nc_mem_size: 0x%llx\n", value);

	return 0;
}

static int ub_hist_probe(struct platform_device *pdev)
{
	int ret;
	unsigned long flags;
	struct ub_hist_ba_device *ba_dev;
	struct resource *r;

	ba_dev = devm_kzalloc(&pdev->dev, sizeof(*ba_dev), GFP_KERNEL);
	if (!ba_dev) {
		return -ENOMEM;
	}
	ba_dev->dev = &pdev->dev;

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ret = ub_hist_get_ba_resource(pdev, ba_dev);
	if (ret) {
		return ret;
	}

	spin_lock_irqsave(&ub_hist_ba_list_lock, flags);
	list_add_tail(&ba_dev->list, &ub_hist_ba_list);
	spin_unlock_irqrestore(&ub_hist_ba_list_lock, flags);
	ret = ub_hist_ba_init(ba_dev);
	if (ret) {
		return ret;
	}
	ret = ub_hist_rd_clr_sts(ba_dev, NULL, 0);
	return ret;
}

static int ub_hist_remove(struct platform_device *pdev)
{
	struct ub_hist_ba_device *ba_dev, *tmp;
	unsigned long flags;

	spin_lock_irqsave(&ub_hist_ba_list_lock, flags);
	list_for_each_entry_safe(ba_dev, tmp, &ub_hist_ba_list, list) {
		if (ba_dev->dev == &pdev->dev) {
			iounmap(ba_dev->base_addr);
			list_del(&ba_dev->list);
			if (ba_dev) {
				devm_kfree(&pdev->dev, ba_dev);
			}
			break;
		}
	}
	spin_unlock_irqrestore(&ub_hist_ba_list_lock, flags);
	return 0;
}

static const struct acpi_device_id ub_hist_acpi_ids[] = {
	{ "HISI0531", 0 },
	{},
};
MODULE_DEVICE_TABLE(acpi, ub_hist_acpi_ids);

static struct platform_driver ub_hist_platform_driver = {
    .probe = ub_hist_probe,
    .remove = ub_hist_remove,
    .driver =
        {
            .name = "ub-hist-platform-driver",
            .owner = THIS_MODULE,
            .acpi_match_table = ACPI_PTR(ub_hist_acpi_ids),
        },
};

int ub_hist_init(enum platform_type platform)
{
	int ret;
	ret = misc_register(&ub_hist_miscdev);
	if (ret) {
		pr_err("register misc failed %d\n", ret);
		return ret;
	}
	ret = platform_driver_register(&ub_hist_platform_driver);
	if (ret) {
		misc_deregister(&ub_hist_miscdev);
		pr_err("register platform failed %d\n", ret);
		return ret;
	}
	ret = ub_hist_init_sysfs();
	if (ret) {
		platform_driver_unregister(&ub_hist_platform_driver);
		misc_deregister(&ub_hist_miscdev);
		pr_err("init ub hist failed %d\n", ret);
		return ret;
	}
	pr_info("ub_hist init success.\n");
	return ret;
}

void ub_hist_exit(void)
{
	ub_hist_deinit_sysfs();
	platform_driver_unregister(&ub_hist_platform_driver);
	misc_deregister(&ub_hist_miscdev);
	pr_info("ub_hist exit success.\n");
}
