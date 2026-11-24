/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 */

#include "swap_shadow_map.h"

namespace smap {
namespace mext {

FlatVpnHashTable::FlatVpnHashTable() : FlatVpnHashTable(1024) {}

FlatVpnHashTable::FlatVpnHashTable(uint32_t cap_pow2)
{
    uint32_t cap = 64;
    while (cap < cap_pow2) {
        cap <<= 1U;
    }
    slots_.resize(cap);
    mask_ = cap - 1U;
}

void FlatVpnHashTable::Clear()
{
    for (auto &s : slots_) {
        s.state = EMPTY;
        s.key = 0;
        s.val = 0;
    }
    size_ = 0;
    used_ = 0;
}

void FlatVpnHashTable::Rehash(uint32_t new_cap)
{
    std::vector<Slot> old = std::move(slots_);
    slots_.assign(new_cap, Slot{});
    mask_ = new_cap - 1U;
    size_ = 0;
    used_ = 0;
    for (const auto &s : old) {
        if (s.state == OCCUPIED) {
            Upsert(s.key, s.val);
        }
    }
}

void FlatVpnHashTable::Upsert(uint64_t vpn, uint64_t seq)
{
    if ((used_ + 1) * 10 >= slots_.size() * 7) {
        uint32_t next_cap = (size_ * 2 >= slots_.size()) ? static_cast<uint32_t>(slots_.size() * 2)
                                                         : static_cast<uint32_t>(slots_.size());
        if (next_cap <= MEXT_SHADOW_LOG_CAP * 2) {
            Rehash(next_cap);
        } else {
            Rehash(static_cast<uint32_t>(slots_.size()));
        }
    }

    uint32_t idx = static_cast<uint32_t>(Hash(vpn)) & mask_;
    int32_t first_tomb = -1;
    for (uint32_t step = 0; step <= mask_; ++step) {
        uint32_t pos = (idx + step) & mask_;
        Slot &s = slots_[pos];
        if (s.state == OCCUPIED && s.key == vpn) {
            s.val = seq;
            return;
        }
        if (s.state == TOMBSTONE && first_tomb < 0) {
            first_tomb = static_cast<int32_t>(pos);
        }
        if (s.state == EMPTY) {
            uint32_t target = (first_tomb >= 0) ? static_cast<uint32_t>(first_tomb) : pos;
            if (first_tomb < 0) {
                ++used_;
            }
            slots_[target].key = vpn;
            slots_[target].val = seq;
            slots_[target].state = OCCUPIED;
            ++size_;
            return;
        }
    }
}

bool FlatVpnHashTable::Lookup(uint64_t vpn, uint64_t *seq_out) const
{
    uint32_t idx = static_cast<uint32_t>(Hash(vpn)) & mask_;
    for (uint32_t step = 0; step <= mask_; ++step) {
        const Slot &s = slots_[(idx + step) & mask_];
        if (s.state == EMPTY) {
            return false;
        }
        if (s.state == OCCUPIED && s.key == vpn) {
            if (seq_out) {
                *seq_out = s.val;
            }
            return true;
        }
    }
    return false;
}

bool FlatVpnHashTable::Erase(uint64_t vpn, uint64_t *seq_out)
{
    uint32_t idx = static_cast<uint32_t>(Hash(vpn)) & mask_;
    for (uint32_t step = 0; step <= mask_; ++step) {
        Slot &s = slots_[(idx + step) & mask_];
        if (s.state == EMPTY) {
            return false;
        }
        if (s.state == OCCUPIED && s.key == vpn) {
            if (seq_out) {
                *seq_out = s.val;
            }
            s.state = TOMBSTONE;
            --size_;
            return true;
        }
    }
    return false;
}

void SwapShadowMap::EnsureLogCapacity(PidShadow *ps)
{
    if (ps->log.empty()) {
        ps->log.resize(MEXT_SHADOW_LOG_INIT_CAP);
        ps->log_mask = MEXT_SHADOW_LOG_INIT_CAP - 1U;
        return;
    }
    if (ps->log.size() < MEXT_SHADOW_LOG_CAP && ps->next_seq >= ps->log.size()) {
        uint32_t new_cap = static_cast<uint32_t>(ps->log.size() * 2U);
        if (new_cap > MEXT_SHADOW_LOG_CAP) {
            new_cap = MEXT_SHADOW_LOG_CAP;
        }
        uint32_t new_mask = new_cap - 1U;
        std::vector<EvictLogEntry> grown(new_cap);
        for (const auto &e : ps->log) {
            if (e.active && e.vpn != 0) {
                grown[e.seq & new_mask] = e;
            }
        }
        ps->log = std::move(grown);
        ps->log_mask = new_mask;
    }
}

uint64_t SwapShadowMap::RecordEviction(int32_t pid, uint64_t vpn, uint64_t epoch)
{
    if (pid <= 0 || vpn == 0) {
        return 0;
    }
    PidShadow &ps = pids_[pid];
    EnsureLogCapacity(&ps);

    /* If this VPN already had an older swap entry, retire it */
    uint64_t prev_seq = 0;
    if (ps.vpn_to_seq.Lookup(vpn, &prev_seq)) {
        EvictLogEntry &old_e = ps.log[prev_seq & ps.log_mask];
        if (old_e.seq == prev_seq) {
            old_e.active = false;
        }
    }

    uint64_t seq = ps.next_seq++;
    EvictLogEntry &slot = ps.log[seq & ps.log_mask];
    if (slot.active && slot.vpn != vpn) {
        ps.vpn_to_seq.Erase(slot.vpn);
    }

    slot.vpn = vpn;
    slot.epoch = epoch;
    slot.seq = seq;
    slot.active = true;
    ps.vpn_to_seq.Upsert(vpn, seq);

    /* Clear from recent-prefetch filter since page is freshly evicted again */
    uint32_t pf_slot = static_cast<uint32_t>(vpn * 2654435761ULL) & MEXT_PREFETCH_FILTER_MASK;
    if (ps.recent_prefetch[pf_slot] == vpn) {
        ps.recent_prefetch[pf_slot] = 0;
    }

    return seq;
}

bool SwapShadowMap::OnPageFault(int32_t pid, uint64_t vpn, uint64_t *evict_seq_out, uint64_t *epoch_out)
{
    auto it = pids_.find(pid);
    if (it == pids_.end()) {
        return false;
    }
    PidShadow &ps = it->second;
    if (ps.log.empty()) {
        return false;
    }
    uint64_t seq = 0;
    if (!ps.vpn_to_seq.Erase(vpn, &seq)) {
        return false;
    }
    EvictLogEntry &e = ps.log[seq & ps.log_mask];
    uint64_t epoch = 0;
    if (e.seq == seq) {
        epoch = e.epoch;
        e.active = false;
    }
    if (evict_seq_out) {
        *evict_seq_out = seq;
    }
    if (epoch_out) {
        *epoch_out = epoch;
    }
    return true;
}

bool SwapShadowMap::ResolveEvictNeighbor(int32_t pid, uint64_t base_seq, int32_t delta,
                                         uint64_t *vpn_out, uint64_t *epoch_out) const
{
    if (base_seq == 0 || delta == 0) {
        return false;
    }
    int64_t target = static_cast<int64_t>(base_seq) + static_cast<int64_t>(delta);
    if (target <= 0) {
        return false;
    }
    auto it = pids_.find(pid);
    if (it == pids_.end() || it->second.log.empty()) {
        return false;
    }
    const PidShadow &ps = it->second;
    uint64_t tseq = static_cast<uint64_t>(target);
    const EvictLogEntry &e = ps.log[tseq & ps.log_mask];
    if (!e.active || e.seq != tseq || e.vpn == 0) {
        return false;
    }
    if (vpn_out) {
        *vpn_out = e.vpn;
    }
    if (epoch_out) {
        *epoch_out = e.epoch;
    }
    return true;
}

bool SwapShadowMap::IsSwappedOut(int32_t pid, uint64_t vpn) const
{
    auto it = pids_.find(pid);
    if (it == pids_.end()) {
        return false;
    }
    return it->second.vpn_to_seq.Lookup(vpn, nullptr);
}

bool SwapShadowMap::WasRecentlyPrefetched(int32_t pid, uint64_t vpn) const
{
    auto it = pids_.find(pid);
    if (it == pids_.end()) {
        return false;
    }
    uint32_t pf_slot = static_cast<uint32_t>(vpn * 2654435761ULL) & MEXT_PREFETCH_FILTER_MASK;
    return it->second.recent_prefetch[pf_slot] == vpn;
}

void SwapShadowMap::MarkPrefetched(int32_t pid, uint64_t vpn)
{
    if (pid <= 0 || vpn == 0) {
        return;
    }
    PidShadow &ps = pids_[pid];
    uint32_t pf_slot = static_cast<uint32_t>(vpn * 2654435761ULL) & MEXT_PREFETCH_FILTER_MASK;
    ps.recent_prefetch[pf_slot] = vpn;
}

void SwapShadowMap::RemovePid(int32_t pid)
{
    pids_.erase(pid);
}

void SwapShadowMap::Clear()
{
    pids_.clear();
}

uint64_t SwapShadowMap::TotalActivePages() const
{
    uint64_t total = 0;
    for (const auto &kv : pids_) {
        total += kv.second.vpn_to_seq.Size();
    }
    return total;
}

uint64_t SwapShadowMap::TotalEvictionsForPid(int32_t pid) const
{
    auto it = pids_.find(pid);
    if (it == pids_.end() || it->second.next_seq <= 1ULL) {
        return 0;
    }
    return it->second.next_seq - 1ULL;
}

} // namespace mext
} // namespace smap
