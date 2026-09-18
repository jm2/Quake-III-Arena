# BSP finite inputs and native geometry storage — 2026-09-17

This step for #45 follows header/reference validation with bytewise checks
before either loader changes state. Plane words, model bounds, all ten float
attributes per draw vertex and consumed surface plane/LOD/flare values must be
finite. Arbitrary four-byte vertex colors are excluded from float checks.
Model bounds remain ordered. Other unused surface metadata stays ignored.
Draw vertices are scanned once, avoiding repeated shared-span work.

Quadratic patch dimensions are odd and at least three, matching the native
collision generator. Dimension/storage checks use actual caller capacities:
collision grids have 129 rows/columns and the loader's 1,024 control vectors;
renderer grids have 65 rows/columns and its 1,024-vector input buffer. Products
are checked by division before multiplication and fit the declared, already
range-checked vertex span. Collision's grid constant is shared with its native
generator. Compiler dimension defaults are not imposed on safe native inputs;
65x15 renderer controls and 129x3 collision controls remain supported.

Renderer face/triangle counts fit its existing shader tessellation arrays
(less than 1,000 vertices and 6,000 indices). Faces need the first point used
by plane construction; empty triangle surfaces remain accepted. Face storage
uses offsetof plus actual point/index sizes. Remove the old 64-point truncation
that could leave indices referencing omitted points; every validated point is
copied, supporting full 999-point faces with matching indices and alpha.

## Validation

ASan/UBSan executes actual CM_LoadMap and RE_LoadWorldMap for structure-aware
malformed geometry. Valid headers/references precede every mutation. Positive/
negative infinity and NaN cover every plane word, model bound, vertex float,
patch LOD bound and consumed face/flare value. Reversed bounds, signed/zero/
even/short patch dimensions, mismatched control counts, native capacity excess,
zero-point faces and tessellation limits reject at all four FS alignments.
Input is released before ERR_DROP; the loaded collision map and checksum,
renderer globals/cached world data, shader/model callbacks and hunk/temporary
ownership remain unchanged. Renderer acceptance of complete valid worlds is
not claimed by these rejection tests.

The fixture also runs actual ParseFace, ParseTriSurf, ParseMesh and tr_curve.c
subdivision with real renderer structures. Goldens check 999-point faces and
999-vertex triangles with 5,999 indices, exact native allocation/index layout,
complete copied points/indices, preserved color alpha, one-point/empty shapes,
31x31 and 65x15 patches, real subdivided endpoints/finite normals and released
curve heap ownership. Imported shader/image/model and allocation callbacks
are isolated; the native collision patch generator is stubbed. Native 129x3
collision controls are validated without that backend execution.

All four BSP fixtures, nine Python checks and Bash syntax pass. The prior
face pointer-to-integer host warning is removed. Both Retro68 products build
without compiler diagnostics and validate as PPC PEFs with merged master
`4fd62bd` integrated, using temporary libraries from
[loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,716,501 | `e07d594238154fe2b5270094eaaeda94720af7070747ed72fafe3cfaf6369e65` |
| Quake3_TeamArena | 3,865,075 | `6c623e2e37fb27898c6cdc0f8b4db9b9edc74c48bbadee1e2ad3eb488fdcc1d5` |

## Remaining acceptance

Keep #45 open. Graph termination, derived floating-point geometry/facet limits,
entity/grid parsing, aggregate/runtime model capacity and complete transactional
publication still need validation. Finite inputs alone do not guarantee finite
results from every legacy computation. Retail models/maps/rendering and Mac
OS 9 device/visual acceptance remain deferred to the follow-up session.
