# Complete movement libvar setup — 2026-09-18

BotSetupMoveAI assigns nullable LibVar results directly to ten shared movement
pointers, clears brush classifications before validation and always reports success.
Later movement can dereference missing state even though initialization succeeded.

Resolve all ten complete native libvars before publishing references or updating
brush models. A failed import reports the existing BLERR_LIBRARYNOTSETUP code and
names the failed variable; prior references/model bytes remain. Complete shared
libvar cache entries retain their native ownership and are available for retry.
No public enum/import/syscall/layout changes are needed; commercial 1.32c defaults,
configured values, cached identity and legacy protocol behavior remain compatible.

## Validation

The fixture compiles the entire actual movement source, actual libvar implementation
and native brush classifier. Forty original-body proofs cover two nullable imports
for each variable with empty/complete prior references; all original paths report
success or mutate incomplete state. Original successful/default/configured/cached
reference and plat/door classification goldens pass separately.

Each fixed failure stops at the failed import with a diagnostic and nonzero setup
result. Complete prior reference/model/node/value/flag bytes remain. Physical owner
counts show only complete shared cached libvars and no partial failed variable.
Every case retries, publishes all ten native references and classifies actual
brush entities. Configured step/grapple values and cached pointer identity remain
across a second setup without imports. All shared owners physically release.

Clang ASan/UBSan and optimized GCC pass normal/release fast-math modes, including
parent native-libvar checks. Five ledger/manifest checks, Bash syntax and diff checks
pass; GCC/Clang CI runs the fixture. Clang reports the existing elevator integer
abs-on-float warning outside this setup change; it is retained for separate numeric
assessment. Both PPC products build with zero diagnostics and valid PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,779,493 | `653753725ab9ab59906f4c8ce0796d2bd3b7ab7ba5272544a48272738c4ad1ce` |
| Quake3_TeamArena | 3,928,067 | `930e467b1f4de977352a2f30d0f209e9e538114c11ce2f087f44b4937c2c4dd6` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #48 open for AAS/goal and other direct libvar consumers, builtin/source
publication callers, file comment compression and aggregate work/recursion budgets.
This verifies movement initialization, not complete bot-library rollback or target
navigation. Retail/Mac OS 9 execution remains deferred.
