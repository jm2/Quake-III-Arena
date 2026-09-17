# Remaining botlib AI syscall validation — 2026-09-17

The fifteenth step for issue #35 checks the remaining character, goal,
movement, weapon, and genetic syscall families. It replaces every remaining
legacy pointer conversion in the game syscall dispatcher with full VM
string, buffer, struct, vector, inventory, or rank-array validation. The retail
baseq3/Team Arena inventory contract has 256 integer entries.

NULL long-term/movement goals and unused no-goal target outputs preserve
native behavior. Non-NULL inputs/outputs require their complete range and
alignment. Negative or overflowing rank sizes reject before dispatch. Calls
requiring botlib fail with a controlled module fault if its API is absent.

Native genetic selection now clips the inclusive random endpoint to the last
ranking: the existing random macro can return exactly one, which previously
indexed one past the ranking array. Invalid selection dimensions return
before indexing or conversion. The ranking/selection ABI remains unchanged.

This branch also carries the independent CI scheduling change in PR #68, so
future PR updates schedule one complete portable check set.

## Validation

Two ASan/UBSan fixtures execute the actual remaining-AI dispatcher and native
genetic routines with exact allocations. They cover complete inventory,
goal/move/weapon structures, bounded characteristic/filename strings,
nullable goals and no-goal targets, rank arrays and all scalar outputs,
invalid alignment/count arithmetic, and absent API handling. Native tests
force the inclusive random endpoint at three and 256 rankings and check
distinct bounded results with unchanged input. Rejected VM requests preserve
the whole image and never call native callbacks.

The new and affected server-core/navigation/chat/action runners, plus all
eight Python checks, pass. Both Retro68 products build without compiler
diagnostics and pass PEF validation using the temporary libraries in the
[loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,690,561 | `dedd0934cea235a358f2cab0840d7d2ded736b4768cc6c9c0b6e1b953507bc06` |
| Quake3_TeamArena | 3,839,135 | `ab069b8c549303ba10b2237296d0ee0fb4835b01c26985fa04130ddb4985b6d0` |

## Remaining acceptance

Keep #35 open. Scalar indices, indirect native accesses, pointer-bearing ABI
records, and unknown-capacity text operations still need review. Native bot
file/graph inputs depend on #47/#48; protected cvar/command policy remains
#39. Retail 1.32c and Mac OS 9 live acceptance remain deferred.
