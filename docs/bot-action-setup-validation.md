# Complete action input replacement — 2026-09-18

EA_Setup already rejects invalid client counts and oversized input payloads, but
assigns a nullable allocation directly to botinputs and clears actual capacity on
failure. Complete prior input state becomes inaccessible. A successful replacement
also leaves its prior logical tracked allocator record linked.

Stage native input storage, then release the prior logical record and publish its
pointer/capacity together. Capture the validated count for allocation/publication.
Existing error codes, count/cost checks, cleared payloads and action behavior remain.
Commercial 1.32c imports/syscalls/layouts and legacy protocol stay compatible.

## Validation

Actual EA setup/action/input/shutdown and native allocator adapters compile with
the existing AAS physical-owner fixture. Original occupied nullable replacement
fails in normal/fast and tracked/untracked configurations; original tracked
successful replacement leaves logical owners. Original empty nullable failure and
native default/action goldens pass separately in all four proof configurations.

Fixed cases cover nullable input allocation with empty/occupied prior state, eight
invalid count/cost paths, successful replacement and native 128-client/default
clearing/action/input export. Failed replacement retains every prior payload byte,
pointer and actual capacity despite a larger public global count; prior inputs
remain usable. Retry publishes complete cleared storage and larger actual capacity.
Tracked teardown leaves zero logical blocks/bytes. Physical hunk replacement owners
remain with the engine until an explicit arena reset; native heap owners release.

Clang ASan/UBSan/float-cast-overflow and optimized GCC pass all six normal/fast,
debug and tracked allocator modes without fixture compilation diagnostics. Existing
issue #35 VM/native action and server client tests pass both compilers, retaining
their existing 64-bit-host native-pointer/VM-format warnings. Five ledger/manifest
checks, Bash syntax and diff checks pass; GCC/Clang CI runs the new fixture. Both
PPC products build with zero diagnostics and valid PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,779,537 | `f44f11283888dd3e0fa3d9cb7994f6ebc169bcff552ace4cc55af5b434c17a4e` |
| Quake3_TeamArena | 3,928,111 | `4108be51f32ab65d40e457da210531dedd71d8f6d9653d19ae3ef371186a1cb6` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #35/#47/#48 open for aggregate/runtime and further publication consumers,
full library/world rollback and physical arena policy. Retail/Mac OS 9 execution
and target action/navigation acceptance remain deferred.
