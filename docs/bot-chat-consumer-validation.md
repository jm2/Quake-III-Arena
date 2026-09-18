# Native reply, initial-count and match consumers — 2026-09-18

BotReplyChat copied arbitrary input into its fixed native match array before
selection. Reject missing input or text that cannot fit with its terminator
before copying or touching pending state and selected-line timing. Valid input
through 255 bytes remains complete; overlong input returns native false.

BotNumInitialChats now returns zero if no chat file/type is available. The
private initial selector also rejects a missing state/chat/type. Existing
Q_stricmp already handles NULL type, so that path is native compatibility
coverage rather than an original failure claim. BotMatchVariable handles a
missing match with the same empty-string result as its existing invalid-span
cases, while preserving output canaries and valid substring behavior.

## Validation

Actual reply selection, chat construction, initial count/selection and match
substring bodies cover missing inputs and reply lengths 7/255/256/1024. Invalid
reply input preserves every pending-state, dictionary and selected-line byte;
valid input produces native text and line time. Real loaded initial count,
construction and timing stay; absent files/types leave all state unchanged.
Match missing/valid output checks retain guard bytes and physical ownership.

Five proofs against the actual pre-change body reproduce overlong/missing reply
input, absent-file count, missing match and missing private selector state in
all twelve compiler/mode builds (60 failures). Separate original native reply,
255-byte input, loaded initial selection, missing type and match-substring
goldens pass all twelve builds. No production adapter is used.

Clang ASan/UBSan/float-cast-overflow and optimized GCC pass all six allocator/
fast modes. CI runs both compilers; Bash syntax, five ledger/manifest checks and
diff checks pass. Ownership uses the [recorded actual-owner fixtures](bot-item-config-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,792,409 | `acaf05b1397218d0a3b8eb5b334a5d7c57777c873d194f7d33195339327c8397` |
| Quake3_TeamArena | 3,940,983 | `a8d81f9047ceb8d6aa0b5fe2d78a68f79aaa79506186dee739d982007e099040` |

Both PPC products have valid PEF headers and build with zero diagnostics using
the [recorded toolchain libraries](qvm-loading-validation.md). Commercial 1.32c
public structs, imports and syscall tables remain unchanged.

## Remaining acceptance

Keep #48 open for combined optional-variable/expansion bounds, dictionary/cache
loaders, complete library/world transactions and aggregate parser/work/memory
budgets. Retail/Mac OS 9 execution remains explicitly deferred.
