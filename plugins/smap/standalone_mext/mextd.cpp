/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 * Description: Standalone MEXT Predictive Memory & Dynamic Proactive Reclaim
 *              daemon (`mextd`). Runs completely decoupled from SMAP's legacy
 *              page-table tracking (`smap_access_tracking.ko`), NUMA migration,
 *              and `ubturbod`.
 */

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/mman.h>
#endif

#include "../src/tiering/mext_ring_abi.h"
#include "../src/user/predictor/mext_engine.h"
#include "../src/user/predictor/mext_predictor_c_api.h"
#include "../src/user/reclaim/proactive_reclaim.h"
#include "../src/user/reclaim/proactive_reclaim_c_api.h"

namespace {

using smap::mext::MextEngine;
using smap::reclaim::HostMemSnapshot;
using smap::reclaim::ProactiveReclaimController;
using smap::reclaim::ReclaimDecision;

constexpr uint8_t kMextIocMagic = 0xB9;
#define MEXTD_IOCTL_CTRL_V1 _IOWR(kMextIocMagic, 6, struct mext_ctrl_msg)
#define MEXTD_IOCTL_CTRL_V2 _IOWR(kMextIocMagic, 7, struct mext_ctrl_msg)

constexpr uint64_t kCgroupUnlimitedBytes = (1ULL << 56);

std::atomic<bool> g_stop_requested{ false };

void SignalHandler(int)
{
    g_stop_requested.store(true, std::memory_order_release);
}

struct DaemonOptions {
    std::string device_path = "/dev/mext_ctl";
    std::string stats_json_path = "/var/run/smap_mext_stats.json";
    std::string pidfile_path;
    std::string ctl_fifo_path;
    std::string watch_cgroup = "auto";
    std::string weights_path;
    bool track_all_pids = true;
    bool enable_reclaim = false;
    bool force_zram = false;
    bool force_nvme = false;
    bool tune_page_cluster = true;
    bool daemonize = false;
    bool verbose = false;
    bool self_test = false;
    std::vector<int32_t> initial_pids;
};

enum class SwapBackendKind {
    kNone,
    kNvmeOrBlock,
    kZram,
};

SwapBackendKind DetectSystemSwapBackend(std::string *first_dev_out)
{
    FILE *fp = std::fopen("/proc/swaps", "r");
    if (!fp) {
        return SwapBackendKind::kNone;
    }
    char line[512];
    if (!std::fgets(line, sizeof(line), fp)) {
        std::fclose(fp);
        return SwapBackendKind::kNone;
    }
    SwapBackendKind kind = SwapBackendKind::kNone;
    while (std::fgets(line, sizeof(line), fp) != nullptr) {
        char dev[256] = { 0 };
        if (std::sscanf(line, "%255s", dev) == 1 && dev[0] == '/') {
            if (first_dev_out && first_dev_out->empty()) {
                *first_dev_out = dev;
            }
            if (std::strncmp(dev, "/dev/zram", 9) == 0) {
                kind = SwapBackendKind::kZram;
                break;
            }
            kind = SwapBackendKind::kNvmeOrBlock;
        }
    }
    std::fclose(fp);
    return kind;
}

class PageClusterGuard {
public:
    explicit PageClusterGuard(bool enable)
    {
        if (!enable) {
            return;
        }
        FILE *fr = std::fopen("/proc/sys/vm/page-cluster", "r");
        if (fr) {
            char buf[32] = { 0 };
            if (std::fgets(buf, sizeof(buf), fr)) {
                saved_val_ = std::atoi(buf);
                has_saved_ = true;
            }
            std::fclose(fr);
        }
        FILE *fw = std::fopen("/proc/sys/vm/page-cluster", "w");
        if (fw) {
            std::fputs("0\n", fw);
            std::fclose(fw);
            modified_ = true;
        }
    }

