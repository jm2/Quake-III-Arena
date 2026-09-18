# Renderer face plane numeric preflight — 2026-09-18

This step for #45 validates the native first-point/normal dot product before
world, shader, model or hunk changes. Finite source words previously published
an infinite planar face distance. Decode the validated first vertex and face
normal bytewise, execute the same native expression and reject nonfinite
results. Ordinary face parsing, normal classification and file/module/syscall
layouts remain unchanged, with no new coordinate limit.

## Validation

The unchanged actual ParseFace publishes an infinite distance from FLT_MAX and
a finite normal component of 2. The actual tr_bsp.c fixture now rejects product
overflow, sum overflow and opposite infinite-term cancellation at all four FS
alignments. Each actual RE_LoadWorldMap rejection frees the file and preserves
world/sun/shader/model/hunk/heap state without curve workspace allocation.

Four native finite results retain exact distances, normals, plane types and
sign bits, including nonaxial normals, positive/negative axial normals,
FLT_MAX coordinates with zero normal components and large coordinates with
small normals. Existing native face/triangle storage, RGB/alpha and eight curve
fingerprints remain unchanged. All eleven affected BSP sanitizer runners,
nine Python checks, Bash syntax and diff checks pass.

Both Retro68 products build without compiler diagnostics and validate as PPC
PEFs, using temporary toolchain libraries from [loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,733,329 | `d02d52b3297cf196d0fd1178c58670c8c7366dfca9f47a66a012d2872b39e0b7` |
| Quake3_TeamArena | 3,881,903 | `856ab3df164319971b93b0f86a8c2d1d6950b52bd4de5ad91487b074c035cae8` |

## Remaining acceptance

Keep #45 open. Other geometry/query validation, renderer/query budgets and
complete transactional publication remain. Retail commercial 1.32c and
Mac OS 9 live acceptance remains deferred to the follow-up session.
