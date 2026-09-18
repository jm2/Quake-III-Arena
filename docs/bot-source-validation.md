# Native script/source owner transactions — 2026-09-18

Script loaders expand unchecked signed lengths, use nullable script/punctuation
allocations, truncate full native paths and ignore short file reads. Source
creation uses nullable header/dictionary owners and cannot unwind failed global
copies. Global definition copies dereference null name/token imports and can
leak earlier copies. Source names truncate unnecessarily. Signed high bytes are
classified as whitespace and can form invalid punctuation-table indexes.

Check complete signed script-buffer and name/path costs before imports. Every
failed file stage closes its handle and releases staged script/table owners;
short reads reject before source/dictionary allocation. File and memory source
creation share a checked constructor, which publishes only after base and copied
global owners complete. Copied definitions initialize both chains before cloning
and free partial chains on returned failure. Source names retain their native
1,024-byte field capacity and termination. Whitespace/table indexes use unsigned
bytes; unsupported high bytes outside strings reject with a native diagnostic.

Memory scripts already initialize punctuation tables; this step checks that
existing stage's failure. Valid commercial 1.32c file/memory tokens, real macros,
empty/anonymous sources and native path/name capacities remain accepted.
Structure, parser/QVM/syscall and asset layouts are unchanged. No arbitrary
script-size cap replaces the signed import representability check.

## Validation

The fixture links the complete actual native preprocessor, lexer, character/
libvar bodies and Q_shared core. File, heap and printing are interfaces; script/
source construction, punctuation, token reads, macro/global copying, dictionary
publication and physical cleanup execute actual production bodies.

Fourteen original actual-body proofs fail: file script/table/source null clearing,
null dictionary publication, ignored short read, oversized signed file cost,
nullable memory punctuation, truncated memory labels, swallowed high bytes,
nullable global name/body/parameter copies, truncated full native path and
negative memory copy size. Global proofs target clone stages explicitly so an
earlier base-owner fault cannot mask them.

Real ordinary and empty file/memory sources retain their four base owners,
literal name/number/string/punctuation tokens and EOF. Every base owner fails
independently with no partial owner/token publication and then retries; file
close/read order remains exact. Negative/overflowing costs reject before imports;
the last signed buffer cost remains representable and propagates a nullable
import. Short reads close/free script/table owners before dictionary allocation.

Maximum terminated memory labels, zero-byte anonymous/null-data sources and last
fitting bare/prefixed VFS paths remain valid. Overlong names, invalid pointers and
the next full path byte reject before import/lookup/truncation. Real memory macro
definitions and token pasting parse through EOF. Unsupported high-byte memory
tokens reject without a signed table index and physically release.

Two real global macros contain name/body/parameter owners. All twelve base/
global-copy allocation positions fail for both file and memory source creation,
unwind all prospective source owners and preserve original global owners/counters.
Only returned null token copies keep the native fatal diagnostic. Every failure
retries, executes real substituted global tokens and releases its copies; actual
global cleanup finally returns every physical owner/native token count to zero.

Clang normal/release fast-math ASan/UBSan and optimized GCC pass. Parent real
character sanitizers pass. GCC and Clang CI runners execute both configurations.
Five ledger/manifest checks, Bash and diff checks pass. Whole-preprocessor host
compilation retains two existing unrelated `abs(long)` expression warnings.
Both Retro68 products rebuild with zero compiler diagnostics and valid PPC PEFs:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,766,509 | `21988e783cce6010fc190d185ac0b37f54613b0798a3d9ac34ee2e3866b92b16` |
| Quake3_TeamArena | 3,915,083 | `4d1f6a3184ab3e2250b34a8d0c2918cc23d6e59993ac1015f69ded8cb6095018` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).
Source includes the checked character/macro/include/builtin/libvar/routing parents.

## Remaining acceptance

Keep #48 open for remaining directive/global/indent/unread-token consumers,
global deletion, default/interpolated character owners, expression/numeric and
lexical cursor bounds, more real nested/malformed source coverage and aggregate
parse/recursion budgets. Successful preparation of global definitions does not
prove every nullable stage in the directive factory. Retail/Mac OS 9 execution
remains deferred.
