# Public characteristic float getters — 2026-09-18

Characteristic_Float returns non-finite stored values. Characteristic_BFloat
accepts NaN bounds, making native comparisons silently choose an arbitrary
unbounded result; a non-finite field can similarly escape or become a bound.
Reject these representations with the existing zero/error convention before
selecting output. Keep defined signed infinity bounds, finite native clamping,
integer conversion, negative zero and wrong-type/index error fallbacks.

A shared private bit reader materializes copied integer representation through a
volatile value, keeping checks observable under release finite-math assumptions.
The public skill classifier uses the same reader. Public imports/QVM/syscall ABI,
commercial 1.32c protocols and asset layouts remain unchanged. Getters neither
allocate nor change character owners.

## Validation

The runner includes actual character bodies and links real preprocessor, lexer,
libvar and core bodies with file/heap/print interfaces. Eight original actual-body
proofs fail: lower/upper NaN bounds, unbounded NaN/positive infinity/negative
infinity fields, and the same three bounded fields. Original valid corpus passes.
A test-only reference header preserves the original actual getter bodies from
parent `05c0f61d830c7dc36a3c99ce24b7b3dc0d816587`, changing only names and invoking
them solely on defined inputs.

801 quarter-step fields with eleven finite bound pairs plus unbounded reads
produce 9,612 exact bit comparisons. Native signed integer endpoints, negative
zero, wrong types, index 80 and reversed bounds retain behavior. Five infinity
bound pairs match original result bits, including forced positive/negative
infinity outputs. Positive/negative NaN bounds and NaN/infinity fields reject.
Every case snapshots the complete character header/field bytes and owned string,
retains import counts, and physically releases all character/source/string owners.

Clang ASan/UBSan and optimized GCC pass normal/release fast-math configurations.
Affected integer, skill and interpolation-numeric parent fixtures pass both
compilers/configurations. Five ledger/manifest tests, Bash syntax and diff checks
pass. GCC/Clang CI runs both modes. Host compilation retains two existing
`abs(long)` expression warnings. Both Retro68 products build with zero compiler
diagnostics and valid PowerPC PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,770,905 | `d18c84796aa9fcd9a63763f0b70ab6bf9f8ba332ef2485221492171c698a9deb` |
| Quake3_TeamArena | 3,919,479 | `5ceaf33a3068532a0f293dbae9011e2d77a8d5022eb51cb9e61ec144a763411d` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #48 open for other publication/factory callers, source file comment
compression, expression metadata/arithmetic and aggregate work/recursion budgets.
Retail/Mac OS 9 execution remains deferred.
