/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef LINUX_MOCK_H
#define LINUX_MOCK_H

#include <linux/types.h>
#ifdef __cplusplus
extern "C" {
#endif

/* from linux/kallsyms.h. */
#define KSYM_NAME_LEN 512

/* from nolibc/types.h. */
#define PATH_MAX 4096

/* from librttest.h. */
#define ATOMIC_INIT(i)  { (i) }

/* from uapi/asm-generic/int-ll64.h. */
#ifdef __GNUC__
__extension__ typedef __signed__ long long __s64;
__extension__ typedef unsigned long long __u64;
#else
typedef __signed__ long long __s64;
typedef unsigned long long __u64;
#endif

// from uapi/asm-generic/errno-base.h
#define	ERANGE		34
#define	EDOM		33
#define	EPIPE		32
#define	EMLINK		31
#define	EROFS		30
#define	ESPIPE		29
#define	ENOSPC		28
#define	EFBIG		27
#define	ETXTBSY		26
#define	ENOTTY		25
#define	EMFILE		24
#define	ENFILE		23
#define	EINVAL		22
#define	EISDIR		21
#define	ENOTDIR		20
#define	ENODEV		19
#define	EXDEV		18
#define	EEXIST		17
#define	EBUSY		16
#define	ENOTBLK		15
#define	EFAULT		14
#define	EACCES		13
#define	ENOMEM		12
#define	EAGAIN		11
#define	ECHILD		10
#define	EBADF		 9
#define	ENOEXEC		 8
#define	E2BIG		 7
#define	ENXIO		 6
#define	EIO		     5
#define	EINTR		 4
#define	ESRCH		 3
#define	ENOENT		 2
#define	EPERM		 1

// from asm-generic/rwonce.h
#define __WRITE_ONCE(x, val) \
do { \
    *(volatile typeof(x) *)&(x) = static_cast<volatile typeof(x)>(val); \
} while (0)

#define WRITE_ONCE(x, val) \
do { \
    __WRITE_ONCE(x, val); \
} while (0)

// from linux/poison.h
# define POISON_POINTER_DELTA 0
#define LIST_POISON1  (reinterpret_cast<void *>(0x100 + POISON_POINTER_DELTA))
#define LIST_POISON2  (reinterpret_cast<void *>(0x122 + POISON_POINTER_DELTA))

/* from linux/compiler.h. */
#define __rcu
#define __user

/* from linux/compiler_attributes.h. */
#define __always_inline                 inline __attribute__((__always_inline__))
#define __attribute_const__             __attribute__((__const__))
#if __has_attribute(__designated_init__)
# define __designated_init              __attribute__((__designated_init__))
#else
# define __designated_init
#endif

// from linux/compiler_types.h
# define __force
#if defined(RANDSTRUCT) && !defined(__CHECKER__)
# define __randomize_layout __designated_init __attribute__((randomize_layout))
# define __no_randomize_layout __attribute__((no_randomize_layout))
/* This anon struct can add padding, so only enable it under randstruct. */
# define randomized_struct_fields_start struct {
# define randomized_struct_fields_end } __randomize_layout;
#else
# define __randomize_layout __designated_init
# define __no_randomize_layout
# define randomized_struct_fields_start
# define randomized_struct_fields_end
#endif

// from linux/stddef.h
#ifndef __cplusplus
#define NULL ((void *)0)
#else
#define NULL nullptr
#endif

// from linux/gfp_types.h
#define ___GFP_DMA 0x01u
#define ___GFP_HIGHMEM 0x02u
#define ___GFP_DMA32 0x04u
#define ___GFP_MOVABLE 0x08u
#define ___GFP_RECLAIMABLE 0x10u
#define ___GFP_HIGH 0x20u
#define ___GFP_IO 0x40u
#define ___GFP_FS 0x80u
#define ___GFP_ZERO 0x100u
#define ___GFP_RELIABLE 0
#define ___GFP_DIRECT_RECLAIM 0x400u
#define ___GFP_KSWAPD_RECLAIM 0x800u
#define ___GFP_WRITE 0x1000u
#define ___GFP_NOWARN 0x2000u
#define ___GFP_RETRY_MAYFAIL 0x4000u
#define ___GFP_NOFAIL 0x8000u
#define ___GFP_NORETRY 0x10000u
#define ___GFP_MEMALLOC 0x20000u
#define ___GFP_COMP 0x40000u
#define ___GFP_NOMEMALLOC 0x80000u
#define ___GFP_HARDWALL 0x100000u
#define ___GFP_THISNODE 0x200000u
#define ___GFP_ACCOUNT 0x400000u
#define ___GFP_ZEROTAGS 0x800000u
#define ___GFP_SKIP_ZERO 0
#define ___GFP_SKIP_KASAN 0
#define ___GFP_NOLOCKDEP 0

#define __GFP_DMA ((__force unsigned int)___GFP_DMA)
#define __GFP_HIGHMEM ((__force unsigned int)___GFP_HIGHMEM)
#define __GFP_DMA32 ((__force unsigned int)___GFP_DMA32)
#define __GFP_MOVABLE ((__force unsigned int)___GFP_MOVABLE)
#define GFP_ZONEMASK (__GFP_DMA|__GFP_HIGHMEM|__GFP_DMA32|__GFP_MOVABLE)

#define __GFP_RECLAIMABLE ((__force unsigned int)___GFP_RECLAIMABLE)
#define __GFP_WRITE ((__force unsigned int)___GFP_WRITE)
#define __GFP_HARDWALL ((__force unsigned int)___GFP_HARDWALL)
#define __GFP_THISNODE ((__force unsigned int)___GFP_THISNODE)
#define __GFP_ACCOUNT ((__force unsigned int)___GFP_ACCOUNT)

#define __GFP_HIGH ((__force unsigned int)___GFP_HIGH)
#define __GFP_MEMALLOC ((__force unsigned int)___GFP_MEMALLOC)
#define __GFP_NOMEMALLOC ((__force unsigned int)___GFP_NOMEMALLOC)

#define __GFP_IO ((__force unsigned int)___GFP_IO)
#define __GFP_FS ((__force unsigned int)___GFP_FS)
#define __GFP_DIRECT_RECLAIM ((__force unsigned int)___GFP_DIRECT_RECLAIM)
#define __GFP_KSWAPD_RECLAIM ((__force unsigned int)___GFP_KSWAPD_RECLAIM)
#define __GFP_RECLAIM ((__force unsigned int)(___GFP_DIRECT_RECLAIM|___GFP_KSWAPD_RECLAIM))
#define __GFP_RETRY_MAYFAIL ((__force unsigned int)___GFP_RETRY_MAYFAIL)
#define __GFP_NOFAIL ((__force unsigned int)___GFP_NOFAIL)
#define __GFP_NORETRY ((__force unsigned int)___GFP_NORETRY)

#define __GFP_NOWARN ((__force unsigned int)___GFP_NOWARN)
#define __GFP_COMP ((__force unsigned int)___GFP_COMP)
#define __GFP_ZERO ((__force unsigned int)___GFP_ZERO)
#define __GFP_ZEROTAGS ((__force unsigned int)___GFP_ZEROTAGS)
#define __GFP_SKIP_ZERO ((__force unsigned int)___GFP_SKIP_ZERO)
#define __GFP_SKIP_KASAN ((__force unsigned int)___GFP_SKIP_KASAN)

/* Disable lockdep for GFP context tracking */
#define __GFP_NOLOCKDEP ((__force unsigned int)___GFP_NOLOCKDEP)

/* Alloc memory from mirrored region */
#define __GFP_RELIABLE ((__force unsigned int)___GFP_RELIABLE)

/* Room for N __GFP_FOO bits */
#define __GFP_BITS_SHIFT (26 + IS_ENABLED(CONFIG_LOCKDEP))
#define __GFP_BITS_MASK ((__force unsigned int)((1 << __GFP_BITS_SHIFT) - 1))

#define GFP_ATOMIC (__GFP_HIGH|__GFP_KSWAPD_RECLAIM)
#define GFP_KERNEL (__GFP_RECLAIM | __GFP_IO | __GFP_FS)
#define GFP_KERNEL_ACCOUNT (GFP_KERNEL | __GFP_ACCOUNT)
#define GFP_NOWAIT (__GFP_KSWAPD_RECLAIM)
#define GFP_NOIO (__GFP_RECLAIM)
#define GFP_NOFS (__GFP_RECLAIM | __GFP_IO)
#define GFP_USER (__GFP_RECLAIM | __GFP_IO | __GFP_FS | __GFP_HARDWALL)
#define GFP_DMA __GFP_DMA
#define GFP_DMA32 __GFP_DMA32
#define GFP_HIGHUSER (GFP_USER | __GFP_HIGHMEM)
#define GFP_HIGHUSER_MOVABLE (GFP_HIGHUSER | __GFP_MOVABLE | __GFP_SKIP_KASAN)
#define GFP_TRANSHUGE_LIGHT ((GFP_HIGHUSER_MOVABLE | __GFP_COMP | \
        __GFP_NOMEMALLOC | __GFP_NOWARN) & ~__GFP_RECLAIM)
