# Native preprocessor builtin ownership — 2026-09-18

Both date/time macros free the storage returned by `ctime()`, which belongs to
the runtime, and dereference a failed conversion. Empty/unsupported builtin
expansion also leaks its copied token. A returned failed token copy is used
without a guard, although the native fatal error normally terminates first.

Borrow time storage without freeing or modifying it. A failed conversion releases
the copied token, reports a native source error and returns false with no output.
Empty/unsupported expansion keeps its existing successful empty result while
releasing the unused copy. A returned null copy preserves the native fatal
diagnostic and returns false without publishing a token. Successful token text,
types/subtypes, numeric metadata, whitespace and source location retain native
behavior. Commercial 1.32c parser/QVM/syscall interfaces are unchanged.

## Validation

The fixture includes the complete actual native preprocessor. Token copy/free,
builtin formatting, ownership and diagnostics execute unchanged; clock, native
heap, printing and unused script interfaces are the fixture boundary. No builtin
or token-ownership body is replaced.

Six original actual-body proofs fail: both macros attempt to free borrowed static
time storage, both failed conversions perform null pointer arithmetic, empty
expansion retains a copied token, and a returned null copy is dereferenced.

Literal date/time expansions run three times against the same borrowed buffer
and preserve its text. Line/file values and the maximum valid 1,023-byte filename
retain native token values. Source tokens remain unchanged, and copied location,
whitespace and numeric metadata retain their previous values. Failed date/time
conversion frees its owner and retries; empty/unknown results release their copy.
Each of five native builtin kinds rejects a returned null copy with its original
fatal diagnostic and no published token. Every successful published token is
physically freed and the native token count returns to zero.

Clang normal/release fast-math ASan/UBSan and optimized GCC pass. GCC and Clang CI
runners cover both sanitizer modes. Including the whole preprocessor exposes two
existing host warnings about `abs(long)` in unrelated expression directives;
that expression work remains open. Both Retro68 products rebuild with zero
compiler diagnostics and valid PPC PEFs:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,762,345 | `a2bbec590f2ec868ef0360168da04fa67fa7d1b0025a85d3527c93559b1170c9` |
| Quake3_TeamArena | 3,910,919 | `1109b85441d8b45aeefd00de97bb4ff2a8ac177555ffd16945e465983b2be03a` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).
Source includes the checked libvar and routing parents. Review-ledger, Bash and
diff checks pass.

## Remaining acceptance

Keep #48 open for other parser allocation consumers, token merging/stringizing,
include/path bounds and synchronization, expression/character bounds, nested
parser malformed-input coverage and aggregate work/recursion budgets. This
checked helper does not establish allocation failure handling in every caller.
Retail/Mac OS 9 execution remains deferred.
