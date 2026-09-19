# Native cached/private chat ownership — 2026-09-18

BotFreeChatFile freed data retained by the cache and sibling handles. Re-loading
one cached key could immediately attach freed storage, and a reload-policy change
could either free shared data or orphan private data. BotLoadChatFile also dropped
the prior root before a failed replacement and dereferenced nullable cache imports.

Detach cached aliases without freeing their owner; free private data by actual
cache membership regardless of the current reload flag. Stage complete cache/
packed candidates before replacement. Reject invalid inputs, full cache and NULL
imports without prior mutation. Keep names that exceed fixed cache-key fields
loadable as private data, rather than storing truncated cache keys. Cached owners
remain until normal library shutdown and release once after all handle detachments.

Keep commercial 1.32c public layouts/imports/syscalls, exact fitting cache keys,
normal shared/private lookup and cache reuse without extra imports. No public
status or handle values change.

## Host and product evidence

- [The actual cache fixture](../tests/bot_chat_cache_regression.c) runs whole
  chat/parser/source/allocator bodies with physical engine-owner imports. A
  fixture-only heap-capacity option keeps the existing default and permits
  loading all 64 real cache entries and their packed owners.
- Seventy-eight nullable replacement positions cover private/shared policy,
  fresh/prior ownership, complete prior header/type/line/cache-key bytes and
  roots, source cleanup and successful retry.
- Alias detach/same-key reattach, both policy-change directions, malformed
  replacement, full cache, non-fitting keys, missing inputs and actual NULL
  cache headers reproduce 144 failures across twelve families against the
  unchanged pre-fix body. Every real cache slot loads, full capacity rejects
  without prior mutation and existing keys still reuse without imports.
- Twelve separate original native golden runs preserve ordinary shared/private
  keys, same physical cache reuse, public lookup, maximum fitting keys and
  complete physical release. Six normal/fast/debug/debug-fast/manager/
  manager-fast modes pass Clang ASan/UBSan/float-cast-overflow and optimized GCC.
  CI runs both compilers. Parent initial/state suites, five ledger/manifest
  checks, Bash syntax and diff checks pass.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,796,737 | `00f865b6a91e2db2eaa1cc285cd0bef9ead0a45ea99bb86d571323518dce9073` |
| Quake3_TeamArena | 3,945,311 | `e3c4876dac637868c3bd4fa34f55ab5d800ecaf0429e3a9658b614bf77ca2989` |

Both PPC products have valid PEF headers and build without diagnostics using
[the recorded toolchain libraries](qvm-loading-validation.md).

## Remaining acceptance

Keep #48 open for full library/world publication and aggregate parser/work/memory
budgets. Private/cached heap ownership does not establish physical engine hunk
recovery for unrelated persistent owners. Retail/Mac OS 9 execution is explicitly
deferred.
