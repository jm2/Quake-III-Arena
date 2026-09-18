# Derived collision patch numeric validation — 2026-09-18

This step for #45 rejects nonfinite values produced by native collision
patch arithmetic from finite inputs. A finite 3x3 control grid spanning
1e20 units previously loaded successfully and published two nonfinite plane
words. Raw-input finite checks alone did not protect derived geometry.

Check native float bits in the shared refinement/build before publication.
Validate curve midpoints, deviation/distance, edge differences, cross products,
normalization and plane distance. Distinguish numeric failures from native
degenerate triangles and capacity failures before indexing. Check generated
and clipped winding vertices and release their ownership on failure; bevel
plane failures preserve their numeric error classification.

CM_LoadMap releases the BSP input and preflight workspace before rejection,
preserving the loaded world/checksum/hunk geometry. The direct generator also
rejects before hunk publication. Existing finite native operations, matching,
tolerances and file/module/syscall layouts remain unchanged. No arbitrary
coordinate or compiler limits are introduced.

## Validation

The actual native collision/winding ASan/UBSan fixtures first confirm the old
accepted finite controls publish two nonfinite values in one plane. The fix
rejects those controls before publication. Five finite-source mutation classes
cover cross-product overflow, squared normalization overflow, opposite endpoint
difference overflow, positive midpoint overflow and squared curve-distance
overflow. Each rejects at all four FS alignments, retaining the loaded patch
fingerprint and releasing all input/grid/winding ownership.

Direct plane candidate tests cover positive/negative infinity and NaN in every
plane field before matching or insertion. Native valid bounds/planes/facets
still match all five fingerprints captured from the unchanged generator;
fitting 129-axis flat and 127-axis curved controls remain accepted. Existing
native plane/facet/border limits, winding ownership and workspace failure
regressions continue to pass.

All eleven affected BSP sanitizer runners, nine Python checks, Bash syntax
and diff checks pass. The 34 CI runners are retained. Both Retro68 products
build without compiler diagnostics and validate as PPC PEFs, using temporary
toolchain libraries from [loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,729,073 | `dc42501fe7ed9e00db78a831756361e7540f4249968e89d47eabf56ac40ca211` |
| Quake3_TeamArena | 3,877,647 | `ba59fa28531b35d14f09d875602ea93c883daf173c6386354ba93ee6aa53de98` |

## Remaining acceptance

Keep #45 open. Renderer and other consumed geometry numeric validation,
aggregate map/query budgets and complete transactional publication remain.
Retail commercial 1.32c and Mac OS 9 live acceptance are deferred to the
follow-up session.
