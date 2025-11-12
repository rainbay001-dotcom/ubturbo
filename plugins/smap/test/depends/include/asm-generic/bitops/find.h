/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_GENERIC_BITOPS_FIND_H_
#define _ASM_GENERIC_BITOPS_FIND_H_

#ifndef find_next_bit
extern unsigned long find_next_bit(const unsigned long *addr, unsigned long size, unsigned long offset);
#endif

#ifndef find_first_bit
#define find_first_bit(addr, size) find_next_bit((addr), (size), 0)
#endif

#endif