#define GFP_TRANSHUGE (GFP_TRANSHUGE_LIGHT | __GFP_DIRECT_RECLAIM)
#define GFP_RELIABLE __GFP_RELIABLE

// from linux/numa.h
#define NODES_SHIFT     10
#define MAX_NUMNODES    (1 << NODES_SHIFT)

// from linux/bits.h
#define BIT_MASK(nr) (UL(1) << ((nr) % BITS_PER_LONG))
#define BIT_WORD(nr) ((nr) / BITS_PER_LONG)
#define BIT_ULL_MASK(nr) (ULL(1) << ((nr) % BITS_PER_LONG_LONG))
#define BIT_ULL_WORD(nr) ((nr) / BITS_PER_LONG_LONG)
#define BITS_PER_BYTE 8

// from uapi/linux/const.h
#define __KERNEL_DIV_ROUND_UP(n, d) (((n) + (d)-1) / (d))

// from linux/bitops.h
#define BITS_PER_TYPE(type) (sizeof(type) * BITS_PER_BYTE)
#define BITS_TO_LONGS(nr) __KERNEL_DIV_ROUND_UP(nr, BITS_PER_TYPE(long))
#define BITS_TO_U64(nr) __KERNEL_DIV_ROUND_UP(nr, BITS_PER_TYPE(u64))
#define BITS_TO_U32(nr) __KERNEL_DIV_ROUND_UP(nr, BITS_PER_TYPE(u32))
#define BITS_TO_BYTES(nr) __KERNEL_DIV_ROUND_UP(nr, BITS_PER_TYPE(char))
#ifndef __WORDSIZE
#define __WORDSIZE (__SIZEOF_LONG__ * 8)
#endif

