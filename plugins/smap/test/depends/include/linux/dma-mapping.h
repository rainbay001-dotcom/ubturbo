/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_DMA_MAPPING_H
#define _LINUX_DMA_MAPPING_H

#define DMA_BIT_MASK(n)	(((n) == 64) ? ~0ULL : ((1ULL<<(n))-1))

static inline void *dma_alloc_coherent(struct device *dev, size_t size,
				       dma_addr_t *dma_handle, gfp_t gfp)
{
}

static inline void dma_free_coherent(struct device *dev, size_t size,
				     void *cpu_addr, dma_addr_t dma_handle)
{
}

static inline int dma_set_mask_and_coherent(struct device *dev, u64 mask)
{
	return 0;
}

#endif /* _LINUX_DMA_MAPPING_H */
