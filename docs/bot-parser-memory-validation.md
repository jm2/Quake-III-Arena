# Aggregate bot parser memory budgets — 2026-09-19

The native preprocessor previously bounded work performed by one public token
read and the number of active includes, but it did not bound memory retained by
many source handles, persistent definitions, queued replacement tokens, raw
scripts, or conditional frames. A caller could therefore stay below every
individual allocation and still grow native parser memory without a common
ceiling.

Each raw script now has an 8 MiB ceiling that includes its header, input buffer,
and 256-entry punctuation index. The preprocessor reserves from one 16 MiB
budget before publishing any source header, definition hash, script or include,
definition, copied token, or conditional frame. Reservations use checked
subtraction, and every normal, malformed-input, nullable-allocation, EOF, and
shutdown path releases the exact recorded cost. Concurrent source handles and
global definitions share the same budget. Conditional nesting stops at 128
frames, alongside the existing 64-file include depth, 128-call string recursion,
and 4,096-unit per-read work limits.

Budget exhaustion follows the existing parser failure path and leaves previously
published dictionaries and source owners usable. Native allocator failures keep
their historical diagnostics. The change adds only private bookkeeping fields;
public parser handles, tokens, QVM syscall signatures, protocol data, and
commercial 1.32c script syntax remain unchanged.

## Validation

The focused actual-body regression compiles the production preprocessor, lexer,
libvar, character parser, allocator interface, and Q_shared core with reduced
32 KiB per-script and 64 KiB aggregate test ceilings so every boundary is
reachable under ASan/UBSan.

It proves the last-fitting raw memory script and first rejected byte, plus an
oversized file that closes before allocation or read. Concurrent empty sources
fill the aggregate budget and reject the next source without leaking its
detached script; complete cleanup restores allocation. Copied token owners stop
exactly at the remaining byte count without invoking the fatal allocator path.
The 129th conditional rejects with all 128 prior frames unchanged. An attached
include reserves its complete recorded script cost and releases it at EOF.

Source-local and global definitions then fill the shared budget. Their failing
candidate leaves every prior header/token owner and byte count unchanged;
source or registry cleanup returns the counter to zero and immediate retries
succeed. GCC and Clang run normal and optimized fast-math sanitizer modes. The
existing source, source-error, conditional-factory, definition, include, and
preprocessor-work suites pass both compilers. Every host runner that links
`l_precomp.c` also passes its GCC sanitizer modes. Five ledger/manifest checks,
Bash syntax, and diff checks pass. Both Retro68 products build with no compiler
diagnostics and retain valid PowerPC PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,805,089 | `0af21c51eb2e3182f17d83c3b2f61640c04322387b49a639063b0083fcb79b2a` |
| Quake3_TeamArena | 3,953,663 | `1b03a5ee0d10d5a5d0b39dd257bb3a9450c79303f438a8934b63a04209c3843f` |

## Remaining acceptance

This completes the aggregate parser-memory item for #48. The parent issue stays
open until its stacked implementation PRs merge through the required exact-head
CI and review gates. Retail data and Mac OS 9 runtime acceptance remain deferred
under the user-approved follow-up policy.
