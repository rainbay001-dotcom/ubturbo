// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: SMAP : hist_ops
 */
#include <asm/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/vmalloc.h>
#include <linux/mmzone.h>
#include <linux/pfn.h>
#include <linux/sort.h>
#include <linux/kthread.h>
#include <linux/minmax.h>
#include <linux/cpumask.h>
#include <linux/topology.h>

#include "access_iomem.h"
#include "access_acpi_mem.h"
#include "smap_hist_mid.h"
#include "ub_hist.h"
#include "hist_ops.h"

#define MAX_READ_TRY (2)
#define HOT_WINDOW_RATIO (10)
static struct smap_hist_dev g_smap_hist_dev;

static inline u64 align_addr(u64 addr, u32 low_bit_len)
{
	return (addr >> low_bit_len) << low_bit_len;
}

static inline bool addr_is_aligned(u64 addr, u32 low_bit_len)
{
	return !(addr & ((1ULL << low_bit_len) - 1));
}

static inline u64 align_addr_by_sts_size(u64 addr, u8 sts_size)
{
	if (sts_size == STS_SIZE_4K) {
		return align_addr(addr, HIST_ADDR_SHIFT_32M);
	}
	return align_addr(addr, HIST_ADDR_SHIFT_16G);
}

static bool addr_seg_is_valid(struct segs_info *info)
{
	u32 i;
	u64 start, size;
	if (!info) {
		pr_err("input info is null\n");
		return false;
	}
	if (!info->segs) {
		pr_err("input info->segs is null\n");
		return false;
	}
	for (i = 0; i < info->cnt; i++) {
		start = info->segs[i].start;
		size = info->segs[i].size;
		if (!addr_is_aligned(start, HIST_ADDR_SHIFT_32M) ||
		    !addr_is_aligned(start + size, HIST_ADDR_SHIFT_32M)) {
			pr_err("input addr seg[%d] start%llu and size %llu is not 32M-aligned\n",
			       i, start, size);
			return false;
		}
	}
	return true;
}

/* 按照start字段升序排序 */
static int addr_seg_cmp_start(const void *seg1, const void *seg2)
{
	struct addr_seg *s1 = (struct addr_seg *)seg1;
	struct addr_seg *s2 = (struct addr_seg *)seg2;
	if (s1->start > s2->start) {
		return 1;
	} else if (s1->start < s2->start) {
		return -1;
	} else {
		return 0;
	}
}

/* 按照max字段降序排序 */
static int addr_seg_cmp_max(const void *win1, const void *win2)
{
	struct addr_seg *w1, *w2;
	w1 = (struct addr_seg *)win1;
	w2 = (struct addr_seg *)win2;
	return w2->max - w1->max;
}

static inline u64 seg_end(struct addr_seg *seg)
{
	return seg->start + seg->size - 1;
}

static inline bool addr_seg_is_continuous_16g(struct addr_seg *seg1,
					      struct addr_seg *seg2)
{
	if (seg2->start - seg_end(seg1) <= SIZE_16G) {
		return true;
	}
	return false;
}

static u32 count_nr_windows(struct addr_seg *segs, int cnt, u8 sts_size)
{
	u64 start, end;
	u32 total_scans = 0;
	int i;
	u32 shift = sts_size == STS_SIZE_2M ? HIST_ADDR_SHIFT_16G :
					      HIST_ADDR_SHIFT_32M;
	for (i = 0; i < cnt; i++) {
		start = segs[i].start >> shift;
		end = seg_end(&segs[i]) >> shift;
		total_scans += (end - start) + 1;
	}
	return total_scans;
}

static void align_segments(struct addr_seg *segs, int cnt, u8 sts_size)
{
	u32 hist_addr_shift = sts_size == STS_SIZE_2M ? HIST_ADDR_SHIFT_16G :
							HIST_ADDR_SHIFT_32M;
	u64 hist_win_size = sts_size == STS_SIZE_2M ? SIZE_16G : SIZE_32M;
	for (int i = 0; i < cnt; i++) {
		u32 nr_wins = count_nr_windows(&segs[i], 1, sts_size);
		segs[i].start = align_addr(segs[i].start, hist_addr_shift);
		segs[i].size = hist_win_size * nr_wins;
	}
}

