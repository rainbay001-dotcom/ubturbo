// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: SMAP : smap_hist_mid
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/minmax.h>
#include <linux/sort.h>
#include <linux/completion.h>

#include "ub_hist.h"
#include "smap_hist_mid.h"

#define BACKEND_BUFFER_COUNT \
	3 /* Prevent worker thread starvation: value must be ≥ 3 */

struct hist_roi_node {
	struct hist_roi_data data;
	struct list_head list;
};

struct hist_roi_list {
	size_t count;
	uint64_t ba_tag;
	size_t length;
	struct list_head head;
};

struct hist_roi_mgmt {
	int list_count;
	size_t total_length;
	size_t total_count;
	struct hist_roi_list *list;
	struct mutex roi_lock;
};

struct hist_scan_work {
	struct work_struct async_work;
	struct task_struct *scan_kthread;
};

struct buffer_node {
	struct addr_count_pair *result_pair;
	unsigned int pair_count;
	unsigned int used_count;
	struct list_head list;
};

struct scan_work_desc {
	struct task_struct *thread;
	struct list_head *list_head;
	struct buffer_node result_buffer;
	struct completion thread_cplt;
	uint64_t ba_tag;
	uint32_t window_shift;
	uint32_t record_shift;
};

struct hist_sts_buffer {
	struct list_head write_fifo;
	struct list_head read_fifo;
	spinlock_t buffer_lock;
};

struct smap_hist_mid {
	union hist_status status;
	struct hist_scan_cfg config;
	struct hist_scan_work work;
	struct hist_roi_mgmt rois;
	struct hist_sts_buffer result;
	enum hist_ecc_mode ecc_mode;
};

struct smap_hist_scan_record {
	size_t buffer_index;
	phys_addr_t window_end;
	phys_addr_t current_addr;
};

static struct smap_hist_mid *hist_mid;

static inline size_t align_down(phys_addr_t addr, size_t alignment)
{
	return (addr) & ~((alignment)-1ULL);
}

static inline size_t align_up(phys_addr_t addr, size_t alignment)
{
	size_t mask = alignment - 1;

	return ((addr + mask) & ~mask);
}

static inline bool smap_hist_roi_overlapping(struct hist_roi_node *first,
					     struct hist_roi_node *second)
{
	phys_addr_t end_first = first->data.start_addr + first->data.length;
	phys_addr_t end_second = second->data.start_addr + second->data.length;

	return first->data.start_addr <= end_second &&
	       second->data.start_addr <= end_first;
}

static inline bool smap_hist_roi_within_ranges(struct hist_roi_node *roi,
					       struct ub_hist_ba_info *info)
{
	phys_addr_t roi_end = roi->data.start_addr + roi->data.length;

	return (roi->data.start_addr >= info->cc_range.start &&
		roi_end < info->cc_range.end) ||
	       (roi->data.start_addr >= info->nc_range.start &&
		roi_end < info->nc_range.end);
}

static int smap_hist_get_ba_tags(uint64_t **pp_tags, int *count)
{
	int ret;
	uint64_t *p_tags;

	*count = ub_hist_query_ba_count();
	if (*count == 0) {
		pr_err("ba dev not found\n");
		return -ENODEV;
	}

	p_tags = kcalloc(*count, sizeof(uint64_t), GFP_KERNEL);
	if (!p_tags) {
		pr_err("malloc mem failed for p_tags\n");
		return -ENOMEM;
	}

	ret = ub_hist_query_ba_tags(p_tags, *count);
	if (ret) {
		pr_err("the number of BA devices was changed\n");
		kfree(p_tags);
		return -EAGAIN;
	}

	*pp_tags = p_tags;
	return 0;
}

int smap_hist_middle_set_ecc_config(enum hist_ecc_mode mode, uint16_t thresh)
{
	int i, ret, ba_count;
	uint64_t *p_tags;
	struct ub_hist_ba_config config;

	config.regs.reg_ecc.val = 0;
	config.regs.reg_thresh.ecc_1bit_intr_th = thresh;
	switch (mode) {
	case ECC_MODE_DISABLE:
		config.regs.reg_ecc.smap_1bit_ecc_int_en = 0;
		config.regs.reg_ecc.smap_2bit_ecc_int_en = 0;
		break;
	case ECC_MODE_1BIT:
		config.regs.reg_ecc.smap_1bit_ecc_int_en = 0;
		break;
	case ECC_MODE_1BIT_WITH_REPORT:
		config.regs.reg_ecc.smap_1bit_ecc_int_en = 1;
		break;
	case ECC_MODE_2BIT:
		config.regs.reg_ecc.smap_2bit_ecc_int_en = 0;
		break;
	case ECC_MODE_2BIT_WITH_REPORT:
		config.regs.reg_ecc.smap_2bit_ecc_int_en = 1;
		break;
	default:
		pr_err("invalid ecc mode %d\n", mode);
		return -EINVAL;
	}
	config.mask = BA_ECC_REG_CFG_MASK;

	ret = smap_hist_get_ba_tags(&p_tags, &ba_count);
	if (ret) {
		return ret;
	}
	for (i = 0; i < ba_count; i++) {
		ub_hist_set_state(&config, p_tags[i]);
	}
	kfree(p_tags);

	hist_mid->ecc_mode = mode;
	return ret;
}

