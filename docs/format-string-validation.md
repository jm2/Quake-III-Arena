# Native format-string validation — 2026-09-18

The authoritative ioquake3 format-string fixes in
[`59c231c6`](https://github.com/ioquake/ioq3/commit/59c231c6c6ee9c460a252aea74a8aa1b84da4e1a)
and
[`8ca8d845`](https://github.com/ioquake/ioq3/commit/8ca8d845911fb6545bf723cade39944d874d01ea)
are present in the local call sites. They pass AAS diagnostics, script paths,
server disconnect reasons, authorization requests and responses, master-server
queries, download errors, bot node text and UI model names through literal
formats. The shared out-of-band packet formatter is bounded and terminated.

The same audit found that native `Com_Error` still used `vsprintf` to write a
fixed `MAXPRINTMSG` buffer. Format it with `Q_vsnprintf` and force the last byte
to NUL, matching the bounded native behavior used by current ioquake3. This
changes no public structure, syscall, QVM import, file format or network byte.
Commercial 1.32c messages keep their existing syntax; an oversized local error
message now truncates to the fixed engine buffer instead of overwriting memory.

## Validation

Four fixtures compile the actual `Com_Error`, `AAS_Error`, `PS_SetBaseFolder`
and `NET_OutOfBandPrint` bodies. Hostile `%n`, `%08x`, `%s` and `%%` text remains
data. The error fixture covers a short message, exactly `MAXPRINTMSG - 1`, one
byte over capacity and twice capacity through the real abort-frame path. The
other fixtures cover the 1,024-byte AAS buffer, bot base-folder capacity and
the complete `MAX_MSGLEN` out-of-band header/payload boundary.

The source contract tokenizes C call expressions, ignores comments and checks
that every audited printf-style sink has a literal format argument. It also
requires the exact safe calls from both upstream commits, so alternate spacing,
nested-call spelling or a nearby comment cannot hide a nonliteral format. GCC
and Clang run all four actual-body fixtures under ASan/UBSan in normal and
optimized fast-math modes. The exact parent `common.c` fails the twice-capacity
proof with an ASan global-buffer-overflow; the bounded source passes the
complete matrix. Bash and Python syntax, five ledger/manifest tests and the
whitespace/error diff checks pass locally.

Both PPC products build without compiler diagnostics and have valid
`Joy!peffpwpc` headers.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,784,017 | `8ae0a01406383248e7b922c0f47a8de82fa21efcb74bbede77247d7a186a651b` |
| Quake3_TeamArena | 3,932,591 | `a0e9b64c8563fd1b9cd4e938b64b16ad36607e4414e2ff656a66800d4e2a9dd8` |

## Remaining acceptance

The QVM libc still maps `Q_vsnprintf` to its historical unbounded `vsprintf`.
Keep the broader QVM and security-provenance work open for a bounded QVM
formatter and the remaining formatter audit. Retail-asset and Mac OS 9 live
execution remain deferred until the user provides that environment.
