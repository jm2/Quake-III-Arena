# Native encoded-message components — 2026-09-18

BotLoadChatMessage estimated random keyword encoding as seven bytes even when
the lexer name was up to 1,023 bytes, then used unbounded sprintf. Numeric
components also accepted unavailable variable slots, and failure could leave a
partial caller-owned message. Measure complete encoded components, reserve NUL
space before every copy, validate native slots zero through seven, and publish
only a fully parsed message. Missing source/output rejects before dereference.

Valid literal/variable/random order and encoded bytes remain native. Using the
actual four-byte variable encoding also admits an exact-capacity 251-byte literal
plus variable that the old seven-byte estimate rejected. This safe extension does
not change commercial 1.32c public structs, imports or syscall tables.

## Validation

Actual lexer/source/message bodies cover random names of 252/253/1,023 bytes,
literals of 255/256 bytes, every native variable slot, unavailable slot eight and
large numeric values, combined components, missing delimiter/lexical/unknown
components and missing source/output. Failure preserves every output/canary byte.
Exact-capacity literal/random/combined messages and ordered native encodings are
separate goldens. No persistent arena storage is acquired; actual source/heap
owners release fully through [the owner fixtures](bot-item-config-validation.md).

Ten actual pre-change failure proofs reproduce across all twelve compiler/mode
builds (120 failures). Separate original native encodings, every valid slot,
255-byte literal, 252-byte random name and accepted combined-message goldens pass
all twelve builds. No adapter or implementation mirror is used. Fixed checks
pass six allocator/fast modes under Clang ASan/UBSan/float-cast-overflow and
optimized GCC. CI runs both compilers. Five ledger/manifest checks, Bash syntax
and diff checks pass.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,792,403 | `12c5dc9050d73316e910a592d0a154494e9163a207bca412ad102755d79f8a82` |
| Quake3_TeamArena | 3,940,977 | `d6524fc8d915e5f0b4b4298d2b7c983c6acf10b1f5324aa25df8b8db5d533999` |

Both PPC products have valid PEF headers and build with zero diagnostics using
[the recorded toolchain libraries](qvm-loading-validation.md).

## Remaining acceptance

Keep #48 open for random/match/reply/initial dictionary and cache factories,
complete library/world transactions and aggregate parser/work/memory budgets.
Retail/Mac OS 9 execution remains explicitly deferred.
