# Native weapon configuration staging — 2026-09-18

LoadWeaponConfig previously dereferenced an unchecked hunk allocation before the
source was complete. Malformed scripts spent persistent physical arena storage,
and source errors at the end of a valid prefix could publish a partial result.
Native count conversions and unterminated/NULL filenames also lacked checks.

Validate both cached count variables before integer conversion and bound the
combined header/weapon/projectile cost to the native signed allocation request.
Both defaults remain 32; valid fractional truncation, zero capacity, negative
integer fallback/cache update and empty configuration behavior remain native.
Reject invalid full filenames before cache/VFS imports. Parse checked cleared
heap staging, clean sources and reject source errors, then perform the existing
name/projectile checks and copies. Only a complete result reaches a checked
persistent hunk request. Copy/rebase both inline arrays and release the private
physical heap owner before returning. LoadWeaponConfig does not replace the
shared weaponconfig root; setup/weight publication is a subsequent scoped step.

## Validation

Actual weapon/parser/structure/libvar/allocator bodies preserve exact native
weapon/projectile fields, fixup bytes, unused cleared slots and both inline array
pointers. Six modes cover normal/fast/debug/debug-fast/tracked/tracked-fast under
Clang ASan/UBSan/float-cast-overflow and optimized GCC. Each of fifteen nullable
imports rejects without persistent hunk consumption, frees private source/token/
staging owners, preserves only complete count caches and retries successfully.
Malformed syntax, missing/undefined name/projectile, invalid numbers, zero
projectile capacity and suffix source errors consume no physical hunk. A failed
distinct load retains the prior header, arrays and fixup payload. Both invalid
count fields cover NaN/infinity, unrepresentable values and excessive payloads;
a separately checked combined-cost case has two individually representable
arrays whose sum exceeds the native signed request. Both fallback updates retain
prior cache storage on failed replacement. NULL/empty/overlong paths stop before
imports. Native defaults/fractions/zero/negative golden behavior remains separate.

Six original actual-factory failures reproduce independently for malformed input,
source-error suffix, NaN count, excessive projectile payload and NULL/overlong
paths in every mode under both compilers. Separate original native field/fixup
and default/fraction/zero/negative golden checks pass in every mode. The original
factory body is unchanged in these proof builds.

Physical hunk release remains an owning-engine arena reset; logical FreeMemory
is not treated as physical hunk recovery. Host physical allocator alignment uses
the measured ownership prefix described in [item evidence](bot-item-config-validation.md).
CI runs the new six-mode fixture with both compilers. Bash syntax, five ledger/
manifest checks and diff checks pass.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,783,995 | `e71c8af7c097f9a3bd951c419c835528a2967b3fc975bc4e8cd6335231d7816b` |
| Quake3_TeamArena | 3,932,569 | `6b445275eb549d91815b725d7073f09a26ed88a76892472fd0347cfe1677eb45` |

Both PPC products compile/link with zero diagnostics and valid PEF headers.
Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).
Commercial 1.32c public structs/imports/syscalls, protocol defaults, field sizes,
parser format and valid native values remain unchanged.

## Remaining acceptance

Keep #48 open for weapon setup/weight/public allocation consumers, chat and other
nullable factories, builtin dictionaries, complete transactions and aggregate
expression/parse/recursion/memory limits. No retail/Mac OS 9 execution is claimed;
that acceptance is explicitly deferred.
