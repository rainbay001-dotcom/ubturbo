// SPDX-License-Identifier: GPL-2.0+
#include <linux/xarray.h>
#include <linux/limits.h>
#include <linux/maple_tree.h>

#define MA_ROOT_PARENT 1

static struct kmem_cache *maple_node_cache;
#define ma_parent_ptr(x) ((struct maple_pnode *)(x))
#define ma_enode_ptr(x) ((struct maple_enode *)(x))
#define ma_mnode_ptr(x) ((struct maple_node *)(x))

#define MA_STATE_PREALLOC	4
#define MA_STATE_BULK		1
#define MA_STATE_REBALANCE	2

#ifdef CONFIG_DEBUG_MAPLE_TREE
// Array to store maximum values for different maple tree configurations
static const unsigned long mt_max[] = {
    // Maximum value for maple_arange_64
    [maple_arange_64]	= ULONG_MAX,

    // Maximum value for maple_range_64
    [maple_range_64]	= ULONG_MAX,

    // Maximum value for maple_leaf_64
    [maple_leaf_64]		= ULONG_MAX,

    // Maximum value for maple_dense
    [maple_dense]		= MAPLE_NODE_SLOTS,
};
#define mt_node_max(val) mt_max[mte_node_type(val)]
#endif

static const unsigned char mt_slots[] = {
    [maple_arange_64]	= MAPLE_ARANGE64_SLOTS,
    [maple_range_64]	= MAPLE_RANGE64_SLOTS,
    [maple_leaf_64]		= MAPLE_RANGE64_SLOTS,
    [maple_dense]		= MAPLE_NODE_SLOTS,
};
#define mt_slot_count(val) mt_slots[mte_node_type(val)]
#define MAPLE_BIG_NODE_GAPS	(MAPLE_ARANGE64_SLOTS * 2 + 1)
#define DEPENDS_GAPS (MAPLE_ARANGE64_SLOTS * 2 + 1)

static const unsigned char mt_pivots[] = {
    [maple_arange_64]	= MAPLE_ARANGE64_SLOTS - 1,
    [maple_range_64]	= MAPLE_RANGE64_SLOTS - 1,
    [maple_leaf_64]		= MAPLE_RANGE64_SLOTS - 1,
	[maple_dense]		= 0,
};
#define mt_pivot_count(val) mt_pivots[mte_node_type(val)]

#define MAPLE_BIG_NODE_SLOTS (MAPLE_RANGE64_SLOTS * 2 + 2)
#define DEPENDS_SLOTS (MAPLE_RANGE64_SLOTS * 2 + 2)

static const unsigned char mt_min_slots[] = {
    [maple_arange_64]	= (MAPLE_ARANGE64_SLOTS / 2) - 1,
    [maple_range_64]	= (MAPLE_RANGE64_SLOTS / 2) - 2,
    [maple_leaf_64]		= (MAPLE_RANGE64_SLOTS / 2) - 2,
	[maple_dense]		= MAPLE_NODE_SLOTS / 2,
};
#define mt_min_slot_count(val) mt_min_slots[mte_node_type(val)]

struct maple_big_node {
	union {
		struct {
			uint64_t padding[DEPENDS_GAPS];
            uint64_t gap[DEPENDS_GAPS];
		};
		char reserved2;
		/* Depends maple_enode */
		struct maple_enode *slot[DEPENDS_SLOTS];
	};
	/* Depends maple_pnode */
	struct maple_pnode *parent;
	char reserved1;
	/* Depends pivot */
	unsigned long pivot[DEPENDS_SLOTS - 1];
	char reserved3;
	uint8_t b_end;
	char reserved4;
	enum maple_type type;
	char reserved5;
};


struct maple_subtree_state {
    struct ma_state *orig_r;
	char reserved1;
	struct ma_state *orig_l;
	char reserved2;
    struct ma_state *r;
	char reserved3;
    struct ma_state *m;
	char reserved4;
	struct ma_state *l;
	char reserved5;
    struct ma_topiary *destroy;
	char reserved6;
	struct ma_topiary *free;
	char reserved7;
	struct maple_big_node *bn;
};

