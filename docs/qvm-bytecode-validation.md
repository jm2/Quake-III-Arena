# QVM bytecode preparation validation — 2026-09-17

The second implementation step for issue #35 validates the interpreter's
instruction stream before allocating decoded code or modifying its instruction
table. Every declared opcode and operand must fit in the code section;
conditional branch indices must name a declared instruction. Unknown opcodes
and `OP_UNDEF` are rejected. Four-byte immediates are read as little-endian
bytes, avoiding unaligned integer accesses on PowerPC.

Preparation retains the existing sparse code representation and accepts the
trailing alignment padding emitted by `q3asm/q3asm.c`. Branch translation runs
after every instruction offset is known. Failed preparation returns to
`VM_Create`, which frees the file, clears the unfinished VM registration, and
raises `ERR_DROP`.

## Validation

`tests/run_vm_bytecode_tests.sh` links the actual preparation function under
ASan/UBSan. Exact-sized fixtures cover every truncation of each operand type,
all 256 opcode values, mismatched instruction counts, every conditional branch
opcode with negative/out-of-range targets, four code alignments, and a valid
mixed stream with a forward branch, signed immediates, an argument, and padding.
Rejected streams leave the instruction table unchanged and allocate no decoded
code. The loading harness also checks cleanup and a subsequent successful load
after preparation fails. Neither harness executes QVM instructions.

All three host C regression runners and all eight Python tests pass locally.
The bytecode runner is included in Portable CI. Clang reports existing pointer
width warnings in discarded legacy interpreter code on the 64-bit host.

```sh
export TMPDIR="${TMPDIR:-/var/tmp}"
CC=clang bash tests/run_vm_bytecode_tests.sh
CC=clang bash tests/run_vm_loading_tests.sh
CC=clang bash tests/run_host_c_tests.sh
python3 -m unittest discover -s tests -p 'test_*.py' -v
```

Both Retro68 products compile without warning/error diagnostics and pass PEF
header validation. The temporary host runtime setup is documented in the
[loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,669,539 | `cc3af067da5a16e90e41ebbf945871777689f446ca6ac0b3090424f1fbe9651e` |
| Quake3_TeamArena | 3,818,113 | `44506fa3ba249b783a15d3b8bb6800b1dd8914785f90f5592de8789a62193ec8` |

## Remaining acceptance

The [next step](qvm-runtime-validation.md) covers runtime stacks and dynamic
control flow. Keep #35 open: data-image accesses, arithmetic edge cases, VM
call marshalling, and syscall pointer/range checks remain separate work. Preparation checks do not prove that execution is safe. Retail
baseq3/Team Arena QVM compatibility on Mac OS 9 remains deferred to the user's
live-testing session; no target runtime result is claimed.
