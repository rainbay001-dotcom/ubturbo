# CLAUDE.md

Guidance for the `@claude` bot answering issues and reviews on this repo.

## openEuler kernel reference

When kernel-side context is needed (kernel API signatures, hook locations,
struct definitions, RFC behavior), read source from
`vendor/openeuler-kernel/` — the workflow checks out openEuler OLK-6.6
into that path before invoking Claude. Pinned to upstream OLK-6.6 HEAD
`e156f160bb75` (commit `!21981 Fix CVE-2026-31415`, 2026-04-28).

Subtrees most often relevant to ubturbo work:

- `vendor/openeuler-kernel/mm/` — page cache, swap, mempolicy, MMU
  notifiers, OOM, compaction. The bulk of ubturbo questions about
  memory-pressure hooks and NVMe-backed swap land here.
- `vendor/openeuler-kernel/kernel/sched/` — PSI, schedstats, cpufreq,
  fairness/EAS. Useful when a question references PSI thresholds or
  scheduler-driven memory pressure.
- `vendor/openeuler-kernel/fs/` — block-layer integration, filesystem
  hooks (e.g. when ubturbo crosses fs boundaries on swap files).
- `vendor/openeuler-kernel/include/linux/` — public kernel headers for
  the APIs ubturbo's modules link against.
- `vendor/openeuler-kernel/drivers/ub/` — UB stack (ubcore, uburma, hw/udma)
  if a question touches URMA / UDMA. Less common for ubturbo's core
  memory work but present.

## Refreshing the kernel reference

The pinned commit advances when the OLK-6.6 upstream gains relevant
fixes. To refresh, re-push the rainbay001-dotcom/openEuler repo from a
local OLK-6.6 clone (the chunked-push procedure: see
`Docs-repo/UMDK/umdk_udma_warmup_deployment.md` for the sibling pattern,
or just re-run the three `git push` chunks against an updated local
clone). The bot's cross-repo `actions/checkout` always pulls whatever
ref `OLK-6.6` points at; no workflow change needed when the pin moves.

## What the kernel reference is *not*

- Not a place to write to. The bot has read-only access; any patch
  proposals belong in ubturbo or in a dedicated kernel patch series.
- Not a complete or up-to-the-minute mirror of upstream openEuler. It
  reflects a known-good snapshot pushed manually. If the bot needs a
  newer commit than the pinned one, ask the maintainer to refresh.
