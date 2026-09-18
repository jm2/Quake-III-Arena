# Sky subdivisions and shared cloud mesh — 2026-09-18

This #46 step keeps commercial 1.32c shader/QVM interfaces, the native quarter-cell
sky rounding, face/image/vertex/index order and the existing five cloud faces.
Check finite side bounds and clamp them to the visible cube before subdivision
multiplication, floor/ceil or int conversion. Non-finite sides skip rendering.
Validate direct cloudy-side grid bounds, counters and complete vertex/index
capacity before any buffer/counter writes. Reserve the backend's last-slot
sentinels as native overflow checking requires.

Previously each cloud stage appended the same geometry, while only the first
stage appended indexes. Full coverage generated 405 vertices per stage: three
stages hit the 1,000-vertex buffer despite valid shader stage capacity. Build the
shared indexed mesh once. Zero/absent stages and zero cloud height still generate
no geometry; one through eight stages retain the same 405 indexed vertices and
1,920 indexes. Only unreachable duplicate vertices are removed.

## Validation

Before editing, actual native bodies reproduce finite extreme sky/cloud bounds
reaching infinite int conversions, out-of-bounds vertex/index writes with full
counters, and three-stage clouds dropping on overflow. The original full sky
trace was 2556d1b2 (six binds, 48 strips, 864 vertices); the cloud fingerprint
was 4205bd9d (405 vertices, 1,920 indexes). Captured fingerprints are dated
evidence, not portable cross-compiler expectations.

Normal/release-fast-math ASan/UBSan/float-cast-overflow runs compare native
pre-fix bodies with corrected complete graphics traces and cloud vertex/UV/index
arrays for 1,122 ordinary bounds cases. The fixture isolates graphics callbacks
and extracts the unchanged native Q_acos body directly from common.c, requiring
exactly one source seam match. Test zero through eight stages, all four bounds
fields with six positive/negative quiet/signaling NaN/infinity patterns, finite
extreme saturation and preserved indexed geometry after removing duplicates.
Direct preflight tests cover last usable slots, sentinel retention, eleven
invalid bounds/counter/capacity cases and complete state retention on rejection.

Both sky sanitizer configurations, separate normal/fast GCC checks, six related
shader/skin/font runners, nine Python checks, Bash syntax and diff checks pass
without diagnostics. Both shipped Retro68 products build without diagnostics
and validate as PPC PEFs.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,745,811 | `ac12f2e48fb988a0175f2e133757622fbfd08350b28da6d1c221e8d18b4ac8f1` |
| Quake3_TeamArena | 3,894,385 | `83c9ea79be430842958cbb0d80695804e47fdff3db7fd2bb11b13e4b6c4636da` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #46 open for cloud-layer numeric calculations and deferred commercial
1.32c/Mac OS 9 live acceptance; #45 retains renderer transaction/budget gates.
Graphics callbacks make source execution observable without a graphics context.
This does not claim GPU acceptance or validate every renderer input geometry
contract. Earlier validation remains dated.
