# Complete native material fallback and cache identity — 2026-09-18

This #46 step preserves commercial 1.32c renderer/QVM interfaces, valid shader
behavior and failed-name cache/zero-handle semantics. A rejected definition
previously reached FinishShader with accepted prefix stages and sky/fog/deform/
render metadata. A missing implicit texture finalized an empty material.
Use the same minimal initializer as the native built-in default material:
clear all staging arrays/metadata, retain the failed name, select the native
default image/state and generic iterator, and mark the named entry defaulted.
The initialized material has one complete pass and LIGHTMAP_NONE. Existing
failed-name cache matching across native lighting modes stays unchanged.

## Validation

Before editing, actual native registration reproduces retained prefix sky
metadata on a rejected definition and an empty missing-texture fallback.
Expanded normal/optimized stage sanitizer fixtures check five rejected prefix
classes (sky, fog, deform, complete/partial stages, render flags), complete
native default image/state/iterator, case-folded name identity, reuse across
lighting modes without reallocation, exported zero handles and healthy following
definitions. Missing implicit texture queries import once and cache the same
complete fallback. The 0–10-stage registration checks now expect one native
default pass for rejected counts; all valid stage outputs remain unchanged.

The fixture resets now supply defaultImage/whiteImage as native renderer
bootstrap requires. Other parser/metadata/numeric/index/cache regressions remain
covered by the existing stage/archive runners. This establishes fallback
behavior with graphics/filesystem imports isolated, not live rendering.

Both stage sanitizer configurations, the archive sanitizer runner, separate
normal/fast GCC, nine Python checks, Bash syntax and diff checks pass without
diagnostics. Both shipped Retro68 products build without diagnostics and
validate as PPC PEFs.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,745,843 | `b444788ce656d39a7bda2d4f44a03c49b1339ecefb3dc00b79d4b45fbeb50e35` |
| Quake3_TeamArena | 3,894,417 | `579bf836b10fef6d1a2b5011fd9970803b24c92a915747fc7096656154c7e3b0` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #46 open for remaining renderer input/cleanup and deferred commercial
1.32c/Mac OS 9 live acceptance. Earlier accepted-prefix fallback counts are
superseded by this complete native default behavior. Renderer-wide budgets,
transactional hunk/GPU cleanup and failures inside asset imports remain #45.
Optional FreeType stays disabled in shipped products; earlier evidence remains
dated.
