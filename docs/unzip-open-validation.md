# Complete native ZIP decoder initialization — 2026-09-18

unzOpenCurrentFile ignored inflateInit2 failures, published an incomplete decoder
and returned success. Reject failed initialization before publishing its root,
release the private read buffer/record, and propagate the actual error. Embedded
inflate already releases its own partial state; verify physical release through
each actual nullable allocator import. Preserve native successful stored/deflated
reads and commercial 1.32c interfaces and ZIP formats.

## Validation

Actual decoder/filesystem bodies read complete stored and deflated ZIP32 entries.
The native zone-import fixture returns NULL separately at all six compressed
initialization positions: record, input buffer and four embedded inflate owners.
Each direct/public shared-open failure publishes no decoder root, clears its
candidate handle, frees every private physical owner and retains OS descriptor
counts. Complete prior buffered owner/payload/cursor bytes remain intact. Native
retry returns the full expected payload; prior partial reads still work and
teardown clears every handle, mount, decoder and temporary owner.

The unchanged preceding decoder body at `2bd8a32` reports success for four failed
inflate positions in both direct/public paths and all four compiler/modes:
32 demonstrated failures. Eight separate unchanged-source stored/deflated native
goldens pass ordinary initialization, partial payload, EOF and physical close.
The first two native initialization failure paths already rejected; fixed checks
cover those paths too. Parser/open/read/close bodies are not replaced.

Fixed checks pass normal/optimized-fast Clang ASan/UBSan and optimized local GCC
without diagnostics. Twelve nullable positions per combination cover all six
direct/public imports. Parent real ZIP/metadata/handle/read/seek suites, five
ledger/manifest checks, Bash syntax and diff checks pass. CI runs both compilers
with sanitizers inside the existing host job.

Both PPC products build without diagnostics and have valid PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,783,927 | `132b0e7d508f9a5a9607f4458da87f999bb1ec9e346753f19bd7f8cd0daab827` |
| Quake3_TeamArena | 3,936,597 | `3b2811dd9bdcfb6fcd4972ad6ff9232de18fcd1905f211fd50995be1af6c3f5f` |

Source includes the preceding ZIP/read/seek/handle/metadata steps and this complete
decoder initialization check. Toolchain libraries follow
[loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #36 open for further archive/buffer factories, active decoder replacement,
remaining open/read/seek paths and merged CI/review/dependency gates. The nullable
fixture verifies library failure control flow; native engine fatal allocation
behavior remains. Retail PK3/Mac OS 9 execution is explicitly deferred. No public
layout/import/syscall/protocol or archive-budget/CRC policy changes.
