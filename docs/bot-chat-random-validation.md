# Native random dictionary publication — 2026-09-18

BotLoadRandomStrings acquired persistent hunk storage before the second parse
succeeded, packed pointer-bearing records after arbitrary string lengths without
alignment, and did not bound second-pass writes or reject sticky lexical errors.
Measure aligned complete record/string costs, parse again into checked private
heap storage, reject changed capacity/source errors, then allocate a checked
persistent candidate and rebase every group/message/string pointer. Release all
private/source owners on failure; no failed dictionary consumes replacement hunk.

Keep native forward group and reverse message order/counts, case-sensitive lookup,
empty optional dictionaries and zero-message groups. NULL filename was already
rejected by the source factory; its behavior remains a native compatibility
check, while explicit full-path checks occur before imports. Public commercial
1.32c structs/imports/syscalls and valid message bytes remain unchanged; aligned
private records do not change the retail ABI. Physical hunk reset stays engine-owned.

## Validation

Actual dictionary/parser/lexer/libvar/allocator bodies cover 56 fresh/prior
nullable imports, complete prior payload/pointers/lookup, successful retry and
public shutdown. Failure retains prior roots and consumes no new persistent
hunk; temporary token recovery allocations may occur after an import fails and
are fully released. Checked sticky source errors stop further dictionary work.
Second-pass malformed/growing/empty/suffix changes and first-pass lexical/suffix
errors keep every prior byte. Arbitrary short names/messages verify aligned
pointer-bearing storage independently of sanitizer diagnostics. Native ordered
groups/messages, real one-entry lookup and optional empty/group behavior remain
separate goldens. Parent encoded-message checks pass both compilers and six modes.

Eight actual pre-change proofs reproduce these second-pass/source/hunk/alignment
failures across all twelve compiler/mode builds (96 failures). Optimized GCC
also verifies actual pointer alignment, rather than depending on x86 unaligned
access trapping. Separate original aligned native dictionaries/order/lookup,
empty inputs and already-safe NULL filename goldens pass all twelve builds.
No adapter or implementation mirror is used.

Fixed checks pass all six allocator/fast modes under Clang ASan/UBSan/
float-cast-overflow and optimized GCC; CI runs both compilers. Five ledger/manifest
checks, Bash syntax and diff checks pass. Physical ownership follows
[the actual-owner fixtures](bot-item-config-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,792,437 | `0f62088d0f42cd4d8d7ee3953bb3f088639ea23f0fff987c7f2446910ececdff` |
| Quake3_TeamArena | 3,941,011 | `b1afbc4227c93b87d1f4b9d7924339d1acce35a3d7e8e40f8c3a70f6cad97004` |

Both PPC products have valid PEF headers and build with zero diagnostics using
[the recorded toolchain libraries](qvm-loading-validation.md).

## Remaining acceptance

Keep #48 open for match/reply/initial dictionary/cache factories, complete
library/world transactions and aggregate parser/work/memory budgets. Empty
optional data remains distinct from failed load; full setup status/ownership
is separate work. Retail/Mac OS 9 execution remains explicitly deferred.
