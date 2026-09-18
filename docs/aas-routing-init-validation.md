# AAS routing initialization costs and publication — 2026-09-18

The native pipeline forms derived array costs from area, cluster, portal,
reachability and reversed-link counts. Several products/sums can overflow
before engine imports; nullable allocations are used without checks. A
failed pipeline can also be marked initialized. The travel matrix interleaves
pointer rows and 16-bit costs, misaligning later pointers after odd cost counts.

Check each complete signed import cost before multiplication/allocation and
propagate every nullable stage failure. Paired allocations release their first
owner if the second fails. A failed pipeline releases partial routing owners,
leaves `initialized` false and disables `loaded`, preventing retries every frame.
Successful initialization keeps the native stage order, array counts and values.
The native frame entry writes a requested cache only after initialization succeeds;
pending reachability or failed initialization cannot serialize absent tables.
A pending save survives until a successful initialized frame, and native frame
return values/bookkeeping remain unchanged.
Put all travel-matrix pointer rows before its 16-bit cost data so each row stays
aligned without adding allocation bytes. The private in-memory layout changes;
retail AAS, QVM, syscall, protocol and optional native cache-file layouts do not.
The legacy outgoing reverse-link clipping behavior remains unchanged.

## Validation

Five original actual-body proofs fail: first-allocation null dereference,
unrepresentable area-cost/null use, fully backed 65,536-by-65,536 travel-matrix
signed multiplication overflow, an odd travel-cost count's misaligned
pointer store, and the original frame entry writing cleared cache tables after
nullable initialization failure. The fixture includes the entire actual routing source and
strictly extracts only the actual initialization continuation and frame bodies; native
heap, clock, absent optional cache and completed clustering interfaces are seams.
No production bounds/allocation/publication logic is replaced.

A valid small world builds ten native routing table owners and retains literal
reverse-link ownership, travel-matrix values, contents classification and route
time 12/reachability 2. Each of ten allocations is independently made nullable;
initialization stops, all partial owners physically release, no initialized
message or optional-cache lookup occurs, and the next frame does not allocate.
All ten failures also run through the actual frame entry with a pending cache
save; no cache writer or save-reset occurs on either that or the next frame.
Pending reachability delays a save safely; the next successful frame performs
the actual native version-two empty-cache write, closes it and resets the request.
An empty-dummy world retains valid initialization with six positive allocations
and null zero-capacity arrays. Seven unrepresentable derived shapes reject
before imports, including the fully backed large graph matrix. Native successful
cleanup releases all routing/cache owners and returns accounting to zero.

Clang normal/release fast-math ASan/UBSan and optimized GCC pass. Parent routing
workspace/time and cache-provider regressions pass with the combined source.
Review-ledger, Bash and diff checks pass. Both Retro68 products rebuild with zero
compiler diagnostics and valid PPC PEFs:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,762,301 | `dc3035bab8eeb8cc07447ab728cbf1d192f778e85acaffd208b966842f4652d6` |
| Quake3_TeamArena | 3,910,875 | `2d68da6be360abf942e44089b549c2c51ccbba3f7fb072937fac0105ce69bc61` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).
Source includes the checked/accounted native optional-cache reader.

## Remaining acceptance

Keep #47 open for aggregate native RAM/work budgets, remaining runtime query,
geometry/mover/recovery and legacy outgoing byte-index capacity roots. Routing
cache cvar float conversion and whole-world failure/arena recovery still need
review. This change releases derived routing allocations; it does not establish
a transaction for every raw AAS/world arena owner. Retail assets and Mac OS 9
execution remain deferred. Bots stay disabled through the remaining roots.
