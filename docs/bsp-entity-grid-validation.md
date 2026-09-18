# BSP entity and light-grid bounds — 2026-09-17

This step for #45 copies exactly the declared entity bytes into owned storage
and appends a NUL. Worldspawn parsing and the cgame entity token API no longer
read through a following lump or beyond the file. The shared quoted-token
parser clears its cursor at an unterminated quote instead of returning a
pointer beyond the NUL. Valid quoted/unquoted tokens keep the existing API.

Preflight uses a recoverable temporary entity copy and the same worldspawn
parser as loading, without shader remaps or warnings. Preserve the native
64/64/128 defaults, partial custom gridsize values, ignored suffixes/unknown
keys, remap prefixes, vertex-light conditions and missing-semicolon stop.
Custom components must remain positive and finite, with representable float
inverses. Direct strtof conversion retains q3map's binary32 decimal rounding;
checks reject nonfinite/underflow outcomes and unsafe inverses without an
intermediate double or an overflowing scanf assignment.

The validated world model bounds and grid size determine origins/dimensions
using the native float quotient before ceil/floor, stored float origins/maxima
and float count expression. This retains q3map fractional-grid rounding:
size 0.1 over bounds 0..1 has 11 samples per axis, not 10. Range checks use
double comparisons before float/int conversion. Quotients/origins and native
integer dimensions must be representable; division checks precede products
and the existing signed
8-byte grid strides/allocation must fit. Loading consumes this staged result
rather than repeating unchecked float casts or products. Empty grid lumps
skip unused derived coordinates. Safe mismatched sample lengths retain the
native warning/disabled-data behavior; empty intersections never publish an
array with zero samples. No map-compiler coordinate or grid count limit is
imposed. The existing algorithm/defaults can also be inspected in
[ioquake3's renderer](https://github.com/ioquake/ioq3/blob/master/code/renderergl1/tr_bsp.c).

Sampling clamps coordinates before float-to-int conversion, including
nonfinite entity positions. Outside coordinates use the nearest grid edge.
Zero-weight corners are skipped before constructing their data pointers, so
one-point grids and all upper boundaries never access absent neighbors.
Interior trilinear weighting, wall-sample exclusion/renormalization, RGB
conversion and direction bytes retain the native behavior.

## Validation

ASan/UBSan executes actual RE_LoadWorldMap, R_GetEntityToken and
R_LightForPoint with real renderer structures. Successful minimal worlds
publish their entity/grid data and release the exact FS input. Cases cover
empty/non-NUL entities, every small-worldspawn prefix, quotes/comments at EOF,
embedded NULs, a following non-NUL sample lump, partial/unknown grid settings,
remap arguments/vertex-light conditions and malformed remap stopping.
Fractional-grid goldens execute native 11x11x11 and partial 11x1x1 grids,
retaining all q3map-generated samples and actual lighting at their edges.
Exact/just-above binary32 decimal midpoints distinguish 11 from 10 native
samples, retaining direct float parsing and actual edge lighting.

Invalid zero/negative/nonfinite/underflow/overflow settings, finite bounds
with unsafe grid dimensions/products/origins and injected temporary OOM
reject at all four FS alignments before world/shader/model/hunk changes.
FS and temporary ownership are released before ERR_DROP. Native safe raised
grid dimensions remain accepted without allocating their mismatched samples.

Exact 8- and 64-byte sampling arrays cover one-point data, the center/all eight
corners of a 2x2x2 grid, upper/lower/extreme/nonfinite coordinates, native RGB/
light-direction goldens, both in-place overbright conversions and ignored
black corners with renormalized weights. Imported shader/image/model and
allocation callbacks are isolated; actual curve code supplies native loader
link dependencies. Native collision patch generation is stubbed.

All 28 inherited host sanitizer runners, nine Python checks and Bash syntax
pass. Legacy warnings in unrelated host fixtures remain recorded in the local
suite log. Both Retro68 products build without compiler diagnostics and
validate as PPC PEFs with merged master `4fd62bd` integrated, using temporary
libraries from [loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,720,691 | `821f605ebffbdb1bda3631796ccf4b8f99718e7162d8a2a95033e2178e7c8941` |
| Quake3_TeamArena | 3,869,265 | `a60eb8897e2568559c23f93d8ef3e3e1d463b07d4eb42e069191657ad677cdbd` |

## Remaining acceptance

Keep #45 open. Graph termination, derived geometry/facet limits,
aggregate/runtime model capacity and complete transactional publication
remain. Retail 1.32c map/mod rendering and Mac OS 9 visual/device acceptance
stay deferred to the follow-up session. These host goldens establish the
scoped storage/ownership behavior, not full retail or device acceptance.
