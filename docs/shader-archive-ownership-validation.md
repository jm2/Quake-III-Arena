# Shader archive sizes, ownership and empty restarts — 2026-09-18

This focused #46 change keeps the commercial 1.32c renderer interface, shader
syntax, existing 4,096-file cap, reversed combined text order and native indexed
lookup order. It requires no retail asset changes.

Clear private archive text/index pointers before renderer initialization and
archive discovery. A missing, zero-count or invalid negative-count list leaves
no archive published; release any non-null empty list. Lookup tolerates a null
hash bucket and still creates/cache-reuses native implicit image shaders.

Check each complete scripts/filename path and nonnegative actual FS read length,
then check aggregate addition within signed native Hunk_Alloc sizes. Fail before
permanent archive allocation on controlled path/read/aggregate errors. Release
all acquired FS buffers newest first and the list before ERR_DROP. Normal
FS_ReadFile inputs are NUL-terminated by the filesystem. Preserve per-file
COM_Compress output while assembling the reversed text in linear copy order;
check index counts and native pointer-byte multiplication before allocation.

## Validation

The unchanged actual renderer reproduces a no-file null-bucket load under UBSan,
a restart use-after-free under ASan, unfreed non-null empty lists/prior inputs on
read failure, and an invalid-negative-read-length heap overwrite. The last test
uses an isolated FS failure-contract mock returning a non-null owned buffer with
a negative length; it does not claim this is a normal successful native FS read.

The new actual-body sanitizer fixture shares the existing shader fixture's
isolated graphics imports and executes the public R_InitShaders/R_FindShader
paths. It covers missing and non-null empty lists; startup/cache reuse; actual
hunk teardown followed by an empty restart; unreadable later files; negative,
signed-maximum and overflowing aggregate lengths; null/oversize paths; the exact
maximum valid native path; empty files; negative list counts and the 4,096-file
cap. Synthetic reported lengths exercise integer allocation contracts without
allocating gigabytes. Controlled rejection has no permanent archive allocation,
all input/list ownership balances, and every FS release is in reverse order.

An independently assembled tiny stock-order/compression oracle matches the
complete valid two-file text. Native first-label duplicate priority, both unique
labels, parser output and cache reuse remain intact. The fixture is required in
Portable CI alongside the existing stage/skin/font regressions.

All five affected shader/skin/font sanitizer runners, nine Python checks, Bash
syntax and diff checks pass without diagnostics. Both shipped Retro68 products
build without diagnostics and validate as PPC PEFs.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,737,459 | `86ddb6c15ce02664de72a3c98a8487dedbce50ea26d9d0f5d3ceba4f43ddff5b` |
| Quake3_TeamArena | 3,886,033 | `d8a2beb3fa1854ca05fb678df3accc49ca3adad390162ae16fab25218922916a` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #46 open for definition/index isolation and the remaining shader semantic
audit, plus deferred commercial 1.32c/Mac OS 9 live acceptance. Renderer-wide hunk
capacity, fatal allocator rollback and transaction/publication work remain #45;
this step checks arithmetic and controlled input ownership, not full renderer
allocation-budget acceptance. Earlier validation documents remain dated evidence.