int smap_hist_middle_get_ecc_status(bool *ecc)
{
	int i, ret, ba_count;
	uint64_t *p_tags;
	bool ecc_occured = false;
	struct ub_hist_ba_config config = { 0 };

	if (hist_mid->ecc_mode == ECC_MODE_DISABLE) {
		*ecc = false;
		return 0;
	}

	ret = smap_hist_get_ba_tags(&p_tags, &ba_count);
	if (ret) {
		return ret;
	}
	config.mask |= BA_THRESH_REG_CFG_MASK;
	for (i = 0; i < ba_count; i++) {
		ret = ub_hist_get_state(&config, p_tags[i]);
		if (ret) {
			continue;
		}

		if ((hist_mid->ecc_mode == ECC_MODE_1BIT) ||
		    (hist_mid->ecc_mode == ECC_MODE_1BIT_WITH_REPORT)) {
			ecc_occured |=
				config.regs.reg_ecc.smap_1bit_ecc_int_src;
		} else if ((hist_mid->ecc_mode == ECC_MODE_2BIT) ||
			   (hist_mid->ecc_mode == ECC_MODE_2BIT_WITH_REPORT)) {
			ecc_occured |=
				config.regs.reg_ecc.smap_2bit_ecc_int_src;
		}
	}

	/* reset ecc status */
	config.regs.reg_ecc.val = 0;
	if (hist_mid->ecc_mode == ECC_MODE_1BIT_WITH_REPORT) {
		config.regs.reg_ecc.smap_1bit_ecc_int_en = 1;
		config.regs.reg_ecc.smap_1bit_ecc_int_src = 1;
	}
	if (hist_mid->ecc_mode == ECC_MODE_2BIT_WITH_REPORT) {
		config.regs.reg_ecc.smap_2bit_ecc_int_en = 1;
		config.regs.reg_ecc.smap_2bit_ecc_int_src = 1;
	}
	for (i = 0; i < ba_count; i++) {
		ub_hist_set_state(&config, p_tags[i]);
	}
	*ecc = ecc_occured;

	kfree(p_tags);
	return ret;
}

size_t smap_hist_middle_get_roi_count(void)
{
	size_t count;

	mutex_lock(&hist_mid->rois.roi_lock);
	count = hist_mid->rois.total_count;
	mutex_unlock(&hist_mid->rois.roi_lock);

	return count;
}

size_t smap_hist_middle_get_roi_total_len(void)
{
	size_t total_len;

	mutex_lock(&hist_mid->rois.roi_lock);
	total_len = hist_mid->rois.total_length;
	mutex_unlock(&hist_mid->rois.roi_lock);

	return total_len;
}

int smap_hist_get_roi_info(struct hist_roi_data *user_rois, size_t count)
{
	int list_i, buf_i = 0;
	struct hist_roi_node *pos;

	if (!user_rois)
		return -EINVAL;

	mutex_lock(&hist_mid->rois.roi_lock);
	if (count != hist_mid->rois.total_count) {
		pr_err("roi count not match\n");
		mutex_unlock(&hist_mid->rois.roi_lock);
		return -EINVAL;
	}

	for (list_i = 0; list_i < hist_mid->rois.list_count; list_i++) {
		list_for_each_entry(pos, &hist_mid->rois.list[list_i].head,
				    list) {
			user_rois[buf_i].start_addr = pos->data.start_addr;
			user_rois[buf_i].length = pos->data.length;
			buf_i++;
		}
	}
	mutex_unlock(&hist_mid->rois.roi_lock);

	return 0;
}

static inline void smap_hist_middle_merge_roi(struct hist_roi_node *t,
					      const struct hist_roi_node *s)
{
	phys_addr_t end;

	if (s->data.start_addr < t->data.start_addr) {
		t->data.start_addr = s->data.start_addr;
	}

	end = s->data.start_addr + s->data.length;
	if (end > t->data.start_addr + t->data.length) {
		t->data.length = end - t->data.start_addr;
	}
}

static void smap_hist_middle_insert_roi(struct hist_roi_node *new_roi,
					struct list_head *roi_list)
{
	struct hist_roi_node *entry, *tmp;
	bool inserted = false;

	list_for_each_entry_safe(entry, tmp, roi_list, list) {
		if (new_roi->data.start_addr <= entry->data.start_addr) {
			list_add_tail(&new_roi->list, &entry->list);
			inserted = true;
			break;
		}
	}

