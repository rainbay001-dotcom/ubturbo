/*
 * swap_cold_pages.c — 基于历史访存频次，使用 process_madvise(MADV_PAGEOUT)
 * 周期性换出目标 QEMU 进程中访问最冷的 2MB 大页。
 *
 * 编译：gcc -O2 -Wall -o swap_cold_pages swap_cold_pages.c
 * 依赖：Linux 5.10+，CAP_SYS_PTRACE 或 root 权限
 *
 * 用法：swap_cold_pages -p <pid> -f <freq.txt> -r <ratio> [-i <seconds>] [-v]
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <getopt.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <sys/types.h>

/* ── syscall numbers (arm64 / x86_64 both use 434/440 for new syscalls) ── */
#ifndef __NR_pidfd_open
# if defined(__aarch64__) || defined(__x86_64__)
#  define __NR_pidfd_open 434
# else
#  error "Unsupported architecture: define __NR_pidfd_open manually"
# endif
#endif

#ifndef __NR_process_madvise
# if defined(__aarch64__) || defined(__x86_64__)
#  define __NR_process_madvise 440
# else
#  error "Unsupported architecture: define __NR_process_madvise manually"
# endif
#endif

#ifndef MADV_PAGEOUT
# define MADV_PAGEOUT 21
#endif

/* ── constants ── */
#define HUGEPAGE_SIZE   (2ULL * 1024 * 1024)   /* 2 MiB per huge page */
#define MAX_REGIONS     64                       /* max numa_maps regions */
#define IOV_BATCH       1024                     /* syscall iov batch size */

/* ── data structures ── */
struct region {
    uint64_t base_va;
    size_t   page_count;
};

struct page_entry {
    uint64_t va;
    uint64_t freq;
};

/* ── globals ── */
static int verbose    = 0;
static volatile int g_stop = 0;

static void on_signal(int sig)
{
    (void)sig;
    g_stop = 1;
}

/* ── syscall wrappers ── */
static int do_pidfd_open(pid_t pid)
{
    return (int)syscall(__NR_pidfd_open, (long)pid, 0L);
}

static ssize_t do_process_madvise(int pidfd, const struct iovec *iov,
                                   size_t vlen, int advice)
{
    return (ssize_t)syscall(__NR_process_madvise, (long)pidfd,
                            iov, (unsigned long)vlen,
                            (long)advice, 0L);
}

/* ── numa_maps parser ── */
/*
 * 筛选同时含有 "huge" 和 "file=/dev/hugepages" 的行，
 * 提取行首 VA 和 anon= 字段的页数。
 */
static int parse_numa_maps(pid_t pid, struct region *regions, int *out_count)
{
    char path[64];
    char line[4096];
    FILE *f;
    int  n = 0;

    snprintf(path, sizeof(path), "/proc/%d/numa_maps", (int)pid);
    f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Cannot open %s: %s\n", path, strerror(errno));
        return -1;
    }

    while (fgets(line, sizeof(line), f)) {
        char     *endptr;
        uint64_t  base_va;
        char     *anon_ptr;
        size_t    page_count;

        if (!strstr(line, "huge") || !strstr(line, "file=/dev/hugepages"))
            continue;

        base_va = strtoull(line, &endptr, 16);
        if (endptr == line)
            continue;

        anon_ptr = strstr(line, "anon=");
        if (!anon_ptr)
            continue;

        page_count = (size_t)strtoull(anon_ptr + 5, NULL, 10);
        if (page_count == 0)
            continue;

        if (n >= MAX_REGIONS) {
            fprintf(stderr, "Warning: more than %d regions, ignoring the rest\n",
                    MAX_REGIONS);
            break;
        }

        regions[n].base_va    = base_va;
        regions[n].page_count = page_count;

        if (verbose)
            printf("[init] region %d: base=0x%016lx  pages=%zu  (%.1f GiB)\n",
                   n, base_va, page_count,
                   (double)page_count * 2.0 / 1024.0);
        n++;
    }

    fclose(f);
    *out_count = n;
    return 0;
}

