/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _LINUX_TIME_H
#define _LINUX_TIME_H

#include <linux/types.h>
#include <linux/time64.h>

struct tm {
    int tm_mon;
    int tm_mday;
    int tm_hour;
    int tm_min;
	int tm_sec;
	long tm_year;
	int tm_wday;
	int tm_yday;
};

struct timespec64 ns_to_timespec64(s64 nsec);

void time64_to_tm(time64_t totalsecs, int offset, struct tm *result);

#define ktime_to_timespec64(kt)		ns_to_timespec64((kt))


#endif
