/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_GENERIC_BITOPS_DEPENDS_BUILTIN___FFS_H_
#define _ASM_GENERIC_BITOPS_DEPENDS_BUILTIN___FFS_H_

static __always_inline unsigned long __ffs(unsigned long w)
{
	return __builtin_ctzl(w);
}

#endif