	if (!inserted) {
		list_add_tail(&new_roi->list, roi_list);
		entry = list_entry(new_roi->list.prev, struct hist_roi_node,
				   list);
		if (&entry->list == roi_list) {
			return;
		}
		if (smap_hist_roi_overlapping(entry, new_roi)) {
			smap_hist_middle_merge_roi(entry, new_roi);
			list_del(&new_roi->list);
			kfree(new_roi);
		}
		return;
	} else {
		tmp = list_entry(new_roi->list.prev, struct hist_roi_node,
				 list);
		if ((&tmp->list != roi_list) &&
		    (smap_hist_roi_overlapping(new_roi, tmp))) {
			smap_hist_middle_merge_roi(tmp, new_roi);
			list_del(&new_roi->list);
			kfree(new_roi);
			new_roi = tmp;
		}
	}

	while (&entry->list != roi_list) {
		tmp = list_entry(entry->list.next, struct hist_roi_node, list);
		if (!smap_hist_roi_overlapping(new_roi, entry)) {
			break;
		}

		smap_hist_middle_merge_roi(new_roi, entry);
		list_del(&entry->list);
		kfree(entry);
		entry = tmp;
	}
}

static void smap_hist_middle_update_roi_info(void)
{
	int i;
	struct hist_roi_node *pos;

	hist_mid->rois.total_count = 0;
	hist_mid->rois.total_length = 0;
	for (i = 0; i < hist_mid->rois.list_count; i++) {
		hist_mid->rois.list[i].length = 0;
		hist_mid->rois.list[i].count = 0;
		list_for_each_entry(pos, &hist_mid->rois.list[i].head, list) {
			hist_mid->rois.list[i].length += pos->data.length;
			hist_mid->rois.list[i].count++;
		}
		hist_mid->rois.total_count += hist_mid->rois.list[i].count;
		hist_mid->rois.total_length += hist_mid->rois.list[i].length;
	}
}

int smap_hist_middle_add_roi(uint64_t start_addr, size_t length)
{
	int i, ret;
	struct ub_hist_ba_info info;
	struct hist_roi_node *new_roi;

	if (hist_mid->status.flags.initialized) {
		pr_err("disable first before add roi\n");
		return -EBUSY;
	}

	new_roi = kmalloc(sizeof(*new_roi), GFP_KERNEL);
	if (!new_roi) {
		pr_err("malloc mem failed for new roi\n");
		return -ENOMEM;
	}

	/* make input roi align to 4K */
	new_roi->data.start_addr = align_down(start_addr, SIZE_4K);
	new_roi->data.length =
		align_down(start_addr + length + SIZE_4K - 1, SIZE_4K) -
		new_roi->data.start_addr;

	mutex_lock(&hist_mid->rois.roi_lock);
	for (i = 0; i < hist_mid->rois.list_count; i++) {
		if (ub_hist_query_ba_info(hist_mid->rois.list[i].ba_tag,
					  &info)) {
			ret = -ENODEV;
			break;
		}
		if (smap_hist_roi_within_ranges(new_roi, &info)) {
			smap_hist_middle_insert_roi(
				new_roi, &hist_mid->rois.list[i].head);
			smap_hist_middle_update_roi_info();
			mutex_unlock(&hist_mid->rois.roi_lock);
			return 0;
		}
	}

	kfree(new_roi);
	mutex_unlock(&hist_mid->rois.roi_lock);

	return ret;
}

static int smap_hist_middle_del_roi_inner(struct hist_roi_node *cur_roi,
					  uint64_t delete_start,
					  size_t delete_length,
					  struct list_head *current_list)
{
	unsigned long delete_end = delete_start + delete_length;
	unsigned long cur_roi_end =
		cur_roi->data.start_addr + cur_roi->data.length;

	/* no overlapping */
	if ((cur_roi->data.start_addr >= delete_end) ||
	    (cur_roi_end <= delete_start)) {
		return 0;
	}

	/* cur_roi roi is overlapped by roi to be deleted */
	if (cur_roi->data.start_addr >= delete_start &&
	    cur_roi_end <= delete_end) {
		list_del(&cur_roi->list);
		kfree(cur_roi);
		return 0;
	}

	/* roi to be deleted is overlapped by cur_roi roi */
	if (cur_roi->data.start_addr < delete_start &&
	    cur_roi_end > delete_end) {
		struct hist_roi_node *new_roi =
			kmalloc(sizeof(struct hist_roi_node), GFP_KERNEL);

		if (!new_roi) {
			pr_err("malloc mem failed for new roi\n");
			return -ENOMEM;
		}
		cur_roi->data.length = delete_start - cur_roi->data.start_addr;
		new_roi->data.start_addr = delete_end;
		new_roi->data.length = cur_roi_end - delete_end;
		smap_hist_middle_insert_roi(new_roi, current_list);
		return 0;
	}

