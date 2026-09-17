# Botlib navigation syscall validation — 2026-09-17

The twelfth step for issue #35 validates the botlib common and AAS syscall
families before dispatch to native code. Strings must terminate in the VM;
structs, vectors, parser outputs, and arrays must fit their full size and
alignment. Count multiplication rejects negative or overflowing sizes.

Empty BBox, trace-area, and alternative-route output capacities return zero
without dispatch. Native BBox and trace-area implementations write their first
output before checking capacity, so merely accepting a zero-length array would
leave an overwrite. Trace-area point output remains optional. NULL map names
and entity states preserve native query/update and removal behavior.

Snapshot retrieval, console output, and bot user commands check client indices
against a saved count of actually allocated server clients. Both allocation
paths record this count. Later changes to the max-client cvar cannot enlarge
this bound. This does not finish the protected-cvar policy in issue #39.
Calls requiring the botlib API fail with a controlled VM drop when it is
unavailable; setup and shutdown retain their existing native handling.

## Validation

The ASan/UBSan fixture invokes the actual navigation dispatcher with an
exact-sized VM image and native callbacks that access complete outputs. It
covers bounded strings, full entity/area structures, array ranges and count
overflow, optional point outputs, empty capacities, NULL map/entity operations,
client indices outside the allocation despite a larger cvar, and absent API
handling. Rejections fault the game module before native dispatch and preserve
the complete VM image.

The portable host runners and all eight Python checks pass. Both Retro68
products build without compiler diagnostics and pass PEF validation with the
temporary libraries in the [loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,686,351 | `10d43c1a9d8818634a37a5b5cc88f091c093c395850c98a07483442946e9728c` |
| Quake3_TeamArena | 3,834,925 | `f114e8108c547ef4f7ae3c7bafe6b204169d48ac2a5aadae1d217ed39e4f8a7b` |

## Remaining acceptance

Keep #35 open. Botlib elementary-action and AI syscall pointer families,
scalar indices, and indirect native accesses still need review. Retail 1.32c
and Mac OS 9 live acceptance remain deferred to the follow-up session.
