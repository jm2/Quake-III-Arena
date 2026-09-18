# Native escape and literal bounds — 2026-09-18

Decimal/hex escapes overflow signed arithmetic before the existing byte clamp.
Unknown and empty numeric escapes can report an error but return success. The
separate legacy literal helper skips a closing quote twice after extra characters
and can advance beyond the terminating NUL after empty/unterminated input.

Check byte expansion before multiplication/addition, remember overflow, consume
the complete native numeric escape and emit one existing clamp warning. Invalid
escapes return false without changing the supplied output byte. Legacy literals
reject missing characters/closing quotes without advancing beyond the NUL;
extra characters retain their first-byte warning while consuming the closing
quote once. Punctuation checks remaining length before comparing a spelling.

Current token entry uses the string reader for single quotes; retain that reader,
complete quoted text and length subtype. Legacy-helper first-character subtype
is tested separately. Retain native decimal ASCII escapes and the full hex
letter mapping also present in [ioquake3](https://github.com/ioquake/ioq3/blob/master/code/botlib/l_script.c).
Commercial 1.32c parser/QVM/syscall/asset and structure layouts are unchanged.
The whitespace loops already stop at NUL and need no cursor change in this step.

## Validation

The runner links actual character/preprocessor/lexer/libvar/Q_shared bodies with
file/heap/printing interfaces. Quoted-token scanning, escape/literal helpers,
punctuation matching and physical script/table cleanup execute production bodies.

Eight original actual-body proofs fail: signed decimal multiplication/hex shift,
accepted unknown/empty hex escapes, two heap overreads after malformed legacy
literals, lost following token bytes and failed direct-escape output mutation.
Original native escape, quoted-token and punctuation goldens also pass the same
valid tests as the fixed bodies.

Eighteen ordinary escape spellings retain bytes through direct/double/single
quotes, including NUL/high bytes and decimal ASCII/hex extensions. Existing
oversized-byte warnings clamp once. Six-thousand-digit decimal/hex escapes clamp
before overflow and retain exact cursor/following-token behavior. Invalid active
and direct escapes reject; failed direct output bytes stay unchanged.

Separate legacy helper reads preserve valid/escaped/first-character values and
consume quotes once. Empty/unterminated/newline inputs reject, and repeated EOF
reads cannot cross the script NUL. Native single-quote whole text, concatenation
across comments/newlines, disabled escapes, existing maximum string payloads,
comment/whitespace EOF and final short/longest punctuation tokens remain valid.
Every fixture finally frees physical script/table owners and native token counts.

Clang normal/release fast-math ASan/UBSan, optimized GCC, parent actual number/
source sanitizers, five ledger/manifest checks, Bash syntax and diff checks pass.
GCC/Clang CI runs both configurations. Whole-preprocessor host compilation keeps
two existing unrelated `abs(long)` expression warnings.
Both Retro68 products rebuild with zero compiler diagnostics and valid PPC PEFs:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,766,599 | `a95465f8ec271546a02fc342a954c02a3196c2e6d6dac7bc5bd7c859c381483f` |
| Quake3_TeamArena | 3,915,173 | `352ac6763b5993b7244aeedbdc8f125e7c6e02343f66fa2ff560ed251899234d` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).
Source includes the checked number/default/interpolation/registry/source/
character/parser/libvar/routing parents.

## Remaining acceptance

Keep #48 open for distinguishing lexical errors from EOF through source/include
readers and every publication caller, character/public numeric/skill bounds,
expression arithmetic, remaining factories/libvar consumers, further real
nested/malformed sources and aggregate parse/recursion budgets. Retail/Mac OS 9
execution remains deferred.
