# BSP tree topology and parent initialization — 2026-09-17

This step for #45 validates every node component after layout/references and
before either loader resets world state, checksums input or allocates native
map storage. The renderer stores one parent per decision node and visible
leaf. Decision nodes have one incoming edge at most; world node zero has
none. Visible leaves may be used twice by the same parent, but cannot have
different parents. Cluster -1 leaves have no PVS ancestor walk and remain
shareable. Invalid aliases can otherwise make recursive initialization repeat
exponentially or make visible ancestor paths ambiguous.

Separate inline-model trees remain accepted. The original
[q3map emitter](https://github.com/id-Software/Quake-III-Arena/blob/master/q3map/writebsp.c)
emits node/leaf records recursively and emits trees for individual models.
Inspect every forest component, including unreachable closed cycles, without
imposing compiler node budgets or a maximum depth. A single recoverable heap
workspace holds parent markers and a queue, with actual unsigned-word storage
checked against signed allocator capacity. Unique incoming edges bound queue
insertion; root processing detects cycles in linear node/leaf work. Workspace
and FS input are released before controlled errors, preserving the old map,
checksum, renderer globals, callbacks and allocation ownership.

Renderer parent initialization now clears parents and assigns validated child
links in a linear pass instead of recursively walking from the world root.
Forest roots retain NULL parents. A shared invisible leaf's parent is not
used by the PVS ancestor walk; visible ancestors remain unambiguous.

## Validation

ASan/UBSan runs actual CM_LoadMap, RE_LoadWorldMap and iterative
CM_PointLeafnum queries with real renderer/collision structures. Successful
minimal worlds and three-model forests publish complete native parent links
and retain inline roots. Real cm_test.c also supplies native area flooding;
shader/image/model and allocation callbacks are isolated. Collision patch
backend callbacks remain stubbed and these maps have no patches.

Legal-reference mutations cover root/self/mutual cycles, unreachable closed
cycles, duplicate decision edges, acyclic diamonds, visible-leaf aliases and
39 levels that would recursively repeat 2^39 paths. Both actual loader entry
points reject at four FS alignments before map/checksum/hunk/shader/model
changes, freeing input/workspace before ERR_DROP. Injected workspace OOM
preserves both loaded worlds. Every one of the 15,625 three-node/two-leaf
graphs is checked against an independent small recursive topology oracle.

A 4,096-node valid chain runs through both actual loaders and point queries
with a limited 512 KiB test stack. Complete native parents/visible ancestors
are checked. A 131,073-node chain, above the compiler default, passes layout,
reference/geometry/tree and actual native allocation preflight without a
depth/compiler cap; that larger map is not allocated by the native loaders
in this fixture. All temporary allocations are balanced.

All six affected BSP sanitizer fixtures, nine Python checks and Bash syntax
pass, retaining all 29 inherited CI runners. Both Retro68 products build
without compiler diagnostics and validate as PPC PEFs. Merged QVM/CI changes
through #69 and RoQ #70 are present; the final direct-float parent #84 is
integrated. Builds use temporary libraries from
[loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,720,783 | `5dffe0ac18ba25ade06159c11d83b75e67e300236d154d98dd2ce496e33fb167` |
| Quake3_TeamArena | 3,869,357 | `637910aae5b52e6d46c026c632dcc13f1c12819e0708d1f0f8cf0de835c1b19b` |

## Remaining acceptance

Keep #45 open. Some legacy spatial rendering/collision queries still recurse
on valid deep trees; their runtime stack safety needs its own step. Derived
geometry/facet limits, aggregate/runtime model capacity and complete
transactional publication remain. Retail 1.32c map/mod rendering and Mac OS 9
visual/device acceptance stay deferred to the follow-up session. This step
establishes bounded loader graph validation and parent initialization.
