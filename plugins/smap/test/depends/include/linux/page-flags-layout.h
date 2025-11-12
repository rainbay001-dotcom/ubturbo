/* SPDX-License-Identifier: GPL-2.0 */
#ifndef PAGE_FLAGS_LAYOUT_H
#define PAGE_FLAGS_LAYOUT_H

#if defined(CONFIG_SPARSEMEM) && !defined(CONFIG_SPARSEMEM_VMEMMAP)
#define SECTIONS_WIDTH		SECTIONS_SHIFT  /* SECTIONS_WIDTH = SECTIONS_SHIFT */
#else
#define SECTIONS_WIDTH		0
#endif

#define ZONES_SHIFT 2
#define ZONES_WIDTH		ZONES_SHIFT

#if SECTIONS_WIDTH + ZONES_WIDTH + NODES_SHIFT <= BITS_PER_LONG - NR_PAGEFLAGS
#define NODES_WIDTH		NODES_SHIFT
#else
#ifdef CONFIG_SPARSEMEM_VMEMMAP
#error "[Depends] Vmemmap: No space for nodes field in page flags"
#endif
#define NODES_WIDTH		0
#endif

#endif
