/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef _LINUX_MAPLE_TREE_DEPENDS_H
#define _LINUX_MAPLE_TREE_DEPENDS_H

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/rcupdate.h>

#if defined(CONFIG_64BIT) || defined(BUILD_VDSO32_64)
#define MAPLE_ARANGE64_META_MAX	15
#define MAPLE_ARANGE64_SLOTS	10
#define MAPLE_RANGE64_SLOTS	16
#define MAPLE_NODE_SLOTS	31
#define MAPLE_ALLOC_SLOTS	(MAPLE_NODE_SLOTS - 1)
#else
#define MAPLE_ARANGE64_META_MAX	31
#define MAPLE_ARANGE64_SLOTS	21
#define MAPLE_RANGE64_SLOTS	32
#define MAPLE_NODE_SLOTS	63
#define MAPLE_ALLOC_SLOTS	(MAPLE_NODE_SLOTS - 2)
#endif

#define MAPLE_NODE_MASK		255UL

struct maple_metadata {
    unsigned char gap;
	unsigned char end;
};

struct maple_range_64 {
	struct maple_pnode *parent;
	unsigned long pivot[MAPLE_RANGE64_SLOTS - 1];
	union {
		void *slot[MAPLE_RANGE64_SLOTS];
		struct {
			void *pad[MAPLE_RANGE64_SLOTS - 1];
			struct maple_metadata meta;
		};
	};
};

struct maple_arange_64 {
	struct maple_pnode *parent;
	unsigned long pivot[MAPLE_ARANGE64_SLOTS - 1];
	void *slot[MAPLE_ARANGE64_SLOTS];
	unsigned long gap[MAPLE_ARANGE64_SLOTS];
	struct maple_metadata meta;
};

struct maple_topiary {
	struct maple_pnode *parent;
	struct maple_enode *next;
};

struct maple_alloc {
	unsigned long total;
	unsigned char node_count;
	unsigned int request_count;
	struct maple_alloc *slot[MAPLE_ALLOC_SLOTS];
};

enum maple_type {
	maple_dense = 0,
	maple_leaf_64 = 1,
	maple_range_64 = 2,
	maple_arange_64 = 3,
};

#define MAPLE_RESERVED_RANGE	4096

#define MAPLE_NODE_TYPE_SHIFT	0x03
#define MAPLE_NODE_TYPE_MASK	0x0F

#define MAPLE_HEIGHT_MAX	31

#define MT_FLAGS_LOCK_EXTERN	0x300
#define MT_FLAGS_LOCK_BH	0x200
#define MT_FLAGS_LOCK_IRQ	0x100
#define MT_FLAGS_LOCK_MASK	0x300
#define MT_FLAGS_HEIGHT_MASK	0x7C
#define MT_FLAGS_HEIGHT_OFFSET	0x02
#define MT_FLAGS_USE_RCU	0x02
#define MT_FLAGS_ALLOC_RANGE	0x01

typedef struct { } lockdep_map_p;

struct maple_tree {
	union {
		lockdep_map_p ma_external_lock;
	};
	void *ma_root;
	unsigned int ma_flags;
};

#define mt_lock_is_held(m_tree)	1
#define mt_set_external_lock(m_tree, lock)	do { } while (0)

#define MTREE_INIT(name, __flags) {					\
	.ma_flags = (__flags),						\
	.ma_root = NULL,						\
}

#ifdef CONFIG_LOCKDEP
#define MTREE_INIT_EXT(name, __flags, __lock) {				\
	.ma_external_lock = &(__lock).dep_map,				\
	.ma_flags = (__flags),						\
	.ma_root = NULL,						\
}
#else
#define MTREE_INIT_EXT(name, __flags, __lock)	MTREE_INIT(name, __flags)
#endif

#define DEFINE_MTREE(name) struct maple_tree (name) = MTREE_INIT(name, 0)

#define mtree_lock(m_tree)
#define mtree_unlock(m_tree)

