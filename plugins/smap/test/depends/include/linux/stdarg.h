// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef _LINUX_STDARG_H
#define _LINUX_STDARG_H

typedef __builtin_va_list va_list;
#define va_start(x, l) __builtin_va_start(x, l)
#define va_end(x) __builtin_va_end(x)
#define va_arg(x, T) __builtin_va_arg(x, T)
#define va_copy(dest, src) __builtin_va_copy(dest, src)

#endif