static inline enum maple_type mte_node_type(const struct maple_enode *mt_entry)
{
	return ((unsigned long)(void *)mt_entry >> MAPLE_NODE_TYPE_SHIFT) &
		MAPLE_NODE_TYPE_MASK;
}

static inline bool ma_is_leaf(const enum maple_type type)
{
	return type < maple_range_64;
}

#define MAPLE_ENODE_TYPE_SHIFT		0x03

static inline bool mas_is_ptr(struct ma_state *maple_state)
{
	return maple_state->node == MAS_ROOT;
}

#define MAPLE_ROOT_NODE			0x02

static inline bool mas_is_start(struct ma_state *maple_state)
{
	return maple_state->node == MAS_START;
}

#define MAPLE_PARENT_NOT_RANGE16	0x02

static inline bool mas_searchable(struct ma_state *maple_state)
{
	if (mas_is_none(maple_state))
		return false;

	if (mas_is_ptr(maple_state))
		return false;

	return true;
}

#define MAPLE_PARENT_RANGE64		0x06

static inline struct maple_node *mte_to_node(const struct maple_enode *mt_entry)
{
	return (struct maple_node *)((unsigned long)(void *)mt_entry & ~MAPLE_NODE_MASK);
}

#define MAPLE_PARENT_16B_SLOT_MASK	0xFC

static inline struct maple_node *mas_mn(const struct ma_state *maple_state)
{
	return mte_to_node(maple_state->node);
}

#define MAPLE_ENODE_NULL		0x04

static inline struct maple_enode *mt_mk_node(const struct maple_node *node, enum maple_type type)
{
	return (void *)((unsigned long)node | (type << MAPLE_ENODE_TYPE_SHIFT) | MAPLE_ENODE_NULL);
}

#define MAPLE_PARENT_16B_SLOT_SHIFT	0x02

static inline void *mte_safe_root(const struct maple_enode *node)
{
	return (void *)((unsigned long)node & ~MAPLE_ROOT_NODE);
}

#define MAPLE_PARENT_SLOT_SHIFT		0x03

static inline bool ma_is_root(struct maple_node *node)
{
	return ((unsigned long)node->parent & MA_ROOT_PARENT);
}

#define MAPLE_PARENT_SLOT_MASK		0xF8

static inline bool mte_is_root(const struct maple_enode *node)
{
	return ma_is_root(mte_to_node(node));
}

#define MAPLE_PARENT_ROOT		0x01

static inline bool mt_is_alloc(struct maple_tree *mt)
{
	return (mt->ma_flags & MT_FLAGS_ALLOC_RANGE);
}
#define MAPLE_PARENT_RANGE32		0x04

static inline unsigned long mte_parent_shift(unsigned long parent)
{
	/* Note bit 1 == 0 means 16B */
	if (likely(parent & MAPLE_PARENT_NOT_RANGE16))
		return MAPLE_PARENT_SLOT_SHIFT;

	return MAPLE_PARENT_16B_SLOT_SHIFT;
}

static inline unsigned long mte_parent_slot_mask(unsigned long parent)
{
	/* Note bit 1 == 0 means 16B */
	if (likely(parent & MAPLE_PARENT_NOT_RANGE16))
		return MAPLE_PARENT_SLOT_MASK;

	return MAPLE_PARENT_16B_SLOT_MASK;
}

static enum maple_type mte_parent_enum(struct maple_enode *enode_p, struct maple_tree *m_tree)
{
	unsigned long type;

	type = (unsigned long)(void *)enode_p;
	if (type & MAPLE_PARENT_ROOT)
		return 0;

	type &= MAPLE_NODE_MASK;
	type = type & ~(MAPLE_PARENT_ROOT | mte_parent_slot_mask(type));

	switch (type) {
        case MAPLE_PARENT_RANGE64:
            if (mt_is_alloc(m_tree))
                return maple_arange_64;
            return maple_range_64;
        default:
            return 0;
	}
}