    ~PageClusterGuard()
    {
        if (modified_ && has_saved_) {
            FILE *fw = std::fopen("/proc/sys/vm/page-cluster", "w");
            if (fw) {
                std::fprintf(fw, "%d\n", saved_val_);
                std::fclose(fw);
            }
        }
    }

private:
    int saved_val_ = 3;
    bool has_saved_ = false;
    bool modified_ = false;
};

int IssueCtrlIoctl(int fd, uint32_t op, int32_t pid)
{
    mext_ctrl_msg msg{};
    msg.op = op;
    msg.pid = pid;
    if (ioctl(fd, MEXTD_IOCTL_CTRL_V1, &msg) == 0) {
        return 0;
    }
    return ioctl(fd, MEXTD_IOCTL_CTRL_V2, &msg);
}

bool IsFiniteCgroupDir(const std::filesystem::path &dir)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path v2_max = dir / "memory.max";
    if (fs::is_regular_file(v2_max, ec)) {
        FILE *fp = std::fopen(v2_max.c_str(), "r");
        if (!fp) {
            return false;
        }
        char buf[64] = { 0 };
        bool ok = (std::fgets(buf, sizeof(buf), fp) != nullptr);
        std::fclose(fp);
        if (!ok || std::strncmp(buf, "max", 3) == 0) {
            return false;
        }
        char *endp = nullptr;
        unsigned long long v = std::strtoull(buf, &endp, 10);
        return v > 0 && static_cast<uint64_t>(v) < kCgroupUnlimitedBytes;
    }

    fs::path v1_lim = dir / "memory.limit_in_bytes";
    if (fs::is_regular_file(v1_lim, ec)) {
        FILE *fp = std::fopen(v1_lim.c_str(), "r");
        if (!fp) {
            return false;
        }
        char buf[64] = { 0 };
        bool ok = (std::fgets(buf, sizeof(buf), fp) != nullptr);
        std::fclose(fp);
        if (!ok) {
            return false;
        }
        char *endp = nullptr;
        unsigned long long v = std::strtoull(buf, &endp, 10);
        return v > 0 && static_cast<uint64_t>(v) < kCgroupUnlimitedBytes;
    }
    return false;
}

void ReadPidsFromCgroupProcs(const std::filesystem::path &procs_file,
                             int32_t self_pid,
                             std::unordered_set<int32_t> *out_pids)
{
    FILE *fp = std::fopen(procs_file.c_str(), "r");
    if (!fp) {
        return;
    }
    char line[64];
    while (std::fgets(line, sizeof(line), fp) != nullptr) {
        int32_t pid = static_cast<int32_t>(std::strtol(line, nullptr, 10));
        if (pid > 1 && pid != self_pid) {
            out_pids->insert(pid);
        }
    }
    std::fclose(fp);
}

/*
 * Scans cgroup hierarchy (up to depth 4) and collects PIDs belonging to any
 * memory-capped sandbox cgroup (v1 `memory.limit_in_bytes < 64 PiB` or v2
 * `memory.max != "max"`), automatically ignoring unconstrained host daemons.
 */
void DiscoverCappedCgroupPids(const std::vector<std::string> &roots,
                              int32_t self_pid,
                              std::unordered_set<int32_t> *out_pids)
{
    namespace fs = std::filesystem;
    std::error_code ec;

    for (const auto &root_str : roots) {
        fs::path root(root_str);
        if (!fs::is_directory(root, ec)) {
            continue;
        }
        if (IsFiniteCgroupDir(root)) {
            ReadPidsFromCgroupProcs(root / "cgroup.procs", self_pid, out_pids);
        }

        fs::recursive_directory_iterator it(
            root, fs::directory_options::skip_permission_denied, ec);
        fs::recursive_directory_iterator end;
        for (; !ec && it != end; it.increment(ec)) {
            if (it.depth() > 4) {
                it.disable_recursion_pending();
                continue;
            }
            if (!it->is_directory(ec)) {
                continue;
            }
            const fs::path &sub = it->path();
            if (IsFiniteCgroupDir(sub)) {
                ReadPidsFromCgroupProcs(sub / "cgroup.procs", self_pid, out_pids);
                /* Include nested child cgroups (e.g. `.../sb-42/vcpu/cgroup.procs`) */
                std::error_code sub_ec;
                for (const auto &child : fs::directory_iterator(
                         sub, fs::directory_options::skip_permission_denied, sub_ec)) {
                    if (child.is_directory(sub_ec)) {
                        ReadPidsFromCgroupProcs(child.path() / "cgroup.procs",
                                                self_pid, out_pids);
                    }
                }
            }
        }
    }
}

