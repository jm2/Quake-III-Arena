# MD3 layout validation — 2026-09-17

This first step for issue #44 retains FS file length and reads identification
bytewise after checking its complete span. A header-only validator checks
all MD3 file/surface sections before payload hunk allocation, copying or
native endian conversion. File input may be unaligned; offsets must align
for their native copied types. Size/count arithmetic checks complete ranges
by division and subtracts limits before forming pointers.

Frames, tags and surface chains are disjoint; each surface has forward
progress and disjoint triangle/shader/ST/XYZ-normal arrays. Counts retain
the format's frame/tag/surface/shader limits and the renderer's strict tess
limits (at most 999 vertices and 1999 triangles per surface). Surface frame
counts match the header. Consumed surface/tag/shader names terminate in
bounds; triangle indexes fit vertices. Culling metadata, tags and texture
coordinates are finite, with nonnegative radii; frame bounds may be inverted,
as q3data's cleared bounds on tag-only hand models are (#244).
Unused fixed-width frame labels and reserved flags are not interpreted as
strings or array references. Native copying/conversion retains packed vertex
normals, surface naming and shader registration behavior. Tag lookup also
clamps negative frame indexes before pointer arithmetic, preserving the
existing upper-frame fallback.

Registration stages candidate MD3 LOD files before allocating payloads.
A malformed primary model releases every staged file and keeps only the
empty MOD_BAD cache entry; it cannot retain a previously loaded coarse MD3.
Malformed optional MD3 LODs or incompatible frame counts produce a warning
and fall back to compatible data. Missing LOD slots use the nearest available
coarser model or most detailed fallback, including coarse-only registration.
The LOD count reflects the highest available slot, and every render slot
has a valid pointer. Aggregate model byte counts validate before payload
allocation. All staging buffers release after either outcome.

MD4 is still a distinct unfinished step of #44. Its identification/header
is bounded here, is selected only by the requested base file and cannot mix
with MD3 payloads, but its variable bones, weights and LOD layout still require validation and correct native conversion.
This MD3 step does not close the model-loader security issue.

## Validation

ASan/UBSan executes actual RE_RegisterModel and R_LoadMD3; only renderer imports
and unused GL entry points are isolated. Exact input allocations at all four
alignments test every prefix of a small two-frame/two-surface model. Golden
copies check header/frame/tag values, lowercase names, shader indexes,
triangle indexes, ST values and packed vertex/normal words. The input remains
unchanged. Cases exercise maximum frame/tag/surface/shader and native tess
counts without oversized output allocation. A tag-only 16-frame model with
q3data's cleared (inverted) bounds registers at all four alignments.

Malformed cases include negative/huge offsets and counts, zero surface
progress, mismatched frames, overlapping/misaligned sections, unterminated
names, triangle indexes, NaN/infinity, negative radius,
invalid identification/version and aggregate allocation overflow. Rejection
asserts no payload allocation, shader registration or changed model state.
Actual registration checks missing and malformed optional LODs, incompatible
frame counts, coarse-only fallback, failed-primary cleanup/cache reuse,
short/negative file lengths and invalid names. A native-loadable MD4 at an
optional MD3 path cannot replace the requested base or load without it.
Both Mac products and the portable Python/ledger checks are validated after the final source changes.

The MD3 fixture and nine Python tests pass. Its host build reports the existing
MD4 null-pointer/int stride warning; the MD4 follow-up replaces that expression.
Both Retro68 products build without compiler diagnostics and validate as PPC
PEFs using temporary loader libraries from [loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,690,925 | `debf2657aa274ebc1797954433067cbc09357a5601aeae74c5eb0dd9f04da6fc` |
| Quake3_TeamArena | 3,839,499 | `cf5db0a1a2705b2f1c9129a9cd0488548decc0549845a0ed69dee0f7f48b75a3` |

## Remaining acceptance

Keep #44 open for the MD4 step, gated merges and deferred commercial 1.32c
models/animations, mod LOD behavior and strict-alignment PPC acceptance.

Reviewed QVM work through #69, single-run CI (#68) and the RoQ console
shutdown fix are integrated. The affected sanitizer fixtures, nine Python
checks and both Mac products pass; CI retains every runner. The table
records these integrated artifacts.
