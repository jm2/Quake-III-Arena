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

Loaded caches previously bypassed accounting; freeing them could make the byte
counter negative and prevent regeneration. The file reader also treated the
first word as a size, whereas the actual version-two writer stores the complete
native cache structure with `size` later in its header. Read that native header,
validate its exact world-dependent payload cost, bounds, floats and usable
reachability indices, and allocate through the same checked/accounted path.
Stage the complete dump before publication; rollback releases partial owners
and restores the counter. Discard every stored pointer, reconstruct payload
addresses and rebuild the table/LRU links. Close the file on every opened path.

Retain the writer's native version-two layout, scalar fields, CRCs and payload
values. This optional cache remains native to its writer's ABI and byte order;
incompatible or malformed dumps are ignored and routes regenerate. No retail
AAS, QVM, protocol or syscall layout changes.

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

Two additional original actual-body proofs fail: the native writer's dump is
misread as a zero-byte allocation, and reader-compatible entries are loaded
without byte accounting. The new file fixture uses the actual writer/reader and
native cache providers. Three valid paths cover an empty dump, a populated dump
and poisoned stored pointers. Loaded cross-cluster routes retain literal time
25/reachability 4; disabling/re-enabling an area regenerates that route without
negative accounting, leaks or foreign LRU links.

The 64-bit host dump has 414 bytes. All 414 truncations, ten malformed headers,
54 malformed record/float variants, two invalid cached reachability indices,
one trailing byte, eight nullable allocation cases, nine short reads and one
late cumulative budget failure reject (499 cases). Earlier staged entries are
physically released; preexisting cache bytes/table/LRU ownership stay intact.
Floating validation reads integer bits from memory, avoiding Clang fast-math's
finite assumption on floating function arguments. Both GCC and Clang normal/
release fast-math builds exercise these cases.

Clang normal/release fast-math sanitizers, optimized GCC, integrated native
cache-provider failure/retry tests, review-ledger/Bash/diff checks pass.
Both Retro68 products rebuild with zero compiler diagnostics and PPC PEFs:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,762,279 | `5f2fda45ee29519f94b62f139e9450e4f19ec3c76cc36413e924cc16baa85c09` |
| Quake3_TeamArena | 3,910,853 | `9bc0edcd77bc4b1e94e72d0e3d8cc6fe50e7517ef24cd33dca8de592b46228bf` |

Source includes final parent ownership/travel-cost/workspace fixes.
Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #47 open for aggregate native memory/work budgets, reversed/travel-table
and other derived allocation costs, aggregate optional-cache work/RAM budgets,
outgoing byte-index
capacity, remaining runtime query/geometry/mover/recovery and deferred retail/
Mac OS 9 execution. Signed representability does not establish a RAM/work budget.
Bots remain disabled through the remaining roots.
