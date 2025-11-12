/* SPDX-License-Identifier: GPL-2.0-only */

#include <linux/time.h>

struct timespec64 ns_to_timespec64(s64 nsec)
{
	struct timespec64 ts = { 0, 0 };

	return ts;
}

void time64_to_tm(time64_t totalsecs, int offset, struct tm *result)
{
}
