# BSP projected-mark query traversal — 2026-09-17

This step for #45 removes C recursion from R_BoxSurfaces_r, the world-tree
query used to collect surfaces for projected marks. A valid 4,096-node crossing
tree previously overflowed the C stack under the fixture's 512 KB limit.

Walk already validated renderer decision-node parents with constant C stack
space and no query allocation or extra node metadata. Descend front first,
retest front ancestors when choosing a back subtree and stop at the requested
subtree root. Process shared opaque leaves through their current decision
owner, so their nonunique parent pointers are not followed. Direct leaf
queries retain their original behavior.

The existing leaf filter is extracted unchanged: plane intersection,
projection-angle rejection, NOIMPACT/NOMARKS/fog flags, accepted face/grid
types, view-count deduplication and list-capacity behavior stay native. No map
depth limit, file layout or commercial 1.32c module ABI changes.

## Validation

The existing graph fixture includes actual tr_marks.c and runs ASan/UBSan with
a 512 KB stack. Before the change, its new loaded 4,096-node crossing query
fails with stack-overflow in R_BoxSurfaces_r; afterward it passes.

Every accepted topology among all 15,625 three-node/two-leaf graphs queries
each subtree root for five boxes. Renderer mark counts/order match the unique
front-first leaf sequence from an independent stock recursive oracle restricted
to small trusted fixtures. Deep native renderer tests check real face/grid
surfaces, a separated face, an angled face, shader exclusions, unsupported
triangle surfaces, deduplication across shared leaves, exact list prefixes and
guards, zero capacity with unchanged view counters, front/back-only ordering
and direct leaf calls. Existing actual collision/query/loading/ownership tests
remain. Imported graphics and allocation callbacks stay isolated from devices.

All seven affected BSP sanitizer runners, nine Python checks and Bash syntax
pass. All 30 inherited CI runners remain. Both Retro68 products build without
compiler diagnostics and validate as PPC PEFs, using temporary toolchain
libraries from [loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,724,907 | `7e490d1c89cf1df97676157c5e61b1e9f5d09d36b3ed7c6c601a2f228e1a8e6c` |
| Quake3_TeamArena | 3,873,481 | `c9e006ff9fd22547964d4bdad3e8ee8f14f82a0653aff4acd380dabbec0e0632` |

## Remaining acceptance

Keep #45 open. Swept collision traces and renderer world traversal still
recurse. Derived geometry/facet limits, aggregate map-memory budgets and
complete transactional publication remain. Retail 1.32c projected marks and
Mac OS 9 visual/device acceptance stay deferred to the follow-up session.