static int merge_segments(struct addr_seg *segs, int cnt)
{
	int i, merged_cnt = 0;
	for (i = 1; i < cnt; i++) {
		if (addr_seg_is_continuous_16g(&segs[merged_cnt], &segs[i])) {
			segs[merged_cnt].size =
				max_t(u64, seg_end(&segs[merged_cnt]),
				      seg_end(&segs[i])) -
				segs[merged_cnt].start + 1;
		} else {
			merged_cnt++;
			segs[merged_cnt] = segs[i];
		}
	}
	return merged_cnt + 1;
}

static inline bool is_intersect(struct addr_seg *seg1, struct addr_seg *seg2,
				u64 *inter_start, u64 *inter_end)
{
	if (seg_end(seg1) < seg2->start || seg_end(seg2) < seg1->start) {
		return false;
	}
	/* 计算交集的起始地址和结束地址 */
	*inter_start = (seg1->start > seg2->start) ? seg1->start : seg2->start;
	*inter_end = (seg_end(seg1) < seg_end(seg2)) ? seg_end(seg1) :
						       seg_end(seg2);

	return true;
}

static void calc_32m_hot_wins(struct segs_info *info, u16 *buf,
			      struct segs_info *win_info)
{
	u32 seg_pages = 0, win_cnt = 0;
	unsigned int i;
	for (i = 0; i < info->cnt; i++) {
		u64 start = info->segs[i].start;
		while (start < seg_end(&info->segs[i])) {
			/* 在当前对齐的32MB范围内计算最大值 */
			u16 max_val = 0, nr = SIZE_32M / SIZE_2M;
			u32 offset = (start - info->segs[i].start) >>
				     HIST_ADDR_SHIFT_2M;
			u16 *buf_ptr = &buf[seg_pages + offset];
			while (nr--) {
				max_val = max_t(u16, max_val, *buf_ptr++);
			}
			/* 越界保护 */
			if (win_cnt >= win_info->cnt) {
				pr_err("out-of-bounds when calc wins.[nr_wins:%u]\n",
				       win_info->cnt);
				start += SIZE_32M;
				continue;
			}
			win_info->segs[win_cnt].start = start;
			win_info->segs[win_cnt].size = SIZE_32M;
			win_info->segs[win_cnt].max = max_val;
			win_cnt++;

			start += SIZE_32M;
		}
		seg_pages += info->segs[i].size >> HIST_ADDR_SHIFT_2M;
	}
}

static int generate_aligned_16gb_wins_info(struct segs_info *win_info,
					   struct segs_info *info)
{
	int merged_cnt;
	u32 nr_wins = 0, total_wins;
	struct addr_seg *merged_segs, *aligned_segs;

	if (info->cnt == 0)
		return -EINVAL;

	merged_segs = vzalloc(info->cnt * sizeof(struct addr_seg));
	if (!merged_segs) {
		return -ENOMEM;
	}
	memcpy(merged_segs, info->segs, info->cnt * sizeof(struct addr_seg));
	merged_cnt = merge_segments(merged_segs, info->cnt);
	align_segments(merged_segs, merged_cnt, STS_SIZE_2M);
	total_wins = count_nr_windows(merged_segs, merged_cnt, STS_SIZE_2M);
	if (total_wins == 0) {
		vfree(merged_segs);
		return -EINVAL;
	}
	aligned_segs = vzalloc(total_wins * sizeof(struct addr_seg));
	if (!aligned_segs) {
		vfree(merged_segs);
		return -ENOMEM;
	}
	for (int i = 0; i < merged_cnt; i++) {
		u64 start = merged_segs[i].start;
		u64 size = merged_segs[i].size;
		while (size > 0) {
			if (nr_wins >= total_wins) {
				pr_err("out-of-bounds when creating segs.[i:%d, total:%d]\n",
				       nr_wins, total_wins);
				vfree(merged_segs);
				vfree(aligned_segs);
				return -EINVAL;
			}
			aligned_segs[nr_wins].start = start;
			aligned_segs[nr_wins].size = SIZE_16G;
			start += SIZE_16G;
			size -= SIZE_16G;
			nr_wins++;
		}
	}
	vfree(merged_segs);
	if (nr_wins == 0) {
		vfree(aligned_segs);
		return -EINVAL;
	}
	win_info->cnt = nr_wins;
	win_info->segs = aligned_segs;
	return 0;
}