	/* cur_roi roi's tail is overlapped */
	if (cur_roi->data.start_addr < delete_start) {
		cur_roi->data.length = delete_start - cur_roi->data.start_addr;
		return 0;
	}

	/* cur_roi roi's head is overlapped */
	cur_roi->data.length = cur_roi_end - delete_end;
	cur_roi->data.start_addr = delete_end;
	return 0;
}

int smap_hist_middle_del_roi(uint64_t start_addr, size_t length)
{
	int ret, i;
	struct hist_roi_node *cur_roi, *tmp;
	struct list_head *current_list;

	if (hist_mid->status.flags.initialized) {
		pr_err("need disable first before del roi\n");
		return -EBUSY;
	}

	mutex_lock(&hist_mid->rois.roi_lock);
	for (i = 0; i < hist_mid->rois.list_count; i++) {
		current_list = &hist_mid->rois.list[i].head;
		list_for_each_entry_safe(cur_roi, tmp, current_list, list) {
			ret = smap_hist_middle_del_roi_inner(
				cur_roi, start_addr, length, current_list);
			if (ret) {
				mutex_unlock(&hist_mid->rois.roi_lock);
				return ret;
			}
		}
	}
	smap_hist_middle_update_roi_info();
	mutex_unlock(&hist_mid->rois.roi_lock);
	return 0;
}

bool smap_hist_middle_query_addr_in_roi(phys_addr_t addr)
{
	int i;
	uint64_t roi_end;
	struct hist_roi_node *pos;

	mutex_lock(&hist_mid->rois.roi_lock);
	for (i = 0; i < hist_mid->rois.list_count; i++) {
		list_for_each_entry(pos, &hist_mid->rois.list[i].head, list) {
			roi_end = pos->data.start_addr + pos->data.length;
			if (addr >= pos->data.start_addr && addr < roi_end) {
				mutex_unlock(&hist_mid->rois.roi_lock);
				return true;
			}
		}
	}
	mutex_unlock(&hist_mid->rois.roi_lock);
	return false;
}

int smap_hist_middle_reset_roi(void)
{
	int i;
	struct hist_roi_node *pos, *n;

	if (hist_mid->status.flags.initialized) {
		pr_err("need disable first before reset roi\n");
		return -EBUSY;
	}

	mutex_lock(&hist_mid->rois.roi_lock);
	for (i = 0; i < hist_mid->rois.list_count; i++) {
		list_for_each_entry_safe(pos, n, &hist_mid->rois.list[i].head,
					 list) {
			list_del(&pos->list);
			kfree(pos);
		}
	}
	hist_mid->rois.total_count = 0;
	hist_mid->rois.total_length = 0;
	mutex_unlock(&hist_mid->rois.roi_lock);

	return 0;
}

int smap_hist_middle_set_scan_config(struct hist_scan_cfg *cfg_data)
{
	if (!cfg_data || !hist_mid) {
		pr_err("invalid input of hist-mid-set-config\n");
		return -EINVAL;
	}

	if (hist_mid->status.flags.initialized) {
		pr_err("need disable first before set config\n");
		return -EBUSY;
	}

	if (cfg_data->scan_mode >= SCAN_MODE_MAX) {
		pr_err("failed to set scan mode %d\n", cfg_data->scan_mode);
		return -EINVAL;
	}

	if (cfg_data->run_mode >= RUN_MODE_MAX) {
		pr_err("failed to set run mode %d\n", cfg_data->run_mode);
		return -EINVAL;
	}

	if (cfg_data->scan_interval == 0) {
		cfg_data->scan_interval = SCAN_TIME_INTERVAL_MIN;
	}

	if ((cfg_data->scan_interval < SCAN_TIME_INTERVAL_MIN) ||
	    (cfg_data->scan_interval > SCAN_TIME_INTERVAL_MAX)) {
		pr_err("failed to set scan interval %d\n",
		       cfg_data->scan_interval);
		return -EINVAL;
	}

	if (cfg_data->sort_enable == false) {
		cfg_data->result_ratio = SCAN_RESULT_RATIO_MAX;
	}

	if ((cfg_data->result_ratio < SCAN_RESULT_RATIO_MIN) ||
	    (cfg_data->result_ratio > SCAN_RESULT_RATIO_MAX)) {
		pr_err("failed to set result ratio %d\n",
		       cfg_data->result_ratio);
		return -EINVAL;
	}

	hist_mid->config = *cfg_data;

	return 0;
}

int smap_hist_middle_get_scan_config(struct hist_scan_cfg *cfg_data)
{
	if (!cfg_data) {
		return -EINVAL;
	}
	*cfg_data = hist_mid->config;

	return 0;
}

