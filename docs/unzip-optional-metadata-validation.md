# Bounded optional ZIP metadata — 2026-09-18

The local extra-field reader calculated the caller-limited count but passed the
complete field length to `fread`, overwriting short destinations. It also omitted
the archive prefix from its local-header offset. Read exactly the limited count
from the prefix-adjusted position. Reject a positive global-comment request with
a NULL destination before seeking or dereferencing it; zero-length NULL reads
retain their native result.

## Validation

Actual whole filesystem/decoder bodies mount stored and deflated ZIP32 archives,
with and without a 23-byte leading prefix. Real local extras contain a four-byte
header and 16-byte payload. Exact one-, four- and twenty-byte destinations retain
guard bytes; full and repeated reads preserve native query/count semantics.
Truncated local extras report I/O failure without touching destination tails.
NULL global-comment calls preserve the physical FILE cursor, while short and
complete valid comment reads retain counted unterminated/terminated behavior.
Active payload cursors continue through EOF and all physical owners/descriptors
release.

The unchanged parent decoder at `4d648eb` demonstrates 24 isolated overwrite,
prefix and NULL-comment failures in all four compiler/modes. Eight separate
stored/deflated valid optional-metadata goldens pass. Normal/optimized-fast Clang
ASan/UBSan and optimized local GCC pass the fix without diagnostics. All eleven
affected parent ZIP/metadata/decoder/handle suites, five ledger/manifest checks,
Bash syntax and diff checks pass. CI runs both sanitized compilers inside the
existing host job.

Both PPC products build without diagnostics and have valid PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,788,023 | `a53e56f542e3e32cdb5491168e39694cb5aa285afdb2f4685e783cd33b8fb389` |
| Quake3_TeamArena | 3,936,597 | `70e96072026e489420ba96e38e0c480ae4b2fbac0843acc42c802deb640176f6` |

Source includes preceding ZIP transaction/ownership steps and these optional
metadata bounds. Toolchain libraries follow
[loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #36 open for remaining read/seek paths and merged CI/review/dependency
gates. Retail PK3/Mac OS 9 execution is explicitly deferred. Commercial 1.32c
packed layouts, public imports/syscalls/protocols, valid metadata behavior and
archive/CRC policies stay intact.
