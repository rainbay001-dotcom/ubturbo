/* SPDX-License-Identifier: MIT */
/*
 * memdemo - allocate a region of 4K/2M pages, drive an access pattern over it
 * (optionally behind a KVM memslot so SMAP treats us as a guest), and dump a
 * binary hot/cold ground-truth bitmap for scan-accuracy evaluation.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "memdemo.h"

static const char *pattern_name(enum access_pattern p)
{
	switch (p) {
	case PAT_UNIFORM:
		return "uniform";
	case PAT_RANDOM:
		return "random";
	case PAT_GAUSSIAN:
		return "gaussian";
	case PAT_ZIPF:
		return "zipf";
	}
	return "?";
}

int main(int argc, char **argv)
{
	struct memdemo_config cfg;
	if (parse_args(argc, argv, &cfg) != 0) {
		print_usage(argv[0]);
		return 2;
	}

	struct mem_region region;
	if (mem_alloc(&cfg, &region) != 0)
		return 1;

	fprintf(stderr,
		"memdemo: pid=%d page=%s mode=%s pattern=%s size=%zu MiB pages=%llu\n",
		(int)getpid(), cfg.page_kind == PAGE_2M ? "2M" : "4K",
		cfg.mode == MODE_VM ? "vm" : "process", pattern_name(cfg.pattern),
		region.length >> 20, (unsigned long long)region.npages);

	struct vm_context vm = { .kvm_fd = -1, .vm_fd = -1 };
	if (cfg.mode == MODE_VM) {
		if (vm_register(&cfg, &region, &vm) != 0) {
			fprintf(stderr, "vm registration failed; aborting\n");
			mem_free(&region);
			return 1;
		}
	}

	uint64_t *counts = calloc(region.npages, sizeof(*counts));
	if (!counts) {
		fprintf(stderr, "out of memory for %llu counters\n",
			(unsigned long long)region.npages);
		vm_release(&vm);
		mem_free(&region);
		return 1;
	}

	int rc = run_access_pattern(&cfg, &region, counts);
	if (rc == 0)
		rc = write_ground_truth(&cfg, &region, counts);

	free(counts);
	vm_release(&vm);
	mem_free(&region);
	return rc == 0 ? 0 : 1;
}
