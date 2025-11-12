/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_LIST_DEPENDS_H
#define _LINUX_LIST_DEPENDS_H

#include <linux/container_of.h>
#include <linux/types.h>
#include <linux/spinlock_types.h>
#include <linux/ktime.h>
#include <linux/stddef.h>
#include <linux/workqueue.h>
#include <linux/const.h>
#include <linux/time64.h>
#include <linux/poison.h>

#define LIST_HEAD_INIT(list_name) { &(list_name), &(list_name) }

#define LIST_HEAD(list_name) struct list_head list_name = LIST_HEAD_INIT(list_name)

static inline void INIT_LIST_HEAD(struct list_head *list)
{
	list->next = list;
	list->prev = list;
}

#ifdef CONFIG_DEBUG_LIST
extern bool __list_add_valid(struct list_head *new2, struct list_head *prev, struct list_head *next);
extern bool __list_del_entry_valid(struct list_head *entry);
#else
static inline bool __list_add_valid(struct list_head *new2, struct list_head *prev, struct list_head *next)
{
	return true;
}
static inline bool __list_del_entry_valid(struct list_head *entry)
{
	return true;
}
#endif

static inline void __list_add(struct list_head *new2,
			      struct list_head *prev,
			      struct list_head *next)
{
	if (!__list_add_valid(new2, prev, next))
		return;

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

static inline void __list_del_clearprev(struct list_head *list_entry)
{
	__list_del(list_entry->prev, list_entry->next);
	list_entry->prev = (struct list_head*)0;
}

static inline void __list_del_entry(struct list_head *list_entry)
{
	if (!__list_del_entry_valid(list_entry))
		return;

	__list_del(list_entry->prev, list_entry->next);
}

static inline void list_del(struct list_head *list_entry)
{
	__list_del_entry(list_entry);
	list_entry->next = (struct list_head *)LIST_POISON1;
	list_entry->prev = (struct list_head *)LIST_POISON2;
}

static inline void list_replace(struct list_head *old,
				struct list_head *new2)
{
	new2->next = old->next;
	new2->next->prev = new2;
	new2->prev = old->prev;
	new2->prev->next = new2;
}

static inline void list_replace_init(struct list_head *old,
				     struct list_head *new2)
{
	list_replace(old, new2);
	INIT_LIST_HEAD(old);
}

static inline void list_swap(struct list_head *list_entry1,
			     struct list_head *list_entry2)
{
	struct list_head *pos = list_entry2->prev;

	list_del(list_entry2);
	list_replace(list_entry1, list_entry2);
	if (pos == list_entry1)
		pos = list_entry2;
	list_add(list_entry1, pos);
}

static inline void list_del_init(struct list_head *list_entry)
{
	__list_del_entry(list_entry);
	INIT_LIST_HEAD(list_entry);
}

static inline void list_move(struct list_head *list_name, struct list_head *h)
{
	__list_del_entry(list_name);
	list_add(list_name, h);
}

static inline void list_move_tail(struct list_head *list_name, struct list_head *h)
{
	__list_del_entry(list_name);
	list_add_tail(list_name, h);
}

static inline void list_bulk_move_tail(struct list_head *head_entry,
				       struct list_head *first_entry,
				       struct list_head *last_entry)
{
	first_entry->prev->next = last_entry->next;
	last_entry->next->prev = first_entry->prev;

	head_entry->prev->next = first_entry;
	first_entry->prev = head_entry->prev;

