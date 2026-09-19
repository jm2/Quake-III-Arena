# Preprocessor include-stack budget — 2026-09-19

The native bot preprocessor rejected a file only when its filename already
appeared in the active include stack. A chain of distinct files could therefore
keep loading scripts and deepen parser recursion until allocation or the C stack
failed.

Script publication now permits at most 64 active source files, including the
root. `PC_PushScript` counts the existing stack during its original recursive
filename scan. A 65th source reports an error without changing the stack; the
include directive then releases the loaded candidate through its existing
failure path. Existing recursive-name rejection, path lookup order, source
handles, parser structures, botlib imports, QVM syscalls, protocol defaults and
commercial 1.32c data layouts remain unchanged. Valid stacks through the new
boundary retain their original order and ownership.

## Validation

The actual include directive and script publication bodies run against native
script owners. A root plus 62 nested owners accepts one more loaded include for
exactly 64 active sources. A root plus 63 nested owners rejects the next loaded
include, preserves the prior top of stack, emits one error and frees the
candidate exactly once. Final cleanup releases every accepted owner.

The complete existing include suite also retains quoted and angle-path bounds,
direct/fallback lookup order, token synchronization, recursive-name rejection
and script ownership. GCC and Clang ASan/UBSan pass normal and optimized
fast-math modes. The parent preprocessor work-budget suite passes both compilers.
Five ledger/manifest checks, Bash syntax and diff checks pass. Both Retro68
products rebuild without compiler diagnostics and have valid PPC PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,805,027 | `f75d6a0545a752bb6404f262ed4924280756ea784862d1ea1747176dfb898610` |
| Quake3_TeamArena | 3,953,601 | `fdfd74bd4a2fa3cc021c4feca44192e2457ae24f873bef67b311974834a18d65` |

## Remaining acceptance

Keep #48 open for remaining character/source allocation consumers and aggregate
parser memory budgets. Retail/Mac OS 9 execution is deferred.
