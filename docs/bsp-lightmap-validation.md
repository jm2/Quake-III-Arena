# BSP RGB lightmap bounds — 2026-09-17

This focused step for #45 prevents both lightmap source overreads. Native
maps with one source lightmap still publish two textures to preserve the
existing fullbright workaround. Both uploads now use that one complete RGB
record instead of reading a nonexistent second record. Source count stays
separate from upload count; the existing fixed MAX_LIGHTMAPS table and warning
behavior remain unchanged.

Lightmap RGB conversion reads exactly three source bytes and publishes alpha
255. Geometry RGBA conversion captures and restores its original alpha,
including when input/output overlap. RGB normalization and valid overbright
behavior remain unchanged. Empty lightmap lumps reset the published count
without uploading; partial/negative records reject before synchronization,
creation or count changes. The enclosing world loader still performs complete
FS header/lump preflight before calling this private loader.

## Validation

The ASan/UBSan fixture includes actual tr_bsp.c and real tr_local.h structures;
only unused OpenGL declarations and image factory/synchronization imports are
isolated. Exact-sized source allocations have no readable trailer. Cases
cover one/two source records at four alignments, the intensity color-coding
branch, MAX_LIGHTMAPS and one more source record, every output alpha and final
RGB sample, native upload arguments/names/slots, unchanged capacity warnings,
empty/partial records, RGB normalization goldens and aliased geometry alpha.
Single-source uploads match across every pixel in both textures. Image
creation copies/inspects actual RGBA bytes synchronously, matching the native
factory's ownership contract; no GL/window/device behavior is claimed.

All three BSP fixtures, nine Python checks and Bash syntax pass. Both
Retro68 products build without compiler diagnostics and validate as PPC PEFs
with master `4fd62bd` and the corrected reference preflight integrated.
Temporary loader libraries are described in [loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,712,245 | `0518eafed1c8bb222485ae4d7653585e8394242ad0e66db2c32fb4f700a323a7` |
| Quake3_TeamArena | 3,860,819 | `185b8c294521801a5fb1072b9496c479f66914a9a253b49ad6f750cb522849d8` |

The host
fixture reports the existing face-allocation pointer-to-int warning in an
unused legacy function; the geometry follow-up will replace that expression.

## Remaining acceptance

Keep #45 open. Graph termination, geometry products/finite values, entity/grid
parsing, renderer capacities and full transactional world publication remain
outstanding. This step does not establish overall BSP safety. Retail visual
acceptance and Mac OS 9 texture/device testing remain deferred to the user's
follow-up session.
