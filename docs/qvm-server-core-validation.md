# QVM core server syscall validation — 2026-09-17

The tenth step for issue #35 checks core game syscall strings, file/output
buffers, structures, vectors, matrices, and arrays before native dispatch.
Supported optional NULL arguments remain for cvar resets/registration,
file-length queries, point-trace/contact bounds, config/userinfo resets,
real-time queries, and optional `AngleVectors` outputs. Legacy address masking
and trusted native module pointers remain; invalid open modes and unsupported
traps drop the active QVM.

Persistent game data receives separate validation. Entity count must cover the
server's clients and stay within `MAX_GENTITIES`; entity/client strides must
fit their shared structures and align to four bytes. Both complete arrays are
checked before either registration changes. The server saves the validated
client capacity instead of assuming a later cvar value still describes it.
Entity/client index helpers check registered bounds, and inverse entity lookup
requires an exact registered slot before division or native field access.
Errors in persistent access fault the responsible game VM even when the
engine's cached current VM refers to another module.

Native debug-polygon imports now reject counts beyond their fixed 128-point
storage and ignore invalid/reserved handles before indexing. Empty polygons
still reserve debug-line handles and do not read NULL point buffers.

## Validation

The server ASan/UBSan runner calls actual registration and array-access
functions with an exact-sized VM image. It covers private strides, complete
boundary ranges, masked/negative pointer aliases, invalid dimensions,
overflowing sizes, null/unaligned pointers, invalid indices and slots, saved
capacity after a cvar change, and faults with a different current VM. Every
rejection preserves the complete data image and server registration.

A second fixture calls the real native debug-polygon imports with exact-sized
storage. It checks full point capacity, oversized/negative counts, invalid
handles, null inputs, exhausted slots, deletion, and zero-point line reservation.

All nine sanitizer runners and eight Python checks pass. After the scoped-error
helper change, the affected server, common memory-trap, and cgame fixtures pass
again. Both Retro68 products build without compiler diagnostics and pass PEF
validation using the temporary libraries in the [loading evidence](qvm-loading-validation.md).
Portable CI includes the new runner.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,682,209 | `55deaeba6886d9674a4ad595767fdff0c2650e9ac51570f0f26c441ea2c7a7b8` |
| Quake3_TeamArena | 3,830,783 | `abeabfdef122c4902f706bf842df04a8f6f30f2e6f67889339e722724bf1e58c` |

## Remaining acceptance

Keep #35 open. Botlib syscall pointer families, returned VM strings, scalar
indices, and indirect native accesses still need review. Bounds do not impose
cvar/command permissions (#39). Retail 1.32c and Mac OS 9 live acceptance remain
deferred to the follow-up session.
