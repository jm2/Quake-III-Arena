# Complete AAS and public count setup — 2026-09-18

AAS_Setup converts nullable libvar values to integers without validation, replaces
world counts and the routing reference before allocation succeeds, then initializes
unchecked entity storage. Export_BotLibSetup also converts the counts before AAS
can reject them. Failed imports or nonfinite/oversized values can corrupt prior
world state, dereference missing entities or invoke undefined integer conversions.

Validate complete cached native counts before casting, including entity payload
cost in the signed engine allocator range. Stage all libvars and entity storage
before replacing world fields. Public setup publishes integer counts only after
successful AAS initialization. Failures use existing BLERR_LIBRARYNOTSETUP and
retain prior world/entity bytes and routing reference. Complete shared cache entries
keep native ownership for retry. Native 128/1024 defaults, positive fractional
truncation, imports, syscalls, packed layouts and legacy protocol remain compatible
with commercial 1.32c; no arbitrary new count limit is introduced.

## Validation

The fixture compiles actual public setup, AAS setup/entity initialization, libvars,
heap/hunk adapters and shared core. Twenty original-body proofs expose fourteen
nullable node/value/routing/entity imports with empty or occupied worlds, four NaN
count paths through direct/public setup and two entity-cost overflow paths. The
original successful default/fractional replacement golden passes separately.

Fixed tests cover fourteen nullable failures, twenty-eight invalid count paths,
two entity-cost failures and successful public/default/fractional replacement.
Full prior world, entity payload and routing-reference bytes remain after failure;
only complete shared cache entries survive, no failed operation consumes a hunk,
and every nullable case retries successfully. Actual entity invalidation assigns
all entity numbers. Invalid counts stop before downstream setup or public casts.
Downstream subsystem setup is stubbed; this does not prove whole-library rollback.

Physical heap ownership releases through the actual adapter. Hunk FreeMemory is
logical release only: prior replacement storage remains physically owned until the
fixture explicitly resets its engine arena. Checks distinguish these lifetimes.
Clang ASan/UBSan/float-cast-overflow and optimized GCC pass normal/fast-math,
MEMDEBUG and tracked MEMORYMANEGER configurations, six modes each. Clang reports
two existing integer abs-on-float warnings in entity proximity checks; those receive
separate numeric assessment. Five ledger/manifest checks, Bash syntax and diff checks
pass; GCC/Clang CI runs the fixture. Both PPC products build with zero diagnostics
and valid PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,779,521 | `9b60929a23b82ea8947e36c0473a733aca62ce6da468ace7a0833daaa7da5948` |
| Quake3_TeamArena | 3,928,095 | `f47fa8c23b7e958d148aa8f7d6b72f33193bd904529c56b9543ca4269c0b2537` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #47/#48 open for EA/goal and further allocation/publication consumers, routing
and aggregate parser/world budgets, full setup rollback and physical arena policy.
Entity payload validation does not bound other arrays that consume maxclients.
Retail assets, Mac OS 9 execution and target navigation acceptance remain deferred.
