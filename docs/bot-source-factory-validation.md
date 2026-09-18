# Unread and conditional source factories — 2026-09-18

PC_UnreadSourceToken dereferences a nullable copied token. PC_PushIndent likewise
writes through a missing allocation, and directive callers report success without
checking creation. Check imports before writing/linking and record a source error;
unread keeps native copy-factory fatal severity. A failed push preserves its prior
stack and skip count. Its private return value now propagates through entering
conditional directives. Else/elif reuse the complete same-script frame without
allocating a replacement. Elif retains its frame until expression evaluation
succeeds, then reacquires it: EOF can unwind the script and free the frame.
A branch crossing the end of its script rejects instead of mutating an enclosing
conditional. Public imports/QVM/syscalls, commercial 1.32c
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
all indents. Six pre-review actual-body proofs reproduce lost else/elif frames,
failed operand/lookahead recovery, root EOF and an included-script transition.
Reused frames preserve native selection/token order with no replacement import;
failed expression imports retain prior frame bytes/skip, and subsequent endif
closes that condition. Exhausted scripts unwind safely; included EOF preserves
the enclosing frame and its native body. Every teardown physically releases
source/queue/indent/token owners.

Clang ASan/UBSan and optimized GCC pass normal/release fast-math modes. Expression
collection, source-error, macro and weight parent fixtures pass both compilers/modes.
Five ledger/manifest checks, Bash syntax and diff checks pass. GCC/Clang CI runs the
new fixture. Both PPC products build with zero diagnostics and valid PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,779,431 | `9b3b434a23a62c0db261e05e90d33c8ed0943207e1f51b70d0ebe775bc4d88bb` |
| Quake3_TeamArena | 3,928,005 | `ebbbb2104c02c1e010b7a84010746358897ce25f516da3b7fa9b273303b64205` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #48 open for define/builtin and direct libvar factories, other publication
callers, file comment compression and aggregate work/recursion budgets.
Retail/Mac OS 9 execution remains deferred.
