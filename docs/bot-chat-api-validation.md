# Native public chat API boundaries — 2026-09-18

Console input of 256 bytes or more could fill its native fixed message array
without a NUL terminator. Use the existing bounded native string helper and
reject missing input before allocating a queue slot. Reject missing console
output before copying queued messages. GetChatMessage rejects a missing output
or nonpositive size before removing tildes or consuming pending text. Reject a
missing chat name before changing client/name metadata, and missing match input
or output before copying. The existing bounded match copy is unchanged.

Valid FIFO handles, time/type/text, property bytes, match newline trimming and
GetChatMessage truncation, strncpy padding, NUL termination and consumption
remain native. Commercial 1.32c public structs, imports and syscall tables are
unchanged. These guards add no allocation or persistent owner.

## Validation

The actual chat API, queue/state, parser, libvar, allocator and shared string
bodies cover console lengths 0/1/31/254/255/256/1024 with output canaries and
native FIFO values. Missing queue input preserves every pool/client/serial byte;
missing FIFO output retains the queued message for retry. Chat output sizes
0/-1/INT_MIN and NULL preserve every pending state/destination byte; valid sizes
1/5/32 preserve native tildes, truncation, tail padding and consumption. Missing
name and match inputs preserve state/output. Native bounded match copying,
long-text clipping and trailing-newline removal have separate goldens.

Eight proofs against the actual pre-change chat source reproduce all eight
failures across six modes under Clang sanitizers and optimized GCC (96 failures).
Separate original first-state/property/FIFO/output/match goldens pass all twelve
builds. No production adapter or implementation mirror is used. Physical heap
ownership and logical record cleanup are checked independently from engine-owned
arena reset, following [item evidence](bot-item-config-validation.md).

The fixed checks pass normal, fast, debug, debug-fast, tracked and tracked-fast
modes with both compilers. CI checks Bash syntax and runs both compilers. Five
ledger/manifest checks, Bash syntax and diff checks pass.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,788,307 | `a5447808bffb6e522c908db05e8014a1afacea68c08c2612608e354c3dd28558` |
| Quake3_TeamArena | 3,940,977 | `f7844674cdf6d199e5dd6f585aa66bb1a03b67aede4adf2f154e56328b8a4cfa` |

Both PPC products have valid PEF headers and build with zero diagnostics using
the [recorded temporary toolchain libraries](qvm-loading-validation.md).

## Remaining acceptance

Keep #48 open for reply/expansion and other chat consumers, dictionary/cache
ownership, full world/library transactions and aggregate expression/parse/
recursion/memory budgets. Retail/Mac OS 9 execution is explicitly deferred.
