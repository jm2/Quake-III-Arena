# Chat synonym source construction — 2026-09-18

BotLoadSynonyms frees its source before reporting an unclosed context, producing
a real use-after-free. Source errors can appear as ordinary EOF and publish
partial synonyms. Its second pass writes through a nullable hunk pointer without
capacity checks; changed files can overflow the first pass allocation. Odd string
lengths leave later private structs misaligned. Failed second-pass parsing also
consumes an incomplete hunk allocation that cannot be individually reclaimed.

Check aligned representable private entry costs in both passes, and validate every
second-pass write against measured capacity. Stage the second pass in reclaimable
heap memory. Keep source diagnostics/status until validation, then release source
owners before allocating/copying a single complete native hunk owner. Rebase all
list/entry/string pointers into the final block and free staging. Reject capacity
growth/shrinkage, source errors and unrepresentable/non-finite float weights or
totals. Empty source retains native no-synonym behavior. Native context semantics,
entry order, valid weights and public synonym replacement behavior remain intact.
Commercial 1.32c public imports/QVM/syscalls, protocols and asset layouts remain
unchanged; only private allocation alignment and construction change.

## Validation

The fixture includes actual chat bodies and links real source/lexer/libvar/core
bodies, with independent physical heap/hunk and VFS interfaces. Fourteen original
actual-body proofs fail: missing-context use-after-free; ignored preprocessor EOF;
changed second-pass growth/shrinkage; missing second source; nullable hunk import;
unrepresentable weight and overflowing total; odd-string struct alignment;
second-pass lexical error; and four nullable second-source factory imports that
retain an already allocated hunk. Original valid native context/string/weight
corpus passes the same goldens.

Three native lists retain contexts 1/3/1, six ordered strings and weights, totals
3.75/4/8 and complete final physical owner ranges. All source/staged heap and token
owners release before returning the sole hunk block. Root/included errors and
malformed first/second-pass syntax retain no published hunk. Nine nullable imports
(eight actual source factory positions plus staging) and final hunk failure release
all earlier owners; heap/source failures retry. Changed capacity, empty source,
missing filenames, odd-sized strings, weight limits and aggregate overflow are
covered. The test explicitly tears down its physical hunk only after acceptance;
engine hunk ownership remains native.

Clang ASan/UBSan and optimized GCC pass normal/release fast-math modes. Existing
chat dispatcher/native buffer fixtures pass both compilers. Five ledger/manifest
checks, Bash syntax and diff checks pass; GCC/Clang CI runs the new fixture.
Whole-source host compilation retains two existing `abs(long)` warnings in
expression directives. Both PPC products rebuild with zero compiler diagnostics:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,775,103 | `9490bbf6fe2df1ce26c41ab82c87527a739cea0734a4c914b7df5c0f2d3f89b8` |
| Quake3_TeamArena | 3,923,677 | `f102a135884c3cdc0925a845dbb0ec220eda1b92ec94bcb407f1002ff4f48961` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #48 open for other chat/item/weapon publication and factory callers, file
comment compression, expression metadata/arithmetic and aggregate work/recursion
budgets. Retail/Mac OS 9 execution remains deferred.
