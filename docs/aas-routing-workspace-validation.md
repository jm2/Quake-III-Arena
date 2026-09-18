# AAS routing start workspace and failed cache creation — 2026-09-18

The native area-cache update uses a fixed 128-entry start array but consumes
one element for every incoming reversed link. Size checked heap scratch from
the incoming degree, supporting native worlds with 129 or more predecessors.
Reject unrepresentable/nullable workspace costs before cache/update mutation,
release scratch physically, and remove its retained pointer from the start
entry. A zero-degree update needs no allocation. No hard map-size cap is added.

Report area/portal update failure before publishing new caches. Nullable cache
imports do not change byte accounting; failed updates release unfinished cache
owners and preserve existing table/LRU entries. Nested portal failures clear
pending queue flags and propagate through all route callers. Retry can rebuild
the cache instead of reusing an empty result. Internal status returns preserve
botlib exports, QVM syscall ABI, native cache layouts and commercial 1.32c data.

## Validation

Three original full-body proofs fail: the fixed start array overruns at 129
incoming links; an initial heap-workspace failure publishes a blank cache; and
a nullable cache import dereferences NULL. The failure fixtures execute actual
providers/update/routing bodies, using only allocator/clock/query seams.

Twenty-two degree/filter goldens cover 0, 1, 2, 32, 127, 128, 129, 255, 256, 512
and 1,024 distinct predecessor areas, each with one outgoing cache-byte index.
Native destination/source times remain 1/11, disabled areas stay unreachable,
exact scratch length/lifetime matches degree, and no freed start pointer remains.
Four invalid counts and one nullable workspace leave cache/update bytes unchanged.

Cold/warm cache goldens retain native payload offsets/costs. Four area failure
cases preserve empty/existing cache tables, physical owners, counters and LRU
bytes; three cold nested portal failures leave no owner. A failure after two
portal updates are queued retains a valid nested cache, clears pending flags and
retries both sides with literal costs 12/22. Three public route paths handle
nullable providers. All cache owners release physically through native LRU free.

Clang normal/release fast-math sanitizers, optimized GCC, inherited travel-cost/
isolated-routing tests, final portal checks, review-ledger/Bash/diff checks pass.
Both final Retro68 products rebuild with zero compiler diagnostics and PPC PEFs:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,758,211 | `20e37e7ba85fce3433e115e54cc9c9fb467e357b0d5e6da8f54c15db5b4a07cd` |
| Quake3_TeamArena | 3,906,785 | `caaf8fff599bcccdbc1f3ec2469da48c36ee31539abe7935a1a2f433e38f142d` |

The source includes final parent portal/dummy ownership and travel-cost fixes.
Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #47 open for routing cache/derived allocation arithmetic and aggregate work/
memory budgets, cache-file grammar, outgoing byte-index capacity, runtime query
arguments, complete geometry/movers and transactional replacement/physical hunk
recovery. Retail and Mac OS 9 execution remain deferred; bots remain disabled.
