# Connection challenge and netchan validation — 2026-09-19

## Compatibility decision

Commercial Quake III Arena protocol 68 remains the advertised and default
protocol. Its connectionless commands, sequenced-packet headers, game-message
body and demo format remain byte-for-byte compatible with 1.32c. The client
attempts the q3noclient extension without requiring it, and either peer falls
back to protocol 68 when the other peer is stock.

The negotiated path uses protocol 69 from the two issue-referenced ioquake3
changes:

- [`a5580d8974`](https://github.com/ioquake/ioq3/commit/a5580d8974008b077edf5ddaf7347d3b6006351d)
  introduced the client challenge, bound `connectResponse`, and added the
  challenge-derived netchan checksum.
- [`e06c117e9e`](https://github.com/ioquake/ioq3/commit/e06c117e9e4361d5c0e4124682a23fd9872bd41f)
  made protocols 68 and 69 explicit compatibility and secure choices.

Later ioquake3/Quake3e protocol numbers include other message-layer features.
This port therefore advertises only the original checksum-only protocol 69.
An unknown or later extension response falls back to protocol 68 rather than
claiming unsupported wire semantics.

## Negotiation

| Client/server combination | Challenge exchange | Selected stream |
| --- | --- | --- |
| Stock client, updated server | Bare `getchallenge`; server returns the original one-argument response | Protocol 68 |
| Updated client, stock server | Client sends a nonce; the stock server ignores it and returns a bare response from the requested endpoint | Protocol 68 |
| Updated client and server | Server echoes the client nonce and advertises 69; the client requires the exact echo | Protocol 69 |
| Nonce-aware proxy handoff | A response from a different endpoint is accepted only with the exact client nonce | Protocol 68 or 69, according to the explicit response |

The client generates a fresh nonzero challenge for every remote connection.
Before changing its destination, it validates the echoed nonce. A secure
`connectResponse` must then come from the exact negotiated endpoint and carry
the server challenge. Bare stock `connectResponse` packets remain accepted
only in protocol-68 mode and only from that exact endpoint.

The server emits `connectResponse <serverChallenge>` for both modes. Extra
OOB arguments are ignored by stock 1.32c clients. The updated client requires
the argument in protocol 69 and validates it when a legacy response supplies
one. Decimal nonce/challenge/protocol/qport fields are parsed completely with
checked signed bounds; malformed or overflowing values reject.

## Sequenced packets

Protocol 68 retains the commercial header:

```
sequence [client qport] [fragment start, fragment length] payload
```

Protocol 69 adds the upstream four-byte checksum after the optional qport and
before fragment metadata:

```
sequence [client qport] challenge-checksum [fragment metadata] payload
```

Both directions and every fragment use the negotiated server challenge.
Checksum multiplication is explicitly unsigned modulo 2^32, preserving the
upstream wire value without signed-overflow undefined behavior. Short headers
reject before channel sequence or fragment state changes.

The server updates a translated client UDP port only after `Netchan_Process`
accepts the packet. A packet that merely matches the base address and 16-bit
qport can no longer redirect replies before sequence/checksum validation.
Unknown sequenced traffic is dropped silently instead of producing a
source-spoofable disconnect response. The client updates its timeout only
after a sequenced packet passes endpoint and netchan validation.

## Protection limits

Protocol 68 necessarily retains its original security limits. A blind sender
that can predict qport and sequence state may still forge or disrupt legacy
traffic. The tightened response endpoint checks and post-validation port
migration reduce compatible attack surface but do not add packet
authentication to stock peers.

Protocol 69 follows the referenced q3noclient checksum exactly for
interoperability. It is a 32-bit spoofing check, not a cryptographic MAC; the
challenge crosses the network in clear text, so an on-path observer can forge
it. The upstream formula also produces zero for sequence one. It raises the
bar for blind off-path packets after negotiation but does not provide
confidentiality, replay protection beyond the existing sequence window, or
protection from an active on-path attacker.

## Host regression evidence

`tests/run_network_challenge_tests.sh` builds the actual message/netchan bodies
under ASan/UBSan and links the actual server challenge-response formatter in
normal and optimized fast-math modes. GCC and Clang cover:

- exact stock protocol-68 client headers and successful legacy processing;
- protocol-69 client/server headers, checksums and fragment placement;
- correct address/qport/sequence preconditions with the wrong challenge
  checksum, rejected before sequence or fragment mutation;
- truncated secure headers;
- absent, malformed, overflowing, mismatched and valid challenge fields;
- exact-endpoint legacy fallback and nonce-bound proxy handoff;
- required secure `connectResponse` challenge binding;
- bare stock and echoed extended server response text; and
- defined checksum wraparound and the upstream sequence-one wire value.

The pre-existing message/Huffman suite also passes with both compilers after
the netchan header change. Portable CI runs both suites under GCC and Clang.

## Retro68 cross-build

Both shipped products compile without compiler diagnostics and pass the build
script's size and header checks. Each output begins with `Joy!peffpwpc`:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,809,283 | `fba0f01a5633385ef47973e990c3f5f148ee4bb883e55b1226dc6b0d36d276d6` |
| Quake3_TeamArena | 3,957,857 | `d7ca9cb7130191e89001884967c2330d808c301874c35f5aa5d01e713b41e398` |

The builds use the temporary toolchain libraries described in the
[QVM loading evidence](qvm-loading-validation.md); no system packages were
installed.

## Deferred acceptance

Live stock-1.32c, updated-peer, proxy/NAT-rebinding and hostile-packet runs
remain deferred until the user supplies the legal retail assets and target or
emulator environment. The host evidence proves negotiation policy, exact
legacy bytes, extended header processing and rejection state; it does not
substitute for that end-to-end target interoperability run.
