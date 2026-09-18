# Shader registration names, lighting modes and remap inputs — 2026-09-18

This #46 step keeps commercial 1.32c renderer/QVM interfaces and all four native
negative lighting sentinels. Use a bounded native name check before hashing,
lookup or registration. Empty/null/overlong names return the existing default
shader/zero handle. Hash extended filename bytes through unsigned-char ctype
input. Reject unknown negative lightmap indexes before array access. Native
nonnegative missing-lightmap fallback to vertex lighting also applies to image
registration.

New image registrations require an image before allocation/publication; existing
cache probes still reuse an existing handle without a new image. Validate both
remap names and optional time offset before material creation/remapping. Keep
prior state on non-finite/out-of-float-range offsets; valid offsets and the native
empty-offset-to-zero behavior remain unchanged.

## Validation

The unchanged actual exported registration reproduces a null strlen argument;
actual R_FindShader reproduces an INT_MIN negative lightmap array access under
UBSan. Native remapping accepts a non-finite offset before the correction.

The expanded actual-body sanitizer fixture executes public registration/remap
and native lookup paths. It checks all three exported null-name calls; empty and
64–70-byte names across all entry points; every valid 1–63-byte name and exact
case-folded maximum-name reuse; all extended bytes 128–255 with stable lookup/
cache identity; all native lighting sentinels and a real valid lightmap. Unknown
negative indexes reject without allocation/import, positive absent lightmaps
retain vertex fallback and new null-image registration publishes nothing. Native
existing null-image cache probes still reuse their handle. Valid remap/offset,
empty offset, invalid names and six non-finite/out-of-range offsets preserve
expected identity/state. Normal and release-fast-math stage configurations pass.

All five affected sanitizer runners (including both stage modes), a separate
GCC 16 fast-math run, nine Python checks, Bash syntax and diff checks pass
without diagnostics. Both shipped Retro68 products build without diagnostics
and validate as PPC PEFs.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,741,633 | `8c6d210d9fa49c732f5cf333724d9e75df1bcfd5815ad3a0a77b85bb0437202f` |
| Quake3_TeamArena | 3,890,207 | `35d1552691fc702d51ca7a57af69db9e71d550469fd172a6e5042c5602c0a211` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #46 open for derived rendering conversions and deferred commercial 1.32c/
Mac OS 9 live acceptance. String storage still follows the native C-string
contract; this bounded name check is not an ownership-size ABI change. The
unsigned-byte hash tests isolate filesystem/graphics imports and do not claim
live HFS filename or GPU acceptance. Renderer-wide budgets/transactions remain
#45 and earlier evidence remains dated.
