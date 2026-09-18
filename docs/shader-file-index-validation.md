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

## Remaining acceptance

Keep #46 open for remaining shader numeric/semantic paths and deferred commercial
1.32c/Mac OS 9 live acceptance. Full renderer hunk budgeting/transactions remain
#45. The retained duplicate priority is intentional compatibility behavior;
this step does not introduce a new override policy.
