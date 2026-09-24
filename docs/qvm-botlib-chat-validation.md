# Botlib chat syscall validation — 2026-09-17

The thirteenth step for issue #35 checks the bot chat syscall family before
native dispatch. Console messages marshal through a native temporary into
the retail 276-byte QVM layout, clearing 32-bit links and excluding native pointer padding. A
no-message result preserves the output. Match structures, output capacities,
input strings, and nullable chat variables have complete VM range checks.
Initial and reply chat variables reach botlib whatever their combined size;
botlib leaves a variable that does not fit its fixed buffer unset (#306,
#340). NULL optional variables and
substring queries preserve existing native behavior.

Selected match variables must lie within their terminated embedded string.
Negative lengths, excessive spans, and invalid variable indices fault the
module before native dispatch. Native substring output now uses bounded
`memmove`, including overlapping output, and rejects invalid embedded spans.
Native match-string copying always terminates at the fixed buffer boundary.

The user selected a safe in-place limit for the size-less retail synonym
syscall. For QVMs the VM wrapper passes the original terminated string length
to the exported replacement routine, so longer replacements may be skipped; it
checks that original span rather than assuming an additional 256 writable
bytes. Interior pointers and short objects at the VM boundary retain their
surrounding bytes. Syscall opcodes and retail QVM arguments remain unchanged;
native game modules pass their remaining message capacity as a size (#245).

Private helpers receive the capacity of internal chat buffers explicitly, so
initial/reply variable expansion and weighted generated text may still grow
within their known 256-byte buffers. The word search keeps the 1.32 word
boundaries and stops at the terminator; skipping a synonym inside an existing
replacement stops where 1.32 would resume past the terminator. These native
fixes also cover part of #48.

## Validation

Two ASan/UBSan fixtures execute the actual VM dispatcher and native text
routines with exact-sized allocations. They cover exact QVM console output
and adjacent object canaries on a 64-bit host, scalar/message layout, cleared links, unchanged no-message output,
eight nullable variables, variables past the combined size, reply message limits,
unterminated strings, match metadata rejection, growing/shrinking/empty
synonym replacements, exact known expansion capacity, exact-sized/interior
exported objects, near-end and empty VM strings, unchanged object canaries,
retained internal initial/reply variable growth, trailing delimiters,
termination of long native match input, and overlapping substring output.
Rejected VM requests preserve the image and never call the native API.

The new runner, affected server-core/navigation runners, and all eight Python
checks pass. Both Retro68 products build without compiler diagnostics and pass
PEF validation with the temporary libraries in the
[loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,686,413 | `d19a74548b69ca3625fc72b490b8bfc2cfff27523220f8d356f06d7161511566` |
| Quake3_TeamArena | 3,834,987 | `bb116f8d48173a46269ba179db0df142a57f0fb5b862cd0594a3ad794623b857` |

## Remaining acceptance

Keep #35 and #48 open. Botlib elementary-action and remaining AI syscall
families, scalar indices, indirect native accesses, and bot file/parser
bounds remain under review. Native initial/reply concatenation bounds itself
(#306); the broader native bot-data audit is unfinished.
Retail 1.32c and Mac OS 9 live acceptance remain deferred.
