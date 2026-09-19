# Complete native ZIP seek semantics — 2026-09-18

Shared ZIP seeks aborted at 64 KiB, reset before every smaller offset, ignored the
requested origin and left setter/open failures unchecked. Streamed current seeks
also continued after platform delegation, applying the offset twice. Record the
validated native entry length, calculate SET/CUR/END targets without full-width
arithmetic overflow, clamp to the entry, restart only for backward movement and
skip in checked 64 KiB chunks. Platform streaming delegation returns immediately.
The established commercial ZIP return convention (requested offset on success)
remains intact.

## Validation

Actual whole filesystem/decoder bodies mount complete 196,625-byte stored and
deflated ZIP32 entries. Isolated checks cover 64 KiB absolute seeks, current/end
origins in both directions, positive EOF clamping, unknown origins, `LONG_MIN`/
`LONG_MAX`, invalid selected-entry positions and a platform callback that
recursively enters the real filesystem seek exactly once. Every successful seek
checks tell position and deterministic native payload; rejected selection keeps
the prior decoder usable. All physical archive/decoder/FILE owners release.

The unchanged parent at `ed0bff8` demonstrates 64 isolated fatal, origin,
selection and double-delegation failures in all four compiler/modes. Eight
separate stored/deflated legacy small-absolute-seek goldens pass. Fixed checks
pass normal/optimized-fast Clang ASan/UBSan and optimized local GCC without
diagnostics. All twelve parent ZIP/metadata/decoder/handle suites, five ledger/
manifest checks, Bash syntax and diff checks pass. CI runs both sanitized
compilers inside the existing host job.

Both PPC products build without diagnostics and have valid PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,788,017 | `1ed9ae663256f3abc7e05a42167bdf726b9af3e8848ebadc24abc1f80361c4eb` |
| Quake3_TeamArena | 3,936,591 | `f20dc11bcd191b64bb9c34e2f3a21c20dd4d6aa0e926d65cc220bb86287c4a48` |

Source includes preceding ZIP transaction/ownership/metadata steps and these
seek semantics. Toolchain libraries follow
[loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #36 open for merged CI/review/dependency gates and retail PK3/Mac OS 9
execution, which is explicitly deferred. Commercial 1.32c packed layouts,
public imports/syscalls/protocols, valid seek return values and archive/CRC
policies stay intact.
