# Native macro token bounds and rollback — 2026-09-18

Stringization permits its terminator to overwrite the following token field,
can omit a closing quote and returns success after truncation. Name/string
pasting appends unchecked; an empty malformed string indexes before its buffer.
Failed expansion leaks argument and partial output chains, and returned null
copies are used without checks. Stringized copies also contain uninitialized
stack metadata.

Reuse the complete-string helper for tokens and paths. Build stringized text in
private storage while reserving closing quote/NUL capacity. Preflight complete
name/string paste costs before mutation. Rejections leave helper outputs intact.
Failed expansion releases both private chains and publishes no result; nullable
copies retain native fatal diagnostics if they return. Successful stringization
uses invocation metadata and a defined string-length subtype. Source publication
still links complete output before queued tokens.

Valid commercial 1.32c name/name, name/number and string/string text, stringize
concatenation, empty results and native maximum parameter count retain their
behavior. Number/number pasting remains unsupported. Structure, parser/QVM/syscall
and asset layouts are unchanged. [ioquake3's native source](https://github.com/ioquake/ioq3/blob/master/code/botlib/l_precomp.c)
was inspected as a text/grammar compatibility reference; rejecting overflow and
releasing failed private chains are explicit fixes in this step.

## Validation

The fixture includes the complete actual native preprocessor and reuses include
lexer/heap/file/print interfaces. Token building, parameter reading/substitution,
copy/free, merging, cleanup and source publication execute unchanged; no helper,
expansion or rollback body is replaced.

Eight original actual-body proofs fail: oversized stringize success/mutation,
both name/string paste buffer overflows, empty-string indexing, unsupported-paste
output leakage, incomplete-argument leakage, nullable parameter copy access and
aliased stringize copying. Ten short literal native text/type goldens pass
against the original body and the fixed body.

Single/cumulative stringize boundaries reserve both quote/terminator bytes; the
last fitting text remains complete. Name/string paste exact capacity remains
accepted and the next byte rejects before mutation. Aliased stringize builds
privately and aliased name append uses overlapping-copy-safe storage. Actual
parameter paste retains its two output tokens; stringized substitution retains
head/string/tail text and defined invocation line/whitespace/subtype metadata.

All six parameter/output allocation failures roll back private physical owners
and native token counts, publish no output and retry successfully. Oversized
parameter paste/stringize, incomplete arguments and unsupported paste also free
both chains. Failed source expansion preserves preexisting queued token ownership;
successful output links before that queue and is consumed through the actual
source reader. Invalid parameter counts reject before imports; the native
128-parameter maximum and too-few warning remain accepted. Final cleanup returns
all physical owners and native token counts to zero.

Clang normal/release fast-math ASan/UBSan and optimized GCC pass. Parent include
and builtin sanitizers pass. GCC and Clang CI runners execute both configurations.
Five ledger/manifest checks, Bash and diff checks pass. Whole-preprocessor host
compilation retains two existing unrelated `abs(long)` expression warnings.
Both Retro68 products rebuild with zero compiler diagnostics and valid PPC PEFs:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,766,481 | `8fedd120e0407a820169342ab7c3a2370d710fcee3cac33fb4937e4fc66b8874` |
| Quake3_TeamArena | 3,915,055 | `2bf7ad9e6ae98006bb63b04af08bd34f0d9ce1da565c450662df23274d859665` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).
Source includes the checked include/builtin/libvar/routing parents.

## Remaining acceptance

Keep #48 open for other token/define/indent/source-loader allocation consumers,
expression/character bounds, real nested/malformed source coverage and aggregate
parse/recursion budgets. This expansion transaction does not establish all native
source-loader or tokenizer transactions. Retail/Mac OS 9 execution is deferred.
