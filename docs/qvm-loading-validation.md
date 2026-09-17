# QVM loading validation — 2026-09-17

This is the first implementation step for issue #35. `VM_Create` and
`VM_Restart` now share file-header validation before allocation, clearing, or
copying data. It covers header truncation, code/data file ranges, instruction
count/allocation arithmetic, initialized-word alignment, data/literal/BSS
sums, and power-of-two data allocation. Restart rejects either growth or
shrinkage of the existing allocation, or any change to the original image.
A hunk-owned copy of the normalized header and complete file bytes provides
an exact comparison, including code, initialized data, and padding. It costs
one additional file-sized allocation per loaded QVM; restart does not allocate.
Changed modules require a full reload rather than `map_restart`.
Invalid headers free their file buffer
and raise `ERR_DROP`. Creation clears its uninitialized VM registration;
restart preserves the live VM for the normal shutdown callback before cleanup.

## Host regressions

`tests/run_vm_loading_tests.sh` runs the actual loader functions under
ASan/UBSan with exact-sized synthetic file buffers. Its tests cover:

- every truncation of a 47-byte structural fixture, on create and restart;
- negative, overlapping, out-of-file, and extreme signed offsets/counts;
- same-allocation changes to code, counts, layout, BSS, and initial data;
- incomplete initialized words, overflowing data images, and an empty image;
- valid initialization, byte order, literal bytes, zero filling, and restart;
- rejected restarts preserving the VM and data until a simulated
  `SV_ShutdownGameProgs` callback runs through the real `VM_Call` dispatcher;
- balanced file-buffer ownership and rejection before hunk allocation.

The harness substitutes file I/O, hunk storage, and interpreter preparation.
It does **not** execute QVM instructions or validate the runtime sandbox.
The same harness linked against baseline `204fe36` reports an ASan
heap-buffer-overflow; it passes with this change using Clang. The host compiler
reports existing 32-bit-pointer/format warnings in unrelated, discarded VM
functions. The local GCC sanitizer runtime is unavailable; CI uses its own
host compiler/runtime.

```sh
export TMPDIR="${TMPDIR:-/var/tmp}"
CC=clang bash tests/run_vm_loading_tests.sh
python3 -m unittest discover -s tests -p 'test_*.py' -v
```

The eight existing Python tests also pass, and the new harness is included in
Portable CI's host C regression job.

## Retro68 cross-build

`./build_mac.sh --team-arena` compiled both products and validated their
`Joy!peff` / `pwpc` headers. No compiler warning/error diagnostics occurred in
the successful build log.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,669,573 | `1a9fc1f7bd9b22a98989e4edde8e4ad5196d92ebee3bbc0c54aef7526a52aa35` |
| Quake3_TeamArena | 3,818,147 | `b9b99762404e29f8eb114bd1f16c5c99a3c6ed9b7e2146293b96333ade6201ab` |

The installed compiler/archive tools initially lacked `libisl.so.23` and
`libfl.so.2`. Validation used temporary libraries through `LD_LIBRARY_PATH`:
ISL 0.24 from the GCC infrastructure archive, verified against the SHA-512 in
`tools/Retro68-src/gcc/contrib/prerequisites.sha512`, and Fedora's signed
`libfl2-2.6.4-24.fc44.x86_64` package. No system packages were installed.
A fresh configure after repairing compiler startup selected the target
archiver/ranlib instead of retaining the failed configure's host tools.

## Remaining acceptance

The [next step](qvm-bytecode-validation.md) adds bytecode preparation checks.
Keep #35 open: runtime stack/PC/data-image bounds, syscall pointer/range
checks, and retail baseq3/Team Arena QVM testing
on Mac OS 9 still need implementation or evidence. These builds and tests do
not establish that untrusted QVM execution is safe. No target runtime,
packaging, or mounted-resource validation is claimed here.