struct maple_node {
	union {
        struct maple_range_64 mr64;
		struct maple_arange_64 ma64;
		struct maple_alloc alloc;
        struct {
            unsigned int ma_flags;
            enum maple_type type;
            struct rcu_head rcu;
			void *pad;
		};
		struct {
			struct maple_pnode *parent;
			void *slot[MAPLE_NODE_SLOTS];
		};
	};
};

struct ma_topiary {
    struct maple_enode *tail;
	struct maple_enode *head;
	struct maple_tree *mtree;
};

void *mtree_load(struct maple_tree *m_tree, unsigned long index);

int mtree_insert(struct maple_tree *m_tree, unsigned long index, void *entry, gfp_t gfp);
int mtree_insert_range(struct maple_tree *m_tree, unsigned long first,
		unsigned long last, void *entry, gfp_t gfp);
int mtree_alloc_range(struct maple_tree *m_tree, unsigned long *startp,
		void *entry, unsigned long size, unsigned long min,
		unsigned long max, gfp_t gfp);
int mtree_alloc_rrange(struct maple_tree *m_tree, unsigned long *startp,
		void *entry, unsigned long size, unsigned long min,
		unsigned long max, gfp_t gfp);

int mtree_store_range(struct maple_tree *m_tree, unsigned long first, unsigned long last, void *entry, gfp_t gfp);
int mtree_store(struct maple_tree *m_tree, unsigned long index, void *entry, gfp_t gfp);
void *mtree_erase(struct maple_tree *m_tree, unsigned long index);

void mtree_destroy(struct maple_tree *m_tree);
void __mt_destroy(struct maple_tree *m_tree);

static inline bool mtree_empty(const struct maple_tree *m_tree)
{
	return m_tree->ma_root == NULL;
}

struct ma_state {
	struct maple_tree *tree;
    unsigned long last;
	unsigned long index;
	struct maple_enode *node;
    unsigned long max;
	unsigned long min;
	struct maple_alloc *alloc;
    unsigned char mas_flags;
    unsigned char offset;
	unsigned char depth;
};

struct ma_wr_state {
	struct ma_state *maple_state;
	struct maple_node *node;
    unsigned long r_max;
	unsigned long r_min;
	enum maple_type type;
    unsigned char node_end;
	unsigned char offset_end;
	unsigned long *pivots;
	unsigned long end_piv;
    void *content;
	void *entry;
	void **slots;
};

#define mas_lock(maple_state)
#define mas_unlock(maple_state)

#define MA_ERROR(err) ((struct maple_enode *)(((unsigned long)(err) << 2) | 2UL))
#define MAS_PAUSE	((struct maple_enode *)17UL)
#define MAS_NONE	((struct maple_enode *)9UL)
#define MAS_ROOT	((struct maple_enode *)5UL)
#define MAS_START	((struct maple_enode *)1UL)

#define MA_STATE(name, m_tree, start, end)					\
	struct ma_state (name) = {					\
		.tree = (m_tree),						\
		.index = (start),						\
		.last = (end),						\
		.node = MAS_START,					\
		.min = 0,						\
		.max = ULONG_MAX,					\
		.alloc = NULL,						\
	}

#define MA_WR_STATE(name, ma_state, wr_entry)				\
	struct ma_wr_state (name) = {					\
		.maple_state = (ma_state),					\
		.content = NULL,					\
		.entry = (wr_entry),					\
	}

#define MA_TOPIARY(name, m_tree)						\
	struct ma_topiary (name) = {					\
		.head = NULL,						\
		.tail = NULL,						\
		.mtree = (m_tree),						\
	}

bool mas_is_err(struct ma_state *maple_state);
int mas_preallocate(struct ma_state *maple_state, void *entry, gfp_t gfp);
void *mas_find_rev(struct ma_state *maple_state, unsigned long min);
void *mas_find(struct ma_state *maple_state, unsigned long max_idx);
void mas_store_prealloc(struct ma_state *maple_state, void *entry);
int mas_store_gfp(struct ma_state *maple_state, void *entry, gfp_t gfp);
void *mas_erase(struct ma_state *maple_state);
void *mas_store(struct ma_state *maple_state, void *entry);
void *mas_walk(struct ma_state *maple_state);

void *mas_prev(struct ma_state *maple_state, unsigned long min);
void *mas_next(struct ma_state *maple_state, unsigned long max);

