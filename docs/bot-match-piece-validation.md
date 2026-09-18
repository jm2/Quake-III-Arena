# Native match-piece ownership — 2026-09-18

BotLoadMatchPieces freed its caller's source on syntax failure, could return a
partial list at EOF without the closing delimiter, and dereferenced unchecked
persistent imports. It now owns checked private heap pieces/inline strings,
requires the delimiter and rejects sticky source errors. Every failure releases
partial pieces; the source remains borrowed. Both template/reply callers release
their source once after failure. Successful pieces remain owned by the existing
BotFreeMatchPieces path, which now physically releases their heap storage.

Keep native piece/alternative order, variable slots, empty-alternative adjacency
rules, captured offsets/lengths, both delimiters, successful template/reply
lookups and selected-line timing. Public imports, syscall tables and retail
1.32c formats are unchanged.

## Host and product evidence

- Actual whole chat/parser/source/allocator bodies run in
  [the match fixture](../tests/bot_match_piece_regression.c), using the existing
  physical-owner imports rather than allocator stubs that hide storage.
- Thirty nullable parser/piece/string positions across fresh/prior lists reject,
  release private pieces and buffered source owners, preserve prior record/pointer
  bytes and matching, then retry successfully. Callers retain their source until
  releasing it; successful template/reply loads and public lookup/construction
  also release through normal shutdown.
- Unterminated, invalid-slot/adjacency/alternative/token, lexical/directive and
  missing source/delimiter cases reject without replacement persistent storage.
  Eleven families reproduce 132 failures against the unchanged pre-fix body.
  Twelve separate original native golden runs preserve order, all slots,
  delimiters, empty alternatives, public matching and reply construction/timing.
- All six normal/fast/debug/debug-fast/manager/manager-fast modes pass Clang
  ASan/UBSan/float-cast-overflow and optimized GCC; CI runs both compilers.
  Parent encoded-message/random suites, five ledger/manifest checks, Bash syntax
  and diff checks pass.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,792,471 | `e5ca9ff2daf17fe11b1c0e3fb86d44ac7a98e3ce49c211f01e72187c56c2bdcd` |
| Quake3_TeamArena | 3,941,045 | `cd1f3a0ced1da64ee328409e7910ea8f620a4b7edc641ce0fdd09f52f7e71702` |

Both PPC products have valid PEF headers and build without diagnostics using
[the recorded toolchain libraries](qvm-loading-validation.md).

## Remaining acceptance

Keep #48 open for complete template/reply/initial dictionary/cache factories,
library/world publication and aggregate parser/work/memory budgets. Template
and reply container allocations are separate work; this step covers pieces and
borrowed-source failure ownership. Physical hunk reset remains the engine's
responsibility. Retail/Mac OS 9 execution is explicitly deferred.