class StandalonePidRegistry {
public:
    StandalonePidRegistry(int dev_fd, MextEngine *eng, ProactiveReclaimController *reclaim)
        : dev_fd_(dev_fd), eng_(eng), reclaim_(reclaim), self_pid_(static_cast<int32_t>(getpid()))
    {
    }

    void SyncDiscoveredPids(const std::unordered_set<int32_t> &discovered, bool register_mext_filter)
    {
        std::vector<int32_t> to_add;
        std::vector<int32_t> to_remove;

        {
            std::lock_guard<std::mutex> lk(mu_);
            for (int32_t pid : discovered) {
                if (pid <= 1 || pid == self_pid_) {
                    continue;
                }
                if (tracked_.insert(pid).second) {
                    to_add.push_back(pid);
                }
            }
            for (auto it = tracked_.begin(); it != tracked_.end();) {
                int32_t pid = *it;
                if (explicit_pids_.count(pid) == 0 && discovered.count(pid) == 0) {
                    to_remove.push_back(pid);
                    it = tracked_.erase(it);
                } else {
                    ++it;
                }
            }
        }

        for (int32_t pid : to_add) {
            if (register_mext_filter && eng_) {
                (void)eng_->TrackPid(pid);
            }
            if (reclaim_) {
                (void)reclaim_->TrackPid(pid);
            }
        }
        for (int32_t pid : to_remove) {
            if (eng_) {
                eng_->UntrackPid(pid);
            }
            if (reclaim_) {
                reclaim_->UntrackPid(pid);
            }
        }
    }

    void AddExplicitPid(int32_t pid, bool register_mext_filter)
    {
        if (pid <= 1 || pid == self_pid_) {
            return;
        }
        bool inserted = false;
        {
            std::lock_guard<std::mutex> lk(mu_);
            explicit_pids_.insert(pid);
            inserted = tracked_.insert(pid).second;
        }
        if (inserted) {
            if (register_mext_filter && eng_) {
                (void)eng_->TrackPid(pid);
            }
            if (reclaim_) {
                (void)reclaim_->TrackPid(pid);
            }
        }
    }

    void RemoveExplicitPid(int32_t pid)
    {
        bool erased = false;
        {
            std::lock_guard<std::mutex> lk(mu_);
            explicit_pids_.erase(pid);
            erased = (tracked_.erase(pid) > 0);
        }
        if (erased) {
            if (eng_) {
                eng_->UntrackPid(pid);
            }
            if (reclaim_) {
                reclaim_->UntrackPid(pid);
            }
        }
    }

    void PruneDeadExplicitPids(const std::string &proc_root = "/proc")
    {
        std::vector<int32_t> snapshot;
        {
            std::lock_guard<std::mutex> lk(mu_);
            snapshot.assign(explicit_pids_.begin(), explicit_pids_.end());
        }
        for (int32_t pid : snapshot) {
            char p[64];
            std::snprintf(p, sizeof(p), "%s/%d", proc_root.c_str(), pid);
            if (access(p, F_OK) != 0) {
                RemoveExplicitPid(pid);
            }
        }
    }

