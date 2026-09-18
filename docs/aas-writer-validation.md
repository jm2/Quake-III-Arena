# AAS writer world ownership and exact I/O — 2026-09-18

The runtime writes generated/optimized AAS data and then immediately initializes
routing in AAS_ContinueInit. On PowerPC the original writer swaps the live world
before opening the file and never restores it, including successful writes.
It also ignores payload/header write and final seek results.

Preflight all fourteen output counts, required buffers, typed byte lengths and
the aggregate signed file offset before opening. Preserve the native retail v5
header, obfuscation, field layouts, padding and order. Require exact writes and
successful header seek; after swapping, restore native data on every exit and
close the owned file once. No new retail format or commercial 1.32c interface
is introduced. No hard map size cap or allocator change is added.

## Validation

Three original actual-body proofs fail: missing output changes world bytes,
success changes world bytes and a short payload write reports success.
The new sanitizer runner checks every metadata and typed lump byte, independent
complete header/payload/padding output, empty/native layouts and repeated writes
under identity and byte-reversal helpers. It covers open failure, each of sixteen
header/payload write failures, final seek failure and every negative/overflow
count or missing nonempty buffer across all fourteen roots. Aggregate overflow
rejects before file effects even when the individual first lump is representable.
These are endian/ownership models; identity output matches literal retail bytes.
They do not claim PowerPC execution or valid traversable graph content.

Clang ASan/UBSan/float-cast-overflow, optimized GCC normal/fast-math and native
AAS layout/endian suites, nine Python checks, Bash syntax and diff checks pass.
Both Retro68 products build with zero compiler diagnostics and validate as PPC
PEFs.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,745,895 | `c4322cc29e9bb6d3bf9ca1aed9ce02995109270d698a3d83d91fe5a4f9f51e62` |
| Quake3_TeamArena | 3,894,469 | `8c509fc48392d91cfe916559e03d2d08f026df01ce80b529f5603289c7261d11` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #47 open for graph/reference/numeric validation before loaded publication,
aggregate arena/runtime budgets, transactional late-load replacement and
complete mover/entity checks. This change preserves the live world during
writing; it does not make file replacement atomic or reclaim hunk storage.
An I/O failure may leave an incomplete output file and is reported to the native
caller. Retail assets and live Mac OS 9 acceptance remain deferred.