// from linux/nodemask.h
typedef struct {
    DECLARE_BITMAP(bits, MAX_NUMNODES);
} nodemask_t;

// from linux/build_bug.h
#define BUILD_BUG_ON_INVALID(e) ((void)(sizeof((__force long)(e))))

// from linux/mmdebug.h
#define VM_BUG_ON(cond) BUILD_BUG_ON_INVALID(cond)
#define VM_BUG_ON_FOLIO(cond, folio) VM_BUG_ON(cond)

// from linux/stddef.h
#define offsetof(TYPE, MEMBER)	__builtin_offsetof(TYPE, MEMBER)

// from linux/container_of.h
#define container_of(ptr, type, member) ({				\
    void *__mptr = (void *)(ptr);					\
    ((type *)(__mptr - offsetof(type, member))); })

// from linux/list.h
#define LIST_HEAD_INIT(name) \
    {                        \
        &(name), &(name)     \
    }
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)

#define list_entry(ptr, type, member) container_of(ptr, type, member)

#define list_first_entry(ptr, type, member) list_entry((ptr)->next, type, member)

#define list_entry_is_head(pos, head, member) (&((pos)->member) == (head))

#define list_next_entry(pos, member) list_entry((pos)->member.next, typeof(*(pos)), member)

#define list_for_each_entry(pos, head, member) \
    for ((pos) = list_first_entry(head, typeof(*(pos)), member); !list_entry_is_head((pos), head, member); \
        (pos) = list_next_entry((pos), member))