static void save_statistic_result_to_end(struct scan_work_desc *work_desc,
					 phys_addr_t *current_addr,
					 phys_addr_t window_end,
					 struct hist_roi_node *roi,
					 size_t *buffer_index,
					 uint16_t *result_buffer)
{
	unsigned int index;
	phys_addr_t record_addr;
	size_t record_size = 1ULL << work_desc->record_shift;
	phys_addr_t aligned_roi_start =
		align_down(roi->data.start_addr, record_size);
	phys_addr_t aligned_roi_end =
		align_up(roi->data.start_addr + roi->data.length, record_size);
	phys_addr_t aligned_current_start =
		align_down(*current_addr, record_size);

	record_addr = aligned_current_start < aligned_roi_start ?
			      aligned_roi_start :
			      aligned_current_start;
	index = BA_STS_VALUE_COUNT -
		((window_end - record_addr) >> work_desc->record_shift);
	while (record_addr < window_end) {
		if (record_addr >= aligned_roi_end) {
			break;
		}
		work_desc->result_buffer.result_pair[*buffer_index].address =
			record_addr;
		work_desc->result_buffer.result_pair[*buffer_index].count =
			result_buffer[index++];
		(*buffer_index)++;
		record_addr += record_size;
	}
	*current_addr = record_addr;
}

static int do_ba_statistic(struct ub_hist_ba_config *config, uint64_t ba_tag)
{
	int ret;

	config->regs.reg_ctrl.sts_enable = 1;
	ret = ub_hist_set_state(config, ba_tag);
	if (ret) {
		return ret;
	}
	unsigned long min_us = hist_mid->config.scan_interval * USEC_PER_MSEC;
	unsigned long max_us = min_us + SCAN_INTERVAL_RANGE_US;
	usleep_range(min_us, max_us);

	config->regs.reg_ctrl.sts_enable = 0;
	ret = ub_hist_set_state(config, ba_tag);
	if (ret) {
		return ret;
	}

	return 0;
}

static int smap_hist_scan_one_roi(struct smap_hist_scan_record *record,
				  struct ub_hist_ba_config *config,
				  struct hist_roi_node *roi,
				  struct scan_work_desc *work_desc,
				  struct ub_hist_ba_result *result)
{
	size_t window_size = 1ULL << work_desc->window_shift;
	if (record->current_addr < roi->data.start_addr) {
		record->current_addr =
			align_down(roi->data.start_addr, window_size);
	}

	if (record->window_end > record->current_addr) {
		save_statistic_result_to_end(work_desc, &record->current_addr,
					     record->window_end, roi,
					     &record->buffer_index,
					     (uint16_t *)result->buffer);
		return 0;
	}

	while (record->current_addr < roi->data.start_addr + roi->data.length) {
		record->window_end = record->current_addr + window_size;
		config->regs.reg_ctrl.sts_base_addr = record->current_addr >>
						      SHIFT_32M;
		if (do_ba_statistic(config, result->ba_tag) ||
		    ub_hist_get_statistic_result(result)) {
			hist_mid->status.flags.error = 1;
			hist_mid->status.err_code = ERR_CODE_BA_MATCH;
			return -EFAULT;
		}
		save_statistic_result_to_end(work_desc, &record->current_addr,
					     record->window_end, roi,
					     &record->buffer_index,
					     (uint16_t *)result->buffer);
	}
	return 0;
}

static int smap_hist_scan_work(void *data)
{
	int ret = 0;
	struct hist_roi_node *roi;
	enum scan_mode scan_mode = hist_mid->config.scan_mode;
	struct scan_work_desc *work_desc = (struct scan_work_desc *)data;
	struct ub_hist_ba_result *result;
	struct ub_hist_ba_config config;
	struct smap_hist_scan_record record = { 0 };

	result = kzalloc(sizeof(struct ub_hist_ba_result), GFP_KERNEL);
	if (!result) {
		hist_mid->status.flags.error = 1;
		hist_mid->status.err_code = ERR_CODE_GENERAL;
		pr_err("malloc mem failed for ba_result\n");
		complete(&work_desc->thread_cplt);
		return -ENOMEM;
	}

	config.mask = BA_CTRL_REG_CFG_MASK;
	result->ba_tag = work_desc->ba_tag;
	ret = ub_hist_get_state(&config, work_desc->ba_tag);
	if (ret) {
		hist_mid->status.flags.error = 1;
		hist_mid->status.err_code = ERR_CODE_BA_MATCH;
		kfree(result);
		complete(&work_desc->thread_cplt);
		return ret;
	}

	config.regs.reg_ctrl.sts_size =
		scan_mode == SCAN_WITH_2M ? STS_SIZE_2M : STS_SIZE_4K;
	config.regs.reg_ctrl.sts_rd_en = 1;
	config.regs.reg_ctrl.sts_wr_en = 1;

	list_for_each_entry(roi, work_desc->list_head, list) {
		ret = smap_hist_scan_one_roi(&record, &config, roi, work_desc,
					     result);
		if (ret)
			break;
	}

	kfree(result);
	complete(&work_desc->thread_cplt);
	return ret;
}

