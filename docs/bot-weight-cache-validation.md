# Native weight cache ownership — 2026-09-18

FreeWeightConfig previously selected cleanup from the current reload variable.
Changing that policy could free a cached owner still shared by states and leave
its cache entry dangling, or retain private records that should release. Check
actual weightFileList ownership before cleanup. Cached data belongs to weight
shutdown; uncached data releases through the caller regardless of policy changes.
The reload setting continues to control loading and caching as before.

## Validation

Actual goal and weapon public pair loaders/free functions plus fuzzy/parser/
libvar/allocator bodies cover both policy transitions, cached siblings, complete
prior state/header/tree bytes, native evaluation, cached aliases and physical
private cleanup. Cached-to-private replacement retains shared cached data and
sibling roots; private-to-cached replacement frees the private tree/name/header
and attaches a complete new index. Direct weight cleanup follows the same owner
rule. NULL cleanup is a no-op. Fixtures start with an empty cache and load the
shared prior file into its actual first native slot; tests observe that owner.

Five original weight-cleanup scenarios reproduce for each state family, compiler
and mode: 120 expected failures. Separate original ordinary cached/private
mapping/evaluation/free/shutdown goldens pass all 24 family/compiler/mode builds.
The original weight production body is unchanged in those proof builds; parent
goal/weapon pair loaders use their already validated complete publication bodies.
Physical heap owners must release before engine reset, while hunk ownership stays
distinct from logical release. Tracked modes prove zero logical owners.

Clang ASan/UBSan/float-cast-overflow and optimized GCC pass all six normal/fast/
debug/tracked modes for both families. Affected parent goal/weapon pairs pass all
Clang modes. CI runs both compilers. Bash syntax, five ledger/manifest and diff
checks pass. Physical host alignment follows [item evidence](bot-item-config-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,796,783 | `c4c386dd0434e27838e7b97819984b03d8f7fcb72e504c62fbce2994097e5b3d` |
| Quake3_TeamArena | 3,945,357 | `f69820cc7c5dc24e21a092272ba1543efea7be4b89b18311973ac502c534da63` |

Both PPC products build with zero diagnostics and valid PEF headers. Temporary
toolchain libraries: [loading evidence](qvm-loading-validation.md). Commercial
1.32c public structs/imports/syscalls, cache capacity, native protocol defaults,
loading policy and successful field/evaluation values remain unchanged.

## Remaining acceptance

Keep #48 open for complete cache/memory budgets, further publication/public
consumers, full library/world transactions and aggregate expression/parse/
recursion limits. Native cache storage still releases during BotShutdownWeights;
this step does not free shared owners while live states are attached. Retail/
Mac OS 9 execution is deferred.
