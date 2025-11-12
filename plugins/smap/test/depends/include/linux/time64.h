/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_TIME64_H
#define _LINUX_TIME64_H

#include <uapi/asm-generic/int-ll64.h>
#include <uapi/asm-generic/fcntl.h>
#include <linux/timekeeping.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef __s64 time64_t;

struct timespec64 {
    /* seconds */
	time64_t	tv_sec;
    /* nanoseconds */
	long		tv_nsec;
};

struct itimerspec64 {
    // 定时器的重复间隔时间（interval）
	struct timespec64 it_interval;
    // 定时器的首次触发时间（value）
	struct timespec64 it_value;
};

time64_t ktime_get_seconds(void);

#ifdef __cplusplus
}
#endif

#endif