/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_TYPES_H
#define _LINUX_TYPES_H

#define __EXPORTED_HEADERS__
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <uapi/linux/types.h>
#include <uapi/asm-generic/posix_types.h>
#include <asm-generic/ioctl.h>


#ifndef __ASSEMBLY__

#define DECLARE_BITMAP(name, bits) \
	unsigned long name[BITS_TO_LONGS(bits)]

typedef u32 __kernel_dev_t;

typedef __kernel_gid16_t        gid16_t;
typedef __kernel_uid16_t        uid16_t;
typedef __kernel_gid32_t	gid_t;
typedef __kernel_uid32_t	uid_t;

typedef unsigned short		umode_t;
typedef __kernel_mode_t		mode_t;
typedef __kernel_ulong_t	ino_t;
typedef __kernel_mqd_t		mqd_t;
typedef __kernel_clockid_t	clockid_t;
typedef __kernel_suseconds_t	suseconds_t;
typedef __kernel_key_t		key_t;
typedef __kernel_daddr_t	daddr_t;
typedef __kernel_pid_t		pid_t;
typedef __kernel_off_t		off_t;
typedef u32			nlink_t;

typedef unsigned int  gfp_t;
typedef unsigned long		uintptr_t;
#ifdef CONFIG_HAVE_UID16
typedef __kernel_old_gid_t	old_gid_t;
typedef __kernel_old_uid_t	old_uid_t;
#endif

#if defined(__GNUC__)
#endif

#ifndef _CADDR_T
#define _CADDR_T
typedef __kernel_caddr_t	caddr_t;
#endif

#ifndef _CLOCK_T
#define _CLOCK_T
typedef __kernel_clock_t	clock_t;
#endif

#ifndef _PTRDIFF_T
#define _PTRDIFF_T
typedef __kernel_ptrdiff_t	ptrdiff_t;
#endif

#ifndef _SSIZE_T
#define _SSIZE_T
// ssize_t is used for sizes that can be negative, such as return values of system calls
typedef __kernel_ssize_t	ssize_t;
#endif

#ifndef _SIZE_T
#define _SIZE_T
// size_t is used for sizes of objects, typically the size of memory blocks
typedef __kernel_size_t		size_t;
#endif

typedef unsigned long		ulong;
typedef unsigned int		uint;
typedef unsigned short		ushort;
typedef unsigned char		unchar;

typedef unsigned long		u_long;
typedef unsigned int		u_int;
typedef unsigned short		u_short;
typedef unsigned char		u_char;

#ifndef __BIT_TYPES_DEFINED__
#define __BIT_TYPES_DEFINED__

typedef unsigned int uint;

typedef s32			int32_t;
typedef u32			u_int32_t;
typedef s16			int16_t;
typedef u16			u_int16_t;
typedef s8			int8_t;
typedef u8			u_int8_t;

#endif

typedef u64			uint64_t;
typedef u32			uint32_t;
typedef u16			uint16_t;
typedef u8			uint8_t;

#if defined(__GNUC__)
#endif

typedef u64 sector_t;

#define aligned_le64	__aligned_le64
#define aligned_be64	__aligned_be64
#define aligned_u64		__aligned_u64

#define pgoff_t unsigned long

typedef unsigned int __bitwise fmode_t;
typedef unsigned int __bitwise slab_flags_t;
typedef unsigned int __bitwise gfp_t;

#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
typedef u64 dma_addr_t;
#else
typedef u32 dma_addr_t;
#endif

typedef unsigned long irq_hw_number_t;

#ifdef CONFIG_PHYS_ADDR_T_64BIT
typedef u64 phys_addr_t;
#else
typedef u32 phys_addr_t;
#endif

struct list_head {
	struct list_head *next, *prev;
};

#ifdef CONFIG_64BIT
typedef struct {
	s64 counter;
} atomic64_t;
#endif

typedef struct {
	int counter;
} atomic_t;

#define ATOMIC_INIT(i) { (i) }

struct ustat {
	__kernel_daddr_t	f_tfree;
#ifdef CONFIG_ARCH_32BIT_USTAT_F_TINODE
	unsigned int		f_tinode;
#else
	unsigned long		f_tinode;
#endif
    char			f_fpack[6];
	char			f_fname[6];
};

struct hlist_head {
	struct hlist_node *first;
};

struct callback_head {
	struct callback_head *next;
	void (*func)(struct callback_head *head);
} __attribute__((aligned(sizeof(void *))));
#define rcu_head callback_head

typedef int (*cmp_func_t)(const void *a, const void *b);
typedef int (*cmp_r_func_t)(const void *a, const void *b, const void *priv);

struct hlist_node {
	struct hlist_node *next, **pprev;
};

typedef void (*swap_func_t)(void *a, void *b, int size);
typedef void (*swap_r_func_t)(void *a, void *b, int size, const void *priv);

typedef void (*rcu_callback_t)(struct rcu_head *head);
typedef void (*call_rcu_func_t)(struct rcu_head *head, rcu_callback_t func);

typedef phys_addr_t resource_size_t;

#endif
#endif
