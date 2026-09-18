# Requested and cached character skill inputs — 2026-09-18

A NaN requested skill bypasses ordinary comparisons and can load/interpolate
characters with non-finite desired skill. Cached loading also casts skill plus
0.5 directly to int, allowing NaN/infinity/finite extremes to invoke undefined
behavior before fallback parsing.

Classify requested representations before imports: reject NaN and retain native
signed infinity clamps to one/five. Ordinary finite clamps and cohort selection
remain unchanged. Cached loading rejects non-finite or unrepresentable rounded
skill before cache/file/heap work; valid rounding and any-skill fallback remain.
Commercial 1.32c public imports/QVM/syscalls, protocol, assets and character
layouts stay intact. Rejection preserves all existing cached owners and slots.

## Validation

The runner links actual public/cached character/source/lexer/libvar/Q_shared
bodies with file/heap/printing interfaces. Seven original actual-body proofs fail:
cold/occupied public NaN publication and cached casts of NaN, signed infinities
and positive/negative finite extremes. Original valid clamp/round/cache/fallback
goldens also pass.

34 requested cases retain quarter-step finite/cohort/interpolation results,
finite extremes, signed infinity clamps and negative zero. Eight cached cases
retain exact skills, rounding, any-skill fallback and safe signed endpoint inputs.
Requested normalized cache reuse imports nothing. 28 invalid cold/occupied/reload
cases retain every registry pointer, complete character header/field bytes and
string owner, plus exact file/heap/token counters. Rejected loads perform no
imports or cleanup of existing owners. Shutdown physically frees every complete
source/character/string/libvar owner.

Clang normal/release fast-math ASan/UBSan, optimized GCC, parent interpolation/
default sanitizers, five ledger/manifest checks, Bash syntax and diff checks pass.
GCC/Clang CI runs both configurations. Whole-preprocessor host compilation retains
two unrelated `abs(long)` expression warnings. Both Retro68 products rebuild with
zero compiler diagnostics and valid PPC PEFs:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,770,853 | `b15f2a19dff92378189f1b43d499ac04db50c04e56bbdb324ba752142a46339b` |
| Quake3_TeamArena | 3,919,427 | `bf0ccef46e96d7f03072e27bef28586dbb6239b253fad354175464e42d43f569` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).
Source includes the checked numeric publication/string/integer-getter/weight/
source-error and earlier parser/character/libvar/routing parents.

## Remaining acceptance

Keep #48 open for interpolation endpoint/arithmetic/logging behavior, float
bounds, other publication/factory callers, file comment compression, expression
metadata/arithmetic and aggregate work/recursion budgets. Retail/Mac OS 9 execution
remains deferred.
