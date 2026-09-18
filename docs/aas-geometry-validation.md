# AAS geometric fields and reference ranges — 2026-09-18

The native loader publishes non-finite coordinates and out-of-range geometric
references. After endian conversion, validate every bbox/vertex/plane/area float
by representation, ordered bbox/area bounds, safe plane types, edge vertices,
signed edge/face indices and subtraction-checked face/area index spans. Require
area identity and both face-plane orientations for real-area geometry. Reject
INT_MIN before signed-reference negation. Failed geometry cannot publish loaded;
close once and clear all partial logical owners.

Retain native retail v4/v5 layouts, signed orientation, flag bits and all six
legacy plane types. No new asset, syscall or QVM field, hard map size cap or
allocator policy is introduced. Reachability fields have travel-type-dependent
meanings and remain for a separate routing audit.

## Validation

Two original actual-loader proofs fail: a non-finite vertex and an out-of-range
edge vertex still report success. The new runner loads independent literal
triangle/area data across 48 version/order/orientation/plane-type cases and
compares every retained typed payload byte. It tests all 47 geometric float
fields with six infinity/NaN patterns in both versions, plus bad types, vertex/
signed references, span/identity limits, INT_MIN, paired-plane limits and inverted
bounds: 708 malformed cases must reject before publication and clear logical
owners. Native dummy/empty layout fixtures remain supported at this step.

Clang sanitizer normal/release fast-math and optimized GCC normal/fast-math checks
pass. All native AAS layout/endian/writer seams, nine Python checks, Bash syntax and
diff checks pass. Both Retro68 products build with zero compiler diagnostics and
validate as PPC PEFs.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,749,997 | `f6404271315d4d6637904175c31ec960958ad00bdada46c63f87d6144dbfb463` |
| Quake3_TeamArena | 3,898,571 | `a2a2a3dc94151573800f4fa6ff5371da1578d2513894d2c0a680d34e819e37d0` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #47 open for node/root/leaf validation and graph termination, routing/
reachability/portal/cluster references, derived numeric/runtime safety and
aggregate hunk/work budgets, transactional late-load replacement and complete
mover/entity checks. Finite values can still overflow derived runtime math;
this step does not claim that audit complete. Logical hunk release does not
physically reclaim arena bytes. Retail assets and Mac OS 9 execution remain
deferred; bots stay disabled through the remaining root work.
