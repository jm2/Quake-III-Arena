# Botlib chat syscall validation — 2026-09-17

The thirteenth step for issue #35 checks the bot chat syscall family before
native dispatch. Console-message and match structures, output capacities,
input strings, and nullable chat variables have complete VM range checks.
Initial and reply chat variables must fit the fixed native buffer together;
reply messages count toward that capacity. NULL optional variables and
substring queries preserve existing native behavior.

Selected match variables must lie within their terminated embedded string.
Negative lengths, excessive spans, and invalid variable indices fault the
module before native dispatch. Native substring output now uses bounded
`memmove`, including overlapping output, and rejects invalid embedded spans.
Native match-string copying always terminates at the fixed buffer boundary.

Synonym replacement checks the full 256-byte VM output capacity and bounds
native expansion, including reply synonyms. The native word search now finds
later words without stepping past the terminator. Skipping a synonym inside
an existing replacement advances within the source rather than beyond it.
These native changes also address part of the bot text bounds work in #48.

## Validation

Two ASan/UBSan fixtures execute the actual VM dispatcher and native text
routines with exact-sized allocations. They cover complete console output,
eight nullable variables, combined-size limits, reply message limits,
unterminated strings, match metadata rejection, growing/shrinking/empty
synonym replacements, exact expansion capacity, trailing delimiters,
termination of long native match input, and overlapping substring output.
Rejected VM requests preserve the image and never call the native API.

The new runner, affected server-core/navigation runners, and all eight Python
checks pass. Both Retro68 products build without compiler diagnostics and pass
PEF validation with the temporary libraries in the
[loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,686,413 | `e546993cc76253a4e35c7018ce0dd946446e940d6e7286b2eead51f78064d2b3` |
| Quake3_TeamArena | 3,834,987 | `21793eeff750e04d36909e2cc9445f56ed78d4c22bcc9e44a391bbe896d949f7` |

## Remaining acceptance

Keep #35 and #48 open. Botlib elementary-action and remaining AI syscall
families, scalar indices, indirect native accesses, and bot file/parser
bounds remain under review. Native initial/reply concatenation is protected
by syscall capacity checks; the broader native bot-data audit is unfinished.
Retail 1.32c and Mac OS 9 live acceptance remain deferred.
