// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * Description: smap5.0 remote iomem module
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/mmzone.h>
#include <linux/numa.h>
#include <linux/memory.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/namei.h>

#include "numa.h"
#include "smap_msg.h"
#include "smap_migrate_wrapper.h"
#include "iomem.h"

#undef pr_fmt
#define pr_fmt(fmt) "smap_iomem: " fmt

#define REMOTE_RAM_NAME "System RAM (Remote)"
#define MEMID_IORES_PREFIX "MEMID_"
#define OBMM_SYS_DIR "/sys/devices/obmm"
#define OBMM_SHM_DIR "obmm_shmdev"
#define OBMM_FILE_SIZE 128
#define HEAD_MEMID 0
#define WAIT_MEMID_MS 500
#define MAX_MEMID_RETRY 2
#define HEX 16

LIST_HEAD(remote_ram_list);
static bool remote_ram_changed;

struct obmm_dev_info {
	struct list_head list;
	struct mutex lock;
};
struct obmm_dev_info obmm_dev = {
	.list = LIST_HEAD_INIT(obmm_dev.list),
	.lock = __MUTEX_INITIALIZER(obmm_dev.lock),
};

static void free_remote_ram(struct list_head *head)
{
	struct ram_segment *seg, *tmp;
	list_for_each_entry_safe(seg, tmp, head, node) {
		list_del(&seg->node);
		kfree(seg);
	}
}

static void copy_remote_ram(struct list_head *dst, struct list_head *src)
{
	struct ram_segment *seg, *tmp;
	list_for_each_entry_safe(seg, tmp, src, node) {
		list_move_tail(&seg->node, dst);
	}
}

static void __print_remote_ram(struct list_head *head)
{
	struct ram_segment *seg;
	list_for_each_entry(seg, head, node) {
		pr_info("[%d] %#llx-%#llx\n", seg->numa_node, seg->start,
			seg->end);
	}
}

static int insert_remote_ram(u64 pa_start, u64 pa_end, struct list_head *head)
{
	struct ram_segment *seg, *tmp;
	u64 start, end;
	unsigned long pfn;
	int nid;

	start = pa_start;
	while (start < pa_end) {
		pfn = PHYS_PFN(start);
		if (!pfn_valid(pfn) || !pfn_to_online_page(pfn)) {
			start += MIN_MEMORY_BLOCK_SIZE;
			continue;
		}
		nid = page_to_nid(pfn_to_online_page(pfn));
		if (nid == NUMA_NO_NODE) {
			return -EINVAL;
		}
		end = start + MIN_MEMORY_BLOCK_SIZE - 1;
		if (nid >= SMAP_MAX_NUMNODES) {
			start = end + 1;
			continue;
		}
		seg = kmalloc(sizeof(*seg), GFP_KERNEL);
		if (!seg) {
			return -ENOMEM;
		}

		end = MIN(pa_end, end);
		seg->start = start;
		seg->end = end;
		seg->numa_node = nid;

		if (list_empty(head)) {
			list_add_tail(&seg->node, head);
		} else {
			tmp = list_last_entry(head, struct ram_segment, node);
			if (seg->start == tmp->end + 1 &&
			    nid == tmp->numa_node) {
				tmp->end = seg->end;
				kfree(seg);
			} else {
				list_add_tail(&seg->node, head);
			}
		}

		start = end + 1;
	}
	return 0;
}

static int fixed_remote_ram(struct list_head *head)
{
	struct ram_segment *seg = kmalloc(sizeof(*seg), GFP_KERNEL);
	if (!seg)
		return -ENOMEM;
	seg->start = REMOTE_PA_START;
	seg->end = REMOTE_PA_END;
	seg->numa_node = REMOTE_NUMA_ID;
	list_add_tail(&seg->node, head);
	return 0;
}

static int update_resource(struct resource *r, void *arg)
{
	int ret;
	struct list_head *head = (struct list_head *)arg;

	if (!r || !arg)
		return -EINVAL;

	if (r->flags & IORESOURCE_SYSRAM_DRIVER_MANAGED) {
		ret = insert_remote_ram(r->start, r->end, head);
		if (ret) {
			free_remote_ram(head);
			return ret;
		}
	}
	return 0;
}

static int walk_system_ram_remote_range(struct list_head *head)
{
	if (!head)
		return -EINVAL;
	return walk_iomem_res_desc(IORES_DESC_NONE, IORESOURCE_SYSTEM_RAM, 0,
				   -1, head, update_resource);
}

static void free_obmm_dev(void)
{
	struct memid_range *mr, *tmp;

	mutex_lock(&obmm_dev.lock);
	list_for_each_entry_safe(mr, tmp, &obmm_dev.list, node) {
		list_del(&mr->node);
		kfree(mr);
	}
	mutex_unlock(&obmm_dev.lock);
}

