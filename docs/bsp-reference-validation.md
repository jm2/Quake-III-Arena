# BSP collision and material references — 2026-09-17

The second step for #45 adds shared bytewise payload-reference validation
following complete header/layout preflight and before collision or renderer
checksums, allocations or state resets. Shader/fog names terminate inside
MAX_QPATH. Brush sides reference owned planes/shaders; brushes have complete
side spans and at least the six sides consumed by native bounds loaders.
Nodes reference existing planes and nodes/leaves; complemented unsigned leaf
indices handle INT_MIN without signed negation.

Leaf surface/brush indices and spans, model surface/brush spans, fog brushes
and visible sides, and surface shader/fog/type references validate by
subtraction before products or pointer construction. Consumed vertex/index
spans and surface-local indices fit their arrays. Leaves support opaque -1
clusters, including area -1 for opaque leaves, and the fixed retail area
visibility bit vector; cluster counts fit
native novis allocation arithmetic and declared PVS rows. Collision rejects
more than its existing MAX_SUBMODELS limit before allocation.

Native patches ignore their index-array fields and flares ignore both geometry
array fields. Those unused fields remain ignored. The four retail negative
lightmap selectors remain supported; positive unavailable lightmaps retain
R_FindShader's existing vertex-light fallback. Adjustable compiler utility
budgets are not introduced as format caps.

The [original compiler area assignment](https://github.com/id-Software/Quake-III-Arena/blob/master/q3map/portals.c)
and [leaf writer](https://github.com/id-Software/Quake-III-Arena/blob/master/q3map/writebsp.c)
retain area -1 for opaque leaves. Visible leaves require an addressable retail
area bit; a negative visible area would reach a negative renderer mask index.

## Validation

The shared fixture runs bytewise references at all four input alignments and
actual CM_LoadMap with exact FS allocation. Its native golden includes shaders,
brushes, leaf/model spans, fog, a triangle surface, a 3x3 collision patch and a
flare. The patch backend is stubbed to verify validated control points. Native
collision state and synthesized submodel indices are checked using one owned
hunk arena, matching the engine's allocation relationships.

Structure-aware mutations cover late shader/fog names, every side reference,
brush spans and six-side minimum, both node children, leaf indices/spans,
cluster/area boundaries, both model spans, fog references, every surface's
materials/type, consumed vertex/index spans and negative/local indices.
Every malformed payload retains a valid header and rejects at four FS
alignments before checksum, hunk allocation, map reset or patch clearing.
The previous loaded map and caller checksum remain unchanged; FS ownership is
released before controlled ERR_DROP.

Valid cases preserve area 255, opaque clusters with area 0 or -1, zero-length spans at array
ends, all four negative lightmap selectors, positive lightmap fallback and
ignored patch/flare fields. Actual collision loading accepts 256 submodels;
257 rejects before state changes. The inherited header fixture uses the same
arena and remains covered. These fixtures do not execute renderer payload
loading or the native patch-generation algorithm.

Both BSP ASan/UBSan fixtures, nine Python checks and Bash syntax pass. Both
Retro68 products build without compiler diagnostics and validate as PPC PEFs
using the temporary libraries from [loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,708,115 | `329ac4ae65dc4e8c6aeda785e8fde23467ea032d96646a4232fc0d74a9f5ccd9` |
| Quake3_TeamArena | 3,856,689 | `608e8e5ae9a247d1527f2c00758dd004da6cf4fab44a447d832a603ca186361b` |

## Remaining acceptance

Keep #45 open. Node graph termination, patch dimensions/products, finite and
derived geometry, renderer capacity checks, entity/grid parsing and complete
transactional publication remain outstanding. Valid references do not make
untrusted BSP payloads safe yet. Retail collision/rendering and Mac OS 9 live
acceptance are deferred to the user's follow-up session.

Merged master `4fd62bd`, including all reviewed bot syscall steps, is
integrated. The affected sanitizer fixtures, nine Python checks and both
product builds pass; the table records the current integrated artifacts.
