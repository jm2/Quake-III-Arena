# File block-comment validation — 2026-09-18

COM_Compress removes an unterminated block-comment suffix before the bot lexer can
report it. File/source/include callers can accept a truncated prefix, and a complete
selected character can publish despite a malformed file. Raw memory lexer checks
do not cover this path.

Validate block-comment closure before compression, respecting native quoted text,
escapes and line comments. Malformed files retain raw-line diagnostics, close the
file and free script/punctuation owners before returning failure. Valid files keep
the exact existing compression output and untouched lexer initialization state.
Commercial 1.32c public imports/syscalls/protocols and retail formats remain compatible.

## Validation

Six original actual-body proofs reproduce direct boundary/multiline file loads,
source creation, complete-prefix character publication and both include forms.
Fixed roots reject with original raw line numbers and no physical retained owner.
Quoted includes retain their native second fallback attempt; angle includes try
once. Both reject malformed children, preserve parent source status and recover to
the synchronized next token. Raw memory lexer diagnostics remain sticky and native
character values/ownership remain.

The 1,035 original valid-file comparisons pass separately: eleven quote/escape/
line-comment/block cases plus 256 block lengths by four newline counts. Compressed
bytes/lengths and initialization flags/positions/lines match native compression;
actual file and compressed-memory lexers match every token type/subtype/text/value/
line field. Readers reach the compressed NUL with no new diagnostic. Every
file/memory/source/character teardown physically releases all heap/token owners.

Clang ASan/UBSan and optimized GCC pass normal/release fast-math modes. Parent
source/source-error/include/weight/synonym fixtures pass both compilers/modes.
Five ledger/manifest checks, Bash syntax and diff checks pass; GCC/Clang CI runs the
fixture. Both PPC products build with zero diagnostics and valid PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,779,493 | `cffe2f8a7dc865df1c6dc88b9365d472f3559f8e351a7c0f82900eab07f71597` |
| Quake3_TeamArena | 3,928,067 | `0d00b387bffa88a0a9d8e017b2e17ae70bdb886ce7f434242408866b5e4e973c` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #48 open for the stale compressed-file EOF pointer/conditional cleanup,
further native escaped-quote compression assessment, builtin/direct libvar and
other publication consumers, and aggregate work/recursion budgets. The existing
compressor's valid byte output remains; this step checks comment closure only.
Retail/Mac OS 9 execution remains deferred.