static inline enum maple_type mas_parent_enum(struct ma_state *maple_state, struct maple_enode *enode)
{
	return mte_parent_enum(ma_enode_ptr(mte_to_node(enode)->parent), maple_state->tree);
}

static inline unsigned int mte_parent_slot(const struct maple_enode *m_enode)
{
	unsigned long value = (unsigned long) mte_to_node(m_enode)->parent;
	if (value & 1)
		return 0;

	return (value & MAPLE_PARENT_16B_SLOT_MASK) >> mte_parent_shift(value);
}

static inline struct maple_node *mte_parent(const struct maple_enode *enode)
{
	return (void *)((unsigned long)
			(mte_to_node(enode)->parent) & ~MAPLE_NODE_MASK);
}

static inline bool ma_dead_node(const struct maple_node *node)
{
	struct maple_node *parent = (void *)((unsigned long)
					     node->parent & ~MAPLE_NODE_MASK);

	return (parent == node);
}

static inline unsigned long *ma_pivots(struct maple_node *m_node, enum maple_type m_type)
{
	switch (m_type) {
        case maple_arange_64:
            return m_node->ma64.pivot;
        case maple_range_64:
        case maple_leaf_64:
            return m_node->mr64.pivot;
        case maple_dense:
            return NULL;
    }
	return NULL;
}

static inline unsigned long
mas_safe_min(struct ma_state *maple_state, unsigned long *pivots, unsigned char offset)
{
	if (likely(offset))
		return pivots[offset - 1] + 1;

	return maple_state->min;
}

static inline void **ma_slots(struct maple_node *m_node, enum maple_type m_tree)
{
	switch (m_tree) {
        default:
        case maple_arange_64:
            return m_node->ma64.slot;
        case maple_range_64:
        case maple_leaf_64:
            return m_node->mr64.slot;
        case maple_dense:
            return m_node->slot;
	}
}

static inline unsigned long
mas_safe_pivot(const struct ma_state *maple_state, unsigned long *pivots,
	       unsigned char piv, enum maple_type type)
{
	if (piv >= mt_pivots[type])
		return maple_state->max;

	return pivots[piv];
}

static inline void *mt_slot(const struct maple_tree *m_tree,
		void **slots, unsigned char offset)
{
	return slots[offset];
}

static inline void *mas_slot(struct ma_state *maple_state, void **slots,
			     unsigned char offset)
{
	return mt_slot(maple_state->tree, slots, offset);
}

static inline void *mas_root(struct ma_state *maple_state)
{
	return maple_state->tree->ma_root;
}

static inline struct maple_metadata *ma_meta(struct maple_node *m_node, enum maple_type m_tree)
{
	switch (m_tree) {
        case maple_arange_64:
            return &m_node->ma64.meta;
        default:
            return &m_node->mr64.meta;
	}
}

static inline unsigned char ma_meta_end(struct maple_node *mn,
					enum maple_type m_tree)
{
	struct maple_metadata *meta = ma_meta(mn, m_tree);

	return meta->end;
}

static int mas_ascend(struct ma_state *maple_state)
{
    bool set_max = false, set_min = false;
    unsigned char offset;
    unsigned long *pivots;
    unsigned long min, max;
    enum maple_type type_a;
    unsigned char slot_a;
    struct maple_node *p_node;
    struct maple_node *a_node;
    struct maple_enode *enode_a;
	struct maple_enode *enode_p;

	a_node = mas_mn(maple_state);
	if (ma_is_root(a_node)) {
		maple_state->offset = 0;
		return 0;
	}

	p_node = mte_parent(maple_state->node);
	if (unlikely(a_node == p_node))
		return 1;
	type_a = mas_parent_enum(maple_state, maple_state->node);
	offset = mte_parent_slot(maple_state->node);
	enode_a = mt_mk_node(p_node, type_a);

	if (p_node != mte_parent(maple_state->node))
		return 1;

	maple_state->node = enode_a;
	maple_state->offset = offset;

	if (mte_is_root(enode_a)) {
        maple_state->min = 0;
		maple_state->max = ULONG_MAX;
		return 0;
	}

    max = ULONG_MAX;
	min = 0;
	do {
		enode_p = enode_a;
        slot_a = mte_parent_slot(enode_p);
		type_a = mas_parent_enum(maple_state, enode_p);
		a_node = mte_parent(enode_p);
		enode_a = mt_mk_node(a_node, type_a);
        pivots = ma_pivots(a_node, type_a);

		if (!set_min && slot_a) {
            min = pivots[slot_a - 1] + 1;
			set_min = true;
		}

		if (!set_max && slot_a < mt_pivots[type_a]) {
            max = pivots[slot_a];
			set_max = true;
		}

		if (ma_dead_node(a_node))
			return 1;

		if (ma_is_root(a_node))
			break;
	} while (!set_min || !set_max);

	maple_state->max = max;
	maple_state->min = min;
	return 0;
}

