# Actual ZIP metadata I/O and error propagation — 2026-09-18

The selected-entry setter marked malformed metadata invalid but returned success.
Real-file truncation tests also expose primitive two/four-byte readers ignoring
fread results and decoding incomplete stack values. Signed decoding widened
unsigned ZIP fields incorrectly, especially on the LP64 host fixture.

Require complete scalar reads, initialize failed outputs to zero, and preserve
unsigned two/four-byte ZIP values before widening. Return the setter's actual
decoder error. Native valid metadata and commercial 1.32c archive formats,
public layouts/imports/syscalls/protocols remain unchanged. No CRC policy change.

## Validation

Actual decoder/filesystem bodies use real ZIP32 files. After a valid mount,
separate writable descriptors corrupt the central signature or truncate the
central metadata completely/partially. The real stdio input cache is discarded;
no parser return is fabricated. Invalid selected positions and exact scalar
short reads also reject. Unsigned 16/32-bit boundary values decode exactly.
Native stdio fixtures explicitly create/unlink disk-backed owners under
`${TMPDIR:-/var/tmp}` and close them after use.

Public unique opens reject changed metadata before entry allocation/open, clear
their candidate handle and release their private clone/OS descriptor. A complete
prior buffered owner/payload/cursor remains. With an active shared reader, its
complete archive/decoder bytes and physical FILE position also remain unchanged.
Restoring the actual archive bytes permits a native retry and both prior readers
continue their expected payload. Physical zone/temporary/mount/decoder owners,
load stack, handle records and OS descriptor counts return to baseline.

The unchanged preceding decoder body at `1941de0` fails all nine isolated
metadata/scalar cases in four normal/optimized-fast Clang/GCC combinations:
36 demonstrated failures. Four separate unchanged-source valid position/name/
length/buffered-payload and ordinary scalar goldens pass. No original production
body is rewritten around the defects.

Fixed checks pass all four compiler/modes without diagnostics. Clang runs
ASan/UBSan; local GCC runs optimized because its installed sanitizer runtime is
unavailable. Parent real ZIP/handle/read/seek suites, five ledger/manifest checks,
Bash syntax and diff checks pass. CI runs both compilers with sanitizers in the
existing host job.

Both PPC products build without diagnostics and have valid PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,783,927 | `448d19d98796e8f8c7e6883569f84945343cffae4cd8d944f6ccd298c376f6db` |
| Quake3_TeamArena | 3,936,597 | `e3a506b0500b275a10a6e20f55cdb725c1df399c6420506d6cf3c15288cad7fe` |

Source includes the preceding ZIP/read/seek/handle steps and this complete
metadata I/O fix. The parent's subsequent scratch-fixture correction changes
tests only. Toolchain libraries follow [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #36 open for remaining open/read/seek/metadata paths and merged CI/review
gates. Retail PK3/Mac OS 9 execution remains explicitly deferred. Scalar maximum
evidence does not replace complete huge payloads or aggregate resource budgets.
