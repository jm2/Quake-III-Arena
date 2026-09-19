# Transactional public unzip APIs — 2026-09-19

Several public unzip entry points dereferenced required caller pointers without
validation. A positive-length `unzReadCurrentFile` accepted a null destination,
and a failed `unzLocateFile` restored only the numeric entry and central-directory
position. The latter left the cached metadata describing the last searched entry,
could leave the selection invalid, and moved the shared physical `FILE` cursor.
An archive filename containing an embedded NUL could also compare equal to its
visible prefix because lookup ignored the recorded ZIP filename length.

Required global-info, selected-position and lookup-name pointers now reject with
`UNZ_PARAMERROR` before access. A positive read requires an output buffer while a
zero-length null-buffer read retains its legacy no-op result. Filename lookup
compares the exact central-directory name length and, on every miss or parser
error, restores the prior entry number, central position, validity flag, public
and private metadata, and physical stream cursor. Successful lookup still selects
the matched entry.

No public function signature, structure layout, compression method, CRC policy,
PK3 layout, engine import, syscall or network protocol changes. Ordinary
commercial ZIP32 names and stored/deflated payloads retain their existing results.

## Validation

The regression compiles the actual filesystem and unzip bodies and creates real
stored and deflated two-entry PK3s. It checks valid global information, current
positions, successful lookup, decoder open/read/EOF/close, every required null
pointer, and both inactive and active missing-name searches. Failed searches must
preserve the complete `unz_s`, active decoder bytes and physical cursor; the
selected or partially read first entry must then continue through its exact
payload. A separately mutated central-directory filename contains an embedded NUL
and must not impersonate its shorter visible prefix.

The unchanged exact parent at `3c3d238` fails all seven isolated defect cases:
the three required pointer calls and positive null read stop under ASan/UBSan,
both missing-name cases expose selection/cursor changes, and the embedded-NUL
name aliases its prefix. An unchanged native success golden passes.

Fixed stored, deflated and malformed-name checks pass in normal and
optimized-fast modes under GCC and Clang ASan/UBSan. All fourteen ZIP,
filesystem-handle, metadata, decoder, ownership and seek runners also pass under
both compilers. Bash syntax, diff checks, and the five ledger/renderer-manifest
unit tests pass.

Both PPC products build without diagnostics and have valid PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,809,283 | `a846cec30e5fefcd7462f7e5f9cf269db65c75429259a10d1f2db3f804da6645` |
| Quake3_TeamArena | 3,957,857 | `0bde3906f380280fdc2b770120faba26f9f43e6a7764c8868e148fca2024df15` |

Source includes all preceding ZIP and network-hardening steps. Toolchain library
loading follows [the QVM loader evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #36 open for central-directory discovery/navigation/open arithmetic, merged
CI/review/dependency gates, and retail PK3 execution on Mac OS 9. Live target
testing remains explicitly deferred. Host fixtures do not replace full retail
archive, HFS and target-memory behavior.