/* ── freq.txt reader ── */
static int read_freq(const char *path, uint64_t *freq,
                     size_t *out_count, size_t max_count)
{
    FILE   *f;
    size_t  n = 0;
    uint64_t val;

    f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Cannot open %s: %s\n", path, strerror(errno));
        return -1;
    }

    while (fscanf(f, "%lu", &val) == 1) {
        if (n >= max_count) {
            fprintf(stderr,
                    "Warning: freq.txt has more entries than pages; "
                    "truncating at %zu\n", max_count);
            break;
        }
        freq[n++] = val;
    }

    fclose(f);
    *out_count = n;
    return 0;
}

/* ── comparator: ascending frequency ── */
static int cmp_freq_asc(const void *a, const void *b)
{
    const struct page_entry *pa = (const struct page_entry *)a;
    const struct page_entry *pb = (const struct page_entry *)b;
    if (pa->freq < pb->freq) return -1;
    if (pa->freq > pb->freq) return  1;
    return 0;
}

/* ── usage ── */
static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s -p <pid> -f <freq.txt> -r <ratio> [-i <sec>] [-v] [-h]\n"
        "\n"
        "  -p <pid>     Target QEMU process PID\n"
        "  -f <path>    Path to freq.txt (space-separated access counts,\n"
        "               ordered by ascending VA across all hugepage regions)\n"
        "  -r <ratio>   Fraction of coldest pages to page out, e.g. 0.30\n"
        "  -i <sec>     Pageout cycle interval in seconds (default: 60)\n"
        "  -v           Verbose: print each page address on pageout\n"
        "  -h           Show this help\n",
        prog);
}

