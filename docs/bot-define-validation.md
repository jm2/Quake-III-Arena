# Complete macro-definition publication — 2026-09-18

PC_Directive_define links a definition before its parameters/body validate and
frees a prior definition before replacement succeeds. Nullable header/token
imports can crash or leave partial owners. PC_DefineFromString has unchecked
script/hash imports and frees the wrong list after a malformed hashed definition.

Definitions now remain private until their complete parse succeeds. Every failure
releases staged parameter/body/name owners while retaining the prior dictionary
and complete definition bytes. Raw parameter reads avoid expanding a still-active
previous macro. Unsupported parameter counts reject before publication; the native
128-parameter capacity remains. External factories check inputs and imports, free
queued/hash/script owners and retain a definition only after source validation.
Native replacement warnings, macro order, token metadata and empty markers remain.
Commercial 1.32c public imports/syscalls/protocols and retail formats are unchanged.

## Validation

The fixture links actual macro/source/expression/lexer/libvar/character bodies and
tracks every physical heap owner. Twenty-nine original-body failure proofs cover
partial new/replacement publication, duplicate/recursive external leaks, seven
nullable imports for each new/replacement path, nine nullable external imports,
lexical-prefix publication and a replacement parameter matching the previous name.
The last case is a corrected parse defect, separate from native compatibility
comparisons. Original valid object/function/replacement/empty-marker/external
macro goldens pass unchanged.

Fourteen malformed new/replacement cases retain full prior hash/header/name/token
bytes and release every staged owner. Seven new/replacement fault positions and
nine external positions reject without partial publication, then external retries
succeed. Empty/null external input rejects before imports. Complete 128-parameter
macros remain callable; 129 rejects with private cleanup. Actual source reading
preserves native macro values/order and replacement warnings. Every teardown
physically releases all script/hash/definition/token owners.

Clang ASan/UBSan and optimized GCC pass normal/release fast-math modes. Affected
macro, expression, collection and source-factory parent fixtures pass both compilers
and modes. Five ledger/manifest checks, Bash syntax and diff checks pass; GCC/Clang
CI runs the fixture. Both PPC products build with zero diagnostics and valid PEFs:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,779,481 | `81a8f8a8b364dc61ee2069781f3eb551a1236ab6d40bb23922720e5ec9872eaf` |
| Quake3_TeamArena | 3,928,055 | `a4ea04bb46ec892cce479b2009a2a761d84fbd5d422f32cfbff997ee846c330c` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #48 open for valid empty expansion returning false EOF, builtin factories,
direct libvar and other publication callers, file comment compression and aggregate
expression/parse/recursion budgets. Empty markers are validated here; invoking an
empty macro is the separate confirmed reader defect. Retail/Mac OS 9 execution
remains deferred.