static inline void inc_obmm_dev_seq(void)
{
	if (!list_empty(&obmm_dev.list)) {
		(list_first_entry(&obmm_dev.list, struct memid_range, node))
			->seq++;
	}
}

static int init_obmm_dev(void)
{
	struct memid_range *mr = kzalloc(sizeof(*mr), GFP_KERNEL);
	if (!mr) {
		return -ENOMEM;
	}
	list_add(&mr->node, &obmm_dev.list);
	return 0;
}

static void update_obmm_dev(u64 memid, u64 pa, u64 size)
{
	struct memid_range *mr, *tmp;
	u64 std_seq;
	bool found = false;

	if (unlikely(list_empty(&obmm_dev.list))) {
		pr_err("obmm_dev list should not be empty\n");
		return;
	}

	mr = list_first_entry(&obmm_dev.list, struct memid_range, node);
	std_seq = mr->seq;

	list_for_each_entry(mr, &obmm_dev.list, node) {
		if (mr->memid == memid) {
			pr_debug("update memid %llu, pa %#llx, size %#llx\n",
				 memid, pa, size);
			found = true;
			mr->memid = memid;
			mr->start = pa;
			mr->end = pa + size - 1;
			mr->seq = std_seq;
			break;
		}
	}
	if (found) {
		return;
	}
	pr_debug("add memid %llu, pa %#llx, size %#llx\n", memid, pa, size);

	tmp = kmalloc(sizeof(*tmp), GFP_KERNEL);
	if (!tmp) {
		pr_err("allocate mem for obmm_shmdev%llu failed\n", memid);
		return;
	}

	tmp->memid = memid;
	tmp->start = pa;
	tmp->end = pa + size - 1;
	tmp->seq = std_seq;
	list_add(&tmp->node, &obmm_dev.list);
}

static void clean_obmm_dev(void)
{
	struct memid_range *mr, *tmp;
	u64 std_seq;

	pr_debug("enter clean_obmm_dev\n");
	if (list_empty(&obmm_dev.list)) {
		return;
	}

	mr = list_first_entry(&obmm_dev.list, struct memid_range, node);
	std_seq = mr->seq;
	list_for_each_entry_safe(mr, tmp, &obmm_dev.list, node) {
		if (mr->seq != std_seq) {
			pr_info("delete memid %llu\n", mr->memid);
			list_del(&mr->node);
			kfree(mr);
		}
	}
	pr_debug("exit clean_obmm_dev\n");
}

static bool is_import_shmdev(const char *name)
{
	struct path path;
	char filepath[OBMM_FILE_SIZE] = { 0 };
	int ret;

	if (strncmp(name, OBMM_SHM_DIR, strlen(OBMM_SHM_DIR)) != 0) {
		pr_debug("%s is not obmm share mem directory\n", name);
		return false;
	}

	ret = scnprintf(filepath, sizeof(filepath), "%s/%s/import_info",
			OBMM_SYS_DIR, name);
	if (ret <= 0) {
		pr_err("build obmm shmdev path for %s failed\n", name);
		return false;
	}

	ret = kern_path(filepath, LOOKUP_DIRECTORY, &path);
	if (ret == 0) {
		path_put(&path);
		return true;
	}
	pr_debug("%s is not an import obmm share mem directory\n", name);
	return false;
}

static int extract_hex_content(const char *file_path, u64 *content)
{
	struct file *filp;
	char buf[OBMM_FILE_SIZE] = { 0 };
	int ret;
	loff_t pos = 0;
	u64 value;

	filp = filp_open(file_path, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		pr_err("failed to open file %s: %ld\n", file_path,
		       PTR_ERR(filp));
		return PTR_ERR(filp);
	}

	ret = kernel_read(filp, buf, OBMM_FILE_SIZE - 1, &pos);
	if (ret <= 0) {
		pr_err("failed to read file %s: %d\n", file_path, ret);
		goto error;
	}

	buf[ret] = '\0';
	pr_debug("content of %s: %s\n", file_path, buf);
	ret = kstrtoull(buf, HEX, &value);
	if (ret) {
		pr_err("failed to convert %s to u64: %d\n", buf, ret);
		goto error;
	}
	*content = value;

error:
	filp_close(filp, NULL);

	return ret;
}

struct read_obmm_callback {
	struct dir_context ctx;
	int ret;
};

