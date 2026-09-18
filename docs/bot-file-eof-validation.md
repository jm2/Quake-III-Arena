# Complete compressed-file EOF — 2026-09-18

LoadScriptFile updates its length after compression but leaves end_p at the raw
buffer end. EndOfScript reports false at the compressed NUL. Root conditionals can
retain frames/skip state at EOF; exhausted included scripts can release while their
frames remain, corrupting parent branch/endif processing.

Set the private EOF pointer to buffer plus the complete compressed length. Native
EOF handling now warns/unwinds missing child/root conditionals before script
release. Bytes, token parsing and public layouts/imports/syscalls remain unchanged;
commercial 1.32c defaults, native protocol and retail formats remain compatible.

## Validation

Five original actual-body proofs reproduce nonempty/empty compressed-file EOF,
retained root frames/skip and active/skipped included conditionals. Fixed script
intervals and token progress reach the actual EOF pointer. Root EOF warns exactly
once, clears its frames/skip and retains only four complete source owners across
repeated reads.

An active child returns its native inside token; a skipped child omits its body.
Both physically release child frames/scripts at exhausted compressed EOF, preserve
all parent frame bytes/skip and recover parent body/endif/tail order. Native warning
severity remains; no source error is added. Teardown physically releases every
script/source/queue/frame/token owner.

The original 1,035 valid compressed-byte/token-metadata comparisons and an unchanged
noncompacting source EOF golden pass separately. Clang ASan/UBSan and optimized GCC
pass normal/release fast-math. File-comment/source-factory/source-error/include/
weight parents pass both compilers/modes. Five ledger/manifest checks, Bash syntax
and diff checks pass; GCC/Clang CI runs the fixture. Both PPC products build with
zero diagnostics and valid PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,779,493 | `2f15884c66bf250f792226fa6a51b771663d9541fbb9a4e63c8472819d5d1fe6` |
| Quake3_TeamArena | 3,928,067 | `33f5decc737c809e0d5697f05ba09cbe77607202f57f2b9db4fbc372fc63b3d9` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #48 open for further native escaped-quote compression assessment, builtin/
direct libvar and other publication consumers and aggregate work/recursion budgets.
Missing endif retains its native warning behavior; this fixes exhausted ownership,
not the full parser policy. Retail/Mac OS 9 execution remains deferred.
