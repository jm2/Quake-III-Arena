# Shader waveforms, modifier staging and deformations — 2026-09-18

This #46 step preserves commercial 1.32c renderer/QVM interfaces, native valid
float parsing and waveform/modifier/deformation semantics. Return and propagate
failure for incomplete/non-finite/out-of-native-float-range waveform fields,
texture modifiers and deformations. Stage numeric animation/portal values use
the same checked conversion. Finite checks use the integer representation that
survives release fast-math assumptions.

Build each texture modifier in cleared local storage and publish its array slot
only after complete successful parsing. Check the existing modifier count before
access and reject unknown/incomplete modifiers. Bound the assembled tcMod line
before copying, instead of silently truncating it. Excess modifiers/deformations
produce native default shader fallback rather than processing an invalid slot.

Check deformation spread's reciprocal fits the native float before conversion.
Distinguish actual signed zero by bits, retaining the native spread-100 warning/
default for zero and legacy atof-to-zero tokens. Reject tiny nonzero values whose
reciprocal would overflow even when fast-math enables flush-to-zero comparisons.
Ordinary legacy atof prefix/fallback behavior is otherwise retained.

## Validation

The unchanged actual stage parser accepts an incomplete waveform, non-finite
waveform/texture-modifier data and an incomplete scale modifier. Capture native
complete bytes for seven modifier types and eight deformation types before the
change; all 15 host fingerprints match after checked staging. Stale unused
modifier bytes are cleared on publication.

Both normal and optimized release-fast-math Clang ASan/UBSan fixtures check every
numeric parameter position across all valid modifier/deformation types against
six non-finite/out-of-range values. Check all missing waveform prefixes, each
missing/unknown modifier, exact/excess modifier and deformation counts, overlong
lines, invalid animation/portal values, reciprocal overflow and signed-zero/
legacy-zero defaults. Public invalid numeric shaders cache fallback and retain
the following valid definition. Existing 1,021 native constant-color goldens and
768 stock archive lookups remain intact. A separate GCC 16 fast-math run checks
the complete expanded stage fixture.

All five affected sanitizer runners (including both stage configurations), nine
Python checks, Bash syntax and diff checks pass without diagnostics. Both shipped
Retro68 products build without diagnostics and validate as PPC PEFs.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,737,525 | `6f8911b62b3938aa5aabf8ca0b6d42060a04ed4ff15a81f999e455de45978749` |
| Quake3_TeamArena | 3,890,195 | `ea8b0e28e818464419c381bdb42ce982824bab0db39cdee1dae46a3848393244` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #46 open for sky/sun/fog/sort handling, derived rendering conversions and
deferred retail commercial 1.32c/Mac OS 9 acceptance. This step validates parsed
numeric storage and the spread reciprocal, not every later time/geometry
calculation; renderer-wide hunk budgets/transactions remain #45. Earlier evidence
remains dated, and public interfaces/retail asset layouts are unchanged.
