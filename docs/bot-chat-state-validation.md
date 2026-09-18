# Native chat state lifecycle — 2026-09-18

BotAllocChatState previously returned a positive handle after a NULL import.
Return native failure handle zero and publish no incomplete state. Shutdown now
visits slots 1 through MAX_CLIENTS, including the previously skipped final slot,
and clears both console heap/free-list roots after release. Native successful
handle allocation, properties and queue behavior remain unchanged.

## Validation

Actual chat/factory/message-list/libvar/allocator bodies cover fresh and occupied
nullable state imports, every prior byte/root and same-slot retry to complete
cleared state. Actual public allocation fills all 64 client slots and returns
zero without imports when full; shutdown releases every root and permits slot 1
reuse. Native name/client/gender/empty-state goldens use the real setters and free.
Actual two-message pool and FIFO operations retain handles/time/type/text and
public output links. Shutdown removes the remaining queued message, logically
releases pool storage, frees states and clears both pool roots before a physical
engine arena reset. Repeated shutdown leaves no remaining owners. Tracked modes
prove zero logical records; physical hunk ownership is kept distinct.

Four original actual-chat proofs independently reproduce false positive handles
with fresh/occupied state, skipped final-slot shutdown and stale console root.
Each fails in all twelve compiler/mode builds. Separate original native first-
state/property/free/shutdown goldens pass all twelve builds. Original production
bodies and actual dependencies are unchanged in these proof builds. FIFO timing
uses the AAS clock boundary stub; queue/state/allocator bodies are actual code.

Clang ASan/UBSan/float-cast-overflow and optimized GCC pass all six normal/fast/
debug/tracked modes without diagnostics. CI runs both compilers. Bash syntax,
five ledger/manifest checks and diff checks pass. Physical host alignment follows
[item evidence](bot-item-config-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,788,171 | `25614ba333047ad64425aa619accacea3bf31ecb6194e8adcb14965301f2e193` |
| Quake3_TeamArena | 3,936,745 | `26e61cd1c70f2b76e4adf72199ad93aa57efb66425c5457b346de67241c489bb` |

Both PPC products build with zero diagnostics and valid PEF headers. Temporary
toolchain libraries: [loading evidence](qvm-loading-validation.md). Commercial
1.32c public structs/imports/syscalls, handle conventions, protocol defaults and
native field/queue behavior remain unchanged.

## Remaining acceptance

Keep #48 open for console pool count/allocation/publication, further chat loaders/
string consumers, cache policy ownership, complete library/world transactions
and aggregate expression/parse/recursion/memory limits. The ordinary two-message
pool golden does not claim count/factory hardening. Retail/Mac OS 9 execution is
explicitly deferred.
