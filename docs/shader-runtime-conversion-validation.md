# Derived shader integer conversions and periodic noise — 2026-09-18

This #46 step retains the commercial 1.32c renderer/QVM ABI, native waveform
phase/animation formulas, all valid C truncation and platform myftol rounding.
Private float guards inspect IEEE bits through a volatile integer so release
fast-math cannot remove non-finite checks. Converted values must fit a native
32-bit int; otherwise the conversion returns zero. Apply the guards to wave
indexes, animation indexes, bulge indexes, dynamic/diffuse lighting and
wave/specular/portal/fog color conversions. Keep native clamp and byte behavior
for valid values. Check wave/portal values before clamping under fast-math, and
check fog coordinates before table indexing. Invalid indexes select the existing
first table/frame.

The native noise table repeats every 256 integer cells. Ordinary coordinates
retain native floor-to-int/fraction arithmetic, with room for adjacent cells
and nested permutation additions. Reduce extreme finite floor coordinates
modulo 256 before int conversion, retaining native fractional interpolation.
Reject every non-finite coordinate before floor/cast. This does not cap valid
shader timing, speed or noise coordinates and does not change table generation.

## Validation

Before editing, the actual bodies under ASan/UBSan reproduce four failures:
finite frequency FLT_MAX multiplied by time overflows the waveform index;
FLT_MAX noise coordinates overflow floor-to-int; and a non-finite wave base
reaches float-to-alpha conversion; a non-finite fog coordinate reaches an
unchecked table-index conversion. The corrected runtime runner compiles actual
tr_shade_calc.c, tr_noise.c and tr_image.c, with shared native q_shared/q_math
bodies and
isolated renderer state. It explicitly enables float-cast-overflow sanitization.

A copied pre-fix noise body provides an independent native oracle for defined
ordinary inputs under the same compiler/flags. Test 645 waveform evaluations
and their complete four-vertex color/alpha results against native formulas;
513 noise interpolations match exactly in normal builds. Release fast-math
may reassociate interpolation differently with added control flow: those
results must stay within four float epsilons; indexes/color bytes stay exact.
The original Clang fingerprints captured before edits were ae684c49 (normal)
and 48ee0a69 (release fast-math); these are evidence, not portable expectations.

Check eight representable conversion values including INT_MIN, signed zero,
fractions and the final representable float below INT_MAX; ten out-of-range/
non-finite bit patterns include positive/negative quiet/signaling NaNs and
infinities. Every noise axis tests eight extreme finite periodic coordinates
and six non-finite patterns. Actual waveform consumers test overflow and
non-finite phase for all five tables, non-finite color/alpha, finite-overflow
bulge phase and diffuse channels. All 8,192 actual fog texture samples retain
their native density; test both fog axes against six non-finite patterns and
finite saturation/negative bounds. Normal and optimized sanitizer configurations
and separate normal/fast GCC builds validate compiler behavior.

The runtime runner, 20 affected renderer/model/BSP runners (including both stage
modes), nine Python checks, Bash syntax and diff checks pass. The existing hunk
fixture emits its prior 64-bit pointer-cast and deliberate crash-helper warnings
in both modes; the changed shader runners emit no diagnostics.
Both shipped Retro68 products build without diagnostics and validate as PPC PEFs.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,745,805 | `712ac7d35bb475287d98e69c7e7ed7db3720d6c037989e1d30f4f64ab001c335` |
| Quake3_TeamArena | 3,894,379 | `bfc4f9ae06419ebfc41776227723741b19d08abe152204f1c0f1ba6008d97402` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #46 open for deferred commercial 1.32c/Mac OS 9 acceptance, shader/cloud
texture calculations and remaining renderer-wide limits/transactions (#45).
The actual backend animation/portal/dynamic-light/fog/specular callers compile
in both products but are not executed by this graphics-free fixture. Do not
interpret private int guards as validation of every floating GPU input or the
legacy QVM's entire renderer input contract. Native noise table generation uses
the platform C library's existing rand implementation. Optional FreeType stays
disabled in shipped products. Earlier evidence remains dated.
