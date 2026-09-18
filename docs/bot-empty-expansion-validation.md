# Empty macro source continuation — 2026-09-18

PC_ExpandDefine returns success for a complete expansion that produces no tokens.
PC_ExpandDefineIntoSource instead reports failure, so the source reader returns
false before it reaches actual EOF. Subsequent tokens or lexical errors can be
hidden, concatenated strings split and valid character fields rejected.

A complete empty expansion now reports successful consumption. Readers continue
with the existing queue/script until they return a real token, reach actual EOF or
report an error. Nonempty expansion and failure paths retain their native behavior.
Commercial 1.32c public imports/syscalls/protocols and retail formats remain compatible.

## Validation

The actual-body fixture links complete source/macro/expression/lexer/libvar/character
bodies with physical heap/VFS callbacks. Ten original-body proofs fail for object
and consecutive empty macros, empty function bodies, empty arguments, string
concatenation, hidden lexical errors, included-script continuation, direct queued
expansion, actual character fields and unconsumed empty-only input. Original valid
nonempty object/function goldens pass separately.

Fixed readers preserve ordered tokens and native warnings, concatenate strings
across empty macros, and verify actual script EOF. Lexical errors after an empty
macro remain visible/sticky. Included empty scripts physically release before the
parent token returns. Direct empty expansion preserves complete prior queued bytes
without imports. Actual character fields return complete native integer/float/string
values and release source owners before publication. Every teardown physically
releases all definitions, parameters, tokens, scripts and character/string owners.

Clang ASan/UBSan and optimized GCC pass normal/release fast-math modes. Affected
definition/macro/source-factory/source-error fixtures pass both compilers/modes.
Five ledger/manifest checks, Bash syntax and diff checks pass. GCC/Clang CI runs the
fixture. Both PPC products build with zero diagnostics and valid PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,779,481 | `6fdb73a68f230c31f55f905105ba5bc37a032bc4eb106b71ca61861f8f6279d3` |
| Quake3_TeamArena | 3,928,055 | `79433d15a568e0a6a2206b8f86609530bc6351d3c1c7a51d020ea0a05291760e` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #48 open for builtin factories, direct libvar and other publication consumers,
file comment compression and aggregate expression/parse/recursion budgets. Native
zero-parameter function/object distinctions and cyclic expansion require further
assessment; this step changes successful empty consumption only. Retail/Mac OS 9
execution remains deferred.