static int generate_aligned_32m_wins_info(struct segs_info *win_info,
					  struct segs_info *info, u16 *buf)
{
	struct addr_seg *wins_4k;
	u32 nr_wins_4k = count_nr_windows(info->segs, info->cnt, STS_SIZE_4K);
	if (nr_wins_4k == 0) {
		return -EINVAL;
	}
	wins_4k = vzalloc(nr_wins_4k * sizeof(struct addr_seg));
	if (!wins_4k) {
		return -ENOMEM;
	}
	win_info->cnt = nr_wins_4k;
	win_info->segs = wins_4k;
	calc_32m_hot_wins(info, buf, win_info);
	sort(win_info->segs, win_info->cnt, sizeof(struct addr_seg),
	     addr_seg_cmp_max, NULL);
	/* 默认取前10%最热窗口 */
	win_info->cnt = max_t(u32, 1, win_info->cnt / HOT_WINDOW_RATIO);
	return 0;
}

static void copy_actc_to_buf(struct segs_info *info, struct addr_seg *seg,
			     u16 *dst_buf, struct addr_count_pair *actc_pair,
			     u32 buf_len, u8 sts_size)
{
	u32 shift = sts_size == STS_SIZE_2M ? HIST_ADDR_SHIFT_2M :
					      HIST_ADDR_SHIFT_4K;
	u64 inter_start, inter_end, inter_pages, j;
	u64 seg_offset, hist_offset, seg_pages = 0;
	unsigned int i;
	for (i = 0; i < info->cnt; i++) {
		if (is_intersect(&info->segs[i], seg, &inter_start,
				 &inter_end)) {
			/* 拷贝actc数据到目标buffer */
			hist_offset = (inter_start - seg->start) >> shift;
			seg_offset = (inter_start - info->segs[i].start) >>
				     shift;
			inter_pages = (inter_end - inter_start + 1) >> shift;
			if (seg_pages + seg_offset + inter_pages > buf_len) {
				pr_err("out-of-bounds when coping to dst_buf.[segp:%llu, offset:%llu, inter:%llu]\n",
				       seg_pages, seg_offset, inter_pages);
				continue;
			}
			u16 *dst = dst_buf + seg_pages + seg_offset;
			for (j = hist_offset; j < (hist_offset + inter_pages);
			     j++) {
				*dst++ = actc_pair[j].count;
			}
		}
		seg_pages += info->segs[i].size >> shift;
	}
}

static void clear_actc_buf(void)
{
	struct smap_hist_dev *dev = &g_smap_hist_dev;
	if (dev->buf) {
		memset(dev->buf, 0, dev->pgcount * sizeof(u16));
	}
}

