# File block-comment validation — 2026-09-18

COM_Compress removes an unterminated block-comment suffix before the bot lexer can
report it. File/source/include callers can accept a truncated prefix, and a complete
selected character can publish despite a malformed file. Raw memory lexer checks
do not cover this path.

Validate block-comment closure before compression, matching native compression double-quote
and line-comment rules. Compression does not interpret escapes or single quotes;
the scan deliberately uses those exact delimiters so erased comments cannot hide
behind different quote classification. Malformed files retain raw-line diagnostics, close the
file and free script/punctuation owners before returning failure. Valid files keep
the exact existing compression output and untouched lexer initialization state.
Commercial 1.32c public imports/syscalls/protocols and retail formats remain compatible.

## Validation

Six original actual-body proofs reproduce direct boundary/multiline file loads,
source creation, complete-prefix character publication and both include forms.
Two pre-review proofs additionally reproduce complete-prefix publication after
escaped double quotes and single quotes; both now reject with no retained owner.
The original 1,035 valid-byte/token corpus still passes. Fixed roots reject with
original raw line numbers and no physical retained owner.
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
| Quake3 | 3,779,493 | `61beca427d98366248ab83c17c3d203ea85aad2f9dacc5102c09dd0bbbc240b1` |
| Quake3_TeamArena | 3,928,067 | `fb9884484adde342b93aa90e3afaee44fc5fa28a4275a0ac5f2cd5202700362a` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #48 open for the stale compressed-file EOF pointer/conditional cleanup,
further native escaped-quote compression assessment, builtin/direct libvar and
other publication consumers, and aggregate work/recursion budgets. The existing
compressor's valid byte output remains; this step checks comment closure only.
Retail/Mac OS 9 execution remains deferred.