static struct maple_enode *mas_start(struct ma_state *maple_state)
{
	if (mas_is_start(maple_state)) {
		struct maple_enode *root;

        maple_state->max = ULONG_MAX;
		maple_state->depth = 0;
		maple_state->offset = 0;
		maple_state->node = MAS_NONE;
		maple_state->min = 0;

		root = mas_root(maple_state);
		if (likely(xa_is_node(root))) {
            maple_state->node = mte_safe_root(root);
			maple_state->depth = 1;
			return NULL;
		}

		if (unlikely(!root)) {
			maple_state->offset = MAPLE_NODE_SLOTS;
			return NULL;
		}

        maple_state->offset = MAPLE_NODE_SLOTS;
		maple_state->node = MAS_ROOT;

		if (maple_state->index > 0)
			return NULL;

		return root;
	}

	return NULL;
}

static inline unsigned char ma_data_end(struct maple_node *m_node, enum maple_type m_type,
					unsigned long *pivots, unsigned long max)
{
	unsigned char pivots_offset;

	if (maple_arange_64 == m_type)
		return ma_meta_end(m_node, m_type);

	pivots_offset = mt_pivots[m_type] - 1;
	if (likely(!pivots[pivots_offset]))
		return ma_meta_end(m_node, m_type);

	if (likely(pivots[pivots_offset] == max))
		return pivots_offset;

	return mt_pivots[m_type];
}

static void *mtree_range_walk(struct ma_state *maple_state)
{
    unsigned long prev_max, prev_min;
    unsigned long max, min;
    unsigned char end;
    void **slots;
    enum maple_type m_type;
    struct maple_enode *next, *last;
    struct maple_node *node;
    unsigned char offset;
	unsigned long *maple_pivots;

    max = maple_state->max;
    min = maple_state->min;
	next = maple_state->node;
	do {
		offset = 0;
		last = next;
        m_type = mte_node_type(next);
		node = mte_to_node(next);
		maple_pivots = ma_pivots(node, m_type);
		end = ma_data_end(node, m_type, maple_pivots, max);
		if (unlikely(ma_dead_node(node)))
			goto dead_node;

		if (maple_pivots[offset] >= maple_state->index) {
            max = maple_pivots[offset];
            prev_min = min;
			prev_max = max;
			goto next;
		}

		do {
			offset++;
		} while ((offset < end) && (maple_pivots[offset] < maple_state->index));

		prev_min = min;
		min = maple_pivots[offset - 1] + 1;
		prev_max = max;
		if (likely(offset < end && maple_pivots[offset]))
			max = maple_pivots[offset];

next:
		slots = ma_slots(node, m_type);
		next = mt_slot(maple_state->tree, slots, offset);
		if (unlikely(ma_dead_node(node)))
			goto dead_node;
	} while (!ma_is_leaf(m_type));

    maple_state->node = last;
    maple_state->max = prev_max;
    maple_state->min = prev_min;
    maple_state->last = max;
    maple_state->index = min;
	maple_state->offset = offset;
	return (void *) next;

dead_node:
	mas_reset(maple_state);
	return NULL;
}

