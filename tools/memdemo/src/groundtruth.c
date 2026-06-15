/* SPDX-License-Identifier: MIT */
/*
 * Classify each page hot/cold from its access count and emit a binary bitmap.
 *
 * File layout (little-endian), then ceil(npages/8) bytes of packed bits where
 * bit i (LSB-first within each byte) is 1 when page i is hot:
 *
 *   off  size  field
 *     0     8  magic "MEMDEMOB"
 *     8     4  version (=1)
 *    12     4  page_size (bytes)
 *    16     8  page_count (== number of bits)
 *    24     8  gpa_base (0 in process mode)
 *    32     4  pattern enum
 *    36     4  mode enum
 *    40     8  hot_threshold actually used
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "memdemo.h"

#define MEMDEMO_MAGIC "MEMDEMOB"
#define MEMDEMO_VERSION 1u

struct bitmap_header {
	char magic[8];
	uint32_t version;
	uint32_t page_size;
	uint64_t page_count;
	uint64_t gpa_base;
	uint32_t pattern;
	uint32_t mode;
	uint64_t hot_threshold;
};

int write_ground_truth(const struct memdemo_config *cfg,
		       const struct mem_region *region,
		       const uint64_t *counts)
{
	uint64_t npages = region->npages;

	/* Resolve the hot threshold: explicit value, else the mean count. */
	uint64_t threshold;
	if (cfg->hot_threshold >= 0) {
		threshold = (uint64_t)cfg->hot_threshold;
	} else {
		uint64_t total = 0;
		for (uint64_t i = 0; i < npages; i++)
			total += counts[i];
		threshold = npages ? total / npages : 0;
		if (threshold == 0)
			threshold = 1; /* any access counts as hot */
	}

	size_t nbytes = (size_t)((npages + 7) / 8);
	unsigned char *bits = calloc(nbytes ? nbytes : 1, 1);
	if (!bits) {
		fprintf(stderr, "out of memory for bitmap\n");
		return -1;
	}

	uint64_t hot = 0;
	for (uint64_t i = 0; i < npages; i++) {
		if (counts[i] >= threshold) {
			bits[i >> 3] |= (unsigned char)(1u << (i & 7));
			hot++;
		}
	}

	struct bitmap_header h;
	memset(&h, 0, sizeof(h));
	memcpy(h.magic, MEMDEMO_MAGIC, 8);
	h.version = MEMDEMO_VERSION;
	h.page_size = (uint32_t)region->page_bytes;
	h.page_count = npages;
	h.gpa_base = cfg->mode == MODE_VM ? cfg->gpa_base : 0;
	h.pattern = (uint32_t)cfg->pattern;
	h.mode = (uint32_t)cfg->mode;
	h.hot_threshold = threshold;

	FILE *f = fopen(cfg->out_path, "wb");
	if (!f) {
		perror("fopen(out)");
		free(bits);
		return -1;
	}
	int ok = fwrite(&h, sizeof(h), 1, f) == 1 &&
		 (nbytes == 0 || fwrite(bits, 1, nbytes, f) == nbytes);
	fclose(f);
	free(bits);

	if (!ok) {
		fprintf(stderr, "short write to %s\n", cfg->out_path);
		return -1;
	}

	fprintf(stderr,
		"groundtruth: %llu/%llu pages hot (threshold>=%llu) -> %s\n",
		(unsigned long long)hot, (unsigned long long)npages,
		(unsigned long long)threshold, cfg->out_path);
	return 0;
}
