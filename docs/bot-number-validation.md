# Native numeric token conversion — 2026-09-18

Unchecked integer accumulation wraps oversized literals into small values,
including valid-looking characteristic indexes. Fraction divisors wrap, trailing
dots process the NUL as a digit, and float auxiliary integers use unchecked casts.
Malformed multiple dots and empty base prefixes can produce successful numbers.
The uppercase hexadecimal scanner accepts only `A` from `A`–`F`.

Check each integer digit against native `ULONG_MAX` before multiplication/addition.
Reject conversion failures through the lexer with zero numeric outputs. Retain
finite floating values and saturate only unrepresentable auxiliary unsigned
integers. Check whole float expansion before arithmetic; preserve fraction
division until its divisor cannot safely grow, then use a decreasing weight.
Consume decimal points without processing the terminator. Reject multiple points
and empty base prefixes; restore complete uppercase hex scanning.

Native decimal/hex/octal/binary spellings, long/unsigned suffixes, punctuation
signs, metadata and existing token capacities remain. No arbitrary literal cap
replaces representability. [ioquake3's lexer](https://github.com/ioquake/ioq3/blob/master/code/botlib/l_script.c)
provides a primary grammar reference. Commercial 1.32c asset, parser/QVM/syscall
and structure layouts are unchanged. Consumers still need their own numeric
bounds; safe token conversion does not prove safe narrowing/interpolation.

## Validation

The runner links actual character/preprocessor/lexer/libvar/Q_shared bodies;
file/heap/printing are interfaces. Token scanning/conversion, real character
index parsing and physical owner cleanup execute production bodies.

Eleven original actual-body proofs fail: trailing dot, wrapped fractional divisor,
integer overflow, multiple decimal points, partial uppercase hex, oversized float
auxiliary cast, wrapped character indexes in all four bases and whole-float
overflow. The fixture declares the original converter's actual void signature
for original-body builds. Eighteen ordinary native goldens pass original and
fixed bodies, including bases, suffix combinations and floating values.

Every integer base accepts native `ULONG_MAX` and rejects overflow without partial
numeric values or losing the following token. Long leading zeros retain native
decimal/octal and hex/binary capacities; the next byte rejects. Trailing dots,
uppercase hex, empty prefixes, malformed points, punctuation signs and maximum
fractions exercise real token reads. Finite floats beyond unsigned range retain
their floating value with bounded auxiliary integers. Independent `strtold`
references check fractions and large finite floats.

Direct converter inputs exceed the host divisor/long-double range, exercising
safe tiny-fraction weights/underflow and whole-float rejection independently of
the lexer's smaller token capacity. Actual character files first own a string,
then reject oversized literals that previously wrapped into assignable indexes.
All prior character/string/source/table owners physically release and token
counters return to zero. These fixtures are host evidence, not target execution.

Clang normal/release fast-math ASan/UBSan, optimized GCC, parent real character,
source and default sanitizers, five ledger/manifest checks, Bash and diff checks
pass. GCC/Clang CI runs both configurations. Host whole-preprocessor compilation
retains two existing unrelated `abs(long)` expression warnings.
Both Retro68 products rebuild with zero compiler diagnostics and valid PPC PEFs:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,766,565 | `188379c0930efab366923ea6f522e7f4158c2ce5043bc4241bce0ef136d9ceb9` |
| Quake3_TeamArena | 3,915,139 | `a278fc81954d3ffc763da4040feab01bb8b10071a1b5cad12235b487a0bdaad4` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).
Source includes the checked default/interpolation/registry/source/character/
parser/libvar/routing parents.

## Remaining acceptance

Keep #48 open for character/public numeric/skill bounds, expression arithmetic,
remaining directive/global/indent/unread-token factories and libvar consumers,
lexical cursors, more real nested/malformed sources and aggregate parse/recursion
budgets. Retail/Mac OS 9 execution remains deferred.