static bool fill_obmmdev(struct dir_context *ctx, const char *name, int namelen,
			 loff_t offset, u64 ino, unsigned int d_type)
{
	int ret;
	char path[OBMM_FILE_SIZE] = { 0 };
	u64 memid, pa, size;
	struct read_obmm_callback *callback =
		container_of(ctx, struct read_obmm_callback, ctx);

	pr_debug("found %s, type %u\n", name, d_type);
	if (!is_import_shmdev(name)) {
		callback->ret = 0;
		return true;
	}

	ret = scnprintf(path, sizeof(path), "%s/%s/import_info/pa",
			OBMM_SYS_DIR, name);
	if (ret <= 0) {
		pr_err("build obmm shmdev pa path for %s failed\n", name);
		goto err;
	}
	ret = extract_hex_content(path, &pa);
	if (ret != 0) {
		goto err;
	}

	ret = scnprintf(path, sizeof(path), "%s/%s/size", OBMM_SYS_DIR, name);
	if (ret <= 0) {
		pr_err("build obmm shmdev size path for %s failed\n", name);
		goto err;
	}
	ret = extract_hex_content(path, &size);
	if (ret != 0) {
		goto err;
	}

	ret = sscanf(name, "obmm_shmdev%llu", &memid);
	if (ret != 1) {
		pr_err("build obmm shmdev filename for %llu failed\n", memid);
		ret = -ENOMEM;
		goto err;
	}
	pr_debug("memid: %llu, pa: %#llx, size: %#llx\n", memid, pa, size);

	update_obmm_dev(memid, pa, size);

	callback->ret = 0;
	return true;

err:
	callback->ret = ret;
	return false;
}

int iterate_obmm_dev_dir(void)
{
	int ret;
	struct file *dir_file;
	struct read_obmm_callback callback = {
		.ctx = {
			.pos = 0,
			.actor = fill_obmmdev,
		},
	};

	dir_file = filp_open(OBMM_SYS_DIR, O_RDONLY | O_DIRECTORY, 0);
	if (IS_ERR(dir_file)) {
		pr_err("failed to open obmm directory: %s\n", OBMM_SYS_DIR);
		return PTR_ERR(dir_file);
	}

	ret = iterate_dir(dir_file, &callback.ctx);
	if (ret == 0 && callback.ret != 0) {
		pr_err("iterate obmm_dev dir callback.ret is %d\n",
		       callback.ret);
		ret = callback.ret;
	}
	filp_close(dir_file, NULL);

	return ret;
}

int iterate_obmm_dev(void)
{
	int ret;

	pr_debug("enter iterate_obmm_dev\n");
	mutex_lock(&obmm_dev.lock);
	if (unlikely(list_empty(&obmm_dev.list))) {
		ret = init_obmm_dev();
		if (ret) {
			pr_err("init obmm_dev failed: %d\n", ret);
			goto out;
		}
	} else {
		inc_obmm_dev_seq();
	}

	ret = iterate_obmm_dev_dir();
	if (ret) {
		pr_err("iterate obmm_dev dir failed: %d\n", ret);
		goto out;
	}

	clean_obmm_dev();

out:
	mutex_unlock(&obmm_dev.lock);
	pr_debug("exit iterate_obmm_dev\n");

	return ret;
}

void release_remote_ram(void)
{
	free_remote_ram(&remote_ram_list);
	free_obmm_dev();
}

int refresh_remote_ram(void)
{
	int ret;
	LIST_HEAD(tmp_head);

	if (smap_scene == UB_QEMU_SCENE) {
		ret = fixed_remote_ram(&tmp_head);
	} else {
		ret = walk_system_ram_remote_range(&tmp_head);
	}
	if (ret) {
		return ret;
	}
	free_remote_ram(&remote_ram_list);
	copy_remote_ram(&remote_ram_list, &tmp_head);
	free_remote_ram(&tmp_head);
	__print_remote_ram(&remote_ram_list);
	remote_ram_changed = false;
	ret = iterate_obmm_dev();
	if (ret) {
		pr_err("iterate obmm dev failed: %d\n", ret);
	}
	return ret;
}

int calc_acidx_paddr_iomem(u64 index, int nid, u64 *paddr)
{
	struct ram_segment *seg;
	u64 range;
	int shift = is_smap_pg_huge() ? TWO_MEGA_SHIFT : PAGE_SHIFT;
	u64 tmp_index = index << shift;

	list_for_each_entry(seg, &remote_ram_list, node) {
		if (seg->numa_node != nid)
			continue;
		range = seg->end - seg->start + 1;
		if (tmp_index >= range) {
			tmp_index -= range;
			continue;
		}
		*paddr = seg->start + tmp_index;
		return 0;
	}
	return -ERANGE;
}

int find_range_by_memid(u64 memid, u64 *start, u64 *end)
{
	struct memid_range *mr;
	bool found = false;
	int retry_count = MAX_MEMID_RETRY + 1;

	do {
		if (retry_count < MAX_MEMID_RETRY) {
			pr_warn("there are %d chances left to find memid %llu range\n",
				retry_count, memid);
		}
		mutex_lock(&obmm_dev.lock);
		list_for_each_entry(mr, &obmm_dev.list, node) {
			if (mr->memid == memid) {
				found = true;
				*start = mr->start;
				*end = mr->end;
				break;
			}
		}
		mutex_unlock(&obmm_dev.lock);
		if (!found) {
			msleep(WAIT_MEMID_MS);
		}
		retry_count--;
	} while (!found && retry_count > 0);

	return found ? 0 : -ENOENT;
}
