/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_HASHTABLE_H
#define __LINUX_HASHTABLE_H

#include <linux/list.h>
#include <linux/container_of.h>

#define hlist_entry(ptr, type, member) container_of(ptr, type, member)

#define hlist_entry_safe(ptr, type, member) \
    ({ typeof(ptr) ____ptr = (ptr); \
       ____ptr ? hlist_entry(____ptr, type, member) : NULL; \
    })

#define hlist_for_each_entry_safe(pos, node, head, member)            \
    for (pos = hlist_entry_safe((head)->first, typeof(*pos), member);    \
         pos && ({ node = pos->member.next; 1; });            \
         pos = hlist_entry_safe(node, typeof(*pos), member))

#define hlist_for_each_entry(pos, head, member)                \
    for (pos = hlist_entry_safe((head)->first, typeof(*(pos)), member);    \
         pos;            \
         pos = hlist_entry_safe((pos)->member.next, typeof(*(pos)), member))

#define GOLDEN_RATIO_32 0x61C88647
#define GOLDEN_RATIO_64 0x61C8864680B583EBull

#define hash_for_each_safe(name, bkt, tmp, obj, member)    \
    for ((bkt) = 0, obj = NULL; obj == NULL && (bkt) < HASH_SIZE(name); \
            (bkt)++) \
        hlist_for_each_entry_safe(obj, tmp, &name[bkt], member)

#define __hash_32 __hash_32_generic
#define hash_64 hash_64_generic

static __always_inline u32 hash_64_generic(u64 val, unsigned int bits)
{
#if BITS_PER_LONG == 64
    return val * GOLDEN_RATIO_64 >> (64 - bits);
#else
    return hash_32((u32)val ^ __hash_32(val >> 32), bits);
#endif
}

#define hash_long(val, bits) hash_64(val, bits)

static inline u32 __hash_32_generic(u32 val)
{
    return val * GOLDEN_RATIO_32;
}

static inline u32 hash_32(u32 val, unsigned int bits)
{
    return __hash_32(val) >> (32 - bits); // 32
}

#define hash_min(val, bits)    \
    (sizeof(val) <= 4 ? hash_32(val, bits) : hash_long(val, bits))

#define __same_type(a, b) __builtin_types_compatible_p(typeof(a), typeof(b))

#define BUILD_BUG_ON_ZERO(e) ((int)(sizeof(struct { int:(-!!(e)); })))

#define __must_be_array(a) BUILD_BUG_ON_ZERO(__same_type((a), &(a)[0]))

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]) + __must_be_array(arr))

#define DECLARE_HASHTABLE(name, bits)    \
    struct hlist_head name[1 << (bits)]
#define HASH_SIZE(name) (ARRAY_SIZE(name))

#define hash_for_each_possible(name, obj, member, key)    \
    hlist_for_each_entry(obj, &name[hash_min(key, 9)], member)

#define hash_for_each_possible_safe(name, obj, tmp, member, key)    \
    hlist_for_each_entry_safe(obj, tmp, \
        &name[hash_min(key, 9)], member)

#define INIT_HLIST_HEAD(ptr) ((ptr)->first = NULL)

static inline void __hash_init(struct hlist_head *ht, unsigned int sz)
{
    unsigned int i;
    for (i = 0; i < sz; i++)
        INIT_HLIST_HEAD(&ht[i]);
}

#define hash_init(hashtable) __hash_init(hashtable, 9)

#define hash_add(hashtable, node, key)    \
    hlist_add_head(node, &hashtable[hash_min(key, 9)])
#endif