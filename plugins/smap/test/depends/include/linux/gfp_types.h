/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_GFP_TYPES_H
#define __LINUX_GFP_TYPES_H

#define GFP_ATOMIC 1
#define GFP_KERNEL 1
#define GFP_KERNEL_ACCOUNT 1
#define GFP_NOWAIT 1
#define GFP_NOIO 1
#define GFP_NOFS 1
#define GFP_USER 1
#define GFP_DMA 1
#define GFP_DMA32 1
#define GFP_HIGHUSER 1
#define GFP_HIGHUSER_MOVABLE 1
#define GFP_TRANSHUGE_LIGHT 1
#define GFP_TRANSHUGE 1
#define ___GFP_MOVABLE		0x08u
#define ___GFP_RETRY_MAYFAIL	0x4000u

#define ___GFP_MOVABLE		0x08u
#define ___GFP_RETRY_MAYFAIL	0x4000u
 
#define __GFP_MOVABLE	((__force gfp_t)___GFP_MOVABLE)  /* ZONE_MOVABLE allowed */
#define __GFP_RETRY_MAYFAIL	((__force gfp_t)___GFP_RETRY_MAYFAIL)

#endif /* __LINUX_GFP_TYPES_H */
