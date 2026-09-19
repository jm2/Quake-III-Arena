# Complete native ZIP mount metadata — 2026-09-18

FS_LoadZipFile used strlen on names for which the legacy unzip API supplies no
terminator when the name fills its 256-byte output. It also accepted partially
parsed central directories and incremented the shared file count before a
complete candidate existed. Reject names that cannot include their terminator,
embedded NUL aliases, failed metadata/iteration calls and unrepresentable payload
or shared-count costs. Check both source passes and the measured name capacity.
Publish the shared count only after complete native hash/position/checksum data.
Failure closes the candidate stream and frees every private mount allocation.

Keep native entry order, lowercase paths, supported 255-byte names, empty entries,
hash capacity and CRC/pure-checksum construction. Commercial 1.32c PK3/game/syscall/
network interfaces and legacy decoder CRC policy remain unchanged.

## Validation

The fixture executes actual filesystem and embedded decoder bodies against real
Python-generated ZIP32 files. Eight invalid cases cover 256-byte names with and
without a valid prefix entry, embedded NUL names, malformed first/second central
records, inconsistent declared counts, truncated filename metadata and the native
shared-counter limit. Each retains the prior root, complete mount/header/name/hash
bytes, independent buffered handle/payload, file count and physical owner count,
then retries successfully. Actual OS descriptor inventories prove stream release.

Three further cases truncate an actual candidate file at real engine allocation
imports after the first pass measures it. The hook observes the native stream
allocation and discards its host buffered input; it does not replace metadata
parsing, iteration or failure returns. The second pass reads the truncated file.
Every candidate allocation and OS descriptor releases, prior complete mount and
buffer bytes remain, and a valid source retries. This is a host source-change
fault fixture, not a claim that Mac OS 9 concurrent filesystem behavior was tested.

The unchanged preceding filesystem body fails all eleven cases across normal/fast
Clang and optimized GCC, for 44 demonstrated failures. Failures are the expected
rejection/cleanup assertions, Clang stack-buffer overflows or signed-counter
overflow. Four separate original native goldens pass, checking all four entry
positions/hash membership, full 255-byte lowercase name, stored/deflated payloads,
empty entries, supported prefixed empty archives and native CRC/pure checksums.
The independent payload CRC golden is Python zlib CRC32 `0xb070ee60`.

Fixed normal/optimized-fast Clang ASan/UBSan and optimized GCC checks pass without
diagnostics. Parent decoder and buffered-handle fixtures pass both compilers.
Five ledger/manifest unit checks, Bash syntax and diff checks pass. CI runs both
compilers inside the existing host job.

Both PPC products build with zero diagnostics and valid PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,783,927 | `8a11ebb0a0d0379eb4ca8f4661e3dc23cf780463564e552967743ea607d09bb1` |
| Quake3_TeamArena | 3,932,501 | `67f38d21d1b2cd41ddf8e1436945ff2986f1303e6452430b8dfdaf257ad73c1e` |

Source is the `a036478` baseline plus the preceding decoder/buffered-slot changes
and this mount change. Temporary libraries follow
[loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #36 open for entry-size cap/`INT_MAX`/`UINT32_MAX` and truncated compressed
payload fixtures and remaining open/read/cleanup paths. Retail PK3s and Mac OS 9
execution remain explicitly deferred. Host mount evidence does not close them.
