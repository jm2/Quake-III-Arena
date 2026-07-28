# Security provenance review

Review date: 2026-07-28
Local baseline: `abe5028afda280240d48d012a62c09e713689fd2`
ioquake3 comparison point:
[`588393618dbc82e7207c21c6ddecca229944a03a`](https://github.com/ioquake/ioq3/commit/588393618dbc82e7207c21c6ddecca229944a03a)
(2026-07-16)

This is a first-pass provenance ledger, not a claim of complete CVE coverage.
The local status column describes this review patch set. A cross-build proves
that the code compiles; it does not prove exploit resistance on Mac OS 9.
Every accepted family still needs a focused malformed-input regression.

## Confirmed security patch families

| Family | Upstream reference | Baseline status | Current local status |
| --- | --- | --- | --- |
| Arbitrary server file download (CVE-2006-2082) | [`60293f49`](https://github.com/ioquake/ioq3/commit/60293f49ee8c665673202e80ecd103f13a9fa6ab) | Missing: any requested server path could reach `FS_SV_FOpenFileRead` | Locally ported: only exact referenced `.pk3` names are opened; retail paks remain blocked |
| Shader-remap/extension overflow (CVE-2006-2236) | [`d2141145`](https://github.com/ioquake/ioq3/commit/d21411452ef32b86c0b79ddcaf49221701dcdb07) | Missing two-argument unbounded `COM_StripExtension` | Locally ported with destination sizes at every call site |
| Malicious download and snapshot lengths | [`99abd01c`](https://github.com/ioquake/ioq3/commit/99abd01c2f5e1a181acb8623edceff10cd918751) | Missing | Locally ported |
| Truncated Huffman/message reads and exact-capacity writes | [`d2b1d124`](https://github.com/ioquake/ioq3/commit/d2b1d124d4055c2fcbe5126863487c52fd58cca1), [`1e309787`](https://github.com/ioquake/ioq3/commit/1e309787224326b66f04cd166fbd9e200f5fded5) | Missing | Locally ported, including unaligned OOB integer access fixes for PowerPC |
| Reliable-acknowledgement server DoS | [`47c96419`](https://github.com/ioquake/ioq3/commit/47c9641939d84cfae249b38d2691d37ff84be817) | Missing | Locally ported |
| Cgame shader-state/configstring overflows | [`797168fa`](https://github.com/ioquake/ioq3/commit/797168fa0898fd81491b093ee9c5f9c6f82fee36), [`604b63f0`](https://github.com/ioquake/ioq3/commit/604b63f00f3f38ab8be33d8e1e72c086d9148fbd) | Missing | Locally ported |
| Cgame item configstring overflow | [`fc244c97`](https://github.com/ioquake/ioq3/commit/fc244c97ef1a5f1c6e7c1f46a098c8f57f271153) | Missing | Locally ported |
| Client connect-packet overflow | [`63e6c82f`](https://github.com/ioquake/ioq3/commit/63e6c82f4b91f7ae0ffb9de149a1d05eb5e28e9a) | Missing | Locally ported |
| Client-command/callvote injection | [`f5aae784`](https://github.com/ioquake/ioq3/commit/f5aae78481d71307a0b874b1f17ecdead1469392), [`cf791d14`](https://github.com/ioquake/ioq3/commit/cf791d14c58f536eec8220d93fb9af443f8837e9) | Missing | Locally ported argument separator and length sanitization |
| Remote/local format-string call sites | [`59c231c6`](https://github.com/ioquake/ioq3/commit/59c231c6c6ee9c460a252aea74a8aa1b84da4e1a), [`8ca8d845`](https://github.com/ioquake/ioq3/commit/8ca8d845911fb6545bf723cade39944d874d01ea) | Multiple missing call-site fixes | Known upstream call sites locally ported; a broader formatter audit remains open |
| Rcon, token, info-string, and server-command bounds | [`33a48a03`](https://github.com/ioquake/ioq3/commit/33a48a0336865a9d21983e4836920cd9f3401101) | Missing | Locally ported |
| Bot primitive/preprocessor/avoid-reach bounds | [`90f2f02c`](https://github.com/ioquake/ioq3/commit/90f2f02c55af937f83cacfdcd4188ea6359ddaa0), [`078d004d`](https://github.com/ioquake/ioq3/commit/078d004dc272759154caf83ca9549c3a4c0cb5ee), [`b97a7e25`](https://github.com/ioquake/ioq3/commit/b97a7e25836d15003c1e7fd0fc60c10f195f642a) | Missing | Locally ported; larger bot parser/AAS validation remains open |
| Patch collision plane OOB | [`9d742275`](https://github.com/ioquake/ioq3/commit/9d74227559d46b85d0c43d395cd280d3de7ae8f4) | Missing | Locally changed from warning-and-index to `ERR_DROP` |
| Renderer one-minus-entity static overflow | [`21e0bdd9`](https://github.com/ioquake/ioq3/commit/21e0bdd99378c2cb10ab8d0c28e3a4c1de4c9df2) | Missing | Locally ported |
| Lightmap array overflow | [`769372e2`](https://github.com/ioquake/ioq3/commit/769372e2f9e384b73f4f1882ddb888857ed3c1e8) | Missing | Locally capped; general BSP lump validation remains open |
| Long `tcMod` shader arguments | [`eeeaf3f1`](https://github.com/ioquake/ioq3/commit/eeeaf3f1252d95a6037f33d30fdf2e945e340f79) | Missing | Locally converted to bounded concatenation |
| JPEG RGB/RGBA allocation overflow | [`62678a02`](https://github.com/ioquake/ioq3/commit/62678a021554ec4ef6e310dfc63ab2e4f58135f6) | Missing and deterministically wrote alpha beyond a three-byte allocation | Locally allocates four bytes/pixel, validates dimensions/components, and expands backwards; length-aware JPEG I/O remains open |
| Download-list truncation and path overwrite | [`813a6ecd`](https://github.com/ioquake/ioq3/commit/813a6ecdc3b8572796a8a85b260b03e1c3d87ef4) | Partial traversal check, but non-atomic pair construction and mismatched pak-name counts remained | Locally validates complete relative `.pk3` pairs, appends atomically, and skips missing name entries |
| Oversized/truncated PK3 entries | Local audit; [#36](https://github.com/jm2/Quake-III-Arena/issues/36) | ZIP-controlled unsigned size narrowed to `int`; unzip opens/reads were unchecked | Locally rejects sizes above `INT_MAX - 1`, validates open/exact read, and cleans failure state; malicious ZIP corpus remains required |
| Server-controlled native cgame index | Local audit; [#40](https://github.com/jm2/Quake-III-Arena/issues/40) | Gamestate `clientNum` reached native `cgs.clientinfo[]` indexing unchecked | Locally rejected outside `[0, MAX_CLIENTS)`; malformed-gamestate regression remains required |

## Confirmed unresolved security families

- [#35](https://github.com/jm2/Quake-III-Arena/issues/35):
  interpreted QVM validation and sandbox bounds. The Mac port forces bytecode
  interpretation, while the old interpreter lacks the later branch, stack,
  program-counter, syscall pointer, and block-copy checks from
  [`469c9866`](https://github.com/ioquake/ioq3/commit/469c986640a8f237e4b1776c4e7cb1aa99d7f7f8)
  and
  [`83522282`](https://github.com/ioquake/ioq3/commit/83522282f1cc4919e2866104030364839fd482de).
- [#37](https://github.com/jm2/Quake-III-Arena/issues/37): connection setup and
  sequenced packets are not bound to negotiated challenges.
- [#38](https://github.com/jm2/Quake-III-Arena/issues/38): connectionless
  `getinfo`, `getstatus`, `getchallenge`, and rcon limiting is incomplete and
  globally unfair.
- [#39](https://github.com/jm2/Quake-III-Arena/issues/39): QVM syscalls can
  modify engine-sensitive cvars and remove built-in commands.
- [#41](https://github.com/jm2/Quake-III-Arena/issues/41) through
  [#47](https://github.com/jm2/Quake-III-Arena/issues/47): renderer, collision,
  cinematic, image, model, BSP, font, skin, shader, and AAS loaders do not
  validate complete input ranges and graph references before pointer
  arithmetic and allocation.
- [#43](https://github.com/jm2/Quake-III-Arena/issues/43): JPEG
  source/destination managers are not length/capacity aware, duplicate
  public libjpeg entry points are linked, and malformed decoder errors can
  terminate the process.
- [#48](https://github.com/jm2/Quake-III-Arena/issues/48): bot preprocessor
  token concatenation, paths, and cleanup remain unsafe.
- The QVM libc still maps `Q_vsnprintf` to its historical unbounded
  `vsprintf`; native static modules use the bounded formatter.

## Required validation

- [ ] Add host ASan/UBSan harnesses for message, download, format-string, image,
      model, BSP, bot/AAS, cinematic, and QVM malformed-input corpora.
- [ ] Exercise accepted message patches at empty, one-bit-short, exact-capacity,
      and one-bit-over limits.
- [ ] Exercise every download rejection spelling (`../`, `..\\`, `::`,
      absolute, empty, non-pk3, unreferenced, and truncated pair).
- [ ] Run valid baseq3 and missionpack assets/QVMs after hardening to detect
      compatibility regressions.
- [ ] Re-run the upstream security-history comparison whenever the ioquake3
      anchor changes.
