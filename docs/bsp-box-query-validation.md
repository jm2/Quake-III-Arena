# BSP collision box-query traversal — 2026-09-17

This step for #45 removes C recursion from CM_BoxLeafnums_r, shared by leaf
listing, brush listing and position-test leaf collection. A valid 4,096-node
crossing tree previously overflowed the C stack under the fixture's 512 KB
stack limit. Preserve valid deep trees and their native query results rather
than imposing a new map depth limit.

CMod_LoadNodes records one signed parent index per validated decision node;
forest roots retain -1. Shared opaque leaves need no parent. The query descends
front first and ascends decision-node parents, retesting a front ancestor's
plane when needed to choose its back subtree. Bounds stay constant in native
leaf/brush callbacks. A query starting inside the tree stops at its requested
subtree root, including roots that have a parent in another part of the tree.
Direct leaf calls retain the original behavior.

Traversal uses constant C stack space and makes no query-time allocation.
Each visited decision edge is followed a bounded number of times. The internal
collision node gains one 32-bit parent word on PPC; existing allocation
preflight uses the resulting actual sizeof(cNode_t). This is engine-private
metadata and changes no commercial 1.32c module structure, syscall or file ABI.

## Validation

The actual cm_test.c query functions run in the existing graph fixture under
ASan/UBSan with a 512 KB stack. Before the fix, the new loaded 4,096-node test
fails with AddressSanitizer stack-overflow in recursive CM_BoxLeafnums_r;
afterward it passes, preserving every front-first visible/opaque callback.

Every accepted topology among all 15,625 three-node/two-leaf graphs runs
queries at each of its three subtree roots for five boxes, compared with an
independent stock recursive traversal restricted to these small trusted
fixtures. Native collision loading also checks every decision-node parent.
Deep goldens cover front-only/back-only/crossing boxes, exact bounded-list
prefixes and guards, native last-visible-leaf tracking after overflow,
zero-capacity lists, direct leaf calls and real brush deduplication across
repeated shared leaves. Actual collision and renderer loading, graph cleanup
and world ownership tests remain.

All seven affected BSP sanitizer runners, nine Python checks and Bash syntax
pass. All 30 inherited CI runners remain. Both Retro68 products build without
compiler diagnostics and validate as PPC PEFs, using temporary toolchain
libraries from [loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,724,907 | `41098c93e42e66351e12ec0317530255057324487813a70e136a269ca61aafe1` |
| Quake3_TeamArena | 3,873,481 | `a345840e55a1b5aeb897cce1eb5e17b508d75c4a645052fa8c7da6f044a671b7` |

## Remaining acceptance

Keep #45 open. Swept collision traces, renderer world traversal and projected
mark queries still recurse. Derived geometry/facet limits, aggregate map-memory
budgets and complete transactional publication remain. Retail 1.32c map/mod
collision and Mac OS 9 acceptance stay deferred to the follow-up session.
