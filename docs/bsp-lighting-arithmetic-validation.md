# BSP lighting byte arithmetic — 2026-09-17

This step for #45 defines overbright conversion for the complete signed cvar
range. The legacy helper left-shifted bytes by an unchecked difference of
r_mapOverBrightBits and tr.overbrightBits, allowing negative/oversized shifts,
signed subtraction overflow and overflow in normalization products.

Compare the settings before unsigned subtraction, which represents their
nonnegative difference even at INT_MIN/INT_MAX. Only eight byte shifts matter:
negative differences of eight or more produce black; positive differences of
eight or more normalize every nonzero sample. Normalize original bytes before
shifting so the common power of two cancels and products stay at most 65,025.
For positive shifts that fit, retain the stock shift result. For negative
shifts, use nonnegative integer right shifts to dim the sample.

Every defined ordinary legacy positive-shift result remains identical. Default
settings, RGB lightmap record size, in-place grid layout, geometry alpha, and
the commercial 1.32c module ABI remain unchanged. Cvar values are not rewritten
or newly rejected. RGB input reads three bytes and writes opaque RGBA; the
geometry wrapper captures alpha before aliased writes.

## Validation

The existing actual tr_bsp.c lightmap fixture runs ASan/UBSan over 19,773 RGB
goldens from the original defined arithmetic at shifts zero through eight,
covering normalization thresholds and each channel's role. It also checks all
256 source-byte values under each negative shift, odd-value rounding,
INT_MIN/INT_MAX differences in both directions, black input and overlapping
RGB/RGBA buffers at every input byte alignment.

Actual R_LoadLightmaps uploads retain exact-source single-map duplication and
match independently calculated native/default and dimmed RGB pixels. Actual
R_LoadLightGrid checks ambient and directed conversion in place, including the
overlapping fourth byte and both unchanged light-direction bytes. The actual
face, triangle and patch parsers check all four geometry-color bytes under
ordinary, negative and extreme settings; patch subdivision is executed.
Imported image and allocation callbacks remain isolated from graphics/device
hardware. The tests do not provide live visual acceptance.

All seven affected BSP sanitizer runners, nine Python checks and Bash syntax
pass. All 30 inherited CI runners remain. Both Retro68 products build without
compiler diagnostics and validate as PPC PEFs, using the temporary toolchain
libraries recorded in [loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,720,811 | `ae9d1515a03634acf50acd85995d80d77369a41579ac97092cfde1ec5630e0da` |
| Quake3_TeamArena | 3,869,385 | `b6cb119bd399f3680d07969ca98f4b99ced51c6424cdf389fe625f735aafc5a4` |

## Remaining acceptance

Keep #45 open. Deep recursive runtime queries, derived geometry/facet limits,
aggregate map-memory budgets and complete transactional publication remain.
Retail 1.32c map/mod rendering and Mac OS 9 visual/device acceptance stay
deferred to the follow-up session.
