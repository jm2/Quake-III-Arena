# Public characteristic string capacity — 2026-09-18

Characteristic_String subtracts one from any supplied capacity and uses the
result as a strncpy length and destination index. Zero/negative capacities
become enormous unsigned copies; INT_MIN subtraction overflows, and a missing
output buffer reaches the copy unchanged.

Return before subtraction, character lookup or copy when output is missing or
capacity is nonpositive. Preserve the native copy for every valid capacity,
including complete strncpy NUL padding, truncation and final termination.
Commercial 1.32c public imports/QVM/syscalls, protocol, asset and character
layouts remain unchanged; no output owner is published or imported by getters.

## Validation

The runner links actual character/source/lexer/libvar/Q_shared bodies with only
file/heap/printing interfaces. Six original actual-body proofs fail: zero/negative
copy lengths, INT_MIN signed subtraction and three missing-buffer configurations.
Original valid capacity and handle/index/type goldens also pass the same tests.

The fixed suite checks complete ordinary/empty output and surrounding canaries
for every capacity one through 1,024 (2,048 outputs). Every copied, truncated,
terminating and padded byte matches native behavior. Missing/nonpositive output
is unchanged and performs no imports/diagnostics. With valid buffers, existing
handle/index/type errors retain diagnostics and leave all output bytes unchanged.
Every getter preserves complete character/field bytes and independent ordinary/
empty string owners; final source/character/string cleanup physically frees all.

Clang normal/release fast-math ASan/UBSan, optimized GCC, parent character
sanitizers, five ledger/manifest checks, Bash syntax and diff checks pass.
GCC/Clang CI runs both configurations. Whole-preprocessor host compilation retains
two unrelated `abs(long)` expression warnings. Both Retro68 products rebuild with
zero compiler diagnostics and valid PPC PEFs:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,770,781 | `000080e2da26e93718135f1d85940e34d95cdb8e5cf788c94deb81830798c337` |
| Quake3_TeamArena | 3,919,355 | `399188c9b5e200e0f4cbf100556569c88fa49fa3ab7dd096707a21af5f6942ed` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).
Source includes the checked integer-getter/weight/source-error and earlier
parser/character/libvar/routing parents.

## Remaining acceptance

Keep #48 open for file numeric narrowing, skill/interpolation arithmetic, float
bounds, other source publication/factory callers, file comment compression,
expression and aggregate parse/recursion budgets. Retail/Mac OS 9 execution remains
deferred.
