# AAS reachability spans and travel-dependent fields — 2026-09-18

The native loader publishes invalid reachability destinations and area spans.
Validate subtraction-checked per-area ranges, exclude dummy/zero-start spans
from real routing ownership and bound total referenced records by native
reachability table capacity. Check destination areas, finite start/end fields
and signed ordinary face/edge references before loaded publication.

Match native AAS_Optimize semantics: elevator, jump-pad and func-bob fields carry
mover/velocity/packed values and must not be reinterpreted as geometry indices.
Retain all 32 native travel slots, team/other upper flag bits and uint16 times.
No retail v4/v5 layout, commercial 1.32c syscall/QVM field, hard size cap or
allocator policy changes are introduced.

## Validation

Two original actual-loader proofs fail: an invalid destination and an invalid
per-area count still report success. New actual-body normal/release fast-math
sanitizers cover 134 accepted type/team/signed/packed cases retaining every
payload byte, plus 106 malformed finite/reference/span/aggregate cases in both
versions. Three individually valid reused area spans exceed backing table
capacity and must reject before publication. Every rejection closes once and
clears partial logical ownership. Existing layout/geometry/node/endian/writer
and allocator seams remain passing. Optimized GCC normal/fast-math, nine Python,
Bash syntax and diff checks pass. Both Retro68 products build with zero compiler
diagnostics and validate as PPC PEFs.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,749,997 | `e6a1374948cc56cfe7ebdf08d62d8dff1c39b697ed84ea59a97786170bbef726` |
| Quake3_TeamArena | 3,902,667 | `bf5baf0c3ad9c22398d62c0080d6bb7363581cb0068959969c06a661104a9004` |

After integrating the #123 review fixes through #124, all eight allocator
sanitizer configurations and node/reachability checks pass again. Both PPC
products rebuild with zero compiler diagnostics and unchanged artifact hashes.

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #47 open for portal/cluster references, derived numeric/runtime safety,
aggregate hunk/work budgets, physical arena recovery and transactional late
replacement, plus complete runtime mover/BSP entity validation. Packed mover
fields remain intact; dynamic model availability must be checked at use sites.
This step bounds total backing references, not complete routing memory or
processing cost. Retail assets and live Mac OS 9 execution stay deferred;
bots remain disabled through remaining roots.
