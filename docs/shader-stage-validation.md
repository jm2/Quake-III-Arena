# Native shader stage capacity and synchronization — 2026-09-18

This step for #46 checks MAX_SHADER_STAGES before accessing stages[s]. Reject
an excess stage through the native invalid-shader fallback path. Consume the
remaining open stage/shader using whitespace/token boundaries, quoted strings,
line/block comments and nested braces; stop safely at EOF. Shader lookup and
both archive-indexing passes share that skip logic, so following definitions
remain discoverable even after quoted braces in a rejected shader.

Valid stage operations, native eight-stage capacity, default caching and
commercial 1.32c text/module/syscall layouts remain unchanged. Do not invoke
image/cinematic parsing for excess stages. Preserve native zero-sized texture
modifier allocations but omit empty copies from null unused-bundle sources.

## Validation

The unchanged actual ParseShader/ParseStage reproduces an ASan global-buffer
write immediately beyond the eight-stage array. The actual parser fixture
covers 0–10 stages and the following shader: preserve valid stage image,
RGB/alpha, blend/depth and scroll modifiers, reject invalid counts, keep
following labels synchronized and parse the following body. Native zero-stage
fog/sky exceptions remain accepted.

Excess-stage tails cover quoted/embedded braces, comments/nesting, unused video
maps and truncated comment/string/blocks. Native registration accepts/caches
bounded valid/default shaders and registers following definitions. Both native
archive hash passes handle a rejected quoted-brace shader and its successor,
releasing input/list ownership. All fixture allocations balance. Graphics and
filesystem imports are isolated; no live GPU/retail acceptance is claimed.

The shader runner and all twelve affected BSP sanitizer runners, nine Python
checks, Bash syntax and diff checks pass. Runner 37 is registered in CI.
Two existing host warnings compare alpha fields with color enums; their valid
shader semantics need a separate focused follow-up.

Both Retro68 products build without compiler diagnostics and validate as PPC
PEFs, using temporary toolchain libraries from [loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,733,351 | `7405e6b60195414e36d2aacd9590be4184afb53f0c939062ce33772dfd5a815c` |
| Quake3_TeamArena | 3,886,021 | `caf5641b642b93da0d60c4d1d666a74fcadd9bd8ab37c581cd886309f0da468d` |

## Remaining acceptance

Keep #46 open for skin/font bounds and ownership and the remaining shader
semantic/file-allocation audit. Retail commercial 1.32c and Mac OS 9 live
acceptance remains deferred to the follow-up session.
