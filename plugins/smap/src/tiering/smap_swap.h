/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: SMAP NVMe Swap - Header definitions
 */

#ifndef _SMAP_SWAP_H
#define _SMAP_SWAP_H

#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/completion.h>
#include <linux/hashtable.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>

/* Forward declarations */
struct smap_swap_map;
struct smap_swap_device;

/* Constants */
#define SMAP_SWAP_MAGIC		0x534D4150	/* "SMAP" */
#define MAX_SWAP_BATCH		64
#define SWAP_SLOT_4K		PAGE_SIZE
#define SWAP_SLOT_2M		(2UL * 1024 * 1024)
#define SWAP_MAPPING_BITS	14		/* 2^14 = 16384 buckets */
#define MAX_SWAP_ADDRS		4096

/*
 * Swap slot -> process mapping entry, stored in a kernel hash table.
 * Used to track which (pid, vaddr) owns each NVMe slot so that
 * swap-in can restore the page to the correct process.
 */
struct smap_swap_mapping {
	u32 slot_index;
	pid_t pid;
	u64 vaddr;
	struct hlist_node node;
};

/*
 * Bitmap-based swap slot allocator.
 */
struct smap_swap_map {
	unsigned long *bitmap;
	u32 total_slots;
	u32 slot_size;
	atomic_t free_slots;
	spinlock_t lock;
};

/*
 * Per-device state for the NVMe swap backend.
 */
struct smap_swap_device {
	struct block_device *bdev;
	struct smap_swap_map *map_4k;
	struct smap_swap_map *map_2m;
	u64 capacity;			/* device size in bytes */
	atomic64_t total_swap_out;
	atomic64_t total_swap_in;
	atomic64_t current_swapped;
	spinlock_t lock;
	DECLARE_HASHTABLE(mappings, SWAP_MAPPING_BITS);
	spinlock_t map_lock;		/* protects mappings hash table */
};

/*
 * Context for a single bio I/O operation.
 */
struct smap_swap_bio_ctx {
	struct page *page;
	u32 slot_index;
	struct completion done;
	int error;
	struct bio *bio;
};

/*
 * Userspace ioctl messages.
 */
struct swap_msg {
	int cnt;
	pid_t pid;
	u64 __user *addrs;
	u64 __user *results;
};

struct swap_in_msg {
	int cnt;
	pid_t pid;
	u64 __user *slots;
	u64 __user *results;
	int dest_nid;
};

struct swap_dev_config {
	char path[256];
	u64 size;
	bool enable_2m;
};

struct swap_stats {
	u64 total_swap_out;
	u64 total_swap_in;
	u64 current_swapped;
	u64 free_slots_4k;
	u64 free_slots_2m;
};

/* === Swap map allocator (smap_swap_map.c) === */
struct smap_swap_map *smap_swap_map_create(u64 device_size, u32 slot_size);
void smap_swap_map_destroy(struct smap_swap_map *map);
int smap_swap_map_alloc(struct smap_swap_map *map);
int smap_swap_map_alloc_contiguous(struct smap_swap_map *map, u32 count);
void smap_swap_map_free(struct smap_swap_map *map, u32 slot_index);
void smap_swap_map_free_contiguous(struct smap_swap_map *map,
				   u32 slot_index, u32 count);
void smap_swap_map_get_stats(struct smap_swap_map *map,
			     u32 *total, u32 *free);

/* === Bio I/O engine (smap_swap_bio.c) === */
int smap_swap_bio_init(struct smap_swap_device *dev, const char *path);
void smap_swap_bio_exit(struct smap_swap_device *dev);
int smap_swap_write_page(struct smap_swap_device *dev, struct page *page,
			 u32 slot_index,
			 bio_end_io_t callback);
int smap_swap_read_page(struct smap_swap_device *dev, struct page *page,
			u32 slot_index,
			bio_end_io_t callback);
int smap_swap_write_page_sync(struct smap_swap_device *dev,
			      struct page *page, u32 slot_index);
int smap_swap_read_page_sync(struct smap_swap_device *dev,
			     struct page *page, u32 slot_index);
int smap_swap_write_pages_batch(struct smap_swap_device *dev,
				struct page **pages, u32 *slots, u32 count);

/* === Core swap logic (smap_swap.c) === */
int smap_swap_init(struct swap_dev_config *config);
void smap_swap_exit(void);
int do_swap_out(struct swap_msg __user *umsg);
int do_swap_in(struct swap_in_msg __user *umsg);
int smap_swap_set_device(struct swap_dev_config __user *uconfig);
int smap_swap_get_stats(struct swap_stats __user *ustats);
int smap_swap_module_init(void);
void smap_swap_module_exit(void);

#endif /* _SMAP_SWAP_H */
