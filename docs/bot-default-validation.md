# Native default characteristic transactions — 2026-09-18

Default inheritance changes numeric/type fields before string cloning finishes,
dereferences nullable imports and cannot report failure to its skill caller.
Clone all missing strings privately before changing target fields. Returned
failure releases prospective clones and preserves the entire target. Successful
inheritance retains native missing-field values and independent string ownership.

The sole internal caller checks the new status. Preserve cached targets on
failure; release only targets created by this call, including reloads alongside
an existing cache entry. Return no failed handle. Complete defaults may remain
cached, and native missing-file fallback remains intact. Public commercial 1.32c
QVM/syscall interfaces, character structures and ordinary values are unchanged.

## Validation

The runner links complete actual character/preprocessor/lexer/libvar/Q_shared
bodies with real file/heap/printing interfaces. Default/target inputs come from
native skill source text. Inheritance, cache loading, reload cvars, public getters,
registry cleanup and physical owner release execute production bodies.

Six original actual-body proofs fail: all three nullable string clones, numeric
mutation before cloning finishes, and ignored failure for new/cached targets.
Caller proofs replace only the original inheritance helper with the fixed actual
body, so its null-copy defect cannot mask the original caller defect. Ordinary
native inheritance, new-target loading and fallback goldens also pass against
the original bodies.

Literal numeric defaults and first/empty/last strings fill missing fields;
occupied values, filename/skill and original strings remain intact. Copied strings
are independent; repeated/self inheritance requires no additional owners. An
import observer checks the entire target before every prospective clone.

Three primitive nullable positions and all nine combinations of cached/new/reload
targets and clone positions reject without partial fields, stranded handles or
owner/token leaks. Existing complete structures and string contents survive;
new failed targets physically free. Every case retries successfully. Public
getters return inherited values; final target/default/libvar cleanup returns all
physical owners/token counts to zero. A missing custom file still returns the
native cached default.

Clang normal/release fast-math ASan/UBSan, optimized GCC, parent real interpolation
sanitizers, five ledger/manifest checks, Bash syntax and diff checks pass. GCC and
Clang CI run both configurations. Host whole-preprocessor compilation retains
two unrelated existing `abs(long)` expression warnings. Both Retro68 products
build with zero compiler diagnostics and valid PPC PEFs:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,766,543 | `162afe1873c4df680e7131d3213cb1faeca6da1e2330ddb8fa8e027bbdff6e4a` |
| Quake3_TeamArena | 3,915,117 | `c970739ccad7446225b4fc375d118beee47031455d49a99886dad3eaa21b0d45` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).
Source includes the checked interpolation/registry/source/character/parser/
libvar/routing parents.

## Remaining acceptance

Keep #48 open for character numeric/skill bounds, remaining directive/global/
indent/unread-token factories and libvar consumers, expression/numeric and lexical
cursors, more real nested/malformed sources and aggregate parse/recursion budgets.
Retail/Mac OS 9 execution remains deferred.
