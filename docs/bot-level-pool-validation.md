# Complete native level-item pool — 2026-09-18

InitLevelItemHeap frees prior storage before resolving unchecked count/allocation
imports, then links missing or zero-size storage and writes at count minus one.
The public initializer clears map information before noticing no heap failure.

Validate complete native counts and signed payload cost before integer conversion.
Stage cleared pool storage and its complete free chain before replacing heap/free/
active lists and count. Propagate the private failure before public map-information
reset. The engine map API uses an internal checked initializer, returns an existing
nonzero error on pool failure and skips brush/success handling. The game loader
checks this status before bot resets/deathmatch setup. Failed setup or load disables
bot creation/frames for that map; human initialization continues with a warning.
Successful reload restores readiness. An engine-owned map flag survives game
DLL/QVM data reset and is queried through the existing variable syscall on
tournament restart; an ordinary/stale variable cannot revive a failed map. This
query allocates no cache and leaves the retail syscall table unchanged.
The retail void export/syscall remains unchanged. Prior pool/list bytes survive failure; complete shared cache variables retain
native retry ownership. Existing 256 default, positive fractional truncation,
allocation/free/list behavior, commercial 1.32c interfaces and protocol remain.

## Validation

Twenty original actual-pool/public-path proofs expose twelve nullable node/value/
pool imports with empty/occupied prior state, zero/fractional-below-one/negative/
oversized counts, infinities/NaN and payload overflow. They fail in normal/fast modes.
Original private/public 256-node ascending free-chain/cleared allocation/list/
exhaustion goldens and native 2.75-to-two-node golden pass separately. The proof
renames the original void helper and uses a return-true adapter for the new private
status signature; original pool body and public caller behavior remain unchanged.

Fixed tests exercise twelve nullable paths and sixteen invalid count/cost paths
through private/public initialization. Every failed setup retains complete pool,
free/active pointers, active count and both node payloads; no map-info iterator runs
and only complete shared variables survive. Nullable cases retry and physically
release prior heap storage. Native default and configured chains, actual Alloc/
Free/Add helpers, clearing and exhausted-pool diagnostic remain. Successful complete
replacement resets active state and publishes only its new free chain. Native tracked
allocator teardown leaves zero logical blocks/bytes; all physical heap owners release.
Public success uses a synthetic empty BSP; runtime navigation queries assert unreachable.

Clang ASan/UBSan/float-cast-overflow and optimized GCC pass six normal/fast/debug/
tracked allocator modes without diagnostics. Actual goal/structure/source/allocator
bodies compile; host callback alignment uses measured ownership prefixes as documented
in [item evidence](bot-item-config-validation.md). Seven pre-review actual map-API proofs reproduce six nullable imports and NaN
count reporting false success; native success, setup/AAS early gates pass original
and fixed. Actual engine map API tests pass all six allocator modes with both
compilers and check native retry/call order, retained prior bytes and brush skipping.
Two additional pre-review game-loader/initializer failure proofs fail in four
base/missionpack normal/fast configurations. Separate original successful map/game
initialization, tournament restart and bots-disabled goldens pass. Fixed actual
game load/setup/shutdown/initializer bodies pass with both compilers in those four
configurations, checking empty/occupied prior bot bytes, two engine error codes,
retry and native resets. Frame/client checks preserve their exact entry prefixes
up to the first import and use a success sentinel; complete bodies are PPC-built.
A further pre-review fresh-VM restart proof reproduces the cleared-static-state
regression in all four configurations; same-module native goldens pass separately.
Fixed tests cover fresh VM restarts with successful/failed retained engine maps.
Actual engine status queries check bounded output, no cache allocation, immunity
to an ordinary stale variable and false status after failed load/uninitialized
library. Setup clears the private flag; shutdown clears it before teardown.
Both changed game files also compile to baseq3/missionpack QVM bytecode assembly
with the repository legacy compiler built as a 32-bit host tool. A full game QVM
build reaches a pre-existing missing QVM-libc strrchr dependency in q_shared.c;
that build blocker is a separate follow-up, not completed acceptance here.
Five ledger/manifest checks, Bash
syntax and diff checks pass; GCC/Clang CI runs the fixture. Both PPC products build
with zero diagnostics and valid PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,783,921 | `1da5e9595d7abca2db4012ad1077987dfcf3b05a55bd06123ae7db7d1f6cf72b` |
| Quake3_TeamArena | 3,932,495 | `c6e19f3899a6c6812c41d666ee3c1edbee64021835097bce907f1a92654d176e` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #47/#48 open for map-info/weapon/chat and further publication/allocation
consumers, aggregate budgets and full metadata/world/library transactions. Pool
failure preserves map info; later metadata/entity-scan failures still require their
own transaction policy. Retail/Mac OS 9 execution and target bot acceptance remain
deferred.