/* ── main ── */
int main(int argc, char *argv[])
{
    pid_t       pid      = 0;
    const char *freq_path = NULL;
    double      ratio    = 0.0;
    int         interval = 60;
    int         opt;

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

    if (pid <= 0 || !freq_path || ratio <= 0.0 || ratio > 1.0) {
        fprintf(stderr, "Error: -p, -f, and -r (0,1] are required.\n\n");
        usage(argv[0]);
        return 1;
    }
    if (interval <= 0) {
        fprintf(stderr, "Error: interval must be > 0.\n");
        return 1;
    }

    if (geteuid() != 0)
        fprintf(stderr,
                "Warning: not running as root; process_madvise may be denied.\n");

    /* ════════════════════════════════════════════
     * Initialization phase (runs once)
     * ════════════════════════════════════════════ */

    /* 1. Parse numa_maps */
    struct region regions[MAX_REGIONS];
    int region_count = 0;

    if (parse_numa_maps(pid, regions, &region_count) < 0)
        return 1;
    if (region_count == 0) {
        fprintf(stderr,
                "No hugepage regions found in /proc/%d/numa_maps.\n"
                "Make sure the VM is running and the path filter "
                "\"file=/dev/hugepages\" matches.\n", (int)pid);
        return 1;
    }

    size_t total_pages = 0;
    for (int i = 0; i < region_count; i++)
        total_pages += regions[i].page_count;

    printf("[init] %d region(s), %zu × 2MiB pages = %.1f GiB total\n",
           region_count, total_pages, (double)total_pages * 2.0 / 1024.0);

    /* 2. Read freq.txt */
    uint64_t *freq = malloc(total_pages * sizeof(uint64_t));
    if (!freq) { perror("malloc"); return 1; }

    size_t freq_count = 0;
    if (read_freq(freq_path, freq, &freq_count, total_pages) < 0) {
        free(freq);
        return 1;
    }
    if (freq_count == 0) {
        fprintf(stderr, "freq.txt is empty or unreadable.\n");
        free(freq);
        return 1;
    }
    if (freq_count != total_pages) {
        fprintf(stderr,
                "Warning: freq.txt has %zu entries, numa_maps has %zu pages; "
                "using min=%zu.\n",
                freq_count, total_pages,
                freq_count < total_pages ? freq_count : total_pages);
        if (freq_count < total_pages)
            total_pages = freq_count;
    }

    /* 3. Build (va, freq) pairs — VA assigned globally in ascending VA order */
    struct page_entry *pages = malloc(total_pages * sizeof(struct page_entry));
    if (!pages) { perror("malloc"); free(freq); return 1; }

    {
        size_t idx = 0;
        for (int r = 0; r < region_count && idx < total_pages; r++) {
            for (size_t p = 0; p < regions[r].page_count && idx < total_pages; p++) {
                pages[idx].va   = regions[r].base_va + p * HUGEPAGE_SIZE;
                pages[idx].freq = freq[idx];
                idx++;
            }
        }
    }
    free(freq);

    /* 4. Sort ascending by frequency — coldest pages at the front */
    qsort(pages, total_pages, sizeof(*pages), cmp_freq_asc);

    size_t cold_count = (size_t)(total_pages * ratio);
    if (cold_count == 0) cold_count = 1;

    printf("[init] swap ratio=%.2f → %zu coldest pages selected (%.1f GiB)\n",
           ratio, cold_count, (double)cold_count * 2.0 / 1024.0);
    if (verbose) {
        printf("[init] cold page VA list (first 10):\n");
        for (size_t i = 0; i < cold_count && i < 10; i++)
            printf("       [%zu] va=0x%016lx  freq=%lu\n",
                   i, pages[i].va, pages[i].freq);
        if (cold_count > 10)
            printf("       ... (%zu more)\n", cold_count - 10);
    }

    /* 5. Open pidfd for the target process */
    int pidfd = do_pidfd_open(pid);
    if (pidfd < 0) {
        fprintf(stderr, "pidfd_open(%d) failed: %s\n", (int)pid, strerror(errno));
        free(pages);
        return 1;
    }

    /* Pre-allocate iovec batch buffer */
    size_t batch_cap = (cold_count < IOV_BATCH) ? cold_count : IOV_BATCH;
    struct iovec *iov_buf = malloc(batch_cap * sizeof(struct iovec));
    if (!iov_buf) { perror("malloc"); close(pidfd); free(pages); return 1; }

    /* ════════════════════════════════════════════
     * Periodic phase
     * ════════════════════════════════════════════ */
    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);

    printf("[loop] starting pageout every %d second(s); Ctrl+C to stop\n",
           interval);

    size_t cycle = 0;
    while (!g_stop) {
        size_t paged_bytes = 0;
        size_t err_pages   = 0;
        size_t i = 0;

        while (i < cold_count && !g_stop) {
            /* Fill one batch */
            size_t n = cold_count - i;
            if (n > batch_cap) n = batch_cap;

            for (size_t j = 0; j < n; j++) {
                iov_buf[j].iov_base = (void *)(uintptr_t)pages[i + j].va;
                iov_buf[j].iov_len  = HUGEPAGE_SIZE;
                if (verbose)
                    printf("[cycle %zu] pageout va=0x%016lx freq=%lu\n",
                           cycle + 1, pages[i + j].va, pages[i + j].freq);
            }

            ssize_t ret = do_process_madvise(pidfd, iov_buf, n, MADV_PAGEOUT);
            if (ret < 0) {
                if (errno == ESRCH) {
                    fprintf(stderr,
                            "Target process %d has exited. Stopping.\n",
                            (int)pid);
                    g_stop = 1;
                    break;
                }
                err_pages += n;
                if (verbose)
                    fprintf(stderr,
                            "[cycle %zu] process_madvise failed (batch %zu pages "
                            "at va=0x%lx): %s\n",
                            cycle + 1, n, pages[i].va, strerror(errno));
            } else {
                paged_bytes += (size_t)ret;
            }
            i += n;
        }

        cycle++;
        printf("[cycle %zu] done: ~%.1f MiB advised out, %zu page-errors\n",
               cycle,
               (double)paged_bytes / (1024.0 * 1024.0),
               err_pages);

        if (!g_stop)
            sleep((unsigned int)interval);
    }

    printf("[exit] total cycles: %zu\n", cycle);

    free(iov_buf);
    close(pidfd);
    free(pages);
    return 0;
}
