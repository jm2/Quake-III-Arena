# Unread and conditional source factories — 2026-09-18

PC_UnreadSourceToken dereferences a nullable copied token. PC_PushIndent likewise
writes through a missing allocation, and directive callers report success without
checking creation. Check imports before writing/linking and record a source error;
unread keeps native copy-factory fatal severity. A failed push preserves its prior
stack and skip count. Its private return value now propagates through all four
conditional directive call sites. Public imports/QVM/syscalls, commercial 1.32c
protocols and asset layouts remain unchanged.

## Validation

The runner reuses real expression/source/lexer/libvar/character bodies and physical
heap callbacks. Six original actual-body sanitizer proofs fail: empty/occupied
unread queues, newline-lookahead unread, and if/ifdef/ifndef indent creation.
Original valid nested conditionals and unread token order pass. Original proof
compilation excludes direct calls to the newly return-valued private indent helper;
its unchanged original void signature is exercised through original directive bodies.

Failed unread copies retain caller and prior queued token bytes, source owners and
physical counts, then retry through historical source status. Failed line lookahead
records source status and leaves no partial queue. A direct failed push keeps the
complete existing indent/skip bytes and retries; native boolean skip and nested pop
restore the prior stack. All three entering directive forms propagate creation
failure, retaining four source owners and any native queued next-line token.
Actual nested if/ifdef/else/ifndef parsing returns ordered native tokens and releases
all indents. Every teardown physically releases source/queue/indent/token owners.

Clang ASan/UBSan and optimized GCC pass normal/release fast-math modes. Expression
collection, source-error, macro and weight parent fixtures pass both compilers/modes.
Five ledger/manifest checks, Bash syntax and diff checks pass. GCC/Clang CI runs the
new fixture. Both PPC products build with zero diagnostics and valid PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,779,441 | `13c20a9a03da6f36de61b3956ed40a3be98c7c7e02816594a31dd80275c80b39` |
| Quake3_TeamArena | 3,928,015 | `91f5db7b15e4f679c56bb170dab6e6b78801cec6a0c72eec4c456b4d5542c3ff` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #48 open for define/global/builtin and direct libvar factories, conditional
replacement transaction/recovery, other publication callers, file comment
compression and aggregate work/recursion budgets. This step preserves failed push
state; it does not change the native prior-pop behavior of else/elif replacement.
Retail/Mac OS 9 execution remains deferred.
