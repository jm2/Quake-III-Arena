# Security provenance review

Review date: 2026-07-28
Last validation update: 2026-11-10 (tree at `4c50a5f1`, after #566)
Local baseline: `abe5028afda280240d48d012a62c09e713689fd2`
ioquake3 comparison point:
[`588393618dbc82e7207c21c6ddecca229944a03a`](https://github.com/ioquake/ioq3/commit/588393618dbc82e7207c21c6ddecca229944a03a)
(2026-07-16)

This is a provenance ledger, not a claim of complete CVE coverage. A
cross-build proves that the code compiles; it does not prove exploit
resistance on Mac OS 9. A family whose regression column says "none yet"
still needs a focused malformed-input test.

Every CVE id in this file was checked against its MITRE record
(`cveawg.mitre.org/api/cve/<id>`) on 2026-11-10, and the record's text or
references were matched to the upstream fix named in the row. Where the
link between a CVE and an upstream commit rests on something weaker than
the record itself, the row says so. Any CVE id cited in `code/`, `docs/`,
`tests/` or the top-level documents must have a row here;
`tests/run_cve_provenance_tests.sh` enforces that.

Columns:

- **Local fix**: the PR or commit in this repository that made the change.
  [`204fe36d`][c204fe36d] is the 2026-07-28 review commit that carried the
  first-pass backports; it was pushed directly, not through a PR.
- **Regression**: the `tests/run_*_tests.sh` runner that fails without the
  fix. "None yet" means no runner reaches the changed check with a
  malformed input.

## Confirmed security patch families

| Family | Upstream reference | Baseline status | Local fix | Current local status | Regression |
| --- | --- | --- | --- | --- | --- |
| Arbitrary server file download (CVE-2006-2082) | [`60293f49`](https://github.com/ioquake/ioq3/commit/60293f49ee8c665673202e80ecd103f13a9fa6ab) | Missing: any requested server path could reach `FS_SV_FOpenFileRead` | [`204fe36d`][c204fe36d]; the refusal text is held back by [#452][pr452] | Locally ported: only exact referenced `.pk3` names are opened; retail paks remain blocked | `run_server_snapshot_budget_tests.sh` checks the unreferenced, id pk3 and Team Arena pk3 refusals. The `../`, `..\` and `::` spellings are not exercised yet |
| Shader-remap/extension overflow (CVE-2006-2236) | [`d2141145`](https://github.com/ioquake/ioq3/commit/d21411452ef32b86c0b79ddcaf49221701dcdb07) | Missing two-argument unbounded `COM_StripExtension` | [`204fe36d`][c204fe36d]; game-side remap names bounded by [#449][pr449] | Locally ported with destination sizes at every call site | `run_host_c_tests.sh` (`COM_StripExtension` into a short buffer); `run_game_shader_remap_tests.sh` (remap table and `CS_SHADERSTATE`) |
| Malicious download and snapshot lengths (CVE-2006-2875) | [`99abd01c`](https://github.com/ioquake/ioq3/commit/99abd01c2f5e1a181acb8623edceff10cd918751) | Missing | [`204fe36d`][c204fe36d] | Locally ported: `CL_ParseDownload` refuses a chunk size outside `[0, sizeof data]` and `CL_ParseSnapshot` an areamask longer than its array. The CVE record names `CL_ParseDownload`; `99abd01c` fixes that function's overflow reported by Auriemma in June 2006 | None yet |
| Truncated Huffman/message reads and exact-capacity writes (CVE-2017-11721) | [`d2b1d124`](https://github.com/ioquake/ioq3/commit/d2b1d124d4055c2fcbe5126863487c52fd58cca1), [`1e309787`](https://github.com/ioquake/ioq3/commit/1e309787224326b66f04cd166fbd9e200f5fded5) | Missing | [`204fe36d`][c204fe36d], [#210][pr210] | Locally ported, including unaligned OOB integer access fixes for PowerPC; [exact-limit sanitizer evidence](message-huffman-validation.md) covers the codec and out-of-band caller. The CVE record links `d2b1d124` | `run_message_huffman_tests.sh` |
| Delta field counts | [`3ff26663`](https://github.com/ioquake/ioq3/commit/3ff266637578b9727ddee84e966214dd5dc51f04) (ioquake3 bug 4669; no CVE found) | Missing: a field count above the table walked past `entityStateFields`/`playerStateFields` | [`e1fcf08f`][ce1fcf08f] | Locally ported (`ERR_DROP`). [`53440e7f`][c53440e7f] adds a local per-snapshot cap of `MAX_PARSE_ENTITIES` in `CL_DeltaEntity`, which has no upstream counterpart | None yet |
| Reliable-acknowledgement server DoS | [`47c96419`](https://github.com/ioquake/ioq3/commit/47c9641939d84cfae249b38d2691d37ff84be817) | Missing | [`204fe36d`][c204fe36d] | Locally ported. No CVE found | None yet: no out-of-range acknowledge case |
| Cgame shader-state/configstring overflows | [`797168fa`](https://github.com/ioquake/ioq3/commit/797168fa0898fd81491b093ee9c5f9c6f82fee36), [`604b63f0`](https://github.com/ioquake/ioq3/commit/604b63f00f3f38ab8be33d8e1e72c086d9148fbd) | Missing | [`204fe36d`][c204fe36d] | Locally ported. No CVE found | `run_game_configstring_transport_tests.sh` (`teamVoteString` size); `run_game_shader_remap_tests.sh` (`CG_ShaderStateChanged`) |
| Cgame item configstring overflow (CVE-2006-3401) | [`fc244c97`](https://github.com/ioquake/ioq3/commit/fc244c97ef1a5f1c6e7c1f46a098c8f57f271153) | Missing | [`204fe36d`][c204fe36d] | Locally ported: `CS_ITEMS` is copied with `Q_strncpyz`. The CVE record and `fc244c97` cite the same exploit (milw0rm/exploit-db 1977) | None yet |
| Client connect-packet overflow | [`63e6c82f`](https://github.com/ioquake/ioq3/commit/63e6c82f4b91f7ae0ffb9de149a1d05eb5e28e9a) | Missing | [`204fe36d`][c204fe36d] | Locally ported: the connect packet is built with `Com_sprintf`. No CVE found | `run_protocol71_tests.sh` checks the connect packet's bytes; no oversized-userinfo case |
| Client-command/callvote injection | [`f5aae784`](https://github.com/ioquake/ioq3/commit/f5aae78481d71307a0b874b1f17ecdead1469392), [`cf791d14`](https://github.com/ioquake/ioq3/commit/cf791d14c58f536eec8220d93fb9af443f8837e9) | Missing | [`204fe36d`][c204fe36d]; vote display [#460][pr460] | Locally ported argument separator and length sanitization. No CVE found | None yet for the separators; `run_game_configstring_transport_tests.sh` runs `Cmd_CallVote_f` with ordinary votes |
| Remote/local format-string call sites | [`59c231c6`](https://github.com/ioquake/ioq3/commit/59c231c6c6ee9c460a252aea74a8aa1b84da4e1a), [`8ca8d845`](https://github.com/ioquake/ioq3/commit/8ca8d845911fb6545bf723cade39944d874d01ea) | Multiple missing call-site fixes | [`204fe36d`][c204fe36d]; compile-time checks [#455][pr455], [#506][pr506] | Known upstream call sites locally ported; a broader formatter audit remains open. No CVE found | `run_format_attribute_tests.sh` and `run_client_format_tests.sh` (compile-time format checks; no run-time injection case) |
| Rcon, token, info-string, and server-command bounds (CVE-2005-0983) | [`33a48a03`](https://github.com/ioquake/ioq3/commit/33a48a0336865a9d21983e4836920cd9f3401101) | Missing | [`204fe36d`][c204fe36d] | Locally ported, including the 1022-character cap in `SV_SendServerCommand`. `33a48a03` marks that cap as the fix for Auriemma's q3msgboom advisory, which the CVE record cites | `run_host_c_tests.sh` (`COM_ParseExt` token cap); `run_info_string_tests.sh`. None yet for `CL_Rcon_f` or an over-long server command |
| Infostring crash through an echoed challenge (CVE-2005-0430) | [`01da6d75`](https://github.com/ioquake/ioq3/commit/01da6d757bb3121c9ee077e7269eee7655abd05b), [`6b811ada`](https://github.com/ioquake/ioq3/commit/6b811ada7ab0d419ad678a17150eb8eb90519494) | Missing: `SVC_Status` and `SVC_Info` echoed a challenge of any length into the infostring | [#309][pr309] (for #257) | Locally ported: a challenge over 128 characters gets no reply. `01da6d75` names Auriemma's infostring bug; the CVE record cites his q3infoboom advisory | `run_server_connectionless_tests.sh` |
| Bot primitive/preprocessor/avoid-reach bounds | [`90f2f02c`](https://github.com/ioquake/ioq3/commit/90f2f02c55af937f83cacfdcd4188ea6359ddaa0), [`078d004d`](https://github.com/ioquake/ioq3/commit/078d004dc272759154caf83ca9549c3a4c0cb5ee), [`b97a7e25`](https://github.com/ioquake/ioq3/commit/b97a7e25836d15003c1e7fd0fc60c10f195f642a) | Missing | [`204fe36d`][c204fe36d] | Locally ported; larger bot parser/AAS validation remains open. No CVE found | `run_bot_define_tests.sh` (`PC_DefineFromString`). None yet for `PS_ReadPrimitive`'s token cap or `BotResetLastAvoidReach` |
| Patch collision plane OOB | [`9d742275`](https://github.com/ioquake/ioq3/commit/9d74227559d46b85d0c43d395cd280d3de7ae8f4) | Missing | [`204fe36d`][c204fe36d] | Locally changed from warning-and-index to `ERR_DROP`. No CVE found | None yet |
| Renderer one-minus-entity static overflow | [`21e0bdd9`](https://github.com/ioquake/ioq3/commit/21e0bdd99378c2cb10ab8d0c28e3a4c1de4c9df2) | Missing | [`204fe36d`][c204fe36d] | Locally ported. No CVE found | None yet |
| Lightmap array overflow | [`769372e2`](https://github.com/ioquake/ioq3/commit/769372e2f9e384b73f4f1882ddb888857ed3c1e8) | Missing | [`204fe36d`][c204fe36d], [#82][pr82] | Locally capped; general BSP lump validation remains open. No CVE found | `run_bsp_lightmap_tests.sh` |
| Long `tcMod` shader arguments | [`eeeaf3f1`](https://github.com/ioquake/ioq3/commit/eeeaf3f1252d95a6037f33d30fdf2e945e340f79) | Missing | [`204fe36d`][c204fe36d] | Locally converted to bounded concatenation. No CVE found | None yet for an over-long argument list; `run_shader_stage_tests.sh` parses ordinary `tcMod` stages |
| JPEG RGB/RGBA allocation overflow | [`62678a02`](https://github.com/ioquake/ioq3/commit/62678a021554ec4ef6e310dfc63ab2e4f58135f6) | Missing and deterministically wrote alpha beyond a three-byte allocation | [`204fe36d`][c204fe36d]; JPEG I/O [#76][pr76], [#472][pr472], [#478][pr478] | Locally allocates four bytes/pixel, validates dimensions/components, and expands backwards. No CVE found | `run_jpeg_io_tests.sh`, `run_image_dimension_tests.sh` |
| Download-list truncation and path overwrite (CVE-2006-3324) | [`813a6ecd`](https://github.com/ioquake/ioq3/commit/813a6ecdc3b8572796a8a85b260b03e1c3d87ef4) | Partial traversal check, but non-atomic pair construction and mismatched pak-name counts remained | [`204fe36d`][c204fe36d] | Locally validates complete relative `.pk3` pairs, appends atomically, and skips missing name entries. The CVE record gives ioquake3 svn r804 and Auriemma's q3cfilevar advisory; `813a6ecd` is the June 2006 git commit for that report. The svn-to-git mapping is from the date and message | None yet for the refusals; `run_client_download_tmp_tests.sh` drives `CL_BeginDownload` with valid names only |
| Server overwrite of protected client cvars (CVE-2006-3325) | [`7d51d75b`](https://github.com/ioquake/ioq3/commit/7d51d75b05a9593508040162709043516c0f2a17), [`a24ffc0d`](https://github.com/ioquake/ioq3/commit/a24ffc0dd544d9aabd99f9a0711401f0171195e0), [`8c3f3205`](https://github.com/ioquake/ioq3/commit/8c3f320504923906b074d84f0bfe13c7df92bc89) | Missing: `CL_SystemInfoChanged` set any cvar the server named | [#407][pr407] (for #39) | Locally ported: a server may create cvars and set systeminfo, server-created and user-created ones, but not a cvar the engine or a module registered without `CVAR_SYSTEMINFO` (retail's three prediction cvars excepted); protected cvars are refused | `run_cvar_systeminfo_tests.sh`, `run_protected_cvar_tests.sh` |
| Auto-download content restrictions (CVE-2017-6903) | [`376267d5`](https://github.com/ioquake/ioq3/commit/376267d534476a875d8b9228149c4ee18b74a4fd), [`b173ac05`](https://github.com/ioquake/ioq3/commit/b173ac05993f634a42be3d3535e1b158de0c3372), [`f61fe5f6`](https://github.com/ioquake/ioq3/commit/f61fe5f6a0419ef4a88d46a128052f2e8352e85d) | Partial (#274): configs loaded from pk3s and VM writes had no extension restriction | [#324][pr324] (for #274), [#433][pr433]; protected cvars [#407][pr407] | Locally ported: `q3config.cfg` and `autoexec.cfg` never come from a pk3, and VM- and command-reachable writes, renames and removals refuse code and pak extensions. The native-DLL half does not apply, because the modules are linked statically | `run_fs_config_mutable_tests.sh`, `run_fs_write_ospath_tests.sh` |
| Oversized/truncated PK3 entries | Local audit; [#36](https://github.com/jm2/Quake-III-Arena/issues/36) | ZIP-controlled unsigned size narrowed to `int`; unzip opens/reads were unchecked | [#197][pr197] through [#209][pr209], [#421][pr421] (for #413) | Locally rejects sizes above `INT_MAX - 1`, validates open/exact read, and cleans failure state; host fixtures cover the caps and truncation, a target run remains | `run_fs_zip_entry_tests.sh`, `run_fs_zip_tests.sh` |
| Server-controlled native cgame index | Local audit; [#40](https://github.com/jm2/Quake-III-Arena/issues/40) | Gamestate `clientNum` reached native `cgs.clientinfo[]` indexing unchecked | [`204fe36d`][c204fe36d]; tests and further indexes [#321][pr321], [#418][pr418] | Locally rejected outside `[0, MAX_CLIENTS)` | `run_client_gamestate_tests.sh` (gamestate fuzzing), `run_cgame_server_index_tests.sh` |
| Configstring flood of clients still loading | [`189e8b33`](https://github.com/ioquake/ioq3/commit/189e8b33f31470ff4b0bef18634fc4332c99f776) (ioquake3 bug 2784; no CVE found) | Missing: every configstring change went to `CS_PRIMED` clients, and other players could overflow a slow loader's command queue | [#560][pr560] | Locally ported as ioquake3's `csUpdated`; a new gamestate also clears the marks | `run_server_userinfo_rate_tests.sh` (`LoadingClient`) |
| Connectionless query and rcon amplification (CVE-2010-5077) | [`4056c903`](https://github.com/ioquake/ioq3/commit/4056c9035825b2c6ffef3378b4af78e7e70bb743), [`a3c7003d`](https://github.com/ioquake/ioq3/commit/a3c7003d9cdde316ef1dc4b2c76428f295b93491), [`7b154150`](https://github.com/ioquake/ioq3/commit/7b1541504216ce09ef2c7987eb5a7117a2125a5a); [#38](https://github.com/jm2/Quake-III-Arena/issues/38) | One global 10-a-second count covered only `getstatus` and rcon; `getinfo` and `getchallenge` were unlimited, and a global 500 msec gate let bad rcon lock out the admin | [#431][pr431] (for #38) | Fully ported, no longer partial: each source gets 10 `getstatus`, `getinfo`, `getchallenge` or rcon requests at once and one more a second; the queries share 10 at once and one more every 100 msec, and bad rcon passwords 10 at once and one more a second. Lookups are bounded. A correct rcon password is never locked out: when its address has used up its quota (a flood may spoof that address) or no bucket is free, it falls back on an allowance of 10 at once and one more a second that only correct passwords can use. As with ioquake3, when many players refresh the LAN list at once (32 players send 64 `getinfo`), some miss the server on the first pass and see it on the next refresh | `run_server_rate_limit_tests.sh` |
| UDP spoofing of connection setup and sequenced packets (q3noclient) | [`a5580d89`](https://github.com/ioquake/ioq3/commit/a5580d8974008b077edf5ddaf7347d3b6006351d), [`e06c117e`](https://github.com/ioquake/ioq3/commit/e06c117e9e4361d5c0e4124682a23fd9872bd41f); [#37](https://github.com/jm2/Quake-III-Arena/issues/37) | Missing: a `challengeResponse` from any address replaced the server address, `connectResponse` was matched on the IP alone, sequenced packets on the address, qport and sequence alone, and an unverified packet could move a client's port or get an out-of-band `disconnect` | [#458][pr458]; unsigned challenges [#425][pr425] | Locally ported as ioquake3's protocol 71 (`com_protocol`, default 71), which ioquake3 and Quake3e peers negotiate: the client's own challenge is echoed in `challengeResponse`, the server's comes back in `connectResponse`, and every sequenced packet carries ioquake3's challenge checksum instead of the XOR encoding. Every `challengeResponse` carries ioquake3's three arguments, the client's challenge or 0 and `com_protocol`, as ioquake3 has since 2011: a bare one would be taken from the server's address as a retail server's, and a bare `getchallenge` spoofed from a client's address (whose own request a flood keeps under the rate limit) would talk it down to protocol 68. That line is the one exception to retail bytes; retail clients read only its first argument. Retail 1.32c peers otherwise keep protocol 68 byte for byte and so cannot be bound; a retail server's bare `challengeResponse` and any `connectResponse` are only taken from the address the client asked. Quake3e servers still answer a bare `getchallenge` bare, so a protocol 71 client can be talked down against them. A port moves only for a packet the netchan took, and no `disconnect` answers an unknown one. Challenges come from a generator seeded once from the Mac's microsecond clock, not from `rand()`, which the renderer reseeds with a constant. Neither it nor the checksum is cryptographic (sequence 1's checksum is 0 whatever the challenge); Mac OS 9 interoperability is untested. No CVE found for q3noclient | `run_protocol71_tests.sh`, `run_challenge_rand_tests.sh` |
| Userinfo and info-key character checks | Local audit | `SV_UpdateUserinfo_f` passed any string on; cvar info strings took `\`, `;` and `"` in keys and values | [`4e75fdfd`][c4e75fdf], [`8f4d76a5`][c8f4d76a] | `Info_Validate` gates userinfo updates, and `Info_ValidateKeyValue` gates cvar info strings. These commits first cited CVE ids that belong to other bugs ([#276](https://github.com/jm2/Quake-III-Arena/issues/276)); no CVE applies | `run_server_userinfo_rate_tests.sh` (a userinfo with `;` is refused) |

## Bundled library CVEs

The tree carries old copies of zlib's inflate (inside
`code/qcommon/unzip.c`) and libjpeg 6 (`code/jpeg-6`). Both are reachable
from any pk3, including one downloaded from a server.

| CVE | Library bug | Local fix | Status | Regression |
| --- | --- | --- | --- | --- |
| CVE-2002-0059 | zlib 1.1.3 and earlier: `inflateEnd` frees memory twice after malformed compressed data. Here a tree error other than a data error freed `blens` and `inflate_blocks_reset` freed it again | [#542][pr542] | Fixed as in zlib 1.1.4 | `run_unzip_tree_tests.sh` |
| CVE-2013-6629 | libjpeg 6b `get_sos` (`jdmarker.c`) accepts a component listed twice in one scan, leaving uninitialized memory in the output | [#539][pr539] (part of #43) | Fixed: a repeated component is rejected. The same PR fixes the scan-order plane selection in `decompress_onepass`, which has no CVE | `run_jpeg_io_tests.sh` (`ScanComponents`) |

## CVEs that do not apply

| CVE | What it is | Why it does not apply here |
| --- | --- | --- |
| CVE-2011-1412 | Shell metacharacters in a long `fs_game` reach a shell command in ioquake3's Unix `sys/sys_unix.c` (fixed in ioquake3 svn r2097; in git this appears to be [`22552c7b`](https://github.com/ioquake/ioq3/commit/22552c7bab5d16ce189d2f493720726885c87d14), "Replace usage of system with fork/exec", matched by date and content) | The Mac build has no `sys_unix.c`, and no engine or game source calls `system`, `popen`, `fork` or `exec*`. `CL_SystemInfoChanged` still refuses a server `fs_game` with a dot, `/`, `\`, `:`, `;` or `&` (added in [`d5657120`][cd565712]; [`2cef87c7`][c2cef87c] removed a truncating duplicate that made it unreachable, and [#322][pr322] added `:` for #262); `run_fs_qpath_tests.sh` covers it |
| CVE-2011-2764, CVE-2011-3012 | ioquake3's checks for dangerous file extensions let a crafted addon write a native library that is later loaded | Game modules are linked statically and no native library is loaded from the game directories. The write-side extension guard from CVE-2017-6903 still applies |
| CVE-2012-3345 | ioquake3 wrote `/tmp/ioq3.pid`, which a local user could replace with a symlink | The tree writes no pid file |

## Other Quake 3 CVEs not yet assessed

A keyword search of NVD for Quake 3 and ioquake3 on 2026-11-10 also
returned these. Each was checked against its MITRE record, but no upstream
fix or local code path has been tied to it yet.

- CVE-2000-0303: a server reads or modifies client files through `..`
  paths (Quake III Arena, 2000). No upstream commit identified; the
  download path checks above may cover it.
- CVE-2001-1289: a connection packet starting with several `0xff` bytes
  crashes Quake III Arena 1.29f and 1.29g. No upstream commit identified.
- CVE-2006-3400: a long server command overflows `CG_ServerCommand` in
  Soldier of Fortune 2's cgame. Q3A's cgame has not been compared.
- CVE-2019-1010043: Quake3e's `Cmd_Args` overflows `cmd_args`
  (`MAX_STRING_CHARS`) with `strcat` ([ec-/Quake3e#9](https://github.com/ec-/Quake3e/issues/9)).
  The MITRE record gives "Quake3e < 5ed740d", but `5ed740d` is an unrelated
  Win32 window commit; the fix is `fea3c41`, which cites issue #9 and
  enlarged the buffer to `BIG_INFO_STRING`. This tree had the same code.
  [#573][pr573] bounds `Cmd_Args` and `Cmd_ArgsFrom` with `Q_strcat`
  instead, since no caller needs more than `MAX_STRING_CHARS`, and bounds
  `SV_ConSay_f`, which overran its stack buffer with a long `Cmd_Args`;
  `run_cmd_forward_text_tests.sh` and `run_server_consay_tests.sh` cover
  them.

## Other upstream fixes from #554-#566

These recent PRs port an ioquake3 fix. They are bug fixes rather than
security families.

| PR | Bug | Upstream | Regression |
| --- | --- | --- | --- |
| [#556][pr556] | Tab completion cut the shared match at the end of the typed text only | [`7913ec3c`](https://github.com/ioquake/ioq3/commit/7913ec3c4ba3923742d3883ba587421db81fa9cc) | `run_field_complete_tests.sh` |
| [#561][pr561] | `Q_rsqrt` and `SquareRootFloat` read a float's bits through `long`, 8 bytes on LP64 hosts | [`d54f831d`](https://github.com/ioquake/ioq3/commit/d54f831dd73aa377d43122510b76e2517dd0a622) | `run_q_rsqrt_tests.sh` |
| [#564][pr564] | `R_MovePatchSurfacesToHunk` copied `heightLodError` onto itself, not into the hunk grid | [`aa3f7e1e`](https://github.com/ioquake/ioq3/commit/aa3f7e1eb13c6da375621b18c0385c6f544fed67) | `run_bsp_stitch_tests.sh` |

[#560][pr560] is in the families table. The other PRs in that range fix
local findings with no upstream fix or CVE: [#554][pr554] (packaging
download pinning, `test_packaging_tools.py`), [#555][pr555] (the CD key in
any letter case and startup passwords in the boot log,
`run_fs_config_mutable_tests.sh`, `run_mac_startup_parms_tests.sh`),
[#557][pr557] (in-game `donedl` resends, a regression of this fork,
`run_server_donedl_tests.sh`), [#558][pr558] (control characters in names
and oversized player configstrings, `run_game_configstring_transport_tests.sh`,
`run_server_userinfo_ip_tests.sh`), [#559][pr559] (chained `nextdl`,
`run_server_donedl_tests.sh`), [#562][pr562] (`R_StitchPatches` loops,
the same code as id's release, ioquake3 and Quake3e,
`run_bsp_stitch_tests.sh`), [#563][pr563] (botlib zone exhaustion,
`run_bot_parse_zone_tests.sh`), [#565][pr565] (password masking on the
last console line, `run_mac_startup_parms_tests.sh`) and [#566][pr566]
(`RB_SetColor` float-to-byte conversion, `run_setcolor_tests.sh`).

## Ids wrongly cited before #276

Earlier comments and `implementation_plan.md` cited these ids. They belong
to other products, so they must not appear outside `docs/`
(`run_cve_provenance_tests.sh` checks).

- CVE-2017-11722: a GraphicsMagick `WriteOnePNGImage` out-of-bounds read.
  It was cited for the userinfo check in `SV_UpdateUserinfo_f`.
- CVE-2018-12100: a Sonatype Nexus Repository Manager XSS. It was cited
  for the delta field-count checks and the per-snapshot entity cap.

The Huffman row's id was also cited for `Info_ValidateKeyValue` and for
"cl_guid truncation". It is the `MSG_ReadBits` overflow, and only that row
names it now.

## Confirmed unresolved security families

- [#35](https://github.com/jm2/Quake-III-Arena/issues/35):
  interpreted QVM validation and sandbox bounds. The Mac port forces bytecode
  interpretation, while the old interpreter lacks the later branch, stack,
  program-counter, syscall pointer, and block-copy checks from
  [`469c9866`](https://github.com/ioquake/ioq3/commit/469c986640a8f237e4b1776c4e7cb1aa99d7f7f8)
  and
  [`83522282`](https://github.com/ioquake/ioq3/commit/83522282f1cc4919e2866104030364839fd482de).
- [#36](https://github.com/jm2/Quake-III-Arena/issues/36): the PK3 entry
  checks pass their host fixtures and wait on a Mac OS 9 run.
- [#45](https://github.com/jm2/Quake-III-Arena/issues/45): BSP lumps and
  cross-references are not yet validated as one transaction.
- [#37](https://github.com/jm2/Quake-III-Arena/issues/37) (closed) leaves a
  known limit: protocol 68 connections, which retail 1.32c peers need, cannot
  be bound to the challenge, and a Quake3e server's bare answer to a spoofed
  bare `getchallenge` can talk a protocol 71 client down to them; protocol 71
  awaits a Mac OS 9 interoperability test.
- The QVM libc still maps `Q_vsnprintf` to its historical unbounded
  `vsprintf`; native static modules use the bounded formatter.

Closed since the first pass: #39 (protected cvars and engine commands,
[#407][pr407]), and #41 through #44 and #46 through #48 (RoQ, BMP/PCX/TGA,
JPEG I/O, MD3/MD4, shader/skin/font, AAS and bot preprocessor bounds). Their
PRs and runners are listed on each issue and in `docs/task.md`.

## Required validation

- [x] Add a host ASan/UBSan corpus for message/Huffman and out-of-band caller
      bounds; see [message/Huffman evidence](message-huffman-validation.md).
- [ ] Add remaining host ASan/UBSan corpora for download, format-string, image,
      model, BSP, bot/AAS, cinematic, and QVM malformed inputs.
- [ ] Add malformed-input runners for every family above whose regression
      column says "none yet".
- [x] Exercise accepted message patches at empty, one-bit-short, exact-capacity,
      and one-bit-over limits.
- [ ] Exercise every download rejection spelling (`../`, `..\\`, `::`,
      absolute, empty, non-pk3, unreferenced, and truncated pair).
- [ ] Run valid baseq3 and missionpack assets/QVMs after hardening to detect
      compatibility regressions.
- [ ] Re-run the upstream security-history comparison whenever the ioquake3
      anchor changes.

[c204fe36d]: https://github.com/jm2/Quake-III-Arena/commit/204fe36deccfde8ead94f6bba0422dd8c1ba396e
[ce1fcf08f]: https://github.com/jm2/Quake-III-Arena/commit/e1fcf08fc7d1a1aa9dd3707181ce86e20c835c9a
[c53440e7f]: https://github.com/jm2/Quake-III-Arena/commit/53440e7f081ac218b3b064cb859b971a0061c3ad
[c4e75fdf]: https://github.com/jm2/Quake-III-Arena/commit/4e75fdfdc9c78023e923a36b4aea2996deee908e
[c8f4d76a]: https://github.com/jm2/Quake-III-Arena/commit/8f4d76a5f2840a987e92088d6d36e2634c75b9e7
[c2cef87c]: https://github.com/jm2/Quake-III-Arena/commit/2cef87c77fa9712c8855ea6e3c1b2f344e6f0ace
[cd565712]: https://github.com/jm2/Quake-III-Arena/commit/d5657120282b93853362437981b98877b6b8e884
[pr573]: https://github.com/jm2/Quake-III-Arena/pull/573
[pr76]: https://github.com/jm2/Quake-III-Arena/pull/76
[pr82]: https://github.com/jm2/Quake-III-Arena/pull/82
[pr197]: https://github.com/jm2/Quake-III-Arena/pull/197
[pr209]: https://github.com/jm2/Quake-III-Arena/pull/209
[pr210]: https://github.com/jm2/Quake-III-Arena/pull/210
[pr309]: https://github.com/jm2/Quake-III-Arena/pull/309
[pr321]: https://github.com/jm2/Quake-III-Arena/pull/321
[pr322]: https://github.com/jm2/Quake-III-Arena/pull/322
[pr324]: https://github.com/jm2/Quake-III-Arena/pull/324
[pr407]: https://github.com/jm2/Quake-III-Arena/pull/407
[pr418]: https://github.com/jm2/Quake-III-Arena/pull/418
[pr421]: https://github.com/jm2/Quake-III-Arena/pull/421
[pr425]: https://github.com/jm2/Quake-III-Arena/pull/425
[pr431]: https://github.com/jm2/Quake-III-Arena/pull/431
[pr433]: https://github.com/jm2/Quake-III-Arena/pull/433
[pr449]: https://github.com/jm2/Quake-III-Arena/pull/449
[pr452]: https://github.com/jm2/Quake-III-Arena/pull/452
[pr455]: https://github.com/jm2/Quake-III-Arena/pull/455
[pr458]: https://github.com/jm2/Quake-III-Arena/pull/458
[pr460]: https://github.com/jm2/Quake-III-Arena/pull/460
[pr472]: https://github.com/jm2/Quake-III-Arena/pull/472
[pr478]: https://github.com/jm2/Quake-III-Arena/pull/478
[pr506]: https://github.com/jm2/Quake-III-Arena/pull/506
[pr539]: https://github.com/jm2/Quake-III-Arena/pull/539
[pr542]: https://github.com/jm2/Quake-III-Arena/pull/542
[pr554]: https://github.com/jm2/Quake-III-Arena/pull/554
[pr555]: https://github.com/jm2/Quake-III-Arena/pull/555
[pr556]: https://github.com/jm2/Quake-III-Arena/pull/556
[pr557]: https://github.com/jm2/Quake-III-Arena/pull/557
[pr558]: https://github.com/jm2/Quake-III-Arena/pull/558
[pr559]: https://github.com/jm2/Quake-III-Arena/pull/559
[pr560]: https://github.com/jm2/Quake-III-Arena/pull/560
[pr561]: https://github.com/jm2/Quake-III-Arena/pull/561
[pr562]: https://github.com/jm2/Quake-III-Arena/pull/562
[pr563]: https://github.com/jm2/Quake-III-Arena/pull/563
[pr564]: https://github.com/jm2/Quake-III-Arena/pull/564
[pr565]: https://github.com/jm2/Quake-III-Arena/pull/565
[pr566]: https://github.com/jm2/Quake-III-Arena/pull/566
