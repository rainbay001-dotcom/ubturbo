/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_LIST_DEPENDS_H
#define _LINUX_LIST_DEPENDS_H

#include <linux/container_of.h>
#include <linux/types.h>
#include <linux/spinlock_types.h>
#include <linux/stddef.h>
#include <linux/const.h>
#include <linux/poison.h>

#define LIST_HEAD_INIT(list_name) { &(list_name), &(list_name) }

#define LIST_HEAD(list_name) struct list_head list_name = LIST_HEAD_INIT(list_name)

static inline void INIT_LIST_HEAD(struct list_head *list)
{
    list->next = list;
    list->prev = list;
}

static inline bool __list_add_valid(struct list_head *new2, struct list_head *prev, struct list_head *next)
{
    return true;
}
static inline bool __list_del_entry_valid(struct list_head *entry)
{
    return true;
}

static inline void __list_add(struct list_head *new2, struct list_head *prev, struct list_head *next)
{
    if (!__list_add_valid(new2, prev, next)) {
        return;
    }

    next->prev = new2;
    new2->next = next;
    new2->prev = prev;
    prev->next = new2;
}

static inline void list_add(struct list_head *new2, struct list_head *head)
{
    __list_add(new2, head, head->next);
}

static inline void list_add_tail(struct list_head *new2, struct list_head *head)
{
    __list_add(new2, head->prev, head);
}

static inline void __list_del(struct list_head * prev, struct list_head * next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void __list_del_entry(struct list_head *list_entry)
{
    if (!__list_del_entry_valid(list_entry)) {
        return;
    }

    __list_del(list_entry->prev, list_entry->next);
}

static inline void list_del(struct list_head *list_entry)
{
    __list_del_entry(list_entry);
    list_entry->next = (struct list_head *)LIST_POISON1;
    list_entry->prev = (struct list_head *)LIST_POISON2;
}

static inline void list_move_tail(struct list_head *list_name, struct list_head *h)
{
    __list_del_entry(list_name);
    list_add_tail(list_name, h);
}

static inline int list_empty(const struct list_head *h)
{
    return h->next == h;
}

static inline void __list_splice(const struct list_head *list,
    struct list_head *prev_entry, struct list_head *next_entry)
{
    struct list_head *first_entry = list->next;
    struct list_head *last_entry = list->prev;

    first_entry->prev = prev_entry;
    prev_entry->next = first_entry;

    last_entry->next = next_entry;
    next_entry->prev = last_entry;
}

static inline void list_splice_tail_init(struct list_head *list_name, struct list_head *h)
{
    if (!list_empty(list_name)) {
        __list_splice(list_name, h->prev, h);
        INIT_LIST_HEAD(list_name);
    }
}

#define list_entry(pointer, t, m) container_of(pointer, t, m)

#define list_first_entry(pointer, t, m) list_entry((pointer)->next, t, m)

#define list_last_entry(pointer, t, m) list_entry((pointer)->prev, t, m)

#define list_first_entry_or_null(ptr, type, member) ({ \
    struct list_head *head__ = (ptr); \
    struct list_head *pos__ = head__->next; \
    pos__ != head__ ? list_entry(pos__, type, member) : NULL; \
})

#define list_next_entry(pos, member) \
    list_entry((pos)->member.next, typeof(*(pos)), member)

#define list_entry_is_head(pos, head, member)                \
    (&(pos)->member == (head))

#define list_for_each_entry(pos, head, member)                \
    for ((pos) = list_first_entry(head, typeof(*(pos)), member);    \
         !list_entry_is_head(pos, head, member);            \
         (pos) = list_next_entry(pos, member))

#define list_for_each_entry_safe(pos, node, head, member)            \
    for ((pos) = list_first_entry(head, typeof(*(pos)), member),    \
        (node) = list_next_entry(pos, member);            \
         !list_entry_is_head(pos, head, member);             \
         (pos) = (node), (node) = list_next_entry(node, member))

static inline void hlist_add_head(struct hlist_node *n, struct hlist_head *h)
{
    struct hlist_node *first = h->first;
    n->next = first;
    if (first) {
        first->pprev = &n->next;
    }
    h->first = n;
    n->pprev = &h->first;
}

static inline int hlist_unhashed(const struct hlist_node *h)
{
    return !h->pprev;
}

static inline void __hlist_del(struct hlist_node *n)
{
    struct hlist_node *next = n->next;
    struct hlist_node **pprev = n->pprev;
    *pprev = next;
    if (next) {
        next->pprev = pprev;
    }
}

static inline void INIT_HLIST_NODE(struct hlist_node *h)
{
    h->next = NULL;
    h->pprev = NULL;
}

static inline void hlist_del_init(struct hlist_node *n)
{
    if (!hlist_unhashed(n)) {
        __hlist_del(n);
        INIT_HLIST_NODE(n);
    }
}

static inline void hash_del(struct hlist_node *node)
{
    hlist_del_init(node);
}
#endif
