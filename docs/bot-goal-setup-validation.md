# Complete native goal setup — 2026-09-18

BotSetupGoalAI casts unchecked game type and replaces global item configuration
before a failed parse can return. It resolves nullable droppedweight after consuming
permanent item storage and still reports success if the weight reference is missing.
Successful replacement also retains prior tracked logical configuration ownership.

Validate nullable game-type initialization and finite/representable conversion.
Stage complete configuration/weight libvars before parsing one complete native item
configuration, then publish all goal fields and release the prior logical record.
Failed imports/parsing preserve prior references, game type and item payload. Native
error codes/severity, defaults, configured/fractional values, commercial 1.32c public
interfaces and legacy protocol remain compatible.

## Validation

Sixteen original actual-setup proofs reproduce twelve nullable factory stages with
empty/occupied prior state, NaN game-type casting, failed item replacement and two
isolated nullable weight imports. They fail in four normal/fast and tracked/untracked
proof configurations. Original first/default/field golden passes in all four modes.
Ordinary configured replacement passes original untracked modes; tracked replacement
exposes the prior logical owner left linked.

Fixed checks cover every nullable game/config/weight node/value, five invalid game
types, malformed/source-error files, isolated weight failures and complete native
first/configured replacements. Prior references, game type, item header and every
item payload byte remain on failure. Nullable libvars stop before VFS/physical hunk
imports, retain only complete shared cached variables and retry successfully. Parser
failure cleans private source/staging owners and consumes no additional physical
hunk. Native 0/1000/default config and configured 2.75-to-2 type/777 weight values,
actual parsed field values and cached ownership remain. Tracked teardown has zero
logical blocks/bytes; physical hunk owners remain with the engine until arena reset.

Clang ASan/UBSan/float-cast-overflow and optimized GCC pass six normal/fast/debug/
tracked allocator modes without diagnostics. Actual item/source/structure/allocator
bodies compile with the fixture; host import alignment uses measured ownership
prefixes as documented in [item evidence](bot-item-config-validation.md).
Five ledger/manifest checks, Bash syntax and diff checks pass; GCC/Clang CI runs
the fixture. Both PPC products build with zero diagnostics and valid PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,783,813 | `f2e2dcf080575e7cf5d8053520a3475a093aa908a9a8e36f73874edaba5ad59a` |
| Quake3_TeamArena | 3,932,387 | `73dcb1635d50cbf67027e964158da98a35baaa7303f804b0941e4500d31d8abd` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #47/#48 open for level-item/weapon/chat and further publication/allocation
consumers, aggregate memory/work/recursion budgets and full world/library transactions.
This verifies goal setup, not all runtime goal/item semantics. Retail/Mac OS 9
execution and target bot acceptance remain deferred.
