# Collision patch subdivision bounds — 2026-09-18

This step for #45 guards the two extra columns written by native collision
patch subdivision. A valid source at the 129-column grid limit previously
wrote beyond cGrid_t when curvature required further refinement. Check the
actual native capacity before moving or inserting any columns, on both axes.

Map preflight decodes already validated control spans bytewise and executes
the same refinement as the generator in one temporary native grid. Release
that workspace before returning an error. CM_LoadMap releases the input before
ERR_DROP, preserving the previous collision world, checksum and hunk state.
The direct generator also checks capacity before publishing collision geometry.

Keep native quadratic interpolation, subdivision distance, column collapse,
degenerate removal, transpose and wrapping order. There is no new compiler
limit or commercial 1.32c file/module/syscall layout change. This validation
adds one 199,708-byte temporary grid and repeats refinement during collision
loading; only one preflight grid is live at a time.

## Validation

The new ASan/UBSan fixture links actual cm_load.c, cm_patch.c, cm_polylib.c and
cm_test.c with native collision structures. Before this change its 129x3
curved patch reports stack-buffer-overflow in CM_SubdivideGridColumns.
Afterward overflow on either axis rejects cleanly at all four FS alignments,
and direct generator calls also reject before hunk/facet publication.

Fingerprints captured from the unchanged native generator match every bound,
plane and facet word for flat 3x3, 129x3 and 3x129 patches, and curved 127x3 and
3x127 patches that fit the subdivision limit. Both 129-axis flat inputs remain
accepted. Injected workspace allocation failure at all four FS alignments
releases the file and retains map/checksum/hunk state. All temporary grid and
winding allocations are accounted for and released.

All ten affected BSP sanitizer runners, nine Python checks and Bash syntax
pass. All 33 CI runners are registered in execution and syntax checks. The
host compiler reports the existing 32-bit pointer cast in CopyWinding; the
sanitizer fixtures pass. Both Retro68 products build without compiler
diagnostics and validate as PPC PEFs, using temporary toolchain libraries
from [loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,724,965 | `5070a41a9366e30b21d78d8bbe6908d1fd86d1aca80b3a40830f3d7bf2118d06` |
| Quake3_TeamArena | 3,873,539 | `3a3915a33966210f3d2b931f6329ebc20bc769ca1e91b9d2d3af16575e959789` |

## Remaining acceptance

Keep #45 open. Full derived plane/facet and numeric validation, aggregate
map/query-memory budgets and complete transactional publication remain.
These host fixtures do not exercise retail maps or Mac OS 9 devices; commercial
1.32c/PPC live acceptance stays deferred to the follow-up session.