static int smap_hist_middle_read(u64 start, u64 size,
				 struct addr_count_pair *actc_pair,
				 u32 *pair_cnt)
{
	pr_debug("hist scan segment: [%#llx - %#llx]\n", start,
		 start + size - 1);
	int ret = smap_hist_middle_reset_roi();
	if (ret) {
		pr_err("smap_hist_middle_reset_roi error (%d)\n", ret);
		return ret;
	}
	ret = smap_hist_middle_add_roi(start, size);
	if (ret) {
		pr_err("smap hist middle add roi error (%d)\n", ret);
		return ret;
	}
	ret = smap_hist_middle_scan_enable();
	if (ret) {
		pr_err("smap hist middle scan enable error (%d)\n", ret);
		return ret;
	}
	ret = smap_hist_middle_get_hot_pages(actc_pair, pair_cnt);
	if (ret) {
		pr_err("smap_hist_middle_get_hot_pages error (%d)\n", ret);
		return ret;
	}
	ret = smap_hist_middle_scan_disable();
	if (ret) {
		pr_err("smap hist middle scan disable error (%d)\n", ret);
	}
	if (g_smap_hist_dev.abort_flag) {
		pr_debug("hist scan paused\n");
		g_smap_hist_dev.abort_flag = false;
		ret = -EAGAIN;
	}
	return ret;
}

/* 滑窗扫描 */
static int do_hist_scan_sliding(struct segs_info *info, u32 scan_time, u16 *buf,
				u32 buf_len, u8 sts_size)
{
	int ret;
	u32 i, pair_cnt;
	u64 start, size;
	struct addr_count_pair *actc_pair;
	actc_pair = kmalloc(sizeof(struct addr_count_pair) * HIST_STS_VALUE_NUM,
			    GFP_KERNEL);
	if (!actc_pair) {
		pr_err("malloc mem failed for addr_count_pair\n");
		return -ENOMEM;
	}
	struct segs_info win_info;
	/* 1. 生成地址段对齐的段信息 */
	if (sts_size == STS_SIZE_2M) {
		ret = generate_aligned_16gb_wins_info(&win_info, info);
	} else {
		ret = generate_aligned_32m_wins_info(&win_info, info, buf);
	}
	if (ret) {
		pr_err("generate merged segs info failed. ret: %d\n", ret);
		kfree(actc_pair);
		return ret;
	}
	/* 2. 计算单次硬件扫描的时间scan_time */
	scan_time = max_t(u32, 1, scan_time / win_info.cnt);
	struct hist_scan_cfg hist_cfg = {
		.sort_enable = false,
		.run_mode = RUN_SYNC_BLOCKING,
		.scan_mode = sts_size,
		.scan_interval = scan_time,
	};
	ret = smap_hist_middle_set_scan_config(&hist_cfg);
	if (ret) {
		goto done;
	}
	/* 3. 按照段信息执行扫描 */
	clear_actc_buf();
	for (i = 0; i < win_info.cnt; i++) {
		start = win_info.segs[i].start;
		size = win_info.segs[i].size;
		ret = smap_hist_middle_read(start, size, actc_pair, &pair_cnt);
		if (ret) {
			goto done;
		}
		copy_actc_to_buf(info, &win_info.segs[i], buf, actc_pair,
				 buf_len, sts_size);
	}
done:
	vfree(win_info.segs);
	kfree(actc_pair);
	return ret;
}

/* 核心扫描函数 */
static int hist_scan_sliding(struct segs_info *info, u32 scan_time_total,
			     u16 *buf, u32 buf_len, u32 pgsize)
{
	int ret;
	/* 4K模式执行多粒度滑窗，2M模式执行单粒度滑窗 */
	bool do_multi_gran = pgsize == SIZE_4K;
	if (do_multi_gran) {
		scan_time_total >>= 1;
	}
	/* 检查地址段是否合法，至少是32M对齐的地址段 */
	if (!addr_seg_is_valid(info)) {
		pr_err("addr seg is invalid\n");
		return -EINVAL;
	}
	if (!scan_time_total) {
		pr_err("zero scan time is invalid\n");
		return -EINVAL;
	}
	if (!buf) {
		pr_err("input buf is null\n");
		return -EINVAL;
	}
	/* 执行 2M 粒度扫描操作 */
	ret = do_hist_scan_sliding(info, scan_time_total, buf, buf_len,
				   STS_SIZE_2M);
	if (ret) {
		pr_debug("do hist scan sliding 2m failed. ret: %d\n", ret);
		return ret;
	}
	if (!do_multi_gran) {
		return 0;
	}
	/* 执行 4K 粒度扫描操作 */
	ret = do_hist_scan_sliding(info, scan_time_total, buf, buf_len,
				   STS_SIZE_4K);
	if (ret) {
		pr_debug("do hist scan sliding 4k failed. ret: %d\n", ret);
		return ret;
	}
	return 0;
}

