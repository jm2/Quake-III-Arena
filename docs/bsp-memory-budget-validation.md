# Collision BSP aggregate memory preflight — 2026-09-18

This step for #45 checks the complete collision-map permanent allocation cost
against Hunk_MemoryRemaining while the input file is still live. Use the native
checked alignment/debug-header query for every allocation and subtract costs
from available capacity without an overflowing aggregate sum. Reject and free
the file before checksum, map reset, patch clearing or hunk publication.

Account for all direct arrays and reserved box-hull elements, area storage and
the square portal matrix, entity termination, explicit visibility including its
header or the native novis byte rounding, each inline model's synthesized brush
and surface indexes, and every patch descriptor/header/facet/plane allocation.
The shared native patch preflight reports its actual successful output sizes;
failed queries leave output sizes untouched. Repeated source spans still incur
each native allocation. Zero-sized allocations use the allocator's real cost.

There is no new compiler limit or arbitrary map cap. Small patches are charged
for their actual generated geometry. Native allocation order, geometry output,
commercial 1.32c layouts, module/syscall ABIs and protocol remain unchanged.
The BSPC path retains structural/geometry validation without depending on the
engine's runtime hunk budget.

## Validation

The unchanged loader proceeded with 1,920 bytes of native allocation calls
when the fixture reported one byte available, replacing the old map state.
The actual collision/patch fixture independently measures allocation costs
from native calls, then checks exact-fit loading and rejection one byte below
that cost. Nine maps cover basic arrays, the highest ABI area/portal matrix,
novis rounding, explicit visibility headers, repeated inline indexes, repeated
patches, zero-sized inline/opaque-leaf arrays and native 129-axis patches.

Over-budget, zero-capacity and negative-capacity inputs reject at all four FS
alignments without checksum, map, hunk, file/grid/winding ownership changes.
Successful native loads use aligned FS input and consume exactly the measured
cost. The fixture also enforces the reported remaining capacity on every native
allocation and retains all existing collision plane/facet fingerprints.
Failed patch allocation-size queries publish no output.

All 11 affected BSP sanitizer runners, the release/debug native hunk
runner, nine Python checks, Bash syntax and diff checks pass. Both Retro68 products build without compiler diagnostics and validate as PPC
PEFs, using temporary toolchain libraries from [loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,733,323 | `1f245997aa4105baf960ae5fc42822ab0b8207453c5c9c3d004d777b63aa3d1f` |
| Quake3_TeamArena | 3,881,897 | `bc3c73bca162cbc0d4eca5ca08078dc69419ef0bb65c8576878d5b88efdeccda` |

## Remaining acceptance

Keep #45 open. Renderer/query budgets, other consumed geometry validation and
full transactional publication remain. Retail commercial 1.32c and Mac OS 9
live acceptance remains deferred to the follow-up session.
