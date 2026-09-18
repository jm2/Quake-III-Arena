# Constant-stack native patch LOD propagation — 2026-09-18

This step for #45 replaces recursive load-time propagation between matching
patches. Each active native grid holds a private parent pointer and next-surface
index; resume its candidate scan after the child finishes and clear continuation
fields when returning. Keep native depth-first order, matching tolerances,
merged-edge exclusions, origin/radius filters and all error assignments.

The walk uses constant C stack and no heap allocation. Private grid metadata
adds one pointer and one int (eight bytes on PPC), with no commercial 1.32c
file/module/syscall change or new patch-count cap. The existing native quadratic
candidate scans remain; broader load/query budgets are separate work.

## Validation

The unchanged actual R_FixSharedVertexLodError overflows a 512 KB stack on a
4,096-patch chain. The fix passes that chain plus empty/single/two-grid cases
under ASan/UBSan with the same stack limit, propagating the native root error
and clearing all continuation fields.

401 tiny trusted fixtures compare with the unchanged recursive native routine.
They include width/height orientation, matching tolerances, merged points,
non-grid surfaces, pre-existing visit flags, differing LOD groups and a chain.
Compare every native grid/header/vertex byte, both error arrays and visit flags;
the corpus exercises more than 100 actual matching propagation steps. Allocator
wrappers reject any malloc/calloc/free during the native walk. Stock recursion
is restricted to these seven-grid fixtures.

All twelve affected BSP sanitizer runners, native MD3/MD4 and release/debug
hunk runners, nine Python checks, Bash syntax and diff checks pass. Existing
eight curve and five collision fingerprints remain unchanged. The new runner
is registered in CI (36 runners total).

Both Retro68 products build without compiler diagnostics and validate as PPC
PEFs, using temporary toolchain libraries from [loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,733,329 | `43f9d26c35bb8816bb6052c63a8bdf327e82dc182dbadba956c83ad0f2d08a0e` |
| Quake3_TeamArena | 3,881,903 | `fc1d2ec151b7a201284f73a39c0a2c8b9c53eefe6d939eaa44a4132fee8616ea` |

## Remaining acceptance

Keep #45 open. Other geometry/query validation, renderer/query budgets and
complete transactional publication remain. Renderer publication also depends
on clean shader/resource allocation paths in #46. Retail commercial 1.32c and
Mac OS 9 live acceptance remains deferred to the follow-up session.
