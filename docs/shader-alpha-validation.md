# Native shader alpha semantics — 2026-09-18

This focused #46 follow-up corrects two comparisons of native alphaGen_t with
colorGen_t. Use AGEN_IDENTITY when deciding whether identity alpha is redundant
for identity/diffuse RGB, and AGEN_WAVEFORM when requiring matching alpha waves
before multitexture collapse. Distinct base/amplitude/phase/frequency/function
values retain separate passes. Unused alpha-wave fields do not block portal
alpha collapse. Native stage capacities, text syntax, renderer data/module
layouts and commercial 1.32c ABIs remain unchanged.

## Validation

Actual native tests reproduce both original semantic failures independently:
identity alpha misses its valid skip, and different alpha waves collapse as if
they matched. The full shader sanitizer fixture now covers identity/diffuse/
vertex alpha combinations, each of five distinct waveform parameters, identical
waves, portal alpha with unused wave fields, and the existing distinct RGB-wave
guard. Rejected collapse retains complete original stages/metadata; accepted
collapse retains alpha/texture data. Actual R_FindShader/FinishShader registration
keeps two passes for distinct waves, one pass for identical waves, and reuses
the cache without allocations. Graphics imports are isolated; no GPU acceptance
is claimed.

The complete shader, skin and legacy font ASan/UBSan runners pass without host
compiler warnings. Nine Python checks, Bash syntax and diff checks pass.
Both Retro68 products build without diagnostics and validate as PPC PEFs.
Temporary libraries are described in [loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,737,425 | `e1fb56b48a28477063927aa92d049f866dcf1e14a3e44a02e215e493f07c20d5` |
| Quake3_TeamArena | 3,885,999 | `cee7ee36bfcc2b9e04314633ddd730f03b81063857b26fbcd678241cd2d19463` |

## Remaining acceptance

Keep #46 open for FreeType generation ownership, the remaining shader
semantic/file-allocation audit and deferred commercial 1.32c/Mac OS 9 live
acceptance. Earlier validation documents retain their dated host-warning
observations; this step resolves the two reported comparisons.
