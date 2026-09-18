# Active ZIP decoder replacement — 2026-09-18

Opening an entry again closed its active decoder before header validation or
candidate allocation. A bad local header or failed import therefore destroyed
the prior reader. Stage the complete replacement first, release the old decoder
only on success, and restore the prior physical FILE position on failure.
Private candidate owners release without changing the active decoder.

## Validation

Actual whole filesystem/decoder bodies read complete real ZIP32 entries. The
fixture checks six compressed initialization imports, two stored imports and
corrupted local headers for both methods. It records prior physical allocation
identities, complete archive/decoder records, the complete 64 KiB input buffer,
FILE position and OS descriptor count. Failed replacement frees no prior owner
and preserves these bytes/cursors. The prior decoder then reads every remaining
payload byte through EOF, including repeated stored input refills; a successful
reopen resets the cursor and retries the native payload. All physical owners and
descriptors close.

The unchanged preceding decoder at `c73bd68` fails ten isolated cases in all
four compiler/modes: 40 demonstrated failures. Eight separate unchanged-source
stored/deflated valid reopen goldens pass. Original bodies remain unchanged;
only the real engine allocation import can return NULL in these cases.

Fixed checks pass normal/optimized-fast Clang ASan/UBSan and optimized local GCC
without diagnostics. All ten affected parent native ZIP/handle/metadata/decoder
suites, five ledger/manifest checks, Bash syntax and diff checks pass. CI runs
both sanitized compilers within the existing host job.

Both PPC products build without diagnostics and have valid PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,788,023 | `25e820438523463d3510a98df298702656bde1d56cd8325b4e5a72bdf14e7e45` |
| Quake3_TeamArena | 3,936,597 | `b2f669b89d4181c121d4bf9e305973f3ef768e755e176e339cd20ecb224a0de7` |

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
