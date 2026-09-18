# Expression operand ownership and grammar — 2026-09-18

PC_Evaluate and PC_DollarEvaluate return from parse/evaluation failure without
freeing copied operand chains. Nullable copies dereference NULL; bare defined
reads a missing next token. Dollar evaluation discards its leading token without
checking for ( and publishes a prefix at EOF without a closing ). Lexical failure
can similarly be treated as a valid partial expression.

Give each collector one cleanup path, check operand copies and release the entire
private chain on success or failure. Reject missing defined names before access,
validate dollar delimiters, and reject lexical/new source errors before evaluation.
Compare the private diagnostic generation so historical recoverable source errors
still permit valid later expressions. Preserve native macro/defined values and
copy-factory fatal severity. Public imports/QVM/syscalls, commercial 1.32c
protocols and asset layouts remain unchanged.

## Validation

The fixture shares real expression/source/lexer/libvar/character bodies and heap/
printing interfaces. Seventeen original actual-body proofs fail: undefined-name
and divide-by-zero leaked chains in both collectors; missing defined names in both;
invalid dollar leading/missing-closing delimiters; lexical partial prefixes in
both; six nullable operand copies; and failed macro expansion after copied prefix
operands. Original valid macro/defined/historical-recovery goldens pass unchanged.

All four directives reject malformed names, operators, defined forms and string
operands without copied operand/result owners. Dollar delimiter cases and lexical
tails retain no partial output. First/operator/last copy faults in both collectors
initialize caller outputs, preserve four source owners, record native fatal/source
diagnostics and release all earlier token copies. A real function macro with
missing arguments keeps complete original definition owners and releases the
private prefix. A valid native macro evaluates to eight; bare/parenthesized defined
unknown returns zero. Historical source errors still allow all four valid result
helpers. Every source teardown physically releases all heap/token owners.

The direct macro fixture starts its evaluator on the expression line, matching
native hash-directive call context; a newline boundary correctly stops ReadLine.
The shared expression fixture exposes an optional test entry name so the new
fixture reuses the same actual interfaces without redefining main.

Clang ASan/UBSan and optimized GCC pass normal/release fast-math modes. Expression
token, source-error, macro and weight parent fixtures pass both compilers/modes.
Five ledger/manifest checks, Bash syntax and diff checks pass. GCC/Clang CI runs
the new fixture. Both PPC products build with zero compiler diagnostics and valid
PowerPC PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,775,193 | `35625cbc9681018550916f65d3f3929f7c62f3a26622427002d74ec3d850acee` |
| Quake3_TeamArena | 3,923,767 | `eff2f9fc1f01ffaddf17e24b9405a75d6f0a6f8db4f64da67672c46405c44e06` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #48 open for signed expression arithmetic/shifts/division, other factories/
publication callers, file comment compression and aggregate work/recursion budgets.
Retail/Mac OS 9 execution remains deferred.
