# Native encoded chat expansion — 2026-09-18

Encoded variable eight crossed the exclusive native index bound; unchecked decimal
accumulation and span copying could overflow, and literal output could write a
terminator beyond its array. Construction copied unchecked input and published
partial text when a later expansion failed. Validate bounded input, numeric escape
syntax/exclusive indexes, complete native string/span ranges and every output
append before copying. Random keywords require their closing escape; malformed or
missing entries report one recoverable error.

A private checked helper distinguishes successful nonrandom output from failure.
The existing flag wrapper still returns false for successful literal/variable
expansion and true when a random entry expands. It publishes only complete staged
text. Construction stages every pass and publishes pending text only after all
passes succeed. Keep the native ten-pass cyclic-random limit and its warnings.

Private initial-line selection preserves native random/fallback order; the existing
selector wrapper retains its timing behavior. Initial/reply APIs publish line timing
only after successful checked construction. Reply propagates construction failure
through its existing false result. All native public structs, packed spans, imports
and syscall tables remain unchanged; no new allocation owner is introduced.

## Validation

Actual encoded variable/random/literal, constructor and public initial/reply bodies
cover exclusive indexes, excessive numeric range, negative/excessive/past-end/
unterminated spans, missing match/input/output/state, malformed escapes, missing
random entries, input/output capacity and later recursive errors. Failures preserve
every destination/canary or pending-state/selected-line byte and acquire no memory.
A legal random dictionary value plus a literal prefix checks output capacity
independently from input capacity. Native exact 255-byte literal/random output,
legacy expansion flags, unset variables, real nested-random output, ten-pass cycle
warnings and all-recent initial fallback/timing are separate goldens.

Twenty-one actual pre-change proofs reproduce these failures in all twelve
compiler/mode builds (252 failures). Separate original literal/variable/random/
exact-output/recursive-limit/fallback goldens pass all twelve builds. No adapter
or mirrored implementation is used. Clang ASan/UBSan/float-cast-overflow and
optimized GCC pass all six allocator/fast modes. Parent optional-variable and
consumer checks pass six modes with both compilers. CI runs both compilers; its
host allowance is thirty minutes as in #180 after two observed ten-minute timeouts.
Five ledger/manifest checks, Bash syntax and diff checks pass. Physical ownership
follows [the actual-owner fixtures](bot-item-config-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,792,403 | `d01676072ea62c096de0b3151215936b1fa7e50f653dfe21bc723902eb81894f` |
| Quake3_TeamArena | 3,940,977 | `af067d42e24b36b8c2a85143ae9abe65ad1acf65cdce9215896a87fc02e3518d` |

Both PPC products have valid PEF headers and build with zero diagnostics using
[the recorded toolchain libraries](qvm-loading-validation.md). Commercial 1.32c
valid output, flags, selection/fallback order and cycle behavior remain compatible.

## Remaining acceptance

Keep #48 open for encoded-message parsing, dictionary/cache factories, complete
library/world transactions and aggregate parser/work/memory budgets. The debug
reply test path may print earlier successful lines before a later line rejects;
this does not claim a transaction across that diagnostic batch. Retail/Mac OS 9
execution remains explicitly deferred.