#define hlist_entry(ptr, type, member) container_of(ptr, type, member)
#define hlist_entry_safe(ptr, type, member) \
    ({ typeof(ptr) ____ptr = (ptr); \
       ____ptr ? hlist_entry(____ptr, type, member) : NULL; \
    })
#define hlist_for_each_entry(pos, head, member)				\
    for ((pos) = hlist_entry_safe((head)->first, typeof(*(pos)), member); \
        (pos);							\
        (pos) = hlist_entry_safe((pos)->member.next, typeof(*(pos)), member))

#define hlist_for_each_entry(pos, head, member)				\
    for ((pos) = hlist_entry_safe((head)->first, typeof(*(pos)), member); \
        (pos);							\
        (pos) = hlist_entry_safe((pos)->member.next, typeof(*(pos)), member))

static inline void __list_add(struct list_head *c_new, struct list_head *prev, struct list_head *c_next)
{
    c_next->prev = c_new;
    c_new->next = c_next;
    c_new->prev = prev;
    prev->next = c_new;
}
static inline void list_add(struct list_head *c_new, struct list_head *head)
{
    __list_add(c_new, head, head->next);
}

static inline int list_empty(const struct list_head *head)
{
    return head->next == head;
}

int hlist_empty(const struct hlist_head *h);

static inline void hlist_add_head(struct hlist_node *n, struct hlist_head *h)
{
    struct hlist_node *first = h->first;
    WRITE_ONCE(n->next, first);
    if (first) {
        WRITE_ONCE(first->pprev, &n->next);
    }
    WRITE_ONCE(h->first, n);
    WRITE_ONCE(n->pprev, &h->first);
}
static inline void __hlist_del(struct hlist_node *n)
{
    struct hlist_node *next = n->next;
    struct hlist_node **pprev = n->pprev;
    WRITE_ONCE(*pprev, next);
    if (next) {
        WRITE_ONCE(next->pprev, pprev);
    }
}
static inline void hlist_del(struct hlist_node *n)
{
    __hlist_del(n);
    n->next = reinterpret_cast<struct hlist_node **>(LIST_POISON1);
    n->pprev = reinterpret_cast<struct hlist_node **>(LIST_POISON2);
}

// from linux/vmstat.h
#define count_vm_numa_events(x, y) \
    do {                           \
        (void)(y);                 \
    } while (0)

// from linux/migrate_mode.h
enum migrate_mode {
    MIGRATE_ASYNC,
    MIGRATE_SYNC_LIGHT,
    MIGRATE_SYNC,
    MIGRATE_SYNC_NO_COPY,
};
enum migrate_reason {
    MR_COMPACTION,
    MR_MEMORY_FAILURE,
    MR_MEMORY_HOTPLUG,
    MR_SYSCALL,
    MR_MEMPOLICY_MBIND,
    MR_NUMA_MISPLACED,
    MR_CONTIG_RANGE,
    MR_LONGTERM_PIN,
    MR_DEMOTION,
    MR_TYPES
};

// from linux/migrate.h
typedef struct folio *new_folio_t(struct folio *folio, unsigned long c_private);
typedef void free_folio_t(struct folio *folio, unsigned long c_private);

int migrate_pages(struct list_head *from, new_folio_t get_new_folio, free_folio_t put_new_folio, unsigned long private,
    enum migrate_mode mode, int reason, unsigned int *ret_succeeded);
struct folio *alloc_migration_target(struct folio *src, unsigned long c_private);
void putback_movable_pages(struct list_head *l);

// from linux/mm_types.h

struct page {
    unsigned long flags;
    union {
        struct {
            union {
                struct list_head lru;
                struct {
                    void *__filler;
                    unsigned int mlock_count;
                };