static int cmp_pair(const void *a, const void *b)
{
	return ((struct addr_count_pair *)b)->count -
	       ((struct addr_count_pair *)a)->count;
}

static int smap_hist_scan_roi_inner(int ba_count,
				    struct scan_work_desc *work_desc)
{
	int index, ret = 0;

	for (index = 0; index < ba_count; index++) {
		if (!hist_mid->rois.list[index].count) {
			continue;
		}

		work_desc[index].thread = kthread_run(smap_hist_scan_work,
						      &work_desc[index],
						      "ba_thread_%d", index);
		if (IS_ERR(work_desc[index].thread)) {
			pr_err("failed to create ba_thread_%d\n", index);
			hist_mid->status.flags.error = 1;
			hist_mid->status.err_code = ERR_CODE_GENERAL;
			ret = -ECHILD;
		}
	}

	for (index = 0; index < ba_count; index++) {
		if (!hist_mid->rois.list[index].count) {
			continue;
		}
		if (!IS_ERR(work_desc[index].thread)) {
			wait_for_completion(&work_desc[index].thread_cplt);
		}
	}

	return ret;
}

static unsigned int smap_hist_init_work(struct scan_work_desc *work_desc,
					int index,
					struct addr_count_pair *p_result)
{
	unsigned int pair_count;

	work_desc[index].list_head = &hist_mid->rois.list[index].head;
	if (hist_mid->config.scan_mode == SCAN_WITH_2M) {
		work_desc[index].window_shift = SHIFT_16G;
		work_desc[index].record_shift = SHIFT_2M;
	} else {
		work_desc[index].window_shift = SHIFT_32M;
		work_desc[index].record_shift = SHIFT_4K;
	}
	pair_count = hist_mid->rois.list[index].length >>
		     work_desc[index].record_shift;
	work_desc[index].result_buffer.result_pair = p_result;
	work_desc[index].result_buffer.pair_count = pair_count;
	init_completion(&work_desc[index].thread_cplt);

	return pair_count;
}

static int smap_hist_scan_roi(struct buffer_node *result)
{
	int ret, index, ba_count;
	unsigned int pair_count;
	struct scan_work_desc *work_desc;
	struct addr_count_pair *p_result;
	uint64_t *p_tags;

	ret = smap_hist_get_ba_tags(&p_tags, &ba_count);
	if (ret) {
		hist_mid->status.flags.error = 1;
		hist_mid->status.err_code = ERR_CODE_BA_MATCH;
		return -EINVAL;
	}

	for (index = 0; index < ba_count; index++) {
		if ((index >= hist_mid->rois.list_count) ||
		    (p_tags[index] != hist_mid->rois.list[index].ba_tag)) {
			kfree(p_tags);
			return -EINVAL;
		}
	}

	work_desc =
		kcalloc(ba_count, sizeof(struct scan_work_desc), GFP_KERNEL);
	if (!work_desc) {
		hist_mid->status.flags.error = 1;
		hist_mid->status.err_code = ERR_CODE_GENERAL;
		pr_err("malloc mem failed for work_desc\n");
		kfree(p_tags);
		return -ENOMEM;
	}

	result->used_count = 0;
	p_result = result->result_pair;
	for (index = 0; index < ba_count; index++) {
		work_desc[index].ba_tag = p_tags[index];
		if (!hist_mid->rois.list[index].count) {
			continue;
		}

		pair_count = smap_hist_init_work(work_desc, index, p_result);
		p_result += pair_count;
		result->used_count += pair_count;
	}

	ret = smap_hist_scan_roi_inner(ba_count, work_desc);

	kfree(work_desc);
	kfree(p_tags);
	return ret;
}

static int smap_hist_alloc_scan(void)
{
	int ret;
	struct buffer_node *result;

	if (!hist_mid->status.flags.running) {
		pr_err("context error, wrong running state\n");
		hist_mid->status.flags.error = 1;
		hist_mid->status.err_code = ERR_CODE_OPERATION;
		return -EFAULT;
	}

	spin_lock(&hist_mid->result.buffer_lock);
	if (!list_empty(&hist_mid->result.write_fifo)) {
		result = list_first_entry(&hist_mid->result.write_fifo,
					  struct buffer_node, list);
	} else {
		result = list_first_entry(&hist_mid->result.read_fifo,
					  struct buffer_node, list);
	}
	list_del(&result->list);
	spin_unlock(&hist_mid->result.buffer_lock);
	ret = smap_hist_scan_roi(result);

	spin_lock(&hist_mid->result.buffer_lock);
	list_add_tail(&result->list, &hist_mid->result.read_fifo);
	if (!ret) {
		hist_mid->status.flags.ready = 1;
	}
	spin_unlock(&hist_mid->result.buffer_lock);

	return ret;
}

