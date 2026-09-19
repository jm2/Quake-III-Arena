# Native filesystem read and seek bounds — 2026-09-18

Buffered reads added a requested length to their current position before
clipping. A valid partial read followed by `INT_MAX` overflowed that sum and
could copy beyond the actual payload. Negative stream lengths became unsigned
decoder requests, and the requested-byte statistic could overflow independently.
Buffered seeks narrowed a full-width offset or added it before clipping.

Clip reads against the remaining bytes without adding the request. Reject
nonpositive lengths, missing destinations and invalid/free handles before
access or accounting. Saturate the private statistic at `INT_MAX`. Clamp seek
offsets before narrowing or adding; preserve native ordinary SET/CUR/END,
out-of-range clipping, unknown-origin failure, return values and cursors.
Commercial 1.32c signatures, packed types, imports and protocols stay unchanged.

## Validation

Actual filesystem and embedded decoder bodies read a real compressed ZIP32
entry. Eight isolated cases cover a partial-buffer `INT_MAX` request, negative
stream request, saturated requested-byte accounting, positive int-limit relative
seek and full-width positive/negative SET/CUR/END offsets. Invalid/free/reserved
handle and missing-output cases preserve every byte of the prior handle,
payload, destination and statistic. Teardown releases physical mount, buffer
and decoder owners.

The unchanged preceding filesystem body at `63fe336` fails all eight cases in
normal/optimized-fast Clang and optimized GCC, for 32 demonstrated failures.
Clang reports actual signed read/accounting/seek overflow; optimized GCC's large
buffer request faults. The remaining failures are payload/accounting/cursor
assertions. Four separate unchanged-source native goldens pass ordinary partial
reads, SET/CUR/END, EOF, unknown origins, requested-byte accounting and ordinary
clipping. Original source is not rewritten around the defects.

Fixed checks pass all four compiler/mode combinations without diagnostics.
Clang runs ASan/UBSan; local GCC runs optimized because its installed sanitizer
runtime is unavailable. CI runs both with sanitizers inside the existing host
job. Parent ZIP fixtures, five ledger/manifest checks, Bash syntax and diff
checks pass.

Both PPC products build with zero diagnostics and valid PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,783,927 | `483927c3dc0e406a49acfba0873edfeec41a803e4d35560c8b23628a0dbc59f1` |
| Quake3_TeamArena | 3,932,501 | `a9efc8837daddbfe5135b2e3a1bc2beb82c0399cb7cad99055438df39389ca66` |

Product source includes the preceding ZIP ownership/metadata/cursor steps and
these read/seek bounds. The parent entry-acceptance step changes tests only.
Toolchain libraries follow [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #36 open for remaining handle/metadata/open/read paths and merged CI/review
gates. Retail PK3/Mac OS 9 execution remains explicitly deferred. These checks
do not impose a new archive budget or change the legacy CRC policy.
