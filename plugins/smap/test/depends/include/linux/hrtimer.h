// SPDX-License-Identifier: GPL-2.0
#ifndef _LINUX_HRTIMER_DEPENDS_H
#define _LINUX_HRTIMER_DEPENDS_H

typedef s64	ktime_t;

enum hrtimer_restart {
	HRTIMER_NORESTART,	/* Timer is not restarted */
	HRTIMER_RESTART,	/* Timer must be restarted */
};

struct hrtimer {
    enum hrtimer_restart		(*function)(struct hrtimer *);
	u8				state;
};

enum hrtimer_mode {
    HRTIMER_MODE_HARD	= 0x08,
    HRTIMER_MODE_SOFT	= 0x04,
    HRTIMER_MODE_PINNED	= 0x02,
    HRTIMER_MODE_REL	= 0x01,
	HRTIMER_MODE_ABS	= 0x00,

    HRTIMER_MODE_REL_SOFT	= HRTIMER_MODE_REL | HRTIMER_MODE_SOFT,
	HRTIMER_MODE_ABS_SOFT	= HRTIMER_MODE_ABS | HRTIMER_MODE_SOFT,

    HRTIMER_MODE_REL_PINNED = HRTIMER_MODE_REL | HRTIMER_MODE_PINNED,
	HRTIMER_MODE_ABS_PINNED = HRTIMER_MODE_ABS | HRTIMER_MODE_PINNED,

    HRTIMER_MODE_REL_HARD	= HRTIMER_MODE_REL | HRTIMER_MODE_HARD,
	HRTIMER_MODE_ABS_HARD	= HRTIMER_MODE_ABS | HRTIMER_MODE_HARD,

    HRTIMER_MODE_REL_PINNED_SOFT = HRTIMER_MODE_REL_PINNED | HRTIMER_MODE_SOFT,
	HRTIMER_MODE_ABS_PINNED_SOFT = HRTIMER_MODE_ABS_PINNED | HRTIMER_MODE_SOFT,

    HRTIMER_MODE_REL_PINNED_HARD = HRTIMER_MODE_REL_PINNED | HRTIMER_MODE_HARD,
	HRTIMER_MODE_ABS_PINNED_HARD = HRTIMER_MODE_ABS_PINNED | HRTIMER_MODE_HARD,
};

#ifdef __cplusplus
extern "C" {
#endif
static inline void hrtimer_start(struct hrtimer *timer, ktime_t tim,
				 const enum hrtimer_mode mode)
{
	return;
}

static inline u64 hrtimer_forward_now(struct hrtimer *timer,
				      ktime_t interval)
{
	return 0;
}

static inline int hrtimer_cancel(struct hrtimer *timer)
{
    return 0;
}

static inline void hrtimer_init(struct hrtimer *timer, clockid_t which_clock, enum hrtimer_mode mode)
{
	return;
}

static inline ktime_t ms_to_ktime(u64 ms)
{
	return 0;
}

#ifdef __cplusplus
}
#endif

#endif