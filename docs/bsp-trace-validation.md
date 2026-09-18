# BSP swept collision traversal — 2026-09-17

This step for #45 removes C recursion from CM_TraceThroughTree. A valid
4,096-node tree with overlapping near/far segments previously overflowed the
fixture's 512 KB C stack. Walk explicit pending far segments, keeping 64 inline
frames for ordinary traces and growing temporary storage only when needed.
Each frame retains its node, endpoint vectors and endpoint fractions.

Preserve the stock distance, extent, epsilon and near/far clipping expressions
and visit the near subtree before the saved far subtree. Recheck nearest-hit
pruning when each segment resumes. Copy initial vectors into the current frame
so their callers' aliased tw.start/tw.end stay unchanged.

Growth is bounded by the real validated decision-node count, checked before
multiplication against the existing signed native allocation ceiling. Failed
growth frees owned storage before ERR_DROP; completion and nearest-hit pruning
also release it. One-sided deep traces never need pending frames. No map depth
limit, file format, trace result layout or commercial 1.32c syscall ABI changes.
This step does not introduce an aggregate map/query memory budget.

## Validation

A new ASan/UBSan runner compiles actual cm_trace.c separately with controlled
allocation imports, alongside actual collision loading and cm_test.c. Before
the fix, its valid 4,096-node test reports stack-overflow in CM_TraceThroughTree;
afterward it passes with a 512 KB stack limit.

576 exact structure comparisons cover eight start/end positions, point/box/
capsule shapes and three tiny trusted worlds, including varied axial/nonaxial
planes and a balanced tree. Results and unchanged input vectors match the
original recursive traversal, which is used only on these tiny worlds. Public
CM_BoxTrace goldens check the native 0.21875 hit fraction, epsilon endpoint,
plane, contents, escape from solid and all-solid position test.

Deep tests cover overlapping segments, allocation-free one-sided traversal,
near-hit pruning and failure at each of six buffer growths. Every failure frees
all temporary buffers and retains the loaded world; normal buffer replacement
owns at most two buffers. Actual brush collision/clipping is executed. Patch
callbacks are isolated and these worlds contain no collision patches, so patch
facet correctness remains separate work.

All eight affected BSP sanitizer runners, nine Python checks and Bash syntax
pass; all 31 CI runners are retained/registered. Both Retro68 products build
without compiler diagnostics and validate as PPC PEFs, using temporary
toolchain libraries from [loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,724,935 | `a33328fa93ab40b0cad06fd8fd3fbe2bcac39b6680125bd6a2c7694b856026e5` |
| Quake3_TeamArena | 3,873,509 | `4e129b996619b5121504713d9a3fa3cf9b7792ec21637675349d35d4a73bd868` |

## Remaining acceptance

Keep #45 open. Renderer world traversal still recurses. Derived geometry/facet
limits, aggregate map-memory budgets and complete transactional publication
remain. Retail 1.32c map/mod collision and Mac OS 9 runtime acceptance stay
deferred to the follow-up session.
