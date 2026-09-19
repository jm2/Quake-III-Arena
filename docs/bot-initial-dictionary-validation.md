# Native complete initial dictionary — 2026-09-18

BotLoadInitialChat leaked its packed heap when the second source/load/parse
failed, dereferenced nullable packed imports, packed pointer-bearing records
without alignment and wrote past first-pass capacity if the source grew. It
also accepted first-pass source-error prefixes and stored unterminated type names.
Measure aligned complete record/string costs, check the single packed heap,
validate both source passes and measured capacity, and release all failed
candidate/source owners. Reject type names that cannot fit with their NUL.

Keep native reverse type/message order, case-insensitive selected chat/type
lookup, repeated selected blocks, empty selected chats/types, maximum terminated
type names, initial construction and line timing. The result remains one heap
allocation released by the caller's existing FreeMemory/BotFreeChatFile path.
Commercial 1.32c public layouts/imports/syscalls and retail grammar are unchanged.

## Host and product evidence

- [The actual factory fixture](../tests/bot_initial_dictionary_regression.c) runs
  whole chat/parser/source/allocator bodies with physical engine-owner imports.
- Seventy packed/source/parser nullable positions across fresh/prior dictionaries
  reject, release every candidate/source owner, preserve every prior packed
  payload/pointer byte and public selection, then retry. The existing actual
  public file-free path releases the complete packed physical heap owner.
- Second-pass malformed/grown/shrunken/missing-selected/source-error inputs,
  first-pass lexical/directive errors, unterminated type names, short-string
  alignment and NULL packed imports reproduce 120 failures across ten families
  against the unchanged pre-fix body. Larger lexer-bound names also reject;
  they are not counted as failures of the original implementation.
- Twelve separate original native golden runs preserve order, maximum terminated
  type names/public lookup, empty/repeated selected blocks and actual initial
  construction/timing. All six normal/fast/debug/debug-fast/manager/manager-fast
  modes pass Clang ASan/UBSan/float-cast-overflow and optimized GCC. CI runs both
  compilers. Parent piece tests, five ledger/manifest checks, Bash syntax and
  diff checks pass.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,796,715 | `2082a7425f8e66cf455d37753f33b05e713c5337b505c7adb409b38b97cf6730` |
| Quake3_TeamArena | 3,945,289 | `b7dcf4d7c576b8e2698b849b64ea86d86d6f5bea7381cb89811c06a17475ac58` |

Both PPC products have valid PEF headers and build without diagnostics using
[the recorded toolchain libraries](qvm-loading-validation.md).

## Remaining acceptance

Keep #48 open for cache-policy/private/shared ownership, complete library/world
publication and aggregate parser/work/memory budgets. This factory does not
establish complete public replacement/cache ownership. Physical hunk reset
remains engine-owned. Retail/Mac OS 9 execution is explicitly deferred.
