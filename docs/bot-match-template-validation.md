# Native complete template factory — 2026-09-18

BotLoadMatchTemplates accepted incomplete context blocks at EOF, returned a
complete-looking prefix after source errors and dereferenced unchecked persistent
container imports. Use checked heap containers paired with the owned match
pieces, require every context delimiter and reject sticky source errors before
returning. Free all candidate containers/pieces and the source on any failure.
No failed candidate consumes persistent hunk; successful containers release
physically through the existing normal free/shutdown path.

Keep native forward template order, context masks, type/subtype values and public
variable captures. Empty optional files and closed empty contexts remain accepted
without storage. Public imports/syscalls and commercial 1.32c grammar are unchanged.

## Host and product evidence

- [The actual factory fixture](../tests/bot_match_template_regression.c) runs the
  whole chat/parser/source/allocator bodies with physical engine-owner imports.
- Sixty source/container/piece/string nullable positions across fresh/prior
  dictionaries reject, preserve prior header/pointer bytes and public lookup,
  release every candidate/source owner and retry. Normal public shutdown frees
  successful heap containers/pieces physically.
- Missing context delimiters, malformed metadata/pieces, bad trailing tokens,
  lexical/directive errors and NULL container imports reject without spending
  candidate persistent memory. Eight families reproduce 96 failures against the
  unchanged pre-fix body. Twelve separate original native golden runs retain
  forward order, contexts/type/subtype, actual public captures, empty optional
  files and closed empty contexts. Failure checks compare persistent ownership
  with the prior baseline; they distinguish prior memory from a failed candidate.
- All six normal/fast/debug/debug-fast/manager/manager-fast modes pass Clang
  ASan/UBSan/float-cast-overflow and optimized GCC. CI runs both compilers. Parent
  piece tests, five ledger/manifest checks, Bash syntax and diff checks pass.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,792,511 | `8df507c927bc5b677e14b35b2d25c66d6eaf9d83c44592b49faaf300a5de2789` |
| Quake3_TeamArena | 3,941,085 | `cb4e60837ab515492b36b8ca23041edcf6bdd9f5bdbe0ca1c079ef588409fdbd` |

Both PPC products have valid PEF headers and build without diagnostics using
[the recorded toolchain libraries](qvm-loading-validation.md).

## Remaining acceptance

Keep #48 open for reply/initial dictionary/cache factories, complete library/world
publication and aggregate parser/work/memory budgets. Optional empty data and
failed loading still require distinct status in complete setup work. Physical
hunk reset remains engine-owned. Retail/Mac OS 9 execution is explicitly deferred.
