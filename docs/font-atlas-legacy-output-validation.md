# Optional font metric/atlas bounds and retail LE output — 2026-09-18

This #46 step retains commercial 1.32c fontInfo_t/QVM/syscall layouts and the
20,548-byte untagged legacy version 0 format. BUILD_FREETYPE remains disabled
in shipped products; generation tests isolate FT/FS/graphics imports and use
the [actual-body seam](font-freetype-ownership-validation.md).

Validate native FT_Pos operands before rounding/addition/negation. Calculate
in int64_t and check representable native coordinates, four-byte pitch and the
existing atlas boundary: pitch at most 252, height at most 253. Preserve native
fractional/negative rounding. Allocate one owned byte for empty bitmaps and
one exact 256×256 grayscale page. Move to the next row before checking the
complete row fits. Retry the glyph after a full-page flush, and flush only
after copying the final glyph. All 256 glyphs retain their native rectangles.
Check generated shader names fit the legacy fixed field before importing the
offending page. Previously created image/shader caches keep normal renderer
ownership on a later failure; zero output and no font-cache publication remain
the controlled failure behavior.

Use the actual .dat path for generated cache names. Write each legacy field
explicitly in LE, zero obsolete runtime handles, retain fixed names/scale,
and balance the temporary serialized buffer. Optional save-allocation failure
warns while retaining the usable generated font. Validate TGA dimensions/input
and 32-bit output arithmetic before allocation/reads; valid headers and RGBA
to BGRA output remain unchanged. No required version header is introduced.

## Validation

Unchanged native functions reproduce metric signed overflow, final-glyph loss,
page-boundary missing/out-of-texture glyphs, and TGA size signed overflow.
The native-structure writer's PPC endian incompatibility is a source-format
assessment; no target-generated file/runtime test is claimed.

The actual enabled sanitizer fixture retains the ownership/API failure cases
and adds public invalid metrics, unsupported outlines, an invalid final glyph,
legacy save-allocation failure and a generated name exceeding its fixed field.
Pure metric checks cover native/host integer extremes, negative dimensions,
maximum pitch/height rejection, untouched failed outputs, and exact fractional/
negative rounding. TGA checks cover zero/negative/16-bit/32-bit shape bounds,
null inputs and exact valid header/color bytes.

Generate complete fonts for glyph sizes 0, 1, 4, 64 and 252. Inspect every
rectangle against the actual uploaded page alpha, with all final/page-boundary
glyphs present and within texture bounds. Check generated cache reuse imports
no new files/faces/pages. Independent literal LE bytes verify every serialized
signed integer/UV/zero handle, fixed name and scale/tail offset; the actual
private reader retains all fields. Actual 64-pixel generation saves 29 complete
TGA pages and one legacy .dat, then the public RE_RegisterFont reader reloads
all 256 matching glyphs with refreshed renderer handles and no FT dependency.
All FS/face/Zone ownership balances.

All four affected sanitizer runners, nine Python checks, Bash syntax and diff
checks pass without diagnostics. Enabled native bodies type-check against the
installed real FreeType headers through disposable include substitutions;
this is API type validation, not real rasterizer acceptance. Both shipped
Retro68 builds pass without diagnostics with FT disabled and validate as PPC PEFs.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,737,425 | `951ed5450181bbe4fc4d40545e8a105667783e2d58c5d6e05dfea5dabb83f9a9` |
| Quake3_TeamArena | 3,885,999 | `9a6449bee928ab928454a02927e825325e28ab5b17e5a0f4a941664b6da87d1c` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #46 open for the remaining shader semantic/file-allocation audit and
retail commercial 1.32c/Mac OS 9 live acceptance. No real font/raster library,
GPU, retail assets or target FT generation test is claimed; FT remains disabled.
Earlier documents retain their dated pre-correction observations.