	last_entry->next = head_entry;
	head_entry->prev = last_entry;
}

static inline int list_is_first(const struct list_head *list, const struct list_head *head)
{
	return list->prev == head;
}

static inline int list_is_last(const struct list_head *list, const struct list_head *head)
{
	return list->next == head;
}

static inline int list_empty(const struct list_head *h)
{
	return h->next == h;
}

static inline void list_rotate_left(struct list_head *h)
{
	struct list_head *e;

	if (!list_empty(h)) {
		e = h->next;
		list_move_tail(e, h);
	}
}

static inline int list_is_head(const struct list_head *list, const struct list_head *head)
{
	return list == head;
}

static inline void list_rotate_to_front(struct list_head *list, struct list_head *head)
{
	list_move_tail(head, list);
}

static inline int list_is_singular(const struct list_head *head)
{
	return !list_empty(head) && (head->next == head->prev);
}

static inline void __list_cut_position(struct list_head *list,
		struct list_head *head, struct list_head *list_entry)
{
	struct list_head *new_first = list_entry->next;
	list->next = head->next;
	list->next->prev = list;
	list->prev = list_entry;
	list_entry->next = list;
	head->next = new_first;
	new_first->prev = head;
}

static inline void list_cut_position(struct list_head *list,
		struct list_head *head, struct list_head *list_entry)
{
	if (list_empty(head))
		return;
	if (list_is_singular(head) && !list_is_head(list_entry, head) && (list_entry != head->next))
		return;
	if (list_is_head(list_entry, head))
		INIT_LIST_HEAD(list);
	else
		__list_cut_position(list, head, list_entry);
}

static inline void list_cut_before(struct list_head *list,
				   struct list_head *head,
				   struct list_head *list_entry)
{
	if (head->next == list_entry) {
		INIT_LIST_HEAD(list);
		return;
	}
	list->next = head->next;
	list->next->prev = list;
	list->prev = list_entry->prev;
	list->prev->next = list;
	head->next = list_entry;
	list_entry->prev = head;
}

static inline void __list_splice(const struct list_head *list,
				 struct list_head *prev_entry,
				 struct list_head *next_entry)
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

static inline void list_splice(const struct list_head *list,
				struct list_head *head)
{
	if (!list_empty(list))
		__list_splice(list, head, head->next);
}

static inline void list_splice_tail(struct list_head *list, struct list_head *head)
{
	if (!list_empty(list))
		__list_splice(list, head->prev, head);
}

static inline void list_splice_init(struct list_head *list_name, struct list_head *h)
{
	if (!list_empty(list_name)) {
		__list_splice(list_name, h, h->next);
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

#define list_next_entry_circular(pos, head, member) \
	(list_is_last(&(pos)->member, head) ? \
	list_first_entry(head, typeof(*(pos)), member) : list_next_entry(pos, member))

#define list_prev_entry(pos, member) \
	list_entry((pos)->member.prev, typeof(*(pos)), member)

#define list_prev_entry_circular(pos, head, member) \
	(list_is_first(&(pos)->member, head) ? \
	list_last_entry(head, typeof(*(pos)), member) : list_prev_entry(pos, member))

#define list_for_each(pos, head) \
	for ((pos) = (head)->next; !list_is_head((pos), (head)); (pos) = (pos)->next)

#define list_for_each_rcu(pos, head)		  \
	for ((pos) = rcu_dereference((head)->next); \
	     !list_is_head((pos), (head)); \
	     (pos) = rcu_dereference((pos)->next))

#define list_for_each_continue(pos, head) \
	for ((pos) = (pos)->next; !list_is_head((pos), (head)); (pos) = (pos)->next)

#define list_for_each_prev(pos, head) \
	for ((pos) = (head)->prev; !list_is_head((pos), (head)); (pos) = (pos)->prev)

#define list_for_each_safe(pos, node, head) \
	for ((pos) = (head)->next, (node) = (pos)->next; \
	     !list_is_head(pos, (head)); \
	     (pos) = (node), (node) = (pos)->next)

#define list_for_each_prev_safe(pos, node, head) \
	for ((pos) = (head)->prev, (node) = (pos)->prev; \
	     !list_is_head(pos, (head)); \
	     (pos) = (node), (node) = (pos)->prev)

#define list_entry_is_head(pos, head, member)				\
	(&(pos)->member == (head))

#define list_for_each_entry(pos, head, member)				\
	for ((pos) = list_first_entry(head, typeof(*(pos)), member);	\
	     !list_entry_is_head(pos, head, member);			\
	     (pos) = list_next_entry(pos, member))

#define list_for_each_entry_reverse(pos, head, member)			\
	for ((pos) = list_last_entry(head, typeof(*(pos)), member);		\
	     !list_entry_is_head(pos, head, member); 			\
	     (pos) = list_prev_entry(pos, member))

#define list_prepare_entry(pos, head, member) \
	((pos) ? : list_entry(head, typeof(*(pos)), member))

#define list_for_each_entry_continue(pos, head, member) 		\
	for ((pos) = list_next_entry(pos, member);			\
	     !list_entry_is_head(pos, head, member);			\
	     (pos) = list_next_entry(pos, member))

#define list_for_each_entry_continue_reverse(pos, head, member)		\
	for ((pos) = list_prev_entry(pos, member);			\
	     !list_entry_is_head(pos, head, member);			\
	     (pos) = list_prev_entry(pos, member))

#define list_for_each_entry_from(pos, head, member) 			\
	for (; !list_entry_is_head(pos, head, member);			\
	     (pos) = list_next_entry(pos, member))

#define list_for_each_entry_from_reverse(pos, head, member)		\
	for (; !list_entry_is_head(pos, head, member);			\
	     (pos) = list_prev_entry(pos, member))

#define list_for_each_entry_safe(pos, node, head, member)			\
	for ((pos) = list_first_entry(head, typeof(*(pos)), member),	\
		(node) = list_next_entry(pos, member);			\
	     !list_entry_is_head(pos, head, member); 			\
	     (pos) = (node), (node) = list_next_entry(node, member))

#define list_for_each_entry_safe_continue(pos, node, head, member) 		\
	for ((pos) = list_next_entry(pos, member), 				\
		(node) = list_next_entry(pos, member);				\
	     !list_entry_is_head(pos, head, member);				\
	     (pos) = (node), (node) = list_next_entry(node, member))

#define list_for_each_entry_safe_from(pos, node, head, member) 			\
	for ((node) = list_next_entry(pos, member);					\
	     !list_entry_is_head(pos, head, member);				\
	     (pos) = (node), (node) = list_next_entry(node, member))

#define list_for_each_entry_safe_reverse(pos, node, head, member)		\
	for ((pos) = list_last_entry(head, typeof(*(pos)), member),		\
		(node) = list_prev_entry(pos, member);			\
	     !list_entry_is_head(pos, head, member); 			\
	     (pos) = (node), (node) = list_prev_entry(node, member))

#define list_safe_reset_next(pos, node, member)				\
	(node) = list_next_entry(pos, member)

#define HLIST_HEAD_INIT { .first = NULL }
#define HLIST_HEAD(name) struct hlist_head (name) = {  .first = NULL }
#define INIT_HLIST_HEAD(ptr) ((ptr)->first = NULL)
static inline void INIT_HLIST_NODE(struct hlist_node *h)
{
	h->next = (struct hlist_node*)0;
	h->pprev = (struct hlist_node**)0;
}

static inline int hlist_empty(const struct hlist_head *h)
{
	return !h->first;
}

static inline int hlist_unhashed_lockless(const struct hlist_node *h)
{
	return !h->pprev;
}

static inline int hlist_unhashed(const struct hlist_node *h)
{
	return !h->pprev;
}

static inline void __hlist_del(struct hlist_node *node)
{
	struct hlist_node *next = node->next;
	struct hlist_node **pprev = node->pprev;

	*pprev = next;
	if (next)
		next->pprev = pprev;
}

static inline void hlist_del(struct hlist_node *node)
{
	__hlist_del(node);
	node->next = (struct hlist_node *)LIST_POISON1;
	node->pprev = (struct hlist_node **)LIST_POISON2;
}

static inline void hlist_add_head(struct hlist_node *node, struct hlist_head *h)
{
	struct hlist_node *first = h->first;
	node->next = first;
	if (first)
		first->pprev = &node->next;
	h->first = node;
	node->pprev = &h->first;
}

static inline void hlist_del_init(struct hlist_node *node)
{
	if (!hlist_unhashed(node)) {
		__hlist_del(node);
		INIT_HLIST_NODE(node);
	}
}

static inline void hlist_add_behind(struct hlist_node *node, struct hlist_node *prev)
{
	node->next = prev->next;
	prev->next = node;
	node->pprev = &prev->next;

	if (node->next)
		node->next->pprev = &node->next;
}

static inline void hlist_add_before(struct hlist_node *node, struct hlist_node *next)
{
	node->pprev = next->pprev;
	node->next = next;
	next->pprev = &node->next;
	*(node->pprev) = node;
}

static inline void hlist_move_list(struct hlist_head *old, struct hlist_head *new2)
{
	new2->first = old->first;
	if (new2->first)
		new2->first->pprev = &new2->first;
	old->first = (struct hlist_node*)0;
}

static inline bool hlist_is_singular_node(struct hlist_node *node, struct hlist_head *h)
{
	return !node->next && node->pprev == &h->first;
}

static inline bool hlist_fake(struct hlist_node *h)
{
	return h->pprev == &h->next;
}

static inline void hlist_add_fake(struct hlist_node *node)
{
	node->pprev = &node->next;
}

#define hlist_entry(p, type, member) container_of(p, type, member)

#define hlist_for_each(position, head) \
	for ((position) = (head)->first; (position) ; (position) = (position)->next)

#define hlist_for_each_safe(position, node, head) \
	for ((position) = (head)->first; (position) && ({ (node) = (position)->next; 1; }); \
	     (position) = (node))

#define hlist_entry_safe(p, type, member) \
	({ typeof(p) ____ptr = (p); \
	   ____ptr ? hlist_entry(____ptr, type, member) : NULL; \
	})

#define hlist_for_each_entry(position, head, member)				\
	for ((position) = hlist_entry_safe((head)->first, typeof(*(position)), member); \
	     (position);							\
	     (position) = hlist_entry_safe((position)->member.next, typeof(*(position)), member))

#define hlist_for_each_entry_continue(position, member)			\
	for ((position) = hlist_entry_safe((position)->member.next, typeof(*(position)), member); \
	     (position);							\
	     (position) = hlist_entry_safe((position)->member.next, typeof(*(position)), member))


#define hlist_for_each_entry_from(position, member)				\
	for (; position; (position) = hlist_entry_safe((position)->member.next, typeof(*(position)), member))

#define hlist_for_each_entry_safe(position, node, list_head, member) 		\
	for ((position) = hlist_entry_safe((list_head)->first, typeof(*(position)), member); \
	     (position) && ({ (node) = (position)->member.next; 1; });			\
	     (position) = hlist_entry_safe((node), typeof(*(position)), member))

#endif
