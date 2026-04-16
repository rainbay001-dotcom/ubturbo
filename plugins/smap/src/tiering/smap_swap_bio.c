// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: SMAP NVMe Swap - Block I/O engine
 */

#include <linux/slab.h>
#include <linux/blkdev.h>
#include <linux/bio.h>
#include <linux/completion.h>
#include <linux/fs.h>

#include "smap_swap.h"

#undef pr_fmt
#define pr_fmt(fmt) "SMAP_swap_bio: " fmt

/*
 * Convert a slot index + slot size into a sector number.
 * sector = slot_index * slot_size / 512
 */
static inline sector_t slot_to_sector(u32 slot_index, u32 slot_size)
{
	return (sector_t)slot_index * (slot_size >> SECTOR_SHIFT);
}

/**
 * smap_swap_bio_end_io - Completion callback for async bio
 * @bio: The completed bio
 *
 * Signals the completion and records any error in the context.
 */
static void smap_swap_bio_end_io(struct bio *bio)
{
	struct smap_swap_bio_ctx *ctx = bio->bi_private;

	if (bio->bi_status) {
		ctx->error = blk_status_to_errno(bio->bi_status);
		pr_err("bio error: slot=%u status=%d\n",
		       ctx->slot_index, ctx->error);
	}

	complete(&ctx->done);
}

/**
 * smap_swap_bio_init - Open a block device for swap I/O
 * @dev:  The swap device structure to initialize
 * @path: Path to the block device (e.g. "/dev/nvme0n1p3")
 *
 * Returns 0 on success, negative errno on failure.
 */
int smap_swap_bio_init(struct smap_swap_device *dev, const char *path)
{
	struct block_device *bdev;

	if (!dev || !path || path[0] == '\0') {
		pr_err("invalid parameters for bio_init\n");
		return -EINVAL;
	}

	/*
	 * Open the block device.  We use FMODE_READ | FMODE_WRITE | FMODE_EXCL
	 * to get exclusive access.  On newer kernels (6.5+) the API is
	 * bdev_open_by_path(); on older ones it is blkdev_get_by_path().
	 * We use blkdev_get_by_path() here for broader compatibility.
	 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 5, 0)
	{
		struct bdev_handle *handle;

		handle = bdev_open_by_path(path,
			BLK_OPEN_READ | BLK_OPEN_WRITE | BLK_OPEN_EXCL,
			dev, NULL);
		if (IS_ERR(handle)) {
			pr_err("failed to open block device %s: %ld\n",
			       path, PTR_ERR(handle));
			return PTR_ERR(handle);
		}
		bdev = handle->bdev;
	}
#else
	bdev = blkdev_get_by_path(path, FMODE_READ | FMODE_WRITE | FMODE_EXCL,
				  dev);
	if (IS_ERR(bdev)) {
		pr_err("failed to open block device %s: %ld\n",
		       path, PTR_ERR(bdev));
		return PTR_ERR(bdev);
	}
#endif

	dev->bdev = bdev;
	dev->capacity = i_size_read(bdev->bd_inode);

	pr_info("opened block device %s, capacity=%llu bytes\n",
		path, dev->capacity);

	return 0;
}

/**
 * smap_swap_bio_exit - Release the block device
 * @dev: The swap device
 */
void smap_swap_bio_exit(struct smap_swap_device *dev)
{
	if (!dev || !dev->bdev)
		return;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 5, 0)
	bdev_release(dev->bdev);
#else
	blkdev_put(dev->bdev, FMODE_READ | FMODE_WRITE | FMODE_EXCL);
#endif

	dev->bdev = NULL;
	dev->capacity = 0;

	pr_info("released block device\n");
}

/**
 * smap_swap_write_page - Submit an async write bio for one page
 * @dev:        The swap device
 * @page:       The page to write
 * @slot_index: Target slot in the swap map
 * @callback:   Completion callback (may be NULL for default handler)
 *
 * Returns 0 on success (bio submitted), negative errno on failure.
 */
int smap_swap_write_page(struct smap_swap_device *dev, struct page *page,
			 u32 slot_index, bio_end_io_t callback)
{
	struct bio *bio;
	sector_t sector;

	if (!dev || !dev->bdev || !page)
		return -EINVAL;

	sector = slot_to_sector(slot_index, SWAP_SLOT_4K);

	bio = bio_alloc(dev->bdev, 1, REQ_OP_WRITE, GFP_NOIO);
	if (!bio) {
		pr_err("failed to allocate write bio for slot %u\n",
		       slot_index);
		return -ENOMEM;
	}

	bio->bi_iter.bi_sector = sector;

	if (bio_add_page(bio, page, PAGE_SIZE, 0) != PAGE_SIZE) {
		pr_err("failed to add page to write bio for slot %u\n",
		       slot_index);
		bio_put(bio);
		return -EIO;
	}

	if (callback)
		bio->bi_end_io = callback;
	else
		bio->bi_end_io = smap_swap_bio_end_io;

	submit_bio(bio);

	return 0;
}

/**
 * smap_swap_read_page - Submit an async read bio for one page
 * @dev:        The swap device
 * @page:       The page to read into
 * @slot_index: Source slot in the swap map
 * @callback:   Completion callback (may be NULL for default handler)
 *
 * Returns 0 on success (bio submitted), negative errno on failure.
 */
