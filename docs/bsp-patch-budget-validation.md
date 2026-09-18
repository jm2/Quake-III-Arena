# Collision patch geometry budgets before publication — 2026-09-18

This step for #45 builds native collision planes, facets and bevels during
map preflight. The old MAX_FACETS/MAX_PATCH_PLANES errors happened after
checksum replacement and collision-world reset, while the input remained
allocated. Return explicit build failures and let the caller release the file
before ERR_DROP, retaining the previously loaded collision geometry.

Separate temporary geometry construction from hunk publication. The direct
generator also completes its build before allocating the patch header and
copies the same native arrays in their existing allocation order. Distinguish
degenerate planes from capacity/unresolvable-plane failures before indexing.

Guard every bevel-border insertion, reserve the native opposite-plane slot
and check that final insertion. Failure releases the live winding. Matching
planes in a full table remain reusable; the last legal plane and border still
fit. Keep native 2,048-plane/1,024-facet/26-border capacities and existing
interpolation, plane matching and facet validation. Commercial 1.32c layouts
and query-visible patch geometry are unchanged.

Preflight uses the existing temporary plane/facet scratch arrays and one
owned native grid. It restores build counts and debug-block coordinates;
persistent debug patch/facet pointers and loaded hunk copies are untouched.
The successful loader repeats native geometry construction during publication;
aggregate map/query budgets and complete failure rollback remain separate work.

## Validation

Actual cm_load.c/cm_patch.c/cm_polylib.c/cm_test.c ASan/UBSan fixtures reproduce
the old MAX_FACETS failure with the input still allocated. Now native facet
and plane budget cases reject at all four FS alignments before checksum,
world reset or hunk changes, releasing all file/grid/winding ownership and
retaining the loaded geometry fingerprint. Direct generator budget rejection
also precedes hunk publication.

Tests exercise both full native plane insertion paths, reuse of a matching
plane in a full table, the last legal plane, opposite-plane border capacity,
bevel reservation and winding release after a bevel-plane failure. Preflight
retains build counters, debug-block coordinates and persistent debug ownership.
All five captured unchanged-generator patch output fingerprints still match.

All eleven affected BSP sanitizer runners, nine Python checks, Bash syntax
and diff checks pass. The 34 registered CI runners are retained. Both Retro68
products build without compiler diagnostics and validate as PPC PEFs, using
temporary toolchain libraries from [loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,724,959 | `ed67281cde3beb694ac645fa41a1ec716ff895819f02368fa54032295522d295` |
| Quake3_TeamArena | 3,877,629 | `fc0ff733aad8bdd29b97e5f4a4d3967355dd278d637f4c5e828a12e841d497ba` |

## Remaining acceptance

Keep #45 open. Full derived numeric validation, aggregate map/query budgets
and complete transactional renderer/collision publication remain. Retail
commercial 1.32c and Mac OS 9 live acceptance stay deferred to the follow-up
session.
