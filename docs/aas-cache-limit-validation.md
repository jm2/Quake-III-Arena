# Routing cache cvar signed byte conversion — 2026-09-18

`max_routingcache` is a floating kilobyte libvar. The native initializer casts
it to int and multiplies by 1,024 without checking either signed range. Finite
large values can overflow the cast or multiplication even after numeric parsing
is bounded. The checked numeric-libvar parent ensures its parsed values are
finite; this consumer needs its own integer byte-count bound.

Preserve fractional truncation, zero and every nonnegative representable native
byte result. Values at or above the first overflowing kilobyte count saturate
to `INT_MAX`; unsupported negative capacity becomes zero. Bound before either
cast or multiplication. The default 4,096 KB remains 4,194,304 bytes. Commercial
1.32c data, syscall/protocol interfaces and routing table values stay unchanged.

## Validation

Two original actual-body UBSan proofs fail during the full native routing
initialization: 2,097,152 KB overflows signed multiplication, and 2^31 KB overflows
the float-to-int cast. The existing actual pipeline/continuation fixture adds
twelve independent literal KB/byte goldens, including zero, sub-kilobyte and
fractional truncation, defaults, 2,097,151.875 KB at the last safe multiple,
the first overflowing value, 2^31, maximum finite float and negative capacity.
Every case retains successful ten-table initialization and physical cleanup;
no production conversion/allocation logic is replaced.

Clang normal/release fast-math ASan/UBSan and optimized GCC pass. The existing
native initialization CI runner executes these cases. Review-ledger, Bash and
diff checks pass. Both Retro68 products rebuild with zero compiler diagnostics
and valid PPC PEFs:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,762,317 | `f319b457f7086740733d14987e84170e210d5140dcf05d79a245a54d0505f0c5` |
| Quake3_TeamArena | 3,910,891 | `ac2358f9f20bfb15ff94d4dbe2e05f521a295b2bf62970b2df1d89b7058d5601` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).
Source includes the checked finite numeric-libvar and routing initialization parents.

## Remaining acceptance

Keep #47 open for aggregate native cache/derived RAM and work budget enforcement,
remaining runtime query/geometry/mover roots, legacy outgoing byte-index capacity
and whole-world replacement/physical arena recovery. Checking this cvar's byte
representation does not establish or enforce a usable memory budget. Retail
assets and Mac OS 9 execution remain deferred; bots remain disabled.
