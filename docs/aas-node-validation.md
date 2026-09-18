# AAS node references and graph termination — 2026-09-18

The native loader publishes cyclic nodes and bad node plane/leaf references.
Before loaded publication, require root node storage, matching area/settings
counts, paired node planes and valid positive-node/negative-area references.
Reject INT_MIN leaves before negation. Ignore the unused native dummy node 0.
Use iterative Kahn traversal of every component to prove termination, including
unreachable components, without recursion or changing native node bytes.
Shared acyclic graphs remain supported.

The temporary indegree/queue workspace uses the existing heap adapter whose
signed import/prefix checks and nullable handling are covered by #123. Release
it on success/cycle rejection, and reject allocation failure without ownership.
This workspace is physically freed; native hunk data still releases logically.
No retail v4/v5 layout, QVM/syscall or commercial 1.32c ABI changes are introduced.

## Validation

Two original actual-loader proofs fail: a self-cycle and invalid node plane
still report success. New actual-body checks cover self/indirect/disconnected
cycles, positive-node/INT_MIN/negative-area and plane limits, missing root and
area/settings mismatch in both versions, plus failed workspace allocation.
Ten chain/shared-graph cases through 256 nodes retain every native node byte
and release temporary workspace. The runner extracts the actual unchanged
AAS_PointAreaNum body through a strict source seam and retains area/solid results
in normal/release fast-math sanitizers. Optimized GCC graph checks also pass.

Layout fixtures now retain required solid-world node/plane/area/settings roots
when optional lumps are empty; structurally empty old layouts no longer qualify
as loaded worlds. Exact header/lump/range/read/allocation and geometric rejection
fixtures remain passing. Earlier layout/geometry documents retain their dated
acceptance observations. Nine Python checks, Bash syntax and diff checks pass.
Both Retro68 products build with zero compiler diagnostics and validate as PPC
PEFs; build manifests/logs confirm the changed bot sources compile in both.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,750,003 | `6dde1c951dcfaa8561e036d3651588b2f448277ce157d8a900c6e5f1f1433e62` |
| Quake3_TeamArena | 3,898,577 | `2fb7e2d61d284821110a1a8c7d343dfe9b64c38a365f478b0f37c6d435cb33f5` |

After integrating both #123 review follow-ups, node and native engine zone
sanitizer checks pass. Both PPC products rebuild with zero compiler diagnostics;
the table records the artifacts with the native engine overhead checks.

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #47 open for travel-type-dependent reachability/portal/cluster references,
derived numeric/runtime safety, aggregate native hunk/work budgets, physical
arena recovery and transactional late-load replacement, plus complete mover/
entity acceptance. Acyclic shared graphs can still cause excessive repeated
runtime traversal; termination validation is not a query-work budget. Retail
assets and Mac OS 9 execution remain deferred and bots stay disabled.
