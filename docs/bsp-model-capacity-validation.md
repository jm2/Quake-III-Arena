# BSP renderer model-slot capacity — 2026-09-17

This step for #45 checks brush submodel count against the renderer's remaining
real model-registry slots before world flags, callbacks or hunk allocations
change. R_LoadSubmodels uses R_AllocModel once per file model; exhausting the
existing 1,024-slot registry previously reached its NULL/assert path after
world loading had already changed state. Count existing cached/failed models
and the native reserved null-model slot. Check registry count before
subtraction, reject unavailable slots with input cleanup, and preserve both
cached descriptors and the old world.

The check uses MAX_MOD_KNOWN and the actual current tr.numModels. It introduces
no compiler/model-format count limit, no registry growth and no renderer or
commercial 1.32c module ABI change. Valid worlds that consume exactly the
remaining slots retain stock allocation/index/name/handle behavior.

## Validation

ASan/UBSan includes actual tr_bsp.c, tr_curve.c and tr_model.c with real
renderer structures. R_ModelInit, R_AllocModel and R_GetModelByHandle are
executed directly. Stock null-model bootstrap and one-model world publication
pass. Goldens accept the final available slot and 256 brush submodels filling
the final slots after 768 existing cached entries. Every resulting index,
*submodel name, MOD_BRUSH type, native bmodel pointer, bound and handle lookup
is checked; existing cache pointers/descriptors remain unchanged.

A full registry, two models with one slot left and 256 models with only 255
slots available reject at all four FS alignments before allocations/shader
callbacks/world or cache changes. Invalid negative/overfull internal counts
also reject safely before subtraction. Exact FS ownership is released before
ERR_DROP. Imported shader/image and hunk allocation callbacks are isolated;
model allocation itself is not stubbed. Collision patch callbacks are
stubbed and these fixture worlds have no patches.

All seven affected BSP sanitizer fixtures, nine Python checks and Bash syntax
pass; all 30 inherited CI runners remain. Both Retro68 products build without
compiler diagnostics and validate as PPC PEFs. Current master `eb5f8be`
(source through #70 and assessment #78) is integrated; the final direct-float
and tree parents #84/#85 are present. The master integration changed only
documentation after these product builds. Builds use temporary libraries from
[loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,720,811 | `d88418c545fbdecafdddc28521f05ffa39cdc46aa24bd739be5dd6e430d38b13` |
| Quake3_TeamArena | 3,869,385 | `d4020b9b20375d6c8efe2f2fabd5b40c402ebc6c2186201751fe885a404649ed` |

## Remaining acceptance

Keep #45 open. This check reserves descriptor slots; aggregate map-memory
budgets/allocation recovery still need work. Deep recursive runtime queries,
derived geometry/facet limits and complete transactional publication remain.
Retail 1.32c map/mod rendering and Mac OS 9 visual/device acceptance stay
deferred to the follow-up session.
