# Public characteristic integer conversion — 2026-09-18

Characteristic_Integer casts any stored float to int without checking its
range or representation. Characteristic_BInteger calls that conversion before
clamping, so even a finite float that should become an ordinary bound can invoke
undefined behavior. A real parsed `2147483648.0` already exceeds signed int range.

Classify float representation through copied bits so release fast-math cannot
remove non-finite checks. Unbounded float conversion rejects non-finite or
unrepresentable values with the existing zero/error style. Bounded finite floats
compare against exact integer bounds in double precision and clamp before casting;
non-finite values reject. Keep representable truncation, complete native signed
integer fields, wrong-type/index fallbacks and reversed-bound rejection intact.
Commercial 1.32c public imports/QVM/syscalls, protocol and asset layouts remain
unchanged. Getter failure never changes character ownership or imports memory.

## Validation

The runner links actual character/source/lexer/libvar/Q_shared bodies and real
file/heap/printing interfaces. Nine original actual-body sanitizer proofs fail:
NaN, a real parsed float above INT_MAX, positive/negative FLT_MAX and infinity,
positive/negative finite bounded overflow and bounded NaN. Original valid getter
corpus passes the same 9,612 comparisons as the fixed body.

A test-only reference header contains the original actual getter bodies at parent
`32e2a8295442205228ae841a204e014c515cf80d`, changing only function names. It runs
solely on representable inputs. 801 quarter-step float values across eleven bound
pairs plus unbounded conversion compare exact truncation/clamping behavior.
Additional cases retain signed integer endpoints, negative zero, wrong types,
inaccessible index 80 and reversed bounds. New extremes check finite pre-clamping
at full INT_MIN/INT_MAX bounds without float-rounding ambiguity, and reject
non-finite conversions in both getters. Real lexer/file parsing supplies the wide
float too. Every getter retains complete header/field bytes, physical string
contents, allocation counts and native token ownership; final cleanup frees all.

Clang normal/release fast-math ASan/UBSan, optimized GCC, parent character/
interpolation sanitizers, five ledger/manifest checks, Bash syntax and diff checks
pass. GCC/Clang CI runs both configurations. Whole-preprocessor host compilation
retains two unrelated `abs(long)` expression warnings. Both Retro68 products
rebuild with zero compiler diagnostics and valid PPC PEFs:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,770,781 | `78bc93714f7ca2a95c8b1dd21e792129d4b0e058f617b83ee89233726e8410cb` |
| Quake3_TeamArena | 3,919,355 | `3c32cb264abd0a3fdde77ff9cf2134b75a22c00a765544ef785819ea289c7a83` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).
Source includes the checked weight/source-error and earlier parser/character/
libvar/routing parents.

## Remaining acceptance

Keep #48 open for file-to-character numeric narrowing, public skill/interpolation
arithmetic, float-bound/string getter validation, other publication/factory
callers, file comment compression, expression and aggregate work/recursion budgets.
Retail/Mac OS 9 execution remains deferred.
