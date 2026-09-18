# Returned QVM string validation — 2026-09-17

The eleventh step for issue #35 validates connection-denial strings returned by
`GAME_CLIENT_CONNECT` before native code prints or copies them. Direct connect,
map restart, and map change now use a checked conversion against the supplied
game VM. A non-NULL QVM string must terminate within that VM's allocation;
invalid strings fault the owning module before `ERR_DROP` shutdown.

Validation does not change `currentVM`. The call dispatcher can restore a
different cached module after a call, so using the active-module string check
would inspect the wrong allocation. The common pointer backend now accepts an
explicit owning VM; existing syscall pointer/string checks use that backend
with their active VM. Zero still means a successful connection, legacy masked
addresses remain, and trusted native modules retain their 32-bit pointer ABI.

## Validation

The new ASan/UBSan fixture allocates exact-sized, different data images for the
owning and current VMs. It checks nullable success, required NULL rejection,
unterminated owner boundaries despite a terminated current image, termination
and empty strings at the last byte, positive/negative aliases, unchanged active
context, and native pointer conversion. Rejections preserve the owner image
and fault only the owner.

The new fixture and affected common memory-trap, cgame, and server-core runners
pass; all eight Python checks pass. The earlier loading, bytecode, runtime,
argument, and UI fixtures remain enabled in CI. Both Retro68 products build
without compiler diagnostics and pass PEF validation using the temporary
libraries in the [loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,682,209 | `29cbf1b1901a23802cea82045dc3081d03a16f63a881da90cf545b66aac60c0f` |
| Quake3_TeamArena | 3,830,783 | `d590d165145c9d5a39b1c6932ecf1c693e06a4526548b82f9d4053c98786984e` |

## Remaining acceptance

Keep #35 open. Botlib syscall pointer families, scalar indices, and indirect
native accesses still need review. Retail 1.32c and Mac OS 9 live acceptance
remain deferred to the follow-up session.
