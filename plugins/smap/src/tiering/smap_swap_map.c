// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: SMAP NVMe Swap - Bitmap slot allocator
 */

#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/bitmap.h>
#include <linux/log2.h>

#include "smap_swap.h"

#undef pr_fmt
#define pr_fmt(fmt) "SMAP_swap_map: " fmt

/**
 * smap_swap_map_create - Create a bitmap-based swap slot allocator
 * @device_size: Total device size in bytes
 * @slot_size:   Size of each slot in bytes (SWAP_SLOT_4K or SWAP_SLOT_2M)
 *
 * Returns a pointer to the new map, or ERR_PTR on failure.
 */
struct smap_swap_map *smap_swap_map_create(u64 device_size, u32 slot_size)
{
	struct smap_swap_map *map;
	u32 total_slots;
	unsigned long bitmap_size;

	if (slot_size == 0 || device_size == 0) {
		pr_err("invalid parameters: device_size=%llu slot_size=%u\n",
		       device_size, slot_size);
		return ERR_PTR(-EINVAL);
	}

	total_slots = (u32)(device_size / slot_size);
	if (total_slots == 0) {
		pr_err("device too small for slot_size=%u\n", slot_size);
		return ERR_PTR(-EINVAL);
	}

	map = kzalloc(sizeof(*map), GFP_KERNEL);
	if (!map)
		return ERR_PTR(-ENOMEM);

	bitmap_size = BITS_TO_LONGS(total_slots) * sizeof(unsigned long);

	/*
	 * Use kvzalloc for potentially large bitmaps (e.g. a 1TB device
	 * with 4K slots = 256M slots = 32MB bitmap).
	 */
	map->bitmap = kvzalloc(bitmap_size, GFP_KERNEL);
	if (!map->bitmap) {
		pr_err("failed to allocate bitmap (%lu bytes) for %u slots\n",
		       bitmap_size, total_slots);
		kfree(map);
		return ERR_PTR(-ENOMEM);
	}

	map->total_slots = total_slots;
	map->slot_size = slot_size;
	atomic_set(&map->free_slots, total_slots);
	spin_lock_init(&map->lock);

	pr_info("created swap map: %u slots of %u bytes (device %llu bytes)\n",
		total_slots, slot_size, device_size);

	return map;
}

/**
 * smap_swap_map_destroy - Free a swap map and its bitmap
 * @map: The map to destroy (may be NULL)
 */
void smap_swap_map_destroy(struct smap_swap_map *map)
{
	if (!map)
		return;

	kvfree(map->bitmap);
	map->bitmap = NULL;
	kfree(map);
}

/**
 * smap_swap_map_alloc - Allocate a single swap slot
 * @map: The swap map
 *
 * Returns slot index on success, or -ENOSPC if no free slots.
 */
int smap_swap_map_alloc(struct smap_swap_map *map)
{
	unsigned long bit;
	unsigned long flags;

	if (!map || !map->bitmap)
		return -EINVAL;

	spin_lock_irqsave(&map->lock, flags);

	bit = find_first_zero_bit(map->bitmap, map->total_slots);
	if (bit >= map->total_slots) {
		spin_unlock_irqrestore(&map->lock, flags);
		return -ENOSPC;
	}

	set_bit(bit, map->bitmap);
	atomic_dec(&map->free_slots);

	spin_unlock_irqrestore(&map->lock, flags);

	return (int)bit;
}

/**
 * smap_swap_map_alloc_contiguous - Allocate a contiguous range of slots
 * @map:   The swap map
 * @count: Number of contiguous slots needed (e.g. 512 for a 2M page
 *         when using 4K slots)
 *
 * Returns the starting slot index, or -ENOSPC.
 */
int smap_swap_map_alloc_contiguous(struct smap_swap_map *map, u32 count)
{
	unsigned long start;
	unsigned long flags;
	u32 i;

	if (!map || !map->bitmap || count == 0)
		return -EINVAL;

	if (count > map->total_slots)
		return -ENOSPC;

	spin_lock_irqsave(&map->lock, flags);

	start = bitmap_find_next_zero_area(map->bitmap, map->total_slots,
					   0, count, 0);
	if (start >= map->total_slots) {
		spin_unlock_irqrestore(&map->lock, flags);
		return -ENOSPC;
	}

	bitmap_set(map->bitmap, start, count);
	atomic_sub(count, &map->free_slots);

	spin_unlock_irqrestore(&map->lock, flags);

	return (int)start;
}

/**
 * smap_swap_map_free - Free a single swap slot
 * @map:        The swap map
 * @slot_index: Index of the slot to free
 */
void smap_swap_map_free(struct smap_swap_map *map, u32 slot_index)
{
	unsigned long flags;

	if (!map || !map->bitmap)
		return;

	if (slot_index >= map->total_slots) {
		pr_warn("free: slot_index %u out of range (max %u)\n",
			slot_index, map->total_slots);
		return;
	}

	spin_lock_irqsave(&map->lock, flags);

	if (!test_bit(slot_index, map->bitmap)) {
		spin_unlock_irqrestore(&map->lock, flags);
		pr_warn("free: slot %u already free\n", slot_index);
		return;
	}

	clear_bit(slot_index, map->bitmap);
	atomic_inc(&map->free_slots);

	spin_unlock_irqrestore(&map->lock, flags);
}

/**
 * smap_swap_map_free_contiguous - Free a contiguous range of swap slots
 * @map:        The swap map
 * @slot_index: Starting slot index
 * @count:      Number of contiguous slots to free
 */
void smap_swap_map_free_contiguous(struct smap_swap_map *map,
				   u32 slot_index, u32 count)
{
	unsigned long flags;
	u32 i;

	if (!map || !map->bitmap || count == 0)
		return;

	if (slot_index + count > map->total_slots) {
		pr_warn("free_contiguous: range [%u, %u) out of bounds (max %u)\n",
			slot_index, slot_index + count, map->total_slots);
		return;
	}

	spin_lock_irqsave(&map->lock, flags);

	for (i = 0; i < count; i++) {
		if (!test_bit(slot_index + i, map->bitmap)) {
			pr_warn("free_contiguous: slot %u already free\n",
				slot_index + i);
			continue;
		}
		clear_bit(slot_index + i, map->bitmap);
		atomic_inc(&map->free_slots);
	}

	spin_unlock_irqrestore(&map->lock, flags);
}

/**
 * smap_swap_map_get_stats - Read allocator statistics
 * @map:   The swap map
 * @total: Output: total number of slots
 * @free:  Output: number of free slots
 */
void smap_swap_map_get_stats(struct smap_swap_map *map, u32 *total, u32 *free)
{
	if (!map) {
		if (total)
			*total = 0;
		if (free)
			*free = 0;
		return;
	}

	if (total)
		*total = map->total_slots;
	if (free)
		*free = (u32)atomic_read(&map->free_slots);
}
