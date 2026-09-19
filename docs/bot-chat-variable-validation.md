# Native optional chat variable capacity — 2026-09-18

Initial and reply chat used eight repeated strcat blocks to append optional
variables into a fixed native match array. A single large variable or the
combined strings could cross the array boundary. Replace those blocks with a
shared private append helper that reserves the NUL byte before each copy and
reports native failure if any supplied variable cannot fit.

Initial chat stages the complete match before selecting a line, so rejection
preserves both pending chat and dictionary timing. Reply chat appends only after
an actual matching line is selected, before changing its timing or constructing
output; a nonmatching reply still ignores unused variables as before. All stage
changes are private stack data. Retain native slot numbering, packed offset
representation, provided empty strings and absent-variable behavior. No public
struct/import/syscall ABI or allocation owner changes.

## Validation

Actual initial/reply selection, optional variables and encoded expansion cover
single-variable exact capacity (255 initial, 248 after a seven-byte reply input),
slots zero/seven and all eight variables. Native constructed output and line time
remain separate goldens. Single one-byte overflow, combined 128+128 variables,
1024-byte input and an overlong final slot reject with every pending-state,
dictionary and selected-line byte retained and no new allocation. Real expansion,
shared string/parser/libvar bodies and physical ownership remain in the fixture.

Eight actual pre-change proofs reproduce all four excessive-variable families
for initial and reply chat in all twelve compiler/mode builds (96 failures).
Separate original exact-capacity/eight-variable/encoded-output goldens pass
all twelve builds. No adapter or replacement implementation is used.

Clang ASan/UBSan/float-cast-overflow and optimized GCC pass all six allocator/
fast modes; parent consumer checks also pass both compilers and six modes. An
optional no-main seam preserves their default execution. CI runs both compilers.
Five ledger/manifest checks, Bash syntax and diff checks pass. Ownership follows
[the actual-owner fixtures](bot-item-config-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,788,335 | `428863a6f29c7697ccede7005374ce94e45e1cd2d30bbe9305260694d14ec6a7` |
| Quake3_TeamArena | 3,936,909 | `a6cb06c12e24258597aa54b95ba741c15987c60359d2efd93c4520e5235f2cf6` |

Both PPC products have valid PEF headers and build with zero diagnostics using
[the recorded toolchain libraries](qvm-loading-validation.md). Commercial 1.32c
public interfaces, packed variables and native valid behavior remain compatible.

## Remaining acceptance

Keep #48 open for malformed/recursive expansion, dictionary/cache loaders,
complete library/world transactions and aggregate parser/work/memory budgets.
Retail/Mac OS 9 execution remains explicitly deferred.
