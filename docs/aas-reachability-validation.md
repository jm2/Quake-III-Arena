# AAS reachability spans and travel-dependent fields — 2026-09-18

The native loader publishes invalid reachability destinations and area spans.
Validate subtraction-checked per-area ranges, exclude dummy/zero-start spans
from real routing ownership and bound total referenced records by native
reachability table capacity. A temporary heap bitmap enforces disjoint ownership
in linear bounded work, preserving reordered nonoverlapping spans. Every non-dummy
record must be owned, matching the native AAS_StoreReachability writer. Check destination areas, finite start/end fields
and signed ordinary face/edge references before loaded publication.

Match native AAS_Optimize semantics: elevator, jump-pad and func-bob fields carry
mover/velocity/packed values and must not be reinterpreted as geometry indices.
Retain all 32 native travel slots, team/other upper flag bits and uint16 times.
No retail v4/v5 layout, commercial 1.32c syscall/QVM field, hard size cap or
allocator policy changes are introduced.

## Validation

Four original actual-loader proofs fail: invalid destination/counts, two areas
sharing one record, and an unowned packed mover record still report success. New actual-body normal/release fast-math
sanitizers cover 138 accepted type/team/signed/packed cases retaining every
payload byte, plus 118 malformed finite/reference/span/aggregate cases in both
versions. Three individually valid reused area spans exceed backing table
capacity and must reject before publication. Duplicate and partially overlapping
spans reject even when their aggregate counts fit; adjacent/reordered spans keep
all native bytes. Ownership-workspace failure rejects cleanly in both versions,
and both successful and failed overlap checks physically release the bitmap.
Unowned ordinary/elevator/jump-pad/func-bob records reject before bitmap
allocation in both versions; runtime model scanning can no longer consume them. Every rejection closes once and
clears partial logical ownership. Existing layout/geometry/node/endian/writer
and allocator seams remain passing. Optimized GCC normal/fast-math, nine Python,
Bash syntax and diff checks pass. Both Retro68 products build with zero compiler
diagnostics and validate as PPC PEFs.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,754,099 | `f5b8f78926a44298e8db60f690d5186a58d5a8513a5dc8d34e917e86acb0c90f` |
| Quake3_TeamArena | 3,902,673 | `2b118dc892bb15f47d8c36a4cf89e18860c73dae623207a2219266931e118a95` |

The table includes the inherited #123 native engine overhead fix and the
Codex complete-ownership follow-ups. Node/layout/geometry and nine Python checks
remain passing; both overlap modes pass Clang sanitizers and optimized GCC.

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #47 open for portal/cluster references, derived numeric/runtime safety,
aggregate hunk/work budgets, physical arena recovery and transactional late
replacement, plus complete runtime mover/BSP entity validation. Packed mover
fields remain intact; dynamic model availability must be checked at use sites.
This step bounds total backing references, not complete routing memory or
processing cost. Retail assets and live Mac OS 9 execution stay deferred;
bots remain disabled through remaining roots.
