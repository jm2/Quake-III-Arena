# Builtin definition dictionary publication — 2026-09-19

`PC_AddBuiltinDefines` dereferenced every allocator result immediately. Failure
while creating `__LINE__`, `__FILE__`, `__DATE__` or `__TIME__` therefore reached
a null write; later failures also left the earlier builtin owners published as
an incomplete dictionary.

The existing void setup interface now stages all four fixed definitions in
private checked owners. Any failed allocation releases every staged definition,
records a source error and leaves the live dictionary unchanged. Publication
starts only after the complete set exists and retains the original insertion
order, names, builtin values, fixed flags and empty bodies. Parser structures,
source handles, botlib imports, QVM syscalls, protocol defaults and commercial
1.32c data layouts remain unchanged.

## Validation

The actual source, script, lexer, preprocessor and allocator bodies inject a
null result at each of the four builtin allocation positions. Every position
leaves all four names absent, preserves the complete source owner set, records
the failure and physically releases all staged definitions. Retrying on the same
source publishes all four native owners and metadata together. A failure-free
setup establishes the same dictionary, and source cleanup releases it completely.

The broader source suite retains file/memory loading, path and signed-size
boundaries, short-read rollback, punctuation indexing, global-definition copy
transactions and real macro expansion. GCC and Clang ASan/UBSan pass normal and
optimized fast-math modes. Both Retro68 products rebuild without compiler
diagnostics and have valid PPC PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,805,049 | `b477ffd539d7013b1d23eeee92a73224e0968e858bdf0c20dec304d5f5e89a9a` |
| Quake3_TeamArena | 3,953,623 | `a77ad7d5fe52b3b7d0241c3467d3bc50a35be8306fac14628fad0d88120830b5` |

## Remaining acceptance

Keep #48 open for aggregate parser memory budgets. Retail/Mac OS 9 execution is
deferred.
