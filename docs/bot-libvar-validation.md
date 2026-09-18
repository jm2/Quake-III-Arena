# Bot numeric libvar bounds and grammar — 2026-09-18

The native decimal converter advances past a dot and consumes the next byte
without checking for NUL or a digit. A trailing dot reads beyond an exact string;
invalid first fractional text can become a numeric value. Its signed integer
fractional divisor overflows after nine digits, and long integer text produces
infinity that downstream native integer consumers cannot represent.

Consume each byte once, retain the existing unsigned decimal grammar and invalid
text's zero result, and allow leading/trailing decimal points safely. Use a
bounded double fractional divisor and preserve the native per-digit float value
accumulation for representable integers. Reject an integer step outside float
capacity before narrowing; very long fractional tails remain safe without an
arbitrary string-length cap. Finite valid libvar values, backend ownership and
commercial 1.32c interfaces stay intact. Signs, exponents and nonnumeric special
values retain their prior invalid/zero result.

## Validation

Four original actual-body proofs fail: exact trailing-dot heap-string overread,
signed fractional-divisor multiplication overflow, an invalid first fractional
byte accepted as a number, and an overflowing integer accepted as infinity.
Seventeen independent literal native-value goldens also pass against the original
source. They include defaults, integer thresholds, leading fractions, ordinary
short decimals and zero padding.

The fixed fixture includes the complete actual converter and variable backend.
Forty backend cases retain exact name/text/value caching, case-insensitive lookup,
setter replacement and physical release of both name/string allocations. Two
direct checks exercise an exact trailing-dot string and ten fractional digits.
A thousand-digit fractional tail, malformed late fractional bytes and huge
integer text stay safe. No production parse/ownership logic is replaced.

Clang normal/release fast-math ASan/UBSan and optimized GCC pass. CI runs the
fixture with GCC and Clang. Review-ledger, Bash and diff checks pass. Both Retro68
products rebuild with zero compiler diagnostics and valid PPC PEFs:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,762,317 | `33e24f83e09eb2ce6311295436daf7eef21d674b0ef0054857241183b2658918` |
| Quake3_TeamArena | 3,910,891 | `094df357a4b6acc137f4e10d05d7938be07324a4d503925157f9a6f80f0957fb` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).
Source includes the checked native routing cache reader/initialization parents.

## Remaining acceptance

Keep #48 open for token merge/stringize, include and character paths/indices,
static date/time storage ownership, every parser allocation caller, aliases and
aggregate parse work/recursion budgets. Finite numeric values can still exceed
individual integer consumer ranges; those callers require their own bounds.
Retail/Mac OS 9 execution remains deferred.