static inline void add_to_actc_data(u16 *dst, u16 *src, int len)
{
	int i, sum;
	for (i = 0; i < len; i++) {
		sum = dst[i] + src[i];
		if (likely(sum < U16_MAX)) {
			dst[i] = sum;
		} else {
			dst[i] = U16_MAX;
		}
	}
}

void fetch_hist_actc_buf(u16 *dst_buf, struct addr_seg *seg)
{
	struct smap_hist_dev *dev = &g_smap_hist_dev;
	struct segs_info *info = &dev->info;
	u64 inter_start, inter_end, inter_pages;
	u64 offset, seg_pages = 0;
	u32 shift = dev->pgsize == SIZE_2M ? HIST_ADDR_SHIFT_2M :
					     HIST_ADDR_SHIFT_4K;
	unsigned int i;
	for (i = 0; i < info->cnt; i++) {
		if (is_intersect(&info->segs[i], seg, &inter_start,
				 &inter_end)) {
			/* 拷贝actc数据到目标buffer */
			offset = (inter_start - info->segs[i].start) >> shift;
			inter_pages = (inter_end - inter_start + 1) >> shift;
			if (seg_pages + offset + inter_pages > dev->pgcount) {
				pr_warn("out-of-bounds %llu > %u\n",
					seg_pages + offset + inter_pages,
					dev->pgcount);
				return;
			}
			/* 没有异常状态时，以读清方式copy访存数据 */
			if (!dev->status.status_all) {
				add_to_actc_data(dst_buf,
						 dev->buf + seg_pages + offset,
						 inter_pages);
			}
		}
		seg_pages += info->segs[i].size >> shift;
	}
}

void hist_update_pgsize(u32 pgsize)
{
	struct smap_hist_dev *dev = &g_smap_hist_dev;
	if (pgsize != dev->pgsize) {
		dev->status.flag.new_pgsize = pgsize;
	}
}

void hist_set_iomem(void)
{
	struct smap_hist_dev *dev = &g_smap_hist_dev;
	if (!dev->status.flag.should_update_iomem) {
		dev->status.flag.should_update_iomem = true;
	}
}

void hist_thread_resume(void)
{
	struct smap_hist_dev *dev = &g_smap_hist_dev;
	dev->thread_enable = true;
}

void hist_thread_pause(void)
{
	struct smap_hist_dev *dev = &g_smap_hist_dev;
	dev->thread_enable = false;
	dev->abort_flag = true;
}

void hist_set_flush_actc_cb(flush_actc cb)
{
	struct smap_hist_dev *dev = &g_smap_hist_dev;
	dev->flush_actc = cb;
}

static inline void flush_actc_data(struct smap_hist_dev *dev)
{
	if (dev->flush_actc) {
		dev->flush_actc();
	}
}

static unsigned int get_cpu_socket_count(void)
{
	unsigned int i, cpu;
	int socket_id;
	unsigned int socket_count = 0;
	bool found;
	int *socket_ids;
	unsigned int max_cpus = num_possible_cpus();

	socket_ids = kzalloc(max_cpus * sizeof(int), GFP_KERNEL);
	if (!socket_ids) {
		pr_err("failed to allocate memory for socket IDs\n");
		return -ENOMEM;
	}
	for_each_online_cpu(cpu) {
		socket_id = topology_physical_package_id(cpu);
		found = false;
		for (i = 0; i < socket_count; i++) {
			if (socket_ids[i] == socket_id) {
				found = true;
				break;
			}
		}
		if (!found) {
			socket_ids[socket_count++] = socket_id;
		}
		if (socket_count >= max_cpus) {
			break;
		}
	}
	kfree(socket_ids);
	pr_info("%u cpu(s) belong to %u socket(s)\n", max_cpus, socket_count);
	return socket_count;
}

