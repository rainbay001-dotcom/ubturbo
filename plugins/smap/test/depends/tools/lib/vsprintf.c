/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/types.h>
#include <linux/kernel.h>
#include <stdio.h>

int scnprintf(char * buf, size_t size, const char * fmt, ...)
{
       ssize_t ssize = size;
       va_list args;
       int i;

       va_start(args, fmt);
       i = vsnprintf(buf, size, fmt, args);
       va_end(args);

       return (i >= ssize) ? (ssize - 1) : i;
}
