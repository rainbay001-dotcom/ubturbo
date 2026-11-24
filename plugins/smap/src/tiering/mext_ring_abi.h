/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * Description: Shared Kernel-Userspace ABI for SMAP MEXT Predictive Memory
 *              (lockless fault/eviction mmap ring + prefault & control ioctls)
 */

#ifndef _SMAP_MEXT_RING_ABI_H
#define _SMAP_MEXT_RING_ABI_H

#ifdef __KERNEL__
#include <linux/types.h>
typedef __u32 mext_u32;
typedef __s32 mext_s32;
typedef __u64 mext_u64;
#else
#include <stdint.h>
typedef uint32_t mext_u32;
typedef int32_t mext_s32;
typedef uint64_t mext_u64;
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define MEXT_RING_MAGIC          0x4D455854U /* "MEXT" */
#define MEXT_RING_VERSION        1U
#define MEXT_RING_CAPACITY_SHIFT 14U
#define MEXT_RING_CAPACITY       (1U << MEXT_RING_CAPACITY_SHIFT) /* 16384 slots */
#define MEXT_RING_MASK           (MEXT_RING_CAPACITY - 1U)

#define MEXT_MAX_PREFAULT_PAGES          512U
#define MEXT_MAX_PAGEOUT_PAGES           4096U
#define MEXT_PREFAULT_FLAG_WRITE         (1U << 0)
#define MEXT_PREFAULT_FLAG_NOWAIT        (1U << 1) /* non-blocking SwapCache->PTE only */
#define MEXT_PREFAULT_FLAG_PAGEOUT_COLD  (1U << 2) /* ap_data-free cold anon pageout */
#define MEXT_PREFAULT_FLAG_FORCE_AGE     (1U << 3) /* emergency fallback if all PTEs young */

enum mext_event_type {
	MEXT_EVT_NONE         = 0,
	MEXT_EVT_SWAP_FAULT   = 1, /* do_swap_page major/minor swap fault */
	MEXT_EVT_EVICTED      = 2, /* do_swap_out cold eviction to NVMe/zram */
	MEXT_EVT_MEM_PRESSURE = 3, /* wakeup_kswapd early zone pressure signal */
};

/*
 * 32-byte cache-aligned telemetry record (2 records per 64B cache line).
 * Producer writes payload fields first, then publishes `type` with
 * store-release. Single consumer reads `type` with load-acquire, copies
 * payload, clears `type` to MEXT_EVT_NONE, and advances `tail`.
 *
 * Payload encoding by `type`:
 *   - MEXT_EVT_SWAP_FAULT:   pid = tgid, vpn = fault VPN, aux = vma_slot (0..31)
 *   - MEXT_EVT_EVICTED:      pid = tgid, vpn = evicted VPN, aux = swapout_epoch
 *   - MEXT_EVT_MEM_PRESSURE: pid = nid,  vpn = zone_free_pages,
 *                            aux = (wmark_low_pages << 16) | (order & 0xffff)
 */
struct mext_ring_event {
	mext_u32 type;      /* enum mext_event_type (release/acquire state) */
	mext_u32 pid;       /* target process TGID (or NUMA nid on MEM_PRESSURE) */
	mext_u64 vpn;       /* virtual page number (or free_pages on MEM_PRESSURE) */
	mext_u64 aux;       /* vma_slot / swapout_epoch / wmark_low+order */
	mext_u64 timestamp; /* ktime_get_ns() */
};

/*
 * 64-byte cache-line header at offset 0 of the mmap'd region.
 */
struct mext_ring_header {
	mext_u32 magic;
	mext_u32 version;
	mext_u32 capacity;
	mext_u32 page_shift;
	mext_u64 write_idx;       /* multi-producer reservation cursor */
	mext_u64 tail;            /* single-consumer read cursor */
	mext_u64 dropped;         /* ring overflow / busy slot counter */
	mext_u64 swapout_epoch;   /* monotonic swap-out batch epoch */
	mext_u64 pressure_events; /* early wakeup_kswapd signals emitted */
	mext_u64 reserved[1];
};

struct mext_ring_buffer {
	struct mext_ring_header hdr;
	struct mext_ring_event events[MEXT_RING_CAPACITY];
};

struct mext_prefault_msg {
	mext_s32 pid;
	mext_u32 nr_pages;     /* [IN] number of VPNs in vpns_ptr */
	mext_u32 nr_succeeded; /* [OUT] pages populated into PTEs */
	mext_u32 flags;        /* MEXT_PREFAULT_FLAG_* */
	mext_u64 vpns_ptr;     /* user pointer to uint64_t[nr_pages] */
};

enum mext_ctrl_op {
	MEXT_CTRL_ENABLE     = 1,
	MEXT_CTRL_DISABLE    = 2,
	MEXT_CTRL_ADD_PID    = 3,
	MEXT_CTRL_REMOVE_PID = 4,
	MEXT_CTRL_CLEAR_PIDS = 5,
	MEXT_CTRL_GET_STATS  = 6,
};

struct mext_ctrl_msg {
	mext_u32 op;
	mext_s32 pid;
	mext_u64 faults_captured;
	mext_u64 evicts_captured;
	mext_u64 ring_dropped;
	mext_u64 prefault_succeeded;
};

#ifdef __cplusplus
}
#endif

#endif /* _SMAP_MEXT_RING_ABI_H */
