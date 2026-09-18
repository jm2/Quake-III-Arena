# AAS routing cache allocation costs and byte accounting — 2026-09-18

Native cache counts are multiplied through unsigned sizeof arithmetic and
narrowed to a signed import size. Negative counts overflow pointer arithmetic;
large positive counts can wrap into undersized allocations. The signed cache
byte counter can overflow after an otherwise-valid allocation.

Bound counts and full native payload/header cost before multiplication or
allocation, then check the remaining signed byte-accounting range. Reject
negative/unrepresentable states before imports or mutation. Nullable imports
leave the counter unchanged. Preserve zero-count caches, native time/reachability
payload offsets, valid allocation costs and exact signed byte capacity. These
are target allocator/accounting limits; no arbitrary map-size cap is added.
Cache fields, botlib/QVM exports and commercial 1.32c data remain unchanged.

## Validation

Three original actual-body proofs fail: negative-count pointer arithmetic
triggers UBSan, a large positive count returns an undersized cache, and the
signed byte counter overflows at its last remaining byte.

Ten independent literal count/layout goldens cover 0, 1, 2, 127, 128, 129, 255,
256, 512 and 1,024 entries, including first/last cleared/writable time and byte
payloads. Native linking/free restores the byte counter and releases physical/
LRU ownership. Six invalid counts and four invalid byte budgets reject before
imports or mutations. Exact signed byte capacity remains supported. Three
large representable costs and one ordinary nullable import leave no owner or
accounting change; the fixture does not allocate their multi-gigabyte payloads.

Clang normal/release fast-math sanitizers, optimized GCC, integrated native
cache-provider failure/retry tests, review-ledger/Bash/diff checks pass.
Both Retro68 products rebuild with zero compiler diagnostics and PPC PEFs:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,758,211 | `1f83a23e64b0457fa467112cb5420a7e20f46f684e9e102ac20758a067cd184e` |
| Quake3_TeamArena | 3,906,785 | `66debdccc0b64e492cb0d0d53a127b0b71ddf39f9c58c263bfa6c64a130aec16` |

Source includes final parent ownership/travel-cost/workspace fixes.
Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #47 open for aggregate native memory/work budgets, reversed/travel-table
and other derived allocation costs, cache-file grammar, outgoing byte-index
capacity, remaining runtime query/geometry/mover/recovery and deferred retail/
Mac OS 9 execution. Signed representability does not establish a RAM/work budget.
Bots remain disabled through the remaining roots.
