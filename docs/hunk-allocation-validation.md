# Permanent hunk allocation validation — 2026-09-18

This prerequisite for #45 makes permanent allocation costs available to map
preflight. Hunk_AllocationSize returns the native 32-byte cacheline cost,
including the actual hunkblock_t header in debug builds, or -1 when a request
cannot be represented. Hunk_Alloc uses that same cost and compares it with
Hunk_MemoryRemaining before changing banks or counters. Negative requests,
alignment/header overflow and insufficient capacity produce ERR_DROP.

Ordinary allocation addresses, zeroing, bank preferences and debug metadata
retain native behavior. No arbitrary map cap, commercial 1.32c file change,
module/syscall ABI change or protocol change is introduced.

## Validation

The actual unchanged allocator produces UBSan signed overflow for INT_MAX +
31. The new fixture compiles common.c with error/log implementations redirected
to isolated test imports; allocator bodies and state remain native. Both release
and HUNK_DEBUG modes pass ASan/UBSan: 0–257-byte requests on both banks,
independent 64-bit cost calculations, zeroed payloads and guard bytes, live temp
capacity, exact fits, existing allocations/debug ownership retained on failure,
negative requests, 64 signed-maximum alignment boundaries and near-INT_MAX
capacity accounting. Scratch sources and binaries are removed by the runner.

All 35 registered host sanitizer runners, nine Python checks, Bash syntax and
diff checks pass. Both Retro68 products build without compiler diagnostics and validate as PPC
PEFs, using temporary toolchain libraries from [loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,733,317 | `111fe50dad3f4489d1f0c19ba3a07c0564ab96f9d9e2b827381c8cc60be2ed4d` |
| Quake3_TeamArena | 3,881,891 | `fb1fcb0fc9bf075bcf1c1188e39e7cf5fffb8240f1354b21c8295cd93379ae6a` |

## Remaining acceptance

Keep #45 open. Aggregate map/query budgets, other consumed geometry validation
and full transactional publication remain. Temporary allocator arithmetic is a
separate remaining audit item. Retail commercial 1.32c and Mac OS 9 live
acceptance remains deferred to the follow-up session.
