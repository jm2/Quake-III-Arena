# Enabled native font generation ownership — 2026-09-18

This focused #46 step centralizes cleanup for the optional BUILD_FREETYPE
branch. Close an owned FT_Face before freeing its retained FS input, as required
by the [FreeType memory-face API](https://freetype.org/freetype2/docs/reference/ft2-face_creation.html#ft_new_memory_face).
Release temporary bitmap headers/buffers and page/output buffers on controlled
failure. Propagate glyph-load/raster/allocation failures to zero default font
output without font-cache publication. Check point-size multiplication before
the FT call and reject missing names/read buffers. Handle page image import
failure. Optional TGA output allocation failure warns while preserving a usable
generated font. Repeated successful library initialization reuses the library;
shutdown is idempotent. Earlier created image/shader caches retain their normal
renderer ownership; no whole-renderer rollback is claimed.

Commercial 1.32c fontInfo_t/QVM/syscall and legacy serialized reading remain
unchanged. BUILD_FREETYPE remains disabled in both shipped Retro68 products;
this step does not enable a new target dependency or change their font source.

## Validation

The original actual enabled flow reproduces missing face/file cleanup on
normal generation, new-face/character-size/output-allocation failure, and a
UBSan null store after page-buffer allocation failure. Normal FS release also
occurs before its retained face is closed.

The sanitizer fixture executes the full actual native generation bodies with
isolated FT/FS/graphics imports. The runner copies production source to a
throwaway directory and replaces only its five unavailable legacy FT header
imports with the stub interface; an exact replacement-count assertion guards
this seam. It does not replace native functions or emulate a real rasterizer.

Eighteen scenarios cover success, new-face and character-size errors, output/
page/bitmap-header/bitmap-buffer/TGA allocation failures, zero/negative/null
face inputs, library init failure, glyph-load/raster failure, image import
failure, null/empty font names and overflowing point-size requests. Track exact
Zone/FS/face ownership, assert face closure while input remains alive, preserve
normal generated output, require zero failed output/no font-cache publication,
and check repeated init/shutdown. All temporary/import ownership balances.

The enabled native bodies also type-check against the installed real FreeType
headers (pkg-config module version 26.6.20), replacing only those legacy include
imports with the standard header interface in a disposable copy. This proves
API type compatibility, not real font/raster or target FreeType acceptance.

All four affected font/skin/shader sanitizer runners, nine Python checks,
Bash syntax and diff checks pass without diagnostics. Runner 40 is registered
in portable CI. Both shipped Retro68 products build without diagnostics and
validate as PPC PEFs with FreeType disabled.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,737,425 | `05b31c77e5c49e25985086f401f1ae5d3809a41661438b1b925e4dc91441ea44` |
| Quake3_TeamArena | 3,885,999 | `fc7e78b43e78b34abb478f1a66cd729f804dbcd06761edb9d3df053a4723ff6b` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #46 open for FreeType glyph/atlas numeric bounds, page/final-glyph behavior,
generated cache names and explicit legacy LE output serialization, plus the
remaining shader semantic/file-allocation audit. The current generator still
writes native structures; this step does not claim correct PPC-generated .dat
assets. Real FreeType fonts and commercial retail/Mac OS 9 live acceptance
remain deferred; no shipped font-generation feature is enabled.
