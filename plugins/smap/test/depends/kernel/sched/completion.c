/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/completion.h>

void complete(struct completion *x)
{
}

void wait_for_completion(struct completion *x)
{
}

int wait_for_completion_killable(struct completion *x)
{
    return 0;
}