# Preprocessor work and recursion budgets — 2026-09-19

The native bot preprocessor could expand mutually recursive macros forever,
accumulate queued replacement tokens without a limit, copy arbitrarily wide
definitions before returning one token, and recurse once per macro-generated
adjacent string. Expressions use the same expansion path, so a hostile bot,
character, item, weapon, chat, or map script could consume unbounded CPU, heap,
or C stack before a caller received a token.

Each public token read and direct expression collection now shares one private
4,096-unit budget across raw token reads and replacement-token copies. Nested
string lookahead shares that budget and stops at 128 calls. A limit failure marks
the source, releases every queued replacement and private candidate, and returns
failure through the existing parser interfaces. The budget resets only after a
token is published or a direct expression call returns, so ordinary source size
is unchanged. External definitions use the same bounded transaction. Public
tokens, source handles, botlib imports, QVM syscalls, and commercial 1.32c data
and protocol layouts remain unchanged.

## Validation

The actual preprocessor, lexer, libvar, character, allocator, and diagnostic
bodies cover mutually recursive object macros through both public token reading
and direct expression collection. Replacement tails accumulate behind the cycle,
then the budget failure releases the entire queue while preserving the original
definition owners and initializing expression outputs. In-file and external
5,000-token definitions reject before dictionary publication and release every
candidate name/body/source owner. A 160-element macro-generated string chain
stops before native stack exhaustion and retains its definition owner.

Native goldens concatenate sixteen macro-generated strings exactly and read a
5,000-token ordinary source in order, proving that successful publication resets
the budget rather than limiting whole-file size. GCC and Clang ASan/UBSan pass
normal and optimized fast-math modes. Existing expression collection/arithmetic,
source, definition, empty-expansion, conditional-factory, and error propagation
fixtures pass both compilers. Five ledger/manifest checks, Bash syntax, and diff
checks pass.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,805,021 | `f578e278b0dcd219b2dd5a395005bf0e7a6b43ae2574654ee349673bba1a6879` |
| Quake3_TeamArena | 3,953,595 | `2f311ab0f82fe44b8f1b0584d0efc6848ef577b4bd8061a69ba670bbc55aa423` |

## Remaining acceptance

Keep #48 open for remaining character/source allocation consumers, bounded
include depth, and aggregate parser memory budgets. Retail/Mac OS 9 execution is
deferred.
