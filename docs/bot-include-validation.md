# Native include path bounds and synchronization — 2026-09-18

Quoted fallback paths copy an unchecked prefix/filename into a native 64-byte
buffer. Angle includes also copy an unchecked prefix, silently skip overflowing
pieces and can look up a partial path. Incomplete angle includes can load a file;
a crossed-line quoted operand is consumed. Separator normalization uses
overlapping `strcpy`, empty prefix setup indexes before the array, and recursive
inclusion leaks a newly loaded script while returning success.

Append complete strings only when their contents and terminator fit. Oversized
quoted fallback paths reject before a second lookup. Angle overflow consumes
through the delimiter or unreads the next line, then rejects without a partial
lookup. Missing delimiters and empty filenames reject; crossed-line operands
are unread. Use `memmove` for overlapping separator removal. Prefix setup checks
its complete storage/separator costs before mutation and handles empty/aliased
inputs. Failed recursion frees the unpushed script and returns false.

Valid native direct/fallback lookup order, complete angle pieces, path separator
normalization and script publication remain intact. A successful quoted direct
lookup retains its original larger token capacity; the bounded fallback does
not impose a new limit on that first lookup. Native structure sizes, commercial
1.32c parser/QVM/syscall interfaces and asset layouts are unchanged.

## Validation

The fixture includes the complete actual native preprocessor. Include parsing,
token read/unread/copy/free, normalization, prefix setup and script publication
execute unchanged. Lexer tokens, file lookup, heap, script freeing and diagnostics
are interfaces; no directive, path or publication body is replaced.

Eight original actual-body proofs fail: quoted fallback and angle prefix buffer
overflows, silent angle-piece loss/partial lookup, incomplete angle acceptance,
crossed-line operand loss, overlapping separator copying, empty-prefix indexing,
and recursive script success/leak.

Literal direct/fallback and split-angle paths retain lookup order and published
script ownership. The last fitting 63-byte paths and a successful 127-byte
quoted direct path remain accepted. Oversized prefixes, single and cumulative
pieces reject without partial lookups; angle overflow consumes its delimiter.
Missing delimiters at EOF/next line, missing/crossed/invalid operands, empty
angle filenames and skipped directives retain synchronization and diagnostics.
The next-line token is read unchanged after rejection. Empty, ordinary, aliased,
last-fitting and overlong prefix updates check terminator/separator costs and
preserve prior state on failure. Repeated mixed separators normalize safely.
Recursive rejection frees the loaded owner; final cleanup physically releases
all script/token owners and returns the native token count to zero.

Clang normal/release fast-math ASan/UBSan and optimized GCC pass, including the
parent builtin regressions. GCC and Clang CI runners execute both sanitizer
configurations. Five ledger/manifest checks, Bash and diff checks pass. Whole
preprocessor host compilation retains the two existing unrelated `abs(long)`
expression warnings. Both Retro68 products rebuild with zero compiler diagnostics
and valid PPC PEFs:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,762,379 | `9838cee499e2ce6e2b766d597e58083fe8f81ce1461db848897ed69017a5ed42` |
| Quake3_TeamArena | 3,910,953 | `7c5498e2303121ee5d281f850affc85de7661773c5f806c913405503b4f9bbbd` |

The first local cross-build selected a temporary baseline `.c` proof copy through
the source glob and failed with duplicate definitions. Moving that copy out of
the `.c` glob and rerunning both products establishes the recorded clean build.
Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).
Source includes the checked builtin/libvar/routing parents.

## Remaining acceptance

Keep #48 open for token merging/stringizing, other nullable parser allocation
consumers, expression/character bounds, real nested/malformed lexer-source
coverage and aggregate parse/recursion budgets. Include interface fixtures do
not establish the entire native source-loader transaction. Retail/Mac OS 9
execution remains deferred.