static inline void *mas_state_walk(struct ma_state *maple_state)
{
	void *mt_entry;

	mt_entry = mas_start(maple_state);
	if (mas_is_none(maple_state))
		return NULL;

	if (mas_is_ptr(maple_state))
		return mt_entry;

	return mtree_range_walk(maple_state);
}

static int mas_next_node(struct ma_state *maple_state, struct maple_node *m_node, unsigned long max)
{
	unsigned long min, maple_pivot;
	unsigned long *maple_pivots;
	struct maple_enode *enode;
    unsigned char offset;
	int level = 0;
	enum maple_type m_type;
	void **slots;

	if (maple_state->max >= max)
		goto no_entry;

	level = 0;
	do {
		if (ma_is_root(m_node))
			goto no_entry;

		min = maple_state->max + 1;
		if (min > max)
			goto no_entry;

		if (unlikely(mas_ascend(maple_state)))
			return 1;

		offset = maple_state->offset;
		level++;
		m_node = mas_mn(maple_state);
		m_type = mte_node_type(maple_state->node);
		maple_pivots = ma_pivots(m_node, m_type);
	} while (unlikely(offset == ma_data_end(m_node, m_type, maple_pivots, maple_state->max)));

	slots = ma_slots(m_node, m_type);
	maple_pivot = mas_safe_pivot(maple_state, maple_pivots, ++offset, m_type);
	while (unlikely(level > 1)) {
		enode = mas_slot(maple_state, slots, offset);
		if (unlikely(ma_dead_node(m_node)))
			return 1;

		maple_state->node = enode;
		level--;
		m_node = mas_mn(maple_state);
		m_type = mte_node_type(maple_state->node);
		slots = ma_slots(m_node, m_type);
		maple_pivots = ma_pivots(m_node, m_type);
		offset = 0;
		maple_pivot = maple_pivots[0];
	}

	enode = mas_slot(maple_state, slots, offset);
	if (unlikely(ma_dead_node(m_node)))
		return 1;

    maple_state->max = maple_pivot;
    maple_state->min = min;
	maple_state->node = enode;
	return 0;

no_entry:
	if (unlikely(ma_dead_node(m_node)))
		return 1;

	maple_state->node = MAS_NONE;
	return 0;
}

static void *mas_next_nentry(struct ma_state *maple_state,
	    struct maple_node *node, unsigned long max, enum maple_type m_type)
{
    void *mt_entry;
    void **slots;
	unsigned char count;
	unsigned long maple_pivot;
	unsigned long *maple_pivots;

	if (maple_state->last == maple_state->max) {
		maple_state->index = maple_state->max;
		return NULL;
	}

	maple_pivots = ma_pivots(node, m_type);
	slots = ma_slots(node, m_type);
	maple_state->index = mas_safe_min(maple_state, maple_pivots, maple_state->offset);
	if (ma_dead_node(node))
		return NULL;

	if (maple_state->index > max)
		return NULL;

	count = ma_data_end(node, m_type, maple_pivots, maple_state->max);
	if (maple_state->offset > count)
		return NULL;

	while (maple_state->offset < count) {
		maple_pivot = maple_pivots[maple_state->offset];
		mt_entry = mas_slot(maple_state, slots, maple_state->offset);
		if (ma_dead_node(node))
			return NULL;

		if (mt_entry)
			goto found;

		if (maple_pivot >= max)
			return NULL;

		maple_state->index = maple_pivot + 1;
		maple_state->offset++;
	}

	if (maple_state->index > maple_state->max) {
		maple_state->index = maple_state->last;
		return NULL;
	}

	maple_pivot = mas_safe_pivot(maple_state, maple_pivots, maple_state->offset, m_type);
	mt_entry = mas_slot(maple_state, slots, maple_state->offset);
	if (ma_dead_node(node))
		return NULL;

	if (!maple_pivot)
		return NULL;

	if (!mt_entry)
		return NULL;

found:
	maple_state->last = maple_pivot;
	return mt_entry;
}

static inline void mas_rewalk(struct ma_state *maple_state, unsigned long index)
{
retry:
	mas_set(maple_state, index);
	mas_state_walk(maple_state);
	if (mas_is_start(maple_state))
		goto retry;

	return;
}

