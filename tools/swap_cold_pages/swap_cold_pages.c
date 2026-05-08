// SPDX-License-Identifier: GPL-2.0-only
/*
 * swap_cold_pages.c — user-space tool for cold 2 MiB page eviction
 *
 * Reads access-frequency data from freq.txt, locates the QEMU guest's
 * huge-page regions via /proc/<pid>/numa_maps, selects the coldest pages
 * by ratio, and periodically asks the swap_cold_driver kernel module to
 * reclaim them via ioctl(SWAP_IOC_PAGEOUT).
 *
 * Build:
 *   gcc -O2 -Wall -Wextra -I. -o swap_cold_pages swap_cold_pages.c
 *
 * Usage:
 *   ./swap_cold_pages -p <pid> -f <freq.txt> -r <ratio> [-i <sec>] [-v]
 *
 * Requires:
 *   - swap_cold_driver.ko loaded (creates /dev/swap_cold_pages)
 *   - root or CAP_SYS_ADMIN
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <getopt.h>

#include "uapi/swap_cold.h"

#define HUGE_PAGE_SIZE   (2UL * 1024 * 1024)
#define NUMA_MAPS_LINE   4096
#define MAX_REGIONS      64

static volatile int g_running = 1;

static void sig_handler(int sig)
{
	(void)sig;
	g_running = 0;
}

/* ---- numa_maps parsing -------------------------------------------------- */

struct region {
	uint64_t base;  /* first 2 MiB page VA */
	uint64_t count; /* number of 2 MiB pages */
};

/*
 * Scan /proc/<pid>/numa_maps for lines that contain both "huge" and
 * "file=/dev/hugepages", which identifies QEMU's static 2 MiB backing.
 * Regions are appended in the order they appear (ascending VA order as
 * the kernel always emits numa_maps entries that way).
 */
static int parse_numa_maps(pid_t pid, struct region *regions, int max_regions)
{
	char path[64];
	FILE *fp;
	char line[NUMA_MAPS_LINE];
	int nr = 0;

	snprintf(path, sizeof(path), "/proc/%d/numa_maps", (int)pid);
	fp = fopen(path, "r");
	if (!fp) {
		fprintf(stderr, "[error] fopen %s: %s\n", path, strerror(errno));
		return -1;
	}

	while (fgets(line, sizeof(line), fp)) {
		if (!strstr(line, "huge") || !strstr(line, "/dev/hugepages"))
			continue;

		if (nr >= max_regions) {
			fprintf(stderr,
				"[warn] more than %d hugepage regions; truncating\n",
				max_regions);
			break;
		}

		uint64_t base = strtoull(line, NULL, 16);

		char *anon = strstr(line, "anon=");
		if (!anon)
			continue;
		uint64_t count = strtoull(anon + 5, NULL, 10);
		if (count == 0)
			continue;

		regions[nr].base  = base;
		regions[nr].count = count;
		nr++;
	}

	fclose(fp);
	return nr;
}

/* ---- freq.txt parsing --------------------------------------------------- */

static uint64_t *parse_freq(const char *path, size_t *out_count)
{
	FILE *fp = fopen(path, "r");
	if (!fp) {
		fprintf(stderr, "[error] fopen %s: %s\n", path, strerror(errno));
		return NULL;
	}

	size_t cap = 4096, cnt = 0;
	uint64_t *arr = malloc(cap * sizeof(uint64_t));
	if (!arr) {
		fclose(fp);
		return NULL;
	}

	uint64_t v;
	while (fscanf(fp, "%" SCNu64, &v) == 1) {
		if (cnt >= cap) {
			cap *= 2;
			uint64_t *tmp = realloc(arr, cap * sizeof(uint64_t));
			if (!tmp) {
				free(arr);
				fclose(fp);
				return NULL;
			}
			arr = tmp;
		}
		arr[cnt++] = v;
	}

	fclose(fp);
	*out_count = cnt;
	return arr;
}

/* ---- cold-page selection ------------------------------------------------ */

struct page_entry {
	uint64_t va;
	uint64_t freq;
};

static int cmp_freq_asc(const void *a, const void *b)
{
	const struct page_entry *pa = a, *pb = b;
	if (pa->freq < pb->freq) return -1;
	if (pa->freq > pb->freq) return  1;
	return 0;
}

/* ---- helpers ------------------------------------------------------------ */

static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage: %s -p <pid> -f <freq.txt> -r <ratio> [-i <sec>] [-v]\n"
		"\n"
		"  -p <pid>      PID of the target QEMU process\n"
		"  -f <path>     path to freq.txt (space-separated integers)\n"
		"  -r <ratio>    fraction of coldest pages to evict, e.g. 0.3\n"
		"  -i <seconds>  cycle interval (default: 60)\n"
		"  -v            verbose: print each cold page VA and frequency\n"
		"  -h            show this help\n"
		"\n"
		"Requires: swap_cold_driver.ko loaded, root privileges\n",
		prog);
}

/* ---- main --------------------------------------------------------------- */

