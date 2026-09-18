# Bot libvar allocation and alias ownership — 2026-09-18

The native setter frees an old value before measuring/copying the new input,
which can point into that same value. Factories publish a name/node before the
value allocation completes. Nullable name/value imports are then cleared or
copied without checks, leaving unsafe/partial variable state.

Check full signed name/value import costs and nullable allocations. Clone new
values before releasing prior storage; finish both new owners before variable
creation can succeed. Failed creation releases any prospective string owner and
publishes no node. Failed replacement leaves the entire prior node/value/flags
intact. Getters return native missing-value zero/empty defaults on failed creation;
the pointer factory returns NULL. Cached defaults remain unused when a variable
already exists. Null input guards avoid partial creation. Successful ownership,
case-insensitive naming, modified flags and commercial 1.32c interfaces stay intact.

## Validation

Four original actual-body proofs fail: an interior alias read after free, nullable
name clearing, nullable factory value copy and nullable replacement value copy.
The fixture includes the actual complete numeric converter/variable backend;
native heap/Q_shared clear/name lookup are interfaces, with no production
allocation/publication/value/alias logic replaced.

The existing forty backend numeric cases remain intact. Whole/interior alias
replacements retain their text and two physical owners. Sixteen failed new
creations span factory, numeric/string getter and setter, each nullable owner,
and absence/presence of a previous variable. No prospective owner/node survives;
existing node bytes/value/flags/list links stay unchanged. Each case retries
successfully. Two replacement failures (literal and interior-alias inputs)
preserve the old owner/value/unmodified flag, then retry with native value 45.
Invalid pointers create no node/import, cached defaults ignore unused null input,
and invalid replacement retains the old value. Successful cleanup physically
frees every name/string owner. No arbitrary name/value length cap is added beyond
signed target import representability.

Clang normal/release fast-math ASan/UBSan and optimized GCC pass. Existing GCC and
Clang libvar CI runners execute these cases. Review-ledger, Bash and diff checks
pass. Both Retro68 products rebuild with zero compiler diagnostics and valid
PPC PEFs:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,762,317 | `88f49d132eb296316faa8760bbd9edd356fbfa5053307fc6c7e6e8de4ffaa469` |
| Quake3_TeamArena | 3,910,891 | `3baee58573b42ebac48c9157c573353df0dd9ad7f998c01fee023b292f9f5d49` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).
Source includes checked numeric-libvar/routing initialization/conversion parents.

## Remaining acceptance

Keep #48 open for direct pointer-factory consumers, all other parser allocation
callers, token merge/stringize/include and character path/index bounds, static
date/time storage ownership and aggregate parse work/recursion budgets. This
backend change does not establish allocation failure handling in every library
initializer that holds a libvar pointer. Retail/Mac OS 9 execution stays deferred.