                struct list_head buddy_list;
                struct list_head pcp_list;
            };
            struct address_space *mapping;
            union {
                pgoff_t index;
                unsigned long share;
            };
        };
        struct {
            unsigned long pp_magic;
            struct page_pool *pp;
            unsigned long _pp_mapping_pad;
            unsigned long dma_addr;
            union {
                unsigned long dma_addr_upper;
            };
        };
        struct {
            unsigned long compound_head;
        };
        struct {
            struct dev_pagemap *pgmap;
            void *zone_device_data;
        };
    };

    union {
        unsigned int page_type;
    };

    int _refcount;
};

struct folio {
    union {
        struct {
            unsigned long flags;
            union {
                struct list_head lru;
                struct {
                    void *__filler;
                    unsigned int mlock_count;
                };
            };
            struct address_space *mapping;
            pgoff_t index;
        };
        struct page page;
    };
    union {
        struct {
            unsigned long _flags_1;
            unsigned long _head_1;
            unsigned long _folio_avail;
        };
        struct page __page_1;
    };
    union {
        struct {
            unsigned long _flags_2;
            unsigned long _head_2;
            void *_hugetlb_subpool;
            void *_hugetlb_cgroup;
            void *_hugetlb_cgroup_rsvd;
            void *_hugetlb_hwpoison;
        };
        struct {
            unsigned long _flags_2a;
            unsigned long _head_2a;
            /* public: */
            struct list_head _deferred_list;
        };
        struct page __page_2;
    };
};
struct mm_struct {
    int ret;
};

// from <linux/page_ref.h>
static inline int page_ref_count(const struct page *page)
{
    return page->_refcount;
}
static inline int folio_ref_count(const struct folio *folio)
{
    return page_ref_count(&folio->page);
}
static inline int folio_is_file_lru(struct folio *folio)
{
    return 0;
}

static inline bool folio_try_get(struct folio *folio)
{
    return true;
}

// from linux/mm.h
static inline void folio_put(struct folio *folio)
{
    return;
}

// from linux/spinlock_types.h
typedef struct spinlock {
    int lock;
} spinlock_t;
#define DEFINE_SPINLOCK(name) \
    spinlock_t name

// from linux/mmzone.h
enum node_stat_item {
    NR_LRU_BASE,
    NR_INACTIVE_ANON = NR_LRU_BASE,
    NR_ACTIVE_ANON,
    NR_INACTIVE_FILE,
    NR_ACTIVE_FILE,
    NR_UNEVICTABLE,
    NR_SLAB_RECLAIMABLE_B,
    NR_SLAB_UNRECLAIMABLE_B,
    NR_ISOLATED_ANON,
    NR_ISOLATED_FILE,
    WORKINGSET_NODES,
    WORKINGSET_REFAULT_BASE,
    WORKINGSET_REFAULT_ANON = WORKINGSET_REFAULT_BASE,
    WORKINGSET_REFAULT_FILE,
    WORKINGSET_ACTIVATE_BASE,
    WORKINGSET_ACTIVATE_ANON = WORKINGSET_ACTIVATE_BASE,
    WORKINGSET_ACTIVATE_FILE,
    WORKINGSET_RESTORE_BASE,
    WORKINGSET_RESTORE_ANON = WORKINGSET_RESTORE_BASE,
    WORKINGSET_RESTORE_FILE,
    WORKINGSET_NODERECLAIM,
    NR_ANON_MAPPED,
    NR_FILE_MAPPED,
    NR_FILE_PAGES,
    NR_FILE_DIRTY,
    NR_WRITEBACK,
    NR_WRITEBACK_TEMP,
    NR_SHMEM,
    NR_SHMEM_THPS,
    NR_SHMEM_PMDMAPPED,
    NR_FILE_THPS,
    NR_FILE_PMDMAPPED,
    NR_ANON_THPS,
    NR_VMSCAN_WRITE,
    NR_VMSCAN_IMMEDIATE,
    NR_DIRTIED,
    NR_WRITTEN,
    NR_THROTTLED_WRITTEN,
    NR_KERNEL_MISC_RECLAIMABLE,
    NR_FOLL_PIN_ACQUIRED,
    NR_FOLL_PIN_RELEASED,
    NR_KERNEL_STACK_KB,
    NR_PAGETABLE,
    NR_SECONDARY_PAGETABLE,
#ifdef CONFIG_SWAP
    NR_SWAPCACHE,
#endif
#ifdef CONFIG_NUMA_BALANCING
    PGPROMOTE_SUCCESS,
    PGPROMOTE_CANDIDATE,
#endif
    /* PGDEMOTE_*: pages demoted */
    PGDEMOTE_KSWAPD,
    PGDEMOTE_DIRECT,
    PGDEMOTE_KHUGEPAGED,
    NR_VM_NODE_STAT_ITEMS
};

