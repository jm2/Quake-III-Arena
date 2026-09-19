# Message and adaptive-Huffman bounds — 2026-09-18

Whole-packet adaptive-Huffman decoding now stops at the logical packet end,
including tree traversal and the eight raw bits after a not-yet-transmitted
symbol. A truncated packet sets `overflowed`, restores `cursize` to the
uncompressed prefix, and publishes no partial output. Compression validates
message state and offsets, bounds its scratch stream, and leaves the caller's
packet unchanged if the encoded result cannot fit.

`NET_OutOfBandData` rejects null, negative, and oversized inputs before copying.
It initializes the complete `msg_t`, supplies the real destination capacity, and
does not send a packet after compression failure. The public function signatures,
`msg_t` layout, syscall surface, and network format are unchanged.

## Compatibility contract

The message boundary behavior remains the final ioquake3 behavior from
[`d2b1d124`](https://github.com/ioquake/ioq3/commit/d2b1d124d4055c2fcbe5126863487c52fd58cca1)
and its exact-capacity follow-up
[`1e309787`](https://github.com/ioquake/ioq3/commit/1e309787224326b66f04cd166fbd9e200f5fded5).
The fixture checks the exact 37-byte compressed suffix for a commercial 1.32c
`connect \protocol\68\qport\27960` payload, then decodes it byte-for-byte.
It also sends the payload through the real out-of-band caller and verifies the
four-byte marker, eight-byte clear prefix, compressed tail, and full round trip.
Byte-aligned streams keep the historical extra pad byte and now set it to zero
instead of transmitting stale scratch data.

The codec scratch arrays are static to avoid placing more than 64 KiB on a
Classic Mac OS call stack. This does not add a reentrancy restriction: the
historical codec already shares its bit cursor globally, and network packet
processing is single-threaded.

## Host regression proof

`tests/message_huffman_regression.c` includes the real message, Huffman, and
network-channel bodies. Each case runs in a separate process under ASan/UBSan so
one inherited crash cannot hide later results. The 26 cases cover:

- unaligned exact and one-byte-short OOB 8/16/32-bit access;
- empty, one-bit-short, exact-end, and one-bit-over bit streams;
- signed bit fields and the fixed commercial compressed-stream golden;
- short Huffman headers, truncated tree/NYT symbols, invalid offsets and
  message state, zero-symbol packets, destination expansion, and unchanged
  destination bytes on failure, plus deterministic byte-aligned padding;
- valid out-of-band round trips plus null, negative, oversized, and
  compression-overflow send suppression.

Clang and optimized GCC run normal and `-O2 -DNDEBUG -ffast-math` modes, for
104 isolated executions per source state:

| Source state | Fixture failures | Fixture passes |
| --- | ---: | ---: |
| Original GPL release `dbe4ddb` | 84 | 20 |
| Exact PR parent `31a6554` | 56 | 48 |
| This change | 0 | 104 |

The exact parent already contains the accepted message-bit fixes, so its 56
failures isolate whole-packet Huffman and out-of-band caller bounds. The original
release additionally reproduces exact OOB write rejection, unaligned integer
undefined behavior, short reads, exact-end bit handling, and signed-field faults.
The runner is part of both GCC and Clang portable CI.

Five ledger/manifest unit tests, Bash syntax, and diff checks pass. Both Retro68
products build without compiler diagnostics and have valid `Joy!peffpwpc` PEF
headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,783,967 | `36c73aa13d8636871a1369adc2dc909ce21cb114ab53a2abfa167ef5cb587da6` |
| Quake3_TeamArena | 3,932,541 | `8569e45d743671d94072e6e4c4495ece3d60aa5ece36b68ca0f0bc7cbd374d23` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Retail baseq3/missionpack connection, server, and demo playback on Mac OS 9
remain deferred until the user supplies the legal assets and target environment.
Keep #29 open for the remaining advisory families and malformed-input corpora.
