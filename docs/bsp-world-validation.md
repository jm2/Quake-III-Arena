# BSP world traversal and lighting masks — 2026-09-17

This step for #45 removes C recursion from R_RecursiveWorldNode. Valid deep
worlds previously exhausted the C stack while visiting front children. Keep
explicit pending back children and their inherited frustum/dynamic-light masks,
using 64 inline frames and checked temporary growth bounded by the real
validated decision-node count and signed native allocation ceiling.

Preserve native visibility/frustum rejection, front-first draw order, shared
surface first-visit deduplication, visible bounds and dynamic-light filtering.
Resuming a pending sibling restores its own masks after a front subtree is
culled. Completion/rejection and failed growth release owned temporary storage;
growth failure cleans up before ERR_DROP. No map depth limit, node metadata,
file format or commercial 1.32c module layout/syscall changes.

The audit also found signed shifts for light bit 31 and an oversized shift
when constructing all 32 bits. Use unsigned bit operations in world filtering,
brush lighting and the backend's projected-light membership test. Construct
the full 32-bit mask explicitly, preserving its native field bit pattern and
the existing upper count clamp; negative internal counts safely become zero.

## Validation

The new fixture executes actual tr_bsp.c, tr_curve.c, tr_world.c and tr_light.c
with real renderer structures under ASan/UBSan and a 512 KB stack limit. Before
the change its valid loaded 4,096-node world reports stack-overflow in
R_RecursiveWorldNode; afterward it passes.

96 comparisons against the original recursive traversal on a tiny trusted
world check exact visibility/frustum masks, draw order/fog/light booleans,
surface and face state, frontend counters and visible bounds. Deep goldens
retain every leaf visit and native first-visit shared-surface lighting behavior;
frustum rejection resumes siblings safely. Failure at each of six temporary
buffer growths releases storage and retains the loaded world. Ordinary queries
stay allocation free; replacement owns at most two temporary buffers.

Actual R_AddWorldSurfaces and R_DlightBmodel check every light count zero through
32, face/grid/triangle bit fields, the highest bit alone, boolean state and
count clamps. The one-literal backend membership correction is compiled in
both PPC products; GPU shading/visual behavior is not exercised by these host
fixtures. Graphics draw and primitive-cull imports are isolated. World frustum
culling, face filtering and dynamic-light calculations are executed directly.

All nine affected BSP sanitizer runners, nine Python checks and Bash syntax
pass; all 32 CI runners remain/are registered. Both Retro68 products build
without compiler diagnostics and validate as PPC PEFs, using temporary
toolchain libraries from [loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,724,947 | `90d97919ec1a4f5055b99e5f39575a1baa6ec701a47207c772f2b147269ac3b3` |
| Quake3_TeamArena | 3,873,521 | `466ab10a2ad52b468ed6437ab1470dc7beb3eeedcb4dce2165c8ee791a926bba` |

## Remaining acceptance

Keep #45 open. Derived geometry/facet limits, aggregate map/query-memory
budgets and complete transactional map publication remain. Retail 1.32c world
rendering/projected lighting and Mac OS 9 visual/device acceptance stay deferred
to the follow-up session.
