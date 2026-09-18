# Renderer curve numeric preflight — 2026-09-18

This step for #45 executes the native renderer curve refinement during map
preflight. Finite source attributes previously interpolated into infinite
texture coordinates. Validate computed midpoints, deviation/projection,
normalization/distance, final vertices/attributes/normals, mesh bounds and
compiler-provided LOD bounds before map state, shader/model changes or mesh
allocation.

The preflight and generator share the same native operations and order.
Validation owns one 186,420-byte control/error workspace and releases it
before returning. Workspace failure returns to RE_LoadWorldMap, which releases
the input before ERR_DROP and retains renderer world/sun/model state. The
private generator also rejects invalid derived output before mesh allocation.
Native interpolation, color averaging, normals, orientation, tolerances and
commercial 1.32c file/module/syscall layouts remain unchanged.

Nodraw patches preserve their native early skip. Apply renderer grid capacity
and derived-control checks only to patches it actually refines; a valid
129-axis collision-only/nodraw patch avoids renderer workspace and mesh
allocation. Shared source spans, products and raw finite inputs still validate.

## Validation

The actual tr_bsp.c/tr_curve.c sanitizer fixture confirms the unchanged engine
publishes six nonfinite texture coordinates from finite controls. The fix
rejects seven finite-source mutation classes: midpoint overflow, squared
normalization overflow, opposite endpoint differences, interpolated texture
coordinates, interpolated lightmap coordinates, LOD-origin overflow and
LOD-radius overflow. Each rejects at all four FS alignments before shader,
model, hunk or world changes, releasing all input/workspace ownership.

Eight fingerprints captured from the unchanged generator match all vertices,
normals/colors, bounds/origins/radii and both LOD error tables. They cover flat,
curved, transposed and native 65-axis controls. Existing overbright/alpha
fixtures remain unchanged. Workspace failure rejects at all four FS alignments;
actual ParseMesh retains the nodraw skip for 129-axis controls with no refinement
or mesh allocations. All workspace allocations/frees balance.

All eleven affected BSP sanitizer runners, nine Python checks, Bash syntax
and diff checks pass. The 34 CI runners are retained. Both Retro68 products
build without compiler diagnostics and validate as PPC PEFs, using temporary
toolchain libraries from [loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,733,311 | `cea5adb5acc54d58b12e32b30c6f46d44dddaa494df256ead0b24e0fb69e3531` |
| Quake3_TeamArena | 3,881,885 | `4e0c9e7864ea1023790d66b62b2b87fe612a4ad4f40db26a54813dcc5b5ff98b` |

## Remaining acceptance

Keep #45 open. Other consumed geometry numeric validation, aggregate map/query
budgets and complete transactional renderer/collision publication remain.
Retail commercial 1.32c/PPC visual and device acceptance stays deferred to the
follow-up session; these fixtures isolate graphics imports.
