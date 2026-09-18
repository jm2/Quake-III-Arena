# Complete native chat-library publication — 2026-09-18

BotSetupChatAI overwrote dictionary roots as loaders returned and reported
success after failed files. A later failure could leave mixed roots and spend
persistent synonym/random storage. Stage every dictionary privately, distinguish
valid empty optional files from failures, and require the checked console pool
before committing the complete dictionary group. Failed setup physically releases
all private candidates and preserves prior dictionary/pool/queue roots.

Private checked loader variants report completion separately from nullable data.
Setup keeps synonym/random candidates on the heap until commit; the existing
standalone loader wrappers retain their native persistent ownership. No fallible
imports follow successful final-pool publication. Existing shutdown releases the
new setup dictionary heap owners. Complete configuration-variable rollback and
physical replacement-pool hunk recovery remain separate acceptance work.

Keep native synonym contexts/order/weights and safe size-less replacement,
random lookup, template context/type/subtype/captures, reply construction/timing,
valid empty optional files and disabled replies. Commercial 1.32c public layouts,
imports/syscalls and native formats remain unchanged.

## Host and product evidence

- [The actual setup fixture](../tests/bot_chat_setup_regression.c) runs complete
  chat/parser/source/allocator bodies with physical engine-owner imports.
- 147 nullable imports are measured separately for fresh setup and replacement
  with live queues. Every dictionary/source/final-pool failure rejects, releases
  candidates, spends no replacement hunk and retries. Prior dictionary record/
  pointer/timing bytes, all queue/state/pool bytes and roots, and physical-owner
  counts remain unchanged. Public lookup/construction and FIFO values remain.
- Malformed/missing files in all four dictionary families reproduce 96 failures
  against the unchanged pre-fix setup. Twelve separate original native golden
  runs retain complete-source/public behavior, empty optional files and disabled
  replies. A final NULL pool import is also tested after all sources complete.
- Six normal/fast/debug/debug-fast/manager/manager-fast modes pass Clang
  ASan/UBSan/float-cast-overflow and optimized GCC. CI runs both compilers. Parent
  pool/synonym/random suites, five ledger/manifest checks, Bash syntax and diff
  checks pass. The prior pool fixture's setup check now exercises nullable
  configuration before root publication; final-pool rollback is tested directly.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,796,743 | `a61daf4e5baed2ed70c6732715224c6a714eaaf1f933a0c14e6ea8eef197986f` |
| Quake3_TeamArena | 3,945,317 | `4301f8fac851687189f6a579a14d4e4d3e90789cb67695988002dd7591282179` |

Both PPC products have valid PEF headers and build without diagnostics using
[the recorded toolchain libraries](qvm-loading-validation.md).

## Remaining acceptance

Keep #48 open for complete engine/library/world metadata transactions, aggregate
parser/work/memory budgets and remaining public consumers. Physical engine hunk
recovery does not follow from logical release. Retail/Mac OS 9 execution is
explicitly deferred.
