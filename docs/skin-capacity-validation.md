# Native skin bounds, allocation and ownership — 2026-09-18

This step for #46 retains the commercial 1.32c skin/module/syscall layout and
native cache behavior. Allocate a complete skinSurface_t for both the default
skin and the single-shader path. Check name length before forming the .skin
suffix pointer, preserving native case-sensitive extension matching.

Check the native 32-surface capacity before allocating/indexing an additional
surface or importing its shader. Reserve the token terminator for quoted and
unquoted text; reject overlong tokens, unterminated quotes/block comments and
missing shader tokens. Cache malformed skins with zero active surfaces, return
the native default handle, and free the input exactly once. Native permanent
hunk/cache allocations for prior valid rows remain owned until normal renderer
shutdown; this is not a rollback of shader/hunk caches.

## Validation

The original actual R_InitSkins writes the default shader past a pointer-sized
allocation. With safe fixture default initialization solely to isolate the
remaining unchanged production entry points, sanitizers independently reproduce
RE_RegisterSkin's pointer-sized surface write, short-name suffix read before
an exact input allocation, 1024-byte quoted-token terminator write, and 33rd
surface array write. The final fixture executes actual R_InitSkins and
RE_RegisterSkin with isolated FS/shader/graphics imports and exact allocations.

ASan/UBSan covers every name length 1–63 (including required 1–4), null/empty/
64-byte names, native .SKIN case behavior, 0–34 surfaces (including 31/32/33),
exact complete surface allocations and native lowercase assignment/shader order.
Quoted commas, whitespace/comments and ignored tags preserve native behavior,
including an ignored tag after 32 surfaces. Test 1022–1025-byte quoted/unquoted
surface and shader tokens, EOF positions, truncated quotes/pairs/comments, every
prefix of a complete legacy corpus, missing/empty files, full-cache limits,
invalid handles and valid/default cache reuse without new imports. FS inputs
and fixture allocations balance; excess surfaces/tokens invoke no new imports.

The skin/shader runners, six affected image/model runners, nine Python checks,
Bash syntax and diff checks pass. Runner 38 is registered in portable CI.
The two existing shader alpha/color enum warnings remain the documented
separate semantic follow-up; both Retro68 products build without diagnostics.

| Product | PPC PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,737,425 | `1e3fdb0effa2512c124cdee38fe8f38c00b9188040f4f28d58b380ed252803a4` |
| Quake3_TeamArena | 3,885,999 | `711d2a7ab7ccda3bff5b4f8ed3f2b25160feeac6de49d86e18b3dbcc6f1f7ac4` |

Temporary toolchain libraries are described in [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #46 open for serialized font layout/bounds and file/FreeType ownership,
remaining shader semantic/file-allocation checks, and deferred commercial 1.32c
retail and Mac OS 9 live acceptance. Builds and isolated fixtures do not prove
retail asset or target API behavior.
