/* SPDX-License-Identifier: MIT */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>

#include "memdemo.h"

#define DEFAULT_GPA_BASE (4ULL << 30) /* 4 GiB, comfortably above the 1 GiB floor */

size_t memdemo_page_bytes(enum page_kind kind)
{
    return kind == PAGE_2M ? (2UL << 20) : (4UL << 10);
}

void print_usage(const char *prog)
{
    fprintf(stderr,
        "memdemo - ground-truth generator for SMAP scan accuracy\n\n"
        "Usage: %s [options]\n\n"
        "  --page-size <4k|2m>       page granularity (default 4k)\n"
        "  --mode <process|vm>       run as a plain process or a KVM guest (default process)\n"
        "  --pattern <name>          uniform | random | gaussian | zipf (default uniform)\n"
        "  --size <MB>               total region size in MiB (default 8, min 2)\n"
        "  --hot-ratio <0..1>        fraction of pages forming the hot region (default 0.1)\n"
        "  --iterations <n>          number of accesses to perform (default 100x pages)\n"
        "  --duration <sec>          run for N seconds instead of a fixed count\n"
        "  --seed <n>                PRNG seed for reproducibility (default 1)\n"
        "  --hot-threshold <n>       a page is hot if accessed >= n times (default: mean)\n"
        "  --gpa-base <bytes>        guest physical base for the VM memslot (default 4G)\n"
        "  --out <path>              bitmap output file (default memdemo.bitmap)\n"
        "  --help                    show this help\n",
        prog);
}

static int parse_u64(const char *s, uint64_t *out)
{
    char *end = NULL;
    errno = 0;
    unsigned long long v = strtoull(s, &end, 0);
    if (errno || !end || *end != '\0')
        return -1;
    *out = (uint64_t)v;
    return 0;
}

