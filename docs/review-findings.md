# Repository review findings

Review date: 2026-07-28
Baseline: `abe5028` on `jm2/Quake-III-Arena:master`

## Outcome

The port cross-builds into plausible PowerPC PEFs, but it is not yet a
validated playable, secure, or releasable Mac OS 9 port. The latest
security-review patch set produced a 3,669,561-byte base PEF and a
3,818,135-byte Team Arena PEF; both have `Joy!peff`/`pwpc` headers. No current
artifact has passed an end-to-end target smoke test.

The review produced 52 confirmed GitHub issues. They include both original
port completeness defects and security/release root causes; locally drafted
fixes remain open until focused and target tests pass.

The statement that all modern CVEs are fixed is false for the reviewed
baseline. A first-pass comparison with ioquake3 confirmed missing security
families in QVM interpretation, PK3 decompression, connection authentication
and rate limiting, QVM privileges, cinematics, images, models, BSP/AAS, shader
and bot parsers. The initial provenance matrix is in
[security-provenance.md](security-provenance.md); full coverage and regression
proof remain tracked by
[#29](https://github.com/jm2/Quake-III-Arena/issues/29).

## Highest-risk confirmed areas

1. The interpreted QVM sandbox has unchecked bytecode/header, stack, branch,
   and data-image accesses; Mac OS 9 forces downloaded QVMs through it
   ([#35](https://github.com/jm2/Quake-III-Arena/issues/35)).
2. Malformed RoQ, BMP/PCX/TGA/JPEG, MD3/MD4, BSP, shader/skin, AAS, and bot
   inputs can reach native out-of-bounds access or hangs
   ([#41](https://github.com/jm2/Quake-III-Arena/issues/41) through
   [#48](https://github.com/jm2/Quake-III-Arena/issues/48)).
3. Connection setup/netchan traffic lacks negotiated-challenge binding, and
   connectionless rate limiting is incomplete
   ([#37](https://github.com/jm2/Quake-III-Arena/issues/37),
   [#38](https://github.com/jm2/Quake-III-Arena/issues/38)).
4. Renderer setup and gamma restore are not failure-transactional
   ([#15](https://github.com/jm2/Quake-III-Arena/issues/15),
   [#16](https://github.com/jm2/Quake-III-Arena/issues/16)).
5. Team Arena compiles but its custom parser, model list, and bot list remain
   functionally incomplete
   ([#11](https://github.com/jm2/Quake-III-Arena/issues/11),
   [#12](https://github.com/jm2/Quake-III-Arena/issues/12)); menu allocation
   failure handling is only partially repaired
   ([#49](https://github.com/jm2/Quake-III-Arena/issues/49)).
6. InputSprocket and Open Transport assume success and incomplete device or
   endpoint state
   ([#17](https://github.com/jm2/Quake-III-Arena/issues/17),
   [#20](https://github.com/jm2/Quake-III-Arena/issues/20)).
7. Sound is still opt-in and lacks target validation
   ([#5](https://github.com/jm2/Quake-III-Arena/issues/5)).
8. Hard-linked base modules can defeat QVM-only mods
   ([#13](https://github.com/jm2/Quake-III-Arena/issues/13)).
9. Packaging is not reproducible and has not been mounted/booted as a release
   artifact
   ([#22](https://github.com/jm2/Quake-III-Arena/issues/22),
   [#23](https://github.com/jm2/Quake-III-Arena/issues/23)).

## Local fixes drafted during this pass

- Complete Retro68/OpenGL prerequisite checks in Bash and PowerShell.
- Explicit base/Team Arena build selection and stale-artifact gating.
- Stronger PEF/resource requirements and removal of the invalid demo fallback.
- Catalog Manager file enumeration instead of hardcoded pak/cfg names.
- Removal of forced windowed mode; corrected 16-bit color selection and
  anisotropic cvar initialization.
- Event queue payload ownership, startup argument bounds, fatal exit status,
  undefined `EventRecord` use, and InputSprocket count clamps.
- Sound command/callback/DMA lifecycle fixes while keeping the unvalidated
  backend opt-in.
- Team Arena numeric token handling and removal of a client-time freeze while
  recording with the console open.
- Consistent `IDQ3` creator/BNDL metadata.
- Backports for message/Huffman exact bounds, reliable acknowledgements,
  client/server download restrictions, callvote/client-command injection,
  known format-string call sites, remote configstring lengths, connect/RCON
  buffers, collision/lightmap/tcMod bounds, and several bot parser indexes.
- Length-checked PK3 entry opening/reading, short-filename extension checks,
  and server-controlled `clientNum` validation.
- Partial JPEG RGBA sizing and Team Arena UI allocation/reload hardening.
- Windows Rez include discovery, strict native package exit/fresh-output
  checks, and removal of dummy missing-icon resources.
- Branch-specific setup dependencies and MacBinary encoded-name/date/fork
  metadata validation.

These review changes have cross-build evidence but are not completed issue
closures. The exact status and next checks live in [task.md](task.md).

## Review limits

This pass combined source inspection, history inspection, Bash/PowerShell
static checks, an ioquake3 security-history comparison, base and Team Arena
cross-builds, and PEF header/size validation. It did not have a legal retail
data set, a Mac OS 9 emulator session, period hardware, fault-injection stubs
for Classic APIs, or malformed-input/sanitizer corpora. A successful
cross-build is not exploit-resistance evidence; runtime and security claims
remain open until those checks exist.
