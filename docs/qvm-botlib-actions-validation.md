# Botlib elementary-action validation — 2026-09-17

The fourteenth step for issue #35 validates bot elementary-action traps.
Commands and chat inputs must terminate inside the VM; movement/view vectors
and input snapshots must fit their entire size and alignment. Client indices
must fit both actual server client storage and native bot input storage.

Native elementary actions independently reject clients outside a saved input
allocation capacity, including calls made by other native bot routines.
Setup checks signed allocation arithmetic and records the allocated count;
shutdown clears it. Later changes to the global client count cannot enlarge
that bound. Valid action flags, movement speed clamping, input snapshots, and
the legacy generic-action return value remain unchanged. No syscall opcode or
botlib export-table layout changes.

## Validation

The ASan/UBSan fixture executes the actual action dispatcher into the real
native elementary-action implementations. Exact-sized VM/input allocations
cover full snapshots, command strings, vector ranges/alignment, separate
server/bot capacities, invalid native client indices, setup arithmetic, and
shutdown. Invalid requests preserve both data images and send no commands.

The new runner, affected server-core/navigation/chat runners, and all eight
Python checks pass. Both Retro68 products build without compiler diagnostics
and pass PEF validation using the temporary libraries in the
[loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,690,549 | `c7dd9b4e10c8b7e3966efc6e96d416270e25af9a869befb855671c44a70cf4ec` |
| Quake3_TeamArena | 3,839,123 | `4ebb355aaf0be55696c5f91cd9660c044026ba6b791c449ad46b7c9d0bb7235a` |

## Remaining acceptance

Keep #35 open. Remaining botlib AI syscall families, scalar indices and
indirect native accesses remain under review. This does not finish the
protected-cvar/command policy in #39. Retail 1.32c and Mac OS 9 live acceptance
remain deferred.
