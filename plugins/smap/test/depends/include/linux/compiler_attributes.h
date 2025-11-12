/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_COMPILER_ATTRIBUTES_H
#define __LINUX_COMPILER_ATTRIBUTES_H

#define __maybe_unused   __attribute__((__unused__))
#define __always_unused  __attribute__((__unused__))
#define __must_check     __attribute__((__warn_unused_result__))

#endif
