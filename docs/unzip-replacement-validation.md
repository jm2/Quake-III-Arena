# Active ZIP decoder replacement — 2026-09-18

Opening an entry again closed its active decoder before header validation or
candidate allocation. A bad local header or failed import therefore destroyed
the prior reader. Stage the complete replacement first, release the old decoder
only on success, and restore the prior physical FILE position on failure.
Private candidate owners release without changing the active decoder.

## Validation

Actual whole filesystem/decoder bodies read complete real ZIP32 entries. The
fixture checks six compressed initialization imports, two stored imports and
corrupted local headers for both methods. It covers unchanged selection, next-entry traversal and exact position setters, and records prior physical allocation identities, complete archive/decoder records, the complete 64 KiB input buffer, FILE position and OS descriptor count. Failed replacement frees no prior owner
and preserves these bytes/cursors. The prior decoder then reads every remaining
payload byte through EOF, including repeated stored input refills; a successful
reopen resets the cursor and retries the native payload. All physical owners and
descriptors close.

The unchanged preceding decoder at `c73bd68` fails ten isolated cases in all
four compiler/modes: 40 demonstrated failures. The reviewed `4d648eb` head additionally fails 80 next-entry/position-setter cases because refills depend on changed archive metadata. Eight separate native goldens pass on each unchanged source. Original bodies remain unchanged;
only the real engine allocation import can return NULL in these cases.

Fixed checks pass normal/optimized-fast Clang ASan/UBSan and optimized local GCC
without diagnostics. All ten affected parent native ZIP/handle/metadata/decoder
suites, five ledger/manifest checks, Bash syntax and diff checks pass. CI runs
both sanitized compilers within the existing host job.

Both PPC products build without diagnostics and have valid PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,788,023 | `df0015f5a72248b3c21012398bd7f60261059d5f2c0666c9acb19f9984c6117b` |
| Quake3_TeamArena | 3,936,597 | `3beaeff770c89f6ed6db9cd6c2913bad6cf07787f91e4952bff21db86f641082` |

Source includes the preceding ZIP/metadata/decoder/handle steps and this
replacement transaction. Toolchain libraries follow
[loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #36 open for remaining metadata/read/seek paths and merged CI/review/
dependency gates. Nullable imports check library control flow; native engine
fatal allocation behavior remains. A failed OS cursor restoration reports an
I/O error; continuation requires an intact seekable archive. Retail PK3/Mac OS 9
execution is explicitly deferred. Commercial 1.32c packed layouts, public
imports/syscalls/protocols, successful reopen behavior and archive/CRC policies
stay intact.
