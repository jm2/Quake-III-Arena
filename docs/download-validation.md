# Download boundary validation

This step validates the existing download hardening against the real client,
filesystem, parser, and server code, and closes two gaps found while building
the corpus. It keeps the Quake III Arena 1.32c download commands, packet
layout, public structures, and `.pk3` naming behavior unchanged.

The primary upstream references are ioquake3's
[`813a6ecd`](https://github.com/ioquake/ioq3/commit/813a6ecdc3b8572796a8a85b260b03e1c3d87ef4)
atomic download-list fix,
[`60293f49`](https://github.com/ioquake/ioq3/commit/60293f49ee8c665673202e80ecd103f13a9fa6ab)
referenced-pak restriction, and
[`99abd01c`](https://github.com/ioquake/ioq3/commit/99abd01c2f5e1a181acb8623edceff10cd918751)
download block-length validation. The local checks retain those contracts and
also cover Classic Mac `:` separators.

## Findings and changes

- `CL_BeginDownload` already rejected empty, absolute, traversal, delimiter,
  oversized, and non-pk3 local or remote names. `FS_ComparePaks` already built
  each `@remote@local` pair only after proving the complete pair fit.
- `CL_NextDownload` recursively skipped each rejected pair. A maximum-size
  server list of empty pairs could therefore consume hundreds of client stack
  frames. It now consumes rejected pairs in a bounded loop, clears a trailing
  incomplete field, and starts the first valid complete pair exactly as before.
- Server authorization used `FS_FilenameCompare`, which treats `/`, `\`, and
  `:` as interchangeable. The server now accepts only a case-insensitive,
  literal forward-slash spelling present in `FS_ReferencedPakNames`; traversal,
  native separator aliases, delimiters, non-pk3 names, and unreferenced paks
  are rejected before `FS_SV_FOpenFileRead`.
- Retail `baseq3/pakN.pk3` and `missionpack/pakN.pk3` classification remains in
  place, so commercial assets are still never offered for autodownload.

## Host corpus

`tests/run_download_tests.sh` compiles four isolated regressions with ASan and
UBSan in normal and optimized modes. Portable CI runs the matrix with GCC and
Clang.

- The client fixture calls the production pair validator and list consumer for
  valid pairs, every unsafe spelling, name capacities, a truncated pair, an
  invalid pair followed by a valid pair, and a full 1023-byte invalid list.
- The filesystem fixture calls the production `FS_ComparePaks` body for a
  missing pak, a same-name wrong-checksum pak, an already-present checksum,
  exact and one-byte-short output capacities, multiple pairs, empty names, and
  every path/delimiter rejection.
- The parser fixture calls the production `CL_ParseDownload` body for an
  unsolicited block, negative and oversized chunk lengths, a negative file
  size, an out-of-order block, an exact `MAX_MSGLEN` block, and zero-length EOF.
- The server fixture calls the private production authorization body for
  canonical mod paks, case variants, both retail families, unreferenced names,
  native separator aliases, traversal, delimiters, and overlong input.

Both compiler matrices pass all eight normal/optimized fixture executions.
Base and Team Arena cross-build evidence is recorded with the pull request.
Retail-asset gameplay and Mac OS 9 network testing remain deferred until the
user supplies that environment.
