# Shader file cursor isolation and stock index priority — 2026-09-18

This #46 step keeps commercial 1.32c shader assets and public renderer/QVM
interfaces. Keep each compressed input NUL-terminated inside the private
combined allocation. Count/index definitions through identical file-bounded
walks, so an unterminated shader/quote cannot consume another file. Native
malformed named definitions still register/cache the default fallback.

Retain historical lookup priority: first labels in list order precede the
reversed files' full definition walks. This preserves existing duplicate winners,
including non-first labels and empty files. The generated complete index handles
all normal archive lookup; do not fall back to a cursor that could cross files.
A private publication flag distinguishes a complete index from the legacy
single-text fallback seam; initialization clears both flag and pointers. No new
public format or renderer API is introduced.

## Validation

The unchanged previous actual archive shows an unterminated shader hiding a
healthy definition in the next file. The actual-body sanitizer regression now
covers unterminated shader bodies, quotes and comments at every position in a
three-file list, with named default fallback and the later healthy definition
explicitly registered. Input/list ownership still balances in reverse order.

A tiny independent stock-cursor oracle uses the original reversed concatenation,
per-file starting pointers and boundary comparison. It passed on the unchanged
native index first. For 256 valid one-to-four-file layouts, including empty files
and three duplicate names at different label positions, all 768 native lookup
winners/missing results match after isolation. Flattening the private bounded
segments also retains the complete stock-order compressed text. Existing stage,
constant/vector, waveform/iterator, archive size/cap/restart and consumer checks
remain intact.

Carry the corrected parent finiteness check through the actual merge ancestry.
All five affected sanitizer runners (including both normal and release-fast-math
stage configurations), nine Python checks, Bash syntax and diff checks pass
without diagnostics. Both shipped Retro68 products build without diagnostics
and validate as PPC PEFs.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,737,503 | `4ea4a7f48c94c2e319797ad68f85358528c108da472355dfdb5f74df98209ed1` |
| Quake3_TeamArena | 3,886,077 | `57f7e4fbbe5938f5062df57b31511e93323cc1eae2dc2b3ad3262b3f3cefc717` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #46 open for remaining shader numeric/semantic paths and deferred commercial
1.32c/Mac OS 9 live acceptance. Full renderer hunk budgeting/transactions remain
#45. The retained duplicate priority is intentional compatibility behavior;
this step does not introduce a new override policy.
