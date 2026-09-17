# Botlib navigation syscall validation — 2026-09-17

The twelfth step for issue #35 validates the botlib common and AAS syscall
families before dispatch to native code. Strings must terminate in the VM;
structs, vectors, parser outputs, and arrays must fit their full size and
alignment. Count multiplication rejects negative or overflowing sizes.

Empty BBox, trace-area, and alternative-route output capacities return zero
without dispatch. Native BBox and trace-area implementations write their first
output before checking capacity, so merely accepting a zero-length array would
leave an overwrite. Trace-area point output remains optional. NULL map names
and entity states preserve native query/update and removal behavior. NULL
reachability origins preserve the total-area query; NULL travel origins
preserve cached area-to-area travel-time queries.

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
nullable reachability/travel queries with checked non-NULL origins,
client indices outside the allocation despite a larger cvar, and absent API
handling. Rejections fault the game module before native dispatch and preserve
the complete VM image.

The portable host runners and all eight Python checks pass. Both Retro68
products build without compiler diagnostics and pass PEF validation with the
temporary libraries in the [loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,686,351 | `d9d0ab88691eb6e3becc8437e6c3fb8771d300aabbb3fd06be3194668c2d98d2` |
| Quake3_TeamArena | 3,834,925 | `ecc0ca7996e4ac592f0d1592f57c4cb0c3b423ca6dc51efdaffa67d4f279fde0` |

## Remaining acceptance

Keep #35 open. Botlib elementary-action and AI syscall pointer families,
scalar indices, and indirect native accesses still need review. Retail 1.32c
and Mac OS 9 live acceptance remain deferred to the follow-up session.