int mas_expected_entries(struct ma_state *maple_state, unsigned long nr_entries);
void mas_destroy(struct ma_state *maple_state);
void maple_tree_init(void);
void mas_pause(struct ma_state *maple_state);
bool mas_nomem(struct ma_state *maple_state, gfp_t gfp);

int mas_empty_area(struct ma_state *maple_state, unsigned long min, unsigned long max, unsigned long size);

static inline bool mas_is_none(struct ma_state *maple_state)
{
	return maple_state->node == MAS_NONE;
}

static inline bool mas_is_paused(struct ma_state *maple_state)
{
	return maple_state->node == MAS_PAUSE;
}

void mas_dup_tree(struct ma_state *oldmas, struct ma_state *maple_state);
void mas_dup_store(struct ma_state *maple_state, void *entry);

int mas_empty_area_rev(struct ma_state *maple_state, unsigned long min,
		       unsigned long max, unsigned long size);

static inline void mas_reset(struct ma_state *maple_state)
{
	maple_state->node = MAS_START;
}

#define mas_for_each(__mas, __entry, __max) \
	while (((__entry) = mas_find((__mas), (__max))) != NULL)

static inline void mas_set_range(struct ma_state *maple_state, unsigned long start, unsigned long last)
{
	maple_state->index = start;
	maple_state->last = last;
	maple_state->node = MAS_START;
}

static inline void mas_set(struct ma_state *maple_state, unsigned long index)
{
	mas_set_range(maple_state, index, index);
}

static inline bool mt_external_lock(const struct maple_tree *m_tree)
{
	return (m_tree->ma_flags & MT_FLAGS_LOCK_MASK) == MT_FLAGS_LOCK_EXTERN;
}

static inline void mt_init_flags(struct maple_tree *m_tree, unsigned int flags)
{
	m_tree->ma_flags = flags;
	rcu_assign_pointer(m_tree->ma_root, NULL);
}

static inline bool mt_in_rcu(struct maple_tree *m_tree)
{
#ifdef CONFIG_MAPLE_RCU_DISABLED
	return false;
#endif
	return m_tree->ma_flags & MT_FLAGS_USE_RCU;
}

static inline void mt_init(struct maple_tree *m_tree)
{
	mt_init_flags(m_tree, 0);
}

static inline void mt_set_in_rcu(struct maple_tree *m_tree)
{
	if (mt_in_rcu(m_tree))
		return;

	if (mt_external_lock(m_tree)) {
		m_tree->ma_flags |= MT_FLAGS_USE_RCU;
	} else {
		mtree_lock(m_tree);
		m_tree->ma_flags |= MT_FLAGS_USE_RCU;
		mtree_unlock(m_tree);
	}
}

static inline void mt_clear_in_rcu(struct maple_tree *m_tree)
{
	if (!mt_in_rcu(m_tree))
		return;

	if (mt_external_lock(m_tree)) {
		m_tree->ma_flags &= ~MT_FLAGS_USE_RCU;
	} else {
		mtree_lock(m_tree);
		m_tree->ma_flags &= ~MT_FLAGS_USE_RCU;
		mtree_unlock(m_tree);
	}
}

static inline unsigned int mt_height(const struct maple_tree *m_tree)

{
	return (m_tree->ma_flags & MT_FLAGS_HEIGHT_MASK) >> MT_FLAGS_HEIGHT_OFFSET;
}

void *mt_next(struct maple_tree *m_tree, unsigned long idx, unsigned long max_idx);
void *mt_prev(struct maple_tree *m_tree, unsigned long idx,  unsigned long min_idx);
void *mt_find_after(struct maple_tree *m_tree, unsigned long *idx, unsigned long max_idx);
void *mt_find(struct maple_tree *m_tree, unsigned long *idx, unsigned long max_idx);

#define mt_for_each(__tree, __entry, __index, __max) \
	for ((__entry) = mt_find(__tree, &(__index), __max); __entry; \
		(__entry) = mt_find_after(__tree, &(__index), __max))

#define MT_BUG_ON(__tree, __x) BUG_ON(__x)

#endif