static inline unsigned int get_remote_ram_segs_locked(void)
{
	unsigned int nr_segs = 0;
	struct ram_segment *seg, *tmp;

	list_for_each_entry_safe(seg, tmp, &remote_ram_list, node) {
		if (seg->numa_node < nr_local_numa) {
			continue;
		}
		nr_segs++;
	}
	return nr_segs;
}

static int addr_segs_init(struct smap_hist_dev *dev, u32 pgsize)
{
	unsigned int i = 0, nr_segs = 0;
	u32 pgcount = 0;
	struct ram_segment *seg, *tmp;
	u32 shift = pgsize == SIZE_2M ? HIST_ADDR_SHIFT_2M : HIST_ADDR_SHIFT_4K;
	struct addr_seg *tmp_addr_segs;

	read_lock(&rem_ram_list_lock);
	nr_segs = get_remote_ram_segs_locked();
	read_unlock(&rem_ram_list_lock);
	if (nr_segs == 0) {
		dev->info.cnt = 0;
		dev->info.segs = NULL;
		dev->pgcount = 0;
		dev->pgsize = pgsize;
		pr_info("page count is 0\n");
		return 0;
	}
	tmp_addr_segs = vzalloc(nr_segs * sizeof(struct addr_seg));
	if (!tmp_addr_segs) {
		pr_err("malloc mem failed for info segs\n");
		return -ENOMEM;
	}

	read_lock(&rem_ram_list_lock);
	dev->info.cnt = get_remote_ram_segs_locked();
	if (dev->info.cnt != nr_segs) {
		read_unlock(&rem_ram_list_lock);
		pr_err("dev->info.cnt %d != %d\n", dev->info.cnt, nr_segs);
		vfree(tmp_addr_segs);
		return -EINVAL;
	}
	dev->info.segs = tmp_addr_segs;
	list_for_each_entry_safe(seg, tmp, &remote_ram_list, node) {
		if (seg->numa_node < nr_local_numa)
			continue;
		dev->info.segs[i].start = seg->start;
		dev->info.segs[i].size = seg->end - seg->start + 1;
		pgcount += (dev->info.segs[i].size >> shift);
		i++;
	}
	read_unlock(&rem_ram_list_lock);
	sort(dev->info.segs, dev->info.cnt, sizeof(struct addr_seg),
	     addr_seg_cmp_start, NULL);
	dev->pgcount = pgcount;
	pr_info("page count is updated to %u\n", pgcount);
	dev->pgsize = pgsize;
	return 0;
}

static void addr_segs_deinit(struct smap_hist_dev *dev)
{
	if (dev->info.segs) {
		pr_info("deleted addr segs.\n");
		vfree(dev->info.segs);
		dev->info.segs = NULL;
	}
}

static int hist_buffer_init(struct smap_hist_dev *dev)
{
	int ret;
	if (!dev->pgcount) {
		dev->buf = NULL;
		return 0;
	}
	dev->buf = vzalloc(dev->pgcount * sizeof(u16));
	if (!dev->buf) {
		pr_err("malloc tmp buf for hist dev failed. ret:%d\n", ret);
		return -ENOMEM;
	}
	return 0;
}

static void hist_buffer_deinit(struct smap_hist_dev *dev)
{
	if (dev->buf) {
		vfree(dev->buf);
		dev->buf = NULL;
	}
}

static int hist_pginfo_reinit(struct smap_hist_dev *dev, u32 pgsize_new)
{
	int ret;
	addr_segs_deinit(dev);
	ret = addr_segs_init(dev, pgsize_new);
	if (ret) {
		pr_err("reinit addr segs failed. ret: %d\n", ret);
		return -ENOMEM;
	}
	hist_buffer_deinit(dev);
	ret = hist_buffer_init(dev);
	if (ret) {
		addr_segs_deinit(dev);
		pr_err("reinit hist buffer failed. ret: %d\n", ret);
		return -ENOMEM;
	}
	return 0;
}

