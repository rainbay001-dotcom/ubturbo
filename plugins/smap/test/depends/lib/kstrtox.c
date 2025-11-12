/* SPDX-License-Identifier: GPL-2.0-only */
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <linux/kstrtox.h>

int kstrtoint(const char *str, unsigned int base, int *result)
{
    long tmp;
    tmp = strtol(str, NULL, base);
    if (errno)
        return -errno;
    if (tmp != (unsigned int)tmp)
        return -ERANGE;
    *result = tmp;
    return 0;
}

int kstrtouint(const char *str, unsigned int base, unsigned int *result)
{
	unsigned long long tmp;
	tmp = strtoull(str, NULL, base);
	if (errno)
		return -errno;
	if (tmp != (unsigned int)tmp)
		return -ERANGE;
	*result = tmp;
	return 0;
}

int kstrtoull(const char *s, unsigned int base, unsigned long long *res)
{
    unsigned long long tmp;

    errno = 0;
    tmp = strtoull(s, NULL, base);
    if (errno) {
        return -errno;
    }
    if (tmp != (unsigned int)tmp) {
        return -ERANGE;
    }
    *res = tmp;
    return 0;
}