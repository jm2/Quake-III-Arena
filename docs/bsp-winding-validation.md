# Native collision winding ownership — 2026-09-18

This step for #45 releases the temporary winding when CM_ValidateFacet
encounters a missing border. The previous early return leaked the winding
created or clipped for that facet; repeated rejected facets retained zone
storage. Free the live winding before returning the same rejection result.

CopyWinding now calculates the native header/point byte count using offsetof
and sizeof. The old expression constructed a point address from a null
winding and converted that pointer to int. Retain the existing native
allocation size, point order and independent copy ownership without relying
on the 32-bit pointer cast. Commercial 1.32c file/module/syscall layouts are
unchanged.

## Validation

The ASan/UBSan fixture executes actual cm_patch.c facet validation and
cm_polylib.c winding operations. Before this change it fails its ownership
check immediately on a missing border; afterward it passes 128 repeated
rejections at each of the four border positions, including prior clipping.
Successful bounded facets, missing surface planes, completely clipped facets
and excessive bounds retain their native result and release owned storage.

Copies of every point count zero through 64 check exact native allocation
sizes and every header/coordinate byte. Editing a copied endpoint leaves the
original independent. All original/copy storage and native active-winding
counters balance after release. Existing captured collision-patch output
fingerprints continue to match the unchanged generator.

All eleven affected BSP sanitizer runners, nine Python checks and Bash syntax
pass; all 34 CI runners remain registered in execution and syntax checks.
Both Retro68 products build without compiler diagnostics and validate as PPC
PEFs, using temporary toolchain libraries from
[loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,724,965 | `392a568e1a42b50474b2a5173fc1fa33524825872ee0cd230e5b7ba8663b5541` |
| Quake3_TeamArena | 3,873,539 | `f487ba08c4601f17448c035f108d36c9d986dc654452e4234a1c88a8d4ae2966` |

## Remaining acceptance

Keep #45 open. This step does not complete derived plane/facet/numeric
validation, aggregate budgets or transactional world publication. Retail
commercial 1.32c and Mac OS 9 live acceptance remain deferred to the follow-up
session.