static void smap_hist_scan_worker(struct work_struct *work)
{
	smap_hist_alloc_scan();
	hist_mid->status.flags.running = 0;
}

static int smap_hist_scan_thread_run(void *data)
{
	while (!kthread_should_stop()) {
		smap_hist_alloc_scan();
	}

	return 0;
}

union hist_status smap_hist_middle_run_status()
{
	return hist_mid->status;
}

int smap_hist_middle_get_hot_pages(struct addr_count_pair *result_pair,
				   u32 *result_count)
{
	int ret;
	struct buffer_node *node;
	u32 count = 0;

	*result_count = 0;
	if (hist_mid->config.run_mode == RUN_SYNC_BLOCKING) {
		ret = smap_hist_alloc_scan();
		if (ret) {
			return ret;
		}
	}

	if ((hist_mid->config.run_mode == RUN_ASYNC_BACKGROUND) &&
	    hist_mid->status.flags.running) {
		return -EBUSY;
	}

	if ((hist_mid->config.run_mode == RUN_ASYNC_THREADING) &&
	    !hist_mid->status.flags.ready) {
		return -EBUSY;
	}

	if (hist_mid->status.flags.error) {
		return -EFAULT;
	}

	spin_lock(&hist_mid->result.buffer_lock);
	node = list_first_entry(&hist_mid->result.read_fifo, struct buffer_node,
				list);
	list_del(&node->list);
	hist_mid->status.flags.ready = 0;
	spin_unlock(&hist_mid->result.buffer_lock);

	if (hist_mid->config.sort_enable) {
		sort(node->result_pair, node->used_count,
		     sizeof(struct addr_count_pair), cmp_pair, NULL);
	}

	if (node->used_count != 0) {
		count = max_t(u32, 1,
			      node->used_count * hist_mid->config.result_ratio /
				      SCAN_RESULT_RATIO_MAX);
		memcpy(result_pair, node->result_pair,
		       sizeof(struct addr_count_pair) * count);
	}

	*result_count = count;

	spin_lock(&hist_mid->result.buffer_lock);
	list_add_tail(&node->list, &hist_mid->result.write_fifo);
	spin_unlock(&hist_mid->result.buffer_lock);

	return 0;
}

static void smap_hist_middle_resource_deinit(void)
{
	struct buffer_node *node, *tmp;

	ub_hist_unlock_device();

	list_for_each_entry_safe(node, tmp, &hist_mid->result.read_fifo, list) {
		list_del(&node->list);
		kfree(node->result_pair);
		kfree(node);
	}

	list_for_each_entry_safe(node, tmp, &hist_mid->result.write_fifo,
				 list) {
		list_del(&node->list);
		kfree(node->result_pair);
		kfree(node);
	}
	hist_mid->status.flags.initialized = 0;
}

static int smap_hist_middle_resource_init(void)
{
	int i, ret;
	struct buffer_node *node;

	ret = ub_hist_lock_device();
	if (ret) {
		return ret;
	}

	spin_lock(&hist_mid->result.buffer_lock);
	INIT_LIST_HEAD(&hist_mid->result.read_fifo);
	INIT_LIST_HEAD(&hist_mid->result.write_fifo);
	spin_unlock(&hist_mid->result.buffer_lock);

	mutex_lock(&hist_mid->rois.roi_lock);
	for (i = 0; i < BACKEND_BUFFER_COUNT; i++) {
		node = kzalloc(sizeof(struct buffer_node), GFP_KERNEL);
		if (!node) {
			mutex_unlock(&hist_mid->rois.roi_lock);
			goto error_out;
		}
		node->pair_count =
			hist_mid->rois.total_length >>
			((hist_mid->config.scan_mode == SCAN_WITH_2M) ?
				 SHIFT_2M :
				 SHIFT_4K);
		node->result_pair = kcalloc(node->pair_count,
					    sizeof(struct addr_count_pair),
					    GFP_KERNEL);
		if (!node->result_pair) {
			kfree(node);
			mutex_unlock(&hist_mid->rois.roi_lock);
			goto error_out;
		}
		spin_lock(&hist_mid->result.buffer_lock);
		list_add_tail(&node->list, &hist_mid->result.write_fifo);
		spin_unlock(&hist_mid->result.buffer_lock);
	}
	mutex_unlock(&hist_mid->rois.roi_lock);
	hist_mid->status.flags.initialized = 1;
	return 0;

error_out:
	smap_hist_middle_resource_deinit();
	return -ENOMEM;
}