    uint64_t ExecuteProactiveEvict(uint64_t target_pages)
    {
        if (target_pages == 0 || !eng_ || !reclaim_ || dev_fd_ < 0) {
            return 0;
        }
        bool emergency = (reclaim_->CurrentUrgency() == RECLAIM_EMERGENCY);

        struct WeightedPid {
            int32_t pid;
            uint32_t score;
        };
        std::vector<WeightedPid> active;
        uint64_t total_score = 0;

        {
            std::lock_guard<std::mutex> lk(mu_);
            active.reserve(std::min<size_t>(tracked_.size(), 256U));
            for (int32_t pid : tracked_) {
                uint32_t sc = reclaim_->GetPidPressureScore(pid);
                if (sc == 0U) {
                    continue;
                }
                /*
                 * Do not proactively evict from a sandbox that took a swap-in
                 * fault within the last 50 ms — its cgroup is actively paging in
                 * hot working-set pages; target idle/cooling cgroups instead!
                 */
                if (eng_->WasPidRecentlyFaulting(pid, 50'000'000ULL)) {
                    continue;
                }
                if (active.size() < 256U) {
                    active.push_back({ pid, sc });
                    total_score += sc;
                }
            }
        }

        if (active.empty() || total_score == 0) {
            return 0;
        }

        uint64_t reclaimed_total = 0;
        uint64_t remaining = target_pages;
        for (const auto &wp : active) {
            if (remaining == 0) {
                break;
            }
            uint64_t share = (target_pages * wp.score + total_score - 1ULL) / total_score;
            if (share == 0) {
                share = 1ULL;
            }
            share = std::min<uint64_t>(share, std::min<uint64_t>(remaining, MEXT_MAX_PAGEOUT_PAGES));
            int rec = eng_->PageoutPidCold(dev_fd_, wp.pid, static_cast<uint32_t>(share), emergency);
            if (rec > 0) {
                uint64_t urec = static_cast<uint64_t>(rec);
                reclaimed_total += urec;
                remaining = (urec >= remaining) ? 0ULL : (remaining - urec);
            }
        }
        return reclaimed_total;
    }

    size_t TrackedCount() const
    {
        std::lock_guard<std::mutex> lk(mu_);
        return tracked_.size();
    }

private:
    int dev_fd_ = -1;
    MextEngine *eng_ = nullptr;
    ProactiveReclaimController *reclaim_ = nullptr;
    int32_t self_pid_ = 0;
    mutable std::mutex mu_;
    std::unordered_set<int32_t> tracked_;
    std::unordered_set<int32_t> explicit_pids_;
};

