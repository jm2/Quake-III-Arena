# Native character file bounds and cleanup — 2026-09-18

Character filenames are copied without bounds and the native `botfiles/` prefix
can make file lookup truncate first. Index 80 is accepted although the public
API exposes only 0–79; string cleanup also stops before that hidden slot.
Nullable character/string imports are used without checks, and premature EOF
inside a selected skill block can publish a partial character. Real quote
stripping uses overlapping copies and indexes before empty text.

Check both the character filename field and complete native prefix/path costs
before lookup/cache/default fallback. The limit derives from existing storage.
Compare unsigned native indexes against 80 before narrowing or assignment.
Reject nullable character/string imports and premature EOF after releasing all
prior source/character/string owners. Quote stripping uses `memmove` and checks
nonempty length before removing a trailing delimiter.

Valid commercial 1.32c skill blocks, integer/float/string values, public index
79 and representable complete paths remain accepted. Parser/QVM/syscall layouts,
character storage, ownership and asset layouts are unchanged.

## Validation

The fixture includes the actual character body and links the complete actual
native preprocessor, lexer, libvars and Q_shared core. File, heap and printing
are imports. Character parsing, path/index checks, tokenization, source freeing,
string ownership, public characteristic lookup and handle cleanup are actual
production bodies, with no character/parser/source/cleanup logic replaced.

Ten original actual-body proofs fail: unsafe long filename acceptance, hidden
index 80, nullable character and later string use, incomplete selected-skill
publication, full-path truncation/default fallback, both quote-copy overlaps,
and both empty-text quote underreads. Character proofs use the corrected quote
helper so its unrelated overlap does not mask those character failures; quote
proofs execute the original lexer helper directly.

Real skill selection skips a valid earlier block and retains literal integer 42,
float 1.25 and string `native` at indexes 0/1/79. Public getters retain those
values; index 80 remains inaccessible. Both ordinary and last-fitting complete
prefix/path names load and physically release through native handle cleanup.
Nine long-path cases span first overflow, field overflow and maximum token text
at primitive/cached/public entry points; each rejects before VFS/import/default
loading. Indexes 80/81/INT_MAX/UINT32_MAX reject and free an earlier string.

Failure injection identifies actual character/string imports from the complete
parser allocation sequence, including intervening unread tokens. Nullable
character and either characteristic string release all prior physical owners,
close files and retry. Selected-block EOF, missing value, duplicate index,
invalid index/value syntax and null/empty filenames reject without partial
publication. Quote helpers retain native text and handle overlap, empty text and
unmatched delimiters. Final native token/physical owner counts return to zero.

Clang normal/release fast-math ASan/UBSan and optimized GCC pass. GCC and Clang CI
runners cover both configurations. Five ledger/manifest checks, Bash and diff
checks pass. Whole-preprocessor host compilation retains two existing unrelated
`abs(long)` expression warnings. Both Retro68 products rebuild with zero compiler
diagnostics and valid PPC PEFs:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,766,515 | `a4a050d5e7856577c7edc6e8890f63708eda167d77460ab635b163f4ec8527ee` |
| Quake3_TeamArena | 3,915,089 | `11fcef92d5839f2a946ca9093771723273c7e6ad6d5c6ed09fba1e8cdddc5d05` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).
Source includes the checked macro/include/builtin/libvar/routing parents.

## Remaining acceptance

Keep #48 open for default/interpolated character allocations and publication,
other source/define/indent/token allocation consumers, all source-loader I/O
costs/short reads, expression/numeric conversion bounds, real nested malformed
source coverage and aggregate parser/recursion budgets. Successful character
loads do not prove nullable failure recovery in every earlier source-loader
stage. Retail/Mac OS 9 execution remains deferred.