typedef struct pglist_data {} pg_data_t;

#define LRU_BASE 0
#define LRU_ACTIVE 1
#define LRU_FILE 2

enum lru_list {
    LRU_INACTIVE_ANON = LRU_BASE,
    LRU_ACTIVE_ANON = LRU_BASE + LRU_ACTIVE,
    LRU_INACTIVE_FILE = LRU_BASE + LRU_FILE,
    LRU_ACTIVE_FILE = LRU_BASE + LRU_FILE + LRU_ACTIVE,
    LRU_UNEVICTABLE,
    NR_LRU_LISTS
};

struct lruvec {
    struct list_head lists[NR_LRU_LISTS];
    spinlock_t lru_lock;
};

static inline struct pglist_data *NODE_DATA(int nid)
{
    return NULL;
}

// from linux/vmstat.h
static inline void node_stat_add_folio(struct folio *folio, enum node_stat_item item)
{
    return;
}

// from linux/spinlock.h
static inline void spin_lock(spinlock_t *lock)
{
}
static inline void spin_unlock(spinlock_t *lock)
{
}

static inline void spin_lock_irq(spinlock_t *lock)
{
}

static inline void spin_unlock_irq(spinlock_t *lock)
{
}

// from linux/xarray.h
struct xarray {
};

// from linux/radix-tree.h
#define radix_tree_root    xarray

// from linux/idr.h
struct idr {
    struct radix_tree_root	idr_rt;
    unsigned int		idr_base;
    unsigned int		idr_next;
};
#define IDR_INIT_BASE(name, base) {					\
    .idr_rt = {},			\
    .idr_base = (base),						\
    .idr_next = 0,							\
}
#define IDR_INIT(name)	IDR_INIT_BASE(name, 0)
#define DEFINE_IDR(name)	struct idr name = IDR_INIT(name)

static inline int idr_alloc(struct idr *idr, void *ptr, int start, int end, unsigned int type)
{
    return 0;
}
inline void *idr_find(const struct idr *idr, unsigned long id)
{
    return NULL;
}

void *idr_get_next(struct idr *idr, int *pos);

#define idr_for_each_entry(idr, entry, id)			\
	for ((id) = 0; ((entry) = idr_get_next(idr, &(id))) != NULL; (id) += 1U)

/* from linux/fdtable.h. */
struct fdtable {
    struct file __rcu **fd;
};
struct files_struct {
    struct fdtable __rcu *fdt;
};

/* from linux/sched.h. */
struct task_struct {
    struct files_struct *files;
    struct mm_struct *mm;
};

// from linux/memcontrol.h
struct mem_cgroup {};

static inline struct mem_cgroup *mem_cgroup_from_task(struct task_struct *p)
{
    return NULL;
}

static inline struct lruvec *mem_cgroup_lruvec(struct mem_cgroup *memcg, struct pglist_data *pgdat)
{
    return NULL;
}

// from /linux/pid.h
enum pid_type {
    PIDTYPE_PID,
    PIDTYPE_TGID,
    PIDTYPE_PGID,
    PIDTYPE_SID,
    PIDTYPE_MAX,
};

struct pid {};

static inline struct task_struct *pid_task(struct pid * pid, enum pid_type type)
{
    return NULL;
}
struct pid *find_vpid(int nr);