int RunSelfTest()
{
    namespace fs = std::filesystem;
    fs::path tmp = fs::current_path() / ("mextd_selftest_" + std::to_string(getpid()));
    fs::path mock_proc = tmp / "proc";
    fs::path mock_cg = tmp / "cgroup";
    fs::create_directories(mock_proc / "51001");
    fs::create_directories(mock_proc / "51002");
    fs::create_directories(mock_cg / "memory" / "aenv" / "sb-1" / "vcpu");
    fs::create_directories(mock_cg / "memory" / "system.slice");

    struct Guard {
        fs::path p;
        ~Guard()
        {
            std::error_code ec;
            fs::remove_all(p, ec);
        }
    } cleanup{ tmp };

    /* System slice is unconstrained; sb-1 has 64 MB cgroup v1 limit @ 98% usage */
    std::ofstream(mock_cg / "memory" / "system.slice" / "memory.limit_in_bytes")
        << 9223372036854771712ULL << "\n";
    std::ofstream(mock_cg / "memory" / "system.slice" / "cgroup.procs") << "999\n";

    std::ofstream(mock_cg / "memory" / "aenv" / "sb-1" / "memory.limit_in_bytes")
        << (64ULL << 20) << "\n";
    std::ofstream(mock_cg / "memory" / "aenv" / "sb-1" / "memory.usage_in_bytes")
        << (63ULL << 20) << "\n";
    std::ofstream(mock_cg / "memory" / "aenv" / "sb-1" / "memory.failcnt") << "7\n";
    std::ofstream(mock_cg / "memory" / "aenv" / "sb-1" / "vcpu" / "cgroup.procs")
        << "51001\n51002\n";

    std::ofstream(mock_proc / "51001" / "cgroup") << "6:memory:/aenv/sb-1/vcpu\n";
    std::ofstream(mock_proc / "51002" / "cgroup") << "6:memory:/aenv/sb-1/vcpu\n";

    std::unordered_set<int32_t> discovered;
    DiscoverCappedCgroupPids({ (mock_cg / "memory").string() },
                             static_cast<int32_t>(getpid()), &discovered);
    if (discovered.count(999) != 0 || discovered.count(51001) != 1 || discovered.count(51002) != 1) {
        std::fprintf(stderr, "[FAIL] Cgroup sandbox discovery filter mismatch\n");
        return 1;
    }

    auto *ring = static_cast<mext_ring_buffer *>(std::calloc(1, sizeof(mext_ring_buffer)));
    if (!ring) {
        return 1;
    }
    ring->hdr.magic = MEXT_RING_MAGIC;
    ring->hdr.version = MEXT_RING_VERSION;
    ring->hdr.capacity = MEXT_RING_CAPACITY;
    ring->hdr.page_shift = 12;

    MextEngine eng;
    MextConfig mcfg{};
    MextDefaultConfig(&mcfg);
    fs::path stats_file = tmp / "mext_stats.json";
    eng.SetStatsJsonPath(stats_file.string());
    eng.AttachExternalRing(ring);
    if (eng.Start(-1, mcfg, false) != 0) {
        std::free(ring);
        return 1;
    }

    uint64_t prefetched = 0;
    eng.SetDispatchCallback([&](int32_t, const std::vector<uint64_t> &pf, const std::vector<uint64_t> &flt) {
        prefetched += pf.size() + flt.size();
    });

    for (uint64_t i = 0; i < 16; ++i) {
        mext_ring_event &ev = ring->events[i];
        ev.pid = 51001;
        ev.vpn = 0x20000ULL + i * 2ULL;
        ev.aux = 3;
        ev.timestamp = (i + 1ULL) * 100000ULL;
        ev.type = MEXT_EVT_SWAP_FAULT;
        ring->hdr.write_idx = i + 1ULL;
    }
    (void)eng.ProcessRingOnce(64);
    eng.FlushStatsJson();

    ProactiveReclaimController reclaim;
    reclaim.BindMextEngine(&eng);
    reclaim.SetDiscoveryRoots(mock_proc.string(), mock_cg.string());
    reclaim.SetTelemetryProvider([](HostMemSnapshot *out) -> bool {
        out->timestamp_ns = 1'000'000'000ULL;
        out->total_pages = 201326592ULL;
        out->free_pages = 71014810ULL;
        out->wmark_min_pages = 262144ULL;
        out->wmark_low_pages = 327680ULL;
        out->wmark_high_pages = 393216ULL;
        return true;
    });

    ProactiveReclaimConfig rcfg{};
    ProactiveReclaimDefaultConfig(&rcfg);
    StandalonePidRegistry reg(-1, &eng, &reclaim);
    reg.SyncDiscoveredPids(discovered, false);

    uint64_t evict_req = 0;
    reclaim.Start(rcfg, [&](uint64_t target, uint8_t) {
        evict_req = target;
        return target;
    }, false);
    ReclaimDecision dec = reclaim.StepOnce();

    reclaim.Stop();
    eng.Stop();
    std::free(ring);

    if (prefetched == 0 || !fs::exists(stats_file) ||
        dec.urgency != RECLAIM_EMERGENCY || evict_req == 0 || reg.TrackedCount() != 2U) {
        std::fprintf(stderr, "[FAIL] Standalone mextd self-test invariants failed\n");
        return 1;
    }

    std::printf("[PASS] Standalone mextd self-test: discovered %zu cgroup-v1 sandboxes, "
                "prefetched %llu pages, proactive target=%llu pages (urgency=EMERGENCY)\n",
                reg.TrackedCount(),
                static_cast<unsigned long long>(prefetched),
                static_cast<unsigned long long>(evict_req));
    return 0;
}

