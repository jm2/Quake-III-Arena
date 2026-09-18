# Native weapon setup publication — 2026-09-18

BotSetupWeaponAI previously assigned LoadWeaponConfig directly to the shared
root, losing the previous complete configuration on failure. Its filename helper
also hid a nullable variable factory behind an empty string. Check the actual
variable/string owner, load a private complete candidate and replace the root
only after success. Successful replacement releases the prior logical allocation
record; the owning engine still resets physical hunk storage. Native filename
weapons.c, return codes, layouts, count behavior and parser fixups remain intact.

## Validation

Actual setup and full weapon factory/parser/libvar/allocator bodies exercise all
seventeen nullable imports with empty state and all thirteen with populated state.
Every failure retains the shared root, complete header, both full inline arrays,
unused bytes and copied projectile fixups; private owners release, only complete
shared variables survive and no additional physical hunk is spent. Each case
retries successfully. Malformed syntax and suffix source errors cover empty and
populated state. Separate initial default and complete configured/fractional
replacement goldens verify fields, pointers, source cleanup and native severity.
Tracked modes prove that all logical records release, while physical hunk owners
remain until the engine arena reset.

Four original actual-setup failure proofs reproduce independently for both
filename factory imports and both malformed/suffix loads with populated prior
state, across six modes under both compilers. Separate original native first-
setup/default/field/fixup goldens pass all twelve builds. The original setup and
its parent checked loader bodies are unchanged in these proof builds.

Clang ASan/UBSan/float-cast-overflow and optimized GCC pass six normal/fast/debug/
tracked modes without diagnostics. CI runs both compilers. Bash syntax, five
ledger/manifest checks and diff checks pass. Physical host alignment follows
[item evidence](bot-item-config-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,784,017 | `8fe0d27146bb1342813a9c18aadad8d601ffd67e49178a07aeaed337d5130a17` |
| Quake3_TeamArena | 3,932,591 | `c50240ff0487ef71b18f7c72eb33ccd49bc3d27d09c90ed973188e3c66acde4d` |

Both PPC products build with zero diagnostics and valid PEF headers. Temporary
toolchain libraries: [loading evidence](qvm-loading-validation.md). Commercial
1.32c public imports/syscalls, protocol defaults and native file format remain.

## Remaining acceptance

Keep #48 open for weight-pair/public state consumers, further nullable factories,
chat, complete transactions and aggregate expression/parse/recursion/memory
budgets. This fixture does not claim active weight-index rebuilding across
configuration replacement. Retail/Mac OS 9 execution remains deferred.
