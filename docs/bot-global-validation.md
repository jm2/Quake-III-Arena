# Native global definition deletion — 2026-09-18

Individual global deletion frees the selected definition without unlinking it
from the native registry. Later source creation copies freed name/token owners;
registry cleanup can traverse or free the same node again.

Walk the native registry through the pointer that links each node. Unlink the
first matching definition before releasing its name/body/parameter owners.
Missing and null names return false without mutation. Preserve native
case-sensitive matching, first-match duplicate behavior and the public API.
Existing sources retain their independent copies. Commercial 1.32c macro text,
asset, parser/QVM/syscall and structure layouts are unchanged.

## Validation

The runner links complete actual character/preprocessor/lexer/libvar/Q_shared
bodies and reuses real file/heap/printing interfaces. Native global creation,
registry deletion, source copying, macro expansion and physical cleanup execute
production bodies. The shared source fixture has a configurable entry name so
these tests reuse its interfaces without duplicate main definitions.

Five original actual-body proofs fail: head, middle and tail removal each expose
heap use after free during subsequent source creation; null-name lookup invokes
an invalid string comparison; duplicate removal fails to unlink the first match.

A real three-definition registry contains scalar and parameterized macros.
Each deletion position preserves the remaining chain and requires no new
allocation. A source created before deletion still expands every copied macro;
a later source expands only remaining originals. Physical name/body/parameter
owners and native token counters drop by exactly the selected definition, survive
source cleanup and finally return to zero. Missing, case-distinct and null names
leave owners and registry untouched. Duplicate deletion exposes the earlier
definition, which remains usable through real source expansion; removing its
last original and repeated missing removal retain native results.

Clang normal/release fast-math ASan/UBSan, optimized GCC and parent actual source
sanitizers pass. GCC/Clang CI runs both configurations. Whole-preprocessor host
compilation retains two existing unrelated `abs(long)` expression warnings.
Five ledger/manifest checks, Bash syntax and diff checks pass. Both Retro68
products rebuild with zero compiler diagnostics and valid PPC PEFs:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,766,509 | `2f0bb3ba1f95586e5615aee5f739d9719fcf402954b371d914eafbb1ffbaf789` |
| Quake3_TeamArena | 3,915,083 | `8d63b761d54a5f9d1d3f2f5bb80501fcbc743c4cc69f59939d5808492d6ebdc0` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).
Source includes the checked source/character/macro/include/builtin/libvar/routing
parents.

## Remaining acceptance

Keep #48 open for remaining directive/global/indent/unread-token factories,
default/interpolated character owners, expression/numeric and lexical cursor
bounds, further real nested/malformed sources and aggregate parse/recursion
budgets. Retail/Mac OS 9 execution remains deferred.
