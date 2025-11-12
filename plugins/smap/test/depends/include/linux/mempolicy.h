/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_MEMPOLICY_DEPENDS_H
#define _LINUX_MEMPOLICY_DEPENDS_H

#include <uapi/linux/mempolicy.h>
#include <linux/nodemask.h>
#include <linux/mmzone.h>
struct mempolicy {
        union {
                nodemask_t user_nodemask;
                nodemask_t cpuset_mems_allowed;
        } w;
        int home_node;
        nodemask_t nodes;
        /* Depends mempolicy */
        unsigned short flags;
        unsigned short mode;
        atomic_t refcnt;
};

#endif
