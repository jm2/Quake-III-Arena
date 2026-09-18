# QVM cgame syscall validation — 2026-09-17

The ninth step for issue #35 applies the common VM range helpers to active
cgame pointer arguments: terminated strings, output buffers, cvar structures,
collision traces, sound vectors and axes, renderer structures and arrays,
game state, snapshots, user commands, parser output, and real-time queries.
The existing optional NULL semantics remain for cvar resets/registration,
file-length queries, point-trace bounds, sound origins/background tracks,
renderer color reset, real-time queries, and shader time offsets. Legacy
masked addresses and the trusted native module's 32-bit ABI remain.

Polygon batches validate both dimensions and their complete vertex range
before the renderer reads the array. Negative counts, product overflow,
alignment errors, and allocation crossings fault the QVM. Empty batches skip
the renderer. Mark-fragment requests validate all input/output arrays and
skip empty inputs or output capacities; a zero fragment capacity previously
could reach a native path that wrote its first fragment before checking the
limit. Invalid file-open modes and unsupported traps also drop a faulted QVM.
Font registration now returns instead of falling through to scene clearing.

## Validation

The new ASan/UBSan fixture calls the actual private cgame polygon and fragment
filters. Native renderer callbacks verify complete arrays, masked pointers,
dimensions, and valid return values. Rejected requests preserve the entire
exact-sized VM image and never invoke the renderer. Fixtures cover negative
counts, integer/byte-size overflow, exact-end batches, alignment, null/range
errors for each fragment array/vector, and empty submissions/capacities.

All eight sanitizer runners and eight Python checks pass. Both Retro68
products build without compiler diagnostics and pass PEF validation using
the temporary libraries in the [loading evidence](qvm-loading-validation.md).
Portable CI includes the new fixture; the target build compiles the complete
client dispatchers. The shared-helper and UI output regressions remain active.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,682,157 | `754acc0a1693c64594f2c1b74e7cf52cb82a383a08cdbb82fe9e7fe92dc4cc90` |
| Quake3_TeamArena | 3,830,731 | `4bde5303fb7ea3b3ace1c524a0fa636e90ef9132d2602daeb85e507441eed985` |

## Remaining acceptance

Keep #35 open. Server syscall ranges, persistent game-data registration, scalar
indices, and indirect native accesses still need review. Bounds do not impose
cvar or command permissions (#39). Retail 1.32c QVM and Mac OS 9 live acceptance
remain deferred to the follow-up session.
