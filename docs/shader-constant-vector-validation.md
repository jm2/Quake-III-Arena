# Shader constant colors and vector failure propagation — 2026-09-18

This focused #46 correction keeps commercial 1.32c renderer/QVM interfaces and
valid shader syntax. ParseVector rejects non-finite values or values outside the
finite native float range before conversion. Check the IEEE double exponent
through an integer byte copy so release -ffast-math cannot discard finiteness
validation. Read a volatile integer representation before masking to stop
Clang from folding even the initial non-volatile bit test into fast-math
assumptions. Constant RGB and both texture
vectors now propagate parser failure instead of using incomplete/uninitialized
vector data. Fog vectors already propagated failure and share the finite check.

Validate constants before byte conversion and saturate finite out-of-range
colors to 0–255. Clamp before multiplication so finite extreme inputs cannot
overflow the scale. Preserve the historical distinction: RGB scales in float
precision, alpha scales in double precision; valid normalized colors retain
native truncation/rounding. A missing/non-finite alpha constant rejects the
shader. Ordinary legacy atof prefix/fallback behavior is retained.

## Validation

The unchanged actual ParseStage reproduces non-finite RGB and alpha byte
conversion failures under UBSan, and the invariant failure accepting two
malformed texture vectors. The existing actual-body stage/registration fixture
now checks non-finite and overflowing components in both texture vectors,
malformed/short/empty RGB vectors, missing alpha and full finite vector range.
Finite extreme constants saturate without conversion UB. Codex identified a release fast-math gap in the first comparison-based check;
the unchanged first revision reproduces acceptance of non-finite alpha under
-O2 -DNDEBUG -ffast-math. Both default and optimized fast-math sanitizer
configurations now pass the complete stage fixture under Clang; a separate
GCC 16 optimized fast-math run also passes. All 513 normalized
RGB/alpha samples plus 508 adjacent float byte thresholds match the independent
native scaling/truncation oracle: 1,021 valid quantization goldens.

The public R_FindShader path caches default fallback for each malformed/non-finite
constant/vector case, reuses the cached result and retains the following valid
definition. Existing native 0–10-stage, brace/quote/comment/EOF, waveform collapse
and fast-iterator checks remain intact. Separate archive input/restart, skin,
legacy font and enabled font ownership/atlas runners cover affected consumers.

All five affected sanitizer runners, nine Python checks, Bash syntax and diff
checks pass without diagnostics. Both shipped Retro68 products build without
diagnostics and validate as PPC PEFs.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,737,481 | `98312cb4a63691e04a48de63f11d6e544af1ae3890eff348d3d7e7583c365736` |
| Quake3_TeamArena | 3,886,055 | `681e610aa350066c4fe82ae8df6e94290f7b3ba916c6dc94d799c4b2dab81a9d` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #46 open for archive definition/index isolation, remaining shader numeric/
semantic paths and deferred commercial 1.32c/Mac OS 9 live acceptance. Large
finite texture vectors retain the full native float input range; this does not
claim every derived GPU calculation is overflow-free. Earlier documents remain
dated evidence, and native interfaces/assets are unchanged.
