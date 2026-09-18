# Private source error status — 2026-09-18

A failed lexer read shares its false result with ordinary EOF. Included scripts
can consequently unwind into their parents after malformed input; string
lookahead can accept its preceding token despite the failure. Queued/cached tokens
can bypass a prior lexical failure, and diagnostic suppression loses evidence
needed by publication callers.

Keep persistent lexical and preprocessor status in unused private script flag
bits. Lexical errors stop whitespace/token reads before cached or queued owners
are consumed and keep failed include stacks owned until normal source cleanup.
String lookahead rejects its outer token after a source error. Source diagnostics
record history, which survives normal child unwinding. Ordinary directive recovery
continues to work; publication callers can query the complete source status before
freeing it. Suppressing diagnostics or changing formatting flags cannot clear
recorded errors. Raw-memory unterminated block comments report lexical failure.

Commercial 1.32c parser/QVM/syscall/protocol/asset and structure layouts remain
unchanged. Existing valid nested includes and native EOF retain their behavior.

## Validation

The fixture links actual source, lexer, preprocessor, character, libvar and
Q_shared bodies with file/heap/printing interfaces. Nine original actual-body
proofs fail: malformed one/two-level includes continuing into parents, accepting
an outer string after failed lookahead, consuming queued tokens after failure,
losing preprocessor history on child unwinding, suppressed lexical status,
cached-token bypass, formatting status reset and accepting a raw unterminated
comment. Original native one/two-level include goldens also pass.

Fixed tests retain failed stacks, queued owner identities, prior published token
bytes and allocation counters across repeated reads. Normal directive recovery
reads child/parent tokens while preserving history. Suppressed/ordinary errors
retain status independently of printing. Normal includes close every file, free
child scripts and leave no error; complete cleanup releases all physical script,
table, dictionary and queued-token owners.

Clang normal/release fast-math ASan/UBSan, optimized GCC, eight parent sanitizer
suites, five ledger/manifest checks, Bash syntax and diff checks pass. Parent
number tests now assert retained cursor bytes with stopped reads; raw malformed
comment tests expect the new diagnostic. GCC/Clang CI runs both configurations.
Whole-preprocessor host compilation retains two unrelated `abs(long)` warnings.
Both Retro68 products rebuild with zero compiler diagnostics and valid PPC PEFs:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,766,589 | `37f8c31ef4f1d6f64de0da0094614e385760e419982c1defcb713df5468c7240` |
| Quake3_TeamArena | 3,919,259 | `c01ce9995e128b12065c6e50347bc72ea837c22058846d6ff90e9af7602565e4` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).
Source includes the checked escape/number/default/interpolation/registry/source/
character/parser/libvar/routing parents.

## Remaining acceptance

Keep #48 open for publication callers checking source status, file comment
compression before lexing, character/public numeric/skill bounds, expression
arithmetic, remaining factories/libvar consumers, further real nested/malformed
sources and aggregate parse/recursion budgets. Retail/Mac OS 9 execution remains
deferred.
