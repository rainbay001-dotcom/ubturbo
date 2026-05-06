# CLAUDE.md

Guidance for the `@claude` bot answering issues and reviews on this repo.

## openEuler kernel reference

`vendor/openeuler-kernel/` is checked out by the workflow before invoking
Claude. It contains a sparse-checkout subset of openEuler OLK-6.6 source
(see `.github/workflows/claude.yml`), pinned to upstream HEAD
`e156f160bb75` (commit `!21981 Fix CVE-2026-31415`, 2026-04-28).

### When to consult it

**Only** consult `vendor/openeuler-kernel/` when the question or PR diff
**explicitly mentions** one of:

- A kernel API signature (e.g., `reclaim_pages`, `folio_isolate_lru`)
- A kernel struct definition (e.g., `struct mm_struct`)
- A kernel hook location, tracepoint, or RFC-defined behavior
- An openEuler-specific feature (CONFIG_PSI_FINE_GRAINED, DAMOS_DEMOTION,
  the UB stack, etc.)

For routine code-comment / refactor / formatting / docs / build-system
tasks within ubturbo's own source, **do not** search the kernel reference.
The kernel reference is for context-lookup, not for reading material.

### How to consult it efficiently

When you do consult it:

- Use **targeted `Grep`** with specific symbol names you already know.
  Example: `Grep "reclaim_pages" vendor/openeuler-kernel/mm/`.
- Avoid blanket `Glob '**/*.c'` or full-tree searches across
  `vendor/openeuler-kernel/`. They're slow and rarely productive.
- The sparse subtrees that exist locally:
  `drivers/ub/`, `include/ub/`, `include/uapi/ub/`, `Documentation/ub/`,
  `mm/`, `kernel/sched/`, `include/linux/`, `MAINTAINERS`.
- If a question requires a kernel directory not in the sparse list (e.g.,
  `fs/`, `block/`, `arch/`, `net/`), state that the reference is
  insufficient and ask the maintainer rather than guessing.

### Refreshing the kernel reference

The pinned commit advances when the OLK-6.6 upstream gains relevant
fixes. To refresh, re-push the rainbay001-dotcom/openEuler repo from a
local OLK-6.6 clone (the chunked-push procedure: see
`Docs-repo/UMDK/umdk_udma_warmup_deployment.md` for a sibling pattern).
The bot's cross-repo `actions/checkout` always pulls whatever ref
`OLK-6.6` points at; no workflow change needed when the pin moves.

## What the kernel reference is *not*

- Not a place to write to. The bot has read-only access; any patch
  proposals belong in ubturbo or in a dedicated kernel patch series.
- Not a complete or up-to-the-minute mirror of upstream openEuler. It
  reflects a known-good snapshot, sparse-filtered to common subtrees.
  If you need a directory that isn't checked out, ask for a refresh.

## Scope discipline

When the user's prompt is broad (e.g., "add code comments", "refactor
this", "review this PR"), default to operating only within the changed
files / explicitly mentioned files. Do not expand scope to the whole
repository or to the kernel reference unless the prompt asks for it
or the changed code explicitly references kernel APIs.