int main(int argc, char *argv[])
{
	pid_t   pid      = 0;
	char   *freq_path = NULL;
	double  ratio    = 0.0;
	int     interval = 60;
	int     verbose  = 0;
	int     opt;

	while ((opt = getopt(argc, argv, "p:f:r:i:vh")) != -1) {
		switch (opt) {
		case 'p': pid       = (pid_t)atoi(optarg); break;
		case 'f': freq_path = optarg;               break;
		case 'r': ratio     = atof(optarg);         break;
		case 'i': interval  = atoi(optarg);         break;
		case 'v': verbose   = 1;                    break;
		case 'h': usage(argv[0]); return 0;
		default:  usage(argv[0]); return 1;
		}
	}

	if (!pid || !freq_path || ratio <= 0.0 || ratio > 1.0) {
		usage(argv[0]);
		return 1;
	}

	if (geteuid() != 0)
		fprintf(stderr,
			"[warn] not running as root; ioctl likely to fail\n");

	/* --- parse numa_maps ------------------------------------------------ */
	struct region regions[MAX_REGIONS];
	int nr_regions = parse_numa_maps(pid, regions, MAX_REGIONS);
	if (nr_regions <= 0) {
		fprintf(stderr,
			"[error] no 2 MiB hugepage regions found for PID %d\n",
			pid);
		return 1;
	}

	uint64_t total_pages = 0;
	for (int i = 0; i < nr_regions; i++)
		total_pages += regions[i].count;

	printf("[init] %d region(s), %" PRIu64
	       " × 2MiB pages = %.1f GiB total\n",
	       nr_regions, total_pages,
	       (double)total_pages * HUGE_PAGE_SIZE / (1UL << 30));

	/* --- parse freq.txt ------------------------------------------------- */
	size_t freq_count = 0;
	uint64_t *freq = parse_freq(freq_path, &freq_count);
	if (!freq)
		return 1;

	if (freq_count != (size_t)total_pages) {
		fprintf(stderr,
			"[error] freq.txt has %zu entries, expected %" PRIu64 "\n",
			freq_count, total_pages);
		free(freq);
		return 1;
	}

	/* --- build (va, freq) pairs sorted by frequency ascending ----------- */
	struct page_entry *pages = malloc(total_pages * sizeof(*pages));
	if (!pages) {
		free(freq);
		return 1;
	}

	/* VAs are ordered globally across regions, matching freq.txt order */
	size_t idx = 0;
	for (int r = 0; r < nr_regions; r++) {
		for (uint64_t p = 0; p < regions[r].count; p++, idx++) {
			pages[idx].va   = regions[r].base + p * HUGE_PAGE_SIZE;
			pages[idx].freq = freq[idx];
		}
	}
	free(freq);

	qsort(pages, (size_t)total_pages, sizeof(*pages), cmp_freq_asc);

	size_t cold_count = (size_t)((double)total_pages * ratio);
	if (cold_count == 0)
		cold_count = 1;

	printf("[init] swap ratio=%.2f → %zu coldest pages selected"
	       " (%.1f GiB)\n",
	       ratio, cold_count,
	       (double)cold_count * HUGE_PAGE_SIZE / (1UL << 30));

	/* --- build flat VA array for ioctl ---------------------------------- */
	uint64_t *cold_vas = malloc(cold_count * sizeof(uint64_t));
	if (!cold_vas) {
		free(pages);
		return 1;
	}
	for (size_t i = 0; i < cold_count; i++)
		cold_vas[i] = pages[i].va;

	if (verbose) {
		for (size_t i = 0; i < cold_count; i++)
			printf("  cold[%zu] va=0x%016" PRIx64
			       " freq=%" PRIu64 "\n",
			       i, pages[i].va, pages[i].freq);
	}
	free(pages);

	/* --- open driver device --------------------------------------------- */
	int dev_fd = open(SWAP_COLD_DEV, O_RDWR);
	if (dev_fd < 0) {
		fprintf(stderr, "[error] open %s: %s\n",
			SWAP_COLD_DEV, strerror(errno));
		fprintf(stderr,
			"        ensure swap_cold_driver.ko is loaded\n");
		free(cold_vas);
		return 1;
	}

	signal(SIGINT,  sig_handler);
	signal(SIGTERM, sig_handler);

	printf("[loop] starting pageout every %d second(s);"
	       " Ctrl+C to stop\n", interval);

	int cycle = 0;
	while (g_running) {
		cycle++;
		int errors = 0;

		/* Send in chunks ≤ SWAP_MAX_PAGES per ioctl */
		for (size_t off = 0; off < cold_count && g_running; ) {
			size_t batch = cold_count - off;
			if (batch > SWAP_MAX_PAGES)
				batch = SWAP_MAX_PAGES;

			struct swap_pageout_req req = {
				.pid      = (__u32)pid,
				.nr_pages = (__u32)batch,
				.vas_ptr  = (__u64)(uintptr_t)(cold_vas + off),
			};

			if (ioctl(dev_fd, SWAP_IOC_PAGEOUT, &req) < 0) {
				errors++;
				if (verbose)
					fprintf(stderr,
						"[cycle %d] ioctl at off=%zu: %s\n",
						cycle, off,
						strerror(errno));
			}
			off += batch;
		}

		printf("[cycle %d] done: %.1f MiB requested,"
		       " %d ioctl-error(s)\n",
		       cycle,
		       (double)cold_count * HUGE_PAGE_SIZE / (1UL << 20),
		       errors);

		if (g_running)
			sleep((unsigned)interval);
	}

	printf("[exit] total cycles: %d\n", cycle);
	close(dev_fd);
	free(cold_vas);
	return 0;
}
