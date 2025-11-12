/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef TEST_UACCESS_H
#define TEST_UACCESS_H

#define put_user(x, ptr)	(0)
#define get_user(x, ptr)	(0)

#ifdef __cplusplus
extern "C" {
#endif
unsigned long copy_from_user(void *to, const void *from, unsigned long n);
unsigned long copy_to_user(void *to, const void *from, unsigned long n);
#ifdef __cplusplus
}
#endif

#endif // TEST_UACCESS_H