static bool smap_hist_check_config_validation(struct hist_scan_cfg *config)
{
	if (config->run_mode >= RUN_MODE_MAX) {
		return false;
	}

	if (config->scan_mode >= SCAN_MODE_MAX) {
		return false;
	}

	if (config->scan_interval < SCAN_TIME_INTERVAL_MIN ||
	    config->scan_interval > SCAN_TIME_INTERVAL_MAX) {
		return false;
	}

	if (config->result_ratio < SCAN_RESULT_RATIO_MIN ||
	    config->result_ratio > SCAN_RESULT_RATIO_MAX) {
		return false;
	}

	return true;
}

int smap_hist_middle_scan_enable(void)
{
	int ret;

	if (hist_mid->status.flags.running) {
		pr_warn("background thread has already been enabled\n");
		return 0;
	}

	if (!smap_hist_check_config_validation(&hist_mid->config)) {
		pr_err("wrong configuration\n");
		return -EINVAL;
	}

	if (!hist_mid->status.flags.initialized) {
		ret = smap_hist_middle_resource_init();
		if (ret) {
			pr_err("failed to init resources\n");
			return ret;
		}
	}

	hist_mid->status.flags.error = 0;
	switch (hist_mid->config.run_mode) {
	case RUN_SYNC_BLOCKING:
		hist_mid->status.flags.running = 1;
		break;
	case RUN_ASYNC_BACKGROUND:
		hist_mid->status.flags.running = 1;
		schedule_work(&hist_mid->work.async_work);
		break;
	case RUN_ASYNC_THREADING:
		hist_mid->status.flags.running = 1;
		hist_mid->work.scan_kthread = kthread_run(
			smap_hist_scan_thread_run, NULL, "hist-mid-scan");
		if (!hist_mid->work.scan_kthread) {
			pr_err("failed to create middleware scan thread\n");
			hist_mid->status.flags.running = 0;
			return -ECHILD;
		}
		break;
	default:
		hist_mid->status.flags.running = 0;
		return -EFAULT;
	}

	return 0;
}

int smap_hist_middle_scan_disable(void)
{
	if (hist_mid->config.run_mode == RUN_ASYNC_BACKGROUND) {
		cancel_work_sync(&hist_mid->work.async_work);
	}
	if (hist_mid->work.scan_kthread) {
		kthread_stop(hist_mid->work.scan_kthread);
		hist_mid->work.scan_kthread = NULL;
	}

	if (hist_mid->status.flags.initialized) {
		smap_hist_middle_resource_deinit();
	}

	hist_mid->status.value = 0;

	return 0;
}

void smap_hist_middle_reset(void)
{
	smap_hist_middle_scan_disable();
	smap_hist_middle_reset_roi();
}

int smap_hist_reinit_roi_list(void)
{
	int index, ret, ba_count;
	uint64_t *p_tags;

	if (hist_mid->rois.list) {
		smap_hist_middle_reset_roi();
		kfree(hist_mid->rois.list);
		hist_mid->rois.list_count = 0;
		hist_mid->rois.list = NULL;
	}

	ret = smap_hist_get_ba_tags(&p_tags, &ba_count);
	if (ret) {
		return ret;
	}

	hist_mid->rois.list =
		kcalloc(ba_count, sizeof(struct hist_roi_list), GFP_KERNEL);
	if (!hist_mid->rois.list) {
		pr_err("malloc failed for rois-list\n");
		kfree(p_tags);
		return -ENOMEM;
	}

	for (index = 0; index < ba_count; index++) {
		INIT_LIST_HEAD(&hist_mid->rois.list[index].head);
		hist_mid->rois.list[index].ba_tag = p_tags[index];
	}
	hist_mid->rois.list_count = ba_count;

	kfree(p_tags);

	return 0;
}

int smap_hist_mid_init(void)
{
	int ret;

	hist_mid = kzalloc(sizeof(struct smap_hist_mid), GFP_KERNEL);
	if (!hist_mid) {
		return -ENOMEM;
	}

	ret = smap_hist_reinit_roi_list();
	if (ret) {
		kfree(hist_mid);
		hist_mid = NULL;
		return ret;
	}

	mutex_init(&hist_mid->rois.roi_lock);
	INIT_WORK(&hist_mid->work.async_work, smap_hist_scan_worker);
	hist_mid->config.result_ratio = SCAN_RESULT_RATIO_MAX;
	hist_mid->config.scan_interval = SCAN_TIME_INTERVAL_MIN;
	pr_info("smap hist-mid init success.\n");
	return 0;
}

void smap_hist_mid_exit(void)
{
	smap_hist_middle_reset();
	kfree(hist_mid->rois.list);
	hist_mid->rois.list = NULL;
	kfree(hist_mid);
	hist_mid = NULL;
	pr_info("smap hist-mid exit success.\n");
}
