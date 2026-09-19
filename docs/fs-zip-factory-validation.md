# Native ZIP archive, clone and buffer factory failures — 2026-09-18

Archive/clone factories copied into unchecked allocation results. Unique buffer
allocation could pass NULL into decompression, and missing clone inputs reached
stdio/copy operations. Check these factories before access, close their private
FILE on allocation failure and initialize the complete private archive record.
Failed buffering closes its initialized decoder/private clone, clears the
candidate handle and returns native missing-file status.

## Validation

Actual filesystem/decoder bodies use complete real ZIP32 payloads and native
physical allocation imports. Both archive search/record positions, clone record,
idle unique buffer, active unique clone and active unique buffer are checked
separately. Missing paths/sources reject before stdio or allocation. Candidate
physical owners and OS descriptors release, prior buffered handle/payload/cursor
bytes remain intact, and active shared archive/decoder bytes and physical FILE
position remain unchanged. Native metadata/buffered/shared retries and prior
payload continuation pass.

The unchanged preceding filesystem/decoder bodies at `4c73c90` fail six isolated
factory/input cases in all four compiler/modes: 24 demonstrated failures.
Clang observes actual NULL copy/output operations; optimized GCC faults and also
diagnoses a NULL clone source during original compilation. Eight separate
unchanged-source stored/deflated archive/clone/buffered/full-file goldens pass.
Original bodies remain unchanged. Native free imports require a non-NULL tracked
owner, so an empty tracking slot cannot falsely accept a NULL free.

Fixed checks pass normal/optimized-fast Clang ASan/UBSan and optimized local GCC
without diagnostics. All affected parent real ZIP/metadata/decoder/handle/read/
seek suites, five ledger/manifest checks, Bash syntax and diff checks pass.
CI runs both compilers with sanitizers inside the existing host job. No native
archive/open/read/close bodies are replaced by fixture callbacks.

Both PPC products build without diagnostics and have valid PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,788,023 | `e133e69100a2bfa7cd5a4b0bb504464726103daee03e8826aaceb4b8d6517f82` |
| Quake3_TeamArena | 3,936,597 | `749ca6de62c22e8123679fe974b3c2ecc73f4331903f93560eb50d1a96bc6397` |

Source includes the preceding ZIP/metadata/decoder/handle steps and these factory
checks. Toolchain libraries follow [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #36 open for active decoder replacement, remaining open/read/seek paths
and merged CI/review/dependency gates. Nullable imports verify library control
flow; native engine fatal allocation behavior remains. Retail PK3/Mac OS 9
execution is explicitly deferred. Commercial 1.32c packed layouts, public
imports/syscalls/protocols, buffering and archive-budget/CRC policies stay intact.
