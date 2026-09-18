# Expression arithmetic bounds — 2026-09-18

PC_EvaluateTokens performs signed arithmetic/negation/shifts without range checks.
It computes unused integer fields during float evaluation, allowing a valid float
calculation to invoke signed overflow. Division checks both fields and rejects a
valid fractional divisor whose auxiliary integer is zero. Non-finite intermediates
can become an apparently valid comparison result; wider long-double operands
narrow to double without a representable-range check.

Check selected signed long addition/subtraction/multiplication and division/
remainder traps before operating. Native word shifts validate counts, use unsigned
left-shift bits and explicit arithmetic right extension. Native int-width unary
literal words use unsigned negation/byte transfer, retaining minimum/legacy word
patterns without signed negation overflow. Float mode skips unused integer math,
checks its actual divisor, bounds double narrowing and classifies copied volatile
representations for operands/results. Ternary integer conditions retain long width.
The shared finite reader also serves result token creation. Preserve native valid
selected arithmetic and public commercial 1.32c ABI/protocol/asset behavior.

## Validation

The runner calls actual evaluators and links source/lexer/libvar/character/core
bodies with physical heap interfaces. Seventeen original actual-body proofs fail:
signed add/subtract/multiply; minimum/-one division/remainder; out-of-width/negative
shifts; negative/high-bit left shifts; minimum int-word negation; unused signed
multiply during float evaluation; fractional division; non-finite product followed
by a finite comparison; out-of-double-range input, and a nonzero host long ternary condition truncated to
zero through int. The last case runs only where host long exceeds int width.
Original 4,536 defined
ordinary integer/float goldens pass.

5,184 ordinary calculations cover precedence, signed division/remainder, boolean/
comparison/bit operations, ternary selection, float arithmetic and fractional
divisors. Original float goldens omit divisors below one because the old helper
incorrectly rejects them. Boundary failures initialize selected outputs, record
source errors, retain four source owners and release all copied tokens. Valid
native negative/high-bit shifts, right extension, zero-count shifts, minimum words
and finite float products retain intended results. All teardown physically frees
source/token owners. Actual signed long bounds scale with the target width; PPC
long/int remain native 32-bit. No host-only word-width change is imposed on retail.

Clang ASan/UBSan and optimized GCC pass normal/release fast-math modes. Expression
token/collection, weight and character numeric parent fixtures pass both compilers/
modes. Five ledger/manifest checks, Bash syntax and diff checks pass; GCC/Clang CI
runs the new fixture. Both PPC products rebuild with zero compiler diagnostics
and valid PowerPC PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,775,317 | `e7c1746690438ad22a2f41cf8afd2b917beb07edda55dfd2947e964a0d525614` |
| Quake3_TeamArena | 3,927,987 | `f47a307e76da39d4a1e51fee8bcc455f5aba9414fa033d4b52e0c830bd3c7214` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #48 open for remaining factories/publication callers, file comment compression
and aggregate expression/parser work/recursion budgets. Retail/Mac OS 9 execution
remains deferred.
