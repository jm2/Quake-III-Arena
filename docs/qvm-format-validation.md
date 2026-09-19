# QVM and static-module format validation — 2026-09-18

The shipping game, cgame and UI sources still routed variable text through the
QVM libc's unbounded `vsprintf`, including fixed 1,024-byte diagnostics and
4,096-byte parser messages. The shared `Com_sprintf` path first formatted into
a 32,000-byte QVM stack buffer and only bounded the later copy. A long format
could overwrite either destination before any size check ran.

This change ports ioquake3's bounded QVM formatter from
[`bb47026b`](https://github.com/ioquake/ioq3/commit/bb47026b5f38a4f1478384ac92d07c07d1a4b83e)
and its C99 return/termination correction from
[`476e35f5`](https://github.com/ioquake/ioq3/commit/476e35f50ec42ee085e06459b04af2d7141adf39).
The formatter receives every destination capacity, keeps the prefix that fits,
always terminates a nonempty destination, and returns the complete length that
would have been written. Its signed-integer conversion first forms the
magnitude as unsigned arithmetic, so `INT_MIN` does not invoke signed overflow.
The shipping game/cgame/UI format sinks now pass their real capacities and
force their final byte to NUL. The obsolete 32 KiB intermediate buffer is gone.

These are internal libc and call-site changes. They do not alter QVM opcodes,
VM syscall numbers or arguments, shared structures, file formats, network
bytes, or the commercial 1.32c game interface. Existing native builds continue
to use their platform `vsnprintf`; the QVM declaration selects the local
implementation.

## Host validation

`tests/qvm_format_regression.c` compiles the actual formatter body and surrounds
an eight-byte output with redzones. It checks zero capacity without a write,
`NULL` plus zero-capacity sizing, one-byte termination, exact-capacity output,
truncation prefix and complete return length, integer/string/width/precision,
character, percent and floating formats, and the `INT_MIN`/`UINT_MAX` limits.
A source contract rejects unbounded formatting in every shipping static/QVM
source and verifies each new capacity and explicit terminator.

GCC and Clang pass the actual-body suite under ASan/UBSan in normal and
optimized fast-math modes. Both compilers also pass the complete host C suite.
The exact parent `bg_lib.c` actual `AddString` body writes a 32-byte string into
an eight-byte guarded output and fails with an ASan stack-buffer-overflow; the
bounded candidate passes the corresponding truncation proof.

## QVM link validation

The formatter changes were applied over the complete retail source recipes in
the pending QVM source-build step and compiled with q3lcc. All six q3asm links
completed with zero assembler errors and emitted the expected `0x12721444` QVM
header.

| Module | Bytes | SHA-256 |
| --- | ---: | --- |
| `cgame.qvm` | 332,380 | `5c6324c5efd0cae3f0e33578edbf3567d6a5ce8fab37f253f33d12ff1533d8e9` |
| `cgame_ta.qvm` | 496,604 | `eb0fe79f231768cc958c134f861357e1dccd9ceb81f9c647d9a03fe8239781cc` |
| `game.qvm` | 477,376 | `9b4d73f14012b7bece35779a6b0fe8214e4d8de37c5b05e44c92650c2fb12ac7` |
| `game_ta.qvm` | 559,356 | `838914ddde94bcf52c49f8673e7d427e524a8995efbf3246029cadbc559ccac8` |
| `q3_ui.qvm` | 282,004 | `22628650ca9721dca7cb4cfd3ff666c8600a9217f629dbbc21c18955ae2bbe05` |
| `ui.qvm` | 291,612 | `509d8024ebeedfe9933f7d40c3d48f06abd8d0564f847a9f66569546725b9f8c` |

The legacy compiler emitted only the source recipe's pre-existing empty-
declaration and function-pointer warnings; the formatter added no compile or
link error. Both PPC products then built without compiler diagnostics and have
valid `Joy!peffpwpc` headers.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,784,017 | `345896ab29f5a1f25eb844977ae298e2f9f8e8b1eb3e0fb8f6de472f55d73d6b` |
| Quake3_TeamArena | 3,932,591 | `627df4ee4942bb55d79a6ae7c5b659af096045bff7246009ee58e30b55d0c08b` |

## Remaining acceptance

Win32/Unix platform entry points and the non-shipping BSPC tools retain
historical `vsprintf` sinks and need a separate capacity audit. Issue #35 also
retains its remaining syscall pointer/range work. Retail QVM execution and
Mac OS 9 acceptance remain deferred until the user provides the legal assets
and target environment.
