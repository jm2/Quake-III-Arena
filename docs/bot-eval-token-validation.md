# Expression result token metadata — 2026-09-18

Hash eval directives leave numeric metadata uninitialized. Dollar eval helpers
emit magnitude text and a separate minus token while storing signed metadata;
float helpers also cast negative/huge/non-finite results to unsigned long without
range checks. Integer text uses int abs/formatting on long results, truncating
host long values and overflowing signed absolute value at the native minimum.
Unchecked output copies can expose half a signed result or dereference NULL.

Initialize complete result/sign tokens, store unsigned magnitude metadata, format
integer magnitude through defined unsigned subtraction and bounded %lu, and keep
native two-decimal float text plus the original unrounded magnitude. Float results
use copied volatile integer representation checks before formatting/casting;
auxiliary unsigned integers clamp like native lexer metadata. Stage number and
optional minus copies before linking either into the existing queue, releasing
uncommitted copies on failure and recording a source error. Copy factory fatal
severity remains native. The old unused sign helper is removed.

The dollar magnitude convention agrees with
[ioquake3 expression helpers](https://github.com/ioquake/ioq3/blob/master/code/botlib/l_precomp.c).
Commercial 1.32c public imports/QVM/syscalls, protocols and asset layouts remain
unchanged. Internal magnitude metadata now agrees with emitted text/sign tokens.

## Validation

The fixture calls actual four directive entries and links real evaluator/source/
lexer/libvar/core/character bodies with physical heap and file interfaces.
Seventeen original actual-body proofs fail: two missing hash metadata cases,
negative dollar integer/float metadata, wide finite float conversion, long minimum/
maximum text, infinity/NaN result publication, and eight nullable number/sign copy
cases. Float copy fault proofs use -0.5, which is representable after truncation,
to isolate output allocation failure from the earlier unsafe float cast.

324 quarter-step/integer cohorts retain original spelling/type/sign behavior and
correct complete magnitude metadata. The original body passes 284 defined text
cohorts; forty negative dollar-float cases are excluded from that original corpus
because its cast is undefined. Wide finite values retain raw unrounded metadata
and bounded auxiliary integers. Long minimum/maximum magnitudes retain complete
text without abs overflow. NaN/infinity results publish no tokens. Every output
copy fault keeps source and queued next-line owners, records one copy-factory
fatal and one source diagnostic, releases earlier result copies and physically
frees all owners on source teardown. Public hash/dollar sequencing preserves a
following token; real character fields consume $evalint(42)/$evalfloat(1.25).

Clang ASan/UBSan and optimized GCC pass normal/release fast-math modes. Affected
numeric lexer, source-error, weight, character numeric and synonym parent fixtures
pass both compilers/configurations. Five ledger/manifest tests, Bash syntax and
diff checks pass. Host expression abs warnings are eliminated. Both PPC products
build with zero compiler diagnostics and valid PowerPC PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,775,181 | `122b18f79b5c6d8b30918b1337c8a92dbfc9c538d7d842ce8300cbab6d8c1329` |
| Quake3_TeamArena | 3,923,755 | `0c8881503d2a37fb4a2dc862f7835353c462a5fe6104e1f9c2518eec27b3dea8` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #48 open for expression operand collection/cleanup, signed arithmetic/shift/
division checks, other factories/publication callers, file comment compression and
aggregate work/recursion budgets. Retail/Mac OS 9 execution remains deferred.