int parse_args(int argc, char **argv, struct memdemo_config *cfg)
{
    enum {
        OPT_PAGE_SIZE = 1000,
        OPT_MODE,
        OPT_PATTERN,
        OPT_SIZE,
        OPT_HOT_RATIO,
        OPT_ITERATIONS,
        OPT_DURATION,
        OPT_SEED,
        OPT_HOT_THRESHOLD,
        OPT_GPA_BASE,
        OPT_OUT,
        OPT_HELP,
    };
    static const struct option longopts[] = {
        { "page-size", required_argument, NULL, OPT_PAGE_SIZE },
        { "mode", required_argument, NULL, OPT_MODE },
        { "pattern", required_argument, NULL, OPT_PATTERN },
        { "size", required_argument, NULL, OPT_SIZE },
        { "hot-ratio", required_argument, NULL, OPT_HOT_RATIO },
        { "iterations", required_argument, NULL, OPT_ITERATIONS },
        { "duration", required_argument, NULL, OPT_DURATION },
        { "seed", required_argument, NULL, OPT_SEED },
        { "hot-threshold", required_argument, NULL, OPT_HOT_THRESHOLD },
        { "gpa-base", required_argument, NULL, OPT_GPA_BASE },
        { "out", required_argument, NULL, OPT_OUT },
        { "help", no_argument, NULL, OPT_HELP },
        { NULL, 0, NULL, 0 },
    };

    /* Defaults. */
    memset(cfg, 0, sizeof(*cfg));
    cfg->page_kind = PAGE_4K;
    cfg->mode = MODE_PROCESS;
    cfg->pattern = PAT_UNIFORM;
    cfg->size_bytes = 8UL << 20;
    cfg->hot_ratio = 0.1;
    cfg->iterations = 0;
    cfg->duration_sec = 0;
    cfg->seed = 1;
    cfg->hot_threshold = -1;
    cfg->gpa_base = DEFAULT_GPA_BASE;
    cfg->out_path = "memdemo.bitmap";

    int c;
    while ((c = getopt_long(argc, argv, "h", longopts, NULL)) != -1) {
        uint64_t v;
        switch (c) {
        case OPT_PAGE_SIZE:
            if (!strcmp(optarg, "4k") || !strcmp(optarg, "4K"))
                cfg->page_kind = PAGE_4K;
            else if (!strcmp(optarg, "2m") || !strcmp(optarg, "2M"))
                cfg->page_kind = PAGE_2M;
            else
                return fprintf(stderr, "invalid --page-size: %s\n", optarg), -1;
            break;
        case OPT_MODE:
            if (!strcmp(optarg, "process"))
                cfg->mode = MODE_PROCESS;
            else if (!strcmp(optarg, "vm"))
                cfg->mode = MODE_VM;
            else
                return fprintf(stderr, "invalid --mode: %s\n", optarg), -1;
            break;
        case OPT_PATTERN:
            if (!strcmp(optarg, "uniform"))
                cfg->pattern = PAT_UNIFORM;
            else if (!strcmp(optarg, "random"))
                cfg->pattern = PAT_RANDOM;
            else if (!strcmp(optarg, "gaussian"))
                cfg->pattern = PAT_GAUSSIAN;
            else if (!strcmp(optarg, "zipf"))
                cfg->pattern = PAT_ZIPF;
            else
                return fprintf(stderr, "invalid --pattern: %s\n", optarg), -1;
            break;
        case OPT_SIZE:
            if (parse_u64(optarg, &v) || v == 0)
                return fprintf(stderr, "invalid --size: %s\n", optarg), -1;
            cfg->size_bytes = v << 20;
            break;
        case OPT_HOT_RATIO: {
            char *end = NULL;
            cfg->hot_ratio = strtod(optarg, &end);
            if (!end || *end != '\0' || cfg->hot_ratio <= 0.0 ||
                cfg->hot_ratio > 1.0)
                return fprintf(stderr, "invalid --hot-ratio: %s\n", optarg), -1;
            break;
        }
        case OPT_ITERATIONS:
            if (parse_u64(optarg, &cfg->iterations))
                return fprintf(stderr, "invalid --iterations: %s\n", optarg), -1;
            break;
        case OPT_DURATION:
            if (parse_u64(optarg, &cfg->duration_sec))
                return fprintf(stderr, "invalid --duration: %s\n", optarg), -1;
            break;
        case OPT_SEED:
            if (parse_u64(optarg, &cfg->seed))
                return fprintf(stderr, "invalid --seed: %s\n", optarg), -1;
            break;
        case OPT_HOT_THRESHOLD:
            if (parse_u64(optarg, &v))
                return fprintf(stderr, "invalid --hot-threshold: %s\n", optarg), -1;
            cfg->hot_threshold = (int64_t)v;
            break;
        case OPT_GPA_BASE:
            if (parse_u64(optarg, &cfg->gpa_base))
                return fprintf(stderr, "invalid --gpa-base: %s\n", optarg), -1;
            break;
        case OPT_OUT:
            cfg->out_path = optarg;
            break;
        case OPT_HELP:
        case 'h':
            print_usage(argv[0]);
            exit(0);
        default:
            return -1;
        }
    }

    /* Cross-field validation. */
    size_t pb = memdemo_page_bytes(cfg->page_kind);
    if (cfg->size_bytes % pb)
        cfg->size_bytes = ((cfg->size_bytes + pb - 1) / pb) * pb;

    /* SMAP only scans memslots >= 2 MiB (HUGE_TO_4K_SHIFT) and GPA >= 1 GiB. */
    if (cfg->size_bytes < (2UL << 20)) {
        fprintf(stderr, "region must be at least 2 MiB (SMAP memslot floor)\n");
        return -1;
    }
    if (cfg->mode == MODE_VM && cfg->gpa_base < (1UL << 30)) {
        fprintf(stderr,
            "--gpa-base must be >= 1 GiB or SMAP skips the memslot\n");
        return -1;
    }
    if (cfg->mode == MODE_VM && (cfg->gpa_base % pb)) {
        fprintf(stderr, "--gpa-base must be page-aligned (%zu bytes)\n", pb);
        return -1;
    }
    return 0;
}