void PrintUsage(const char *prog)
{
    std::printf(
        "Usage: %s [options]\n\n"
        "Standalone MEXT Predictive Memory & Dynamic Proactive Reclaim Daemon\n"
        "(Zero dependency on smap_access_tracking.ko, smap_tracking_core.ko, or ubturbod)\n\n"
        "Options:\n"
        "  --device <path>         Kernel MEXT device (default: /dev/mext_ctl, fallback /dev/smap_mig_device)\n"
        "  --track-all             Capture swap faults from all PIDs with zero registration overhead (default)\n"
        "  --explicit-pids         Only capture faults from PIDs discovered via cgroups / --pid / --ctl-fifo\n"
        "  --reclaim               Enable Dynamic Proactive Reclaim (Cgroup v1 + v2 + host pressure)\n"
        "  --watch-cgroup <dir>    Cgroup root to auto-discover capped sandboxes (default: auto)\n"
        "  --pid <pid>             Explicitly track PID (repeatable)\n"
        "  --ctl-fifo <path>       Optional FIFO accepting 'add <pid>' / 'del <pid>' / 'flush'\n"
        "  --stats-json <path>     JSON telemetry output path (default: /var/run/smap_mext_stats.json)\n"
        "  --weights <path>        Optional TinyTransformer weights file\n"
        "  --zram                  Force synchronous zram preset\n"
        "  --nvme                  Force asynchronous NVMe/block swap preset\n"
        "  --keep-page-cluster     Do not set /proc/sys/vm/page-cluster=0\n"
        "  --daemon                Fork into background after initialization\n"
        "  --pidfile <path>        Write daemon PID file\n"
        "  --verbose               Print periodic 5s telemetry summary to stdout\n"
        "  --self-test             Run built-in standalone verification suite and exit\n"
        "  -h, --help              Show this help\n",
        prog);
}

bool ParseArgs(int argc, char **argv, DaemonOptions *opt)
{
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-h" || a == "--help") {
            PrintUsage(argv[0]);
            std::exit(0);
        } else if (a == "--self-test") {
            opt->self_test = true;
        } else if (a == "--track-all") {
            opt->track_all_pids = true;
        } else if (a == "--explicit-pids") {
            opt->track_all_pids = false;
        } else if (a == "--reclaim") {
            opt->enable_reclaim = true;
        } else if (a == "--zram") {
            opt->force_zram = true;
        } else if (a == "--nvme") {
            opt->force_nvme = true;
        } else if (a == "--keep-page-cluster") {
            opt->tune_page_cluster = false;
        } else if (a == "--daemon") {
            opt->daemonize = true;
        } else if (a == "--verbose") {
            opt->verbose = true;
        } else if (a == "--device" && i + 1 < argc) {
            opt->device_path = argv[++i];
        } else if (a == "--stats-json" && i + 1 < argc) {
            opt->stats_json_path = argv[++i];
        } else if (a == "--pidfile" && i + 1 < argc) {
            opt->pidfile_path = argv[++i];
        } else if (a == "--ctl-fifo" && i + 1 < argc) {
            opt->ctl_fifo_path = argv[++i];
        } else if (a == "--watch-cgroup" && i + 1 < argc) {
            opt->watch_cgroup = argv[++i];
        } else if (a == "--weights" && i + 1 < argc) {
            opt->weights_path = argv[++i];
        } else if (a == "--pid" && i + 1 < argc) {
            int32_t p = static_cast<int32_t>(std::strtol(argv[++i], nullptr, 10));
            if (p > 1) {
                opt->initial_pids.push_back(p);
            }
        } else {
            std::fprintf(stderr, "Unknown or incomplete argument: %s\n", a.c_str());
            return false;
        }
    }
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    DaemonOptions opt;
    if (!ParseArgs(argc, argv, &opt)) {
        PrintUsage(argv[0]);
        return 1;
    }
    if (opt.self_test) {
        return RunSelfTest();
    }

    int dev_fd = open(opt.device_path.c_str(), O_RDWR | O_CLOEXEC);
    if (dev_fd < 0 && opt.device_path == "/dev/mext_ctl") {
        dev_fd = open("/dev/smap_mig_device", O_RDWR | O_CLOEXEC);
        if (dev_fd >= 0) {
            opt.device_path = "/dev/smap_mig_device";
        }
    }
    if (dev_fd < 0) {
        std::perror(("Failed to open " + opt.device_path +
                     " (load mext_drv.ko via `insmod kernel/mext_drv.ko`)").c_str());
        return 1;
    }

    if (opt.daemonize) {
        pid_t child = fork();
        if (child < 0) {
            std::perror("fork");
            close(dev_fd);
            return 1;
        }
        if (child > 0) {
            close(dev_fd);
            return 0;
        }
        (void)setsid();
    }

    if (!opt.pidfile_path.empty()) {
        FILE *pf = std::fopen(opt.pidfile_path.c_str(), "w");
        if (pf) {
            std::fprintf(pf, "%d\n", static_cast<int>(getpid()));
            std::fclose(pf);
        }
    }

