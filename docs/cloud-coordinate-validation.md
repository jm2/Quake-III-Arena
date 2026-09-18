# Cloud-layer finite geometry and deferred publication — 2026-09-18

This #46 step retains the commercial 1.32c shader/QVM interfaces and native
stable cloud tables. Stage all 486 intersection parameters/UV pairs before
publishing either global array. Keep the original float formula/normalization
when it produces finite results. If native float products/norm overflow, compute
the same sphere intersection in double: finite float heights and fixed native
sky vectors fit those double products. Normalize in double and bound cosine
inputs before the existing native Q_acos helper. Extreme finite heights retain
their intended sphere instead of receiving an arbitrary height cap.

Layers without a real intersection and direct non-finite inputs use a complete
native 512-unit default layer. Partial arrays are never published. Existing
zero-height parser default and native zFar initialization remain unchanged.
Delay cloud initialization until the entire definition parses successfully.
Later failures retain prior tables/zFar; multiple accepted sky fields publish
the final height once. Native outer/inner image suffix order stays unchanged;
initialization now follows those imports and full parsing, superseding the
older metadata step's initialization timing. Sun publication stays deferred.

## Validation

Unchanged actual bodies produce non-finite tables for FLT_MAX and negative
one-unit layers. The unchanged actual parser initializes cloud state before a
later definition failure. These original failures are recorded before edits.

Expanded normal and release-fast-math ASan/UBSan/float-cast-overflow fixtures
execute actual sky bodies and extracted native Q_acos, compare copied pre-fix
complete tables for 12 stable positive/zero/negative heights (5,832 points),
and retain the earlier 1,122 drawing/indexed-geometry oracles. Normal parameters
and UVs match native values exactly. Optimized reassociation changes final bits:
parameters stay within eight float epsilons times max(1, native magnitude),
normalized UV directions within four float epsilons. Compare cosine directions
because acos near an endpoint amplifies a final-bit normalized rounding change.

For each of six extreme finite heights, independently check all 486 points lie
on the intended sphere within 16 float epsilons times its radius and UV cosines
match normalized intersection directions within eight epsilons. These invariants
do not repeat the production intersection formula. Check three no-intersection
layers and six non-finite bit patterns publish the complete default table.
Actual parser tests cover three later rejection classes and duplicate accepted
sky fields. Prior metadata/stage/cache/native behavior remains covered.

Both expanded sky/stage sanitizer configurations, separate normal/fast GCC
checks, related runtime/archive runners, nine Python checks, Bash syntax and
diff checks pass without diagnostics. Both shipped Retro68 products build
without diagnostics and validate as PPC PEFs.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,745,843 | `96c3af5674c9c268580b2d9c691a0d3aabd0149fd15ac928a98ab96a497849f8` |
| Quake3_TeamArena | 3,894,417 | `33aa6fe61bfeff576a31904c1cdb2ff0d81f35587cca0473f8c6f6a6bc959bb2` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #46 open through remaining renderer input/cleanup checks and deferred
commercial 1.32c/Mac OS 9 live acceptance. GPU/rendering imports are isolated;
these checks establish CPU geometry/ownership/publication, not live visuals.
Renderer-wide budgets/transactions remain #45. Optional FreeType stays disabled
in shipped products; earlier evidence remains dated.
