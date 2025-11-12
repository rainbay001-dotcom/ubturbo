/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_DELAY_H
#define _LINUX_DELAY_H

#include <linux/jiffies.h>

#define USEC_PER_MSEC     1000L
void msleep(unsigned int msecs);
void usleep_range(unsigned long min, unsigned long max);

#endif /* defined(_LINUX_DELAY_H) */