#ifdef __linux__
    /* Keep mextd's ~2-4 MB model & heap resident so mextd never swap-faults on itself */
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
        (void)mlockall(MCL_CURRENT);
    }
#endif

    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    PageClusterGuard pc_guard(opt.tune_page_cluster);

    std::string detected_swap;
    SwapBackendKind swap_kind = DetectSystemSwapBackend(&detected_swap);
    bool use_zram = opt.force_zram || (!opt.force_nvme && swap_kind == SwapBackendKind::kZram);

    MextConfig mcfg{};
    MextDefaultConfig(&mcfg);
    if (use_zram) {
        MextApplyZramPreset(&mcfg);
    } else {
        mcfg.fault_trigger_stride = 2U;
    }
    if (!opt.weights_path.empty()) {
        std::snprintf(mcfg.weights_path, sizeof(mcfg.weights_path), "%s", opt.weights_path.c_str());
    }

    MextEngine engine;
    if (!opt.stats_json_path.empty()) {
        if (opt.stats_json_path == "/var/run/smap_mext_stats.json" && access("/var/run", W_OK) != 0) {
            opt.stats_json_path = "/tmp/smap_mext_stats.json";
        }
        engine.SetStatsJsonPath(opt.stats_json_path);
    }

    if (engine.Start(dev_fd, mcfg, true) != 0) {
        std::fprintf(stderr, "Failed to start MextEngine on %s\n", opt.device_path.c_str());
        close(dev_fd);
        return 1;
    }
    (void)IssueCtrlIoctl(dev_fd, MEXT_CTRL_ENABLE, 0);
    if (opt.track_all_pids) {
        (void)IssueCtrlIoctl(dev_fd, MEXT_CTRL_ADD_PID, 0);
    }

    ProactiveReclaimController reclaim;
    reclaim.BindMextEngine(&engine);
    StandalonePidRegistry registry(dev_fd, &engine, opt.enable_reclaim ? &reclaim : nullptr);

    for (int32_t p : opt.initial_pids) {
        registry.AddExplicitPid(p, !opt.track_all_pids);
    }

    if (opt.enable_reclaim) {
        ProactiveReclaimConfig rcfg{};
        if (use_zram) {
            ProactiveReclaimApplyZramPreset(&rcfg);
        } else {
            ProactiveReclaimDefaultConfig(&rcfg);
        }
        reclaim.Start(
            rcfg,
            [&registry](uint64_t target_pages, uint8_t) -> uint64_t {
                return registry.ExecuteProactiveEvict(target_pages);
            },
            true);
    }

    int fifo_fd = -1;
    if (!opt.ctl_fifo_path.empty()) {
        (void)unlink(opt.ctl_fifo_path.c_str());
        if (mkfifo(opt.ctl_fifo_path.c_str(), 0600) == 0) {
            fifo_fd = open(opt.ctl_fifo_path.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
        }
    }

    std::vector<std::string> cg_roots;
    if (opt.watch_cgroup == "auto") {
        if (access("/sys/fs/cgroup/memory", F_OK) == 0) {
            cg_roots.push_back("/sys/fs/cgroup/memory");
        }
        if (access("/sys/fs/cgroup/cgroup.controllers", F_OK) == 0) {
            cg_roots.push_back("/sys/fs/cgroup");
        }
    } else if (!opt.watch_cgroup.empty() && opt.watch_cgroup != "none") {
        cg_roots.push_back(opt.watch_cgroup);
    }

    std::printf("mextd started on %s | backend=%s (%s) | track_all=%s | reclaim=%s | stats=%s\n",
                opt.device_path.c_str(),
                use_zram ? "zram" : "nvme/block",
                detected_swap.empty() ? "no-active-swap-yet" : detected_swap.c_str(),
                opt.track_all_pids ? "on" : "off",
                opt.enable_reclaim ? "on" : "off",
                opt.stats_json_path.c_str());
    std::fflush(stdout);

    uint64_t tick = 0;
    const int32_t self_pid = static_cast<int32_t>(getpid());

    while (!g_stop_requested.load(std::memory_order_acquire)) {
        if (fifo_fd >= 0) {
            char fbuf[512];
            ssize_t n = read(fifo_fd, fbuf, sizeof(fbuf) - 1);
            if (n > 0) {
                fbuf[n] = '\0';
                char *savep = nullptr;
                for (char *line = strtok_r(fbuf, "\r\n", &savep); line;
                     line = strtok_r(nullptr, "\r\n", &savep)) {
                    int32_t p = 0;
                    if (std::sscanf(line, "add %d", &p) == 1) {
                        registry.AddExplicitPid(p, !opt.track_all_pids);
                    } else if (std::sscanf(line, "del %d", &p) == 1) {
                        registry.RemoveExplicitPid(p);
                    } else if (std::strncmp(line, "flush", 5) == 0) {
                        engine.FlushStatsJson();
                    }
                }
            }
        }

        /* Refresh capped-cgroup sandbox PID discovery every 200 ms when needed */
        if ((opt.enable_reclaim || !opt.track_all_pids) && !cg_roots.empty()) {
            std::unordered_set<int32_t> discovered;
            DiscoverCappedCgroupPids(cg_roots, self_pid, &discovered);
            registry.SyncDiscoveredPids(discovered, !opt.track_all_pids);
        }

        if ((tick % 5U) == 0U) {
            engine.FlushStatsJson();
            registry.PruneDeadExplicitPids();
        }

        if (opt.verbose && (tick % 25U) == 0U && tick > 0U) {
            MextStats ms = engine.GetStatsSnapshot();
            if (opt.enable_reclaim) {
                ProactiveReclaimStats rs = reclaim.GetStatsSnapshot();
                std::printf("[mextd] faults=%llu prefetch=%llu prefault=%llu hit=%.1f%% | "
                            "tracked_pids=%zu reclaimed=%llu urgency=%u\n",
                            static_cast<unsigned long long>(ms.faults_processed),
                            static_cast<unsigned long long>(ms.prefetch_issued),
                            static_cast<unsigned long long>(ms.prefault_issued),
                            ms.hit_rate * 100.0,
                            registry.TrackedCount(),
                            static_cast<unsigned long long>(rs.pages_reclaimed_proactive),
                            rs.current_urgency);
            } else {
                std::printf("[mextd] faults=%llu prefetch=%llu prefault=%llu hit=%.1f%%\n",
                            static_cast<unsigned long long>(ms.faults_processed),
                            static_cast<unsigned long long>(ms.prefetch_issued),
                            static_cast<unsigned long long>(ms.prefault_issued),
                            ms.hit_rate * 100.0);
            }
            std::fflush(stdout);
        }

        ++tick;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    engine.FlushStatsJson();
    if (opt.enable_reclaim) {
        reclaim.Stop();
    }
    (void)IssueCtrlIoctl(dev_fd, MEXT_CTRL_CLEAR_PIDS, 0);
    (void)IssueCtrlIoctl(dev_fd, MEXT_CTRL_DISABLE, 0);
    engine.Stop();

    if (fifo_fd >= 0) {
        close(fifo_fd);
        (void)unlink(opt.ctl_fifo_path.c_str());
    }
    if (!opt.pidfile_path.empty()) {
        (void)unlink(opt.pidfile_path.c_str());
    }
    close(dev_fd);
    return 0;
}
