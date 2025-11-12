/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef __ASM_GENERIC_ATOMIC_H
#define __ASM_GENERIC_ATOMIC_H

#ifndef atomic_read
#define atomic_read(v) (v)
#endif
#define atomic_set(v, i) (v)

#endif /* __ASM_GENERIC_ATOMIC_H */
