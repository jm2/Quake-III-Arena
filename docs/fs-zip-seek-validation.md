# Complete native ZIP seek semantics — 2026-09-18

Shared ZIP seeks aborted at 64 KiB, reset before every smaller offset, ignored the
requested origin and left setter/open failures unchecked. Streamed current seeks
also continued after platform delegation, applying the offset twice. Multiple
logical handles could observe whichever entry and cursor the shared archive had
most recently decoded. Record the validated entry length and a private logical
cursor per handle, reselect the requested entry before advancing, and skip in
checked 64 KiB chunks. SET/CUR/END targets clamp without full-width arithmetic
overflow; unrepresentable requested offsets reject before moving. Platform
delegation runs once and records its recursive result without changing the
callback ABI. The established commercial ZIP return convention (requested
offset on success) remains intact.

## Validation

Actual whole filesystem/decoder bodies mount pairs of complete 196,625-byte
stored and deflated ZIP32 entries. Isolated checks cover 64 KiB absolute seeks,
current/end origins in both directions, positive EOF clamping, unknown origins,
`LONG_MIN`/`LONG_MAX`, invalid selected-entry positions, interleaved shared
handles, and a platform callback that recursively enters the real filesystem
seek exactly once. Every successful seek checks logical tell position and
deterministic native payload; rejected selection keeps the prior decoder usable,
and delegated selection failures propagate. All physical archive/decoder/FILE
owners release.

The unchanged parent at `ed0bff8` demonstrates 80 isolated fatal, origin,
selection, shared-cursor and delegation failures in all four compiler/modes. Eight
separate stored/deflated legacy small-absolute-seek goldens pass. Fixed checks
pass normal/optimized-fast Clang ASan/UBSan and optimized local GCC without
diagnostics. All twelve parent ZIP/metadata/decoder/handle suites, five ledger/
manifest checks, Bash syntax and diff checks pass. CI runs both sanitized
compilers inside the existing host job.

Both PPC products build without diagnostics and have valid PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,788,017 | `ef8b908cbe3754cbc64a9271103ef6ec504ae707670de71e6ef2e4ae635dc471` |
| Quake3_TeamArena | 3,936,591 | `c02489ae2ba9a3dcdddc466d1cacd555487efddc9df87aaf8d1fcff15055104b` |

Source includes preceding ZIP transaction/ownership/metadata steps and these
seek semantics. Toolchain libraries follow
[loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #36 open for merged CI/review/dependency gates and retail PK3/Mac OS 9
execution, which is explicitly deferred. Commercial 1.32c packed layouts,
public imports/syscalls/protocols, valid seek return values and archive/CRC
policies stay intact.