static int scan_thread_run(void *data)
{
	int ret;
	struct smap_hist_dev *dev = &g_smap_hist_dev;

	while (!kthread_should_stop()) {
		if (dev->status.status_all) {
			u32 pgsize = dev->status.flag.new_pgsize ?: dev->pgsize;
			pr_info("hist pginfo reinit. status: %#x\n",
				dev->status.status_all);
			ret = hist_pginfo_reinit(dev, pgsize);
			if (ret) {
				pr_err("hist pginfo reinit failed. ret: %d\n",
				       ret);
				msleep(THREAD_SLEEP);
				continue;
			}
		}
		dev->status.status_all = 0;

		if (!dev->thread_enable || !dev->pgcount) {
			msleep(1);
			continue;
		}

		ret = hist_scan_sliding(&dev->info, dev->period, dev->buf,
					dev->pgcount, dev->pgsize);
		if (ret) {
			pr_debug("hist scan sliding failed. ret: %d\n", ret);
			msleep(THREAD_SLEEP);
			continue;
		}
		if (!dev->status.status_all) {
			pr_debug("hist: flush actc data\n");
			flush_actc_data(dev);
		} else {
			pr_info("skip hist scan sliding, status: %#x\n",
				dev->status.status_all);
		}
	}
	return 0;
}

static int scan_thread_init(struct smap_hist_dev *dev)
{
	dev->kthread = kthread_run(scan_thread_run, NULL, "hist-scan-thread");
	if (!dev->kthread) {
		pr_err("create hist scan thread failed.\n");
		return -ECHILD;
	}

	return 0;
}

static void scan_thread_deinit(struct smap_hist_dev *dev)
{
	if (dev->kthread) {
		pr_info("deleted hist scan thread.\n");
		kthread_stop(dev->kthread);
		dev->kthread = NULL;
	}
}

struct smap_hist_dev *get_hist_dev(void)
{
	return &g_smap_hist_dev;
}

void hist_deinit(void)
{
	struct smap_hist_dev *dev = &g_smap_hist_dev;
	scan_thread_deinit(dev);
	hist_buffer_deinit(dev);
	addr_segs_deinit(dev);
	smap_hist_mid_exit();
	ub_hist_exit();
}

static int smap_hist_chip_init(unsigned int nr_socket, bool is_ub_qemu)
{
	int ret;
	u8 type;
	if (is_ub_qemu) {
		type = nr_socket > 1 ? PLATFORM_QEMU_TWO_SOCKETS :
				       PLATFORM_QEMU_ONE_SOCKET;
	} else {
		type = nr_socket > 1 ? PLATFORM_EVB_TWO_SOCKETS :
				       PLATFORM_EVB_ONE_SOCKET;
	}
	ret = ub_hist_init(type);
	if (ret) {
		return ret;
	}
	ret = smap_hist_mid_init();
	if (ret) {
		ub_hist_exit();
		return ret;
	}
	return ret;
}

int hist_init(u32 pgsize, bool is_ub_qemu)
{
	struct smap_hist_dev *dev = &g_smap_hist_dev;
	int ret;
	unsigned int nr_socket = get_cpu_socket_count();

	ret = smap_hist_chip_init(nr_socket, is_ub_qemu);
	if (ret) {
		return ret;
	}

	dev->period = HIST_THREAD_PERIOD;

	ret = addr_segs_init(dev, pgsize);
	if (ret) {
		goto free_segs;
	}

	ret = hist_buffer_init(dev);
	if (ret) {
		goto free_buf;
	}

	ret = scan_thread_init(dev);
	if (ret) {
		goto free_thread;
	}

	pr_info("Smap hist dev init success\n");
	return 0;

free_thread:
	scan_thread_deinit(dev);
free_buf:
	hist_buffer_deinit(dev);
free_segs:
	addr_segs_deinit(dev);
	smap_hist_mid_exit();
	ub_hist_exit();
	return ret;
}
