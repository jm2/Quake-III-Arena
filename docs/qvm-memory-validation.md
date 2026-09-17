# QVM runtime memory validation — 2026-09-17

The fourth step for issue #35 checks complete memory ranges after applying
legacy VM address masks. LOAD2/LOAD4 reject accesses crossing the data-image
boundary and use native-order byte copies for unaligned reads. STORE2/STORE4
retain their existing address masking and alignment. Stack arguments must be
aligned and fit without wrapping. BLOCK_COPY rejects negative, oversized,
wrapping, or unaligned ranges before copying, accepts ranges ending exactly at
the image boundary, and handles overlap with `memmove`.

Syscall dispatch now receives a local 16-word argument snapshot. Available VM
words are copied and missing words are zeroed so a short frame cannot expose
native memory beyond the data image. This bounds the dispatcher's argument
array; it does not validate syscall arity or the pointers/ranges described by
those arguments. LOCAL address calculation uses unsigned arithmetic to retain
32-bit wrapping without signed overflow.

## Validation

The existing execution harness now covers boundary-crossing loads, unaligned
in-image loads, masked/aligned stores, last-byte accesses, invalid and valid
ARG offsets, negative and overflowing copy lengths, both overlap directions,
zero-length copies, exact-end source/destination ranges, LOCAL overflow, and
short/full syscall argument snapshots. Recursive calls and forged syscall
returns remain covered.

All four ASan/UBSan runners and all eight Python checks pass. The interpreter
now compiles without the previous host pointer-width diagnostics. Portable CI
runs the expanded runtime harness. Both Retro68 products compile without
warning/error diagnostics and pass PEF validation with the temporary host
libraries documented in the [loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,673,829 | `ebaa6c83fd6432f11e16f0e2cb09f668fb274c76232e657f4cc3b2bfaf92e453` |
| Quake3_TeamArena | 3,822,403 | `91785bfb29519f66f3453f4fd08c8dc251e3deba947b4e59ac291d61cf7df701` |

## Remaining acceptance

The [next step](qvm-arithmetic-validation.md) covers arithmetic edge cases.
Keep #35 open: syscall-specific pointer/range checks and VM call argument
marshalling still need work. Retail baseq3/Team Arena
execution and Mac OS 9 performance/compatibility remain deferred to the user's
live-testing session. These fixtures establish the covered memory behaviors,
not a complete sandbox guarantee.
