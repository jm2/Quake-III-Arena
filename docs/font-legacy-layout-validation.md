# Retail legacy font decoding and FS ownership — 2026-09-18

This step for #46 preserves the commercial 1.32c fontInfo_t/module/syscall
ABI and serialized asset layout. Document the existing untagged format as
legacy version 0. A future tagged format must be optional; no new header is
required for retail assets. The layout is derived from this fork and agrees
with the original [id Software reader](https://github.com/id-Software/Quake-III-Arena/blob/master/code/renderer/tr_font.c)
and [ioquake3 reader](https://github.com/ioquake/ioq3/blob/main/code/renderercommon/tr_font.c).
This is source-format evidence; live retail acceptance remains deferred.

| Legacy field | Bytes / encoding |
| --- | --- |
| Each of 256 glyphs | 7 signed LE 32-bit integers, 4 LE binary32 UVs, 1 obsolete LE renderer handle, 32-byte NUL-terminated shader name: 80 bytes |
| Scale | LE binary32: 4 bytes |
| Historical font name | 64 bytes; unused, replaced with the actual asset path |
| Complete file | 20,548 bytes, unchanged |

Read the actual FS buffer once and validate its returned length, every fixed
shader name and finite UV/positive finite scale before importing shaders.
Use defined unsigned LE word assembly and bit copies for signed integers and
floats; no aligned casts, signed shifts or platform endian macros. Replace
all 256 obsolete handles with current shader registrations, including empty
names' native default behavior and the final glyph, matching ioquake3's range.
Release FS input before imports/cache publication; malformed files yield a
zero default output and no font-cache/shader publication. Existing cached
fonts remain reusable even at the native six-font limit. A null output imports
nothing. Native point-size fallback remains 12.

## Validation

The original actual reader reproduces an ASan short-read overrun after the
size-query/read disagree, UBSan signed high-byte shift overflow, an invariant
failure for a nonterminated 32-byte shader name, and missing FS release on
success. The final actual RE_RegisterFont fixture isolates FS/shader imports.
An independent byte-built legacy file verifies every signed field, negative
and finite UVs, scale, 31-byte/empty shader names, all 256 current handles and
asset name exactly at all four input alignments. Full signed bit patterns
remain defined, including INT_MIN/INT_MAX.

Test all 20,548 short lengths (0–20,547), oversized/negative lengths, null
buffers, each of 256 nonterminated names at all four alignments, every glyph
UV field with both infinities and quiet/signaling NaN bit patterns, invalid
scale classes, default point size, null output, cache filling/reuse at capacity,
and new-font rejection without imports. Invalid inputs have zero output,
zero shader/cache publication and balanced file ownership. FS allocations and
caller output balance. The font, skin and shader sanitizer runners, nine Python
checks, Bash syntax and diff checks pass. Runner 39 is registered in CI.

Both Retro68 products build without diagnostics and validate as PPC PEFs.
Temporary libraries are described in [loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,737,425 | `d783c82a4191d521c4ae19c2b4f48afff95a085dde7bce39b5aade985223caf5` |
| Quake3_TeamArena | 3,885,999 | `d550ded388c65454b1eb843cae8cd33344282a58ffc047b98a73034dc10bfb7d` |

## Remaining acceptance

Keep #46 open for FreeType generation ownership and remaining shader
semantic/file-allocation checks. BUILD_FREETYPE remains disabled in these
products; its generation branch is not covered by this fixture. The two
existing host shader alpha/color enum warnings retain their focused follow-up.
Retail commercial 1.32c and Mac OS 9 live acceptance remains deferred.
