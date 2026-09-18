# AAS header/lump layout and exact reads — 2026-09-18

This first #47 file-root step retains retail AAS version 4/plain and version
5/header obfuscation, typed element layouts, map checksum policy and the native
hunk allocator. Retain file length, require an exact header read and preflight
all fourteen nonnegative, in-file, element-sized lump ranges before discarding
the currently loaded world. Nonempty data must follow the complete header;
empty ranges still require an in-file offset. Avoid overflowing range sums.
No new format tag or commercial 1.32c QVM/syscall field is introduced.

Require exact payload reads and track actual offset plus length after seeks.
Read/seek/allocation failures close the file once and clear all partial logical
world ownership/counts. Preserve native dummy storage for empty lumps, but reject
failed dummy allocation. Use GetHunkMemory plus checked clearing, matching the
native allocator that GetClearedHunkMemory previously wrapped.

## Validation

The original actual loader discards a previous world on a missing replacement,
requests an invalid negative seek and ignores a truncated actual header. New
actual-body ASan/UBSan checks cover both versions, every 0–123-byte advertised
and actual header prefix, missing files, ID/version/checksum errors and short
header reads. Every lump tests ten signed/range/stride/header-overlap classes
while preserving complete prior world state and logical arena ownership.

Independent literal byte headers assert the fourteen retail element sizes.
Empty/sequential/reversed layouts cover all counts and one-time file ownership;
mixed reordered payloads retain independent literal 1.25 vertex/2.5 plane data
and require three accurate seeks. Every payload read fails once in both versions;
every nonsequential seek and each real/dummy allocation failure clears partial
counts/owners without duplicate close/release. Layout acceptance retains the
original loaded behavior; these fixtures do not claim valid traversable graphs.

The AAS sanitizer fixture, optimized GCC check, four related bot dispatcher
runners, nine Python checks, Bash syntax and diff checks pass. Changed AAS code
emits no diagnostics. Existing VM dispatcher seams retain their prior 64-bit
pointer-cast/debug-format warnings. Both shipped Retro68 products build without
diagnostics and validate as PPC PEFs.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,745,889 | `02028ffe3d62babed3b8d4d944b2a573c1fa51605c7f444679aafefe5ef78dd3` |
| Quake3_TeamArena | 3,894,463 | `93dd93b8f6957f83bcca41b5fb833708af49a3636041554a68c8cf5732a44681` |

After integrating merged shader fallback PR #118, the AAS/stage/archive sanitizer
runners and nine Python checks pass again. Both product builds remain at the
same sizes with zero diagnostics; combined-source hashes are
`eb1cd923e894fb4be67b38289c2b5b333fb5cd5ea6e3c0d804f8dbf110b85cb6`
(base) and `8a83418f8569d6323e3ed8d1e7619aceb9c96437578a0af6ca66f2cd359a471f`
(Team Arena). The earlier table records the original candidate build.

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #47 open for graph/reference/numeric validation before loaded publication,
transactional late-load rollback, aggregate arena/runtime budgets and complete
mover/entity-model checks. Native bbox float endian conversion also needs its
follow-on audit. Logical FreeMemory on native hunk storage does not physically
reclaim arena bytes; the fixture models that distinction and releases physical
scratch only at reset. This step retains prior world only on header/layout
preflight failure; late failures clear the partial world. Do not claim arena
recovery or complete transactional replacement. Commercial 1.32c/Mac OS 9 bot
acceptance is deferred and bots remain disabled pending root work. Earlier
evidence remains dated.
