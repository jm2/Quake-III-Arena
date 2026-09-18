# Shader sky/sun/fog/sort metadata and sun publication — 2026-09-18

This #46 step preserves commercial 1.32c shader formats and public renderer/QVM
interfaces. Validate complete sky outer/cloud/inner parameters and full suffixed
image paths before imports. Retain native six-face order/wrap modes, missing-image
fallback and zero-height default 512. Reject non-finite/out-of-native-float-range
cloud, sort, clamp-time and fog-depth values and missing required parameters.
Retain named/numeric sorts and ordinary legacy atof-to-zero behavior. Saturate
finite out-of-range fog colors to normalized bounds before later byte conversion.

Parse all six sun inputs into private fields. Validate native source/derived
length, scaled light and direction before success. Keep the pending sun state
private until the entire shader definition parses successfully; any later
stage/parameter/zero-stage failure retains the previous renderer sun. Preserve
native float rounding/normalization/angle calculations for valid definitions.

## Validation

The unchanged actual shader parser accepts non-finite sun, sort, sky and fog
depth. Capture complete native sun-light/direction and fog-field host bytes before
the change; their three fingerprints remain identical in normal and optimized
Clang configurations. All named sorts, numeric sort and legacy-zero sort retain
exact native values. Check all six sun inputs against non-finite/overflow classes,
every truncated prefix, finite-source length overflow and retained old sun state
on bad inputs or a later invalid shader parameter.

Sky checks isolate only graphics imports while executing actual parse bodies:
invalid/truncated/non-finite/oversize complete paths produce no image/cloud
imports. Exact valid 12-face names/order, maximum 63-byte suffixed paths,
one cloud initialization and zero-height defaults remain native. Fog/clamp/sort
missing/non-finite/overflow fields reject and finite extreme fog colors clamp.
Public invalid metadata caches native fallback, retains prior sun and the
following valid definition. All normal/optimized stage goldens, waveform/modifier/deformation and archive
isolation/ownership checks remain intact.

All five affected sanitizer runners (including normal/fast-math stage modes),
a separate GCC 16 fast-math run, nine Python checks, Bash syntax and diff checks
pass without diagnostics. Both shipped Retro68 products build without diagnostics
and validate as PPC PEFs.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,741,627 | `720977e4eb9144ec1a399521b769d2631e9525f3736dbc302d90ad85c0f3f566` |
| Quake3_TeamArena | 3,890,201 | `234c0e86363b8e0370173cd23199fc9d60114f18416b1d3dfa00c6f6e9d0cc0b` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #46 open for derived rendering conversions, public shader inputs and
deferred commercial 1.32c/Mac OS 9 live acceptance. Native sky coordinate math
is isolated by the graphics seam here; source-finite clouds/vectors can still
require derived checks. Earlier image/shader caches keep normal renderer
ownership; renderer-wide budget/transactional publication remains #45. This
step publishes sun only after a complete definition, not after a complete world.