int smap_swap_read_page(struct smap_swap_device *dev, struct page *page,
			u32 slot_index, bio_end_io_t callback)
{
	struct bio *bio;
	sector_t sector;

	if (!dev || !dev->bdev || !page)
		return -EINVAL;

	sector = slot_to_sector(slot_index, SWAP_SLOT_4K);

	bio = bio_alloc(dev->bdev, 1, REQ_OP_READ, GFP_NOIO);
	if (!bio) {
		pr_err("failed to allocate read bio for slot %u\n",
		       slot_index);
		return -ENOMEM;
	}

	bio->bi_iter.bi_sector = sector;

	if (bio_add_page(bio, page, PAGE_SIZE, 0) != PAGE_SIZE) {
		pr_err("failed to add page to read bio for slot %u\n",
		       slot_index);
		bio_put(bio);
		return -EIO;
	}

	if (callback)
		bio->bi_end_io = callback;
	else
		bio->bi_end_io = smap_swap_bio_end_io;

	submit_bio(bio);

	return 0;
}

/**
 * smap_swap_write_page_sync - Synchronous page write to NVMe
 * @dev:        The swap device
 * @page:       The page to write
 * @slot_index: Target slot index
 *
 * Returns 0 on success, negative errno on failure.
 */
int smap_swap_write_page_sync(struct smap_swap_device *dev,
			      struct page *page, u32 slot_index)
{
	struct smap_swap_bio_ctx ctx;
	struct bio *bio;
	sector_t sector;

	if (!dev || !dev->bdev || !page)
		return -EINVAL;

	sector = slot_to_sector(slot_index, SWAP_SLOT_4K);

	bio = bio_alloc(dev->bdev, 1, REQ_OP_WRITE, GFP_NOIO);
	if (!bio)
		return -ENOMEM;

	bio->bi_iter.bi_sector = sector;

	if (bio_add_page(bio, page, PAGE_SIZE, 0) != PAGE_SIZE) {
		bio_put(bio);
		return -EIO;
	}

	ctx.page = page;
	ctx.slot_index = slot_index;
	ctx.error = 0;
	ctx.bio = bio;
	init_completion(&ctx.done);

	bio->bi_private = &ctx;
	bio->bi_end_io = smap_swap_bio_end_io;

	submit_bio(bio);
	wait_for_completion(&ctx.done);

	bio_put(bio);
	return ctx.error;
}

/**
 * smap_swap_read_page_sync - Synchronous page read from NVMe
 * @dev:        The swap device
 * @page:       The page to read into
 * @slot_index: Source slot index
 *
 * Returns 0 on success, negative errno on failure.
 */
int smap_swap_read_page_sync(struct smap_swap_device *dev,
			     struct page *page, u32 slot_index)
{
	struct smap_swap_bio_ctx ctx;
	struct bio *bio;
	sector_t sector;

	if (!dev || !dev->bdev || !page)
		return -EINVAL;

	sector = slot_to_sector(slot_index, SWAP_SLOT_4K);

	bio = bio_alloc(dev->bdev, 1, REQ_OP_READ, GFP_NOIO);
	if (!bio)
		return -ENOMEM;

	bio->bi_iter.bi_sector = sector;

	if (bio_add_page(bio, page, PAGE_SIZE, 0) != PAGE_SIZE) {
		bio_put(bio);
		return -EIO;
	}

	ctx.page = page;
	ctx.slot_index = slot_index;
	ctx.error = 0;
	ctx.bio = bio;
	init_completion(&ctx.done);

	bio->bi_private = &ctx;
	bio->bi_end_io = smap_swap_bio_end_io;

	submit_bio(bio);
	wait_for_completion(&ctx.done);

	bio_put(bio);
	return ctx.error;
}

/**
 * smap_swap_write_pages_batch - Batch-write multiple pages to NVMe
 * @dev:   The swap device
 * @pages: Array of pages to write
 * @slots: Array of target slot indices
 * @count: Number of pages to write
 *
 * Submits all bios, then waits for all to complete.
 * Returns number of pages successfully written.
 */
int smap_swap_write_pages_batch(struct smap_swap_device *dev,
				struct page **pages, u32 *slots, u32 count)
{
	struct smap_swap_bio_ctx *ctxs;
	u32 i;
	int submitted = 0;
	int completed = 0;

	if (!dev || !dev->bdev || !pages || !slots || count == 0)
		return 0;

	ctxs = kzalloc(count * sizeof(*ctxs), GFP_KERNEL);
	if (!ctxs) {
		pr_err("failed to allocate batch context for %u pages\n",
		       count);
		return 0;
	}

	/* Submit all bios */
	for (i = 0; i < count; i++) {
		struct bio *bio;
		sector_t sector;

		if (!pages[i])
			continue;

		sector = slot_to_sector(slots[i], SWAP_SLOT_4K);

		bio = bio_alloc(dev->bdev, 1, REQ_OP_WRITE, GFP_NOIO);
		if (!bio) {
			pr_err("batch: failed to allocate bio for slot %u\n",
			       slots[i]);
			continue;
		}

		bio->bi_iter.bi_sector = sector;

		if (bio_add_page(bio, pages[i], PAGE_SIZE, 0) != PAGE_SIZE) {
			bio_put(bio);
			pr_err("batch: failed to add page for slot %u\n",
			       slots[i]);
			continue;
		}

		ctxs[i].page = pages[i];
		ctxs[i].slot_index = slots[i];
		ctxs[i].error = 0;
		ctxs[i].bio = bio;
		init_completion(&ctxs[i].done);

		bio->bi_private = &ctxs[i];
		bio->bi_end_io = smap_swap_bio_end_io;

		submit_bio(bio);
		submitted++;
	}

	/* Wait for all submitted bios */
	for (i = 0; i < count; i++) {
		if (!ctxs[i].bio)
			continue;

		wait_for_completion(&ctxs[i].done);

		if (ctxs[i].error == 0)
			completed++;

		bio_put(ctxs[i].bio);
	}

	kfree(ctxs);

	pr_debug("batch write: submitted=%d completed=%d/%u\n",
		 submitted, completed, count);

	return completed;
}
