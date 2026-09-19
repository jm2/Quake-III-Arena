# Native filesystem handle operation bounds — 2026-09-18

The private FILE lookup accepted handle 64 into a 64-element table. Close,
tell and length accessed handles before validation. Flush treated an embedded
ZIP decoder as a FILE, and buffered force-flush raised a spurious engine error.
Write passed negative lengths to unsigned stdio requests.

Check exclusive handle bounds and live ownership before table access. Missing
close/flush owners become no-ops; invalid tell/length return -1 and writes return
zero. Flush and writes require an ordinary FILE; buffers and ZIP decoders never
enter stdio. Reject missing destinations and nonpositive write lengths before
I/O. The private FILE lookup retains its native ERR_DROP contract for invalid
or ZIP handles. Ordinary file operations and commercial 1.32c interfaces stay
unchanged.

## Validation

Actual filesystem/decoder bodies use a real compressed ZIP32 entry and actual
stdio temporary files. Buffered/shared tell and complete payload checks retain
their native partial cursor. Ordinary writes/sync/flush, SET seek, length queries,
readback and close work in the final usable native slot. The actual OS descriptor
is closed. Physical buffers, mount/decoder owners and complete handle records
release on teardown.

The unchanged preceding filesystem body at `7c188f1` fails 28 isolated cases:
all eight in both Clang modes and six in each optimized GCC mode. Clang detects
actual negative/exclusive table accesses and an invalid ZIP stdio pointer; GCC
faults or reaches error assertions. Two undefined table accesses happen to return
without a visible GCC failure and are not counted as demonstrated failures.
Four separate unchanged-source native buffered/shared/ordinary FILE goldens pass.

Fixed checks pass normal/optimized-fast Clang ASan/UBSan and optimized local GCC
without diagnostics. Invalid/reserved/free/extreme handles, buffered/shared flush,
double close and missing/nonpositive write inputs preserve prior owners/cursors
and consume no output. A native error hook observes actual private ERR_DROP;
filesystem and decoder bodies are not replaced. Parent ZIP/read/seek suites,
five ledger/manifest checks, Bash syntax and diff checks pass. CI runs both
compilers with sanitizers inside the existing host job.

Both PPC products build without diagnostics and have valid PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,783,927 | `dd66f043a561e0db09c03d7f9af5f12c19eedc22d04e285458cef9d987283609` |
| Quake3_TeamArena | 3,936,597 | `74cc44ed2654ff24a0e168c7371c07d9db265fce9fd58beb68c0b8372071889e` |

Product source includes the preceding ZIP/read/seek steps and these handle
operation checks. Toolchain libraries follow
[loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #36 open for remaining metadata/open/read/seek paths and the merged CI/review
gate. Retail PK3/Mac OS 9 execution remains explicitly deferred. No packed layout,
public signature/import/syscall/protocol or archive-budget/CRC policy changes.
