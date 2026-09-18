# Character numeric field publication — 2026-09-18

Numeric lexer values can exceed the stored characteristic float or native integer
word. The character parser narrows them directly, allowing non-finite float fields
and oversized host integer words to become published characters while prior
strings are already owned.

Check finite float range before narrowing, classify the narrowed candidate before
field/type publication, and reject failures with full source/character/string
cleanup. Check integer tokens against the native unsigned int word and copy valid
word bits into the signed field. Preserve the full native 32-bit word semantics:
`2147483647`, `2147483648` and `4294967295` still become INT_MAX, INT_MIN and -1.

Ordinary decimal/fraction/suffix floats, full FLT_MAX and subnormal output retain
native stored bits. Commercial 1.32c public imports/QVM/syscalls, protocol, asset
and character layouts remain unchanged.

## Validation

The runner links actual character/source/lexer/libvar/Q_shared bodies with only
file/heap/printing interfaces. Ten original actual-body proofs fail: oversized
raw/macro/included floats, a thousand-digit finite lexer float, oversized native
integer words in each of four bases, and macro/included oversized words. Original
native ordinary/fraction/suffix/max/subnormal and 32-bit word goldens also pass.

Fixed tests reject before character publication and release prior strings,
character and every physical script/table/source/dictionary/macro/token owner.
Every file closes, token counts balance and a failed float load retries. Valid
float comparisons inspect stored bytes as well as public getter values; native
integer word/sign endpoints remain intact. Child includes and real macro copies
execute the production parser/lexer and physical cleanup bodies.

Clang normal/release fast-math ASan/UBSan, optimized GCC, parent character/integer
getter sanitizers, five ledger/manifest checks, Bash syntax and diff checks pass.
GCC/Clang CI runs both configurations. Whole-preprocessor host compilation retains
two unrelated `abs(long)` expression warnings. Both Retro68 products rebuild with
zero compiler diagnostics and valid PPC PEFs:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,770,825 | `13ee4814e2bbae770101bef3db2d83e8aee58b8ee0fdc179a0b90629ee39e48d` |
| Quake3_TeamArena | 3,919,399 | `928b05a3b844ccc275a3af7d4b40865bfbf14458ae8cf710a364404def8462c0` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).
Source includes the checked string/integer-getter/weight/source-error and earlier
parser/character/libvar/routing parents.

## Remaining acceptance

Keep #48 open for public skill/interpolation arithmetic, float bounds, other
publication/factory callers, file comment compression, expression metadata/
arithmetic and aggregate work/recursion budgets. Retail/Mac OS 9 execution remains
deferred.