static void *mas_next_entry(struct ma_state *maple_state, unsigned long limit)
{
	void *mt_entry = NULL;
	struct maple_enode *prev_node;
	struct maple_node *node;
	unsigned char offset;
	unsigned long last;
	enum maple_type m_type;

	last = maple_state->last;
retry:
	offset = maple_state->offset;
	prev_node = maple_state->node;
	node = mas_mn(maple_state);
	m_type = mte_node_type(maple_state->node);
	maple_state->offset++;
	if (unlikely(maple_state->offset >= mt_slots[m_type])) {
		maple_state->offset = mt_slots[m_type] - 1;
		goto next_node;
	}

	while (!mas_is_none(maple_state)) {
		mt_entry = mas_next_nentry(maple_state, node, limit, m_type);
		if (unlikely(ma_dead_node(node))) {
			mas_rewalk(maple_state, last);
			goto retry;
		}

		if (likely(mt_entry))
			return mt_entry;

		if (unlikely((maple_state->index > limit)))
			break;

next_node:
		prev_node = maple_state->node;
		offset = maple_state->offset;
		if (unlikely(mas_next_node(maple_state, node, limit))) {
			mas_rewalk(maple_state, last);
			goto retry;
		}
		maple_state->offset = 0;
		node = mas_mn(maple_state);
		m_type = mte_node_type(maple_state->node);
	}

	maple_state->index = maple_state->last = limit;
	maple_state->offset = offset;
	maple_state->node = prev_node;
	return NULL;
}

void *mas_walk(struct ma_state *maple_state)
{
	void *mt_entry;

retry:
	mt_entry = mas_state_walk(maple_state);
	if (mas_is_start(maple_state))
		goto retry;

	if (mas_is_ptr(maple_state)) {
		if (!maple_state->index) {
			maple_state->last = 0;
		} else {
            maple_state->last = ULONG_MAX;
			maple_state->index = 1;
		}
		return mt_entry;
	}

	if (mas_is_none(maple_state)) {
        maple_state->last = ULONG_MAX;
		maple_state->index = 0;
	}

	return mt_entry;
}

void *mas_find(struct ma_state *maple_state, unsigned long max_idx)
{
	if (mas_is_paused(maple_state)) {
		if (maple_state->last == ULONG_MAX) {
			maple_state->node = MAS_NONE;
			return NULL;
		}
        maple_state->index = ++maple_state->last;
		maple_state->node = MAS_START;
	}

	if (mas_is_start(maple_state)) {
		void *mt_entry;
		if (maple_state->index > max_idx)
			return NULL;
		mt_entry = mas_walk(maple_state);
		if (mt_entry)
			return mt_entry;
	}

	if (!mas_searchable(maple_state))
		return NULL;
	return mas_next_entry(maple_state, max_idx);
}

void *mt_find(struct maple_tree *m_tree, unsigned long *idx, unsigned long max_idx)
{
	MA_STATE(maple_state, m_tree, *idx, *idx);
	void *mt_entry;

	if ((*idx) > max_idx)
		return NULL;

retry:
	mt_entry = mas_state_walk(&maple_state);
	if (mas_is_start(&maple_state))
		goto retry;

	if (xa_is_zero(mt_entry))
        mt_entry = NULL;

	if (mt_entry)
		goto unlock;

	while (mas_searchable(&maple_state) && (maple_state.index < max_idx)) {
		mt_entry = mas_next_entry(&maple_state, max_idx);
		if (mt_entry && !xa_is_zero(mt_entry))
			break;
	}

	if (xa_is_zero(mt_entry))
		mt_entry = NULL;
unlock:
	rcu_read_unlock();
	if (mt_entry) {
		*idx = maple_state.last + 1;
	}

	return mt_entry;
}

void *mt_find_after(struct maple_tree *m_tree, unsigned long *idx,
		    unsigned long max_idx)
{
	if (!(*idx))
		return NULL;

	return mt_find(m_tree, idx, max_idx);
}