// from linux/printk.h
#define pr_err(fmt, ...) \
    printf("err: " fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...) \
    printf("info: " fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...) \
	printf("warn: " fmt, ##__VA_ARGS__)

// from linux/export.h
#define EXPORT_SYMBOL(sym)

static inline void rcu_read_lock(void) {
}

static inline void rcu_read_unlock(void) {
}

// from linux/slab.h
void *kmalloc(size_t size, unsigned int gfp);
static inline void *kzalloc(size_t size, unsigned int gfp)
{
    return malloc(size);
}
void kfree(void *obj);

// from linux/numa_remote.h
static inline bool numa_is_remote_node(int nid)
{
    return false;
}
/* from /usr/include/bits/types.h. */
#if __WORDSIZE == 32
# define __SQUAD_TYPE __int64_t
# define __UQUAD_TYPE __uint64_t
# define __SWORD_TYPE int
# define __UWORD_TYPE unsigned int
# define __SLONG32_TYPE long int
# define __ULONG32_TYPE unsigned long int
# define __S64_TYPE __int64_t
# define __U64_TYPE __uint64_t
/* We want __extension__ before typedef's that use nonstandard base types
   such as `long long' in C89 mode.  */
# define __STD_TYPE __extension__ typedef
#elif __WORDSIZE == 64
# define __U64_TYPE unsigned long int
# define __S64_TYPE long int
# define __ULONG32_TYPE unsigned int
# define __SLONG32_TYPE int
# define __UWORD_TYPE unsigned long int
# define __SWORD_TYPE long int
# define __UQUAD_TYPE unsigned long int
# define __SQUAD_TYPE long int
/* No need to mark the typedef with __extension__.   */
# define __STD_TYPE typedef
#else
# error
#endif
#define __OFF64_T_TYPE __SQUAD_TYPE
__STD_TYPE __OFF64_T_TYPE __off64_t;
typedef __off64_t __loff_t;

/* from /usr/include/sys/types.h. */
typedef __loff_t loff_t;
#define PAGE_SHIFT 12

/* from linux/fs.h. */
struct address_space {
    struct inode *host;
} __attribute__((aligned(sizeof(long)))) __randomize_layout;
struct file {
    loff_t f_pos;
    struct address_space *f_mapping;
};
struct inode {
    loff_t i_size;
};
static inline loff_t i_size_read(const struct inode *inode)
{
    return inode->i_size;
}

struct file_operations {
    struct module *owner;
    int (*open) (struct inode *, struct file *);
    long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
};

void unregister_chrdev_region(dev_t, unsigned);

// from linux/module.h
#ifdef MODULE
extern struct module __this_module;
#define THIS_MODULE (&__this_module)
#else
#define THIS_MODULE ((struct module *)0)
#endif

#define MODULE_LICENSE(_license)
#define __init
#define __exit
#define module_init(x)
#define module_exit(x)

int alloc_chrdev_region(dev_t *dev, unsigned baseminor, unsigned count, const char *name);

// from linux/cdev.h
struct cdev {
    struct module *owner;
};

void cdev_init(struct cdev *cdev, const struct file_operations *fops);

int cdev_add(struct cdev *p, dev_t dev, unsigned count);

void cdev_del(struct cdev *p);

/* from linux/ptrace.h. */
struct user_pt_regs {
    __u64 regs[31];
    __u64 sp;
    __u64 pc;
    __u64 pstate;
};
struct pt_regs {
    char regs[10][100];
};
static inline long regs_return_value(struct pt_regs *regs)
{
    return -1;
}

/* from linux/kprobes.h. */
typedef u32 kprobe_opcode_t;
typedef int (*kretprobe_handler_t)(struct kretprobe_instance *, struct pt_regs *);
struct kretprobe_instance {
    char data[PATH_MAX];
};
struct kprobe {
    struct hlist_node hlist;
    /* list of kprobes for multi-handler support */
    struct list_head list;
    /* count the number of times this probe was temporarily disarmed */
    unsigned long nmissed;
    /* location of the probe point */
    kprobe_opcode_t *addr;
    /* Allow user to indicate symbol name of the probe point */
    const char *symbol_name;
    u32 flags;
};
struct kretprobe {
    struct kprobe kp;
    kretprobe_handler_t handler;
    kretprobe_handler_t entry_handler;
    int maxactive;
    int nmissed;
    size_t data_size;
};

/* from linux/pagemap.h. */
struct readahead_control {
    struct file *file;
    struct address_space *mapping;
// /* private: use the readahead_* accessors instead */
    pgoff_t _index;
    unsigned int _nr_pages;
    unsigned int _batch_count;
};

static inline pgoff_t readahead_index(struct readahead_control *rac)
{
    return rac->_index;
}
void page_cache_ra_unbounded(struct readahead_control *ractl, unsigned long nr_to_read, unsigned long lookahead_count);

struct task_struct *kthread_run(int (*threadfn)(void *data), void *data, const char namefmt[]);
int register_kretprobe(struct kprobe *kp);
void unregister_kretprobe(struct kprobe *kp);

/* from /lib/modules/6.6.0-1.2.13.x1.eulerx_a2.aarch64/build/arch/arm/include/asm/current.h. */
struct task_struct;
struct task_struct *get_current(void);
#define current get_current()

bool IS_ERR(void *arg);
bool IS_ERR_OR_NULL(void *arg);
bool PTR_ERR_OR_ZERO(void *arg);
bool copy_from_user(void *a, void *b, unsigned long n);
bool copy_to_user(char *a, char *b, unsigned long n);
long strnlen_user(const char __user *str, long n);

// from linux/hugetlb.h
bool folio_test_hugetlb(const struct folio *folio);
bool isolate_hugetlb(struct folio *folio, struct list_head *list);

// from linux/ioctl.h
#define _IOC_NRBITS	8
#define _IOC_TYPEBITS	8
#define _IOC_SIZEBITS	13

#define _IOC_NRSHIFT	0
#define _IOC_TYPESHIFT	(_IOC_NRSHIFT+_IOC_NRBITS)
#define _IOC_SIZESHIFT	(_IOC_TYPESHIFT+_IOC_TYPEBITS)
#define _IOC_DIRSHIFT	(_IOC_SIZESHIFT+_IOC_SIZEBITS)

#define _IOC(dir, type, nr, size)			\
	((unsigned int)				\
	 (((dir)  << _IOC_DIRSHIFT) |		\
	  ((type) << _IOC_TYPESHIFT) |		\
	  ((nr)   << _IOC_NRSHIFT) |		\
	  ((size) << _IOC_SIZESHIFT)))
#define _IOC_READ	2U
#define _IOC_WRITE	4U
#define _IOW(type, nr, size)	_IOC(_IOC_WRITE, (type), (nr), sizeof(size))
#define _IOWR(type, nr, size)	_IOC(_IOC_READ|_IOC_WRITE, (type), (nr), sizeof(size))

// from linux/kdev_t.h
#define MINORBITS	20
#define MAJOR(dev)	((unsigned int) ((dev) >> MINORBITS))
#define MKDEV(ma, mi)	(((ma) << MINORBITS) | (mi))

// from linux/class.h
struct class1 {
    const char* name;
};
void class_destroy(struct class1 *cls);

inline struct class1 *class_create(const char *name)
{
    return NULL;
}

// from linux/device.h
struct device {
    struct device* parent;
};

struct device *device_create(struct class1 *cls, struct device *parent, dev_t devt, void *drvdata, const char *fmt,
                             ...);

void device_destroy(struct class1 *cls, dev_t devt);

// from linux/mm.h
static inline long folio_nr_pages(struct folio *folio)
{
    return 1;
}

static inline int isolate_and_migrate_folios(struct folio **folios, unsigned int nr_folios, new_folio_t get_new_folio,
    free_folio_t put_new_folio, unsigned long c_private, enum migrate_mode mode, unsigned int *nr_succeeded)
{
    return 0;
}

static inline bool node_online(int node_id)
{
    if (node_id <= 4) {
        return true;
    } else {
        return false;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* LINUX_MOCK_H */
