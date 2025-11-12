/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _VMALLOC_H
#define _VMALLOC_H

#ifdef __cplusplus
extern "C" {
#endif

void *vmalloc(unsigned long size);

void *vzalloc(unsigned long size);

void vfree(void *ptr);

#ifdef __cplusplus
}
#endif

#endif